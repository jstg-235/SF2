// Tests for the v1.1 quality features: spacing correction, capsule stage, spaced pairs,
// binaural and the sound-field map.

#include "TestHelpers.h"

#include "sf2/ArrayCorrection.h"
#include "sf2/Binaural.h"
#include "sf2/Engine.h"
#include "sf2/Fft.h"
#include "sf2/SoundFieldMap.h"
#include "sf2/SpacedPair.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

using namespace sf2;
using namespace sf2::test;

namespace
{
/** Mean squared error of the array's AmbiX against the ideal, relative to the ideal, over directions. */
double relativeError (const MicProfile& p, double f, const CorrectionGains& g)
{
    std::mt19937 rng (5);
    double err = 0.0, ref = 0.0;
    for (int n = 0; n < 400; ++n)
    {
        const Vec3 d = randomDirection (rng);
        const auto b = spacedArrayAmbiX (p, d, f);
        const auto t = encodeAmbiX (d);
        for (std::size_t a = 0; a < 4; ++a)
        {
            const auto h = a == 0 ? g.w : g.xyz;
            err += std::norm (b[a] * h - t[a]);
            ref += t[a] * t[a];
        }
    }
    return err / ref;
}

std::vector<std::complex<double>> spectrum (const std::vector<float>& taps, int nfft)
{
    Fft fft;
    fft.prepare (nfft);
    std::vector<std::complex<double>> v (static_cast<std::size_t> (nfft));
    for (std::size_t i = 0; i < taps.size() && i < v.size(); ++i)
        v[i] = taps[i];
    fft.forward (v.data());
    return v;
}

std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> g (0.0f, 0.3f);
    std::vector<float> v (static_cast<std::size_t> (n));
    for (auto& x : v)
        x = g (rng);
    return v;
}

/** Plane wave of `signal` from direction d, as 4 world-frame AmbiX channels. */
std::vector<std::vector<float>> encode (const std::vector<float>& signal, const Vec3& d)
{
    const auto e = encodeAmbiX (d);
    std::vector<std::vector<float>> b (4, signal);
    for (std::size_t c = 0; c < 4; ++c)
        for (auto& x : b[c])
            x = static_cast<float> (x * e[c]);
    return b;
}

double energy (const std::vector<float>& x, std::size_t from = 0)
{
    double s = 0.0;
    for (std::size_t i = from; i < x.size(); ++i)
        s += static_cast<double> (x[i]) * x[i];
    return s;
}

/** Lag (in samples) at which b best matches a; positive when b arrives later. */
int bestLag (const std::vector<float>& a, const std::vector<float>& b, int maxLag)
{
    int best = 0;
    double bestValue = -1.0e300;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double s = 0.0;
        for (int i = maxLag; i + maxLag < static_cast<int> (a.size()); ++i)
            s += static_cast<double> (a[static_cast<std::size_t> (i)]) * b[static_cast<std::size_t> (i + lag)];
        if (s > bestValue)
            bestValue = s, best = lag;
    }
    return best;
}

/** Sine of frequency f, `n` samples. */
std::vector<float> sine (double f, double sr, int n)
{
    std::vector<float> v (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t> (i)] = static_cast<float> (std::sin (2.0 * std::numbers::pi * f * i / sr));
    return v;
}
} // namespace

//==============================================================================
TEST (SpacingCorrection, IsTransparentAtLowFrequencies)
{
    for (std::size_t m = 0; m < numMicModels; ++m)
    {
        const auto g = designCorrectionAt (getMicProfile (static_cast<MicModel> (m)), 50.0);
        EXPECT_NEAR (std::abs (g.w), 1.0, 1e-3);
        EXPECT_NEAR (std::abs (g.xyz), 1.0, 1e-3);
    }
}

TEST (SpacingCorrection, ReducesErrorBelowAliasing)
{
    for (std::size_t m = 0; m < numMicModels; ++m)
    {
        const auto& p = getMicProfile (static_cast<MicModel> (m));
        const double fa = aliasingFrequency (p);
        for (double f : { 0.3 * fa, 0.5 * fa, 0.7 * fa })
        {
            const double before = relativeError (p, f, {});
            const double after = relativeError (p, f, designCorrectionAt (p, f));
            EXPECT_LT (after, 0.6 * before) << p.name << " at " << f << " Hz";
        }
    }
}

