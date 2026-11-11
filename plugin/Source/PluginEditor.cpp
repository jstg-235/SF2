#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
namespace p = sf2::params;

const juce::Colour background   { 0xff16181d };
const juce::Colour panel        { 0xff1f2229 };
const juce::Colour textColour   { 0xffd8dce3 };
const juce::Colour dimText      { 0xff8a919c };
const juce::Colour accent       { 0xfff2a541 };

constexpr int rowHeight = 26;
constexpr int labelWidth = 112;

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
} // namespace

//==============================================================================
void LevelMeter::setPeak (float linear)
{
    const float db = linear > 1.0e-6f ? juce::Decibels::gainToDecibels (linear) : -100.0f;
    shownDb = db > shownDb ? db : juce::jmax (db, shownDb - 1.5f);   // ~45 dB/s fall at 30 Hz
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0d0e11));
    g.fillRoundedRectangle (r, 2.0f);

    const float frac = juce::jlimit (0.0f, 1.0f, (shownDb + 60.0f) / 60.0f);
    const bool vertical = r.getHeight() > r.getWidth();
    auto lit = vertical ? r.removeFromBottom (r.getHeight() * frac) : r.removeFromLeft (r.getWidth() * frac);
    g.setColour (shownDb > -0.1f ? juce::Colours::red : shownDb > -12.0f ? accent : juce::Colour (0xff4fb286));
    g.fillRoundedRectangle (lit, 2.0f);
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
    auto r = getLocalBounds().toFloat().reduced (4.0f, 18.0f);
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
    g.setColour (juce::Colour (0xff0d0e11));
    g.fillRect (r);
    if (image.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image, r);
    }

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    for (int az = -135; az <= 135; az += 45)
        g.drawVerticalLine (juce::roundToInt (toScreen (az, 0).x), r.getY(), r.getBottom());
    for (int el : { -45, 0, 45 })
        g.drawHorizontalLine (juce::roundToInt (toScreen (0, el).y), r.getX(), r.getRight());

    g.setColour (dimText);
    g.setFont (juce::FontOptions (12.0f));
    const struct { double az; const char* name; } compass[] = { { 180, "Back" }, { 90, "Left" }, { 0, "Front" }, { -90, "Right" }, { -180, "Back" } };
    for (const auto& c : compass)
    {
        const auto pt = toScreen (c.az, 0);
        auto box = juce::Rectangle<float> (pt.x - 30.0f, r.getBottom() + 2.0f, 60.0f, 14.0f);
        auto just = juce::Justification::centred;
        if (c.az > 179.0)  { box = box.withX (r.getX()); just = juce::Justification::centredLeft; }
        if (c.az < -179.0) { box = box.withRightX (r.getRight()); just = juce::Justification::centredRight; }
        g.drawText (c.name, box, just);
    }
    g.drawText ("Up", juce::Rectangle<float> (r.getX(), r.getY() - 16.0f, 40.0f, 14.0f), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (const auto& m : markers)
    {
        const auto pt = toScreen (m.azimuthDeg, m.elevationDeg);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);
        g.setColour (accent);
        g.drawEllipse (pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f, 2.0f);
        g.setColour (textColour);
        g.drawText (m.label, juce::Rectangle<float> (pt.x - 30.0f, pt.y + 7.0f, 60.0f, 13.0f), juce::Justification::centred);
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
    : AudioProcessorEditor (proc), processor (proc), apvts (proc.apvts), field (proc.apvts)
{
    setLookAndFeel (nullptr);
    getLookAndFeel().setColour (juce::ResizableWindow::backgroundColourId, background);

    for (auto* id : { p::inputFormat, p::micModel, p::orientation })
        addRow (inputRows, id);
    addRow (inputRows, p::spacingCorrection, "Spacing correction");
    addRow (inputRows, p::windHighPass);

    for (int c = 0; c < 4; ++c)
    {
        auto& trim = capsuleTrims[static_cast<std::size_t> (c)];
        trim.paramId = p::capsuleTrim (c + 1);
        auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        trim.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, trim.paramId, *slider);
        slider->setTitle ("Capsule " + juce::String (c + 1) + " trim");
        trim.control = std::move (slider);
        addAndMakeVisible (*trim.control);

        auto& inv = capsuleInverts[static_cast<std::size_t> (c)];
        inv.paramId = p::capsuleInvert (c + 1);
        auto button = std::make_unique<juce::ToggleButton> (juce::CharPointer_UTF8 ("\xc3\x98"));   // polarity symbol
        button->setTooltip ("Invert the polarity of capsule " + juce::String (c + 1));
        inv.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, inv.paramId, *button);
        inv.control = std::move (button);
        addAndMakeVisible (*inv.control);

        addAndMakeVisible (inputMeters[static_cast<std::size_t> (c)]);
    }

    for (auto* id : { p::yaw, p::pitch, p::roll })
        addRow (rotationRows, id);

    for (auto* id : { p::stereoType, p::pattern, p::stereoAngle, p::abSpacing, p::shotgun, p::shotgunFocus,
                      p::gain, p::lfeCutoff, p::bFormatOut })
        addRow (outputRows, id);

    for (auto& m : outputMeters)
        addChildComponent (m);

    title.setText ("SF2", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, accent);
    for (auto* l : { &latency, &outputLayout, &hrtfLabel, &capsulesHeading, &outputsHeading })
    {
        l->setColour (juce::Label::textColourId, dimText);
        l->setFont (juce::FontOptions (13.0f));
    }
    capsulesHeading.setText ("Capsules: level, trim, polarity", juce::dontSendNotification);
    outputsHeading.setText ("Output levels", juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &title, &latency, &outputLayout, &hrtfLabel, &capsulesHeading,
                                                            &outputsHeading, &field, &loadSofa, &headModel })
        addAndMakeVisible (*c);

    loadSofa.setTooltip ("Load a measured HRTF set (.sofa) for binaural output");
    loadSofa.onClick = [this] { chooseSofa(); };
    headModel.setTooltip ("Use the built-in spherical head model for binaural output");
    headModel.onClick = [this] { processor.useHeadModel(); };

    setResizable (true, true);
    setResizeLimits (900, 560, 2400, 1500);
    setSize (1100, 640);
    startTimerHz (30);
}

