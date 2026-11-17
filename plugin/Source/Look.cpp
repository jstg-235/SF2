#include "Look.h"

namespace sf2ui
{

juce::Font uiFont (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font numberFont (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

void drawHeading (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Justification just)
{
    g.setColour (colour::dim);
    auto f = uiFont (11.0f, true);
    f.setExtraKerningFactor (0.12f);
    g.setFont (f);
    g.drawText (text.toUpperCase(), area, just, false);
}

Look::Look()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, colour::shell);
    setColour (Label::textColourId, colour::text);
    setColour (Slider::textBoxTextColourId, colour::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, colour::accentDeep);
    setColour (TextEditor::backgroundColourId, colour::well);
    setColour (TextEditor::textColourId, colour::text);
    setColour (TextEditor::highlightColourId, colour::accentDeep);
    setColour (TextEditor::outlineColourId, colour::line);
    setColour (TextEditor::focusedOutlineColourId, colour::accent);
    setColour (CaretComponent::caretColourId, colour::accent);
    setColour (ComboBox::textColourId, colour::text);
    setColour (ComboBox::arrowColourId, colour::dim);
    setColour (PopupMenu::backgroundColourId, colour::raised);
    setColour (PopupMenu::textColourId, colour::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colour::accentDeep);
    setColour (PopupMenu::highlightedTextColourId, colour::text);
    setColour (TextButton::buttonColourId, colour::raised);
    setColour (TextButton::buttonOnColourId, colour::accent);
    setColour (TextButton::textColourOffId, colour::text);
    setColour (TextButton::textColourOnId, colour::shell);
    setColour (ToggleButton::textColourId, colour::text);
    setColour (TooltipWindow::backgroundColourId, colour::raised);
    setColour (TooltipWindow::textColourId, colour::text);
    setColour (TooltipWindow::outlineColourId, colour::line);
    setColour (AlertWindow::backgroundColourId, colour::panel);
    setColour (AlertWindow::textColourId, colour::text);
}

//==============================================================================
void Look::drawPolarPattern (juce::Graphics& g, juce::Rectangle<float> area, float pattern, juce::Colour c)
{
    const auto centre = area.getCentre();
    const float radius = 0.5f * juce::jmin (area.getWidth(), area.getHeight());
    const float p = juce::jlimit (0.0f, 1.0f, pattern);

    // Front up. A point with negative gain shows on the opposite side (the rear lobe, inverted).
    juce::Path front, rear;
    constexpr int steps = 120;
    for (int i = 0; i <= steps; ++i)
    {
        const float theta = juce::MathConstants<float>::twoPi * static_cast<float> (i) / steps;
        const float r = (1.0f - p) + p * std::cos (theta);
        const float rf = juce::jmax (0.0f, r) * radius, rr = juce::jmax (0.0f, -r) * radius;
        const juce::Point<float> pf { centre.x + rf * std::sin (theta), centre.y - rf * std::cos (theta) };
        const juce::Point<float> pr { centre.x - rr * std::sin (theta), centre.y + rr * std::cos (theta) };
        if (i == 0) { front.startNewSubPath (pf); rear.startNewSubPath (pr); }
        else        { front.lineTo (pf); rear.lineTo (pr); }
    }
    front.closeSubPath();
    rear.closeSubPath();

    g.setColour (c.withAlpha (0.10f));
    g.fillPath (rear);
    g.setColour (c.withAlpha (0.45f));
    g.strokePath (rear, juce::PathStrokeType (1.0f));
    g.setColour (c.withAlpha (0.22f));
    g.fillPath (front);
    g.setColour (c);
    g.strokePath (front, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                             float startAngle, float endAngle, juce::Slider& s)
{
    const bool enabled = s.isEnabled();
    const float size = static_cast<float> (juce::jmin (w, h));
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (w), static_cast<float> (h)).withSizeKeepingCentre (size, size);
    const auto centre = bounds.getCentre();
    const float thickness = size > 80.0f ? 4.0f : 3.0f;
    const float arcRadius = 0.5f * size - thickness;

    // Face.
    const auto face = bounds.reduced (thickness * 2.5f);
    g.setColour (colour::raised);
    g.fillEllipse (face);
    g.setColour (colour::line);
    g.drawEllipse (face, 1.0f);

    // Track and value arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colour::well);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = static_cast<bool> (s.getProperties().getWithDefault (bipolarProperty, false));
    const float from = bipolar ? 0.5f * (startAngle + endAngle) : startAngle;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    const auto valueColour = enabled ? colour::accent : colour::faint;
    g.setColour (valueColour);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto tip = centre.getPointOnCircumference (arcRadius, angle);
    if (static_cast<bool> (s.getProperties().getWithDefault (patternProperty, false)))
    {
        drawPolarPattern (g, face.reduced (face.getWidth() * 0.14f), static_cast<float> (s.getValue()),
                          enabled ? colour::accent : colour::faint);
        g.setColour (valueColour);
        g.fillEllipse (juce::Rectangle<float> (thickness * 2.2f, thickness * 2.2f).withCentre (tip));
        return;
    }

    // Pointer.
    const auto inner = centre.getPointOnCircumference (face.getWidth() * 0.18f, angle);
    const auto outer = centre.getPointOnCircumference (face.getWidth() * 0.44f, angle);
    g.setColour (enabled ? colour::text : colour::faint);
    g.drawLine ({ inner, outer }, 2.0f);
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                             juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0.0f, 0.0f, style, s);
        return;
    }

    const bool enabled = s.isEnabled();
    const float cy = static_cast<float> (y) + 0.5f * static_cast<float> (h);
    const auto track = juce::Rectangle<float> (static_cast<float> (x), cy - 2.0f, static_cast<float> (w), 4.0f);
    g.setColour (colour::well);
    g.fillRoundedRectangle (track, 2.0f);

    const bool bipolar = static_cast<bool> (s.getProperties().getWithDefault (bipolarProperty, false));
    const float from = bipolar ? track.getCentreX() : track.getX();
    g.setColour (enabled ? colour::accent : colour::faint);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (from, pos), cy - 2.0f, std::abs (pos - from), 4.0f), 2.0f);
    if (bipolar)
    {
        g.setColour (colour::faint);
        g.fillRect (juce::Rectangle<float> (1.0f, 8.0f).withCentre ({ track.getCentreX(), cy }));
    }

    g.setColour (enabled ? colour::text : colour::faint);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ pos, cy }));
}

