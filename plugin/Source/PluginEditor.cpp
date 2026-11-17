#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
namespace p = sf2::params;
namespace colour = sf2ui::colour;
namespace space = sf2ui::space;

/** Viridis colour map (colour-blind safe), t in 0..1. */
juce::Colour viridis (float t)
{
    static const juce::Colour stops[] = { juce::Colour (0xff440154), juce::Colour (0xff3b528b), juce::Colour (0xff21918c),
                                          juce::Colour (0xff5ec962), juce::Colour (0xfffde725) };
    t = juce::jlimit (0.0f, 1.0f, t) * 4.0f;
    const int i = juce::jmin (3, static_cast<int> (t));
    return stops[i].interpolatedWith (stops[i + 1], t - static_cast<float> (i));
}

float wrapDegrees (float d)
{
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

juce::Colour levelColour (float db)
{
    return db > -0.1f ? colour::levelClip : db > -12.0f ? colour::levelHot : colour::levelOk;
}

constexpr int captionHeight = 16;
constexpr int choiceHeight = captionHeight + 2 + 28;
} // namespace

//==============================================================================
void Dial::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1000 : 250);
    Slider::mouseDown (e);
}

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text,
            Kind k, bool bipolar, bool showsPattern)
    : caption (text), kind (k)
{
    auto* param = state.getParameter (id);
    jassert (param != nullptr);

    if (kind == Kind::Linear)
    {
        dial.setSliderStyle (juce::Slider::LinearHorizontal);
        dial.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 16);
    }
    else
    {
        dial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        dial.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 16);
        dial.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    }
    dial.getProperties().set (sf2ui::bipolarProperty, bipolar);
    dial.getProperties().set (sf2ui::patternProperty, showsPattern);
    dial.setMouseDragSensitivity (250);
    dial.setScrollWheelEnabled (true);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, dial);

    const auto unit = param->getLabel();
    if (unit.isNotEmpty())
        dial.setTextValueSuffix (unit == juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")) ? unit : " " + unit);
    dial.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    dial.setTitle (caption.isNotEmpty() ? caption : param->getName (40));
    dial.setTooltip (param->getName (60) + ". Double-click to reset, Shift-drag for fine control.");
    setTitle (dial.getTitle());

    addAndMakeVisible (dial);
}

void Knob::paint (juce::Graphics& g)
{
    if (caption.isEmpty())
        return;
    g.setColour (isEnabled() ? colour::dim : colour::faint);
    g.setFont (sf2ui::uiFont (kind == Kind::Large ? 12.0f : 11.0f));
    g.drawFittedText (caption, getLocalBounds().removeFromTop (captionHeight), juce::Justification::centred, 1, 0.85f);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty())
        r.removeFromTop (captionHeight);
    dial.setBounds (r);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text)
    : caption (text)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id));
    jassert (param != nullptr);
    box.addItemList (param->choices, 1);
    box.setTitle (caption);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, box);
    setTitle (caption);
    addAndMakeVisible (box);
}

void Choice::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? colour::dim : colour::faint);
    g.setFont (sf2ui::uiFont (11.0f));
    g.drawText (caption, getLocalBounds().removeFromTop (captionHeight), juce::Justification::centredLeft);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (captionHeight + 2);
    box.setBounds (r.removeFromTop (28));
}

//==============================================================================
Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& text)
{
    button.setButtonText (text);
    button.setTitle (text);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, button);
    addAndMakeVisible (button);
}

//==============================================================================
ModeSwitch::ModeSwitch (juce::RangedAudioParameter& param)
    : attachment (param, [this] (float v) { live = v > 0.5f; repaint(); })
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Live mode (low latency)");
    setTooltip ("Studio: best quality. Live: lowest latency for monitoring and live sound. "
                "Skips capsule spacing correction and uses shorter filters for spaced pairs, shotgun and binaural.");
    attachment.sendInitialUpdate();
}

void ModeSwitch::setLive (bool shouldBeLive)
{
    attachment.setValueAsCompleteGesture (shouldBeLive ? 1.0f : 0.0f);
}

void ModeSwitch::mouseDown (const juce::MouseEvent& e)
{
    setLive (e.position.x > 0.5f * static_cast<float> (getWidth()));
}

