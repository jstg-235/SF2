#pragma once

// Conversions into first-order AmbiX (ACN channel order W, Y, Z, X; SN3D normalisation).
// For a unit plane wave from direction d, ideal AmbiX first order is [1, d.y, d.z, d.x].

#include "Geometry.h"
#include "MicProfiles.h"

#include <array>

namespace sf2
{

/** Row-major 4x4 matrix used for channel mixing. */
using Mat4 = std::array<std::array<double, 4>, 4>;

constexpr Mat4 identity4() noexcept
{
    return { { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } } };
}

Mat4 multiply (const Mat4& a, const Mat4& b) noexcept;

/** Inverts a 4x4 matrix with partial pivoting. Returns false if it is singular. */
bool invert (const Mat4& in, Mat4& out) noexcept;

/** Matrix taking A-format capsule signals (in the profile's channel order) to AmbiX,
    expressed in the microphone's own frame. Derived by inverting the capsule model,
    so it is exact for the idealised coincident array of the profile. */
Mat4 aFormatToAmbiX (const MicProfile& profile) noexcept;

/** FuMa first order (W, X, Y, Z; W at -3 dB) to AmbiX. */
Mat4 fuMaToAmbiX() noexcept;

/** AmbiX to FuMa first order (W, X, Y, Z; W at -3 dB). */
Mat4 ambiXToFuMa() noexcept;

/** Rotation of a first-order AmbiX signal by a 3x3 rotation of the scene. */
Mat4 ambiXRotation (const Mat3& r) noexcept;

/** Ideal AmbiX encoding of a unit plane wave from direction d: [1, d.y, d.z, d.x]. */
constexpr std::array<double, 4> encodeAmbiX (const Vec3& d) noexcept { return { 1.0, d.y, d.z, d.x }; }

} // namespace sf2
