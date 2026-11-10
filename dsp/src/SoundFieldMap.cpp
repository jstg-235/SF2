#include "sf2/SoundFieldMap.h"

#include "sf2/AFormat.h"

#include <algorithm>

namespace sf2
{

double steeredPower (const Covariance& c, const Vec3& d) noexcept
{
    constexpr double p = 0.75;   // hypercardioid
    const auto e = encodeAmbiX (d);
    const std::array<double, 4> r { (1.0 - p) * e[0], p * e[1], p * e[2], p * e[3] };

    double s = 0.0;
    std::size_t idx = 0;
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i; j < 4; ++j, ++idx)
            s += (i == j ? 1.0 : 2.0) * r[i] * r[j] * c[idx];
    return std::max (0.0, s);
}

void renderEnergyMap (const Covariance& c, int width, int height, std::vector<float>& out)
{
    const auto n = static_cast<std::size_t> (std::max (0, width) * std::max (0, height));
    if (out.size() < n)
        out.resize (n);

    double peak = 0.0;
    for (int y = 0; y < height; ++y)
    {
        const double el = 90.0 - 180.0 * (y + 0.5) / height;
        for (int x = 0; x < width; ++x)
        {
            const double az = 180.0 - 360.0 * (x + 0.5) / width;
            const double v = steeredPower (c, directionFromAzEl (az, el));
            out[static_cast<std::size_t> (y * width + x)] = static_cast<float> (v);
            peak = std::max (peak, v);
        }
    }

    const float scale = peak > 0.0 ? static_cast<float> (1.0 / peak) : 0.0f;
    for (std::size_t i = 0; i < n; ++i)
        out[i] *= scale;
}

} // namespace sf2
