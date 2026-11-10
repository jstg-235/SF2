// Developer tool: renders the plugin window to a PNG with test signals running through it, so the
// layout can be checked without a DAW. Build with -DSF2_BUILD_DEVTOOLS=ON; run as
//   SF2Screenshot out.png [layout]    layout: stereo (default), binaural, 7.1.4, mono, bformat

#include "../plugin/Source/PluginProcessor.h"
#include "../plugin/Source/Parameters.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>
#include <random>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
        return 1;

    const juce::String layout = argc > 2 ? argv[2] : "stereo";
    std::unique_ptr<juce::AudioProcessor> plugin (createPluginFilter());
    auto& proc = dynamic_cast<SF2AudioProcessor&> (*plugin);

    auto buses = proc.getBusesLayout();
    if (layout == "7.1.4")        buses.outputBuses.getReference (0) = juce::AudioChannelSet::create7point1point4();
    else if (layout == "mono")    buses.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    else if (layout == "bformat") buses.outputBuses.getReference (0) = juce::AudioChannelSet::ambisonic (1);
    proc.setBusesLayout (buses);
    if (layout == "binaural")
        if (auto* p = proc.apvts.getParameter (sf2::params::stereoType))
            p->setValueNotifyingHost (p->convertTo0to1 (5.0f));

    const int block = 512;
    proc.prepareToPlay (48000.0, block);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (1100, 640);

    // Noise from front-left and a little above, as an A-format NT-SF1 would capture it.
    std::mt19937 rng (1);
    std::normal_distribution<float> n (0.0f, 0.2f);
    const double k = 1.0 / std::sqrt (3.0);
    const double axes[4][3] = { { k, k, k }, { k, -k, -k }, { -k, k, -k }, { -k, -k, k } };
    const double az = 40.0 * juce::MathConstants<double>::pi / 180.0, el = 15.0 * juce::MathConstants<double>::pi / 180.0;
    const double d[3] = { std::cos (az) * std::cos (el), std::sin (az) * std::cos (el), std::sin (el) };

    juce::AudioBuffer<float> buffer (juce::jmax (4, proc.getTotalNumOutputChannels()), block);
    juce::MidiBuffer midi;

    // Feed audio from a timer so the editor's own timers run in between, as in a host.
    struct Feeder final : juce::Timer
    {
        std::function<void()> tick;
        void timerCallback() override { tick(); }
    } feeder;
    int frames = 0;
    feeder.tick = [&]
    {
        for (int b = 0; b < 4; ++b)
        {
            buffer.clear();
            for (int i = 0; i < block; ++i)
            {
                const float s = n (rng);
                for (int c = 0; c < 4; ++c)
                    buffer.setSample (c, i, static_cast<float> (s * (0.5 + 0.5 * (axes[c][0] * d[0] + axes[c][1] * d[1] + axes[c][2] * d[2]))));
            }
            proc.processBlock (buffer, midi);
        }
        if (++frames == 60)
        {
            feeder.stopTimer();
            juce::MessageManager::getInstance()->stopDispatchLoop();
        }
    };
    feeder.startTimer (34);
    juce::MessageManager::getInstance()->runDispatchLoop();

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (image, stream);
    editor.reset();
    plugin->releaseResources();
    return ok ? 0 : 2;
}
