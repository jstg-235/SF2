#include "TestHelpers.h"

#include "sf2/Engine.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sf2;
using namespace sf2::test;

namespace
{
/** Response of output o for a unit plane wave from direction d (A-format, upright NT-SF1 by default). */
double response (const Engine& e, const EngineParams& p, int o, const Vec3& d, const Mat3& mount = Mat3::identity())
{
    const auto g = e.computeGainMatrix (p);
    const auto x = capsuleSignals (getMicProfile (p.micModel), d, mount);
    double s = 0.0;
    for (std::size_t j = 0; j < 4; ++j)
        s += g[static_cast<std::size_t> (o)][j] * x[j];
    return s;
}

std::vector<Speaker> speakers (std::initializer_list<SpeakerRole> roles)
{
    std::vector<Speaker> v;
    for (auto r : roles)
        v.push_back (speakerForRole (r));
    return v;
}
} // namespace

TEST (Engine, MonoCardioidPattern)
{
    Engine e;
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Centre }));
    EngineParams p;
    p.pattern = 0.5;
    EXPECT_NEAR (response (e, p, 0, { 1, 0, 0 }), 1.0, 1e-9);
    EXPECT_NEAR (response (e, p, 0, { 0, 1, 0 }), 0.5, 1e-9);
    EXPECT_NEAR (response (e, p, 0, { -1, 0, 0 }), 0.0, 1e-9);
}

TEST (Engine, OmniAndFigure8Extremes)
{
    Engine e;
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Centre }));
    EngineParams p;
    p.pattern = 0.0;
    EXPECT_NEAR (response (e, p, 0, { -1, 0, 0 }), 1.0, 1e-9);
    p.pattern = 1.0;
    EXPECT_NEAR (response (e, p, 0, { -1, 0, 0 }), -1.0, 1e-9);
    EXPECT_NEAR (response (e, p, 0, { 0, 0, 1 }), 0.0, 1e-9);
}

TEST (Engine, StereoPairIsAimedLeftAndRight)
{
    Engine e;
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    p.stereoAngleDeg = 90.0;
    const Vec3 left = directionFromAzEl (45, 0);
    EXPECT_NEAR (response (e, p, 0, left), 1.0, 1e-9);
    EXPECT_GT (response (e, p, 0, left), response (e, p, 1, left));
}

TEST (Engine, YawSteersTheScene)
{
    Engine e;
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Centre }));
    EngineParams p;
    p.yawDeg = -90.0;   // a source on the left moves to the front
    EXPECT_NEAR (response (e, p, 0, { 0, 1, 0 }), 1.0, 1e-9);
}

TEST (Engine, OrientationCompensatesTheMount)
{
    Engine e;
    e.setOutputLayout (OutputMode::AmbiX);

    struct Case { MountOrientation o; Mat3 mount; };
    const Case cases[] = { { MountOrientation::Upright, Mat3::identity() },
                           { MountOrientation::Inverted, rotationX (180) },
                           { MountOrientation::EndFire, rotationY (90) } };

    std::mt19937 rng (7);
    for (const auto& c : cases)
    {
        EngineParams p;
        p.orientation = c.o;
        for (int n = 0; n < 50; ++n)
        {
            const Vec3 d = randomDirection (rng);
            const auto ideal = encodeAmbiX (d);
            for (int ch = 0; ch < 4; ++ch)
                EXPECT_NEAR (response (e, p, ch, d, c.mount), ideal[static_cast<std::size_t> (ch)], 1e-9);
        }
    }
}

TEST (Engine, BFormatInputPassesThrough)
{
    Engine e;
    e.setOutputLayout (OutputMode::FuMa);
    EngineParams p;
    p.inputFormat = InputFormat::FuMa;
    const auto g = e.computeGainMatrix (p);
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j)
            EXPECT_NEAR (g[i][j], i == j ? 1.0 : 0.0, 1e-12);
}

