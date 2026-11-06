#include <JuceHeader.h>
#include "ChimeyGuitarEngine.h"
#include "ChimeyGuitar2Engine.h"
#include "ChimeyDeluxeEngine.h"
#include "Controls.h"
#include <atomic>

class GuitarProcessor final : public juce::AudioProcessor {
public:
    GuitarProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)), state(*this,nullptr,"GuitarSimState",layout()) {
        for(int m=0;m<3;++m) for(int i=0;i<10;++i) values[m][i]=state.getRawParameterValue(controls::id(m,i));
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
        return p;
    }
    int selectedModel() const { return juce::jlimit(0,2,juce::roundToInt(modelValue->load())); }
    void prepareToPlay(double rate,int blockSize) override {
        scratchSize=juce::jmax(8192,blockSize);
        floatScratch.setSize(4,scratchSize); doubleScratch.setSize(4,scratchSize);
        sampleRate=rate; guitar.reset(rate); guitar2.reset(rate); deluxe.reset(rate);
        activeModel=selectedModel(); blend.reset(rate,.01);
        blend.setCurrentAndTargetValue(originalValue->load()>.5f?0.:1.); peaks[0]=0; peaks[1]=0;
    }
    void releaseResources() override { peaks[0]=0; peaks[1]=0; }
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
    }
    template<class T> void process(juce::AudioBuffer<T>& b) {
        juce::ScopedNoDenormals guard;
        if(b.getNumChannels()<1) return;
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
            state.replaceState(restored);
        }
    }
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> uiSize{2};
    std::atomic<float> peaks[2]{};
private:
    ChimeyGuitarEngine guitar;
    ChimeyGuitar2Engine guitar2;
    ChimeyDeluxeEngine deluxe;
    std::array<std::array<std::atomic<float>*,10>,3> values{};
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
        for(auto& knob:knobs) {
            addAndMakeVisible(knob);
            knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            knob.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);
            knob.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,24);
        }
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
        for(auto* button:{&reset,&original}) {
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
        bindModel(processor.selectedModel());
        setSize(980,740); startTimerHz(30);
    }
    ~GuitarPanel() override { stopTimer(); setLookAndFeel(nullptr); }
    std::function<void()> sizeChanged;
    void resized() override {
        sizeChoice.setBounds(750,12,180,26); selector.setBounds(530,52,210,32); reset.setBounds(756,52,174,32);
        original.setBounds(726,648,204,32);
        for(int i=0;i<10;++i) knobs[static_cast<size_t>(i)].setBounds(44+(i%5)*180,292+(i/5)*175,172,140);
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
        // Woven speaker cloth, a small illuminated model badge, and stereo sample-peak meters.
        const juce::Rectangle<float> cloth{44,137,456,116};
        g.setColour(juce::Colour(0xff17201a)); g.fillRoundedRectangle(cloth,8);
        g.setColour(cream.withAlpha(.055f));
        for(int x=48;x<498;x+=5) g.drawVerticalLine(x,141,249);
        for(int y=141;y<250;y+=5) g.drawHorizontalLine(y,48,496);
        g.setColour(ink); g.fillRoundedRectangle(123,158,296,74,5);
        g.setColour(gold); g.drawRoundedRectangle({124.f,159.f,294.f,72.f},4,1);
        g.setFont(juce::FontOptions(25.f)); g.drawText(controls::models[shownModel],128,165,286,34,juce::Justification::centred);
        g.setFont(juce::FontOptions(10.f)); g.setColour(cream.withAlpha(.6f));
        g.drawText(shownModel==2?"THE DELUXE CIRCUIT":"THE CHIME CIRCUIT",128,202,286,18,juce::Justification::centred);
        for(int c=0;c<2;++c) drawMeter(g,544,151+c*48,level[c],c==0?"L":"R");
        g.setColour(muted); g.setFont(juce::FontOptions(10.f));
        g.drawText("OUTPUT  /  SAMPLE PEAK  /  dBFS",543,231,383,18,juce::Justification::right);
        g.setColour(cream.withAlpha(.12f)); g.drawHorizontalLine(270,48,932); g.drawHorizontalLine(445,138,842); g.drawHorizontalLine(632,48,932);
        g.setColour(gold); g.setFont(juce::FontOptions(11.f));
        for(int i=0;i<10;++i) g.drawText(juce::String(controls::name(shownModel,i)).toUpperCase(),44+(i%5)*180,276+(i/5)*175,172,22,juce::Justification::centred);
        g.setColour(cream.withAlpha(.6f)); g.setFont(juce::FontOptions(11.f));
        g.drawText("01 / DYNAMICS & TONE",48,652,380,20,juce::Justification::left);
        g.drawText("02 / BODY & SPEAKER",300,652,380,20,juce::Justification::left);
        g.setColour(ink.withAlpha(.65f));
        g.drawText("GuitarSim  /  "+juce::String(controls::models[shownModel])+" by Airwindows",32,708,650,20,juce::Justification::left);
        g.drawText("MONO + STEREO  /  FLOAT + DOUBLE",650,708,298,20,juce::Justification::right);
    }
