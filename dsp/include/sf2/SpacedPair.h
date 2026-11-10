#pragma once

// Spaced and near-coincident stereo pairs (AB, ORTF, NOS, DIN) from first-order B-format.
//
// A coincident recording has no time differences between mics, and a linear first-order decode
// cannot recreate them accurately: its direction lobes are too broad, so the delays average out
// to well under half the real value. So, like Harpex's stereo mode, this is parametric. Per STFT
// bin it estimates the direction of arrival from the active intensity, then gives each virtual
// mic the time of arrival a plane wave from that direction would have at the mic's position, as a
// phase shift on that mic's coincident signal. A single source gets its true interaural delay at
// every frequency; diffuse sound, whose direction wanders, comes out decorrelated between the two
// mics, as it does with a real spaced pair.
//
// The per-bin phase factors (the output matrix) are smoothed over time, never the directions
// (docs/DESIGN.md, section 5). With zero spacing the output is exactly the coincident pair,
// delayed by the STFT latency.

#include "Fft.h"
#include "Geometry.h"

#include <array>
#include <complex>
#include <vector>

namespace sf2
{

struct PairConfig
{
    double halfAngleDeg = 45.0;   ///< each mic aimed this far off centre
    double spacingMetres = 0.0;   ///< distance between the two capsules
    double pattern = 0.5;         ///< 0 omni ... 1 figure-8
};

class SpacedPair
{
public:
    static constexpr double maxSpacingMetres = 1.0;

    /** Allocates. FFT size scales with sample rate (1024 at 48 kHz). */
    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    int getLatencySamples() const noexcept { return fftSize; }

    /** world: 4 world-frame AmbiX channels. Writes the left and right mics. */
    void process (const float* const* world, int numSamples, const PairConfig& config,
                  float* left, float* right) noexcept;

private:
    void processFrame (const PairConfig& config) noexcept;

    double sampleRate = 48000.0;
    int fftSize = 0, hop = 0, pos = 0;
    Fft fft;

    std::vector<double> window;
    std::array<std::vector<float>, 4> inputs;
    std::array<std::vector<std::complex<double>>, 4> spectra;
    std::array<std::vector<std::complex<double>>, 2> work;
    std::array<std::vector<double>, 2> overlap;
    std::array<std::vector<float>, 2> ready;

    std::vector<Vec3> intensity;                                  // smoothed active intensity per bin
    std::array<std::vector<std::complex<double>>, 2> phase;       // smoothed per-bin phase factor per mic
    double analysisCoeff = 0.0, phaseCoeff = 0.0;
};

} // namespace sf2
