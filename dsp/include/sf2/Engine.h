#pragma once

// The SF2 linear engine: A-format or B-format in, virtual microphones out.
//
// Signal path (docs/DESIGN.md, "DSP architecture"):
//   input -> capsule trim, polarity, wind high-pass -> A-to-B matrix -> capsule spacing correction
//         -> [mic-frame B-format] -> mount orientation -> scene rotation -> gain
//         -> virtual mics per output (speakers, coincident stereo), spaced pair, binaural,
//            shotgun (mono) or B-format out -> LFE low-pass
//
// The first stage runs on the four input channels. After it, every output that is a plain mix of
// B-format (speakers, coincident stereo, B-format) collapses to one (numOutputs x 4) gain matrix
// that is recomputed per block and interpolated sample by sample, so automation is zipper-free.
// Spaced pairs, binaural and shotgun work on the world-frame B-format instead.
//
// Real-time safety: prepare() and setOutputLayout() may allocate and must be called off the audio
// thread (e.g. from prepareToPlay). process() never allocates, locks or does I/O.

#include "AFormat.h"
#include "ArrayCorrection.h"
#include "Binaural.h"
#include "Fir.h"
#include "MicProfiles.h"
#include "Shotgun.h"
#include "SpacedPair.h"
#include "Speakers.h"

#include <array>
#include <atomic>
#include <vector>

namespace sf2
{

enum class InputFormat : int { AFormat = 0, FuMa, AmbiX };

enum class MountOrientation : int { Upright = 0, Inverted, EndFire };

enum class OutputMode : int
{
    Speakers,   ///< One virtual mic aimed at each speaker; a single centre speaker gives mono.
    Stereo,     ///< Two channels; EngineParams::stereoType picks coincident, spaced or binaural.
    AmbiX,      ///< First-order B-format, ACN/SN3D.
    FuMa        ///< First-order B-format, W X Y Z with W at -3 dB.
};

/** What a two-channel output renders. */
enum class StereoType : int
{
    Coincident = 0,   ///< virtual pair at +/- stereoAngle / 2 with the chosen pattern
    AB,               ///< spaced omnis, spacing from abSpacingCm
    ORTF,             ///< cardioids, 110 degrees, 17 cm
    NOS,              ///< cardioids, 90 degrees, 30 cm
    DIN,              ///< cardioids, 90 degrees, 20 cm
    Binaural          ///< headphones
};

struct EngineParams
{
    InputFormat      inputFormat   = InputFormat::AFormat;
    MicModel         micModel      = MicModel::RodeNTSF1;
    MountOrientation orientation   = MountOrientation::Upright;

    /** A-format only: per-capsule level trim, polarity and a 4th-order wind high-pass (0 = off). */
    std::array<double, 4> capsuleTrimDb {};
    std::array<bool, 4>   capsuleInvert {};
    double windHighPassHz = 0.0;

    /** A-format only: correct the capsule spacing (frequency-dependent A-to-B). Adds a short delay. */
    bool spacingCorrection = false;

    double yawDeg   = 0.0;
    double pitchDeg = 0.0;
    double rollDeg  = 0.0;

    /** Virtual mic pattern: 0 = omni, 0.5 = cardioid, 1 = figure-8. */
    double pattern        = 0.5;
    double stereoAngleDeg = 90.0;
    StereoType stereoType = StereoType::Coincident;
    double abSpacingCm    = 40.0;

    double gainDb         = 0.0;
    double lfeCutoffHz    = 120.0;

    /** Shotgun focus for mono output when shotgun mode is on: 0 = plain virtual mic, 1 = narrowest beam. */
    double shotgunFocus   = 0.0;
};

/** The microphone pair a spaced stereo type uses. */
PairConfig pairConfigFor (StereoType type, double abSpacingCm, double pattern, double stereoAngleDeg) noexcept;

class Engine
{
public:
    static constexpr int maxOutputs = 16;
    static constexpr int numInputs  = 4;

    using GainMatrix = std::array<std::array<double, numInputs>, maxOutputs>;

    /** Sets the sample rate and largest block size. Allocates; not for the audio thread. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Sets the output layout. For OutputMode::Speakers pass one Speaker per output channel.
        Not for the audio thread (except switching between AmbiX and FuMa, which is safe). */
    void setOutputLayout (OutputMode mode, const std::vector<Speaker>& speakers = {});

    /** Clears filter state and snaps the next block straight to its target gains. */
    void reset() noexcept;

    /** Processes one block. Input and output pointers may alias (in-place host buffers).
        Missing inputs are treated as silence; outputs beyond getNumOutputs() are cleared. */
    void process (const float* const* inputs, int numInputChannels,
                  float* const* outputs, int numOutputChannels,
                  int numSamples, const EngineParams& params) noexcept;

    /** The (outputs x 4) matrix from raw input channels to outputs for the frequency-independent
        part of the path (no wind filter or spacing correction, no LFE filter). */
    GainMatrix computeGainMatrix (const EngineParams& params) const noexcept;

