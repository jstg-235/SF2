#pragma once

// Binaural rendering of first-order B-format for headphones.
//
// The field is decoded to 26 virtual speakers (dual-band: exact decode below 700 Hz, max-rE above),
// and each speaker is rendered through an HRTF. Because everything is linear this collapses to
// eight FIR filters, one from each of W, Y, Z, X to each ear, designed once when the HRTF set or
// the sample rate changes. Rotation (and so head tracking) happens before, in B-format.
//
// HRTFs come either from a built-in spherical head model (Brown and Duda 1998: head shadow and
// interaural time difference, no pinna cues) or from a measured set, for example loaded from a
// SOFA file. Measured sets are diffuse-field equalised so they sound neutral.
//
// Latency is a fixed bulk delay whatever the HRTF set: about 1 ms, or 0.4 ms in low-latency (Live)
// mode, which trims more of the filters' pre-ringing. Loading a set is done off the
// audio thread; the new filters reach the audio thread through a lock-free hand-over.

#include "Fir.h"
#include "Geometry.h"

#include <array>
#include <atomic>
#include <optional>
#include <vector>

namespace sf2
{

/** Head-related impulse responses, one pair per measured direction. */
struct HrirSet
{
    double sampleRate = 48000.0;
    int length = 0;                    ///< samples per impulse response
    std::vector<Vec3> directions;      ///< unit vectors, listener frame (+x front, +y left, +z up)
    std::vector<float> left, right;    ///< directions.size() * length samples each

    bool isValid() const noexcept
    {
        return length > 0 && ! directions.empty() && sampleRate > 0.0
            && left.size() == directions.size() * static_cast<std::size_t> (length)
            && right.size() == left.size();
    }
};

class Binaural
{
public:
    /** Allocates and designs filters for the current HRTF set. Not for the audio thread. */
    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    /** The bulk delay of the filters the audio thread is using now. */
    int getLatencySamples() const noexcept { return activeDelay.load (std::memory_order_relaxed); }

    /** Message thread: low-latency mode uses a shorter bulk delay (0.4 ms instead of 1 ms). The
        audio thread switches with the next filter hand-over; getLatencySamples() follows it. */
    void setLowLatency (bool shouldBeLowLatency);
    bool isLowLatency() const noexcept { return lowLatency; }
    int getFilterLength() const noexcept { return length; }

    /** world: 4 world-frame AmbiX channels. Real-time safe. */
    void process (const float* const* world, int numSamples, float* left, float* right) noexcept;

    /** Message thread: switch to a measured set. Returns false (and changes nothing) if invalid. */
    bool setHrirs (const HrirSet& set);

    /** Message thread: switch back to the built-in head model. */
    void useHeadModel();

    bool isUsingHeadModel() const noexcept { return ! custom.has_value(); }

    /** Message thread: hands newly designed filters to the audio thread once it has taken the
        previous ones. Call regularly (e.g. from a timer); returns true while a hand-over waits. */
    bool pumpPendingFilters() noexcept;

    /** The eight filters (ear x AmbiX channel) as currently designed, for tests and displays. */
    const std::vector<float>& getDesignedTaps (int ear, int channel) const noexcept
    {
        return designed[static_cast<std::size_t> (ear)][static_cast<std::size_t> (channel)];
    }

    /** Builds the head-model HRIR pair for one direction (for tests). */
    static std::array<std::vector<float>, 2> headModelHrir (const Vec3& d, double sampleRate, int length, int delay);

private:
    using TapSet = std::array<std::array<std::vector<float>, 4>, 2>;

    void design();
    void applyTaps (const TapSet& taps) noexcept;

    double sampleRate = 48000.0;
    int maxBlock = 0, length = 0, bulkDelay = 0;   // bulkDelay: of the designed filters
    bool lowLatency = false;
    int stagingDelay = 0;
    std::atomic<int> activeDelay { 0 };
    std::optional<HrirSet> custom;

    TapSet designed;                  // message thread
    TapSet staging;                   // handed to the audio thread when ready == true
    std::atomic<bool> ready { false };
    bool designPending = false;

    std::array<std::array<Fir, 4>, 2> firs;
};

} // namespace sf2
