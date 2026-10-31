#pragma once

namespace ms
{

/** UTF-8-Literal → juce::String (für Umlaute in UI-Texten). */
inline juce::String de (const char* utf8) { return juce::String::fromUTF8 (utf8); }

//==============================================================================
/** Frequenzraster der Suite: 1/6-Oktav-Bänder von 20 Hz bis 20,48 kHz.

    Tonal Balance, Mastering-Assistent und gespeicherte Zielkurven nutzen alle
    dasselbe Raster – dadurch sind Zielkurven zwischen den Plugins austauschbar.
*/
struct BandLayout
{
    static constexpr double minFrequency   = 20.0;
    static constexpr int    bandsPerOctave = 6;
    static constexpr int    numBands       = 60;    // 10 Oktaven

    static double getLowerEdge (int band) noexcept { return minFrequency * std::exp2 (band / (double) bandsPerOctave); }
    static double getUpperEdge (int band) noexcept { return getLowerEdge (band + 1); }
    static double getCentre    (int band) noexcept { return minFrequency * std::exp2 ((band + 0.5) / (double) bandsPerOctave); }
    static double getMaxFrequency() noexcept       { return getUpperEdge (numBands - 1); }
};

using BandPowers = std::array<double, BandLayout::numBands>;
using BandFlags  = std::array<bool,   BandLayout::numBands>;

//==============================================================================
/** Die vier breiten Bereiche (wie die Übersicht in iZotope Tonal Balance Control). */
struct Region
{
    const char* name;      // UTF-8
    double lowFrequency;
    double highFrequency;
};

inline constexpr int numRegions = 4;

const std::array<Region, numRegions>& getRegions() noexcept;

/** Bereich, zu dem ein Band gehört (nach Bandmitte), oder -1. */
int getRegionForBand (int band) noexcept;

//==============================================================================
/** Leistungs-Betragsquadrat der K-Gewichtung (ITU-R BS.1770) bei Frequenz f,
    ausgewertet mit den Referenzkoeffizienten für 48 kHz. */
double kWeightingPowerGain (double frequency) noexcept;

} // namespace ms
