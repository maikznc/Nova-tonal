#pragma once

namespace ms
{

//==============================================================================
/** Farben der Suite – an einer Stelle, damit alle Plugins gleich aussehen. */
namespace Colours
{
    inline const juce::Colour background   { 0xff101217 };
    inline const juce::Colour panel        { 0xff171a20 };
    inline const juce::Colour panelRaised  { 0xff1e222a };
    inline const juce::Colour outline      { 0xff2a2f39 };
    inline const juce::Colour gridMajor    { 0xff262b34 };
    inline const juce::Colour gridMinor    { 0xff1b1f26 };
    inline const juce::Colour text         { 0xffd3d8e0 };
    inline const juce::Colour textDim      { 0xff7c8494 };
    inline const juce::Colour textFaint    { 0xff4d5463 };
    inline const juce::Colour target       { 0xff5f9fe0 };   // Zielbereich
    inline const juce::Colour live         { 0xfff2f4f7 };   // Live-Kurve
    inline const juce::Colour ok           { 0xff45cfa0 };   // im Zielbereich
    inline const juce::Colour warn         { 0xffff8a4c };   // außerhalb
    inline const juce::Colour accent       { 0xff5f9fe0 };
}

//==============================================================================
class SuiteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SuiteLookAndFeel();

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

} // namespace ms
