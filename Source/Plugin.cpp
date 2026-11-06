#include <JuceHeader.h>
#include "ChimeyGuitarEngine.h"
#include "ChimeyGuitar2Engine.h"
#include "ChimeyDeluxeEngine.h"
#include "Controls.h"
#include "kWoodRoomEngine.h"
#include "WoodenBoxEngine.h"
#include <atomic>
#include "OutputMeter.h"
#include "MeterBallistics.h"

class GuitarProcessor final : public juce::AudioProcessor {
public:
    GuitarProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"GuitarSimState",layout()) {
        for(int m=0;m<3;++m) for(int i=0;i<10;++i) values[m][i]=state.getRawParameterValue(controls::id(m,i));
        for(int i=0;i<6;++i) roomValues[i]=state.getRawParameterValue(controls::roomId(i));
        for(int i=0;i<3;++i) boxValues[i]=state.getRawParameterValue(controls::boxId(i));
        roomModelValue=state.getRawParameterValue("roomModel");
        roomEnabledValue=state.getRawParameterValue("roomEnabled");
        guitarEnabledValue=state.getRawParameterValue("guitarEnabled");
        modelValue=state.getRawParameterValue("model"); originalValue=state.getRawParameterValue("original");
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        juce::AudioProcessorValueTreeState::ParameterLayout p;
        for(int m=0;m<3;++m) for(int i=0;i<10;++i) {
            auto attr=juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float v,int) { return juce::String(v,3); })
                .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); });
            p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{controls::id(m,i),1},
                juce::String(controls::models[m])+" "+controls::name(m,i),juce::NormalisableRange<float>(0,1),controls::defaults[m][i],attr));
        }
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"model",1},"Algorithm",
            juce::StringArray{"ChimeyGuitar","ChimeyGuitar2","ChimeyDeluxe"},1));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"original",1},"Original input",false));
        const auto roomAttributes=juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v,int) { return juce::String(v,3); })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); });
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"roomEnabled",1},"Room enabled",false));
        for(int i=0;i<6;++i) p.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{controls::roomId(i),1},"kWoodRoom "+juce::String(controls::roomNames[i]),
            juce::NormalisableRange<float>(0,1),controls::roomDefaults[i],roomAttributes));
        p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"roomModel",1},"Room algorithm",juce::StringArray{"kWoodRoom","WoodenBox"},0));
        for(int i=0;i<3;++i) p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{controls::boxId(i),1},
            "WoodenBox "+juce::String(controls::boxNames[i]),juce::NormalisableRange<float>(0,1),controls::boxDefaults[i],roomAttributes));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"guitarEnabled",1},"Guitar enabled",true));
        return p;
    }
    int selectedModel() const { return juce::jlimit(0,2,juce::roundToInt(modelValue->load())); }
    int selectedRoom() const { return roomModelValue->load()>.5f?1:0; }
    void prepareToPlay(double rate,int blockSize) override {
        meterTimeoutSeconds.store(juce::jmax(.15,2.0*blockSize/rate),std::memory_order_relaxed);
        meterSequence.fetch_add(1,std::memory_order_release);
        scratchSize=juce::jmax(8192,blockSize);
        floatScratch.setSize(6,scratchSize); doubleScratch.setSize(6,scratchSize);
        box.reset(rate); activeRoom=selectedRoom();
        guitarBlend.reset(rate,.01); guitarBlend.setCurrentAndTargetValue(guitarEnabledValue->load()>.5f?1.:0.);
        room.reset(rate); roomBlend.reset(rate,.01);
        roomBlend.setCurrentAndTargetValue(roomEnabledValue->load()>.5f?1.:0.);
        sampleRate=rate; guitar.reset(rate); guitar2.reset(rate); deluxe.reset(rate);
        activeModel=selectedModel(); blend.reset(rate,.01);
        blend.setCurrentAndTargetValue(originalValue->load()>.5f?0.:1.); peaks[0]=0; peaks[1]=0;
    }
    void releaseResources() override { peaks[0]=0; peaks[1]=0; meterSequence.fetch_add(1,std::memory_order_release); }
    bool isBusesLayoutSupported(const BusesLayout& b) const override {
        const auto channels=b.getMainInputChannelSet();
        return (channels==juce::AudioChannelSet::mono() || channels==juce::AudioChannelSet::stereo()) && channels==b.getMainOutputChannelSet();
    }
    template<class Engine,class T> void render(Engine& e,juce::AudioBuffer<T>& b,int model) {
        for(int i=0;i<10;++i) e.setParameter(i,values[model][i]->load());
        auto& scratch=[this]() -> juce::AudioBuffer<T>& {
            if constexpr(std::is_same_v<T,float>) return floatScratch;
            else return doubleScratch;
        }();
        auto* left=scratch.getWritePointer(0); auto* right=scratch.getWritePointer(1);
        auto* dryL=scratch.getWritePointer(2); auto* dryR=scratch.getWritePointer(3);
        auto* roomL=scratch.getWritePointer(4); auto* roomR=scratch.getWritePointer(5);
        for(int i=0;i<6;++i) room.setParameter(i,roomValues[i]->load());
        for(int i=0;i<3;++i) box.setParameter(i,boxValues[i]->load());
        guitarBlend.setTargetValue(guitarEnabledValue->load()>.5f?1.:0.);
        roomBlend.setTargetValue(roomEnabledValue->load()>.5f?1.:0.);
        blend.setTargetValue(originalValue->load()>.5f?0.:1.);
        for(int offset=0;offset<b.getNumSamples();offset+=scratchSize) {
            const int count=juce::jmin(scratchSize,b.getNumSamples()-offset);
            std::copy_n(b.getReadPointer(0)+offset,count,left);
            std::copy_n(b.getReadPointer(b.getNumChannels()==1?0:1)+offset,count,right);
            std::copy_n(left,count,dryL); std::copy_n(right,count,dryR);
            T* data[]{left,right};
            if constexpr(std::is_same_v<T,float>) e.processReplacing(data,data,count);
            else e.processDoubleReplacing(data,data,count);
            for(int i=0;i<count;++i) {
                const auto mix=guitarBlend.getNextValue();
                if(mix<=0.) { left[i]=dryL[i]; right[i]=dryR[i]; }
                else if(mix<1.) {
                    left[i]=static_cast<T>(dryL[i]*(1-mix)+left[i]*mix);
                    right[i]=static_cast<T>(dryR[i]*(1-mix)+right[i]*mix);
                }
            }
            // Independent post-guitar stage. Keep its history running during bypass;
            // the crossfade lives outside the unmodified Airwindows processing.
            std::copy_n(left,count,roomL); std::copy_n(right,count,roomR);
            T* roomData[]{roomL,roomR};
            if(activeRoom==0) {
                if constexpr(std::is_same_v<T,float>) room.processReplacing(roomData,roomData,count);
                else room.processDoubleReplacing(roomData,roomData,count);
            } else {
                if constexpr(std::is_same_v<T,float>) box.processReplacing(roomData,roomData,count);
                else box.processDoubleReplacing(roomData,roomData,count);
            }
            for(int i=0;i<count;++i) {
                const auto roomMix=roomBlend.getNextValue();
                if(roomMix>=1.) { left[i]=roomL[i]; right[i]=roomR[i]; }
                else if(roomMix>0.) {
                    left[i]=static_cast<T>(left[i]*(1-roomMix)+roomL[i]*roomMix);
                    right[i]=static_cast<T>(right[i]*(1-roomMix)+roomR[i]*roomMix);
                }
                const auto mix=blend.getNextValue();
                left[i]=static_cast<T>(dryL[i]*(1-mix)+left[i]*mix);
                right[i]=static_cast<T>(dryR[i]*(1-mix)+right[i]*mix);
            }
            std::copy_n(left,count,b.getWritePointer(0)+offset);
            if(b.getNumChannels()>1) std::copy_n(right,count,b.getWritePointer(1)+offset);
        }
    }
    template<class T> void meter(const juce::AudioBuffer<T>& b) {
        if(b.getNumChannels()==0) return;
        for(int c=0;c<2;++c) {
            const auto peak=static_cast<float>(b.getMagnitude(juce::jmin(c,b.getNumChannels()-1),0,b.getNumSamples()));
            auto old=peaks[c].load(std::memory_order_relaxed);
            while(old<peak && !peaks[c].compare_exchange_weak(old,peak,std::memory_order_relaxed)) {}
        }
        meterSequence.fetch_add(1,std::memory_order_release);
    }
    template<class T> void process(juce::AudioBuffer<T>& b) {
        juce::ScopedNoDenormals guard;
        if(b.getNumChannels()<1) return;
        if(selectedRoom()!=activeRoom) {
            activeRoom=selectedRoom();
            if(activeRoom==0) room.reset(sampleRate); else box.reset(sampleRate);
        }
        const int model=selectedModel();
        if(model!=activeModel) {
            if(model==0) guitar.reset(sampleRate); else if(model==1) guitar2.reset(sampleRate); else deluxe.reset(sampleRate);
            activeModel=model;
        }
        if(model==0) render(guitar,b,model); else if(model==1) render(guitar2,b,model); else render(deluxe,b,model);
        meter(b);
    }
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override { process(b); }
    void processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer&) override { process(b); }
    void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) override { meter(b); }
    void processBlockBypassed(juce::AudioBuffer<double>& b,juce::MidiBuffer&) override { meter(b); }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    const juce::String getName() const override { return "GuitarSim"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int,const juce::String&) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    void getStateInformation(juce::MemoryBlock& dst) override {
        auto saved=state.copyState(); saved.setProperty("uiSize",uiSize.load(),nullptr);
        if(auto xml=saved.createXml()) copyXmlToBinary(*xml,dst);
    }
    void setStateInformation(const void* data,int size) override {
        if(auto xml=getXmlFromBinary(data,size);xml && xml->hasTagName(state.state.getType())) {
            auto restored=juce::ValueTree::fromXml(*xml);
            if(!restored.isValid()) return;
            uiSize=juce::jlimit(1,3,static_cast<int>(restored.getProperty("uiSize",2)));
            // Sessions saved before the room stage retain the original guitar-only sound.
            for(int i=-1;i<6;++i) {
                const auto id=i<0?juce::String("roomEnabled"):controls::roomId(i);
                if(!restored.getChildWithProperty("id",id).isValid()) {
                    juce::ValueTree parameter{"PARAM"};
                    parameter.setProperty("id",id,nullptr);
                    parameter.setProperty("value",i<0?0.f:controls::roomDefaults[i],nullptr);
                    restored.appendChild(parameter,nullptr);
                }
            }
            for(int i=-2;i<3;++i) {
                const auto id=i==-2?juce::String("guitarEnabled"):i==-1?juce::String("roomModel"):controls::boxId(i);
                if(!restored.getChildWithProperty("id",id).isValid()) {
                    juce::ValueTree parameter{"PARAM"}; parameter.setProperty("id",id,nullptr);
                    parameter.setProperty("value",i==-2?1.f:i==-1?0.f:controls::boxDefaults[i],nullptr);
                    restored.appendChild(parameter,nullptr);
                }
            }
            state.replaceState(restored);
        }
    }
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> uiSize{2};
    std::atomic<float> peaks[2]{};
    std::atomic<uint32_t> meterSequence{0};
    std::atomic<double> meterTimeoutSeconds{.15};
