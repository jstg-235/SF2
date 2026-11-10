#include "PluginProcessor.h"
#include "Parameters.h"
#include "PluginEditor.h"

#if SF2_WITH_SOFA
 #include <sf2/SofaFile.h>
#endif

#include <optional>

namespace
{
using CT = juce::AudioChannelSet::ChannelType;

struct RoleMapping { CT type; sf2::SpeakerRole role; };

constexpr RoleMapping roleMappings[] = {
    { CT::left, sf2::SpeakerRole::Left },                           { CT::right, sf2::SpeakerRole::Right },
    { CT::centre, sf2::SpeakerRole::Centre },                       { CT::LFE, sf2::SpeakerRole::LFE },
    { CT::LFE2, sf2::SpeakerRole::LFE2 },
    { CT::leftSurround, sf2::SpeakerRole::LeftSurround },           { CT::rightSurround, sf2::SpeakerRole::RightSurround },
    { CT::leftSurroundSide, sf2::SpeakerRole::LeftSurroundSide },   { CT::rightSurroundSide, sf2::SpeakerRole::RightSurroundSide },
    { CT::leftSurroundRear, sf2::SpeakerRole::LeftSurroundRear },   { CT::rightSurroundRear, sf2::SpeakerRole::RightSurroundRear },
    { CT::leftCentre, sf2::SpeakerRole::LeftCentre },               { CT::rightCentre, sf2::SpeakerRole::RightCentre },
    { CT::centreSurround, sf2::SpeakerRole::CentreSurround },
    { CT::wideLeft, sf2::SpeakerRole::WideLeft },                   { CT::wideRight, sf2::SpeakerRole::WideRight },
    { CT::topFrontLeft, sf2::SpeakerRole::TopFrontLeft },           { CT::topFrontCentre, sf2::SpeakerRole::TopFrontCentre },
    { CT::topFrontRight, sf2::SpeakerRole::TopFrontRight },
    { CT::topSideLeft, sf2::SpeakerRole::TopSideLeft },             { CT::topSideRight, sf2::SpeakerRole::TopSideRight },
    { CT::topRearLeft, sf2::SpeakerRole::TopRearLeft },             { CT::topRearCentre, sf2::SpeakerRole::TopRearCentre },
    { CT::topRearRight, sf2::SpeakerRole::TopRearRight },           { CT::topMiddle, sf2::SpeakerRole::TopMiddle },
};

std::optional<sf2::SpeakerRole> roleFor (CT type) noexcept
{
    for (const auto& m : roleMappings)
        if (m.type == type)
            return m.role;
    return std::nullopt;
}

bool isSpeakerLayout (const juce::AudioChannelSet& set)
{
    if (set.isDisabled() || set.isDiscreteLayout() || set.size() > sf2::Engine::maxOutputs)
        return false;
    for (auto type : set.getChannelTypes())
        if (! roleFor (type).has_value())
            return false;
    return true;
}
} // namespace

SF2AudioProcessor::SF2AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Mic In", juce::AudioChannelSet::ambisonic (1), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", sf2::params::createLayout())
{
    namespace p = sf2::params;
    inputFormat = apvts.getRawParameterValue (p::inputFormat);
    micModel    = apvts.getRawParameterValue (p::micModel);
    orientation = apvts.getRawParameterValue (p::orientation);
    yaw         = apvts.getRawParameterValue (p::yaw);
    pitch       = apvts.getRawParameterValue (p::pitch);
    roll        = apvts.getRawParameterValue (p::roll);
    pattern     = apvts.getRawParameterValue (p::pattern);
    stereoAngle = apvts.getRawParameterValue (p::stereoAngle);
    gain        = apvts.getRawParameterValue (p::gain);
    lfeCutoff   = apvts.getRawParameterValue (p::lfeCutoff);
    bFormatOut  = apvts.getRawParameterValue (p::bFormatOut);
    shotgunFocus = apvts.getRawParameterValue (p::shotgunFocus);
    shotgunOn    = apvts.getRawParameterValue (p::shotgun);
    spacingCorrection = apvts.getRawParameterValue (p::spacingCorrection);
    windHighPass = apvts.getRawParameterValue (p::windHighPass);
    stereoType   = apvts.getRawParameterValue (p::stereoType);
    abSpacing    = apvts.getRawParameterValue (p::abSpacing);
    for (int c = 0; c < 4; ++c)
    {
        capsuleTrim[static_cast<std::size_t> (c)]   = apvts.getRawParameterValue (p::capsuleTrim (c + 1));
        capsuleInvert[static_cast<std::size_t> (c)] = apvts.getRawParameterValue (p::capsuleInvert (c + 1));
    }

    startTimerHz (10);
}