TEST (SpacingCorrection, GainIsCapped)
{
    const auto& p = getMicProfile (MicModel::RodeNTSF1);
    for (double f = 100.0; f < 24000.0; f *= 1.2)
    {
        const auto g = designCorrectionAt (p, f);
        EXPECT_LE (std::abs (g.w), 2.0 + 1e-9);
        EXPECT_LE (std::abs (g.xyz), 2.0 + 1e-9);
    }
}

TEST (SpacingCorrection, FirMatchesDesign)
{
    const double sr = 48000.0;
    const auto& p = getMicProfile (MicModel::RodeNTSF1);
    const auto filters = designArrayCorrection (p, sr);
    ASSERT_EQ (static_cast<int> (filters.w.size()), arrayCorrectionLength (sr));
    EXPECT_EQ (filters.delay, static_cast<int> (filters.w.size()) / 2);

    const int nfft = 512;
    const auto w = spectrum (filters.w, nfft);
    const auto x = spectrum (filters.xyz, nfft);
    for (int k = 1; k < nfft / 2; ++k)
    {
        const double f = sr * k / nfft;
        if (f > 12000.0)
            break;
        const auto g = designCorrectionAt (p, f);
        const auto delay = std::polar (1.0, -2.0 * std::numbers::pi * k * filters.delay / nfft);
        EXPECT_NEAR (std::abs (w[static_cast<std::size_t> (k)] - g.w * delay), 0.0, 0.08) << f;
        EXPECT_NEAR (std::abs (x[static_cast<std::size_t> (k)] - g.xyz * delay), 0.0, 0.08) << f;
    }
}

TEST (SpacingCorrection, EngineAddsItsDelay)
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setOutputLayout (OutputMode::AmbiX);
    EngineParams p;
    p.spacingCorrection = true;
    const int delay = arrayCorrectionLength (48000.0) / 2;
    EXPECT_EQ (e.getLatencySamples (p), delay);

    // An impulse on all four capsules is a pure W signal; it should come out of W at the delay.
    std::vector<std::vector<float>> buf (4, std::vector<float> (256, 0.0f));
    for (auto& c : buf)
        c[0] = 1.0f;
    float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
    e.process (ptrs, 4, ptrs, 4, 256, p);

    const auto expectedW = e.computeGainMatrix (p)[0][0] * 4.0;
    double sum = 0.0;
    std::size_t peakAt = 0;
    for (std::size_t i = 0; i < 256; ++i)
    {
        sum += buf[0][i];
        if (std::abs (buf[0][i]) > std::abs (buf[0][peakAt]))
            peakAt = i;
    }
    EXPECT_NEAR (sum, expectedW, 1e-3);
    EXPECT_EQ (static_cast<int> (peakAt), delay);
    EXPECT_EQ (e.getLatencySamples(), delay);
}

//==============================================================================
TEST (CapsuleStage, TrimAndPolarityScaleTheCapsule)
{
    Engine e;
    e.setOutputLayout (OutputMode::AmbiX);
    EngineParams p;
    const auto base = e.computeGainMatrix (p);
    p.capsuleTrimDb[1] = 6.0;
    p.capsuleInvert[2] = true;
    const auto g = e.computeGainMatrix (p);
    for (std::size_t r = 0; r < 4; ++r)
    {
        EXPECT_NEAR (g[r][0], base[r][0], 1e-12);
        EXPECT_NEAR (g[r][1], base[r][1] * std::pow (10.0, 6.0 / 20.0), 1e-9);
        EXPECT_NEAR (g[r][2], -base[r][2], 1e-12);
    }
}

TEST (CapsuleStage, WindHighPassCutsRumble)
{
    const double sr = 48000.0;
    const int n = 9600;
    Engine e;
    e.prepare (sr, n);
    e.setOutputLayout (OutputMode::AmbiX);
    EngineParams p;
    p.windHighPassHz = 120.0;

    auto wOut = [&] (double f)
    {
        e.reset();
        const auto s = sine (f, sr, n);
        std::vector<float> out[4];
        for (auto& o : out) o.assign (static_cast<std::size_t> (n), 0.0f);
        const float* in[4] = { s.data(), s.data(), s.data(), s.data() };
        float* outs[4] = { out[0].data(), out[1].data(), out[2].data(), out[3].data() };
        e.process (in, 4, outs, 4, n, p);
        return std::sqrt (energy (out[0], static_cast<std::size_t> (n / 2)) / (n / 2));
    };

    const double pass = wOut (1000.0);
    const double stop = wOut (20.0);
    EXPECT_LT (20.0 * std::log10 (stop / pass), -35.0);
}

