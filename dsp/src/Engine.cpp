#include "sf2/Engine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sf2
{
namespace
{
    Mat3 mountRotation (MountOrientation orientation) noexcept
    {
        switch (orientation)
        {
            case MountOrientation::Upright:  return Mat3::identity();
            case MountOrientation::Inverted: return rotationX (180.0);   // hanging, front still facing front
            case MountOrientation::EndFire:  return rotationY (90.0);    // top of the mic pointing at the front
        }
        return Mat3::identity();
    }

    /** Row of a first-order virtual mic in AmbiX: pattern 0 = omni, 1 = figure-8. */
    std::array<double, 4> virtualMicRow (const Vec3& d, double pattern) noexcept
    {
        const double p = std::clamp (pattern, 0.0, 1.0);
        const double omni = 1.0 - p;
        const auto enc = encodeAmbiX (d);
        return { omni * enc[0], p * enc[1], p * enc[2], p * enc[3] };
    }
} // namespace

void Engine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlock = std::max (1, maximumBlockSize);
    scratch.assign (static_cast<std::size_t> (numInputs * maxBlock), 0.0f);
    lfeCutoff = -1.0;
    reset();
}

void Engine::setOutputLayout (OutputMode newMode, const std::vector<Speaker>& newSpeakers)
{
    mode = newMode;
    switch (mode)
    {
        case OutputMode::Stereo: numOutputs = 2; break;
        case OutputMode::AmbiX:
        case OutputMode::FuMa:   numOutputs = 4; break;
        case OutputMode::Speakers:
            numOutputs = std::clamp (static_cast<int> (newSpeakers.size()), 1, maxOutputs);
            speakers.fill ({});
            std::copy_n (newSpeakers.begin(), std::min<std::size_t> (newSpeakers.size(), maxOutputs), speakers.begin());
            break;
    }
    reset();
}

void Engine::reset() noexcept
{
    for (auto& pair : lfeFilters)
        for (auto& f : pair)
            f.clear();
    snapToTarget = true;
}

Mat4 Engine::inputToAmbiX (const EngineParams& params) noexcept
{
    Mat4 toAmbiX = identity4();
    Mat3 rotation = sceneRotation (params.yawDeg, params.pitchDeg, params.rollDeg);

    switch (params.inputFormat)
    {
        case InputFormat::AFormat:
            toAmbiX = aFormatToAmbiX (getMicProfile (params.micModel));
            rotation = rotation * mountRotation (params.orientation);
            break;
        case InputFormat::FuMa:  toAmbiX = fuMaToAmbiX(); break;
        case InputFormat::AmbiX: break;
    }

    return multiply (ambiXRotation (rotation), toAmbiX);
}

Engine::GainMatrix Engine::computeGainMatrix (const EngineParams& params) const noexcept
{
    const Mat4 in = inputToAmbiX (params);
    const double gain = std::pow (10.0, params.gainDb / 20.0);

    GainMatrix rows {};
    auto setRow = [&] (int out, const std::array<double, 4>& ambiXRow)
    {
        for (int j = 0; j < numInputs; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 4; ++k)
                s += ambiXRow[static_cast<std::size_t> (k)] * in[k][j];
            rows[static_cast<std::size_t> (out)][static_cast<std::size_t> (j)] = gain * s;
        }
    };

    switch (mode)
    {
        case OutputMode::Stereo:
        {
            const double half = 0.5 * params.stereoAngleDeg;
            setRow (0, virtualMicRow (directionFromAzEl (half, 0.0), params.pattern));
            setRow (1, virtualMicRow (directionFromAzEl (-half, 0.0), params.pattern));
            break;
        }
        case OutputMode::AmbiX:
            for (int o = 0; o < 4; ++o)
                setRow (o, identity4()[static_cast<std::size_t> (o)]);
            break;
        case OutputMode::FuMa:
        {
            const Mat4 fuma = ambiXToFuMa();
            for (int o = 0; o < 4; ++o)
                setRow (o, fuma[static_cast<std::size_t> (o)]);
            break;
        }
        case OutputMode::Speakers:
            for (int o = 0; o < numOutputs; ++o)
            {
                const auto& spk = speakers[static_cast<std::size_t> (o)];
                if (spk.isLfe)
                    setRow (o, { 1.0, 0.0, 0.0, 0.0 });   // omni W, band-limited below
                else
                    setRow (o, virtualMicRow (directionFromAzEl (spk.azimuthDeg, spk.elevationDeg), params.pattern));
            }
            break;
    }

    return rows;
}

