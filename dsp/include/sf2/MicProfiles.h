#pragma once

// Tetrahedral (A-format) microphone profiles.
//
// Each capsule is modelled as a first-order pattern
//   r(d) = omniPart + dipolePart * (capsuleAxis . d)
// placed at radiusMetres * capsuleAxis from the array centre. The frequency-independent A-to-B
// matrix treats the array as coincident; the spacing correction (ArrayCorrection.h) uses the
// radius to fix the high-frequency errors the capsule spacing causes.
// See docs/DESIGN.md, section "A-to-B conversion".

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

    /** Distance from the array centre to each capsule's diaphragm, in metres.
        These are estimates from published array sizes, to be replaced by our own measurements. */
    double radiusMetres = 0.0147;
};

/** All five supported models share the common A-format channel order
    1 = front-left-up, 2 = front-right-down, 3 = back-left-down, 4 = back-right-up
    (as documented for these arrays in the Harpex-X manual). */
const MicProfile& getMicProfile (MicModel model) noexcept;

constexpr std::size_t numMicModels = static_cast<std::size_t> (MicModel::numModels);

} // namespace sf2
