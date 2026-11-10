#pragma once

// Shotgun mode: a frequency-domain beam that narrows a first-order virtual mic beyond hypercardioid.
//
// A first-order signal cannot form a sharper *linear* beam than hypercardioid, so, like
// SoundField by RODE's "shotgun-type" patterns, this is non-linear. Per STFT bin it estimates
// the direction of arrival (active intensity) and diffuseness, then applies a gain mask:
//   - sound arriving from the look direction passes,
//   - sound from other directions is attenuated by a raised-cosine beam,
//   - diffuse sound is attenuated by a fixed amount,
//   - a floor limits the depth of attenuation to keep artifacts down.
// Masks are smoothed across frequency and time; the output matrix, not the direction, is smoothed
// (per the Harpex papers, docs/DESIGN.md section 5).
//
// At focus 0 the mask is exactly 1 and the output is the linear virtual mic delayed by
// getLatencySamples(). sqrt-Hann windows at 50% overlap give perfect reconstruction.

#include "Fft.h"
#include "Geometry.h"

#include <array>
#include <complex>
#include <vector>

namespace sf2
{

class Shotgun
{
public:
    /** Allocates. FFT size scales with sample rate (1024 at 48 kHz). */
    void prepare (double sampleRate);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return fftSize; }

    /** ambiX: 4 world-frame AmbiX channels (W, Y, Z, X). micRow: the virtual mic in AmbiX.
        look: unit look direction. focus: 0 (linear mic) to 1 (narrowest beam). */
    void process (const float* const* ambiX, int numSamples, const std::array<double, 4>& micRow,
                  const Vec3& look, double focus, float* out) noexcept;

private:
    void processFrame (const std::array<double, 4>& micRow, const Vec3& look, double focus) noexcept;

    double sampleRate = 48000.0;
    int fftSize = 0, hop = 0, pos = 0;
    Fft fft;

    std::vector<double> window;
    std::array<std::vector<float>, 4> inputs;               // last fftSize input samples per channel
    std::array<std::vector<std::complex<double>>, 4> spectra;
    std::vector<std::complex<double>> work;
    std::vector<double> overlap;                             // overlap-add accumulator, fftSize
    std::vector<float> ready;                                // hop samples ready to output

    std::vector<Vec3> intensity;                             // smoothed active intensity per bin
    std::vector<double> energy, gains, rawGains;

    double analysisCoeff = 0.0, gainCoeff = 0.0;
};

} // namespace sf2
