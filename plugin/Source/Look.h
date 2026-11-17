#pragma once

// SF2's visual system (docs/DESIGN.md, "Interface design"). Every control is drawn here, so the
// look changes in one place.
//
//   Palette   cool graphite shell; amber accent only for values, selection and Live; meter
//             colours green / yellow / red mean level and nothing else; viridis for the map.
//   Type      the system sans in two weights; monospaced digits only for measured readouts.
//   Density   dense, for long sessions; one spacing unit of 4 px (gaps of 8, 12, 16).
//   Signature the polar-pattern dial: its face draws the virtual microphone it is setting.

#include <juce_gui_basics/juce_gui_basics.h>

namespace sf2ui
{

namespace colour
{
    inline const juce::Colour shell      { 0xff0f1216 };
    inline const juce::Colour panel      { 0xff161a20 };
    inline const juce::Colour raised     { 0xff1e232b };
    inline const juce::Colour well       { 0xff0a0c0f };
    inline const juce::Colour line       { 0xff2b313b };
    inline const juce::Colour text       { 0xffe7eaef };
    inline const juce::Colour dim        { 0xff98a1ae };
    inline const juce::Colour faint      { 0xff5f6774 };
    inline const juce::Colour accent     { 0xfff2a541 };
    inline const juce::Colour accentDeep { 0xff6b4a1f };
    inline const juce::Colour levelOk    { 0xff4fb286 };
    inline const juce::Colour levelHot   { 0xffe8c547 };
    inline const juce::Colour levelClip  { 0xffef5a4c };
} // namespace colour

namespace space
{
    inline constexpr int unit = 4, small = 8, medium = 12, large = 16;
    inline constexpr float radius = 6.0f;
} // namespace space

juce::Font uiFont (float size, bool bold = false);
juce::Font numberFont (float size);

/** Section heading: small capitals with a little tracking. */
void drawHeading (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                  juce::Justification = juce::Justification::centredLeft);

/** Slider properties the look reads: "bipolar" draws the arc from the centre, "pattern" draws the
    polar pattern on the dial face. */
inline const juce::Identifier bipolarProperty { "bipolar" };
inline const juce::Identifier patternProperty { "pattern" };

class Look final : public juce::LookAndFeel_V4
{
public:
    Look();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    juce::Font getLabelFont (juce::Label&) override;

    /** The polar pattern of a first-order mic (0 omni ... 1 figure-8), centred in `area`. */
    static void drawPolarPattern (juce::Graphics&, juce::Rectangle<float> area, float pattern, juce::Colour);
};

} // namespace sf2ui
