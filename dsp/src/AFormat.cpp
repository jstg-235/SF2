#include "sf2/AFormat.h"

#include <cmath>
#include <utility>

namespace sf2
{

Mat4 multiply (const Mat4& a, const Mat4& b) noexcept
{
    Mat4 r {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
        {
            double s = 0.0;
            for (int k = 0; k < 4; ++k)
                s += a[i][k] * b[k][j];
            r[i][j] = s;
        }
    return r;
}

bool invert (const Mat4& in, Mat4& out) noexcept
{
    Mat4 a = in;
    out = identity4();

    for (int col = 0; col < 4; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 4; ++row)
            if (std::abs (a[row][col]) > std::abs (a[pivot][col]))
                pivot = row;

        if (std::abs (a[pivot][col]) < 1.0e-12)
            return false;

        std::swap (a[pivot], a[col]);
        std::swap (out[pivot], out[col]);

        const double inv = 1.0 / a[col][col];
        for (int j = 0; j < 4; ++j)
        {
            a[col][j] *= inv;
            out[col][j] *= inv;
        }

        for (int row = 0; row < 4; ++row)
        {
            if (row == col)
                continue;
            const double f = a[row][col];
            for (int j = 0; j < 4; ++j)
            {
                a[row][j] -= f * a[col][j];
                out[row][j] -= f * out[col][j];
            }
        }
    }
    return true;
}

Mat4 aFormatToAmbiX (const MicProfile& profile) noexcept
{
    // Capsule model: r_i = omni + dipole * (u_i . d) = C * [1, d.x, d.y, d.z]
    Mat4 c {};
    for (int i = 0; i < 4; ++i)
    {
        const auto& u = profile.capsuleAxes[static_cast<std::size_t> (i)];
        c[i] = { profile.omniPart, profile.dipolePart * u.x, profile.dipolePart * u.y, profile.dipolePart * u.z };
    }

    Mat4 cInv {};
    if (! invert (c, cInv))
        return identity4();

    // [1, x, y, z] -> ACN order [W, Y, Z, X]
    const Mat4 toAcn { { { 1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 }, { 0, 1, 0, 0 } } };
    return multiply (toAcn, cInv);
}

Mat4 fuMaToAmbiX() noexcept
{
    const double r2 = std::sqrt (2.0);
    // FuMa input order W, X, Y, Z.
    return { { { r2, 0, 0, 0 },   // W
               { 0, 0, 1, 0 },    // Y
               { 0, 0, 0, 1 },    // Z
               { 0, 1, 0, 0 } } }; // X
}

Mat4 ambiXToFuMa() noexcept
{
    const double ir2 = 1.0 / std::sqrt (2.0);
    // AmbiX input order W, Y, Z, X.
    return { { { ir2, 0, 0, 0 },  // W
               { 0, 0, 0, 1 },    // X
               { 0, 1, 0, 0 },    // Y
               { 0, 0, 1, 0 } } }; // Z
}

Mat4 ambiXRotation (const Mat3& r) noexcept
{
    // Cartesian component index (x, y, z) -> ACN channel index.
    constexpr int acn[3] = { 3, 1, 2 };

    Mat4 m {};
    m[0][0] = 1.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            m[acn[i]][acn[j]] = r.m[static_cast<std::size_t> (i)][static_cast<std::size_t> (j)];
    return m;
}

} // namespace sf2
