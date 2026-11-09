#include "TestHelpers.h"

#include "sf2/AFormat.h"

#include <gtest/gtest.h>

using namespace sf2;
using namespace sf2::test;

class AFormatAllMics : public ::testing::TestWithParam<int> {};

TEST_P (AFormatAllMics, RecoversIdealAmbiXForAnyDirection)
{
    const auto& profile = getMicProfile (static_cast<MicModel> (GetParam()));
    const Mat4 m = aFormatToAmbiX (profile);

    std::mt19937 rng (1234);
    for (int n = 0; n < 200; ++n)
    {
        const Vec3 d = randomDirection (rng);
        const auto b = applyMatrix (m, capsuleSignals (profile, d));
        const auto ideal = encodeAmbiX (d);
        for (std::size_t c = 0; c < 4; ++c)
            EXPECT_NEAR (b[c], ideal[c], 1e-9) << profile.name << " channel " << c;
    }
}

INSTANTIATE_TEST_SUITE_P (Mics, AFormatAllMics, ::testing::Range (0, static_cast<int> (numMicModels)));

TEST (AFormat, AllFiveMicModelsHaveNames)
{
    for (std::size_t i = 0; i < numMicModels; ++i)
        EXPECT_FALSE (getMicProfile (static_cast<MicModel> (i)).name.empty());
}

TEST (AFormat, FuMaRoundTrip)
{
    const Mat4 rt = multiply (fuMaToAmbiX(), ambiXToFuMa());
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_NEAR (rt[i][j], i == j ? 1.0 : 0.0, 1e-12);
}

TEST (AFormat, FuMaWIsMinus3dB)
{
    // A front plane wave in FuMa is W = 1/sqrt(2), X = 1.
    const auto fuma = applyMatrix (ambiXToFuMa(), encodeAmbiX ({ 1, 0, 0 }));
    EXPECT_NEAR (fuma[0], 1.0 / std::sqrt (2.0), 1e-12);
    EXPECT_NEAR (fuma[1], 1.0, 1e-12);
    EXPECT_NEAR (fuma[2], 0.0, 1e-12);
    EXPECT_NEAR (fuma[3], 0.0, 1e-12);
}

TEST (AFormat, InvertDetectsSingularMatrix)
{
    Mat4 out {};
    Mat4 singular {};
    EXPECT_FALSE (invert (singular, out));
}
