#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

struct LonglandRenderedSkin;
class LonglandLookAndFeel;
class LonglandSchematicAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LonglandSchematicAudioProcessorEditor(LonglandSchematicAudioProcessor&);
    ~LonglandSchematicAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    class RenderedDial;
    void timerCallback() override;
    juce::Rectangle<int> screenBounds(juce::Rectangle<int>) const;
    juce::Point<float> modelPoint(juce::Point<float>) const;
    int noteAt(juce::Point<float>) const;
    void playMouseNote(const juce::MouseEvent&);
    void releaseMouseNote();
    LonglandSchematicAudioProcessor& processor;
    std::unique_ptr<LonglandLookAndFeel> instrumentLook;
    std::shared_ptr<LonglandRenderedSkin> skin;
    std::vector<std::unique_ptr<RenderedDial>> dials;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::ComboBox presetBox;
    juce::TextButton filterButton { "LP" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterAttachment;
    juce::Label valueLabel, meterLabel;
    juce::TextButton panicButton { "Panic" };
    juce::TooltipWindow tooltips { this, 650 };
    std::array<float,128> keyTravel {};
    std::array<std::uint32_t,128> keySerial {};
    std::array<double,128> keyHoldUntil {};
    juce::Rectangle<float> panelBounds;
    float panelScale=1.0f, needlePosition=0.0f;
    double lastTimerMs=0.0;
    int mouseNote=-1, timerTicks=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LonglandSchematicAudioProcessorEditor)
};
