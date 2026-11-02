#include "PluginEditor.h"

namespace
{
const juce::Colour kBg { 0xff161618 };
const juce::Colour kPanel { 0xff242428 };
const juce::Colour kAmber { 0xffe0a020 };
const juce::Colour kText { 0xffe6e4dc };
const juce::Colour kMuted { 0xff8a8680 };
const juce::Colour kWell { 0xff1a1a1c };
const juce::Colour kLine { 0xff3a3a40 };
constexpr int kEditorW = 420;
constexpr int kEditorH = 240;
} // namespace

DistLookAndFeel::DistLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId, kAmber);
}

void DistLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (kWell);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (kLine);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (kAmber);
    g.strokePath (arc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path tick;
    tick.addRectangle (-1.1f, -radius + 8.0f, 2.2f, radius * 0.42f);
    g.setColour (kText);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

juce::Font DistLookAndFeel::getLabelFont (juce::Label& label)
{
    return label.getFont();
}

DistEditor::DistEditor (DistProcessor& p)
    : AudioProcessorEditor (&p),
      amountA (p.apvts, "amount", amount),
      shapeA (p.apvts, "shape", shape)
{
    setLookAndFeel (&lnf);
    setSize (kEditorW, kEditorH);
    setResizable (false, false);

    title.setText ("OpenRB Distort", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, kAmber);
    title.setFont (juce::FontOptions().withHeight (18.0f).withStyle ("Bold"));
    addAndMakeVisible (title);

    auto setupName = [this] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, kMuted);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupKnob (amount);
    setupKnob (shape);
    setupName (amountN, "AMT");
    setupName (shapeN, "SHAPE");
}

DistEditor::~DistEditor()
{
    setLookAndFeel (nullptr);
}

void DistEditor::setupKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
    s.setNumDecimalPlacesToDisplay (0);
    s.setMouseDragSensitivity (140);
    addAndMakeVisible (s);
}

void DistEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto panel = getLocalBounds().toFloat().reduced (12.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);
}

void DistEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    title.setBounds (r.removeFromTop (28));
    r.removeFromTop (8);

    const int n = 2;
    const int w = r.getWidth() / n;
    const int y = r.getY();
    const int h = r.getHeight();

    auto place = [&] (juce::Slider& s, juce::Label& name, int i)
    {
        auto b = juce::Rectangle<int> (r.getX() + i * w, y, w, h).reduced (6, 0);
        name.setBounds (b.removeFromTop (18));
        s.setBounds (b);
    };

    place (amount, amountN, 0);
    place (shape, shapeN, 1);
}
