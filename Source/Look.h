// Rotary control design adapted from Doornenbal Audio ToTape Studio (MIT).
namespace style {

const juce::Colour cream{0xfff4eddb}, ink{0xff252b29}, gold{0xffd7ac61}, muted{0xff858b80};
class Look final : public juce::LookAndFeel_V4 {
public:
    Look() {
        setColour(juce::Slider::textBoxTextColourId, cream);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::TextEditor::highlightColourId, gold.withAlpha(.4f));
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&) override {
        auto r=juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),static_cast<float>(w),static_cast<float>(h)).reduced(16);
        auto radius=juce::jmin(r.getWidth(),r.getHeight())*.5f;
        auto cx=r.getCentreX(), cy=r.getCentreY();
        juce::Path track, active;
        track.addCentredArc(cx,cy,radius,radius,0,start,end,true);
        active.addCentredArc(cx,cy,radius,radius,0,start,start+pos*(end-start),true);
        g.setColour(cream.withAlpha(.12f)); g.strokePath(track,juce::PathStrokeType(3));
        g.setColour(gold); g.strokePath(active,juce::PathStrokeType(3));
        auto dial=juce::Rectangle<float>(radius*1.58f,radius*1.58f).withCentre({cx,cy});
        g.setColour(juce::Colours::black.withAlpha(.3f)); g.fillEllipse(dial.translated(0,5));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff535a52),cx,cy-radius,ink,cx,cy+radius,false));
        g.fillEllipse(dial); g.setColour(cream.withAlpha(.15f)); g.drawEllipse(dial,1);
        const auto angle=start+pos*(end-start);
        g.setColour(gold);
        g.drawLine(cx+std::sin(angle)*radius*.48f,cy-std::cos(angle)*radius*.48f,
                   cx+std::sin(angle)*radius*.68f,cy-std::cos(angle)*radius*.68f,3);
    }
};
}
