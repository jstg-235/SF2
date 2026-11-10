#pragma once

// Capsule spacing correction for tetrahedral arrays (docs/DESIGN.md, "A-to-B conversion").
//
// The four capsules sit about 1 to 1.5 cm from the array centre, not at one point, so above a few
// kHz the frequency-independent A-to-B matrix gives the wrong level for W and for X, Y, Z, and
// the virtual mic patterns lose their shape. For a regular tetrahedron the error is the same for
// X, Y and Z, so the correction is two filters after the A-to-B matrix: one for W, one shared by
// X, Y and Z.
//
// Both are found per frequency by regularised least squares against the ideal W, X, Y, Z response
// over a dense set of directions, using the physical array model (capsule pattern at radius r).
// Above the spatial aliasing frequency, where direction is ambiguous, the solution fades to a
// diffuse-field (energy-preserving) gain so timbre stays natural. Gains are capped at +6 dB to
// limit noise.
//
// The filters are linear-phase FIRs with a delay of getDelaySamples(); that is the latency the
// correction adds.

#include "Geometry.h"
#include "MicProfiles.h"

#include <array>
#include <complex>
#include <vector>

namespace sf2
{

inline constexpr double speedOfSound = 343.0;

/** Uncorrected AmbiX (W, Y, Z, X) output of the frequency-independent A-to-B matrix for a unit
    plane wave from direction d (mic frame) at frequency f, using the spaced-array model. */
std::array<std::complex<double>, 4> spacedArrayAmbiX (const MicProfile& profile, const Vec3& d, double freqHz) noexcept;

struct CorrectionGains
{
    std::complex<double> w { 1.0 }, xyz { 1.0 };
};

/** The correction the least-squares design asks for at one frequency. Not real-time safe. */
CorrectionGains designCorrectionAt (const MicProfile& profile, double freqHz);

/** Frequency above which the array cannot resolve direction: about c / (pi r). */
double aliasingFrequency (const MicProfile& profile) noexcept;

struct ArrayCorrectionFilters
{
    std::vector<float> w, xyz;   // FIR taps
    int delay = 0;               // modelling delay in samples (the latency)
};

/** FIR length used at a sample rate: 64 taps at 44.1/48 kHz, scaled up with the rate. */
int arrayCorrectionLength (double sampleRate) noexcept;

/** Designs the two correction FIRs. Allocates; call off the audio thread. */
ArrayCorrectionFilters designArrayCorrection (const MicProfile& profile, double sampleRate);

} // namespace sf2
