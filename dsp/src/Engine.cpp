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

    double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

    float peakOf (const float* x, int n) noexcept
    {
        float p = 0.0f;
        for (int i = 0; i < n; ++i)
            p = std::max (p, std::abs (x[i]));
        return p;
    }

    void raisePeak (std::atomic<float>& meter, float value) noexcept
    {
        if (value > meter.load (std::memory_order_relaxed))
            meter.store (value, std::memory_order_relaxed);
    }

    /** Index into the 10-entry upper triangle of a symmetric 4x4 matrix. */
    constexpr std::size_t triIndex (std::size_t i, std::size_t j) noexcept
    {
        if (i > j)
            std::swap (i, j);
        return i * 4 - i * (i + 1) / 2 + j;
    }
} // namespace

PairConfig pairConfigFor (StereoType type, double abSpacingCm, double pattern, double stereoAngleDeg) noexcept
{
    switch (type)
    {
        case StereoType::AB:   return { 0.0, abSpacingCm / 100.0, 0.0 };
        case StereoType::ORTF: return { 55.0, 0.17, 0.5 };
        case StereoType::NOS:  return { 45.0, 0.30, 0.5 };
        case StereoType::DIN:  return { 45.0, 0.20, 0.5 };
        case StereoType::Coincident:
        case StereoType::Binaural: break;
    }
    return { 0.5 * stereoAngleDeg, 0.0, pattern };
}

void Engine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlock = std::max (1, maximumBlockSize);
    scratch.assign (static_cast<std::size_t> (numInputs * maxBlock), 0.0f);
    worldScratch.assign (static_cast<std::size_t> ((numInputs + 2) * maxBlock), 0.0f);   // + 2 spare output channels

    shotgun.prepare (sampleRate);
    spaced.prepare (sampleRate, maxBlock);
    binaural.prepare (sampleRate, maxBlock);

    for (std::size_t m = 0; m < numMicModels; ++m)
        corrections[m] = designArrayCorrection (getMicProfile (static_cast<MicModel> (m)), sampleRate);
    correctionDelay = corrections[0].delay;
    for (auto& f : correctionFirs)
        f.prepare (static_cast<int> (corrections[0].w.size()), maxBlock);
    correctionModel = -1;

    lfeCutoff = -1.0;
    windCutoff = -1.0;
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
    for (auto& pair : windFilters)
        for (auto& f : pair)
            f.clear();
    for (auto& f : correctionFirs)
        f.reset();
    shotgun.reset();
    spaced.reset();
    binaural.reset();
    snapToTarget = true;
}

Mat4 Engine::inputToMicAmbiX (const EngineParams& params) noexcept
{
    switch (params.inputFormat)
    {
        case InputFormat::AFormat:
        {
            Mat4 m = aFormatToAmbiX (getMicProfile (params.micModel));
            for (std::size_t c = 0; c < 4; ++c)
            {
                const double g = dbToGain (std::clamp (params.capsuleTrimDb[c], -12.0, 12.0)) * (params.capsuleInvert[c] ? -1.0 : 1.0);
                for (std::size_t r = 0; r < 4; ++r)
                    m[r][c] *= g;
            }
            return m;
        }
        case InputFormat::FuMa:  return fuMaToAmbiX();
        case InputFormat::AmbiX: break;
    }
    return identity4();
}

Mat4 Engine::micToWorld (const EngineParams& params) noexcept
{
    Mat3 rotation = sceneRotation (params.yawDeg, params.pitchDeg, params.rollDeg);
    if (params.inputFormat == InputFormat::AFormat)
        rotation = rotation * mountRotation (params.orientation);
    return ambiXRotation (rotation);
}