private:
    void drawMeter(juce::Graphics& g,int x,int y,float db,const juce::String& label) {
        g.setColour(style::cream); g.setFont(juce::FontOptions(12.f)); g.drawText(label,x,y,20,20,juce::Justification::left);
        for(int i=0;i<40;++i) {
            const auto threshold=-60.f+static_cast<float>(i)*1.65f;
            const auto colour=threshold>=0?juce::Colour(0xffdf715b):threshold>=-9?style::gold:juce::Colour(0xffb8c99a);
            g.setColour(colour.withAlpha(db>=threshold?1.f:.12f)); g.fillRoundedRectangle(static_cast<float>(x+24+i*7),static_cast<float>(y+2),5,16,1);
        }
        g.setColour(db>=0?juce::Colour(0xffdf715b):style::cream);
        g.drawText(db<=-60?"−∞":juce::String(db,1),x+310,y,70,20,juce::Justification::right);
    }
    void bindModel(int model) {
        if(model==shownModel) return;
        for(auto& a:attachments) a.reset();
        shownModel=model;
        for(int i=0;i<10;++i) {
            auto& knob=knobs[static_cast<size_t>(i)];
            knob.setName(controls::name(model,i)); knob.setComponentID(controls::id(model,i));
            knob.setTooltip(juce::String(controls::models[model])+" / "+controls::name(model,i)+". Original normalized range 0–1. Double-click to reset.");
            attachments[static_cast<size_t>(i)]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state,controls::id(model,i),knob);
            knob.setDoubleClickReturnValue(true,controls::defaults[model][i]);
        }
        repaint();
    }
    void timerCallback() override {
        bindModel(processor.selectedModel());
        original.setButtonText(original.getToggleState()?"A: ORIGINAL INPUT":"B: GUITARSIM");
        if(sizeChoice.getSelectedId()!=processor.uiSize.load()) {
            sizeChoice.setSelectedId(processor.uiSize.load(),juce::dontSendNotification); if(sizeChanged) sizeChanged();
        }
        const auto now=juce::Time::getMillisecondCounterHiRes();
        const auto elapsed=lastTime>0?(now-lastTime)*.001:1./30.; lastTime=now;
        for(int c=0;c<2;++c) {
            const auto target=juce::Decibels::gainToDecibels(processor.peaks[c].exchange(0),-60.f);
            level[c]=juce::jmax(target,level[c]-static_cast<float>(elapsed*24.));
        }
        repaint(540,140,390,110);
    }
    GuitarProcessor& processor;
    style::Look look;
    std::array<juce::Slider,10> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> attachments;
    juce::ComboBox selector,sizeChoice;
    juce::TextButton reset{"RESET THIS MODEL"},original{"B: GUITARSIM"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> selectorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> originalAttachment;
    juce::TooltipWindow tooltip{this,600};
    int shownModel=-1;
    float level[2]{-60,-60};
    double lastTime=0;
};

class GuitarEditor final : public juce::AudioProcessorEditor {
public:
    explicit GuitarEditor(GuitarProcessor& p) : AudioProcessorEditor(p),processor(p),panel(p) {
        addAndMakeVisible(panel); setResizable(false,false); panel.sizeChanged=[this] { applySize(); }; applySize();
    }
private:
    void applySize() {
        const float scale=processor.uiSize.load()==1?.75f:processor.uiSize.load()==3?1.25f:1.f;
        panel.setTransform(juce::AffineTransform::scale(scale)); panel.setBounds(0,0,980,740);
        setSize(juce::roundToInt(980*scale),juce::roundToInt(740*scale));
    }
    GuitarProcessor& processor;
    GuitarPanel panel;
};
juce::AudioProcessorEditor* GuitarProcessor::createEditor() { return new GuitarEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GuitarProcessor; }
