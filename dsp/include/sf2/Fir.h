#pragma once

// Direct-form FIR filter for short, fixed-length filters (capsule spacing correction, binaural).
// prepare() allocates; process() does not. Input and output may alias.

#include <algorithm>
#include <cstddef>
#include <vector>

namespace sf2
{

class Fir
{
public:
    /** Allocates. Taps start as a unit impulse (pass-through, no delay). */
    void prepare (int length, int maximumBlockSize)
    {
        len = std::max (1, length);
        maxBlock = std::max (1, maximumBlockSize);
        reversed.assign (static_cast<std::size_t> (len), 0.0f);
        reversed.back() = 1.0f;
        ext.assign (static_cast<std::size_t> (len - 1 + maxBlock), 0.0f);
    }

    int getLength() const noexcept { return len; }

    /** Copies `length` taps; if length differs from the prepared length the extra taps are dropped
        or zero-padded. Real-time safe. */
    void setTaps (const float* taps, int length) noexcept
    {
        for (int k = 0; k < len; ++k)
            reversed[static_cast<std::size_t> (len - 1 - k)] = k < length ? taps[k] : 0.0f;
    }

    void reset() noexcept { std::fill (ext.begin(), ext.end(), 0.0f); }

    /** Filters n <= maximumBlockSize samples. With accumulate, adds to out instead of writing. */
    void process (const float* in, float* out, int n, bool accumulate = false) noexcept
    {
        const auto hist = static_cast<std::size_t> (len - 1);
        std::copy_n (in, n, ext.begin() + static_cast<std::ptrdiff_t> (hist));

        const float* h = reversed.data();
        for (int i = 0; i < n; ++i)
        {
            const float* x = ext.data() + i;
            float s = 0.0f;
            for (int k = 0; k < len; ++k)
                s += h[k] * x[k];
            out[i] = accumulate ? out[i] + s : s;
        }

        std::copy_n (ext.begin() + n, hist, ext.begin());
    }

private:
    int len = 1, maxBlock = 1;
    std::vector<float> reversed;   // taps, last tap first, so the inner loop runs forwards
    std::vector<float> ext;        // len - 1 samples of history followed by the current block
};

} // namespace sf2