private:
    WoodenBoxEngine box;
    int activeRoom=0;
    juce::SmoothedValue<double> guitarBlend{1.};
    kWoodRoomEngine room;
    juce::SmoothedValue<double> roomBlend{0.};
    ChimeyGuitarEngine guitar;
    ChimeyGuitar2Engine guitar2;
    ChimeyDeluxeEngine deluxe;
    std::array<std::array<std::atomic<float>*,10>,3> values{};
    std::array<std::atomic<float>*,6> roomValues{};
    std::array<std::atomic<float>*,3> boxValues{};
    std::atomic<float>* roomModelValue=nullptr;
    std::atomic<float>* roomEnabledValue=nullptr;
    std::atomic<float>* guitarEnabledValue=nullptr;
    std::atomic<float>* modelValue=nullptr;
    std::atomic<float>* originalValue=nullptr;
    juce::SmoothedValue<double> blend{1.};
    juce::AudioBuffer<float> floatScratch;
    juce::AudioBuffer<double> doubleScratch;
    int scratchSize=0;
    double sampleRate=44100.;
    int activeModel=1;
};

#include "Look.h"
class GuitarPanel final : public juce::Component, private juce::Timer {
public:
    explicit GuitarPanel(GuitarProcessor& p) : processor(p) {
        setLookAndFeel(&look);
        addAndMakeVisible(leftMeter); addAndMakeVisible(rightMeter);
        leftMeter.setComponentID("outputLeft"); rightMeter.setComponentID("outputRight");
        lastMeterSequence=processor.meterSequence.load(std::memory_order_acquire);
        for(size_t c=0;c<2;++c) {
            targets[c]=processor.peaks[c].exchange(0,std::memory_order_relaxed);
            ballistics[c].resetPeak(targets[c]);
        }
        leftMeter.setLevelDb(ballistics[0].getLevelDb()); rightMeter.setLevelDb(ballistics[1].getLevelDb());
        lastMeterTime=juce::Time::getMillisecondCounterHiRes();
        for(auto& knob:knobs) {
            addAndMakeVisible(knob);
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,24);
        }
        for(auto& knob:roomKnobs) {
            addAndMakeVisible(knob);
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,100,24);
        }
        roomSelector.setComponentID("roomAlgorithm"); roomSelector.setName("Room algorithm");
        roomSelector.addItemList({"kWoodRoom","WoodenBox"},1); addAndMakeVisible(roomSelector);
        roomSelectorAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"roomModel",roomSelector);
        roomSelector.onChange=[this] { bindRoom(); };
        guitarEnabled.setComponentID("guitarEnabled"); guitarEnabled.setClickingTogglesState(true);
        guitarEnabled.setTooltip("Engage the selected guitar algorithm before the room stage. Uses a 10 ms crossfade.");
        guitarAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"guitarEnabled",guitarEnabled);
        roomEnabled.setComponentID("roomEnabled"); roomEnabled.setClickingTogglesState(true);
        roomEnabled.setTooltip("Engage the selected room algorithm after the selected guitar algorithm. Uses a 10 ms crossfade.");
        roomAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"roomEnabled",roomEnabled);
        selector.setName("Algorithm"); selector.setComponentID("algorithm");
        selector.addItemList({"ChimeyGuitar","ChimeyGuitar2","ChimeyDeluxe"},1);
        selector.setTooltip("Each Airwindows algorithm keeps its own settings. Switching resets the entering algorithm's audio history.");
        sizeChoice.addItemList({"Small (75%)","Medium (100%)","Large (125%)"},1);
        sizeChoice.setName("Interface size"); sizeChoice.setComponentID("uiSize");
        sizeChoice.setSelectedId(processor.uiSize.load(),juce::dontSendNotification);
        for(auto* box:{&selector,&sizeChoice}) {
            addAndMakeVisible(*box);
            box->setColour(juce::ComboBox::backgroundColourId,style::ink);
            box->setColour(juce::ComboBox::textColourId,style::gold);
            box->setColour(juce::ComboBox::outlineColourId,style::gold.withAlpha(.4f));
        }
        selectorAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state,"model",selector);
        selector.onChange=[this] { bindModel(processor.selectedModel()); };
        sizeChoice.onChange=[this] {
            processor.uiSize=sizeChoice.getSelectedId();
            processor.updateHostDisplay(juce::AudioProcessor::ChangeDetails{}.withNonParameterStateChanged(true));
            if(sizeChanged) sizeChanged();
        };
        for(auto* button:{&reset,&original,&roomEnabled,&guitarEnabled}) {
            addAndMakeVisible(*button);
            button->setColour(juce::TextButton::buttonColourId,style::ink);
            button->setColour(juce::TextButton::textColourOffId,style::gold);
            button->setColour(juce::TextButton::buttonOnColourId,style::gold);
            button->setColour(juce::TextButton::textColourOnId,style::ink);
        }
        reset.setComponentID("resetParameters"); reset.setTooltip("Reset this algorithm's ten controls to the original Airwindows defaults.");
        reset.onClick=[this] {
            const auto model=processor.selectedModel();
            for(int i=0;i<10;++i) {
                auto* parameter=processor.state.getParameter(controls::id(model,i));
                parameter->beginChangeGesture(); parameter->setValueNotifyingHost(controls::defaults[model][i]); parameter->endChangeGesture();
            }
        };
        original.setComponentID("abOriginal"); original.setClickingTogglesState(true);
        original.setTooltip("Compare the unprocessed input with GuitarSim. Uses a 10 ms crossfade.");
        originalAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state,"original",original);
        guitarEnabled.onClick=[this] { refreshStages(); };
        roomEnabled.onClick=[this] { refreshStages(); };
        bindModel(processor.selectedModel()); bindRoom(); refreshStages();
        setSize(1340,740); startTimerHz(60);
    }
    ~GuitarPanel() override { stopTimer(); setLookAndFeel(nullptr); }
    std::function<void()> sizeChanged;
    void resized() override {
        sizeChoice.setBounds(750,12,180,26); selector.setBounds(530,52,210,32); reset.setBounds(756,52,174,32);
        guitarEnabled.setBounds(423,251,132,28);
        leftMeter.setBounds(44,126,365,182); rightMeter.setBounds(567,126,365,182);
        roomSelector.setBounds(1006,171,284,28);
        roomEnabled.setBounds(1006,210,284,32);
        for(int i=0;i<6;++i) roomKnobs[i].setBounds(988+(i%2)*160,294+(i/2)*116,150,108);
        original.setBounds(726,648,204,32);
        for(int i=0;i<10;++i) knobs[static_cast<size_t>(i)].setBounds(44+(i%5)*180,336+(i/5)*155,172,130);
    }
    void paint(juce::Graphics& g) override {
        using namespace style;
        g.fillAll(cream); g.setColour(ink); g.setFont(juce::FontOptions(11.f));
        g.drawText("YODO AUDIO    /    AIRWINDOWS DSP",32,14,600,20,juce::Justification::left);
        g.setFont(juce::FontOptions(40.f)); g.drawText("GuitarSim",30,40,300,55,juce::Justification::left);
        g.setFont(juce::FontOptions(12.f)); g.drawText("C H I M E   /   S T U D I O",305,65,220,20,juce::Justification::left);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff343d36),0,108,juce::Colour(0xff19221d),0,700,false));
        g.fillRoundedRectangle(16,108,948,592,16);
        g.setColour(gold.withAlpha(.25f)); g.drawRoundedRectangle({17.f,109.f,946.f,590.f},15,1);
        for(float x:{30.f,950.f}) for(float y:{122.f,686.f}) {
            g.setColour(juce::Colour(0xff111a15)); g.fillEllipse(x-4,y-4,8,8);
            g.setColour(muted); g.drawLine(x-2,y+1,x+2,y-1,1);
        }
        // A separate warm wood-coloured chassis identifies the post-guitar stage.
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff503e30),980,108,juce::Colour(0xff2e2923),1324,700,false));
        g.fillRoundedRectangle(980,108,344,592,16);
        g.setColour(gold.withAlpha(.4f)); g.drawRoundedRectangle({981.f,109.f,342.f,590.f},15,1);
        g.setColour(gold); g.setFont(juce::FontOptions(25.f));
        g.drawText("ROOM",1000,125,304,40,juce::Justification::centred);
        g.setColour(cream.withAlpha(.65f)); g.setFont(juce::FontOptions(10.f));
        g.drawText("03 / AFTER GUITARSIM",1000,247,304,18,juce::Justification::centred);
        g.drawText("Airwindows / Chris Johnson",1000,653,304,20,juce::Justification::centred);
        for(int i=0;i<(shownRoom==0?6:3);++i) {
            g.setColour(gold); g.setFont(juce::FontOptions(11.f));
            g.drawText(juce::String(shownRoom==0?controls::roomNames[i]:controls::boxNames[i]).toUpperCase(),988+(i%2)*160,278+(i/2)*116,150,22,juce::Justification::centred);
        }
        // Tape-style stereo needles flank the speaker-cloth model badge.
        const juce::Rectangle<float> cloth{421,137,136,102};
        g.setColour(juce::Colour(0xff17201a)); g.fillRoundedRectangle(cloth,8);
        g.setColour(cream.withAlpha(.055f));
        for(int x=425;x<555;x+=5) g.drawVerticalLine(x,141,235);
        for(int y=141;y<236;y+=5) g.drawHorizontalLine(y,425,553);
        g.setColour(ink); g.fillRoundedRectangle(427,155,124,66,5);
        g.setColour(gold); g.drawRoundedRectangle({428.f,156.f,122.f,64.f},4,1);
        g.setFont(juce::FontOptions(14.f)); g.drawText(controls::models[shownModel],430,164,118,30,juce::Justification::centred);
        g.setFont(juce::FontOptions(8.f)); g.setColour(cream.withAlpha(.6f));
        g.drawText(shownModel==2?"DELUXE CIRCUIT":"CHIME CIRCUIT",430,199,118,16,juce::Justification::centred);
        g.setColour(cream.withAlpha(.12f)); g.drawHorizontalLine(314,48,932); g.drawHorizontalLine(470,138,842); g.drawHorizontalLine(632,48,932);
        g.setColour(gold); g.setFont(juce::FontOptions(11.f));
        for(int i=0;i<10;++i) g.drawText(juce::String(controls::name(shownModel,i)).toUpperCase(),44+(i%5)*180,320+(i/5)*155,172,22,juce::Justification::centred);
        g.setColour(cream.withAlpha(.6f)); g.setFont(juce::FontOptions(11.f));
        g.drawText("01 / DYNAMICS & TONE",48,652,380,20,juce::Justification::left);
        g.drawText("02 / BODY & SPEAKER",300,652,380,20,juce::Justification::left);
        g.setColour(ink.withAlpha(.65f));
        g.drawText("GuitarSim  /  "+juce::String(controls::models[shownModel])+" by Airwindows",32,708,650,20,juce::Justification::left);
        g.drawText("MONO + STEREO  /  FLOAT + DOUBLE",650,708,298,20,juce::Justification::right);
    }