bool ModeSwitch::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey
        || key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
    {
        setLive (key == juce::KeyPress::leftKey ? false : key == juce::KeyPress::rightKey ? true : ! live);
        return true;
    }
    return false;
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colour::well);
    g.fillRoundedRectangle (r, space::radius);
    g.setColour (hasKeyboardFocus (false) ? colour::accent : colour::line);
    g.drawRoundedRectangle (r, space::radius, 1.0f);

    auto halves = r.reduced (3.0f);
    const auto studio = halves.removeFromLeft (halves.getWidth() * 0.5f);
    const auto liveHalf = halves;

    g.setColour (live ? colour::accent : colour::raised);
    g.fillRoundedRectangle (live ? liveHalf : studio, space::radius - 2.0f);

    g.setFont (sf2ui::uiFont (13.0f, true));
    g.setColour (live ? colour::dim : colour::text);
    g.drawText ("Studio", studio, juce::Justification::centred);

    const juce::String liveText ("Live");
    const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), liveText);
    const float dot = 7.0f, gap = 6.0f;
    const float x0 = liveHalf.getCentreX() - 0.5f * (textW + dot + gap);
    g.setColour (live ? colour::shell : colour::faint);
    g.fillEllipse (x0, liveHalf.getCentreY() - 0.5f * dot, dot, dot);
    g.setColour (live ? colour::shell : colour::dim);
    g.drawText (liveText, juce::Rectangle<float> (x0 + dot + gap, liveHalf.getY(), textW + 2.0f, liveHalf.getHeight()),
                juce::Justification::centredLeft);
}

std::unique_ptr<juce::AccessibilityHandler> ModeSwitch::createAccessibilityHandler()
{
    struct Handler final : juce::AccessibilityHandler
    {
        explicit Handler (ModeSwitch& s)
            : AccessibilityHandler (s, juce::AccessibilityRole::toggleButton,
                                    juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press,
                                                                            [&s] { s.setLive (! s.isLive()); })),
              owner (s) {}

        juce::AccessibleState getCurrentState() const override
        {
            auto state = AccessibilityHandler::getCurrentState().withCheckable();
            return owner.isLive() ? state.withChecked() : state;
        }

        ModeSwitch& owner;
    };
    return std::make_unique<Handler> (*this);
}

//==============================================================================
void LevelMeter::setPeak (float linear)
{
    const float db = linear > 1.0e-6f ? juce::Decibels::gainToDecibels (linear) : -100.0f;
    shownDb = db > shownDb ? db : juce::jmax (db, shownDb - 1.5f);   // ~45 dB/s fall at 30 Hz
    if (db >= holdDb || --holdFrames <= 0)
    {
        holdDb = juce::jmax (db, holdDb - 1.0f);
        if (db >= holdDb)
            holdFrames = 45;   // 1.5 s
    }
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colour::well);
    g.fillRoundedRectangle (r, 2.0f);

    const bool vertical = r.getHeight() > r.getWidth();
    auto span = [&] (float fromDb, float toDb)
    {
        const float a = fractionFor (fromDb), b = fractionFor (toDb);
        return vertical ? juce::Rectangle<float> (r.getX(), r.getBottom() - b * r.getHeight(), r.getWidth(), (b - a) * r.getHeight())
                        : juce::Rectangle<float> (r.getX() + a * r.getWidth(), r.getY(), (b - a) * r.getWidth(), r.getHeight());
    };

    // Lit segments, each in its zone's colour.
    const struct { float from, to; juce::Colour c; } zones[] = {
        { -60.0f, -12.0f, colour::levelOk }, { -12.0f, -0.1f, colour::levelHot }, { -0.1f, 0.0f, colour::levelClip } };
    for (const auto& z : zones)
        if (shownDb > z.from)
        {
            g.setColour (z.c);
            g.fillRect (span (z.from, juce::jmin (shownDb, z.to)));
        }

    if (holdDb > -60.0f)
    {
        g.setColour (levelColour (holdDb));
        const float f = fractionFor (holdDb);
        if (vertical)
            g.fillRect (r.getX(), r.getBottom() - f * r.getHeight() - 1.0f, r.getWidth(), 2.0f);
        else
            g.fillRect (r.getX() + f * r.getWidth() - 1.0f, r.getY(), 2.0f, r.getHeight());
    }
}

//==============================================================================
SoundFieldView::SoundFieldView (juce::AudioProcessorValueTreeState& state)
    : apvts (state),
      yaw (state.getParameter (p::yaw)),
      pitch (state.getParameter (p::pitch))
{
    setRepaintsOnMouseActivity (false);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTitle ("Sound field");
    setDescription ("Live map of where sound arrives from. Drag to rotate the scene; double-click a spot to bring it to the front.");
}

