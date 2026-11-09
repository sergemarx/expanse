#pragma once
#include "PluginProcessor.h"

class ExpanseAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ExpanseAudioProcessorEditor (ExpanseAudioProcessor&);
    ~ExpanseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct KnobComp : public juce::Component
    {
        KnobComp (APVTS& state, const juce::String& paramID, const juce::String& text);
        void resized() override;

        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        APVTS::SliderAttachment attachment;
    };

    struct ChoiceComp : public juce::Component
    {
        ChoiceComp (APVTS& state, const juce::String& paramID, const juce::String& text,
                    const juce::StringArray& items);
        void resized() override;

        juce::ComboBox box;
        juce::Label label;
        APVTS::ComboBoxAttachment attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<juce::Component*> items;
    };

    ExpanseAudioProcessor& audioProcessor;

    KnobComp mix, decay, size, predelay, damping, modulation, diffusion, lowcut;
    KnobComp shimmer, reverse, revTime, lofi;
    ChoiceComp interval;
    juce::TextButton freezeButton { "FREEZE" };
    APVTS::ButtonAttachment freezeAttachment;
    KnobComp delayMix, delayTime, delayFb, width, output;

    std::vector<Section> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExpanseAudioProcessorEditor)
};