private:
    void bindRoom() {
        const int model=processor.selectedRoom(); if(model==shownRoom) return;
        for(auto& a:roomAttachments) a.reset(); shownRoom=model;
        for(int i=0;i<6;++i) {
            auto& knob=roomKnobs[i]; const bool visible=i<(model==0?6:3); knob.setVisible(visible);
            if(!visible) continue;
            const auto id=model==0?controls::roomId(i):controls::boxId(i);
            const auto name=juce::String(model==0?controls::roomNames[i]:controls::boxNames[i]);
            knob.setName(name); knob.setComponentID(id);
            knob.setTooltip(name+". Original Airwindows range 0-1. Double-click to reset.");
            roomAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,id,knob);
            knob.setDoubleClickReturnValue(true,model==0?controls::roomDefaults[i]:controls::boxDefaults[i]);
        }
        repaint();
    }
    void refreshStages() {
        const bool guitarOn=processor.state.getRawParameterValue("guitarEnabled")->load()>.5f;
        const bool roomOn=processor.state.getRawParameterValue("roomEnabled")->load()>.5f;
        for(auto& knob:knobs) { knob.setEnabled(guitarOn); knob.setAlpha(guitarOn?1.f:.35f); }
        for(auto& knob:roomKnobs) { knob.setEnabled(roomOn); knob.setAlpha(roomOn?1.f:.35f); }
        selector.setEnabled(guitarOn); selector.setAlpha(guitarOn?1.f:.4f); reset.setEnabled(guitarOn);
        roomSelector.setEnabled(roomOn); roomSelector.setAlpha(roomOn?1.f:.4f);
        guitarEnabled.setButtonText(guitarOn?"GUITAR ON":"GUITAR BYPASSED");
        roomEnabled.setButtonText(roomOn?"ROOM ON":"ROOM BYPASSED");
    }
    void bindModel(int model) {
        if(model==shownModel) return;
        for(auto& a:attachments) a.reset();
        shownModel=model;
        for(int i=0;i<10;++i) {
            auto& knob=knobs[static_cast<size_t>(i)];
            knob.setName(controls::name(model,i)); knob.setComponentID(controls::id(model,i));
            knob.setTooltip(juce::String(controls::models[model])+" / "+controls::name(model,i)+". Original normalized range 0-1. Double-click to reset.");
            attachments[static_cast<size_t>(i)]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,controls::id(model,i),knob);
            knob.setDoubleClickReturnValue(true,controls::defaults[model][i]);
        }
        repaint();
    }
    void timerCallback() override {
        bindModel(processor.selectedModel());
        bindRoom(); refreshStages();
        original.setButtonText(original.getToggleState()?"A: ORIGINAL INPUT":"B: GUITARSIM");
        if(sizeChoice.getSelectedId()!=processor.uiSize.load()) {
            sizeChoice.setSelectedId(processor.uiSize.load(),juce::dontSendNotification); if(sizeChanged) sizeChanged();
        }
        const auto now=juce::Time::getMillisecondCounterHiRes();
        const auto elapsed=(now-lastMeterTime)*.001;
        lastMeterTime=now;
        const auto sequence=processor.meterSequence.load(std::memory_order_acquire);
        if(sequence!=lastMeterSequence) {
            lastMeterSequence=sequence; idleSeconds=0;
            for(size_t c=0;c<2;++c) targets[c]=processor.peaks[c].exchange(0,std::memory_order_relaxed);
        } else {
            idleSeconds+=elapsed;
            if(idleSeconds>processor.meterTimeoutSeconds.load(std::memory_order_relaxed)) targets.fill(0);
        }
        for(size_t c=0;c<2;++c) ballistics[c].advance(targets[c],elapsed);
        leftMeter.setLevelDb(ballistics[0].getLevelDb()); rightMeter.setLevelDb(ballistics[1].getLevelDb());
    }
    GuitarProcessor& processor;
    style::Look look;
    juce::TextButton roomEnabled{"ROOM BYPASSED"},guitarEnabled{"GUITAR ON"};
    juce::ComboBox roomSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roomSelectorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> guitarAttachment;
    int shownRoom=-1;
    std::array<juce::Slider,6> roomKnobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,6> roomAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> roomAttachment;
    std::array<juce::Slider,10> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> attachments;
    juce::ComboBox selector,sizeChoice;
    juce::TextButton reset{"RESET THIS MODEL"},original{"B: GUITARSIM"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> selectorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> originalAttachment;
    juce::TooltipWindow tooltip{this,600};
    int shownModel=-1;
    OutputMeter leftMeter{"LEFT"},rightMeter{"RIGHT"};
    std::array<MeterBallistics,2> ballistics;
    std::array<float,2> targets{};
    uint32_t lastMeterSequence=0;
    double lastMeterTime=0,idleSeconds=0;
};

class GuitarEditor final : public juce::AudioProcessorEditor {
public:
    explicit GuitarEditor(GuitarProcessor& p) : AudioProcessorEditor(p),processor(p),panel(p) {
        addAndMakeVisible(panel); setResizable(false,false); panel.sizeChanged=[this] { applySize(); }; applySize();
    }
private:
    void applySize() {
        const float scale=processor.uiSize.load()==1?.75f:processor.uiSize.load()==3?1.25f:1.f;
        panel.setTransform(juce::AffineTransform::scale(scale)); panel.setBounds(0,0,1340,740);
        setSize(juce::roundToInt(1340*scale),juce::roundToInt(740*scale));
    }
    GuitarProcessor& processor;
    GuitarPanel panel;
};
juce::AudioProcessorEditor* GuitarProcessor::createEditor() { return new GuitarEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GuitarProcessor; }
