#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>
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
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
        require(p->getName()=="GuitarSim","Plugin name");
        require(p->getParameters().size()==32,"Unexpected parameters (no level matching)");
        require(param(*p,"model").getValue()==.5f,"ChimeyGuitar2 is not default");
        require(std::abs(param(*p,"ChimeyGuitar2_0").getValue()-.6f)<1.e-6f,"Upstream default");
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
        set(*p,"model",.5f);
        juce::MemoryBlock state; p->getStateInformation(state);
        std::unique_ptr<juce::AudioProcessor> restored(createPluginFilter()); restored->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        require(std::abs(param(*restored,"ChimeyGuitar_0").getValue()-.123f)<1.e-6f,"Original bank lost");
        require(std::abs(param(*restored,"ChimeyGuitar2_0").getValue()-.789f)<1.e-6f,"Guitar2 bank lost");
        require(std::abs(param(*restored,"ChimeyDeluxe_0").getValue()-.456f)<1.e-6f,"Deluxe bank lost");
        require(param(*restored,"model").getValue()==.5f,"Selected model lost");
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
        require(editor->getWidth()==980 && editor->getHeight()==740,"Default editor size");
        auto* selector=dynamic_cast<juce::ComboBox*>(find(*editor,"algorithm")); require(selector,"Model selector");
        const juce::StringArray names{"ChimeyGuitar","ChimeyGuitar2","ChimeyDeluxe"};
        juce::File output(argc>1?juce::String(argv[1]):juce::File::getCurrentWorkingDirectory().getFullPathName()); output.createDirectory();
        for(int model=0;model<3;++model) {
            selector->setSelectedId(model+1,juce::sendNotificationSync);
            require(find(*editor,names[model]+"_0"),"Control bank not rebound");
            if(argc>1) {
                auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true);
                auto stream=output.getChildFile("ui-"+names[model]+".png").createOutputStream(); require(stream!=nullptr,"Screenshot output");
                juce::PNGImageFormat png; require(png.writeImageToStream(image,*stream),"Screenshot encoding");
            }
        }
        auto* reset=dynamic_cast<juce::Button*>(find(*editor,"resetParameters")); require(reset,"Reset button");
        if(reset->onClick) reset->onClick();
        require(std::abs(param(*restored,"ChimeyDeluxe_0").getValue()-.5f)<1.e-6f,"Reset selected bank");
        require(std::abs(param(*restored,"ChimeyGuitar2_0").getValue()-.789f)<1.e-6f,"Reset changed another bank");
        auto* size=dynamic_cast<juce::ComboBox*>(find(*editor,"uiSize")); require(size,"UI size selector");
        size->setSelectedId(1,juce::sendNotificationSync); require(editor->getWidth()==735,"Small UI scaling");
        size->setSelectedId(3,juce::sendNotificationSync); require(editor->getWidth()==1225,"Large UI scaling");
        std::cout<<"GuitarSim: DSP wrapper, banks, state, original, mono, attachments, reset, scaling and rendering passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