void SoundFieldView::setMap (const std::vector<float>& values, int width, int height)
{
    if (image.getWidth() != width || image.getHeight() != height)
        image = juce::Image (juce::Image::RGB, width, height, false);

    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            // Cube for contrast: a first-order map is broad.
            const float v = values[static_cast<std::size_t> (y * width + x)];
            bitmap.setPixelColour (x, y, viridis (v * v * v));
        }
    repaint();
}

void SoundFieldView::setMarkers (std::vector<SF2AudioProcessor::Marker> m)
{
    markers = std::move (m);
}

juce::Rectangle<float> SoundFieldView::mapArea() const
{
    // Room on the left for elevation labels and below for azimuth labels.
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (38.0f);
    r.removeFromBottom (20.0f);
    r.removeFromRight (8.0f);
    r.removeFromTop (2.0f);
    // Keep the 2:1 equirectangular aspect ratio.
    const float w = juce::jmin (r.getWidth(), r.getHeight() * 2.0f);
    return r.withSizeKeepingCentre (w, w * 0.5f);
}

juce::Point<float> SoundFieldView::toScreen (double az, double el) const
{
    const auto r = mapArea();
    return { r.getX() + r.getWidth() * static_cast<float> ((180.0 - az) / 360.0),
             r.getY() + r.getHeight() * static_cast<float> ((90.0 - el) / 180.0) };
}

void SoundFieldView::paint (juce::Graphics& g)
{
    const auto r = mapArea();
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 4.0f);
        g.reduceClipRegion (clip);
        g.setColour (colour::well);
        g.fillRect (r);
        if (image.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (image, r);
        }

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        for (int az = -135; az <= 135; az += 45)
            g.drawVerticalLine (juce::roundToInt (toScreen (az, 0).x), r.getY(), r.getBottom());
        for (int el : { -45, 45 })
            g.drawHorizontalLine (juce::roundToInt (toScreen (0, el).y), r.getX(), r.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawHorizontalLine (juce::roundToInt (toScreen (0, 0).y), r.getX(), r.getRight());
        g.drawVerticalLine (juce::roundToInt (toScreen (0, 0).x), r.getY(), r.getBottom());
    }

    g.setFont (sf2ui::uiFont (11.0f));
    const struct { double az; const char* name; } compass[] = { { 180, "Back" }, { 90, "Left" }, { 0, "Front" }, { -90, "Right" }, { -180, "Back" } };
    for (const auto& c : compass)
    {
        const auto pt = toScreen (c.az, 0);
        auto box = juce::Rectangle<float> (pt.x - 30.0f, r.getBottom() + 4.0f, 60.0f, 14.0f);
        auto just = juce::Justification::centred;
        if (c.az > 179.0)  { box = box.withX (r.getX()); just = juce::Justification::centredLeft; }
        if (c.az < -179.0) { box = box.withRightX (r.getRight()); just = juce::Justification::centredRight; }
        g.setColour (std::abs (c.az) < 1.0 ? colour::text : colour::dim);
        g.drawText (c.name, box, just);
    }
    g.setColour (colour::dim);
    const struct { double el; const char* name; } heights[] = { { 90, "Up" }, { 45, "45" }, { 0, "0" }, { -45, "-45" }, { -90, "Down" } };
    for (const auto& h : heights)
    {
        const auto pt = toScreen (0, h.el);
        auto box = juce::Rectangle<float> (0.0f, pt.y - 7.0f, r.getX() - 6.0f, 14.0f);
        if (h.el > 89.0)  box = box.withY (r.getY());
        if (h.el < -89.0) box = box.withBottomY (r.getBottom());
        g.drawText (h.name, box, juce::Justification::centredRight);
    }

    g.setFont (sf2ui::uiFont (10.0f, true));
    for (const auto& m : markers)
    {
        const auto pt = toScreen (m.azimuthDeg, m.elevationDeg);
        const auto ring = juce::Rectangle<float> (12.0f, 12.0f).withCentre (pt);
        g.setColour (colour::shell.withAlpha (0.55f));
        g.fillEllipse (ring.expanded (1.5f));
        g.setColour (colour::accent);
        g.drawEllipse (ring, 2.0f);
        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (pt));

        const auto label = juce::Rectangle<float> (pt.x - 30.0f, pt.y + 8.0f, 60.0f, 12.0f);
        g.setColour (colour::shell.withAlpha (0.8f));
        g.drawText (m.label, label.translated (0.0f, 1.0f), juce::Justification::centred);
        g.setColour (colour::text);
        g.drawText (m.label, label, juce::Justification::centred);
    }
}

