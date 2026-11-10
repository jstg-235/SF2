#include "sf2/Fft.h"

#include <cmath>
#include <numbers>
#include <utility>

namespace sf2
{

void Fft::prepare (int size)
{
    n = size;
    int bits = 0;
    while ((1 << bits) < n)
        ++bits;

    bitReversed.resize (static_cast<std::size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b))
                r |= 1 << (bits - 1 - b);
        bitReversed[static_cast<std::size_t> (i)] = r;
    }

    twiddles.resize (static_cast<std::size_t> (n / 2));
    for (int k = 0; k < n / 2; ++k)
        twiddles[static_cast<std::size_t> (k)] = std::polar (1.0, -2.0 * std::numbers::pi * k / n);
}

void Fft::transform (std::complex<double>* data, bool inv) const noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitReversed[static_cast<std::size_t> (i)];
        if (j > i)
            std::swap (data[i], data[j]);
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int step = n / len;
        for (int start = 0; start < n; start += len)
            for (int k = 0; k < half; ++k)
            {
                auto w = twiddles[static_cast<std::size_t> (k * step)];
                if (inv)
                    w = std::conj (w);
                const auto u = data[start + k];
                const auto v = data[start + k + half] * w;
                data[start + k] = u + v;
                data[start + k + half] = u - v;
            }
    }
}

void Fft::inverse (std::complex<double>* data) const noexcept
{
    transform (data, true);
    const double scale = 1.0 / n;
    for (int i = 0; i < n; ++i)
        data[i] *= scale;
}

} // namespace sf2
