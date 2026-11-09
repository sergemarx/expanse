#include "PluginEditor.h"

namespace
{
const juce::Colour kBgTop    { 0xff0b1020 };
const juce::Colour kBgBottom { 0xff1b1440 };
const juce::Colour kAccent   { 0xff7fd6ff };
const juce::Colour kAccent2  { 0xffc79bff };
const juce::Colour kText     { 0xffdfe6ff };

constexpr int kCellW = 96;
constexpr int kCellH = 118;
constexpr int kMargin = 22;
constexpr int kHeader = 62;
constexpr int kSectionTitle = 24;
}

ExpanseAudioProcessorEditor::KnobComp::KnobComp (APVTS& state, const juce::String& paramID,
                                                 const juce::String& text)
    : attachment (state, paramID, slider)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff2c3358));
    slider.setColour (juce::Slider::thumbColourId, kAccent2);
    slider.setColour (juce::Slider::textBoxTextColourId, kText);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kCellW - 8, 16);
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, kText.withAlpha (0.8f));
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void ExpanseAudioProcessorEditor::KnobComp::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

ExpanseAudioProcessorEditor::ChoiceComp::ChoiceComp (APVTS& state, const juce::String& paramID,
                                                     const juce::String& text,
                                                     const juce::StringArray& items)
    : attachment (state, paramID, box)
{
    box.addItemList (items, 1);
    box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff20274a));
    box.setColour (juce::ComboBox::textColourId, kText);
    box.setColour (juce::ComboBox::outlineColourId, kAccent.withAlpha (0.4f));
    box.setColour (juce::ComboBox::arrowColourId, kAccent);
    addAndMakeVisible (box);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, kText.withAlpha (0.8f));
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (label);
}

void ExpanseAudioProcessorEditor::ChoiceComp::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 26));
}

ExpanseAudioProcessorEditor::ExpanseAudioProcessorEditor (ExpanseAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      mix (p.apvts, "mix", "MIX"), decay (p.apvts, "decay", "DECAY"), size (p.apvts, "size", "SIZE"),
      predelay (p.apvts, "predelay", "PRE-DELAY"), damping (p.apvts, "damping", "DAMPING"),
      modulation (p.apvts, "modulation", "MODULATION"), diffusion (p.apvts, "diffusion", "DIFFUSION"),
      lowcut (p.apvts, "lowcut", "LOW CUT"),
      shimmer (p.apvts, "shimmer", "SHIMMER"), reverse (p.apvts, "reverse", "REVERSE"),
      revTime (p.apvts, "revTime", "REV TIME"), lofi (p.apvts, "lofi", "LO-FI"),
      interval (p.apvts, "shimInterval", "SHIMMER PITCH", ExpanseAudioProcessor::intervalNames()),
      freezeAttachment (p.apvts, "freeze", freezeButton),
      delayMix (p.apvts, "delayMix", "DELAY MIX"), delayTime (p.apvts, "delayTime", "TIME"),
      delayFb (p.apvts, "delayFb", "FEEDBACK"), width (p.apvts, "width", "WIDTH"),
      output (p.apvts, "output", "OUTPUT")
{
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff20274a));
    freezeButton.setColour (juce::TextButton::buttonOnColourId, kAccent);
    freezeButton.setColour (juce::TextButton::textColourOffId, kText);
    freezeButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff0b1020));

    sections = {
        { "SPACE",  { &mix, &decay, &size, &predelay, &damping, &modulation, &diffusion, &lowcut } },
        { "TAILS",  { &shimmer, &interval, &reverse, &revTime, &lofi, &freezeButton } },
        { "DELAY & OUTPUT", { &delayMix, &delayTime, &delayFb, &width, &output } },
    };

    for (auto& s : sections)
        for (auto* c : s.items)
            addAndMakeVisible (c);

    setSize (kMargin * 2 + kCellW * 8, kHeader + (int) sections.size() * (kSectionTitle + kCellH) + kMargin);
}

ExpanseAudioProcessorEditor::~ExpanseAudioProcessorEditor() = default;

void ExpanseAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0.0f, 0.0f, kBgBottom,
                                             (float) getWidth(), (float) getHeight(), false));
    g.fillAll();

    g.setColour (kText);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("EXPANSE", kMargin, 12, 300, 34, juce::Justification::centredLeft);
    g.setColour (kAccent.withAlpha (0.8f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("ambient reverb for guitar", kMargin + 150, 18, 300, 24, juce::Justification::centredLeft);

    int y = kHeader;
    for (auto& s : sections)
    {
        g.setColour (kAccent2.withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, kMargin, y, 300, kSectionTitle, juce::Justification::centredLeft);
        g.setColour (kAccent.withAlpha (0.15f));
        g.drawHorizontalLine (y + kSectionTitle - 2, (float) kMargin, (float) (getWidth() - kMargin));
        y += kSectionTitle + kCellH;
    }
}

void ExpanseAudioProcessorEditor::resized()
{
    int y = kHeader + kSectionTitle;
    for (auto& s : sections)
    {
        int x = kMargin;
        for (auto* c : s.items)
        {
            juce::Rectangle<int> cell (x, y, kCellW, kCellH);
            if (c == &freezeButton)
                c->setBounds (cell.withSizeKeepingCentre (kCellW - 12, 40));
            else
                c->setBounds (cell);
            x += kCellW;
        }
        y += kSectionTitle + kCellH;
    }
}
