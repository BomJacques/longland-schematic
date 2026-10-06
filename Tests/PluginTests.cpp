#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

namespace
{
int failures=0;
void expect(bool condition,const char* message){if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}}
void pump(int milliseconds=30){juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);}
void parameter(LonglandSchematicAudioProcessor& p,const char* id,float value)
{auto* v=p.state.getParameter(id);expect(v!=nullptr,"parameter exists");v->setValueNotifyingHost(v->convertTo0to1(value));}
void render(LonglandSchematicAudioProcessor& p,int blocks,juce::MidiBuffer events={})
{
    juce::AudioBuffer<float> audio(2,256);
    for(int b=0;b<blocks;++b)
    {
        p.processBlock(audio,events);
        for(int ch=0;ch<2;++ch)for(int i=0;i<256;++i)expect(std::isfinite(audio.getSample(ch,i)),"plug-in audio finite");
    }
}
void saveSnapshot(juce::AudioProcessorEditor& editor,const juce::File& directory,const juce::String& name)
{
    const auto image=editor.createComponentSnapshot(editor.getLocalBounds());
    auto stream=directory.getChildFile(name).createOutputStream();
    expect(stream!=nullptr,"snapshot file opens");
    if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
}
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    LonglandSchematicAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000,256);processor.prepareToPlay(48000,256);
    expect(processor.getParameters().size()==31,"30 rendered knobs plus filter mode are exposed");
    for(const auto& c:longland::floatControls)
    {
        auto* p=processor.state.getParameter(c.id);
        expect(p!=nullptr,"schema parameter exists");
        expect(std::abs(p->convertFrom0to1(p->getDefaultValue())-longland::Parameters{}.*(c.member))<.002f,"schema default preserved");
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setVisible(true);pump();
    int sliders=0;for(auto* child:editor->getChildren())if(dynamic_cast<juce::Slider*>(child))++sliders;
    expect(sliders==30,"all 30 rendered knobs are interactive sliders");
    juce::TextButton* filterButton=nullptr;
    juce::ComboBox* presets=nullptr;
    int comboCount=0;
    for(auto* child:editor->getChildren())
    {
        if(auto* combo=dynamic_cast<juce::ComboBox*>(child)){presets=combo;++comboCount;}
        if(auto* button=dynamic_cast<juce::TextButton*>(child))
            if(button->getName()=="Filter mode")filterButton=button;
    }
    expect(comboCount==1,"only factory presets use a dropdown");
    expect(filterButton!=nullptr,"filter mode is a panel button");
    if(filterButton)
    {
        parameter(processor,"filterMode",1);pump();
        expect(filterButton->getToggleState() && filterButton->getButtonText()=="BP","host automation updates BP button");
        filterButton->triggerClick();pump();
        expect(processor.state.getRawParameterValue("filterMode")->load()==0,"mode button changes the existing DSP parameter");
        expect(filterButton->getButtonText()=="LP","mode button displays LP after click");
        for(int width:{960,1280,1600})
        {
            editor->setSize(width,width*5/8);pump();
            expect(filterButton->getY()>100,"filter button remains on the circuit panel after resizing");
            for(auto* child:editor->getChildren())if(dynamic_cast<juce::Slider*>(child))
                expect(!filterButton->getBounds().intersects(child->getBounds()),"mode button does not overlap a knob hit area");
        }
        editor->setSize(1280,800);
    }
    if(presets)
    {
        expect(presets->findColour(juce::ComboBox::backgroundColourId)==juce::Colour(0xff252119),"closed preset dropdown uses instrument palette");
        expect(presets->getLookAndFeel().findColour(juce::PopupMenu::backgroundColourId)==juce::Colour(0xff252119),"popup background uses instrument palette");
        juce::Image menu(juce::Image::RGB,300,100,true);
        {juce::Graphics graphics(menu);presets->getLookAndFeel().drawPopupMenuBackground(graphics,300,100);}
        expect(menu.getPixelAt(150,50)==juce::Colour(0xff252119),"popup renderer paints the specified background");
    }
    parameter(processor,"cutoff",6200);pump();
    for(auto* child:editor->getChildren())if(auto* slider=dynamic_cast<juce::Slider*>(child))
        if(slider->getName()=="Cutoff")
        {
            expect(std::abs(slider->getValue()-6200)<1,"host automation updates rendered slider");
            slider->setValue(1400,juce::sendNotificationSync);
            expect(std::abs(processor.state.getRawParameterValue("cutoff")->load()-1400)<1,"slider updates DSP parameter");
        }
    parameter(processor,"drive",11);parameter(processor,"pulse",.32f);parameter(processor,"width",.45f);parameter(processor,"filterMode",1);
    juce::MemoryBlock saved;processor.getStateInformation(saved);
    processor.setCurrentProgram(4);processor.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
    expect(std::abs(processor.state.getRawParameterValue("drive")->load()-11)<.01f,"new control state round-trips");
    pump();expect(filterButton && filterButton->getToggleState(),"restored filter state updates the panel button");
    // Simulate a v0.3 project with the newly added properties absent.
    auto legacy=processor.state.copyState();
    for(std::size_t i=12;i<longland::floatControls.size();++i)
        legacy.removeChild(legacy.getChildWithProperty("id",longland::floatControls[i].id),nullptr);
    juce::MemoryBlock old;juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(),old);
    processor.setStateInformation(old.getData(),static_cast<int>(old.getSize()));
    expect(processor.state.getRawParameterValue("drive")->load()==0,"legacy state restores neutral added controls");
    expect(processor.state.getRawParameterValue("expression")->load()==1,"legacy expression is not muted");
    processor.setCurrentProgram(0);parameter(processor,"noiseType",0);
    parameter(processor,"release",.02f);parameter(processor,"ensemble",0);
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,.85f),0);
    render(processor,100,midi);pump(120);
    expect(processor.keyboardState.isNoteOn(1,60),"incoming MIDI presses key");
    expect(processor.getNoteOnSerial(60)>0,"short-note animation latch advances");
    expect(processor.getOutputLevel()>.01f,"VU reads real post-master signal");
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);midi.addEvent(juce::MidiMessage::noteOff(1,60),1);
    render(processor,100,midi);pump(120);
    expect(!processor.keyboardState.isNoteOn(1,60),"key releases while sustain pedal is held");
    expect(processor.getOutputLevel()>.01f,"pedal sustains the sound separately");
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);render(processor,750,midi);pump(120);
    expect(processor.getOutputLevel()<.001f,"VU falls back after release");
    processor.keyboardState.noteOn(16,64,.8f);render(processor,80);pump(100);
    expect(processor.getOutputLevel()>.01f,"on-screen keyboard generates sound");
    processor.keyboardState.noteOff(16,64,0);processor.requestPanic();render(processor,10);pump(80);
    expect(!processor.keyboardState.isNoteOnForChannels(0xffff,64),"panic clears keys");
    if(argc>1)
    {
        juce::File out(juce::String::fromUTF8(argv[1]));out.createDirectory();
        processor.setCurrentProgram(0);pump(120);saveSnapshot(*editor,out,"VST-UI-Rest.png");
        parameter(processor,"filterMode",1);pump(50);saveSnapshot(*editor,out,"VST-UI-Bandpass.png");
        parameter(processor,"filterMode",0);pump(30);
        // Preview the exact popup drawing methods and colours used by the preset menu.
        juce::Image menu(juce::Image::RGB,310,194,true);
        {
            juce::Graphics menuGraphics(menu);
            auto& look=presets->getLookAndFeel();look.drawPopupMenuBackground(menuGraphics,310,194);
            for(int item=0;item<6;++item)
                look.drawPopupMenuItem(menuGraphics,{2,2+item*31,306,31},false,true,item==1,item==0,false,
                                      processor.getProgramName(item),{},nullptr,nullptr);
        }
        auto menuStream=out.getChildFile("Preset-Menu-Style.png").createOutputStream();
        if(menuStream){menuStream->setPosition(0);menuStream->truncate();juce::PNGImageFormat().writeImageToStream(menu,*menuStream);}
        parameter(processor,"cutoff",7200);parameter(processor,"age",.8f);parameter(processor,"drive",4);
        midi.clear();for(int n:{48,52,55,60,63,67})midi.addEvent(juce::MidiMessage::noteOn(1,n,.8f),0);
        render(processor,140,midi);pump(150);saveSnapshot(*editor,out,"VST-UI-Playing.png");
        editor->setSize(960,600);pump(40);saveSnapshot(*editor,out,"VST-UI-Small.png");
        editor->setSize(1600,1000);pump(40);saveSnapshot(*editor,out,"VST-UI-Large.png");
        if(argc>3)
        {
            editor->setSize(960,600);processor.requestPanic();render(processor,500);pump(150);
            for(int frame=0;frame<64;++frame)
            {
                const float value=.5f-.48f*std::cos(frame*juce::MathConstants<float>::twoPi/63.0f);
                for(auto* p:processor.getParameters())
                    if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(p))
                        if(ranged->paramID=="cutoff"||ranged->paramID=="drift"||ranged->paramID=="age"||ranged->paramID=="tone")
                            ranged->setValueNotifyingHost(value);
                midi.clear();
                if(frame<48 && frame%8==0)
                    midi.addEvent(juce::MidiMessage::noteOn(1,48+(frame/8)*3,.8f),0);
                if(frame>0 && frame%8==4)
                    midi.addEvent(juce::MidiMessage::noteOff(1,48+(frame/8)*3),0);
                render(processor,8,midi);pump(25);
                saveSnapshot(*editor,out,"animation-"+juce::String(frame).paddedLeft('0',3)+".png");
            }
        }
    }
    editor.reset();processor.releaseResources();
    if(argc>2)
    {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions,juce::String::fromUTF8(argv[2]));
        expect(descriptions.size()==1,"built VST3 scans as one instrument");
        if(descriptions.size()>0)
        {
            juce::AudioPluginFormatManager manager;manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
            juce::String error;auto plugin=manager.createPluginInstance(*descriptions[0],48000,256,error);
            expect(plugin!=nullptr,"built VST3 loads in an actual plug-in host");
            if(plugin)
            {
                plugin->prepareToPlay(48000,256);juce::AudioBuffer<float> block(2,256);juce::MidiBuffer events;
                events.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);float peak=0;
                for(int i=0;i<100;++i){plugin->processBlock(block,events);events.clear();peak=std::max(peak,block.getMagnitude(0,256));}
                expect(peak>.01f,"built VST3 produces audio from MIDI");
                std::unique_ptr<juce::AudioProcessorEditor> hostedEditor(plugin->createEditorIfNeeded());
                expect(hostedEditor!=nullptr,"built VST3 creates rendered editor");
                if(hostedEditor)expect(hostedEditor->getWidth()>0 && hostedEditor->getHeight()>0,"VST3 reports a valid editor size");
                hostedEditor.reset();plugin->releaseResources();
            }
            else std::cerr<<error<<'\n';
        }
    }
    if(!failures)std::cout<<"All Longland integration tests passed.\n";
    return failures?1:0;
}
