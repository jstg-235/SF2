#include "sf2/Binaural.h"

#include "sf2/ArrayCorrection.h"
#include "sf2/Fft.h"
#include "sf2/VirtualSpeakers.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace sf2
{
namespace
{
    constexpr double headRadius = 0.0875;   // metres
    constexpr double crossoverHz = 700.0;   // basic decode below, max-rE above

    Vec3 earAxis (int ear) noexcept { return directionFromAzEl (ear == 0 ? 100.0 : -100.0, 0.0); }

    /** Brown & Duda spherical head: head shadow plus time of arrival, relative to the head centre. */
    std::complex<double> headModel (const Vec3& d, int ear, double freqHz) noexcept
    {
        const double theta = std::acos (std::clamp (earAxis (ear).dot (d), -1.0, 1.0));
        const double w = 2.0 * std::numbers::pi * freqHz;
        const double w0 = speedOfSound / headRadius;
        constexpr double alphaMin = 0.1, thetaMin = 150.0 * std::numbers::pi / 180.0;
        const double alpha = (1.0 + alphaMin / 2.0) + (1.0 - alphaMin / 2.0) * std::cos (theta / thetaMin * std::numbers::pi);
        const std::complex<double> shadow = std::complex<double> (1.0, alpha * w / (2.0 * w0)) / std::complex<double> (1.0, w / (2.0 * w0));

        const double a = headRadius / speedOfSound;
        const double t = theta < std::numbers::pi / 2.0 ? -a * std::cos (theta) : a * (theta - std::numbers::pi / 2.0);
        return shadow * std::polar (1.0, -w * (t + a));   // + a keeps the earliest arrival at 0
    }

    int filterLength (double sampleRate) noexcept
    {
        int len = 64;
        while (len < 0.005 * sampleRate)
            len *= 2;
        return len;
    }

    /** Windowed-sinc resampling of one impulse response. */
    std::vector<double> resample (const float* x, int n, double fsIn, double fsOut)
    {
        if (std::abs (fsIn - fsOut) < 1.0e-6)
            return std::vector<double> (x, x + n);

        const double ratio = fsOut / fsIn;
        const double fc = std::min (1.0, ratio);
        const double halfWidth = 16.0 / fc;
        const int outLen = static_cast<int> (std::ceil (n * ratio));
        std::vector<double> y (static_cast<std::size_t> (outLen), 0.0);

        for (int i = 0; i < outLen; ++i)
        {
            const double t = i / ratio;
            const int lo = std::max (0, static_cast<int> (std::ceil (t - halfWidth)));
            const int hi = std::min (n - 1, static_cast<int> (std::floor (t + halfWidth)));
            double s = 0.0;
            for (int m = lo; m <= hi; ++m)
            {
                const double tau = t - m;
                const double arg = std::numbers::pi * fc * tau;
                const double sinc = std::abs (arg) < 1.0e-12 ? 1.0 : std::sin (arg) / arg;
                const double u = tau / halfWidth;   // -1 .. 1
                const double win = 0.42 + 0.5 * std::cos (std::numbers::pi * u) + 0.08 * std::cos (2.0 * std::numbers::pi * u);
                s += x[m] * fc * sinc * win;
            }
            y[static_cast<std::size_t> (i)] = s;
        }
        return y;
    }

    int onsetOf (const std::vector<double>& ir) noexcept
    {
        double peak = 0.0;
        for (double v : ir)
            peak = std::max (peak, std::abs (v));
        for (std::size_t i = 0; i < ir.size(); ++i)
            if (std::abs (ir[i]) >= 0.1 * peak)
                return static_cast<int> (i);
        return 0;
    }
} // namespace

std::array<std::vector<float>, 2> Binaural::headModelHrir (const Vec3& d, double fs, int len, int delay)
{
    const int nfft = 2 * len;
    Fft fft;
    fft.prepare (nfft);
    std::array<std::vector<float>, 2> out;
    for (int ear = 0; ear < 2; ++ear)
    {
        std::vector<std::complex<double>> h (static_cast<std::size_t> (nfft));
        for (int k = 0; k <= nfft / 2; ++k)
        {
            auto v = headModel (d, ear, fs * k / nfft) * std::polar (1.0, -2.0 * std::numbers::pi * k * delay / nfft);
            if (k == nfft / 2) v = v.real();
            h[static_cast<std::size_t> (k)] = v;
            if (k > 0 && k < nfft / 2)
                h[static_cast<std::size_t> (nfft - k)] = std::conj (v);
        }
        fft.inverse (h.data());
        out[static_cast<std::size_t> (ear)].resize (static_cast<std::size_t> (len));
        for (int i = 0; i < len; ++i)
            out[static_cast<std::size_t> (ear)][static_cast<std::size_t> (i)] = static_cast<float> (h[static_cast<std::size_t> (i)].real());
    }
    return out;
}

void Binaural::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlock = std::max (1, maximumBlockSize);
    length = filterLength (sampleRate);
    bulkDelay = static_cast<int> (std::lround (0.001 * sampleRate));

    for (auto& ear : firs)
        for (auto& f : ear)
            f.prepare (length, maxBlock);

    for (auto* set : { &designed, &staging })
        for (auto& ear : *set)
            for (auto& ch : ear)
                ch.assign (static_cast<std::size_t> (length), 0.0f);

    ready.store (false);
    design();
    applyTaps (designed);   // audio is stopped during prepare, so apply directly
    designPending = false;
    reset();
}