bool SF2AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 4)
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::ambisonic (1) || isSpeakerLayout (out);
}

void SF2AudioProcessor::configureEngineForLayout()
{
    const auto out = getChannelLayoutOfBus (false, 0);
    outputIsBFormat = (out == juce::AudioChannelSet::ambisonic (1));

    if (outputIsBFormat)
    {
        engine.setOutputLayout (bFormatOut->load() > 0.5f ? sf2::OutputMode::FuMa : sf2::OutputMode::AmbiX);
    }
    else if (out == juce::AudioChannelSet::stereo())
    {
        engine.setOutputLayout (sf2::OutputMode::Stereo);
    }
    else
    {
        std::vector<sf2::Speaker> speakers;
        for (auto type : out.getChannelTypes())
            speakers.push_back (sf2::speakerForRole (roleFor (type).value_or (sf2::SpeakerRole::Centre)));
        engine.setOutputLayout (sf2::OutputMode::Speakers, speakers);
    }
}

void SF2AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    configureEngineForLayout();
    engine.setShotgunEnabled (shotgunOn->load() > 0.5f);
    const int latency = engine.getLatencySamples (readParams());
    wantedLatency.store (latency);
    setLatencySamples (latency);
}

void SF2AudioProcessor::reset()
{
    engine.reset();
}

sf2::EngineParams SF2AudioProcessor::readParams() const noexcept
{
    sf2::EngineParams p;
    p.inputFormat    = static_cast<sf2::InputFormat> (juce::roundToInt (inputFormat->load()));
    p.micModel       = static_cast<sf2::MicModel> (juce::roundToInt (micModel->load()));
    p.orientation    = static_cast<sf2::MountOrientation> (juce::roundToInt (orientation->load()));
    p.yawDeg         = yaw->load();
    p.pitchDeg       = pitch->load();
    p.rollDeg        = roll->load();
    p.pattern        = pattern->load();
    p.stereoAngleDeg = stereoAngle->load();
    p.gainDb         = gain->load();
    p.lfeCutoffHz    = lfeCutoff->load();
    p.shotgunFocus   = shotgunFocus->load() / 100.0;
    p.spacingCorrection = spacingCorrection->load() > 0.5f;
    p.windHighPassHz = windHighPass->load() < 10.0f ? 0.0 : windHighPass->load();
    p.stereoType     = static_cast<sf2::StereoType> (juce::roundToInt (stereoType->load()));
    p.abSpacingCm    = abSpacing->load();
    for (std::size_t c = 0; c < 4; ++c)
    {
        p.capsuleTrimDb[c] = capsuleTrim[c]->load();
        p.capsuleInvert[c] = capsuleInvert[c]->load() > 0.5f;
    }
    return p;
}

