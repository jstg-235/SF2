#include "sf2/SofaFile.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

using namespace sf2;

TEST (Sofa, LoadsMeasuredHrtfs)
{
    std::string error;
    const auto set = loadSofaFile (SF2_TEST_SOFA_FILE, error);
    ASSERT_TRUE (set.has_value()) << error;
    EXPECT_TRUE (set->isValid());
    EXPECT_GT (set->directions.size(), 100u);
    EXPECT_GT (set->sampleRate, 40000.0);

    // A source on the left is louder in the left ear.
    std::size_t left = 0;
    double best = -2.0;
    for (std::size_t i = 0; i < set->directions.size(); ++i)
        if (set->directions[i].y > best)
            best = set->directions[i].y, left = i;
    double el = 0.0, er = 0.0;
    for (int n = 0; n < set->length; ++n)
    {
        const auto i = left * static_cast<std::size_t> (set->length) + static_cast<std::size_t> (n);
        el += set->left[i] * set->left[i];
        er += set->right[i] * set->right[i];
    }
    EXPECT_GT (el, 2.0 * er);
}

TEST (Sofa, RejectsOtherFiles)
{
    std::string error;
    const char junk[] = "this is not a SOFA file";
    EXPECT_FALSE (loadSofaData (junk, sizeof (junk), error).has_value());
    EXPECT_FALSE (error.empty());
    EXPECT_FALSE (loadSofaFile ("/no/such/file.sofa", error).has_value());
}

TEST (Sofa, DrivesTheBinauralRenderer)
{
    std::string error;
    const auto set = loadSofaFile (SF2_TEST_SOFA_FILE, error);
    ASSERT_TRUE (set.has_value()) << error;

    Binaural bin;
    bin.prepare (96000.0, 512);   // different rate from the file: exercises resampling
    ASSERT_TRUE (bin.setHrirs (*set));

    // W + Y for a source on the left.
    const int n = 4096;
    std::vector<float> w (n), y (n), zero (n, 0.0f), l (n), r (n);
    for (int i = 0; i < n; ++i)
        w[static_cast<std::size_t> (i)] = y[static_cast<std::size_t> (i)] = static_cast<float> (std::sin (2.0 * std::numbers::pi * 2000.0 * i / 96000.0));
    const float* world[4] = { w.data(), y.data(), zero.data(), zero.data() };
    bin.process (world, n, l.data(), r.data());

    double el = 0.0, er = 0.0;
    for (int i = 1024; i < n; ++i)
    {
        el += l[static_cast<std::size_t> (i)] * l[static_cast<std::size_t> (i)];
        er += r[static_cast<std::size_t> (i)] * r[static_cast<std::size_t> (i)];
    }
    EXPECT_GT (el, 2.0 * er);
    EXPECT_TRUE (std::isfinite (el));
}
