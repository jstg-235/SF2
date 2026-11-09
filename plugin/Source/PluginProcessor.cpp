#include "PluginProcessor.h"
#include "Parameters.h"

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
    engine.prepare (sampleRate, samplesPerBlock);
    configureEngineForLayout();
    setLatencySamples (0);
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

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int channels = buffer.getNumChannels();

    engine.process (buffer.getArrayOfReadPointers(), juce::jmin (numIn, channels),
                    buffer.getArrayOfWritePointers(), juce::jmin (numOut, channels),
                    buffer.getNumSamples(), readParams());

    for (int ch = numOut; ch < channels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SF2AudioProcessor::createEditor()
{
    // Placeholder until the sound-field editor (docs/DESIGN.md, "Interface design") is built.
    return new juce::GenericAudioProcessorEditor (*this);
}

void SF2AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SF2AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));   // migrate on stateVersion here when it changes
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SF2AudioProcessor();
}
