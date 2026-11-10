#pragma once

// Minimal radix-2 complex FFT. Tables are built in prepare(); transforms do not allocate.

#include <complex>
#include <vector>

namespace sf2
{

class Fft
{
public:
    /** size must be a power of two. Allocates. */
    void prepare (int size);

    int getSize() const noexcept { return n; }

    /** In-place forward transform (no scaling). */
    void forward (std::complex<double>* data) const noexcept { transform (data, false); }

    /** In-place inverse transform, scaled by 1/N so that inverse(forward(x)) == x. */
    void inverse (std::complex<double>* data) const noexcept;

private:
    void transform (std::complex<double>* data, bool inverse) const noexcept;

    int n = 0;
    std::vector<int> bitReversed;
    std::vector<std::complex<double>> twiddles;
};

} // namespace sf2