SF2AudioProcessorEditor::~SF2AudioProcessorEditor()
{
    stopTimer();
}

SF2AudioProcessorEditor::Row& SF2AudioProcessorEditor::addRow (std::vector<Row>& column, const juce::String& id, const juce::String& labelText)
{
    auto* param = apvts.getParameter (id);
    jassert (param != nullptr);

    Row row;
    row.paramId = id;
    row.label = std::make_unique<juce::Label> (juce::String(), labelText.isNotEmpty() ? labelText : param->getName (40));
    row.label->setColour (juce::Label::textColourId, textColour);
    row.label->setFont (juce::FontOptions (13.0f));

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        auto box = std::make_unique<juce::ComboBox>();
        box->addItemList (choice->choices, 1);
        box->setTitle (param->getName (40));
        row.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *box);
        row.control = std::move (box);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        auto button = std::make_unique<juce::ToggleButton> ("On");
        button->setTitle (param->getName (40));
        row.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *button);
        row.control = std::move (button);
    }
    else
    {
        auto slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 66, 20);
        slider->setTitle (param->getName (40));
        row.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *slider);
        row.control = std::move (slider);
    }

    addAndMakeVisible (*row.label);
    addAndMakeVisible (*row.control);
    column.push_back (std::move (row));
    return column.back();
}

SF2AudioProcessorEditor::Row* SF2AudioProcessorEditor::find (const juce::String& id)
{
    for (auto* column : { &inputRows, &outputRows, &rotationRows })
        for (auto& r : *column)
            if (r.paramId == id)
                return &r;
    return nullptr;
}

void SF2AudioProcessorEditor::layoutColumn (std::vector<Row>& column, juce::Rectangle<int>& area)
{
    for (auto& r : column)
    {
        auto line = area.removeFromTop (rowHeight);
        area.removeFromTop (4);
        r.label->setBounds (line.removeFromLeft (labelWidth));
        r.control->setBounds (line);
    }
}

void SF2AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (36);
    const int side = juce::jlimit (280, 380, getWidth() / 4);
    g.setColour (panel);
    g.fillRoundedRectangle (area.removeFromLeft (side).toFloat(), 6.0f);
    g.fillRoundedRectangle (area.removeFromRight (side).toFloat(), 6.0f);
}