void SoundFieldView::setParam (juce::RangedAudioParameter* param, float value)
{
    if (param != nullptr)
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void SoundFieldView::mouseDown (const juce::MouseEvent&)
{
    dragYaw = apvts.getRawParameterValue (p::yaw)->load();
    dragPitch = apvts.getRawParameterValue (p::pitch)->load();
    yaw->beginChangeGesture();
    pitch->beginChangeGesture();
}

void SoundFieldView::mouseDrag (const juce::MouseEvent& e)
{
    const auto r = mapArea();
    if (r.isEmpty())
        return;
    // Grabbing the scene: content follows the pointer.
    const float dAz = -static_cast<float> (e.getDistanceFromDragStartX()) * 360.0f / r.getWidth();
    const float dEl = -static_cast<float> (e.getDistanceFromDragStartY()) * 180.0f / r.getHeight();
    setParam (yaw, wrapDegrees (dragYaw + dAz));
    setParam (pitch, juce::jlimit (-90.0f, 90.0f, dragPitch + dEl));
}

void SoundFieldView::mouseUp (const juce::MouseEvent&)
{
    yaw->endChangeGesture();
    pitch->endChangeGesture();
}

void SoundFieldView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto r = mapArea();
    if (! r.contains (e.position))
        return;
    const float az = 180.0f - 360.0f * (e.position.x - r.getX()) / r.getWidth();
    const float el = 90.0f - 180.0f * (e.position.y - r.getY()) / r.getHeight();
    const float y0 = apvts.getRawParameterValue (p::yaw)->load();
    const float p0 = apvts.getRawParameterValue (p::pitch)->load();
    yaw->beginChangeGesture();
    pitch->beginChangeGesture();
    setParam (yaw, wrapDegrees (y0 - az));
    setParam (pitch, juce::jlimit (-90.0f, 90.0f, p0 - el));
    yaw->endChangeGesture();
    pitch->endChangeGesture();
}

//==============================================================================
SF2AudioProcessorEditor::SF2AudioProcessorEditor (SF2AudioProcessor& proc)
    : AudioProcessorEditor (proc), processor (proc), apvts (proc.apvts),
      mode (*proc.apvts.getParameter (p::liveMode)),
      inputFormat (apvts, p::inputFormat, "Format"),
      micModel (apvts, p::micModel, "Microphone"),
      orientation (apvts, p::orientation, "Mounting"),
      spacingCorrection (apvts, p::spacingCorrection, "Spacing correction"),
      windHighPass (apvts, p::windHighPass, "Wind filter", Knob::Kind::Small),
      field (apvts),
      yaw (apvts, p::yaw, "Rotate", Knob::Kind::Small, true),
      pitch (apvts, p::pitch, "Tilt", Knob::Kind::Small, true),
      roll (apvts, p::roll, "Tumble", Knob::Kind::Small, true),
      stereoType (apvts, p::stereoType, "Stereo type"),
      pattern (apvts, p::pattern, "Polar pattern", Knob::Kind::Large, false, true),
      stereoAngle (apvts, p::stereoAngle, "Angle", Knob::Kind::Small),
      abSpacing (apvts, p::abSpacing, "AB spacing", Knob::Kind::Small),
      gain (apvts, p::gain, "Gain", Knob::Kind::Small, true),
      lfeCutoff (apvts, p::lfeCutoff, "LFE cutoff", Knob::Kind::Small),
      shotgunFocus (apvts, p::shotgunFocus, "Focus", Knob::Kind::Small),
      shotgun (apvts, p::shotgun, "Shotgun (mono)"),
      bFormatOut (apvts, p::bFormatOut, "B-format")
{
    setLookAndFeel (&look);

    spacingCorrection.button.setTooltip ("Corrects the capsules' spacing for a flatter response at high frequencies. Adds 0.7 ms.");
    windHighPass.dial.setTooltip ("Steep high-pass on the capsules against wind and handling noise.");
    shotgun.button.setTooltip ("Narrows a mono virtual mic beyond hypercardioid.");

    for (auto* c : std::initializer_list<juce::Component*> { &mode, &inputFormat, &micModel, &orientation, &spacingCorrection,
                                                            &windHighPass, &field, &yaw, &pitch, &roll, &stereoType, &pattern,
                                                            &stereoAngle, &abSpacing, &gain, &lfeCutoff, &shotgunFocus, &shotgun,
                                                            &bFormatOut, &loadSofa, &headModel })
        addAndMakeVisible (*c);

    for (int c = 0; c < 4; ++c)
    {
        auto& cap = capsules[static_cast<std::size_t> (c)];
        cap.trim = std::make_unique<Knob> (apvts, p::capsuleTrim (c + 1), juce::String(), Knob::Kind::Linear, true);
        cap.trim->dial.setTitle ("Capsule " + juce::String (c + 1) + " trim");
        cap.invert.setButtonText (juce::CharPointer_UTF8 ("\xc3\x98"));   // polarity symbol
        cap.invert.setClickingTogglesState (true);
        cap.invert.setTitle ("Capsule " + juce::String (c + 1) + " polarity invert");
        cap.invert.setTooltip ("Invert the polarity of capsule " + juce::String (c + 1));
        cap.invertAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, p::capsuleInvert (c + 1), cap.invert);
        cap.meter.setTitle ("Capsule " + juce::String (c + 1) + " level");
        addAndMakeVisible (cap.meter);
        addAndMakeVisible (*cap.trim);
        addAndMakeVisible (cap.invert);
    }

    for (auto& m : outputMeters)
        addChildComponent (m);

    loadSofa.setTooltip ("Load a measured HRTF set (.sofa) for binaural output");
    loadSofa.onClick = [this] { chooseSofa(); };
    headModel.setTooltip ("Use the built-in spherical head model for binaural output");
    headModel.onClick = [this] { processor.useHeadModel(); };

    // Children were built before the look was set; let them pick it up (slider text boxes are
    // created by the look).
    sendLookAndFeelChange();

    setResizable (true, true);
    setResizeLimits (1000, 640, 2400, 1500);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

