#pragma once

// A fixed set of virtual loudspeaker directions with an exact first-order decoder, shared by the
// spaced-pair and binaural renderers. Decoding to these directions and summing first-order
// patterns back gives the original signal exactly, so with no delays or HRTFs the renderers
// reduce to the coincident virtual mic.

#include "AFormat.h"
#include "Geometry.h"

#include <array>
#include <vector>

namespace sf2
{

struct VirtualSpeakers
{
    std::vector<Vec3> directions;

    /** decode[k] . [W, Y, Z, X] is the feed of virtual speaker k (least-squares / pseudo-inverse). */
    std::vector<std::array<double, 4>> decode;
};

/** 26 directions: 12 on the horizon, 6 at +45 and 6 at -45 degrees elevation, top and bottom. */
const VirtualSpeakers& virtualSpeakers();

} // namespace sf2