void Binaural::reset() noexcept
{
    for (auto& ear : firs)
        for (auto& f : ear)
            f.reset();
}

void Binaural::applyTaps (const TapSet& taps) noexcept
{
    for (std::size_t e = 0; e < 2; ++e)
        for (std::size_t c = 0; c < 4; ++c)
            firs[e][c].setTaps (taps[e][c].data(), static_cast<int> (taps[e][c].size()));
}

bool Binaural::setHrirs (const HrirSet& set)
{
    if (! set.isValid())
        return false;
    custom = set;
    if (length > 0)
    {
        design();
        designPending = true;
        pumpPendingFilters();
    }
    return true;
}

void Binaural::useHeadModel()
{
    custom.reset();
    if (length > 0)
    {
        design();
        designPending = true;
        pumpPendingFilters();
    }
}

bool Binaural::pumpPendingFilters() noexcept
{
    if (! designPending)
        return false;
    if (ready.load (std::memory_order_acquire))
        return true;   // the audio thread has not taken the previous set yet

    for (std::size_t e = 0; e < 2; ++e)
        for (std::size_t c = 0; c < 4; ++c)
            std::copy (designed[e][c].begin(), designed[e][c].end(), staging[e][c].begin());
    ready.store (true, std::memory_order_release);
    designPending = false;
    return false;
}

