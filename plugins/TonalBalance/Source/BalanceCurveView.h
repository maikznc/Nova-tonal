#pragma once

#include <ms_core/ms_core.h>

//==============================================================================
/** Detailansicht: Zielbereich (Band), Ziel-Median (gestrichelt) und Live-Kurve.
    Abschnitte außerhalb des Zielbereichs werden orange markiert. */
class BalanceCurveView : public juce::Component
{
public:
    using BandValues = std::array<float, ms::BandLayout::numBands>;

    BalanceCurveView();

    void setLive (const BandValues& values, bool hasSignal, bool isStale);
    void setTarget (std::shared_ptr<const ms::TargetCurve>);
    void setNarrowRange (bool);
    void setTilt (float dbPerOctave);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> getPlotArea() const;
    float frequencyToX (double f, juce::Rectangle<float> area) const;
    double xToFrequency (float x, juce::Rectangle<float> area) const;
    float dbToY (float db, juce::Rectangle<float> area) const;
    float tiltAt (double f) const;
    void updateRange();

    void drawGrid (juce::Graphics&, juce::Rectangle<float> area);
    void drawTarget (juce::Graphics&, juce::Rectangle<float> area);
    void drawLive (juce::Graphics&, juce::Rectangle<float> area);
    void drawHover (juce::Graphics&, juce::Rectangle<float> area);
    void drawEmptyHints (juce::Graphics&, juce::Rectangle<float> area);

    BandValues live {};
    bool hasLive = false, stale = false;
    std::shared_ptr<const ms::TargetCurve> target;
    bool narrow = false;
    float tilt = 0.0f;
    float yMin = -30.0f, yMax = 12.0f;
    std::optional<float> hoverX;

    static constexpr double displayMinHz = 20.0, displayMaxHz = 20000.0;
};
