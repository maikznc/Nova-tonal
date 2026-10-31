namespace ms
{

SuiteLookAndFeel::SuiteLookAndFeel()
    : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme {
          Colours::background,   // windowBackground
          Colours::panelRaised,  // widgetBackground
          Colours::panelRaised,  // menuBackground
          Colours::outline,      // outline
          Colours::text,         // defaultText
          Colours::panelRaised,  // defaultFill
          Colours::background,   // highlightedText
          Colours::accent,       // highlightedFill
          Colours::text })       // menuText
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
    setColour (juce::TextButton::buttonColourId, Colours::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent.withAlpha (0.22f));
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::text);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelRaised);
    setColour (juce::ComboBox::outlineColourId, Colours::outline);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::textDim);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panelRaised);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::panelRaised);
    setColour (juce::TooltipWindow::textColourId, Colours::text);
    setColour (juce::TooltipWindow::outlineColourId, Colours::outline);
}

juce::Font SuiteLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.5f));
}

juce::Font SuiteLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return juce::FontOptions (13.0f); }
juce::Font SuiteLookAndFeel::getPopupMenuFont()                 { return juce::FontOptions (13.5f); }
juce::Font SuiteLookAndFeel::getLabelFont (juce::Label& l)      { return l.getFont(); }

void SuiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 4.0f;

    const bool on = button.getToggleState();
    auto fill = on ? button.findColour (juce::TextButton::buttonOnColourId)
                   : button.findColour (juce::TextButton::buttonColourId);

    if (isButtonDown)           fill = fill.brighter (0.15f);
    else if (isMouseOverButton) fill = fill.brighter (0.06f);

    juce::Path p;
    p.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), corner, corner,
                           ! (button.isConnectedOnLeft() || button.isConnectedOnTop()),
                           ! (button.isConnectedOnRight() || button.isConnectedOnTop()),
                           ! (button.isConnectedOnLeft() || button.isConnectedOnBottom()),
                           ! (button.isConnectedOnRight() || button.isConnectedOnBottom()));

    g.setColour (fill);
    g.fillPath (p);
    g.setColour (on ? Colours::accent.withAlpha (0.6f) : Colours::outline);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

void SuiteLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Colours::accent.withAlpha (0.6f) : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // kleiner Pfeil rechts
    const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SuiteLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

} // namespace ms
