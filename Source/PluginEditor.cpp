#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
constexpr int headerHeight = 82;
constexpr int moduleTop = 100;
constexpr int moduleHeight = 190;
}

LonglandSchematicAudioProcessorEditor::LonglandSchematicAudioProcessorEditor(LonglandSchematicAudioProcessor& owner)
    : AudioProcessorEditor(&owner), processor(owner)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(860, 590, 1400, 900);
    setSize(1060, 700);

    for (const auto& preset : longland::factoryPresets())
        presetBox.addItem(preset.name, presetBox.getNumItems() + 1);
    presetBox.setSelectedItemIndex(processor.getCurrentProgram(), juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        processor.setCurrentProgram(presetBox.getSelectedItemIndex());
    };
    presetLabel.setText("INSTRUMENT STATE", juce::dontSendNotification);

    oscillatorBox.addItemList({ "Saw", "Square", "Narrow Pulse", "Triangle", "Organ" }, 1);
    filterBox.addItemList({ "Low-pass", "Band-pass" }, 1);
    noiseTypeBox.addItemList({ "Off", "Thermal", "Pink", "Supply Ripple", "Control Voltage" }, 1);
    oscillatorAttachment = std::make_unique<ComboAttachment>(processor.state, "waveform", oscillatorBox);
    filterAttachment = std::make_unique<ComboAttachment>(processor.state, "filterMode", filterBox);
    noiseTypeAttachment = std::make_unique<ComboAttachment>(processor.state, "noiseType", noiseTypeBox);
    oscillatorLabel.setText("OSCILLATOR", juce::dontSendNotification);
    filterLabel.setText("MODE", juce::dontSendNotification);
    noiseTypeLabel.setText("NOISE", juce::dontSendNotification);

    configureDial(age, ageLabel, "AGE", "age");
    configureDial(body, bodyLabel, "BODY", "body");
    configureDial(drift, driftLabel, "DRIFT", "drift");
    configureDial(cutoff, cutoffLabel, "CUTOFF", "cutoff");
    configureDial(resonance, resonanceLabel, "RESONANCE", "resonance");
    configureDial(attack, attackLabel, "ATTACK", "attack");
    configureDial(decay, decayLabel, "DECAY", "decay");
    configureDial(sustain, sustainLabel, "SUSTAIN", "sustain");
    configureDial(release, releaseLabel, "RELEASE", "release");
    configureDial(ensemble, ensembleLabel, "ENSEMBLE", "ensemble");
    configureDial(compressor, compressorLabel, "COMP", "compressor");
    configureDial(output, outputLabel, "VOLUME", "output");

    const std::array<juce::Component*, 8> primaryComponents {
        &presetBox, &oscillatorBox, &filterBox, &noiseTypeBox, &presetLabel, &oscillatorLabel,
        &filterLabel, &noiseTypeLabel
    };
    for (auto* component : primaryComponents)
        addAndMakeVisible(component);

    for (auto* label : { &presetLabel, &oscillatorLabel, &filterLabel, &noiseTypeLabel, &ageLabel, &bodyLabel, &driftLabel,
                          &cutoffLabel, &resonanceLabel, &attackLabel, &decayLabel, &sustainLabel,
                          &releaseLabel, &ensembleLabel, &compressorLabel, &outputLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colours::black);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    }

    startTimerHz(12);
}

void LonglandSchematicAudioProcessorEditor::configureDial(juce::Slider& slider, juce::Label& label,
                                                            const juce::String& name, const char* parameterId)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 19);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff303030));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffc8c8c8));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    label.setText(name, juce::dontSendNotification);
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.state, parameterId, slider));
}

void LonglandSchematicAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::white);
    graphics.setColour(juce::Colours::black);
    graphics.setFont(juce::FontOptions(28.0f).withStyle("Bold"));
    graphics.drawText("LONGLAND SCHEMATIC", 24, 13, 480, 34, juce::Justification::centredLeft);
    graphics.setFont(juce::FontOptions(13.0f));
    graphics.drawText("A MEMORY SYNTHESIZER  /  DEVELOPMENT PANEL", 26, 49, 470, 20,
                      juce::Justification::centredLeft);
    graphics.drawLine(24.0f, 76.0f, static_cast<float>(getWidth() - 24), 76.0f, 1.5f);

    const int availableWidth = getWidth() - 48;
    const int gap = 18;
    const int moduleWidth = (availableWidth - gap * 4) / 5;
    const std::array<juce::String, 5> titles { "OSC", "DRIFT + AGE", "FILTER", "VCA", "MASTER" };
    const std::array<juce::String, 5> details { "band-limited source", "memory / thermal", "2-pole circuit",
                                                "imperfect envelope", "HPF > RMS > VCA > XFMR" };

    for (int index = 0; index < 5; ++index)
    {
        const int x = 24 + index * (moduleWidth + gap);
        drawModule(graphics, { x, moduleTop, moduleWidth, moduleHeight }, titles[index], details[index]);
        if (index < 4)
        {
            const float from = static_cast<float>(x + moduleWidth);
            const float to = static_cast<float>(x + moduleWidth + gap);
            const float y = static_cast<float>(moduleTop + moduleHeight / 2);
            graphics.drawLine(from, y, to, y, 1.2f);
            juce::Path arrow;
            arrow.addTriangle(to, y, to - 6.0f, y - 4.0f, to - 6.0f, y + 4.0f);
            graphics.fillPath(arrow);
        }
    }

    const int debugTop = 330;
    graphics.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    graphics.drawText("VIRTUAL VOICE CARDS", 24, debugTop, 300, 22, juce::Justification::centredLeft);
    graphics.setFont(juce::FontOptions(12.0f));
    graphics.drawText("BUS GR  " + juce::String(debugState.masterGainReductionDb, 1) + " dB    SUPPLY  "
                          + juce::String(debugState.supplyVoltage * 12.0f, 3) + " V    ACTIVE  "
                          + juce::String(debugState.activeVoices) + "/8",
                      getWidth() - 430, debugTop, 406, 22, juce::Justification::centredRight);

    const auto table = juce::Rectangle<int>(24, debugTop + 28, getWidth() - 48, 202);
    graphics.drawRect(table, 1);
    const int columnWidth = table.getWidth() / 8;
    for (int index = 0; index < 8; ++index)
    {
        const auto cell = juce::Rectangle<int>(table.getX() + index * columnWidth, table.getY(), columnWidth,
                                                table.getHeight());
        if (index > 0)
            graphics.drawVerticalLine(cell.getX(), static_cast<float>(cell.getY()), static_cast<float>(cell.getBottom()));
        const auto& voice = debugState.voices[static_cast<std::size_t>(index)];
        graphics.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        graphics.drawText("VOICE " + juce::String(index + 1), cell.reduced(5).removeFromTop(24),
                          juce::Justification::centred);
        graphics.setFont(juce::FontOptions(11.5f));
        auto content = cell.reduced(7);
        content.removeFromTop(29);
        const juce::String note = voice.active ? juce::MidiMessage::getMidiNoteName(voice.midiNote, true, true, 3) : "--";
        const juce::String lines = note + "\n" + juce::String(voice.centsOffset, 2) + " ct\n"
            + "T " + juce::String(voice.temperature, 3) + "\n"
            + "F " + juce::String(voice.filterMismatch, 3) + "\n"
            + "C " + juce::String(voice.capacitorAge, 3) + "\n"
            + "A " + juce::String(voice.amplitudeMismatch, 3);
        graphics.drawFittedText(lines, content, juce::Justification::centred, 6, 0.85f);
        if (voice.active)
            graphics.fillEllipse(static_cast<float>(cell.getCentreX() - 3), static_cast<float>(cell.getBottom() - 13), 6.0f, 6.0f);
    }

    graphics.setFont(juce::FontOptions(10.5f));
    graphics.drawText("T temperature    F filter calibration    C envelope capacitor    A VCA gain",
                      24, getHeight() - 30, getWidth() - 48, 18, juce::Justification::centredLeft);
}