TEST (Engine, GainScalesOutputs)
{
    Engine e;
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Centre }));
    EngineParams p;
    p.gainDb = -6.0;
    EXPECT_NEAR (response (e, p, 0, { 1, 0, 0 }), std::pow (10.0, -6.0 / 20.0), 1e-9);
}

TEST (Engine, SurroundFrontSpeakerWinsForFrontSource)
{
    Engine e;
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Left, SpeakerRole::Right, SpeakerRole::Centre, SpeakerRole::LFE,
                                                         SpeakerRole::LeftSurround, SpeakerRole::RightSurround }));
    EngineParams p;
    const Vec3 front { 1, 0, 0 };
    const double c = response (e, p, 2, front);
    EXPECT_GT (c, response (e, p, 0, front));
    EXPECT_GT (c, response (e, p, 4, front));
}

TEST (Engine, ProcessesInPlaceWithAnyBlockSize)
{
    Engine e;
    e.prepare (48000.0, 64);
    e.setOutputLayout (OutputMode::AmbiX);
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;

    for (int n : { 0, 1, 17, 64, 200 })
    {
        std::vector<std::vector<float>> buf (4, std::vector<float> (static_cast<std::size_t> (n), 0.25f));
        float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
        e.process (ptrs, 4, ptrs, 4, n, p);
        for (auto& ch : buf)
            for (float v : ch)
                EXPECT_NEAR (v, 0.25f, 1e-6f);
    }
}

TEST (Engine, ParameterChangesRampWithoutJumps)
{
    Engine e;
    const int n = 512;
    e.prepare (48000.0, n);
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::Centre }));
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.pattern = 1.0;

    std::vector<float> w (n, 0.0f), y (n, 0.0f), z (n, 0.0f), x (n, 1.0f), out (n, 0.0f);
    const float* in[4] = { w.data(), y.data(), z.data(), x.data() };
    float* outs[1] = { out.data() };

    e.process (in, 4, outs, 1, n, p);   // settles at yaw 0: output = 1
    p.yawDeg = 180.0;                   // target output = -1
    e.process (in, 4, outs, 1, n, p);

    EXPECT_NEAR (out.back(), -1.0f, 1e-5f);
    for (int i = 1; i < n; ++i)
        EXPECT_LT (std::abs (out[static_cast<std::size_t> (i)] - out[static_cast<std::size_t> (i - 1)]), 0.01f);
}

TEST (Engine, LfeIsLowPassed)
{
    const double sr = 48000.0;
    const int n = 4800;
    Engine e;
    e.prepare (sr, n);
    e.setOutputLayout (OutputMode::Speakers, speakers ({ SpeakerRole::LFE }));
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.lfeCutoffHz = 120.0;

    auto rmsAt = [&] (double freq)
    {
        e.reset();
        std::vector<float> w (n), zero (n, 0.0f), out (n);
        for (int i = 0; i < n; ++i)
            w[static_cast<std::size_t> (i)] = static_cast<float> (std::sin (2.0 * std::numbers::pi * freq * i / sr));
        const float* in[4] = { w.data(), zero.data(), zero.data(), zero.data() };
        float* outs[1] = { out.data() };
        e.process (in, 4, outs, 1, n, p);
        double s = 0.0;
        for (int i = n / 2; i < n; ++i)
            s += out[static_cast<std::size_t> (i)] * out[static_cast<std::size_t> (i)];
        return std::sqrt (s / (n / 2));
    };

    const double pass = rmsAt (40.0);
    const double stop = rmsAt (1000.0);
    EXPECT_NEAR (pass, 1.0 / std::sqrt (2.0), 0.05);
    EXPECT_LT (20.0 * std::log10 (stop / pass), -40.0);
}

TEST (Engine, UnusedOutputsAreCleared)
{
    Engine e;
    e.prepare (48000.0, 32);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    std::vector<std::vector<float>> buf (4, std::vector<float> (32, 1.0f));
    float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
    e.process (ptrs, 4, ptrs, 4, 32, p);
    for (float v : buf[2]) EXPECT_EQ (v, 0.0f);
    for (float v : buf[3]) EXPECT_EQ (v, 0.0f);
}
