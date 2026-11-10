#include "sf2/Shotgun.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sf2
{

void Shotgun::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    fftSize = 256;
    while (fftSize < static_cast<int> (1024.0 * sampleRate / 48000.0))
        fftSize *= 2;
    hop = fftSize / 2;
    fft.prepare (fftSize);

    const auto n = static_cast<std::size_t> (fftSize);
    const auto bins = n / 2 + 1;

    window.resize (n);
    for (std::size_t i = 0; i < n; ++i)
        window[i] = std::sin (std::numbers::pi * static_cast<double> (i) / static_cast<double> (n));   // periodic sqrt-Hann

    for (auto& v : inputs) v.assign (n, 0.0f);
    for (auto& v : spectra) v.assign (n, {});
    work.assign (n, {});
    overlap.assign (n, 0.0);
    ready.assign (static_cast<std::size_t> (hop), 0.0f);
    intensity.assign (bins, {});
    energy.assign (bins, 0.0);
    gains.assign (bins, 1.0);
    rawGains.assign (bins, 1.0);

    const double frameSeconds = hop / sampleRate;
    analysisCoeff = std::exp (-frameSeconds / 0.030);   // 30 ms direction/diffuseness averaging
    gainCoeff     = std::exp (-frameSeconds / 0.010);   // 10 ms gain smoothing

    reset();
}

void Shotgun::reset() noexcept
{
    for (auto& v : inputs) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (overlap.begin(), overlap.end(), 0.0);
    std::fill (ready.begin(), ready.end(), 0.0f);
    std::fill (intensity.begin(), intensity.end(), Vec3 {});
    std::fill (energy.begin(), energy.end(), 0.0);
    std::fill (gains.begin(), gains.end(), 1.0);
    pos = 0;
}

void Shotgun::process (const float* const* ambiX, int numSamples, const std::array<double, 4>& micRow,
                       const Vec3& look, double focus, float* out) noexcept
{
    if (fftSize == 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writeAt = static_cast<std::size_t> (fftSize - hop + pos);
        for (std::size_t c = 0; c < 4; ++c)
            inputs[c][writeAt] = ambiX[c][i];

        out[i] = ready[static_cast<std::size_t> (pos)];

        if (++pos == hop)
        {
            processFrame (micRow, look, focus);
            pos = 0;
        }
    }
}

void Shotgun::processFrame (const std::array<double, 4>& micRow, const Vec3& look, double focus) noexcept
{
    const auto n = static_cast<std::size_t> (fftSize);
    const auto h = static_cast<std::size_t> (hop);
    const auto bins = n / 2 + 1;

    for (std::size_t c = 0; c < 4; ++c)
    {
        auto& s = spectra[c];
        for (std::size_t i = 0; i < n; ++i)
            s[i] = { inputs[c][i] * window[i], 0.0 };
        fft.forward (s.data());
    }

    const double f = std::clamp (focus, 0.0, 1.0);
    const double exponent = 8.0 * f;                          // beam sharpness
    const double diffuseGain = 1.0 - 0.75 * f;                // up to -12 dB on diffuse sound
    const double floorGain = std::pow (10.0, -30.0 * f / 20.0);

    for (std::size_t k = 0; k < bins; ++k)
    {
        const auto w = spectra[0][k], y = spectra[1][k], z = spectra[2][k], x = spectra[3][k];
        const Vec3 iv { (std::conj (w) * x).real(), (std::conj (w) * y).real(), (std::conj (w) * z).real() };
        const double e = 0.5 * (std::norm (w) + std::norm (x) + std::norm (y) + std::norm (z));

        intensity[k] = intensity[k] * analysisCoeff + iv * (1.0 - analysisCoeff);
        energy[k] = energy[k] * analysisCoeff + e * (1.0 - analysisCoeff);

        double g = 1.0;
        if (f > 0.0)
        {
            const double len = intensity[k].length();
            const double diffuseness = std::clamp (1.0 - len / (energy[k] + 1.0e-30), 0.0, 1.0);
            const double cosTheta = len > 0.0 ? intensity[k].dot (look) / len : 0.0;
            const double beam = std::pow (0.5 * (1.0 + cosTheta), exponent);
            g = std::max (floorGain, (1.0 - diffuseness) * beam + diffuseness * diffuseGain);
        }
        rawGains[k] = g;
    }

    for (std::size_t k = 0; k < bins; ++k)
    {
        const double lo = rawGains[k == 0 ? 0 : k - 1];
        const double hi = rawGains[k + 1 < bins ? k + 1 : k];
        const double smoothed = f > 0.0 ? 0.25 * lo + 0.5 * rawGains[k] + 0.25 * hi : 1.0;
        gains[k] = f > 0.0 ? gains[k] * gainCoeff + smoothed * (1.0 - gainCoeff) : 1.0;
    }

    for (std::size_t k = 0; k < bins; ++k)
    {
        std::complex<double> v = 0.0;
        for (std::size_t c = 0; c < 4; ++c)
            v += micRow[c] * spectra[c][k];
        work[k] = v * gains[k];
    }
    for (std::size_t k = bins; k < n; ++k)
        work[k] = std::conj (work[n - k]);

    fft.inverse (work.data());

    for (std::size_t i = 0; i < n; ++i)
        overlap[i] += work[i].real() * window[i];

    for (std::size_t i = 0; i < h; ++i)
        ready[i] = static_cast<float> (overlap[i]);

    std::copy (overlap.begin() + static_cast<std::ptrdiff_t> (h), overlap.end(), overlap.begin());
    std::fill (overlap.end() - static_cast<std::ptrdiff_t> (h), overlap.end(), 0.0);

    for (auto& v : inputs)
    {
        std::copy (v.begin() + static_cast<std::ptrdiff_t> (h), v.end(), v.begin());
        std::fill (v.end() - static_cast<std::ptrdiff_t> (h), v.end(), 0.0f);
    }
}

} // namespace sf2