juce::Label* Look::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (numberFont (12.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, colour::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineWhenEditingColourId, colour::accent);
    return l;
}

juce::Slider::SliderLayout Look::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout layout;
    auto r = s.getLocalBounds();
    if (s.getTextBoxPosition() == juce::Slider::TextBoxBelow)
    {
        layout.textBoxBounds = r.removeFromBottom (16);
        r.removeFromBottom (2);
    }
    else if (s.getTextBoxPosition() == juce::Slider::TextBoxRight)
    {
        layout.textBoxBounds = r.removeFromRight (s.getTextBoxWidth());
        r.removeFromRight (space::unit);
    }
    layout.sliderBounds = s.isRotary() ? r : r.reduced (6, 0);
    return layout;
}

//==============================================================================
void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    const bool enabled = b.isEnabled();
    auto r = b.getLocalBounds().toFloat();

    // A pill switch: unmistakably on (filled, knob right) or off (hollow, knob left).
    const auto pill = r.removeFromLeft (30.0f).withSizeKeepingCentre (30.0f, 16.0f);
    g.setColour (on ? (enabled ? colour::accent : colour::faint) : colour::well);
    g.fillRoundedRectangle (pill, 8.0f);
    g.setColour (highlighted && enabled ? colour::dim : colour::line);
    g.drawRoundedRectangle (pill, 8.0f, 1.0f);
    const auto knob = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? pill.getRight() - 8.0f : pill.getX() + 8.0f, pill.getCentreY() });
    g.setColour (on ? colour::shell : (enabled ? colour::dim : colour::faint));
    g.fillEllipse (knob);

    r.removeFromLeft (static_cast<float> (space::small));
    g.setColour (enabled ? colour::text : colour::faint);
    g.setFont (uiFont (13.0f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = on ? colour::accent : colour::raised;
    if (! b.isEnabled())
        fill = on ? colour::faint : colour::panel;
    else if (down)
        fill = fill.darker (0.2f);
    else if (highlighted)
        fill = fill.brighter (0.08f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    if (! on)
    {
        g.setColour (b.hasKeyboardFocus (false) ? colour::accent : colour::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

juce::Font Look::getTextButtonFont (juce::TextButton&, int)
{
    return uiFont (13.0f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h)).reduced (0.5f);
    g.setColour (box.isEnabled() ? colour::raised : colour::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colour::accent : colour::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path chevron;
    const float cx = r.getRight() - 14.0f, cy = r.getCentreY();
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.isEnabled() ? colour::dim : colour::faint);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font Look::getComboBoxFont (juce::ComboBox&)
{
    return uiFont (13.0f);
}

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (1, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, box.isEnabled() ? colour::text : colour::faint);
}

juce::Font Look::getPopupMenuFont()
{
    return uiFont (13.0f);
}

juce::Font Look::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

} // namespace sf2ui
