#include "sf2/SpacedPair.h"

#include "sf2/ArrayCorrection.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sf2
{

void SpacedPair::prepare (double newSampleRate, int)
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
    for (auto& v : work) v.assign (n, {});
    for (auto& v : overlap) v.assign (n, 0.0);
    for (auto& v : ready) v.assign (static_cast<std::size_t> (hop), 0.0f);
    intensity.assign (bins, {});
    for (auto& v : phase) v.assign (bins, { 1.0, 0.0 });

    const double frameSeconds = hop / sampleRate;
    analysisCoeff = std::exp (-frameSeconds / 0.030);   // 30 ms direction averaging
    phaseCoeff    = std::exp (-frameSeconds / 0.015);   // 15 ms output smoothing
    reset();
}

void SpacedPair::reset() noexcept
{
    for (auto& v : inputs) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : overlap) std::fill (v.begin(), v.end(), 0.0);
    for (auto& v : ready) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (intensity.begin(), intensity.end(), Vec3 {});
    for (auto& v : phase) std::fill (v.begin(), v.end(), std::complex<double> { 1.0, 0.0 });
    pos = 0;
}

void SpacedPair::process (const float* const* world, int numSamples, const PairConfig& config,
                          float* left, float* right) noexcept
{
    if (fftSize == 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto writeAt = static_cast<std::size_t> (fftSize - hop + pos);
        for (std::size_t c = 0; c < 4; ++c)
            inputs[c][writeAt] = world[c][i];

        left[i]  = ready[0][static_cast<std::size_t> (pos)];
        right[i] = ready[1][static_cast<std::size_t> (pos)];

        if (++pos == hop)
        {
            processFrame (config);
            pos = 0;
        }
    }
}

void SpacedPair::processFrame (const PairConfig& cfg) noexcept
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

    const double p = std::clamp (cfg.pattern, 0.0, 1.0);
    const double half = 0.5 * std::clamp (cfg.spacingMetres, 0.0, maxSpacingMetres);
    std::array<std::array<double, 4>, 2> rows {};
    for (std::size_t m = 0; m < 2; ++m)
    {
        const double side = m == 0 ? 1.0 : -1.0;   // left mic on +y
        const Vec3 aim = directionFromAzEl (side * cfg.halfAngleDeg, 0.0);
        rows[m] = { 1.0 - p, p * aim.y, p * aim.z, p * aim.x };
    }

    for (std::size_t k = 0; k < bins; ++k)
    {
        const auto w = spectra[0][k], y = spectra[1][k], z = spectra[2][k], x = spectra[3][k];
        const Vec3 iv { (std::conj (w) * x).real(), (std::conj (w) * y).real(), (std::conj (w) * z).real() };
        intensity[k] = intensity[k] * analysisCoeff + iv * (1.0 - analysisCoeff);
        const Vec3 d = intensity[k].normalised();   // zero vector when there is no estimate

        // Arrival at +/- half on the y axis relative to the centre: tau = -(pos . d) / c.
        const double omega = 2.0 * std::numbers::pi * static_cast<double> (k) / static_cast<double> (n) * sampleRate;
        const double tauLeft = -half * d.y / speedOfSound;

        for (std::size_t m = 0; m < 2; ++m)
        {
            const double tau = m == 0 ? tauLeft : -tauLeft;
            const auto target = k + 1 == bins ? std::complex<double> (1.0) : std::polar (1.0, -omega * tau);
            phase[m][k] = phase[m][k] * phaseCoeff + target * (1.0 - phaseCoeff);

            std::complex<double> v = 0.0;
            for (std::size_t c = 0; c < 4; ++c)
                v += rows[m][c] * spectra[c][k];
            work[m][k] = v * phase[m][k];
        }
    }

    for (std::size_t m = 0; m < 2; ++m)
    {
        auto& wk = work[m];
        wk[bins - 1] = wk[bins - 1].real();
        wk[0] = wk[0].real();
        for (std::size_t k = bins; k < n; ++k)
            wk[k] = std::conj (wk[n - k]);
        fft.inverse (wk.data());

        auto& ov = overlap[m];
        for (std::size_t i = 0; i < n; ++i)
            ov[i] += wk[i].real() * window[i];
        for (std::size_t i = 0; i < h; ++i)
            ready[m][i] = static_cast<float> (ov[i]);
        std::copy (ov.begin() + static_cast<std::ptrdiff_t> (h), ov.end(), ov.begin());
        std::fill (ov.end() - static_cast<std::ptrdiff_t> (h), ov.end(), 0.0);
    }

    for (auto& v : inputs)
    {
        std::copy (v.begin() + static_cast<std::ptrdiff_t> (h), v.end(), v.begin());
        std::fill (v.end() - static_cast<std::ptrdiff_t> (h), v.end(), 0.0f);
    }
}

} // namespace sf2