    int getNumOutputs() const noexcept { return numOutputs; }

    /** Turns shotgun mode on or off. Only mono output uses it. Real-time safe (no allocation).
        Switching changes the latency, so the host's delay compensation must be updated. */
    void setShotgunEnabled (bool shouldBeEnabled) noexcept;
    bool isShotgunEnabled() const noexcept { return shotgunEnabled; }

    /** Latency the host must compensate for the given parameters and the current layout. */
    int getLatencySamples (const EngineParams& params) const noexcept;

    /** Latency for the parameters of the most recent process() call. */
    int getLatencySamples() const noexcept { return getLatencySamples (lastParams); }

    /** Latency shotgun mode adds when it is on, for the current sample rate. */
    int getShotgunLatencySamples() const noexcept { return shotgun.getLatencySamples(); }

    bool isMonoOutput() const noexcept { return mode == OutputMode::Speakers && numOutputs == 1 && ! speakers[0].isLfe; }
    OutputMode getOutputMode() const noexcept { return mode; }

    /** Raw input channels to mic-frame AmbiX: capsule trims and polarity, then A-to-B (or FuMa import). */
    static Mat4 inputToMicAmbiX (const EngineParams& params) noexcept;

    /** Mic-frame AmbiX to world-frame AmbiX: mount orientation, then scene rotation. */
    static Mat4 micToWorld (const EngineParams& params) noexcept;

    /** Matrix from raw input channels to world-frame AmbiX, including orientation and rotation. */
    static Mat4 inputToAmbiX (const EngineParams& params) noexcept { return multiply (micToWorld (params), inputToMicAmbiX (params)); }

    /** Binaural renderer, for loading HRTF sets from the message thread. */
    Binaural& getBinaural() noexcept { return binaural; }

    //==============================================================================
    /** Levels and sound-field statistics for displays. Written by the audio thread; read and
        cleared by the UI. Peaks are linear and hold until read. */
    struct Meters
    {
        std::array<std::atomic<float>, numInputs> inputPeak {};
        std::array<std::atomic<float>, maxOutputs> outputPeak {};

        /** Running sum of the world-frame AmbiX covariance (upper triangle, row major) and the
            number of samples in it. The UI reads it and sets resetCovariance to start again. */
        std::array<std::atomic<float>, 10> covariance {};
        std::atomic<float> covarianceSamples { 0.0f };
        std::atomic<bool> resetCovariance { false };
    };

    Meters& getMeters() noexcept { return meters; }

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

    /** Mic-frame AmbiX to outputs, for the matrix-only output modes. */
    GainMatrix computeOutputMatrix (const EngineParams& params) const noexcept;

    void updateLfeFilters (double cutoffHz) noexcept;
    void updateWindFilters (double cutoffHz) noexcept;
    void updateCorrectionTaps (const EngineParams& params) noexcept;
    bool usesShotgun() const noexcept { return shotgunEnabled && isMonoOutput(); }
    bool usesWorldPath (const EngineParams& params) const noexcept;

    void runInputStage (int len, const EngineParams& params, const Mat4& from, const Mat4& to, int start, double inv) noexcept;
    void runWorldStage (float* const* outputs, int numOutputChannels, int len, int start,
                        const Mat4& from, const Mat4& to, double inv, const EngineParams& params) noexcept;
    void accumulateCovariance (int len, const Mat4& toWorld) noexcept;
    void publishMeters (float* const* outputs, int numOutputChannels, int numSamples) noexcept;

    double sampleRate = 48000.0;
    int maxBlock = 0;

    OutputMode mode = OutputMode::Stereo;
    int numOutputs = 2;
    std::array<Speaker, maxOutputs> speakers {};

    std::vector<float> scratch;            // numInputs * maxBlock: input copy, then mic-frame AmbiX
    std::vector<float> worldScratch;       // numInputs * maxBlock: world-frame AmbiX
    GainMatrix current {};
    Mat4 currentInput {};                  // raw input -> mic-frame AmbiX (interpolated)
    Mat4 currentWorld {};                  // mic frame -> world, with gain (interpolated)
    bool snapToTarget = true;

    double lfeCutoff = -1.0;
    std::array<std::array<Biquad, 2>, maxOutputs> lfeFilters {};  // Linkwitz-Riley 4th order = 2 Butterworth stages

    double windCutoff = -1.0;
    std::array<std::array<Biquad, 2>, numInputs> windFilters {};  // 4th-order Butterworth high-pass

    std::array<ArrayCorrectionFilters, numMicModels> corrections;
    std::array<Fir, numInputs> correctionFirs;
    int correctionModel = -1;              // model whose taps are loaded
    int correctionDelay = 0;

    Shotgun shotgun;
    bool shotgunEnabled = false;
    SpacedPair spaced;
    Binaural binaural;

    EngineParams lastParams;
    Meters meters;
    std::array<double, 10> covAccum {};
    double covCount = 0.0;
};

} // namespace sf2
