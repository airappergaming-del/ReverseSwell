#include "SwellLookAndFeel.h"

using namespace juce;

SwellLookAndFeel::SwellLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, Palette::background);

    setColour (Label::textColourId, Palette::text);

    setColour (Slider::textBoxTextColourId, Palette::textDim);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Palette::deep);

    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Colours::white);

    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::backgroundColourId, Palette::panel);
    setColour (ComboBox::outlineColourId, Palette::border);
    setColour (ComboBox::arrowColourId, Palette::highlight);

    setColour (PopupMenu::backgroundColourId, Palette::panel);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::deep);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);

    setColour (TooltipWindow::backgroundColourId, Palette::panel);
    setColour (TooltipWindow::textColourId, Palette::text);
    setColour (TooltipWindow::outlineColourId, Palette::border);

    setColour (AlertWindow::backgroundColourId, Palette::panel);
    setColour (AlertWindow::textColourId, Palette::text);
}

Font SwellLookAndFeel::font (float height, bool bold)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain));
}

//==============================================================================
void SwellLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle, Slider& slider)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float lineW  = jmax (3.0f, radius * 0.13f);
    const float arcR   = radius - lineW * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const PathStrokeType stroke (lineW, PathStrokeType::curved, PathStrokeType::rounded);

    Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::track);
    g.strokePath (trackArc, stroke);

    if (sliderPos > 0.0f)
    {
        Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? Palette::accent : Palette::textDim);
        g.strokePath (valueArc, stroke);
    }

    const float inner = radius - lineW * 2.2f;
    if (inner > 2.0f)
    {
        const auto knob = Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (centre);
        g.setColour (Palette::panel.brighter (slider.isMouseOverOrDragging() ? 0.14f : 0.07f));
        g.fillEllipse (knob);
        g.setColour (Palette::border);
        g.drawEllipse (knob, 1.0f);

        const auto p1 = centre.getPointOnCircumference (inner * 0.30f, angle);
        const auto p2 = centre.getPointOnCircumference (inner * 0.80f, angle);
        g.setColour (Palette::text);
        g.drawLine ({ p1, p2 }, 2.0f);
    }
}

Label* SwellLookAndFeel::createSliderTextBox (Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (12.0f));
    label->setJustificationType (Justification::centred);
    label->setColour (Label::textColourId, Palette::textDim);
    label->setColour (Label::outlineColourId, Colours::transparentBlack);
    label->setColour (Label::backgroundColourId, Colours::transparentBlack);
    label->setColour (Label::outlineWhenEditingColourId, Palette::accent);
    label->setColour (Label::backgroundWhenEditingColourId, Palette::background);
    label->setColour (Label::textWhenEditingColourId, Palette::text);
    return label;
}

//==============================================================================
void SwellLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour&,
                                             bool isHighlighted, bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool primary = button.getProperties()["primary"];

    auto fill = primary ? Palette::accent : Palette::panel.brighter (0.05f);
    if (isDown)             fill = fill.darker (0.25f);
    else if (isHighlighted) fill = fill.brighter (0.12f);
    if (! button.isEnabled()) fill = fill.withMultipliedAlpha (0.4f);

    g.setColour (fill);
    g.fillRoundedRectangle (r, 8.0f);

    if (! primary)
    {
        g.setColour (isHighlighted && button.isEnabled() ? Palette::accent.withAlpha (0.7f) : Palette::border);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
}

Font SwellLookAndFeel::getTextButtonFont (TextButton&, int buttonHeight)
{
    return font (jmin (14.0f, (float) buttonHeight * 0.5f), true);
}

//==============================================================================
void SwellLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button, bool isHighlighted, bool)
{
    const auto r    = button.getLocalBounds().toFloat();
    const float h   = jmin (18.0f, r.getHeight() - 4.0f);
    const float w   = h * 1.8f;
    const auto pill = Rectangle<float> (r.getX() + 1.0f, r.getCentreY() - h * 0.5f, w, h);
    const bool on   = button.getToggleState();

    g.setColour (on ? Palette::accent : Palette::track);
    g.fillRoundedRectangle (pill, h * 0.5f);
    if (! on)
    {
        g.setColour (isHighlighted ? Palette::accent.withAlpha (0.6f) : Palette::border);
        g.drawRoundedRectangle (pill, h * 0.5f, 1.0f);
    }

    const float d  = h - 4.0f;
    const float kx = on ? pill.getRight() - d - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? Colours::white : Palette::textDim);
    g.fillEllipse (kx, pill.getY() + 2.0f, d, d);

    g.setColour (button.isEnabled() ? (isHighlighted ? Palette::text : Palette::text.withAlpha (0.85f))
                                    : Palette::textDim.withAlpha (0.5f));
    g.setFont (font (13.0f));
    g.drawText (button.getButtonText(), r.withLeft (pill.getRight() + 8.0f), Justification::centredLeft, true);
}

//==============================================================================
void SwellLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool,
                                     int, int, int, int, ComboBox& box)
{
    const auto r = Rectangle<float> ((float) width, (float) height).reduced (0.5f);

    g.setColour (Palette::panel.brighter (box.isMouseOver (true) ? 0.1f : 0.05f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Palette::accent : Palette::border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const float cx = r.getRight() - 14.0f, cy = r.getCentreY();
    Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (Palette::highlight);
    g.strokePath (chevron, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font SwellLookAndFeel::getComboBoxFont (ComboBox&)    { return font (13.0f); }
Font SwellLookAndFeel::getPopupMenuFont()             { return font (14.0f); }

void SwellLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

Font SwellLookAndFeel::getLabelFont (Label& label)
{
    return label.getFont();
}

void SwellLookAndFeel::drawCornerResizer (Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    g.setColour ((isMouseOver || isMouseDragging) ? Palette::accent : Palette::border.brighter (0.3f));
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h - 2.0f, (float) w - 2.0f, (float) h * i, 1.5f);
}
