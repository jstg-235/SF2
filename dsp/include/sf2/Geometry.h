#pragma once

// Small fixed-size vector/matrix helpers for the ambisonic DSP.
//
// Coordinate convention (same as AmbiX / ACN-SN3D):
//   +x = front, +y = left, +z = up.
//   Azimuth is measured from the front, positive towards the left (counter-clockwise seen from above).
//   Elevation is positive upwards.

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace sf2
{

struct Vec3
{
    double x = 0.0, y = 0.0, z = 0.0;

    constexpr Vec3 operator+ (const Vec3& o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vec3 operator- (const Vec3& o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vec3 operator* (double s) const noexcept      { return { x * s, y * s, z * s }; }
    constexpr double dot (const Vec3& o) const noexcept     { return x * o.x + y * o.y + z * o.z; }
    double length() const noexcept                          { return std::sqrt (dot (*this)); }
    Vec3 normalised() const noexcept                        { const auto l = length(); return l > 0.0 ? *this * (1.0 / l) : Vec3{}; }
};

constexpr double degToRad (double deg) noexcept { return deg * std::numbers::pi / 180.0; }
constexpr double radToDeg (double rad) noexcept { return rad * 180.0 / std::numbers::pi; }

/** Unit vector for an azimuth/elevation pair in degrees. */
inline Vec3 directionFromAzEl (double azimuthDeg, double elevationDeg) noexcept
{
    const auto az = degToRad (azimuthDeg);
    const auto el = degToRad (elevationDeg);
    return { std::cos (az) * std::cos (el), std::sin (az) * std::cos (el), std::sin (el) };
}

/** Row-major 3x3 matrix. */
struct Mat3
{
    std::array<std::array<double, 3>, 3> m { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };

    static constexpr Mat3 identity() noexcept { return {}; }

    Vec3 operator* (const Vec3& v) const noexcept
    {
        return { m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                 m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                 m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z };
    }

    Mat3 operator* (const Mat3& o) const noexcept
    {
        Mat3 r;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                r.m[i][j] = m[i][0] * o.m[0][j] + m[i][1] * o.m[1][j] + m[i][2] * o.m[2][j];
        return r;
    }

    Mat3 transposed() const noexcept
    {
        Mat3 r;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }
};

/** Right-handed rotation about +x (front axis). Positive angle lifts the left side up. */
inline Mat3 rotationX (double deg) noexcept
{
    const auto c = std::cos (degToRad (deg)), s = std::sin (degToRad (deg));
    Mat3 r;
    r.m = { { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } } };
    return r;
}

/** Right-handed rotation about +y (left axis). Positive angle tips the front down. */
inline Mat3 rotationY (double deg) noexcept
{
    const auto c = std::cos (degToRad (deg)), s = std::sin (degToRad (deg));
    Mat3 r;
    r.m = { { { c, 0, s }, { 0, 1, 0 }, { -s, 0, c } } };
    return r;
}

/** Right-handed rotation about +z (up axis). Positive angle turns the front towards the left. */
inline Mat3 rotationZ (double deg) noexcept
{
    const auto c = std::cos (degToRad (deg)), s = std::sin (degToRad (deg));
    Mat3 r;
    r.m = { { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } } };
    return r;
}

/** Scene rotation from user-facing yaw/pitch/roll in degrees.
    yaw   > 0 : a source in front moves to the left.
    pitch > 0 : a source in front moves up.
    roll  > 0 : a source on the left moves up.
    Applied in the order roll, then pitch, then yaw. */
inline Mat3 sceneRotation (double yawDeg, double pitchDeg, double rollDeg) noexcept
{
    return rotationZ (yawDeg) * rotationY (-pitchDeg) * rotationX (rollDeg);
}

} // namespace sf2
