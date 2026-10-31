#pragma once

#include <ms_core/ms_core.h>

//==============================================================================
/** Übersicht: vier breite Bereiche, jeweils Zielbereich vs. aktueller Wert –
    relativ zum Ziel-Median, damit man auf einen Blick sieht, wo es hakt. */
class RegionMetersView : public juce::Component
{
public:
    using RegionValues = std::array<float, ms::numRegions>;

    RegionMetersView();

    void setLive (const RegionValues& values, bool hasSignal, bool isStale);
    void setTarget (std::shared_ptr<const ms::TargetCurve>);
    void setNarrowRange (bool);

    void paint (juce::Graphics&) override;

private:
    void drawMeter (juce::Graphics&, juce::Rectangle<float> area, int region);

    RegionValues live {};
    bool hasLive = false, stale = false;
    std::shared_ptr<const ms::TargetCurve> target;
    bool narrow = false;

    static constexpr float scaleDb = 12.0f;   // ±12 dB um den Ziel-Median
};