Engine::GainMatrix Engine::computeOutputMatrix (const EngineParams& params) const noexcept
{
    const Mat4 in = micToWorld (params);
    const double gain = dbToGain (params.gainDb);

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

Engine::GainMatrix Engine::computeGainMatrix (const EngineParams& params) const noexcept
{
    const GainMatrix out = computeOutputMatrix (params);
    const Mat4 in = inputToMicAmbiX (params);

    GainMatrix rows {};
    for (std::size_t o = 0; o < static_cast<std::size_t> (maxOutputs); ++o)
        for (std::size_t j = 0; j < 4; ++j)
        {
            double s = 0.0;
            for (std::size_t k = 0; k < 4; ++k)
                s += out[o][k] * in[k][j];
            rows[o][j] = s;
        }
    return rows;
}

void Engine::setShotgunEnabled (bool shouldBeEnabled) noexcept
{
    if (shouldBeEnabled == shotgunEnabled)
        return;
    shotgunEnabled = shouldBeEnabled;
    shotgun.reset();
    snapToTarget = true;
}

bool Engine::usesWorldPath (const EngineParams& params) const noexcept
{
    return usesShotgun() || (mode == OutputMode::Stereo && params.stereoType != StereoType::Coincident);
}

int Engine::getLatencySamples (const EngineParams& params) const noexcept
{
    int latency = 0;
    if (params.inputFormat == InputFormat::AFormat && params.spacingCorrection)
        latency += correctionDelay;

    if (usesShotgun())
        latency += shotgun.getLatencySamples();
    else if (mode == OutputMode::Stereo && params.stereoType == StereoType::Binaural)
        latency += binaural.getLatencySamples();
    else if (mode == OutputMode::Stereo && params.stereoType != StereoType::Coincident)
        latency += spaced.getLatencySamples();

    return latency;
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

void Engine::updateWindFilters (double cutoffHz) noexcept
{
    const double fc = cutoffHz > 0.0 ? std::clamp (cutoffHz, 10.0, 0.45 * sampleRate) : 0.0;
    if (std::abs (fc - windCutoff) < 1.0e-9)
        return;
    if (windCutoff <= 0.0 && fc > 0.0)
        for (auto& pair : windFilters)
            for (auto& f : pair)
                f.clear();
    windCutoff = fc;
    if (fc <= 0.0)
        return;

    // 4th-order Butterworth high-pass as two biquads.
    const double w0 = 2.0 * std::numbers::pi * fc / sampleRate;
    const double cosw = std::cos (w0);
    constexpr double qs[2] = { 0.54119610, 1.30656296 };
    for (auto& pair : windFilters)
        for (std::size_t s = 0; s < 2; ++s)
        {
            auto& f = pair[s];
            const double alpha = std::sin (w0) / (2.0 * qs[s]);
            const double a0 = 1.0 + alpha;
            f.b0 = (1.0 + cosw) * 0.5 / a0;
            f.b1 = -(1.0 + cosw) / a0;
            f.b2 = f.b0;
            f.a1 = -2.0 * cosw / a0;
            f.a2 = (1.0 - alpha) / a0;
        }
}

void Engine::updateCorrectionTaps (const EngineParams& params) noexcept
{
    auto model = static_cast<int> (params.micModel);
    if (model < 0 || model >= static_cast<int> (numMicModels))
        model = 0;
    if (model == correctionModel)
        return;
    correctionModel = model;

    const auto& c = corrections[static_cast<std::size_t> (model)];
    correctionFirs[0].setTaps (c.w.data(), static_cast<int> (c.w.size()));
    for (std::size_t ch = 1; ch < 4; ++ch)
        correctionFirs[ch].setTaps (c.xyz.data(), static_cast<int> (c.xyz.size()));
}

void Engine::runInputStage (int len, const EngineParams& params, const Mat4& from, const Mat4& to, int start, double inv) noexcept
{
    float* ch[4] = { scratch.data(), scratch.data() + maxBlock, scratch.data() + 2 * maxBlock, scratch.data() + 3 * maxBlock };
    const bool aFormat = params.inputFormat == InputFormat::AFormat;

    if (aFormat && windCutoff > 0.0)
        for (std::size_t c = 0; c < 4; ++c)
        {
            auto& stages = windFilters[c];
            for (int i = 0; i < len; ++i)
                ch[c][i] = stages[1].process (stages[0].process (ch[c][i]));
        }

    if (params.inputFormat != InputFormat::AmbiX)
        for (int i = 0; i < len; ++i)
        {
            const double t = static_cast<double> (start + i + 1) * inv;
            const double x[4] = { ch[0][i], ch[1][i], ch[2][i], ch[3][i] };
            for (std::size_t r = 0; r < 4; ++r)
            {
                double s = 0.0;
                for (std::size_t c = 0; c < 4; ++c)
                    s += (from[r][c] + (to[r][c] - from[r][c]) * t) * x[c];
                ch[r][i] = static_cast<float> (s);
            }
        }

    if (aFormat && params.spacingCorrection)
        for (std::size_t c = 0; c < 4; ++c)
            correctionFirs[c].process (ch[c], ch[c], len);
}

void Engine::accumulateCovariance (int len, const Mat4& toWorld) noexcept
{
    const float* ch[4] = { scratch.data(), scratch.data() + maxBlock, scratch.data() + 2 * maxBlock, scratch.data() + 3 * maxBlock };

    Mat4 c {};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i; j < 4; ++j)
        {
            double s = 0.0;
            for (int n = 0; n < len; ++n)
                s += static_cast<double> (ch[i][n]) * ch[j][n];
            c[i][j] = c[j][i] = s;
        }

    // World frame: M C M^T.
    const Mat4 mc = multiply (toWorld, c);
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i; j < 4; ++j)
        {
            double s = 0.0;
            for (std::size_t k = 0; k < 4; ++k)
                s += mc[i][k] * toWorld[j][k];
            covAccum[triIndex (i, j)] += s;
        }
    covCount += len;
}

