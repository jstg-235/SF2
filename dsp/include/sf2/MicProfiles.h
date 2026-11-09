#pragma once

// Tetrahedral (A-format) microphone profiles.
//
// v1.0 uses an idealised first-order capsule model: each capsule's response is
//   r(d) = omniPart + dipolePart * (capsuleAxis . d)
// with a coincident array. Frequency-dependent, measured calibration
// (capsule spacing, per-unit magnitude/phase) arrives in the v1.1 "Quality"
// phase; see docs/DESIGN.md, section "A-to-B conversion".

#include "Geometry.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace sf2
{

enum class MicModel : int
{
    RodeNTSF1 = 0,
    SoundFieldSPS200,
    SennheiserAmbeo,
    CoreSoundTetraMic,
    ZoomH3VR,
    numModels
};

struct MicProfile
{
    std::string_view name;

    /** Capsule axes in the microphone's own frame, in the order the A-format channels arrive. */
    std::array<Vec3, 4> capsuleAxes;

    /** First-order capsule pattern: omniPart + dipolePart * cos(theta). Cardioid = 0.5 / 0.5. */
    double omniPart   = 0.5;
    double dipolePart = 0.5;
};

/** All five supported models share the common A-format channel order
    1 = front-left-up, 2 = front-right-down, 3 = back-left-down, 4 = back-right-up
    (as documented for these arrays in the Harpex-X manual). */
const MicProfile& getMicProfile (MicModel model) noexcept;

constexpr std::size_t numMicModels = static_cast<std::size_t> (MicModel::numModels);

} // namespace sf2