void SF2AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (outputIsBFormat)
    {
        // Switching AmbiX/FuMa does not allocate, so it is safe here.
        const auto wanted = bFormatOut->load() > 0.5f ? sf2::OutputMode::FuMa : sf2::OutputMode::AmbiX;
        if (engine.getOutputMode() != wanted)
            engine.setOutputLayout (wanted);
    }

    // Shotgun, spacing correction and the stereo type change the latency; the engine handles the
    // switch in real time and the timer reports the new latency to the host.
    engine.setShotgunEnabled (shotgunOn->load() > 0.5f);
    const auto params = readParams();
    wantedLatency.store (engine.getLatencySamples (params), std::memory_order_relaxed);

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int channels = buffer.getNumChannels();

    engine.process (buffer.getArrayOfReadPointers(), juce::jmin (numIn, channels),
                    buffer.getArrayOfWritePointers(), juce::jmin (numOut, channels),
                    buffer.getNumSamples(), params);

    for (int ch = numOut; ch < channels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

void SF2AudioProcessor::timerCallback()
{
    const int wanted = wantedLatency.load (std::memory_order_relaxed);
    if (wanted != getLatencySamples())
        setLatencySamples (wanted);

    engine.getBinaural().pumpPendingFilters();
}

juce::AudioProcessorEditor* SF2AudioProcessor::createEditor()
{
    return new SF2AudioProcessorEditor (*this);
}

bool SF2AudioProcessor::loadSofa (const juce::File& file, juce::String& error)
{
#if SF2_WITH_SOFA
    std::string why;
    const auto set = sf2::loadSofaFile (file.getFullPathName().toStdString(), why);
    if (! set.has_value() || ! engine.getBinaural().setHrirs (*set))
    {
        error = file.getFileName() + ": " + juce::String (why.empty() ? "no usable impulse responses" : why);
        return false;
    }
    sofaFile = file;
    hrtfProblem = {};
    return true;
#else
    juce::ignoreUnused (file);
    error = "This build has no SOFA support.";
    return false;
#endif
}

void SF2AudioProcessor::useHeadModel()
{
    engine.getBinaural().useHeadModel();
    sofaFile = juce::File();
    hrtfProblem = {};
}

juce::String SF2AudioProcessor::getHrtfName() const
{
    return sofaFile == juce::File() ? juce::String ("Built-in head model") : sofaFile.getFileNameWithoutExtension();
}

juce::String SF2AudioProcessor::getOutputDescription() const
{
    const auto out = getChannelLayoutOfBus (false, 0);
    if (out == juce::AudioChannelSet::ambisonic (1))
        return "B-format";
    if (out == juce::AudioChannelSet::mono())
        return "Mono";
    if (out == juce::AudioChannelSet::stereo())
        return "Stereo";
    return out.getDescription();
}

std::vector<SF2AudioProcessor::Marker> SF2AudioProcessor::getMarkers() const
{
    std::vector<Marker> markers;
    const auto out = getChannelLayoutOfBus (false, 0);
    if (out == juce::AudioChannelSet::ambisonic (1))
        return markers;

    const auto p = readParams();
    if (out == juce::AudioChannelSet::stereo())
    {
        if (p.stereoType == sf2::StereoType::Binaural)
            return { { 90.0, 0.0, "L ear" }, { -90.0, 0.0, "R ear" } };
        const auto cfg = sf2::pairConfigFor (p.stereoType, p.abSpacingCm, p.pattern, p.stereoAngleDeg);
        if (cfg.halfAngleDeg < 0.5)
            return { { 0.0, 0.0, "L+R" } };
        return { { cfg.halfAngleDeg, 0.0, "L" }, { -cfg.halfAngleDeg, 0.0, "R" } };
    }

    for (auto type : out.getChannelTypes())
    {
        const auto role = roleFor (type);
        if (! role.has_value())
            continue;
        const auto spk = sf2::speakerForRole (*role);
        if (! spk.isLfe)
            markers.push_back ({ spk.azimuthDeg, spk.elevationDeg, juce::AudioChannelSet::getAbbreviatedChannelTypeName (type) });
    }
    return markers;
}

void SF2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    state.setProperty ("sofaFile", sofaFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SF2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    // Version 1 states simply lack the v1.1 parameters, which then keep their defaults.
    auto state = juce::ValueTree::fromXml (*xml);
    const auto path = state.getProperty ("sofaFile").toString();
    state.removeProperty ("sofaFile", nullptr);
    apvts.replaceState (state);

    if (path.isEmpty() || ! juce::File::isAbsolutePath (path))
    {
        if (sofaFile != juce::File())
            useHeadModel();
        return;
    }

    juce::String error;
    const juce::File file (path);
    if (! loadSofa (file, error))
    {
        useHeadModel();
        hrtfProblem = "Could not load " + file.getFileName() + ", using the head model. " + error;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SF2AudioProcessor();
}