void Engine::runWorldStage (float* const* outputs, int numOutputChannels, int len, int start,
                            const Mat4& from, const Mat4& to, double inv, const EngineParams& params) noexcept
{
    const float* x[4] = { scratch.data(), scratch.data() + maxBlock, scratch.data() + 2 * maxBlock, scratch.data() + 3 * maxBlock };
    float* world[4] = { worldScratch.data(), worldScratch.data() + maxBlock, worldScratch.data() + 2 * maxBlock, worldScratch.data() + 3 * maxBlock };

    for (std::size_t r = 0; r < 4; ++r)
        for (int i = 0; i < len; ++i)
        {
            const double t = static_cast<double> (start + i + 1) * inv;
            double s = 0.0;
            for (std::size_t c = 0; c < 4; ++c)
                s += (from[r][c] + (to[r][c] - from[r][c]) * t) * x[c][i];
            world[r][i] = static_cast<float> (s);
        }

    // Outputs the host did not give us go to spare scratch channels.
    float* spare0 = worldScratch.data() + 4 * maxBlock;
    float* spare1 = spare0 + maxBlock;
    float* out0 = numOutputChannels > 0 && outputs[0] != nullptr ? outputs[0] + start : spare0;
    float* out1 = numOutputChannels > 1 && outputs[1] != nullptr ? outputs[1] + start : spare1;

    if (usesShotgun())
    {
        const Vec3 look = directionFromAzEl (speakers[0].azimuthDeg, speakers[0].elevationDeg);
        shotgun.process (world, len, virtualMicRow (look, params.pattern), look, params.shotgunFocus, out0);
    }
    else if (params.stereoType == StereoType::Binaural)
    {
        binaural.process (world, len, out0, out1);
    }
    else
    {
        spaced.process (world, len, pairConfigFor (params.stereoType, params.abSpacingCm, params.pattern, params.stereoAngleDeg),
                        out0, out1);
    }
}

