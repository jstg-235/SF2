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
        { "RODE NT-SF1",          standardTetrahedron, 0.5, 0.5 },
        { "SoundField SPS200",    standardTetrahedron, 0.5, 0.5 },
        { "Sennheiser AMBEO VR",  standardTetrahedron, 0.5, 0.5 },
        { "Core Sound TetraMic",  standardTetrahedron, 0.5, 0.5 },
        { "Zoom H3-VR (A-format)", standardTetrahedron, 0.5, 0.5 },
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
