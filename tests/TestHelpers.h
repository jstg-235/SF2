#pragma once

#include "sf2/AFormat.h"
#include "sf2/MicProfiles.h"

#include <array>
#include <random>

namespace sf2::test
{

/** Ideal capsule outputs for a unit plane wave arriving from world direction d,
    with the mic mounted so that world = mount * micFrame. */
inline std::array<double, 4> capsuleSignals (const MicProfile& p, const Vec3& worldDir, const Mat3& mount = Mat3::identity())
{
    const Vec3 micDir = mount.transposed() * worldDir;
    std::array<double, 4> r {};
    for (std::size_t i = 0; i < 4; ++i)
        r[i] = p.omniPart + p.dipolePart * p.capsuleAxes[i].dot (micDir);
    return r;
}

inline std::array<double, 4> applyMatrix (const Mat4& m, const std::array<double, 4>& v)
{
    std::array<double, 4> r {};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j)
            r[i] += m[i][j] * v[j];
    return r;
}

inline Vec3 randomDirection (std::mt19937& rng)
{
    std::normal_distribution<double> n (0.0, 1.0);
    return Vec3 { n (rng), n (rng), n (rng) }.normalised();
}

} // namespace sf2::test
