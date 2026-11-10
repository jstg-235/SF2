#include "sf2/MicProfiles.h"

#include <cmath>

namespace sf2
{
namespace
{
    const double k = 1.0 / std::sqrt (3.0);

    // Front-left-up, front-right-down, back-left-down, back-right-up.
    const std::array<Vec3, 4> standardTetrahedron { { { k, k, k }, { k, -k, -k }, { -k, k, -k }, { -k, -k, k } } };

    const std::array<MicProfile, numMicModels> profiles { {
        // Radii are estimates from published capsule sizes and array photos, not measurements.
        { "RODE NT-SF1",           standardTetrahedron, 0.5, 0.5, 0.0147 },
        { "SoundField SPS200",     standardTetrahedron, 0.5, 0.5, 0.0147 },
        { "Sennheiser AMBEO VR",   standardTetrahedron, 0.5, 0.5, 0.0140 },
        { "Core Sound TetraMic",   standardTetrahedron, 0.5, 0.5, 0.0135 },
        { "Zoom H3-VR (A-format)", standardTetrahedron, 0.5, 0.5, 0.0100 },
    } };
} // namespace

const MicProfile& getMicProfile (MicModel model) noexcept
{
    auto index = static_cast<std::size_t> (model);
    if (index >= profiles.size())
        index = 0;
    return profiles[index];
}

} // namespace sf2
