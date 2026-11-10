#include "TestHelpers.h"

#include "sf2/Engine.h"
#include "sf2/Fft.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

using namespace sf2;
using namespace sf2::test;

TEST (Fft, RoundTrip)
{
    Fft fft;
    fft.prepare (64);
    std::vector<std::complex<double>> v (64), orig (64);
    std::mt19937 rng (3);
    std::normal_distribution<double> n;
    for (auto& c : v) c = { n (rng), n (rng) };
    orig = v;
    fft.forward (v.data());
    fft.inverse (v.data());
    for (std::size_t i = 0; i < v.size(); ++i)
        EXPECT_NEAR (std::abs (v[i] - orig[i]), 0.0, 1e-12);
}

TEST (Fft, ImpulseIsFlat)
{
    Fft fft;
    fft.prepare (16);
    std::vector<std::complex<double>> v (16);
    v[0] = 1.0;
    fft.forward (v.data());
    for (auto c : v)
        EXPECT_NEAR (std::abs (c - std::complex<double> (1.0)), 0.0, 1e-12);
}

namespace
{
struct Rendered
{
    std::vector<float> out;
    int latency = 0;
};

/** Renders an A-format NT-SF1 recording of noise from `dir` through mono output. */
Rendered renderMono (const Vec3& dir, double focus, int numSamples, int blockSize, unsigned seed = 11)
{
    Engine e;
    e.prepare (48000.0, blockSize);
    e.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    e.setShotgunEnabled (true);
    EngineParams p;
    p.shotgunFocus = focus;

    const auto caps = capsuleSignals (getMicProfile (MicModel::RodeNTSF1), dir);
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, 0.1f);

    std::vector<std::vector<float>> in (4, std::vector<float> (static_cast<std::size_t> (numSamples)));
    for (std::size_t i = 0; i < static_cast<std::size_t> (numSamples); ++i)
    {
        const float s = noise (rng);
        for (std::size_t c = 0; c < 4; ++c)
            in[c][i] = static_cast<float> (caps[c]) * s;
    }

    Rendered r;
    r.out.assign (static_cast<std::size_t> (numSamples), 0.0f);
    r.latency = e.getLatencySamples();
    for (int start = 0; start < numSamples; start += blockSize)
    {
        const int len = std::min (blockSize, numSamples - start);
        const float* ins[4] = { in[0].data() + start, in[1].data() + start, in[2].data() + start, in[3].data() + start };
        float* outs[1] = { r.out.data() + start };
        e.process (ins, 4, outs, 1, len, p);
    }
    return r;
}

double rms (const std::vector<float>& v, std::size_t from)
{
    double s = 0.0;
    for (std::size_t i = from; i < v.size(); ++i)
        s += static_cast<double> (v[i]) * v[i];
    return std::sqrt (s / static_cast<double> (v.size() - from));
}
} // namespace

TEST (Shotgun, FocusZeroIsTheLinearMicDelayed)
{
    const int n = 48000;
    const Vec3 dir = directionFromAzEl (60, 20);
    const auto shot = renderMono (dir, 0.0, n, 256);
    ASSERT_GT (shot.latency, 0);

    // Linear cardioid response to the same source, delayed by the reported latency.
    Engine lin;
    lin.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    EngineParams p;
    const auto g = lin.computeGainMatrix (p);
    const auto caps = capsuleSignals (getMicProfile (MicModel::RodeNTSF1), dir);
    double resp = 0.0;
    for (std::size_t c = 0; c < 4; ++c)
        resp += g[0][c] * caps[c];

    std::mt19937 rng (11);
    std::normal_distribution<float> noise (0.0f, 0.1f);
    std::vector<float> src (static_cast<std::size_t> (n));
    for (auto& s : src) s = noise (rng);

    for (std::size_t i = static_cast<std::size_t> (shot.latency); i < static_cast<std::size_t> (n); ++i)
        ASSERT_NEAR (shot.out[i], resp * src[i - static_cast<std::size_t> (shot.latency)], 1e-4) << "sample " << i;
}

TEST (Shotgun, FullFocusRejectsTheSideMoreThanACardioid)
{
    const int n = 96000;
    const auto front = renderMono ({ 1, 0, 0 }, 1.0, n, 512);
    const auto side  = renderMono ({ 0, 1, 0 }, 1.0, n, 512);
    const auto from = static_cast<std::size_t> (n / 2);

    const double frontDb = 20.0 * std::log10 (rms (front.out, from) / 0.1);
    const double sideDb  = 20.0 * std::log10 (rms (side.out, from) / 0.1);

    EXPECT_NEAR (frontDb, 0.0, 1.0);        // on-axis passes unchanged
    EXPECT_LT (sideDb - frontDb, -15.0);    // a cardioid would only give -6 dB
}

TEST (Shotgun, LatencyOnlyWhenOnAndMono)
{
    Engine e;
    e.prepare (48000.0, 512);
    e.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    EXPECT_EQ (e.getLatencySamples(), 0);           // shotgun off: plain mono, no latency
    e.setShotgunEnabled (true);
    EXPECT_EQ (e.getLatencySamples(), 1024);
    e.setOutputLayout (OutputMode::Stereo);
    EXPECT_EQ (e.getLatencySamples(), 0);           // shotgun applies to mono only
    e.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    e.prepare (96000.0, 512);
    EXPECT_EQ (e.getLatencySamples(), 2048);
}

TEST (Shotgun, OffMeansPlainMonoWithoutDelay)
{
    Engine e;
    e.prepare (48000.0, 64);
    e.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.shotgunFocus = 1.0;   // ignored while shotgun is off

    std::vector<float> w (64, 1.0f), zero (64, 0.0f), out (64, 0.0f);
    const float* in[4] = { w.data(), zero.data(), zero.data(), zero.data() };
    float* outs[1] = { out.data() };
    e.process (in, 4, outs, 1, 64, p);
    for (float v : out)
        EXPECT_NEAR (v, 0.5f, 1e-6f);   // cardioid gives 0.5 for pure W, immediately
}
