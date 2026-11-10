#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <sf2/Engine.h>

#include <atomic>

class SF2AudioProcessor final : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    SF2AudioProcessor();
    ~SF2AudioProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int stateVersion = 1;

    void configureEngineForLayout();
    void timerCallback() override;
    sf2::EngineParams readParams() const noexcept;

    sf2::Engine engine;
    bool outputIsBFormat = false;

    std::atomic<float>* inputFormat = nullptr;
    std::atomic<float>* micModel = nullptr;
    std::atomic<float>* orientation = nullptr;
    std::atomic<float>* yaw = nullptr;
    std::atomic<float>* pitch = nullptr;
    std::atomic<float>* roll = nullptr;
    std::atomic<float>* pattern = nullptr;
    std::atomic<float>* stereoAngle = nullptr;
    std::atomic<float>* gain = nullptr;
    std::atomic<float>* lfeCutoff = nullptr;
    std::atomic<float>* bFormatOut = nullptr;
    std::atomic<float>* shotgunFocus = nullptr;
    std::atomic<float>* shotgunOn = nullptr;

    // Latency the audio thread wants reported. Applied on the message thread by the timer,
    // because setLatencySamples() notifies the host and must not run on the audio thread.
    std::atomic<int> wantedLatency { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SF2AudioProcessor)
};