SF2AudioProcessorEditor::~SF2AudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

SF2AudioProcessorEditor::Panels SF2AudioProcessorEditor::panelsFor (juce::Rectangle<int> bounds) const
{
    Panels panels;
    auto r = bounds.reduced (space::large);
    panels.header = r.removeFromTop (44);
    r.removeFromTop (space::medium);
    panels.input = r.removeFromLeft (296);
    r.removeFromLeft (space::medium);
    panels.output = r.removeFromRight (316);
    r.removeFromRight (space::medium);
    panels.centre = r;
    return panels;
}

void SF2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colour::shell);
    const auto panels = panelsFor (getLocalBounds());

    for (auto area : { panels.input, panels.centre, panels.output })
    {
        g.setColour (colour::panel);
        g.fillRoundedRectangle (area.toFloat(), space::radius);
        g.setColour (colour::line.withAlpha (0.6f));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), space::radius, 1.0f);
    }

    // Header: name, then latency on the right.
    auto header = panels.header;
    g.setColour (colour::text);
    g.setFont (sf2ui::uiFont (26.0f, true));
    g.drawText ("SF2", header.removeFromLeft (60), juce::Justification::centredLeft);
    g.setColour (colour::dim);
    g.setFont (sf2ui::uiFont (12.0f));
    g.drawText ("Ambisonic microphone processor", header.removeFromLeft (220).translated (0, 2), juce::Justification::centredLeft);

    auto right = header.removeFromRight (250);
    g.setColour (colour::faint);
    g.setFont (sf2ui::uiFont (11.0f));
    g.drawText ("v" JucePlugin_VersionString, right.removeFromRight (44), juce::Justification::centredRight);
    right.removeFromRight (space::large);
    auto top = right.removeFromTop (right.getHeight() / 2);
    sf2ui::drawHeading (g, "Latency", top, juce::Justification::bottomRight);
    g.setFont (sf2ui::numberFont (14.0f));
    g.setColour (mode.isLive() ? colour::accent : colour::text);
    g.drawText (latencyText + "  " + latencySamplesText, right, juce::Justification::topRight);

    // Section headings.
    sf2ui::drawHeading (g, "Input", panels.input.reduced (space::large).removeFromTop (captionHeight));
    if (! capsulesArea.isEmpty())
    {
        auto h = capsulesArea;
        sf2ui::drawHeading (g, "Capsules", h);
        const auto trim = capsules[0].trim->getBounds();
        g.setColour (colour::faint);
        g.setFont (sf2ui::uiFont (11.0f));
        g.drawText ("Trim", h.withX (trim.getX()).withWidth (trim.getWidth()), juce::Justification::centred);
        g.drawText ("Polarity", h.withTrimmedLeft (h.getWidth() - 60), juce::Justification::centredRight);
    }
    for (std::size_t c = 0; c < 4; ++c)
    {
        const auto m = capsules[c].meter.getBounds();
        g.setColour (colour::dim);
        g.setFont (sf2ui::numberFont (12.0f));
        g.drawText (juce::String (c + 1), m.withX (m.getX() - 22).withWidth (14).withSizeKeepingCentre (14, 16), juce::Justification::centred);
    }

    auto fieldHead = panels.centre.reduced (space::large).removeFromTop (captionHeight);
    sf2ui::drawHeading (g, "Sound field", fieldHead);
    g.setColour (colour::faint);
    g.setFont (sf2ui::uiFont (11.0f));
    g.drawText ("Drag to rotate, double-click to face a source", fieldHead, juce::Justification::centredRight);

    auto outHead = panels.output.reduced (space::large).removeFromTop (captionHeight);
    sf2ui::drawHeading (g, "Output", outHead);
    g.setColour (colour::accent);
    g.setFont (sf2ui::uiFont (12.0f, true));
    g.drawText (outputText, outHead, juce::Justification::centredRight);

    // HRTF line.
    if (! hrtfArea.isEmpty())
    {
        g.setColour (loadSofa.isEnabled() ? colour::dim : colour::faint);
        g.setFont (sf2ui::uiFont (11.0f));
        g.drawFittedText (hrtfText, hrtfArea, juce::Justification::centredLeft, 1, 0.85f);
    }

    // Output meters: scale and channel names.
    if (! metersArea.isEmpty() && numMeters > 0)
    {
        auto m = metersArea;
        sf2ui::drawHeading (g, "Output level", m.removeFromTop (captionHeight));
        const auto first = outputMeters[0].getBounds();
        const auto last = outputMeters[static_cast<std::size_t> (numMeters - 1)].getBounds();
        g.setFont (sf2ui::numberFont (10.0f));
        for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
        {
            const float y = static_cast<float> (first.getBottom()) - LevelMeter::fractionFor (db) * static_cast<float> (first.getHeight());
            g.setColour (colour::line);
            g.drawHorizontalLine (juce::roundToInt (y), static_cast<float> (first.getX() - 4), static_cast<float> (last.getRight() + 2));
            g.setColour (colour::faint);
            g.drawText (juce::String (juce::roundToInt (db)), juce::Rectangle<float> (static_cast<float> (m.getX()), y - 6.0f, 24.0f, 12.0f),
                        juce::Justification::centredRight);
        }
        g.setFont (sf2ui::uiFont (9.0f));
        g.setColour (colour::dim);
        for (int o = 0; o < numMeters && o < static_cast<int> (meterNames.size()); ++o)
        {
            const auto b = outputMeters[static_cast<std::size_t> (o)].getBounds();
            g.drawFittedText (meterNames[static_cast<std::size_t> (o)], b.getX() - 4, b.getBottom() + 2, b.getWidth() + 8, 12,
                              juce::Justification::centredTop, 1, 0.7f);
        }
    }
}