void SF2AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (36);
    title.setBounds (header.removeFromLeft (80));
    latency.setBounds (header.removeFromRight (360));
    latency.setJustificationType (juce::Justification::centredRight);

    const int side = juce::jlimit (280, 380, getWidth() / 4);
    auto left = area.removeFromLeft (side).reduced (10);
    auto right = area.removeFromRight (side).reduced (10);
    auto centre = area.reduced (10, 0);

    // Input column.
    layoutColumn (inputRows, left);
    left.removeFromTop (6);
    capsulesHeading.setBounds (left.removeFromTop (20));
    for (std::size_t c = 0; c < 4; ++c)
    {
        auto line = left.removeFromTop (rowHeight);
        left.removeFromTop (4);
        inputMeters[c].setBounds (line.removeFromLeft (70).reduced (0, 8));
        line.removeFromLeft (8);
        capsuleInverts[c].control->setBounds (line.removeFromRight (40));
        capsuleTrims[c].control->setBounds (line);
    }

    // Centre: map and rotation.
    auto rotation = centre.removeFromBottom (3 * (rowHeight + 4));
    layoutColumn (rotationRows, rotation);
    field.setBounds (centre);

    // Output column.
    outputLayout.setBounds (right.removeFromTop (20));
    right.removeFromTop (4);
    layoutColumn (outputRows, right);
    right.removeFromTop (4);
    hrtfLabel.setBounds (right.removeFromTop (20));
    auto buttons = right.removeFromTop (rowHeight);
    loadSofa.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
    headModel.setBounds (buttons.reduced (2, 0));
    right.removeFromTop (8);
    outputsHeading.setBounds (right.removeFromTop (20));

    auto meters = right.removeFromTop (juce::jmin (right.getHeight(), 90));
    const int n = juce::jmax (1, numMeters);
    const int w = juce::jmin (18, meters.getWidth() / n);
    for (int o = 0; o < sf2::Engine::maxOutputs; ++o)
        outputMeters[static_cast<std::size_t> (o)].setBounds (meters.getX() + o * w, meters.getY(), w - 3, meters.getHeight());
}

void SF2AudioProcessorEditor::updateEnablement()
{
    const auto description = processor.getOutputDescription();
    const bool stereo = description == "Stereo";
    const bool mono = description == "Mono";
    const bool bFormat = description == "B-format";
    const bool aFormat = juce::roundToInt (apvts.getRawParameterValue (p::inputFormat)->load()) == 0;
    const int stereoType = juce::roundToInt (apvts.getRawParameterValue (p::stereoType)->load());
    const bool binaural = stereo && stereoType == static_cast<int> (sf2::StereoType::Binaural);
    const bool coincident = stereo && stereoType == static_cast<int> (sf2::StereoType::Coincident);

    auto enable = [this] (const juce::String& id, bool on)
    {
        if (auto* r = find (id))
        {
            r->control->setEnabled (on);
            r->label->setAlpha (on ? 1.0f : 0.45f);
        }
    };

    for (auto* id : { p::micModel, p::orientation, p::spacingCorrection, p::windHighPass })
        enable (id, aFormat);
    for (std::size_t c = 0; c < 4; ++c)
    {
        capsuleTrims[c].control->setEnabled (aFormat);
        capsuleInverts[c].control->setEnabled (aFormat);
    }

    enable (p::stereoType, stereo);
    enable (p::pattern, ! bFormat && ! binaural && (coincident || ! stereo));
    enable (p::stereoAngle, coincident);
    enable (p::abSpacing, stereo && stereoType == static_cast<int> (sf2::StereoType::AB));
    enable (p::shotgun, mono);
    enable (p::shotgunFocus, mono && apvts.getRawParameterValue (p::shotgun)->load() > 0.5f);
    enable (p::lfeCutoff, description.containsIgnoreCase ("LFE") || description.contains (".1") || description.contains (".2"));
    enable (p::bFormatOut, bFormat);
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
        inputMeters[c].setPeak (meters.inputPeak[c].exchange (0.0f, std::memory_order_relaxed));

    const int outs = juce::jmin (processor.getTotalNumOutputChannels(), sf2::Engine::maxOutputs);
    if (outs != numMeters)
    {
        numMeters = outs;
        for (int o = 0; o < sf2::Engine::maxOutputs; ++o)
            outputMeters[static_cast<std::size_t> (o)].setVisible (o < outs);
        resized();
    }
    for (int o = 0; o < outs; ++o)
        outputMeters[static_cast<std::size_t> (o)].setPeak (meters.outputPeak[static_cast<std::size_t> (o)].exchange (0.0f, std::memory_order_relaxed));

    // Slower things.
    if (++tick % 6 == 0)
    {
        field.setMarkers (processor.getMarkers());
        outputLayout.setText ("Output: " + processor.getOutputDescription(), juce::dontSendNotification);

        const int samples = processor.getLatencySamples();
        const double ms = 1000.0 * samples / juce::jmax (1.0, processor.getCurrentSampleRate());
        latency.setText ("v" JucePlugin_VersionString "   Latency " + juce::String (ms, 1) + " ms (" + juce::String (samples) + " samples)",
                         juce::dontSendNotification);

        const auto problem = processor.getHrtfProblem();
        hrtfLabel.setText ("HRTF: " + processor.getHrtfName() + (problem.isNotEmpty() ? " (saved file missing)" : ""), juce::dontSendNotification);
        hrtfLabel.setTooltip (problem);
        updateEnablement();
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
