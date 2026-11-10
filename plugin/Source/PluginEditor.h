#pragma once

// SF2's window (docs/DESIGN.md, "Interface design"): inputs on the left, a live sound-field map
// in the centre, outputs on the right.
//
// The map shows the scene as the virtual mics hear it, after orientation and rotation: front in
// the middle, left to the left, up at the top. Drag it to rotate the scene (yaw and pitch), or
// double-click a hot spot to bring it to the front. Markers show where the outputs point.

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <sf2/SoundFieldMap.h>

#include <memory>
#include <vector>

class LevelMeter final : public juce::Component
{
public:
    /** Takes a linear peak; shows it on a -60 to 0 dBFS scale with a slow fall. */
    void setPeak (float linear);
    void paint (juce::Graphics&) override;

private:
    float shownDb = -100.0f;
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

private:
    struct Row
    {
        juce::String paramId;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Component> control;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
    };

    Row& addRow (std::vector<Row>& column, const juce::String& paramId, const juce::String& labelText = {});
    void layoutColumn (std::vector<Row>& column, juce::Rectangle<int>& area);
    Row* find (const juce::String& paramId);
    void timerCallback() override;
    void updateEnablement();
    void chooseSofa();

    SF2AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::vector<Row> inputRows, outputRows, rotationRows;
    std::array<Row, 4> capsuleTrims, capsuleInverts;
    std::array<LevelMeter, 4> inputMeters;
    std::array<LevelMeter, sf2::Engine::maxOutputs> outputMeters;

    SoundFieldView field;
    juce::Label title, latency, outputLayout, hrtfLabel, capsulesHeading, outputsHeading;
    juce::TextButton loadSofa { "Load SOFA..." }, headModel { "Head model" };
    std::unique_ptr<juce::FileChooser> chooser;

    sf2::Covariance smoothed {};
    std::vector<float> map;
    int numMeters = 0;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SF2AudioProcessorEditor)
};
