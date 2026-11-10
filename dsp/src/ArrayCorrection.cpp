#include "sf2/ArrayCorrection.h"

#include "sf2/AFormat.h"
#include "sf2/Fft.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sf2
{
namespace
{
    /** Roughly uniform directions on the sphere (Fibonacci lattice). */
    const std::vector<Vec3>& designDirections()
    {
        static const std::vector<Vec3> dirs = []
        {
            constexpr int n = 600;
            std::vector<Vec3> v;
            v.reserve (n);
            const double golden = std::numbers::pi * (3.0 - std::sqrt (5.0));
            for (int i = 0; i < n; ++i)
            {
                const double z = 1.0 - 2.0 * (i + 0.5) / n;
                const double r = std::sqrt (1.0 - z * z);
                const double phi = golden * i;
                v.push_back ({ r * std::cos (phi), r * std::sin (phi), z });
            }
            return v;
        }();
        return dirs;
    }

    constexpr double maxGain = 2.0;   // +6 dB noise-gain cap

    std::complex<double> capMagnitude (std::complex<double> h) noexcept
    {
        const double m = std::abs (h);
        return m > maxGain ? h * (maxGain / m) : h;
    }
} // namespace

std::array<std::complex<double>, 4> spacedArrayAmbiX (const MicProfile& p, const Vec3& d, double freqHz) noexcept
{
    const double k = 2.0 * std::numbers::pi * freqHz / speedOfSound;
    std::array<std::complex<double>, 4> caps {};
    for (std::size_t i = 0; i < 4; ++i)
    {
        const double c = p.capsuleAxes[i].dot (d);
        // A capsule at r * u is reached r * (u . d) / c earlier than the centre: phase lead.
        caps[i] = (p.omniPart + p.dipolePart * c) * std::polar (1.0, k * p.radiusMetres * c);
    }

    const Mat4 a0 = aFormatToAmbiX (p);
    std::array<std::complex<double>, 4> b {};
    for (std::size_t r = 0; r < 4; ++r)
        for (std::size_t j = 0; j < 4; ++j)
            b[r] += a0[r][j] * caps[j];
    return b;
}

double aliasingFrequency (const MicProfile& p) noexcept
{
    return speedOfSound / (std::numbers::pi * std::max (p.radiusMetres, 1.0e-4));
}

CorrectionGains designCorrectionAt (const MicProfile& p, double freqHz)
{
    if (freqHz <= 0.0 || p.radiusMetres <= 0.0)
        return {};

    std::complex<double> numW = 0.0, numXyz = 0.0;
    double denW = 0.0, denXyz = 0.0, targetW = 0.0, targetXyz = 0.0;

    for (const auto& d : designDirections())
    {
        const auto b = spacedArrayAmbiX (p, d, freqHz);
        const auto t = encodeAmbiX (d);
        numW += std::conj (b[0]) * t[0];
        denW += std::norm (b[0]);
        targetW += t[0] * t[0];
        for (std::size_t a = 1; a < 4; ++a)
        {
            numXyz += std::conj (b[a]) * t[a];
            denXyz += std::norm (b[a]);
            targetXyz += t[a] * t[a];
        }
    }

    // Tikhonov regularisation, rising with frequency.
    const double fa = aliasingFrequency (p);
    const double lambda = 1.0e-2 * (freqHz / fa) * (freqHz / fa);
    const auto lsW   = numW / (denW * (1.0 + lambda));
    const auto lsXyz = numXyz / (denXyz * (1.0 + lambda));

    // Diffuse-field (energy-preserving) gains for above aliasing.
    const double dfW   = std::sqrt (targetW / std::max (denW, 1.0e-30));
    const double dfXyz = std::sqrt (targetXyz / std::max (denXyz, 1.0e-30));

    // Smooth crossover centred on the aliasing frequency.
    const double ratio = freqHz / fa;
    const double s = 1.0 / (1.0 + std::pow (ratio, 8.0));

    CorrectionGains g;
    g.w   = capMagnitude (s * lsW + (1.0 - s) * dfW);
    g.xyz = capMagnitude (s * lsXyz + (1.0 - s) * dfXyz);
    return g;
}

int arrayCorrectionLength (double sampleRate) noexcept
{
    int len = 64;
    while (len * 48000.0 < 64.0 * sampleRate * 0.95)
        len *= 2;
    return len;
}

ArrayCorrectionFilters designArrayCorrection (const MicProfile& p, double sampleRate)
{
    const int len = arrayCorrectionLength (sampleRate);
    const int nfft = 4 * len;
    const int delay = len / 2;

    std::vector<std::complex<double>> w (static_cast<std::size_t> (nfft)), x (static_cast<std::size_t> (nfft));
    for (int k = 0; k <= nfft / 2; ++k)
    {
        const double f = sampleRate * k / nfft;
        const auto g = designCorrectionAt (p, f);
        const auto phase = std::polar (1.0, -2.0 * std::numbers::pi * k * delay / nfft);
        auto gw = g.w, gx = g.xyz;
        if (k == nfft / 2)   // Nyquist bin must be real
            gw = std::abs (gw), gx = std::abs (gx);
        w[static_cast<std::size_t> (k)] = gw * phase;
        x[static_cast<std::size_t> (k)] = gx * phase;
    }
    for (int k = nfft / 2 + 1; k < nfft; ++k)
    {
        w[static_cast<std::size_t> (k)] = std::conj (w[static_cast<std::size_t> (nfft - k)]);
        x[static_cast<std::size_t> (k)] = std::conj (x[static_cast<std::size_t> (nfft - k)]);
    }

    Fft fft;
    fft.prepare (nfft);
    fft.inverse (w.data());
    fft.inverse (x.data());

    ArrayCorrectionFilters out;
    out.delay = delay;
    out.w.resize (static_cast<std::size_t> (len));
    out.xyz.resize (static_cast<std::size_t> (len));
    for (int i = 0; i < len; ++i)
    {
        // Hann window centred on the modelling delay.
        const double win = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * (i + 0.5) / len);
        out.w[static_cast<std::size_t> (i)]   = static_cast<float> (w[static_cast<std::size_t> (i)].real() * win);
        out.xyz[static_cast<std::size_t> (i)] = static_cast<float> (x[static_cast<std::size_t> (i)].real() * win);
    }

    // Keep the DC gain exact (the window trims it slightly).
    double sw = 0.0, sx = 0.0;
    for (int i = 0; i < len; ++i)
    {
        sw += out.w[static_cast<std::size_t> (i)];
        sx += out.xyz[static_cast<std::size_t> (i)];
    }
    for (int i = 0; i < len; ++i)
    {
        out.w[static_cast<std::size_t> (i)]   = static_cast<float> (out.w[static_cast<std::size_t> (i)] / sw);
        out.xyz[static_cast<std::size_t> (i)] = static_cast<float> (out.xyz[static_cast<std::size_t> (i)] / sx);
    }
    return out;
}

} // namespace sf2
