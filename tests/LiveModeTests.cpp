// Live (low-latency) mode: every output path either has no latency or a much shorter one, the
// engine reports what it uses, and the quality features still work.

#include "TestHelpers.h"

#include "sf2/Binaural.h"
#include "sf2/Engine.h"
#include "sf2/SpacedPair.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using namespace sf2;
using namespace sf2::test;

TEST (LiveMode, SkipsSpacingCorrectionLatency)
{
    Engine e;
    e.prepare (48000.0, 256);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    p.spacingCorrection = true;
    EXPECT_GT (e.getLatencySamples (p), 0);
    p.liveMode = true;
    EXPECT_EQ (e.getLatencySamples (p), 0);

    // An impulse on all four capsules comes out at once.
    std::vector<std::vector<float>> buf (4, std::vector<float> (256, 0.0f));
    for (auto& c : buf)
        c[0] = 1.0f;
    float* ptrs[4] = { buf[0].data(), buf[1].data(), buf[2].data(), buf[3].data() };
    e.process (ptrs, 4, ptrs, 4, 256, p);
    EXPECT_GT (std::abs (buf[0][0]), 0.1f);
    EXPECT_EQ (e.getLatencySamples(), 0);
}

TEST (LiveMode, SpacedPairsAndShotgunUseTheShortStft)
{
    Engine e;
    e.prepare (48000.0, 512);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.stereoType = StereoType::ORTF;
    EXPECT_EQ (e.getLatencySamples (p), Engine::stftSize);
    p.liveMode = true;
    EXPECT_EQ (e.getLatencySamples (p), Engine::liveStftSize);

    e.setOutputLayout (OutputMode::Speakers, { speakerForRole (SpeakerRole::Centre) });
    e.setShotgunEnabled (true);
    EXPECT_EQ (e.getLatencySamples (p), Engine::liveStftSize);
    EXPECT_EQ (e.getShotgunLatencySamples (true), Engine::liveStftSize);
    p.liveMode = false;
    EXPECT_EQ (e.getLatencySamples (p), Engine::stftSize);
}

TEST (LiveMode, ShortStftScalesWithSampleRate)
{
    Engine e;
    e.prepare (96000.0, 512);
    e.setOutputLayout (OutputMode::Stereo);
    EngineParams p;
    p.inputFormat = InputFormat::AmbiX;
    p.stereoType = StereoType::AB;
    p.liveMode = true;
    EXPECT_EQ (e.getLatencySamples (p), 2 * Engine::liveStftSize);   // same 5.3 ms
}

TEST (LiveMode, ShortStftStillGivesTheRightTimeOfArrival)
{
    const double sr = 48000.0;
    const int n = 8192;
    SpacedPair pair;
    pair.prepare (sr, n, Engine::liveStftSize);
    EXPECT_EQ (pair.getLatencySamples(), Engine::liveStftSize);

    const auto b = encode (noise (n, 2), directionFromAzEl (90.0, 0.0));
    const float* world[4] = { b[0].data(), b[1].data(), b[2].data(), b[3].data() };
    std::vector<float> l (n), r (n);
    pair.process (world, n, { 0.0, 0.17, 0.5 }, l.data(), r.data());   // ORTF spacing

    const int expected = static_cast<int> (std::lround (0.17 / speedOfSound * sr));   // 24 samples
    EXPECT_NEAR (bestLag (l, r, 60), expected, 2);
}

TEST (LiveMode, BinauralSwitchesToTheShortBulkDelay)
{
    const double sr = 48000.0;
    const int n = 8192;
    Binaural bin;
    bin.prepare (sr, n);
    EXPECT_EQ (bin.getLatencySamples(), 48);

    bin.setLowLatency (true);
    // The audio thread takes the new filters (and their latency) with its next block.
    std::vector<float> silence (64, 0.0f), l (64), r (64);
    const float* world[4] = { silence.data(), silence.data(), silence.data(), silence.data() };
    bin.process (world, 64, l.data(), r.data());
    EXPECT_EQ (bin.getLatencySamples(), 19);

    // Still lateralises: louder and earlier in the left ear for a source on the left.
    bin.reset();
    const auto b = encode (noise (n, 8), directionFromAzEl (90.0, 0.0));
    const float* w[4] = { b[0].data(), b[1].data(), b[2].data(), b[3].data() };
    std::vector<float> left (n), right (n);
    bin.process (w, n, left.data(), right.data());
    EXPECT_GT (10.0 * std::log10 (energy (left, 1000) / energy (right, 1000)), 3.0);
    EXPECT_GT (bestLag (left, right, 60), 5);

    // The output starts no earlier than the reported latency allows for a frontal impulse.
    bin.reset();
    std::vector<float> imp (256, 0.0f);
    imp[0] = 1.0f;
    const auto bi = encode (imp, directionFromAzEl (0.0, 0.0));
    const float* wi[4] = { bi[0].data(), bi[1].data(), bi[2].data(), bi[3].data() };
    std::vector<float> li (256), ri (256);
    bin.process (wi, 256, li.data(), ri.data());
    std::size_t peak = 0;
    for (std::size_t i = 0; i < li.size(); ++i)
        if (std::abs (li[i]) > std::abs (li[peak]))
            peak = i;
    EXPECT_NEAR (static_cast<double> (peak), 19.0, 6.0);

    bin.setLowLatency (false);
    bin.process (world, 64, l.data(), r.data());
    EXPECT_EQ (bin.getLatencySamples(), 48);
}