//==============================================================================
TEST (SpacedPair, ZeroSpacingIsTheCoincidentPairDelayed)
{
    const double sr = 48000.0;
    const int n = 2048;
    SpacedPair pair;
    pair.prepare (sr, n);
    const int d = pair.getLatencySamples();

    std::mt19937 rng (9);
    const Vec3 dir = randomDirection (rng);
    const auto b = encode (noise (n, 4), dir);
    const float* world[4] = { b[0].data(), b[1].data(), b[2].data(), b[3].data() };
    std::vector<float> l (n), r (n);

    const PairConfig cfg { 55.0, 0.0, 0.5 };
    pair.process (world, n, cfg, l.data(), r.data());

    // Skip the first frames while the analysis settles; with no spacing the phase factor is 1 anyway.
    const double gl = 0.5 + 0.5 * directionFromAzEl (55.0, 0.0).dot (dir);
    const double gr = 0.5 + 0.5 * directionFromAzEl (-55.0, 0.0).dot (dir);
    for (int i = d; i < n; ++i)
    {
        EXPECT_NEAR (l[static_cast<std::size_t> (i)], gl * b[0][static_cast<std::size_t> (i - d)], 1e-4);
        EXPECT_NEAR (r[static_cast<std::size_t> (i)], gr * b[0][static_cast<std::size_t> (i - d)], 1e-4);
    }
}

TEST (SpacedPair, SourceOnTheLeftReachesTheLeftMicFirst)
{
    const double sr = 48000.0;
    const int n = 8192;
    SpacedPair pair;
    pair.prepare (sr, n);

    const auto b = encode (noise (n, 2), directionFromAzEl (90.0, 0.0));
    const float* world[4] = { b[0].data(), b[1].data(), b[2].data(), b[3].data() };
    std::vector<float> l (n), r (n);
    pair.process (world, n, { 0.0, 0.4, 0.0 }, l.data(), r.data());   // 40 cm AB

    const int expected = static_cast<int> (std::lround (0.4 / speedOfSound * sr));   // 56 samples
    EXPECT_NEAR (bestLag (l, r, 100), expected, 2);

    // Halfway round to the front, the delay should shrink with sin(azimuth).
    pair.reset();
    const auto b45 = encode (noise (n, 3), directionFromAzEl (30.0, 0.0));
    const float* w45[4] = { b45[0].data(), b45[1].data(), b45[2].data(), b45[3].data() };
    pair.process (w45, n, { 0.0, 0.4, 0.0 }, l.data(), r.data());
    EXPECT_NEAR (bestLag (l, r, 100), expected / 2, 2);
}

TEST (SpacedPair, EngineReportsConstantLatency)
{
    Engine e;
    e.prepare (48000.0, 512);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    EXPECT_EQ (e.getLatencySamples (p), 0);
    p.stereoType = StereoType::ORTF;
    const int ortf = e.getLatencySamples (p);
    EXPECT_GT (ortf, 0);
    p.stereoType = StereoType::AB;
    p.abSpacingCm = 90.0;
    EXPECT_EQ (e.getLatencySamples (p), ortf);
}

//==============================================================================
namespace
{
struct EarSignals { std::vector<float> left, right; };

EarSignals renderBinaural (Binaural& bin, const Vec3& dir, const std::vector<float>& signal)
{
    const auto b = encode (signal, dir);
    const float* world[4] = { b[0].data(), b[1].data(), b[2].data(), b[3].data() };
    EarSignals out { std::vector<float> (signal.size()), std::vector<float> (signal.size()) };
    bin.process (world, static_cast<int> (signal.size()), out.left.data(), out.right.data());
    return out;
}
} // namespace

