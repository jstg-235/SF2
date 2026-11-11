#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sf2::params
{

// Parameter IDs are permanent: saved sessions and automation refer to them.
// Never rename or reuse one; add new parameters with a higher version hint.
inline constexpr const char* inputFormat  = "inputFormat";
inline constexpr const char* micModel     = "micModel";
inline constexpr const char* orientation  = "orientation";
inline constexpr const char* yaw          = "yaw";
inline constexpr const char* pitch        = "pitch";
inline constexpr const char* roll         = "roll";
inline constexpr const char* pattern      = "pattern";
inline constexpr const char* stereoAngle  = "stereoAngle";
inline constexpr const char* gain         = "gain";
inline constexpr const char* lfeCutoff    = "lfeCutoff";
inline constexpr const char* bFormatOut   = "bFormatOut";
inline constexpr const char* shotgunFocus = "shotgunFocus";
inline constexpr const char* shotgun      = "shotgun";

// Added in v1.1 (version hint 2).
inline constexpr const char* spacingCorrection = "spacingCorrection";
inline constexpr const char* windHighPass      = "windHighPass";
inline constexpr const char* stereoType        = "stereoType";
inline constexpr const char* abSpacing         = "abSpacing";

/** Per-capsule trim and polarity, capsules 1 to 4: "capsuleTrim1", "capsuleInvert1", ... */
juce::String capsuleTrim (int capsule);
juce::String capsuleInvert (int capsule);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace sf2::params