void LonglandSchematicAudioProcessorEditor::drawModule(juce::Graphics& graphics, juce::Rectangle<int> bounds,
                                                        const juce::String& title, const juce::String& detail)
{
    graphics.setColour(juce::Colours::black);
    graphics.drawRect(bounds, 1);
    graphics.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    graphics.drawText(title, bounds.removeFromTop(22), juce::Justification::centred);
    graphics.drawHorizontalLine(bounds.getY(), static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
    graphics.setFont(juce::FontOptions(10.0f));
    graphics.drawText(detail, bounds.removeFromBottom(18), juce::Justification::centred);
}

void LonglandSchematicAudioProcessorEditor::resized()
{
    presetLabel.setBounds(getWidth() - 304, 10, 280, 18);
    presetBox.setBounds(getWidth() - 304, 31, 280, 30);

    const int availableWidth = getWidth() - 48;
    const int gap = 18;
    const int moduleWidth = (availableWidth - gap * 4) / 5;
    auto module = [moduleWidth, gap] (int index)
    {
        return juce::Rectangle<int>(24 + index * (moduleWidth + gap), moduleTop, moduleWidth, moduleHeight).reduced(9, 27);
    };
    auto placeDial = [] (juce::Rectangle<int> area, juce::Slider& dial, juce::Label& label, int slot, int count)
    {
        const int width = area.getWidth() / count;
        auto bounds = juce::Rectangle<int>(area.getX() + slot * width, area.getY(), width, area.getHeight());
        label.setBounds(bounds.removeFromTop(18));
        dial.setBounds(bounds.reduced(1));
    };

    auto oscArea = module(0);
    oscillatorLabel.setBounds(oscArea.removeFromTop(18));
    oscillatorBox.setBounds(oscArea.removeFromTop(28));

    auto ageArea = module(1);
    placeDial(ageArea, age, ageLabel, 0, 3);
    placeDial(ageArea, drift, driftLabel, 1, 3);
    placeDial(ageArea, body, bodyLabel, 2, 3);

    auto filterArea = module(2);
    auto modeRow = filterArea.removeFromBottom(29);
    filterLabel.setBounds(modeRow.removeFromLeft(43));
    filterBox.setBounds(modeRow);
    placeDial(filterArea, cutoff, cutoffLabel, 0, 2);
    placeDial(filterArea, resonance, resonanceLabel, 1, 2);

    auto vcaArea = module(3);
    const int halfHeight = vcaArea.getHeight() / 2;
    auto top = vcaArea.removeFromTop(halfHeight);
    placeDial(top, attack, attackLabel, 0, 2);
    placeDial(top, decay, decayLabel, 1, 2);
    placeDial(vcaArea, sustain, sustainLabel, 0, 2);
    placeDial(vcaArea, release, releaseLabel, 1, 2);

    auto outputArea = module(4);
    auto noiseRow = outputArea.removeFromBottom(29);
    noiseTypeLabel.setBounds(noiseRow.removeFromLeft(43));
    noiseTypeBox.setBounds(noiseRow);
    placeDial(outputArea, ensemble, ensembleLabel, 0, 3);
    placeDial(outputArea, compressor, compressorLabel, 1, 3);
    placeDial(outputArea, output, outputLabel, 2, 3);
}

void LonglandSchematicAudioProcessorEditor::timerCallback()
{
    debugState = processor.getDebugState();
    if (!presetBox.isPopupActive())
        presetBox.setSelectedItemIndex(processor.getCurrentProgram(), juce::dontSendNotification);
    repaint();
}