void Binaural::design()
{
    const auto& vs = virtualSpeakers();
    const std::size_t numDirs = vs.directions.size();
    const int nfft = 2 * length;
    const auto bins = static_cast<std::size_t> (nfft / 2 + 1);

    Fft fft;
    fft.prepare (nfft);

    // HRTF spectra per virtual speaker and ear.
    std::vector<std::array<std::vector<std::complex<double>>, 2>> hrtf (numDirs);
    for (auto& h : hrtf)
        for (auto& e : h)
            e.assign (bins, {});

    if (! custom)
    {
        for (std::size_t k = 0; k < numDirs; ++k)
            for (int ear = 0; ear < 2; ++ear)
                for (std::size_t b = 0; b < bins; ++b)
                    hrtf[k][static_cast<std::size_t> (ear)][b] = headModel (vs.directions[k], ear, sampleRate * static_cast<double> (b) / nfft)
                        * std::polar (1.0, -2.0 * std::numbers::pi * static_cast<double> (b) * bulkDelay / nfft);
    }
    else
    {
        const auto& set = *custom;

        // Nearest measured direction for each virtual speaker, resampled to our rate.
        std::vector<std::array<std::vector<double>, 2>> irs (numDirs);
        int firstOnset = 1 << 30;
        for (std::size_t k = 0; k < numDirs; ++k)
        {
            std::size_t best = 0;
            double bestDot = -2.0;
            for (std::size_t m = 0; m < set.directions.size(); ++m)
            {
                const double dp = set.directions[m].normalised().dot (vs.directions[k]);
                if (dp > bestDot)
                    bestDot = dp, best = m;
            }
            const auto offset = best * static_cast<std::size_t> (set.length);
            irs[k][0] = resample (set.left.data() + offset, set.length, set.sampleRate, sampleRate);
            irs[k][1] = resample (set.right.data() + offset, set.length, set.sampleRate, sampleRate);
            firstOnset = std::min ({ firstOnset, onsetOf (irs[k][0]), onsetOf (irs[k][1]) });
        }

        // Shift so the earliest arrival lands on the bulk delay, then transform.
        const int shift = bulkDelay - firstOnset;
        std::vector<std::complex<double>> buf (static_cast<std::size_t> (nfft));
        for (std::size_t k = 0; k < numDirs; ++k)
            for (std::size_t ear = 0; ear < 2; ++ear)
            {
                std::fill (buf.begin(), buf.end(), std::complex<double> {});
                const auto& ir = irs[k][ear];
                for (std::size_t i = 0; i < ir.size(); ++i)
                {
                    const long j = static_cast<long> (i) + shift;
                    if (j >= 0 && j < length)
                        buf[static_cast<std::size_t> (j)] = ir[i];
                }
                fft.forward (buf.data());
                std::copy_n (buf.begin(), bins, hrtf[k][ear].begin());
            }

        // Diffuse-field equalisation: divide by the smoothed RMS response over all directions.
        std::vector<double> power (bins, 0.0);
        for (std::size_t k = 0; k < numDirs; ++k)
            for (std::size_t ear = 0; ear < 2; ++ear)
                for (std::size_t b = 0; b < bins; ++b)
                    power[b] += std::norm (hrtf[k][ear][b]) / (2.0 * static_cast<double> (numDirs));

        std::vector<double> eq (bins, 1.0);
        for (std::size_t b = 0; b < bins; ++b)
        {
            // Third-octave smoothing.
            const double lo = static_cast<double> (b) / 1.122, hi = static_cast<double> (b) * 1.122;
            double s = 0.0;
            int count = 0;
            for (auto j = static_cast<std::size_t> (std::floor (lo)); j <= std::min (bins - 1, static_cast<std::size_t> (std::ceil (hi))); ++j)
                s += power[j], ++count;
            const double rms = std::sqrt (s / std::max (1, count));
            eq[b] = std::clamp (1.0 / std::max (rms, 1.0e-9), 0.25, 4.0);   // +/- 12 dB
        }
        for (auto& h : hrtf)
            for (auto& e : h)
                for (std::size_t b = 0; b < bins; ++b)
                    e[b] *= eq[b];
    }

    // Combine decoder and HRTFs into one filter per ear and AmbiX channel.
    const double maxReW = std::sqrt (2.0 / (1.0 + 1.0 / 3.0));
    const std::array<double, 4> maxRe { maxReW, maxReW / std::sqrt (3.0), maxReW / std::sqrt (3.0), maxReW / std::sqrt (3.0) };

    std::vector<std::complex<double>> buf (static_cast<std::size_t> (nfft));
    for (std::size_t ear = 0; ear < 2; ++ear)
        for (std::size_t c = 0; c < 4; ++c)
        {
            std::fill (buf.begin(), buf.end(), std::complex<double> {});
            for (std::size_t b = 0; b < bins; ++b)
            {
                const double f = sampleRate * static_cast<double> (b) / nfft;
                const double low = 1.0 / (1.0 + std::pow (f / crossoverHz, 4.0));
                const double weight = low + (1.0 - low) * maxRe[c];
                std::complex<double> s = 0.0;
                for (std::size_t k = 0; k < numDirs; ++k)
                    s += vs.decode[k][c] * hrtf[k][ear][b];
                buf[b] = s * weight;
            }
            buf[bins - 1] = buf[bins - 1].real();
            for (std::size_t b = 1; b + 1 < bins; ++b)
                buf[static_cast<std::size_t> (nfft) - b] = std::conj (buf[b]);
            fft.inverse (buf.data());

            auto& taps = designed[ear][c];
            taps.assign (static_cast<std::size_t> (length), 0.0f);
            const int fade = length / 8;
            for (int i = 0; i < length; ++i)
            {
                double win = 1.0;
                if (i >= length - fade)
                    win = 0.5 + 0.5 * std::cos (std::numbers::pi * (i - (length - fade)) / fade);
                taps[static_cast<std::size_t> (i)] = static_cast<float> (buf[static_cast<std::size_t> (i)].real() * win);
            }
        }
}

void Binaural::process (const float* const* world, int numSamples, float* left, float* right) noexcept
{
    if (length == 0)
        return;

    if (ready.load (std::memory_order_acquire))
    {
        applyTaps (staging);
        ready.store (false, std::memory_order_release);
    }

    float* outs[2] = { left, right };
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = std::min (maxBlock, numSamples - start);
        for (std::size_t e = 0; e < 2; ++e)
            for (std::size_t c = 0; c < 4; ++c)
                firs[e][c].process (world[c] + start, outs[e] + start, len, c > 0);
    }
}

} // namespace sf2
