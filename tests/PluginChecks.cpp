#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>
#include "../Source/ChimeyGuitarEngine.h"
#include "../Source/ChimeyGuitar2Engine.h"
#include "../Source/ChimeyDeluxeEngine.h"
#include "../Source/Controls.h"
#include "../Source/kWoodRoomEngine.h"
#include "../Source/WoodenBoxEngine.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
static void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
static juce::RangedAudioParameter& param(juce::AudioProcessor& p,const juce::String& id) {
    for(auto* v:p.getParameters()) if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(v);r && r->getParameterID()==id) return *r;
    throw std::runtime_error("Missing parameter: "+id.toStdString());
}
static void set(juce::AudioProcessor& p,const juce::String& id,float normalized) { param(p,id).setValueNotifyingHost(normalized); }
static juce::Component* find(juce::Component& c,const juce::String& id) {
    if(c.getComponentID()==id) return &c;
    for(auto* child:c.getChildren()) if(auto* result=find(*child,id)) return result;
    return nullptr;
}
template<class Engine, class T> void compare(juce::AudioProcessor& p,int model,const juce::String& name,bool withRoom=false,int roomModel=0,bool guitarOn=true) {
    set(p,"guitarEnabled",guitarOn?1.f:0.f); set(p,"roomModel",static_cast<float>(roomModel));
    set(p,"roomEnabled",withRoom?1.f:0.f);
    set(p,"model",static_cast<float>(model)*.5f); set(p,"original",0.f);
    p.setProcessingPrecision(std::is_same_v<T,float>?juce::AudioProcessor::singlePrecision:juce::AudioProcessor::doublePrecision);
    p.setPlayConfigDetails(2,2,48000,257); p.prepareToPlay(48000,257);
    Engine e; e.reset(48000);
    WoodenBoxEngine box; box.reset(48000);
    kWoodRoomEngine room; room.reset(48000);
    for(int i=0;i<6;++i) { set(p,controls::roomId(i),controls::roomDefaults[i]); room.setParameter(i,controls::roomDefaults[i]); }
    for(int i=0;i<10;++i) { const float v=static_cast<float>((i*3+4)%11)*.1f; set(p,name+"_"+juce::String(i),v); e.setParameter(i,v); }
    juce::MidiBuffer midi;
    for(int block=0;block<8;++block) {
        juce::AudioBuffer<T> b(2,257),expected(2,257);
        for(int c=0;c<2;++c) for(int i=0;i<257;++i) {
            const auto v=static_cast<T>(.3*std::sin((block*257+i)*.031*(c+1)));
            b.setSample(c,i,v); expected.setSample(c,i,v);
        }
        p.processBlock(b,midi);
        T* data[]{expected.getWritePointer(0),expected.getWritePointer(1)};
        if(guitarOn) {
        if constexpr(std::is_same_v<T,float>) e.processReplacing(data,data,257);
        else e.processDoubleReplacing(data,data,257);
        }
        if(withRoom && roomModel==1) {
            if constexpr(std::is_same_v<T,float>) box.processReplacing(data,data,257);
            else box.processDoubleReplacing(data,data,257);
        } else if(withRoom) {
            if constexpr(std::is_same_v<T,float>) room.processReplacing(data,data,257);
            else room.processDoubleReplacing(data,data,257);
        }
        for(int c=0;c<2;++c) for(int i=0;i<257;++i) require(b.getSample(c,i)==expected.getSample(c,i),"Wrapper altered upstream output");
    }
    require(p.getLatencySamples()==0,"Unexpected latency"); p.releaseResources();
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
        require(p->getName()=="GuitarSim","Plugin name");
        require(p->getParameters().size()==44,"Unexpected parameters (no level matching)");
        require(param(*p,"guitarEnabled").getValue()==1.f,"Guitar default bypassed");
        require(param(*p,"roomModel").getValue()==0.f,"kWoodRoom not default");
        require(param(*p,"roomEnabled").getValue()==0.f,"Room default engaged");
        require(param(*p,"model").getValue()==.5f,"ChimeyGuitar2 is not default");
        require(std::abs(param(*p,"ChimeyGuitar2_0").getValue()-.6f)<1.e-6f,"Upstream default");
        compare<ChimeyGuitarEngine,float>(*p,0,"ChimeyGuitar");
        compare<ChimeyGuitar2Engine,float>(*p,1,"ChimeyGuitar2");
        compare<ChimeyDeluxeEngine,float>(*p,2,"ChimeyDeluxe");
        compare<ChimeyGuitarEngine,double>(*p,0,"ChimeyGuitar");
        compare<ChimeyGuitar2Engine,double>(*p,1,"ChimeyGuitar2");
        compare<ChimeyDeluxeEngine,double>(*p,2,"ChimeyDeluxe");
        compare<ChimeyGuitarEngine,float>(*p,0,"ChimeyGuitar",true);
        compare<ChimeyGuitar2Engine,float>(*p,1,"ChimeyGuitar2",true);
        compare<ChimeyDeluxeEngine,float>(*p,2,"ChimeyDeluxe",true);
        compare<ChimeyGuitarEngine,double>(*p,0,"ChimeyGuitar",true);
        compare<ChimeyGuitar2Engine,double>(*p,1,"ChimeyGuitar2",true);
        compare<ChimeyDeluxeEngine,double>(*p,2,"ChimeyDeluxe",true);
        compare<ChimeyGuitarEngine,float>(*p,0,"ChimeyGuitar",true,1);
        compare<ChimeyGuitar2Engine,double>(*p,1,"ChimeyGuitar2",true,1);
        compare<ChimeyDeluxeEngine,double>(*p,2,"ChimeyDeluxe",true,1);
        compare<ChimeyGuitar2Engine,float>(*p,1,"ChimeyGuitar2",true,0,false);
        compare<ChimeyGuitar2Engine,double>(*p,1,"ChimeyGuitar2",true,1,false);
        compare<ChimeyGuitar2Engine,float>(*p,1,"ChimeyGuitar2",false,0,false);
        set(*p,"guitarEnabled",1.f); set(*p,"roomEnabled",1.f); set(*p,"roomModel",0.f);
        p->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        p->setPlayConfigDetails(2,2,48000,512); p->prepareToPlay(48000,512);
        juce::MidiBuffer midi;
        for(float model:{0.f,.5f,1.f}) {
            set(*p,"model",model);
            juce::AudioBuffer<float> b(2,512);
            for(int c=0;c<2;++c) for(int i=0;i<512;++i) b.setSample(c,i,.1f*std::sin(static_cast<float>(i)*.071f));
            p->processBlock(b,midi);
            for(int c=0;c<2;++c) for(int i=0;i<512;++i) require(std::isfinite(b.getSample(c,i)),"Float output nonfinite");
            juce::AudioBuffer<double> d(2,512); d.clear(); p->processBlock(d,midi);
            for(int i=0;i<512;++i) require(std::isfinite(d.getSample(0,i)),"Double output nonfinite");
        }
        set(*p,"ChimeyGuitar_0",.123f); set(*p,"ChimeyGuitar2_0",.789f); set(*p,"ChimeyDeluxe_0",.456f);
        set(*p,"model",.5f); set(*p,"kWoodRoom_0",.321f);
        juce::MemoryBlock state; p->getStateInformation(state);
        std::unique_ptr<juce::AudioProcessor> restored(createPluginFilter()); restored->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(std::abs(param(*restored,"ChimeyGuitar_0").getValue()-.123f)<1.e-6f,"Original bank lost");
        require(std::abs(param(*restored,"ChimeyGuitar2_0").getValue()-.789f)<1.e-6f,"Guitar2 bank lost");
        require(std::abs(param(*restored,"ChimeyDeluxe_0").getValue()-.456f)<1.e-6f,"Deluxe bank lost");
        require(param(*restored,"model").getValue()==.5f,"Selected model lost");
        require(param(*restored,"roomEnabled").getValue()==1.f,"Room enable lost");
        require(std::abs(param(*restored,"kWoodRoom_0").getValue()-.321f)<1.e-6f,"Room controls lost");
        auto legacyXml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
        for(int i=legacyXml->getNumChildElements()-1;i>=0;--i) {
            auto* child=legacyXml->getChildElement(i);
            const auto id=child->getStringAttribute("id");
            if(id=="roomEnabled" || id=="roomModel" || id=="guitarEnabled" || id.startsWith("kWoodRoom_") || id.startsWith("WoodenBox_")) legacyXml->removeChildElement(child,true);
        }
        juce::MemoryBlock legacy; juce::AudioProcessor::copyXmlToBinary(*legacyXml,legacy);
        restored->setStateInformation(legacy.getData(),static_cast<int>(legacy.getSize()));
        require(param(*restored,"roomEnabled").getValue()==0.f,"Legacy session enabled room");
        require(param(*restored,"guitarEnabled").getValue()==1.f,"Legacy guitar bypassed");
        require(param(*restored,"roomModel").getValue()==0.f,"Legacy room choice");
        require(param(*restored,"kWoodRoom_0").getValue()==.5f,"Legacy room defaults");
        restored->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        set(*p,"original",1.f);
        juce::AudioBuffer<float> dry(2,512);
        for(int block=0;block<3;++block) {
            for(int c=0;c<2;++c) for(int i=0;i<512;++i) dry.setSample(c,i,.123f);
            p->processBlock(dry,midi);
        }
        require(dry.getSample(0,511)==.123f && dry.getSample(1,511)==.123f,"Original not dry");
        p->releaseResources(); p->setPlayConfigDetails(1,1,44100,257); p->prepareToPlay(44100,257);
        juce::AudioBuffer<float> mono(1,257); mono.clear(); p->processBlock(mono,midi);
        require(std::isfinite(mono.getSample(0,256)),"Mono processing");
        p->releaseResources();
        // Exercise attachment rebinding, model-specific reset, UI scaling and render the actual editor.
        std::unique_ptr<juce::AudioProcessorEditor> editor(restored->createEditor());
        require(editor->getWidth()==1340 && editor->getHeight()==740,"Default editor size");
        auto* roomSwitch=dynamic_cast<juce::Button*>(find(*editor,"roomEnabled")); require(roomSwitch,"Room switch");
        require(roomSwitch->getToggleState(),"Room switch attachment");
        auto* roomKnob=dynamic_cast<juce::Slider*>(find(*editor,"kWoodRoom_0")); require(roomKnob,"Room control");
        roomKnob->setValue(.432,juce::sendNotificationSync);
        require(std::abs(param(*restored,"kWoodRoom_0").getValue()-.432f)<1.e-6f,"Room knob attachment");
        auto* selector=dynamic_cast<juce::ComboBox*>(find(*editor,"algorithm")); require(selector,"Model selector");
        const juce::StringArray names{"ChimeyGuitar","ChimeyGuitar2","ChimeyDeluxe"};
        juce::File output(argc>1?juce::String(argv[1]):juce::File::getCurrentWorkingDirectory().getFullPathName()); output.createDirectory();
        for(int model=0;model<3;++model) {
            selector->setSelectedId(model+1,juce::sendNotificationSync);
            require(find(*editor,names[model]+"_0"),"Control bank not rebound");
            if(argc>1) {
                std::array<float,10> saved{};
                for(int i=0;i<10;++i) { saved[static_cast<size_t>(i)]=param(*restored,controls::id(model,i)).getValue(); set(*restored,controls::id(model,i),controls::defaults[static_cast<size_t>(model)][static_cast<size_t>(i)]); }
                auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true);
                auto stream=output.getChildFile("ui-"+names[model]+".png").createOutputStream(); require(stream!=nullptr,"Screenshot output");
                stream->setPosition(0); stream->truncate();
                juce::PNGImageFormat png; require(png.writeImageToStream(image,*stream),"Screenshot encoding");
                for(int i=0;i<10;++i) set(*restored,controls::id(model,i),saved[static_cast<size_t>(i)]);
            }
        }
        auto* reset=dynamic_cast<juce::Button*>(find(*editor,"resetParameters")); require(reset,"Reset button");
        if(reset->onClick) reset->onClick();
        require(std::abs(param(*restored,"kWoodRoom_0").getValue()-.432f)<1.e-6f,"Model reset changed room");
        require(std::abs(param(*restored,"ChimeyDeluxe_0").getValue()-.5f)<1.e-6f,"Reset selected bank");
        require(std::abs(param(*restored,"ChimeyGuitar2_0").getValue()-.789f)<1.e-6f,"Reset changed another bank");
        auto* roomChoice=dynamic_cast<juce::ComboBox*>(find(*editor,"roomAlgorithm")); require(roomChoice,"Room selector");
        roomChoice->setSelectedId(2,juce::sendNotificationSync);
        auto* boxKnob=dynamic_cast<juce::Slider*>(find(*editor,"WoodenBox_0")); require(boxKnob,"WoodenBox controls not rebound");
        boxKnob->setValue(.654,juce::sendNotificationSync);
        auto snapshot=[&](const juce::String& name) {
            if(argc<=1) return;
            auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true);
            auto stream=output.getChildFile(name+".png").createOutputStream(); require(stream!=nullptr,"Stage screenshot output");
            stream->setPosition(0); stream->truncate();
            require(juce::PNGImageFormat().writeImageToStream(image,*stream),"Stage screenshot encoding");
        };
        snapshot("ui-WoodenBox");
        roomChoice->setSelectedId(1,juce::sendNotificationSync);
        require(std::abs(param(*restored,"kWoodRoom_0").getValue()-.432f)<1.e-6f,"Room bank lost on switch");
        require(std::abs(param(*restored,"WoodenBox_0").getValue()-.654f)<1.e-6f,"Box bank lost on switch");
        auto* guitarSwitch=dynamic_cast<juce::Button*>(find(*editor,"guitarEnabled")); require(guitarSwitch,"Guitar switch");
        guitarSwitch->setToggleState(false,juce::sendNotificationSync);
        roomSwitch->setToggleState(false,juce::sendNotificationSync);
        require(!find(*editor,"ChimeyDeluxe_0")->isEnabled(),"Bypassed guitar controls active");
        require(!find(*editor,"kWoodRoom_0")->isEnabled(),"Bypassed room controls active");
        require(find(*editor,"kWoodRoom_0")->getAlpha()<.5f,"Bypassed room not dimmed");
        require(!selector->isEnabled() && !roomChoice->isEnabled(),"Bypassed selectors active");
        snapshot("ui-bypassed");
        guitarSwitch->setToggleState(true,juce::sendNotificationSync);
        roomSwitch->setToggleState(true,juce::sendNotificationSync);
        require(find(*editor,"ChimeyDeluxe_0")->isEnabled() && find(*editor,"kWoodRoom_0")->isEnabled(),"Engaged controls disabled");
        auto* size=dynamic_cast<juce::ComboBox*>(find(*editor,"uiSize")); require(size,"UI size selector");
        size->setSelectedId(1,juce::sendNotificationSync); require(editor->getWidth()==1005,"Small UI scaling");
        size->setSelectedId(3,juce::sendNotificationSync); require(editor->getWidth()==1675,"Large UI scaling");
        std::cout<<"GuitarSim: DSP wrapper, banks, state, original, mono, attachments, reset, scaling and rendering passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