void SF2AudioProcessorEditor::resized()
{
    const auto panels = panelsFor (getLocalBounds());

    mode.setBounds (panels.header.withSizeKeepingCentre (176, 32));

    // Input, top to bottom in signal order.
    {
        auto r = panels.input.reduced (space::large);
        r.removeFromTop (captionHeight + space::small);
        inputFormat.setBounds (r.removeFromTop (choiceHeight));
        r.removeFromTop (space::small);
        micModel.setBounds (r.removeFromTop (choiceHeight));
        r.removeFromTop (space::small);
        orientation.setBounds (r.removeFromTop (choiceHeight));
        r.removeFromTop (space::medium);

        auto row = r.removeFromTop (84);
        windHighPass.setBounds (row.removeFromRight (96));
        spacingCorrection.setBounds (row.withSizeKeepingCentre (row.getWidth(), 28).withX (row.getX()));
        r.removeFromTop (space::large);

        capsulesArea = r.removeFromTop (captionHeight);
        r.removeFromTop (space::small);
        for (auto& cap : capsules)
        {
            auto line = r.removeFromTop (28);
            r.removeFromTop (space::unit + 2);
            line.removeFromLeft (22);
            cap.meter.setBounds (line.removeFromLeft (72).withSizeKeepingCentre (72, 8));
            line.removeFromLeft (space::small);
            cap.invert.setBounds (line.removeFromRight (32).withSizeKeepingCentre (32, 24));
            line.removeFromRight (space::small);
            cap.trim->setBounds (line);
        }
    }

    // Centre: the map, with the rotation under it.
    {
        auto r = panels.centre.reduced (space::large);
        r.removeFromTop (captionHeight + space::small);
        // The map keeps its 2:1 shape; the rotation knobs sit right under it, the pair centred.
        const int knobHeight = 92;
        const int fieldHeight = juce::jmin (r.getHeight() - knobHeight - space::medium, (r.getWidth() - 40) / 2 + 22);
        r = r.withSizeKeepingCentre (r.getWidth(), fieldHeight + space::medium + knobHeight);
        field.setBounds (r.removeFromTop (fieldHeight));
        r.removeFromTop (space::medium);
        auto knobs = r;

        const int w = 96;
        auto row = knobs.withSizeKeepingCentre (3 * w + 2 * space::large, knobs.getHeight());
        for (auto* k : { &yaw, &pitch, &roll })
        {
            k->setBounds (row.removeFromLeft (w));
            row.removeFromLeft (space::large);
        }
    }

    // Output.
    {
        auto r = panels.output.reduced (space::large);
        r.removeFromTop (captionHeight + space::small);
        stereoType.setBounds (r.removeFromTop (choiceHeight));
        r.removeFromTop (space::medium);

        auto row = r.removeFromTop (156);
        pattern.setBounds (row.removeFromLeft (156));
        row.removeFromLeft (space::small);
        stereoAngle.setBounds (row.removeFromTop (row.getHeight() / 2).reduced (0, 1));
        abSpacing.setBounds (row.reduced (0, 1));
        r.removeFromTop (space::small);

        row = r.removeFromTop (80);
        const int third = row.getWidth() / 3;
        gain.setBounds (row.removeFromLeft (third));
        shotgunFocus.setBounds (row.removeFromLeft (third));
        lfeCutoff.setBounds (row);
        r.removeFromTop (space::small);

        row = r.removeFromTop (choiceHeight);
        bFormatOut.setBounds (row.removeFromRight (row.getWidth() / 2 - space::unit));
        shotgun.setBounds (row.removeFromBottom (28));
        r.removeFromTop (space::medium);

        hrtfArea = r.removeFromTop (captionHeight);
        r.removeFromTop (space::unit);
        auto buttons = r.removeFromTop (28);
        loadSofa.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).withTrimmedRight (space::unit));
        headModel.setBounds (buttons.withTrimmedLeft (space::unit));
        r.removeFromTop (space::medium);

        metersArea = r;
        auto meters = r.withTrimmedTop (captionHeight + space::small).withTrimmedBottom (14).withTrimmedLeft (30);
        const int n = juce::jmax (1, numMeters);
        const int w = juce::jlimit (6, 18, meters.getWidth() / n);
        for (int o = 0; o < sf2::Engine::maxOutputs; ++o)
            outputMeters[static_cast<std::size_t> (o)].setBounds (meters.getX() + o * w, meters.getY(), w - 3, meters.getHeight());
    }
}

