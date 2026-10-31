#pragma once

namespace ms
{

//==============================================================================
/** Verteilung eines Werts über alle Referenz-Messfenster. */
struct Percentiles
{
    float p10 = SpectrumSnapshot::invalid;
    float p25 = SpectrumSnapshot::invalid;
    float p50 = SpectrumSnapshot::invalid;
    float p75 = SpectrumSnapshot::invalid;
    float p90 = SpectrumSnapshot::invalid;

    bool isValid() const noexcept                    { return std::isfinite (p50); }
    float getLower (bool narrow) const noexcept      { return narrow ? p25 : p10; }
    float getUpper (bool narrow) const noexcept      { return narrow ? p75 : p90; }

    /** < 0: unter dem Bereich, 0: drin, > 0: darüber (Abstand in dB). */
    float getDeviation (float value, bool narrow) const noexcept
    {
        if (! isValid() || ! std::isfinite (value)) return 0.0f;
        if (value < getLower (narrow)) return value - getLower (narrow);
        if (value > getUpper (narrow)) return value - getUpper (narrow);
        return 0.0f;
    }
};

//==============================================================================
/** Zielkurve: wo das Spektrum der Referenzen typischerweise liegt.

    Pro Band und pro Bereich: Median (p50) plus Streubereich – "weit" = 10.–90.
    Perzentil, "eng" = 25.–75. Perzentil. Wird als JSON gespeichert und von allen
    Plugins der Suite gelesen.
*/
struct TargetCurve
{
    juce::String name;
    juce::StringArray sources;
    std::array<Percentiles, BandLayout::numBands> bands;
    std::array<Percentiles, numRegions> regions;
    Percentiles loudness;       // ≈ LUFS der Referenzen (Kurzzeit-Fenster)

    bool isValid() const noexcept;

    juce::var toVar() const;
    static std::optional<TargetCurve> fromVar (const juce::var&);

    juce::String toJson() const;
    static std::optional<TargetCurve> fromJson (const juce::String&);

    bool saveToFile (const juce::File&) const;
    static std::optional<TargetCurve> loadFromFile (const juce::File&);

    /** Gemeinsamer Ordner für gespeicherte Zielkurven aller Plugins der Suite.
        macOS: ~/Library/Application Support/NOVA/Targets */
    static juce::File getUserTargetsFolder();
};

} // namespace ms
