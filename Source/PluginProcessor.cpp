#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Semitone offsets matching ExpanseAudioProcessor::intervalNames().
constexpr float kIntervals[] = { -12.0f, -5.0f, 5.0f, 7.0f, 12.0f, 19.0f };
}

const juce::StringArray& ExpanseAudioProcessor::intervalNames()
{
    static const juce::StringArray names { "Octave Down", "4th Down", "4th Up",
                                           "5th Up", "Octave Up", "Octave + 5th Up" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout ExpanseAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    auto pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto ms  = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; };
    auto sec = [] (float v, int) { return juce::String (v, 1) + " s"; };
    auto hz  = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; };
    auto db  = [] (float v, int) { return juce::String (v, 1) + " dB"; };
    auto sz  = [] (float v, int) { return juce::String (v, 2) + "x"; };

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (auto param) { p.push_back (std::move (param)); };

    add (std::make_unique<Float> (juce::ParameterID { "mix", 1 },       "Mix",        Range (0.0f, 1.0f, 0.001f), 0.35f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "decay", 1 },     "Decay",      Range (0.3f, 60.0f, 0.01f, 0.4f), 8.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (sec)));
    add (std::make_unique<Float> (juce::ParameterID { "size", 1 },      "Size",       Range (0.5f, 2.5f, 0.01f), 1.5f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (sz)));
    add (std::make_unique<Float> (juce::ParameterID { "predelay", 1 },  "Pre-Delay",  Range (0.0f, 300.0f, 0.1f, 0.5f), 25.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    add (std::make_unique<Float> (juce::ParameterID { "damping", 1 },   "Damping",    Range (0.0f, 1.0f, 0.001f), 0.35f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "modulation", 1 },"Modulation", Range (0.0f, 1.0f, 0.001f), 0.4f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "diffusion", 1 }, "Diffusion",  Range (0.0f, 1.0f, 0.001f), 0.7f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "lowcut", 1 },    "Low Cut",    Range (20.0f, 1000.0f, 1.0f, 0.4f), 120.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (hz)));

    add (std::make_unique<Float> (juce::ParameterID { "shimmer", 1 },   "Shimmer",    Range (0.0f, 1.0f, 0.001f), 0.3f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "shimInterval", 1 }, "Shimmer Interval", intervalNames(), 4));
    add (std::make_unique<Float> (juce::ParameterID { "reverse", 1 },   "Reverse",    Range (0.0f, 1.0f, 0.001f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "revTime", 1 },   "Reverse Time", Range (200.0f, 2000.0f, 1.0f), 800.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "freeze", 1 }, "Freeze", false));
    add (std::make_unique<Float> (juce::ParameterID { "lofi", 1 },      "Lo-Fi",      Range (0.0f, 1.0f, 0.001f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    add (std::make_unique<Float> (juce::ParameterID { "delayMix", 1 },  "Delay Mix",  Range (0.0f, 1.0f, 0.001f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "delayTime", 1 }, "Delay Time", Range (20.0f, 2000.0f, 1.0f, 0.4f), 375.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (ms)));
    add (std::make_unique<Float> (juce::ParameterID { "delayFb", 1 },   "Delay Feedback", Range (0.0f, 0.95f, 0.001f), 0.4f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    add (std::make_unique<Float> (juce::ParameterID { "width", 1 },     "Width",      Range (0.0f, 2.0f, 0.01f), 1.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    add (std::make_unique<Float> (juce::ParameterID { "output", 1 },    "Output",     Range (-24.0f, 12.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    return { p.begin(), p.end() };
}

ExpanseAudioProcessor::ExpanseAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
}

bool ExpanseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo out; mono or stereo in (mono guitar tracks are summed to both sides).
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void ExpanseAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void ExpanseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);
    if (totalIn == 1 && totalOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    if (buffer.getNumChannels() < 2)
        return;

    params.mix           = get ("mix");
    params.decay         = get ("decay");
    params.size          = get ("size");
    params.preDelayMs    = get ("predelay");
    params.damping       = get ("damping");
    params.modulation    = get ("modulation");
    params.diffusion     = get ("diffusion");
    params.lowCutHz      = get ("lowcut");
    params.shimmer       = get ("shimmer");
    params.shimmerSemis  = kIntervals[juce::jlimit (0, 5, (int) std::lround (get ("shimInterval")))];
    params.reverse       = get ("reverse");
    params.reverseTimeMs = get ("revTime");
    params.freeze        = get ("freeze") > 0.5f;
    params.lofi          = get ("lofi");
    params.delayMix      = get ("delayMix");
    params.delayTimeMs   = get ("delayTime");
    params.delayFeedback = get ("delayFb");
    params.width         = get ("width");
    params.outputDb      = get ("output");

    engine.setParams (params);
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
}

juce::AudioProcessorEditor* ExpanseAudioProcessor::createEditor()
{
    return new ExpanseAudioProcessorEditor (*this);
}

void ExpanseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ExpanseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExpanseAudioProcessor();
}