void SF2AudioProcessorEditor::updateEnablement()
{
    const auto description = processor.getOutputDescription();
    const bool stereo = description == "Stereo";
    const bool mono = description == "Mono";
    const bool bFormat = description == "B-format";
    const bool aFormat = juce::roundToInt (apvts.getRawParameterValue (p::inputFormat)->load()) == 0;
    const int type = juce::roundToInt (apvts.getRawParameterValue (p::stereoType)->load());
    const bool binaural = stereo && type == static_cast<int> (sf2::StereoType::Binaural);
    const bool coincident = stereo && type == static_cast<int> (sf2::StereoType::Coincident);
    const bool live = mode.isLive();

    micModel.setEnabled (aFormat);
    orientation.setEnabled (aFormat);
    windHighPass.setEnabled (aFormat);
    spacingCorrection.setEnabled (aFormat && ! live);
    spacingCorrection.button.setTooltip (live ? "Live mode skips spacing correction to save 0.7 ms."
                                              : "Corrects the capsules' spacing for a flatter response at high frequencies. Adds 0.7 ms.");
    for (auto& cap : capsules)
    {
        cap.trim->setEnabled (aFormat);
        cap.invert.setEnabled (aFormat);
    }

    stereoType.setEnabled (stereo);
    pattern.setEnabled (! bFormat && ! binaural && (coincident || ! stereo));
    stereoAngle.setEnabled (coincident);
    abSpacing.setEnabled (stereo && type == static_cast<int> (sf2::StereoType::AB));
    shotgun.setEnabled (mono);
    shotgunFocus.setEnabled (mono && apvts.getRawParameterValue (p::shotgun)->load() > 0.5f);
    lfeCutoff.setEnabled (description.containsIgnoreCase ("LFE") || description.contains (".1") || description.contains (".2"));
    bFormatOut.setEnabled (bFormat);
    loadSofa.setEnabled (binaural);
    headModel.setEnabled (binaural);
}

