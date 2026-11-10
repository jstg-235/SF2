#include "Parameters.h"

#include <sf2/MicProfiles.h>

namespace sf2::params
{
namespace
{
    juce::String patternName (float p)
    {
        // Named first-order patterns, by figure-8 share.
        struct Named { float value; const char* name; };
        static constexpr Named names[] = { { 0.0f, "Omni" }, { 0.3f, "Wide cardioid" }, { 0.5f, "Cardioid" },
                                           { 0.63f, "Supercardioid" }, { 0.75f, "Hypercardioid" }, { 1.0f, "Figure-8" } };
        for (const auto& n : names)
            if (std::abs (p - n.value) < 0.005f)
                return n.name;
        return juce::String (p, 2);
    }
} // namespace

juce::String capsuleTrim (int capsule)   { return "capsuleTrim" + juce::String (capsule); }
juce::String capsuleInvert (int capsule) { return "capsuleInvert" + juce::String (capsule); }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray mics;
    for (std::size_t i = 0; i < numMicModels; ++i)
        mics.add (String (std::string (getMicProfile (static_cast<MicModel> (i)).name)));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { inputFormat, 1 }, "Input format",
                    StringArray { "A-format", "B-format (FuMa)", "B-format (AmbiX)" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { micModel, 1 }, "Microphone", mics, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { orientation, 1 }, "Orientation",
                    StringArray { "Upright", "Inverted", "End-fire" }, 0));

    auto degrees = AudioParameterFloatAttributes().withLabel (juce::CharPointer_UTF8 ("\xc2\xb0"));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { yaw, 1 }, "Rotate (yaw)",
                    NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f, degrees));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pitch, 1 }, "Tilt (pitch)",
                    NormalisableRange<float> (-90.0f, 90.0f, 0.1f), 0.0f, degrees));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { roll, 1 }, "Tumble (roll)",
                    NormalisableRange<float> (-180.0f, 180.0f, 0.1f), 0.0f, degrees));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pattern, 1 }, "Polar pattern",
                    NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f,
                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return patternName (v); })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { stereoAngle, 1 }, "Stereo angle",
                    NormalisableRange<float> (0.0f, 180.0f, 0.1f), 90.0f, degrees));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { gain, 1 }, "Output gain",
                    NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lfeCutoff, 1 }, "LFE cutoff",
                    NormalisableRange<float> (40.0f, 200.0f, 1.0f), 120.0f,
                    AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bFormatOut, 1 }, "B-format output",
                    StringArray { "AmbiX", "FuMa" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { shotgun, 1 }, "Shotgun mode (mono)", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { shotgunFocus, 1 }, "Shotgun focus (mono)",
                    NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                    AudioParameterFloatAttributes().withLabel ("%")));

    // v1.1
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { spacingCorrection, 2 }, "Capsule spacing correction", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { windHighPass, 2 }, "Wind high-pass",
                    NormalisableRange<float> (0.0f, 200.0f, 1.0f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return v < 10.0f ? String ("Off") : String (roundToInt (v)) + " Hz"; })));
    for (int c = 1; c <= 4; ++c)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { capsuleTrim (c), 2 }, "Capsule " + String (c) + " trim",
                        NormalisableRange<float> (-6.0f, 6.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { capsuleInvert (c), 2 }, "Capsule " + String (c) + " polarity invert", false));
    }
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { stereoType, 2 }, "Stereo output",
                    StringArray { "Coincident pair", "Spaced AB", "ORTF", "NOS", "DIN", "Binaural (headphones)" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { abSpacing, 2 }, "AB spacing",
                    NormalisableRange<float> (5.0f, 100.0f, 0.5f), 40.0f, AudioParameterFloatAttributes().withLabel ("cm")));

    return layout;
}

} // namespace sf2::params
