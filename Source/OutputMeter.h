#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Read-only output display. It never feeds back into audio processing.
class OutputMeter final : public juce::Component {
public:
    explicit OutputMeter(juce::String channelName) : channel(std::move(channelName)) {
        setTitle("Output level " + channel);
        setDescription("Final output sample peak in dBFS, with a damped needle; not a VU or gain reduction meter");
        setAccessible(true);
    }
    void setPeak(float linearPeak) {
        setLevelDb(juce::Decibels::gainToDecibels(juce::jmax(0.f,linearPeak),-60.f));
    }
    void setLevelDb(float levelDb) {
        db=juce::jmax(-60.f,levelDb);
        repaint();
    }
    float getLevelDb() const { return db; }
    void paint(juce::Graphics& g) override {
        auto bounds=getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(.4f));
        g.fillRoundedRectangle(bounds.reduced(1).translated(0,3),12);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777d70),0,0,juce::Colour(0xff151b18),0,bounds.getHeight(),false));
        g.fillRoundedRectangle(bounds,12);
        auto face=bounds.reduced(7);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfffff4cf),face.getCentreX(),face.getCentreY(),juce::Colour(0xffcfbb8b),face.getRight(),face.getBottom(),true));
        g.fillRoundedRectangle(face,7);
        juce::Graphics::ScopedSaveState clip(g); g.reduceClipRegion(face.toNearestInt());
        const float cx=bounds.getCentreX(),cy=bounds.getHeight()-25.f;
        const float radius=juce::jmin(bounds.getWidth()*.385f,bounds.getHeight()*.65f);
        auto angleFor=[](float value) { return juce::jmap(juce::jlimit(-60.f,0.f,value),-60.f,0.f,-.98f,.98f); };
        auto point=[cx,cy](float angle,float distance) { return juce::Point<float>{cx+std::sin(angle)*distance,cy-std::cos(angle)*distance}; };
        juce::Path arc; arc.addCentredArc(cx,cy,radius,radius,0,angleFor(-60),angleFor(0),true);
        g.setColour(juce::Colour(0xff5a5141)); g.strokePath(arc,juce::PathStrokeType(1));
        for(int value=-60;value<=0;value+=3) {
            const bool major=value%12==0 || value==-6;
            const auto angle=angleFor(static_cast<float>(value));
            const auto a=point(angle,radius),b=point(angle,radius+(major?10.f:5.f));
            g.setColour(value>=-6?juce::Colour(0xffa44f3e):juce::Colour(0xff514a3d));
            g.drawLine({a,b},major?1.3f:.7f);
            if(major) {
                const auto centre=point(angle,radius+22);
                g.setFont(juce::FontOptions(11.f));
                g.drawText(juce::String(value),juce::Rectangle<float>(28,16).withCentre(centre),juce::Justification::centred);
            }
        }
        g.setColour(juce::Colour(0xff706249)); g.setFont(juce::FontOptions(10.f));
        g.drawText("OUTPUT / dBFS",0,static_cast<int>(cy-46),getWidth(),16,juce::Justification::centred);
        g.setFont(juce::FontOptions(12.f).withStyle("Bold"));
        g.drawText(channel,0,static_cast<int>(cy-27),getWidth(),16,juce::Justification::centred);
        const auto tip=point(angleFor(db),radius+7);
        g.setColour(juce::Colours::black.withAlpha(.16f)); g.drawLine(cx+2,cy+2,tip.x+2,tip.y+2,3);
        g.setColour(juce::Colour(0xffad4234)); g.drawLine(cx,cy,tip.x,tip.y,2);
        g.setColour(juce::Colour(0xff484639)); g.fillEllipse(cx-6,cy-6,12,12);
        g.setColour(juce::Colour(0xffb6aa88)); g.fillEllipse(cx-2,cy-2,4,4);
        g.setColour(juce::Colour(0xff675941)); g.setFont(juce::FontOptions(10.f));
        g.drawText(db<=-60?"-inf dBFS":juce::String(db,1)+" dBFS",getWidth()-90,getHeight()-28,73,15,juce::Justification::right);
        juce::Path reflection; reflection.addTriangle(face.getX(),face.getY(),face.getRight(),face.getY(),face.getX(),face.getY()+face.getHeight()*.42f);
        g.setColour(juce::Colours::white.withAlpha(.14f)); g.fillPath(reflection);
    }
private:
    juce::String channel;
    float db=-60;
};
