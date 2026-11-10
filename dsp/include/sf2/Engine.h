#pragma once

// The v1.0 "linear engine": A-format or B-format in, virtual microphones out.
//
// Signal path (docs/DESIGN.md, "DSP architecture"):
//   input -> A-to-B (or FuMa/AmbiX import) -> mount orientation -> scene rotation
//         -> virtual mics per output (speakers, stereo pair) or B-format out -> gain -> LFE low-pass
//
// Everything up to the outputs is linear and frequency-independent in v1.0, so the whole chain
// collapses to one (numOutputs x 4) gain matrix that is recomputed per block and interpolated
// sample by sample, which keeps automation zipper-free.
//
// Real-time safety: prepare() and setOutputLayout() may allocate and must be called off the audio
// thread (e.g. from prepareToPlay). process() never allocates, locks or does I/O.

#include "AFormat.h"
#include "MicProfiles.h"
#include "Shotgun.h"
#include "Speakers.h"

#include <array>
#include <vector>

namespace sf2
{

enum class InputFormat : int { AFormat = 0, FuMa, AmbiX };

enum class MountOrientation : int { Upright = 0, Inverted, EndFire };

enum class OutputMode : int
{
    Speakers,   ///< One virtual mic aimed at each speaker; a single centre speaker gives mono.
    Stereo,     ///< Coincident virtual pair at +/- stereoAngle / 2.
    AmbiX,      ///< First-order B-format, ACN/SN3D.
    FuMa        ///< First-order B-format, W X Y Z with W at -3 dB.
};

struct EngineParams
{
    InputFormat      inputFormat   = InputFormat::AFormat;
    MicModel         micModel      = MicModel::RodeNTSF1;
    MountOrientation orientation   = MountOrientation::Upright;

    double yawDeg   = 0.0;
    double pitchDeg = 0.0;
    double rollDeg  = 0.0;

    /** Virtual mic pattern: 0 = omni, 0.5 = cardioid, 1 = figure-8. */
    double pattern        = 0.5;
    double stereoAngleDeg = 90.0;
    double gainDb         = 0.0;
    double lfeCutoffHz    = 120.0;

    /** Shotgun focus for mono output when shotgun mode is on: 0 = plain virtual mic, 1 = narrowest beam. */
    double shotgunFocus   = 0.0;
};

class Engine
{
public:
    static constexpr int maxOutputs = 16;
    static constexpr int numInputs  = 4;

    using GainMatrix = std::array<std::array<double, numInputs>, maxOutputs>;

    /** Sets the sample rate and largest block size. Allocates; not for the audio thread. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Sets the output layout. For OutputMode::Speakers pass one Speaker per output channel.
        Not for the audio thread. */
    void setOutputLayout (OutputMode mode, const std::vector<Speaker>& speakers = {});

    /** Clears filter state and snaps the next block straight to its target gains. */
    void reset() noexcept;

    /** Processes one block. Input and output pointers may alias (in-place host buffers).
        Missing inputs are treated as silence; outputs beyond getNumOutputs() are cleared. */
    void process (const float* const* inputs, int numInputChannels,
                  float* const* outputs, int numOutputChannels,
                  int numSamples, const EngineParams& params) noexcept;

    /** The (outputs x 4) matrix from raw input channels to outputs, before LFE filtering. */
    GainMatrix computeGainMatrix (const EngineParams& params) const noexcept;

    int getNumOutputs() const noexcept { return numOutputs; }

    /** Turns shotgun mode on or off. Only mono output uses it. Real-time safe (no allocation).
        Switching changes getLatencySamples(), so the host's delay compensation must be updated. */
    void setShotgunEnabled (bool shouldBeEnabled) noexcept;
    bool isShotgunEnabled() const noexcept { return shotgunEnabled; }

    /** Latency the host must compensate: the shotgun STFT length while shotgun mode runs on mono
        output, otherwise 0. Focus can be automated freely without changing it. */
    int getLatencySamples() const noexcept { return usesShotgun() ? shotgun.getLatencySamples() : 0; }

    /** Latency shotgun mode adds when it is on, for the current sample rate. */
    int getShotgunLatencySamples() const noexcept { return shotgun.getLatencySamples(); }

    bool isMonoOutput() const noexcept { return mode == OutputMode::Speakers && numOutputs == 1 && ! speakers[0].isLfe; }
    OutputMode getOutputMode() const noexcept { return mode; }

    /** Matrix from raw input channels to world-frame AmbiX, including orientation and rotation. */
    static Mat4 inputToAmbiX (const EngineParams& params) noexcept;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        float process (float x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return static_cast<float> (y);
        }
        void clear() noexcept { z1 = z2 = 0.0; }
    };

    void updateLfeFilters (double cutoffHz) noexcept;
    bool usesShotgun() const noexcept { return shotgunEnabled && isMonoOutput(); }
    void processShotgun (const float* const* inputs, int usedInputs, float* out, int numSamples,
                         const EngineParams& params) noexcept;

    double sampleRate = 48000.0;
    int maxBlock = 0;

    OutputMode mode = OutputMode::Stereo;
    int numOutputs = 2;
    std::array<Speaker, maxOutputs> speakers {};

    std::vector<float> scratch;            // numInputs * maxBlock, copy of the input (handles aliasing)
    GainMatrix current {};
    bool snapToTarget = true;

    double lfeCutoff = -1.0;
    std::array<std::array<Biquad, 2>, maxOutputs> lfeFilters {};  // Linkwitz-Riley 4th order = 2 Butterworth stages

    Shotgun shotgun;
    bool shotgunEnabled = false;
    std::vector<float> worldScratch;       // numInputs * maxBlock, world-frame AmbiX for the shotgun
    Mat4 currentWorld {};
};

} // namespace sf2
