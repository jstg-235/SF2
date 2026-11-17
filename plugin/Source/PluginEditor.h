#pragma once

// SF2's window (docs/DESIGN.md, "Interface design"): input on the left, the live sound-field map
// in the centre with the scene rotation under it, output on the right, all in signal-flow order.
// The header holds the Studio / Live switch and the latency the host compensates.
//
// The map shows the scene as the virtual mics hear it, after orientation and rotation: front in
// the middle, left to the left, up at the top. Drag it to rotate the scene (yaw and pitch), or
// double-click a hot spot to bring it to the front. Markers show where the outputs point.
//
// All drawing lives in Look (Look.h); the components here only lay out and connect parameters.

#include "Look.h"
#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <sf2/SoundFieldMap.h>

#include <memory>
#include <vector>

/** A slider that drags four times finer with Shift held. */
class Dial final : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent&) override;
};

/** Rotary or linear control bound to one parameter, with its caption and value readout. */
class Knob final : public juce::Component
{
public:
    enum class Kind { Large, Small, Linear };

    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption,
          Kind, bool bipolar = false, bool showsPattern = false);

    void paint (juce::Graphics&) override;
    void resized() override;
    void enablementChanged() override { repaint(); }

    Dial dial;

private:
    juce::String caption;
    Kind kind;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Choice parameter as a captioned drop-down. */
class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& caption);

    void paint (juce::Graphics&) override;
    void resized() override;
    void enablementChanged() override { repaint(); }

private:
    juce::String caption;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

/** On/off parameter as a pill switch with its name. */
class Switch final : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }

    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** The Studio / Live segmented switch in the header. */
class ModeSwitch final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit ModeSwitch (juce::RangedAudioParameter& liveParam);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    bool isLive() const noexcept { return live; }
    void setLive (bool shouldBeLive);

private:
    juce::ParameterAttachment attachment;
    bool live = false;
};

class LevelMeter final : public juce::Component
{
public:
    /** Takes a linear peak; shows it on a -60 to 0 dBFS scale with a slow fall and a peak hold. */
    void setPeak (float linear);
    void paint (juce::Graphics&) override;

    /** Height fraction (0 bottom ... 1 top) of a level on the meter scale. */
    static float fractionFor (float db) noexcept { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); }

private:
    float shownDb = -100.0f, holdDb = -100.0f;
    int holdFrames = 0;
};

class SoundFieldView final : public juce::Component
{
public:
    explicit SoundFieldView (juce::AudioProcessorValueTreeState& state);

    void setMap (const std::vector<float>& values, int width, int height);
    void setMarkers (std::vector<SF2AudioProcessor::Marker> markers);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> mapArea() const;
    juce::Point<float> toScreen (double azimuthDeg, double elevationDeg) const;
    void setParam (juce::RangedAudioParameter* p, float value);

    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* yaw;
    juce::RangedAudioParameter* pitch;
    juce::Image image;
    std::vector<SF2AudioProcessor::Marker> markers;
    float dragYaw = 0.0f, dragPitch = 0.0f;
};

class SF2AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit SF2AudioProcessorEditor (SF2AudioProcessor&);
    ~SF2AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1120, baseHeight = 700;

private:
    struct Capsule
    {
        LevelMeter meter;
        std::unique_ptr<Knob> trim;
        juce::TextButton invert;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> invertAttachment;
    };

    struct Panels { juce::Rectangle<int> header, input, centre, output; };
    Panels panelsFor (juce::Rectangle<int> bounds) const;

    void timerCallback() override;
    void updateEnablement();
    void chooseSofa();

    SF2AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    sf2ui::Look look;
    juce::TooltipWindow tooltips { this, 600 };

    ModeSwitch mode;

    Choice inputFormat, micModel, orientation;
    Switch spacingCorrection;
    Knob windHighPass;
    std::array<Capsule, 4> capsules;

    SoundFieldView field;
    Knob yaw, pitch, roll;

    Choice stereoType;
    Knob pattern, stereoAngle, abSpacing, gain, lfeCutoff, shotgunFocus;
    Switch shotgun;
    Choice bFormatOut;
    juce::TextButton loadSofa { "Load SOFA..." }, headModel { "Head model" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::array<LevelMeter, sf2::Engine::maxOutputs> outputMeters;

    juce::Rectangle<int> hrtfArea, capsulesArea, metersArea;   // set by resized(), drawn by paint()
    juce::String latencyText, latencySamplesText, outputText, hrtfText;
    std::vector<juce::String> meterNames;

    sf2::Covariance smoothed {};
    std::vector<float> map;
    int numMeters = 0;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SF2AudioProcessorEditor)
};
