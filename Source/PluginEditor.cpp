#include "PluginEditor.h"
#include "FactoryPresets.h"
#include "BinaryData.h"
#include <cmath>

namespace
{
juce::Image loadImage(const juce::String& file)
{
    int size=0;
    const auto* data=BinaryData::getNamedResource(file.replaceCharacter('.', '_').toRawUTF8(),size);
    return data?juce::ImageFileFormat::loadFrom(data,static_cast<std::size_t>(size)):juce::Image{};
}
struct Sprite
{
    std::vector<juce::Image> poses;
    juce::Rectangle<int> rect;
    int frames=1, note=-1;
    juce::String parameter;
    bool black=false;
};
Sprite readSprite(const juce::var& data,int frames)
{
    const auto& r=data["rect"];
    Sprite s;
    s.rect={static_cast<int>(r[0]),static_cast<int>(r[1]),static_cast<int>(r[2]),static_cast<int>(r[3])};
    const auto sheet=loadImage(data["asset"].toString());s.frames=frames;s.parameter=data["parameter"].toString();
    s.note=data.hasProperty("note")?static_cast<int>(data["note"]):-1;s.black=static_cast<bool>(data["black"]);
    jassert(sheet.isValid() && sheet.getWidth()==s.rect.getWidth() && sheet.getHeight()==frames*s.rect.getHeight());
    // Independent frame pixels prevent the scaled-image sampler from reading
    // across a filmstrip boundary into the next pose's top/bottom edge.
    s.poses.reserve(static_cast<std::size_t>(frames));
    for(int frame=0;frame<frames;++frame)
        s.poses.push_back(sheet.getClippedImage({0,frame*s.rect.getHeight(),s.rect.getWidth(),s.rect.getHeight()}).createCopy());
    return s;
}
void drawSprite(juce::Graphics& g,const Sprite& s,float amount,bool interpolate)
{
    const auto position=juce::jlimit(0.0f,1.0f,amount)*static_cast<float>(s.frames-1);
    const int lower=interpolate?static_cast<int>(position):juce::roundToInt(position);
    const auto draw=[&](int frame){g.drawImageAt(s.poses[static_cast<std::size_t>(frame)],s.rect.getX(),s.rect.getY());};
    draw(lower);
    // Opaque patches interpolate; transparent keys use true poses without double edges.
    if(interpolate && lower<s.frames-1){g.setOpacity(position-static_cast<float>(lower));draw(lower+1);g.setOpacity(1.0f);}
}
}
class LonglandLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LonglandLookAndFeel()
    {
        const juce::Colour dark(0xff252119), ivory(0xffeadfc7), edge(0xff786c55), accent(0xff985b3d);
        setColour(juce::ComboBox::backgroundColourId,dark);
        setColour(juce::ComboBox::textColourId,ivory);
        setColour(juce::ComboBox::arrowColourId,ivory);
        setColour(juce::ComboBox::outlineColourId,edge);
        setColour(juce::ComboBox::focusedOutlineColourId,accent);
        setColour(juce::PopupMenu::backgroundColourId,dark);
        setColour(juce::PopupMenu::textColourId,ivory);
        setColour(juce::PopupMenu::headerTextColourId,ivory);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,accent);
        setColour(juce::PopupMenu::highlightedTextColourId,ivory);
        setColour(juce::TextButton::buttonColourId,dark);
        setColour(juce::TextButton::buttonOnColourId,accent);
        setColour(juce::TextButton::textColourOffId,ivory);
        setColour(juce::TextButton::textColourOnId,ivory);
        setColour(juce::TooltipWindow::backgroundColourId,ivory);
        setColour(juce::TooltipWindow::textColourId,dark);
        setColour(juce::TooltipWindow::outlineColourId,edge);
    }
    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override
    {
        g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
        g.setColour(juce::Colour(0xff786c55));g.drawRect(0,0,width,height);
    }
    juce::Font getPopupMenuFont() override {return juce::FontOptions(16.0f);}
    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {return juce::FontOptions(juce::jlimit(10.0f,16.0f,box.getHeight()*.62f));}
    juce::Font getTextButtonFont(juce::TextButton&,int height) override
    {return juce::FontOptions(juce::jlimit(10.0f,14.0f,height*.6f));}
};
struct LonglandRenderedSkin
{
    juce::Image panel;
    std::vector<Sprite> knobs,keys;
    Sprite vu;
    int width=1600,height=960;
    LonglandRenderedSkin()
    {
        int size=0;const auto* bytes=BinaryData::getNamedResource("manifest_json",size);
        const auto data=juce::JSON::parse(juce::String::fromUTF8(bytes,size));
        width=static_cast<int>(data["width"]);height=static_cast<int>(data["height"]);panel=loadImage("panel.png");
        for(const auto& item:*data["knobs"].getArray())knobs.push_back(readSprite(item,static_cast<int>(data["knobFrames"])));
        for(const auto& item:*data["keys"].getArray())keys.push_back(readSprite(item,static_cast<int>(data["keyFrames"])));
        std::stable_sort(keys.begin(),keys.end(),[](const auto& a,const auto& b){return a.black<b.black;});
        vu=readSprite(data["vu"],static_cast<int>(data["knobFrames"]));
    }
};
class LonglandSchematicAudioProcessorEditor::RenderedDial final : public juce::Slider
{
public:
    explicit RenderedDial(juce::RangedAudioParameter& p):parameter(p)
    {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        setMouseDragSensitivity(180);setDoubleClickReturnValue(true,p.convertFrom0to1(p.getDefaultValue()));
        setName(p.getName(64));setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics&) override {} // Parent composites assets at one precise scale.
    void mouseEnter(const juce::MouseEvent& e) override {juce::Slider::mouseEnter(e);if(showValue)showValue();}
    float position() const{return parameter.convertTo0to1(static_cast<float>(getValue()));}
    juce::RangedAudioParameter& parameter;
    std::function<void()> showValue;
};