TEST (Binaural, HeadModelLateralises)
{
    const double sr = 48000.0;
    const int n = 8192;
    Binaural bin;
    bin.prepare (sr, n);
    EXPECT_TRUE (bin.isUsingHeadModel());
    EXPECT_EQ (bin.getLatencySamples(), 48);

    const auto left = renderBinaural (bin, directionFromAzEl (90.0, 0.0), sine (3000.0, sr, n));
    EXPECT_GT (10.0 * std::log10 (energy (left.left, 1000) / energy (left.right, 1000)), 3.0);

    bin.reset();
    const auto front = renderBinaural (bin, directionFromAzEl (0.0, 0.0), noise (n, 6));
    EXPECT_NEAR (10.0 * std::log10 (energy (front.left, 1000) / energy (front.right, 1000)), 0.0, 0.2);

    bin.reset();
    const auto ln = renderBinaural (bin, directionFromAzEl (90.0, 0.0), noise (n, 8));
    EXPECT_GT (bestLag (ln.left, ln.right, 60), 5);   // right ear hears it later
}

TEST (Binaural, MeasuredSetIsHandedToTheAudioThread)
{
    const double sr = 48000.0;
    const int n = 4096;
    Binaural bin;
    bin.prepare (sr, n);

    // A synthetic "measured" set at 44.1 kHz: left ear only, so a swap is easy to spot.
    HrirSet set;
    set.sampleRate = 44100.0;
    set.length = 128;
    for (int az = 0; az < 360; az += 15)
        for (int el = -60; el <= 60; el += 30)
        {
            set.directions.push_back (directionFromAzEl (az, el));
            const auto ir = Binaural::headModelHrir (set.directions.back(), set.sampleRate, set.length, 20);
            set.left.insert (set.left.end(), ir[0].begin(), ir[0].end());
            set.right.insert (set.right.end(), set.length, 0.0f);
        }
    ASSERT_TRUE (set.isValid());
    ASSERT_TRUE (bin.setHrirs (set));
    EXPECT_FALSE (bin.isUsingHeadModel());
    EXPECT_FALSE (bin.pumpPendingFilters());   // handed over straight away: nothing was waiting

    const auto out = renderBinaural (bin, directionFromAzEl (0.0, 0.0), noise (n, 1));
    EXPECT_GT (energy (out.left, 1000), 1.0);
    EXPECT_LT (energy (out.right, 1000), 1e-9);

    HrirSet bad;
    EXPECT_FALSE (bin.setHrirs (bad));
}

TEST (Binaural, EngineRoutesStereoToHeadphones)
{
    Engine e;
    e.prepare (48000.0, 512);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.stereoType = StereoType::Binaural;
    EXPECT_EQ (e.getLatencySamples (p), 48);

    const auto b = encode (noise (512, 3), directionFromAzEl (90.0, 0.0));
    std::vector<std::vector<float>> buf = b;
    float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
    e.process (ptrs, 4, ptrs, 4, 512, p);
    EXPECT_GT (energy (buf[0], 100), energy (buf[1], 100));
    EXPECT_EQ (energy (buf[2]), 0.0);
}

//==============================================================================
TEST (SoundFieldMap, PeaksAtTheSource)
{
    Engine e;
    const int n = 4096;
    e.prepare (48000.0, n);
    e.setOutputLayout (OutputMode::AmbiX);
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.yawDeg = 30.0;   // map shows the rotated scene: source at 60 + 30 = 90 degrees

    const auto b = encode (noise (n, 12), directionFromAzEl (60.0, 20.0));
    std::vector<std::vector<float>> buf = b;
    float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
    e.process (ptrs, 4, ptrs, 4, n, p);

    auto& m = e.getMeters();
    EXPECT_NEAR (m.covarianceSamples.load(), static_cast<float> (n), 0.5f);
    Covariance c {};
    for (std::size_t i = 0; i < c.size(); ++i)
        c[i] = m.covariance[i].load();

    const int w = 72, h = 36;
    std::vector<float> map;
    renderEnergyMap (c, w, h, map);
    std::size_t best = 0;
    for (std::size_t i = 0; i < map.size(); ++i)
        if (map[i] > map[best])
            best = i;
    const double az = 180.0 - 360.0 * (static_cast<double> (best % w) + 0.5) / w;
    const double el = 90.0 - 180.0 * (static_cast<double> (best / w) + 0.5) / h;
    EXPECT_NEAR (az, 90.0, 6.0);
    EXPECT_NEAR (el, 20.0, 6.0);
    EXPECT_FLOAT_EQ (map[best], 1.0f);

    m.resetCovariance.store (true);
    e.process (ptrs, 4, ptrs, 4, 16, p);
    EXPECT_NEAR (m.covarianceSamples.load(), 16.0f, 0.5f);
    EXPECT_GT (m.inputPeak[0].load(), 0.1f);
}
