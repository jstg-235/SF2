#pragma once

// Energy map of the sound field for the editor's heat map.
//
// From the world-frame AmbiX covariance C (published by Engine::Meters), the power a steered
// first-order beam would pick up from direction d is r(d)^T C r(d). A hypercardioid beam gives the
// sharpest first-order map; the editor sharpens it further for display.

#include "Geometry.h"

#include <array>
#include <vector>

namespace sf2
{

/** Symmetric 4x4 covariance stored as its upper triangle, row major (10 values). */
using Covariance = std::array<double, 10>;

/** Power a hypercardioid aimed at d would pick up. */
double steeredPower (const Covariance& c, const Vec3& d) noexcept;

/** Equirectangular map: width columns from azimuth +180 (left edge) to -180 (right edge), height
    rows from elevation +90 (top) to -90 (bottom). Values are normalised to 0..1. Allocates only if
    out is too small. */
void renderEnergyMap (const Covariance& c, int width, int height, std::vector<float>& out);

} // namespace sf2
