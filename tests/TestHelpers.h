#pragma once

#include "sf2/AFormat.h"
#include "sf2/MicProfiles.h"

#include <array>
#include <random>
#include <vector>

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

//==============================================================================
// Signals for the quality tests.

inline std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> g (0.0f, 0.3f);
    std::vector<float> v (static_cast<std::size_t> (n));
    for (auto& x : v)
        x = g (rng);
    return v;
}

/** Plane wave of `signal` from direction d, as 4 world-frame AmbiX channels. */
inline std::vector<std::vector<float>> encode (const std::vector<float>& signal, const Vec3& d)
{
    const auto e = encodeAmbiX (d);
    std::vector<std::vector<float>> b (4, signal);
    for (std::size_t c = 0; c < 4; ++c)
        for (auto& x : b[c])
            x = static_cast<float> (x * e[c]);
    return b;
}

inline double energy (const std::vector<float>& x, std::size_t from = 0)
{
    double s = 0.0;
    for (std::size_t i = from; i < x.size(); ++i)
        s += static_cast<double> (x[i]) * x[i];
    return s;
}

/** Lag (in samples) at which b best matches a; positive when b arrives later. */
inline int bestLag (const std::vector<float>& a, const std::vector<float>& b, int maxLag)
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

} // namespace sf2::test
