#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    const juce::Colour background { 0xff0b0b0f };
    const juce::Colour panel      { 0xff15151c };
    const juce::Colour border     { 0xff26262f };
    const juce::Colour accent     { 0xff8b5cf6 };
    const juce::Colour highlight  { 0xffa78bfa };
    const juce::Colour deep       { 0xff4c1d95 };
    const juce::Colour text       { 0xffededed };
    const juce::Colour textDim    { 0xff8a8a99 };
    const juce::Colour track      { 0xff24242d };
    const juce::Colour wave       { 0xff5f5f6e };
}

class SwellLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SwellLookAndFeel();

    static juce::Font font (float height, bool bold = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isHighlighted, bool isDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;

    juce::Font getLabelFont (juce::Label&) override;

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
};
