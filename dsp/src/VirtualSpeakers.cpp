#include "sf2/VirtualSpeakers.h"

namespace sf2
{

const VirtualSpeakers& virtualSpeakers()
{
    static const VirtualSpeakers vs = []
    {
        VirtualSpeakers v;
        for (int i = 0; i < 12; ++i)
            v.directions.push_back (directionFromAzEl (30.0 * i, 0.0));
        for (int i = 0; i < 6; ++i)
        {
            v.directions.push_back (directionFromAzEl (60.0 * i + 30.0, 45.0));
            v.directions.push_back (directionFromAzEl (60.0 * i, -45.0));
        }
        v.directions.push_back ({ 0, 0, 1 });
        v.directions.push_back ({ 0, 0, -1 });

        // E E^T, with E the 4 x K matrix of encodings.
        Mat4 eet {};
        for (const auto& d : v.directions)
        {
            const auto e = encodeAmbiX (d);
            for (std::size_t i = 0; i < 4; ++i)
                for (std::size_t j = 0; j < 4; ++j)
                    eet[i][j] += e[i] * e[j];
        }
        Mat4 inv {};
        invert (eet, inv);

        for (const auto& d : v.directions)
        {
            const auto e = encodeAmbiX (d);
            std::array<double, 4> row {};
            for (std::size_t j = 0; j < 4; ++j)
                for (std::size_t i = 0; i < 4; ++i)
                    row[j] += e[i] * inv[i][j];
            v.decode.push_back (row);
        }
        return v;
    }();
    return vs;
}

} // namespace sf2