void Engine::publishMeters (float* const* outputs, int numOutputChannels, int numSamples) noexcept
{
    for (int o = 0; o < std::min (numOutputChannels, numOutputs); ++o)
        if (outputs[o] != nullptr)
            raisePeak (meters.outputPeak[static_cast<std::size_t> (o)], peakOf (outputs[o], numSamples));

    // Let old statistics fade if no display is reading them.
    if (covCount > 10.0 * sampleRate)
    {
        for (auto& v : covAccum)
            v *= 0.5;
        covCount *= 0.5;
    }
    for (std::size_t i = 0; i < covAccum.size(); ++i)
        meters.covariance[i].store (static_cast<float> (covAccum[i]), std::memory_order_relaxed);
    meters.covarianceSamples.store (static_cast<float> (covCount), std::memory_order_release);
}

void Engine::process (const float* const* inputs, int numInputChannels,
                      float* const* outputs, int numOutputChannels,
                      int numSamples, const EngineParams& params) noexcept
{
    if (numSamples <= 0 || maxBlock == 0)
        return;

    updateLfeFilters (params.lfeCutoffHz);
    updateWindFilters (params.inputFormat == InputFormat::AFormat ? params.windHighPassHz : 0.0);
    updateCorrectionTaps (params);

    if (usesWorldPath (params) != usesWorldPath (lastParams))
        snapToTarget = true;
    lastParams = params;

    if (meters.resetCovariance.exchange (false, std::memory_order_acq_rel))
    {
        covAccum.fill (0.0);
        covCount = 0.0;
    }

    const bool worldPath = usesWorldPath (params);
    const Mat4 inTarget = inputToMicAmbiX (params);
    const Mat4 toWorld = micToWorld (params);

    GainMatrix target {};
    Mat4 worldTarget {};
    if (worldPath)
    {
        const double gain = dbToGain (params.gainDb);
        worldTarget = toWorld;
        for (auto& row : worldTarget)
            for (auto& v : row)
                v *= gain;
    }
    else
    {
        target = computeOutputMatrix (params);
    }

    if (snapToTarget)
    {
        currentInput = inTarget;
        current = target;
        currentWorld = worldTarget;
        snapToTarget = false;
    }

    const Mat4 fromIn = currentInput;
    const GainMatrix from = current;
    const Mat4 fromWorld = currentWorld;
    const int usedInputs = std::clamp (numInputChannels, 0, numInputs);
    const double inv = 1.0 / numSamples;

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = std::min (maxBlock, numSamples - start);

        // Copy inputs first: hosts usually process in place, so outputs may alias inputs.
        for (int ch = 0; ch < numInputs; ++ch)
        {
            float* dst = scratch.data() + ch * maxBlock;
            if (ch < usedInputs && inputs[ch] != nullptr)
            {
                std::copy_n (inputs[ch] + start, len, dst);
                raisePeak (meters.inputPeak[static_cast<std::size_t> (ch)], peakOf (dst, len));
            }
            else
            {
                std::fill_n (dst, len, 0.0f);
            }
        }

        runInputStage (len, params, fromIn, inTarget, start, inv);
        accumulateCovariance (len, toWorld);

        int outs = std::min (numOutputChannels, numOutputs);
        if (worldPath)
        {
            runWorldStage (outputs, numOutputChannels, len, start, fromWorld, worldTarget, inv, params);
        }
        else
        {
            const float* x0 = scratch.data();
            const float* x1 = x0 + maxBlock;
            const float* x2 = x1 + maxBlock;
            const float* x3 = x2 + maxBlock;

            for (int o = 0; o < outs; ++o)
            {
                float* out = outputs[o];
                if (out == nullptr)
                    continue;

                const auto& g0 = from[static_cast<std::size_t> (o)];
                const auto& g1 = target[static_cast<std::size_t> (o)];

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
        }

        for (int o = outs; o < numOutputChannels; ++o)
            if (outputs[o] != nullptr)
                std::fill_n (outputs[o] + start, len, 0.0f);
    }

    currentInput = inTarget;
    current = target;
    currentWorld = worldTarget;

    publishMeters (outputs, numOutputChannels, numSamples);
}

} // namespace sf2
