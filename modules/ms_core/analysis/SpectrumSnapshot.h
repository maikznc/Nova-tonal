#pragma once

namespace ms
{

//==============================================================================
/** Lautheitsnormiertes Spektrum – das Format, in dem Live-Signal und Zielkurven
    verglichen werden.

    Jeder Bandwert ist in dB relativ zur (K-gewichteten) Gesamtlautheit des
    Signals. Dadurch ist die Kurve pegelunabhängig: Ein Track bei −8 LUFS und
    derselbe Track bei −14 LUFS ergeben dieselbe Kurve. 0 dB entspricht etwa
    rosa Rauschen gleicher Lautheit.
*/
struct SpectrumSnapshot
{
    static constexpr float invalid = std::numeric_limits<float>::quiet_NaN();
    static constexpr double silenceThresholdLufs = -70.0;

    std::array<float, BandLayout::numBands> bands;
    std::array<float, numRegions> regions;
    float loudness = -100.0f;   // ≈ LUFS des Analysefensters
    bool valid = false;

    SpectrumSnapshot() { bands.fill (invalid); regions.fill (invalid); }

    /** Normiert rohe Bandleistungen. Unterhalb von −70 LUFS gilt das Fenster als Stille (valid = false). */
    static SpectrumSnapshot fromBandPowers (const BandPowers& powers, const BandFlags& validBands);

    /** ≈ LUFS aus Bandleistungen (K-Gewichtung je Bandmitte, −0,691 dB Offset nach BS.1770). */
    static double computeLoudness (const BandPowers& powers, const BandFlags& validBands) noexcept;
};

} // namespace ms