void SF2AudioProcessorEditor::timerCallback()
{
    auto& meters = processor.getMeters();

    // Sound-field map from the covariance the audio thread has gathered since the last read.
    const float count = meters.covarianceSamples.load (std::memory_order_acquire);
    if (count > 0.0f)
    {
        sf2::Covariance c {};
        for (std::size_t i = 0; i < c.size(); ++i)
            c[i] = meters.covariance[i].load (std::memory_order_relaxed) / count;
        meters.resetCovariance.store (true, std::memory_order_release);
        for (std::size_t i = 0; i < c.size(); ++i)
            smoothed[i] = 0.7 * smoothed[i] + 0.3 * c[i];
    }
    else
    {
        for (auto& v : smoothed)
            v *= 0.9;
    }

    constexpr int mapW = 96, mapH = 48;
    if (smoothed[0] > 1.0e-10)   // W power: above about -100 dBFS
        sf2::renderEnergyMap (smoothed, mapW, mapH, map);
    else
        map.assign (static_cast<std::size_t> (mapW * mapH), 0.0f);
    field.setMap (map, mapW, mapH);

    for (std::size_t c = 0; c < 4; ++c)
        capsules[c].meter.setPeak (meters.inputPeak[c].exchange (0.0f, std::memory_order_relaxed));

    const int outs = juce::jmin (processor.getTotalNumOutputChannels(), sf2::Engine::maxOutputs);
    if (outs != numMeters)
    {
        numMeters = outs;
        meterNames.clear();
        const auto set = processor.getChannelLayoutOfBus (false, 0);
        for (int o = 0; o < outs; ++o)
        {
            const auto type = set.getTypeOfChannel (o);
            meterNames.push_back (set == juce::AudioChannelSet::ambisonic (1) ? juce::String ("WYZX").substring (o, o + 1)
                                                                              : juce::AudioChannelSet::getAbbreviatedChannelTypeName (type));
        }
        for (int o = 0; o < sf2::Engine::maxOutputs; ++o)
            outputMeters[static_cast<std::size_t> (o)].setVisible (o < outs);
        resized();
        repaint();
    }
    for (int o = 0; o < outs; ++o)
        outputMeters[static_cast<std::size_t> (o)].setPeak (meters.outputPeak[static_cast<std::size_t> (o)].exchange (0.0f, std::memory_order_relaxed));

    // Slower things.
    if (++tick % 6 == 0)
    {
        field.setMarkers (processor.getMarkers());

        const int samples = processor.getLatencySamples();
        const double ms = 1000.0 * samples / juce::jmax (1.0, processor.getCurrentSampleRate());
        const auto newLatency = juce::String (ms, 1) + " ms";
        const auto newSamples = juce::String (samples) + " smp";
        const auto newOutput = processor.getOutputDescription();
        const auto problem = processor.getHrtfProblem();
        const auto newHrtf = "HRTF: " + processor.getHrtfName() + (problem.isNotEmpty() ? " (saved file missing)" : "");
        if (newLatency != latencyText || newSamples != latencySamplesText || newOutput != outputText || newHrtf != hrtfText)
        {
            latencyText = newLatency;
            latencySamplesText = newSamples;
            outputText = newOutput;
            hrtfText = newHrtf;
            repaint();
        }
        loadSofa.setTooltip (problem.isNotEmpty() ? problem : "Load a measured HRTF set (.sofa) for binaural output");
        updateEnablement();
        repaint (getLocalBounds().removeFromTop (72));   // Live colour of the latency readout
    }
}

void SF2AudioProcessorEditor::chooseSofa()
{
    chooser = std::make_unique<juce::FileChooser> ("Load an HRTF set", juce::File(), "*.sofa");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<SF2AudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;
                              const auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              juce::String error;
                              if (! safe->processor.loadSofa (file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "SOFA file not loaded", error);
                          });
}