LonglandSchematicAudioProcessorEditor::LonglandSchematicAudioProcessorEditor(LonglandSchematicAudioProcessor& owner)
    :AudioProcessorEditor(&owner),processor(owner)
{
    instrumentLook=std::make_unique<LonglandLookAndFeel>();setLookAndFeel(instrumentLook.get());
    static std::weak_ptr<LonglandRenderedSkin> cached;
    skin=cached.lock();if(!skin){skin=std::make_shared<LonglandRenderedSkin>();cached=skin;}
    setOpaque(true);setWantsKeyboardFocus(true);
    for(const auto& sprite:skin->knobs)
    {
        auto* parameter=processor.state.getParameter(sprite.parameter);jassert(parameter!=nullptr);
        auto dial=std::make_unique<RenderedDial>(*parameter);auto* raw=dial.get();
        juce::String description=parameter->getName(64);
        for(const auto& c:longland::floatControls)if(sprite.parameter==c.id)description=c.description;
        if(sprite.parameter=="waveform")description="Saw / Square / Narrow Pulse / Triangle / Organ";
        if(sprite.parameter=="noiseType")description="Off / Thermal / Pink / Supply Ripple / Control Voltage";
        raw->setTooltip(description+". Drag vertically; double-click to reset.");
        raw->showValue=[this,raw]
        {
            const auto unit=raw->parameter.getLabel();
            const auto text=raw->parameter.isDiscrete()?raw->parameter.getText(raw->position(),48)
                :juce::String(raw->getValue(),unit=="Hz"?0:unit=="s"?3:2);
            valueLabel.setText(raw->parameter.getName(64)+": "+text+" "+unit,juce::dontSendNotification);
        };
        raw->onValueChange=[this,raw,bounds=sprite.rect]
        {
            raw->showValue();
            repaint(screenBounds(bounds).expanded(2));
        };
        addAndMakeVisible(*raw);
        attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,sprite.parameter,*raw));
        dials.push_back(std::move(dial));
    }
    for(const auto& preset:longland::factoryPresets())presetBox.addItem(preset.name,presetBox.getNumItems()+1);
    presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
    presetBox.onChange=[this]{processor.setCurrentProgram(presetBox.getSelectedItemIndex());};
    presetBox.setName("Factory preset");presetBox.setTooltip("Factory instrument state");
    filterButton.setName("Filter mode");filterButton.setClickingTogglesState(true);
    filterButton.setTooltip("Filter mode: LP = low-pass, BP = band-pass. Click to switch.");
    filterButton.onStateChange=[this]{filterButton.setButtonText(filterButton.getToggleState()?"BP":"LP");};
    filterAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"filterMode",filterButton);
    panicButton.onClick=[this]{releaseMouseNote();processor.requestPanic();};panicButton.setTooltip("Release held notes and sustain pedals");
    for(auto* c:std::array<juce::Component*,5>{&presetBox,&filterButton,&valueLabel,&meterLabel,&panicButton})addAndMakeVisible(c);
    for(auto* label:{&valueLabel,&meterLabel}){label->setColour(juce::Label::textColourId,juce::Colour(0xffd7cdb8));label->setFont(juce::FontOptions(12.0f));}
    valueLabel.setText("Drag a knob / Click keys to play",juce::dontSendNotification);
    meterLabel.setTooltip("Output RMS, 300 ms response. 0 VU = -18 dBFS. GR = compressor gain reduction.");
    setResizable(true,true);setResizeLimits(960,600,1600,1000);getConstrainer()->setFixedAspectRatio(1.6);setSize(1280,800);
    lastTimerMs=juce::Time::getMillisecondCounterHiRes();startTimerHz(60);
}
LonglandSchematicAudioProcessorEditor::~LonglandSchematicAudioProcessorEditor()
{stopTimer();releaseMouseNote();attachments.clear();filterAttachment.reset();setLookAndFeel(nullptr);}
juce::Rectangle<int> LonglandSchematicAudioProcessorEditor::screenBounds(juce::Rectangle<int> bounds) const
{return bounds.toFloat().transformedBy(juce::AffineTransform::scale(panelScale).translated(panelBounds.getX(),panelBounds.getY())).getSmallestIntegerContainer();}
void LonglandSchematicAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff24211c));juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(panelScale).translated(panelBounds.getX(),panelBounds.getY()));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);g.drawImageAt(skin->panel,0,0);
    for(const auto& key:skin->keys)drawSprite(g,key,keyTravel[static_cast<std::size_t>(key.note)],false);
    for(std::size_t i=0;i<skin->knobs.size();++i)drawSprite(g,skin->knobs[i],dials[i]->position(),true);
    drawSprite(g,skin->vu,needlePosition,true);
}
void LonglandSchematicAudioProcessorEditor::resized()
{
    const float scale=getWidth()/1600.0f;const auto toolbar=juce::roundToInt(40*scale);
    panelScale=std::min(getWidth()/static_cast<float>(skin->width),(getHeight()-toolbar)/static_cast<float>(skin->height));
    panelBounds={(getWidth()-skin->width*panelScale)*.5f,static_cast<float>(toolbar),skin->width*panelScale,skin->height*panelScale};
    for(std::size_t i=0;i<dials.size();++i)dials[i]->setBounds(screenBounds(skin->knobs[i].rect));
    for(const auto& knob:skin->knobs)if(knob.parameter=="cutoff")
        filterButton.setBounds(screenBounds({knob.rect.getRight()+3,knob.rect.getCentreY()-12,32,24}));
    auto place=[scale](juce::Component& c,int x,int w){c.setBounds(juce::roundToInt(x*scale),4,juce::roundToInt(w*scale),juce::roundToInt(40*scale)-8);};
    place(presetBox,12,310);place(valueLabel,338,750);place(meterLabel,1100,365);place(panicButton,1480,105);
}
void LonglandSchematicAudioProcessorEditor::timerCallback()
{
    const double now=juce::Time::getMillisecondCounterHiRes();
    const auto dt=static_cast<float>(juce::jlimit(.001,.1,(now-lastTimerMs)*.001));lastTimerMs=now;
    for(const auto& key:skin->keys)
    {
        const auto note=static_cast<std::size_t>(key.note);const auto serial=processor.getNoteOnSerial(key.note);
        if(serial!=keySerial[note]){keySerial[note]=serial;keyHoldUntil[note]=now+45.0;}
        const bool down=processor.keyboardState.isNoteOnForChannels(0xffff,key.note)||now<keyHoldUntil[note];const auto old=keyTravel[note];
        keyTravel[note]+=((down?1.0f:0.0f)-old)*(1.0f-std::exp(-dt*(down?55.0f:30.0f)));
        if(std::abs(keyTravel[note]-(down?1.0f:0.0f))<.001f)keyTravel[note]=down?1.0f:0.0f;
        if(juce::roundToInt(old*4)!=juce::roundToInt(keyTravel[note]*4))repaint(screenBounds(key.rect).expanded(2));
    }
    const float vu=juce::Decibels::gainToDecibels(processor.getOutputLevel(),-100.0f)+18.0f;
    const auto target=juce::jlimit(0.0f,1.0f,(vu+20.0f)/23.0f),previous=needlePosition;
    needlePosition+=(target-needlePosition)*(1.0f-std::exp(-dt*24.0f));
    if(std::abs(previous-needlePosition)>.00005f)repaint(screenBounds(skin->vu.rect).expanded(2));
    if(++timerTicks%6==0)
    {
        if(!presetBox.isPopupActive())presetBox.setSelectedItemIndex(processor.getCurrentProgram(),juce::dontSendNotification);
        meterLabel.setText("0 VU = -18 dBFS | GR "+juce::String(processor.getGainReduction(),1)+" dB",juce::dontSendNotification);
    }
}
juce::Point<float> LonglandSchematicAudioProcessorEditor::modelPoint(juce::Point<float> p) const{return(p-panelBounds.getPosition())/panelScale;}
int LonglandSchematicAudioProcessorEditor::noteAt(juce::Point<float> p) const
{
    const auto local=modelPoint(p);for(auto i=skin->keys.rbegin();i!=skin->keys.rend();++i)if(i->rect.toFloat().contains(local))return i->note;return -1;
}
void LonglandSchematicAudioProcessorEditor::playMouseNote(const juce::MouseEvent& e)
{
    const int note=noteAt(e.position);if(note==mouseNote)return;releaseMouseNote();mouseNote=note;
    if(mouseNote>=0)
    {
        float velocity=.8f;for(const auto& key:skin->keys)if(key.note==note)velocity=juce::jlimit(.25f,1.0f,.3f+.7f*(modelPoint(e.position).y-key.rect.getY())/key.rect.getHeight());
        processor.keyboardState.noteOn(16,mouseNote,velocity);
    }
}
void LonglandSchematicAudioProcessorEditor::releaseMouseNote(){if(mouseNote>=0){processor.keyboardState.noteOff(16,mouseNote,0);mouseNote=-1;}}
void LonglandSchematicAudioProcessorEditor::mouseDown(const juce::MouseEvent& e){grabKeyboardFocus();playMouseNote(e);}
void LonglandSchematicAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e){playMouseNote(e);}
void LonglandSchematicAudioProcessorEditor::mouseUp(const juce::MouseEvent&){releaseMouseNote();}