void Engine::updateLfeFilters (double cutoffHz) noexcept
{
    const double fc = std::clamp (cutoffHz, 20.0, 0.45 * sampleRate);
    if (std::abs (fc - lfeCutoff) < 1.0e-9)
        return;
    lfeCutoff = fc;

    const double w0 = 2.0 * std::numbers::pi * fc / sampleRate;
    const double cosw = std::cos (w0);
    const double alpha = std::sin (w0) / std::numbers::sqrt2;   // sin(w0) / (2Q), Q = 1/sqrt(2)
    const double a0 = 1.0 + alpha;

    for (auto& pair : lfeFilters)
        for (auto& f : pair)
        {
            f.b0 = (1.0 - cosw) * 0.5 / a0;
            f.b1 = (1.0 - cosw) / a0;
            f.b2 = f.b0;
            f.a1 = -2.0 * cosw / a0;
            f.a2 = (1.0 - alpha) / a0;
        }
}

void Engine::process (const float* const* inputs, int numInputChannels,
                      float* const* outputs, int numOutputChannels,
                      int numSamples, const EngineParams& params) noexcept
{
    if (numSamples <= 0 || maxBlock == 0)
        return;

    updateLfeFilters (params.lfeCutoffHz);

    const GainMatrix target = computeGainMatrix (params);
    if (snapToTarget)
    {
        current = target;
        snapToTarget = false;
    }

    const GainMatrix from = current;
    const int usedInputs = std::clamp (numInputChannels, 0, numInputs);

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = std::min (maxBlock, numSamples - start);

        // Copy inputs first: hosts usually process in place, so outputs may alias inputs.
        for (int ch = 0; ch < numInputs; ++ch)
        {
            float* dst = scratch.data() + ch * maxBlock;
            if (ch < usedInputs && inputs[ch] != nullptr)
                std::copy_n (inputs[ch] + start, len, dst);
            else
                std::fill_n (dst, len, 0.0f);
        }

        const int outs = std::min (numOutputChannels, numOutputs);
        for (int o = 0; o < outs; ++o)
        {
            float* out = outputs[o];
            if (out == nullptr)
                continue;

            const auto& g0 = from[static_cast<std::size_t> (o)];
            const auto& g1 = target[static_cast<std::size_t> (o)];
            const float* x0 = scratch.data();
            const float* x1 = x0 + maxBlock;
            const float* x2 = x1 + maxBlock;
            const float* x3 = x2 + maxBlock;
            const double inv = 1.0 / numSamples;

            for (int i = 0; i < len; ++i)
            {
                const double t = static_cast<double> (start + i + 1) * inv;
                const double a = g0[0] + (g1[0] - g0[0]) * t;
                const double b = g0[1] + (g1[1] - g0[1]) * t;
                const double c = g0[2] + (g1[2] - g0[2]) * t;
                const double d = g0[3] + (g1[3] - g0[3]) * t;
                out[start + i] = static_cast<float> (a * x0[i] + b * x1[i] + c * x2[i] + d * x3[i]);
            }

            if (mode == OutputMode::Speakers && speakers[static_cast<std::size_t> (o)].isLfe)
            {
                auto& stages = lfeFilters[static_cast<std::size_t> (o)];
                for (int i = 0; i < len; ++i)
                    out[start + i] = stages[1].process (stages[0].process (out[start + i]));
            }
        }

        for (int o = outs; o < numOutputChannels; ++o)
            if (outputs[o] != nullptr)
                std::fill_n (outputs[o] + start, len, 0.0f);
    }

    current = target;
}

} // namespace sf2
