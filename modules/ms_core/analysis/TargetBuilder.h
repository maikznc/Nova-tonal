#pragma once

namespace ms
{

//==============================================================================
/** Sammelt die Messfenster mehrerer Referenztracks und berechnet daraus eine Zielkurve.

    - Jeder Track zählt gleich viel, egal wie lang er ist.
    - Leise Passagen (mehr als 10 dB unter der mittleren Lautheit des Tracks,
      z. B. Intros, Breaks, Fade-outs) werden ignoriert – wie das relative Gate
      bei der LUFS-Messung.
*/
class TargetBuilder
{
public:
    void beginTrack (const juce::String& trackName);
    void addObservation (const SpectrumSnapshot& snapshot);

    /** Schließt den Track ab. false, wenn er keine verwertbaren Messfenster enthielt. */
    bool endTrack();

    /** Verwirft den angefangenen Track (z. B. bei Abbruch). */
    void cancelTrack() noexcept { current.reset(); }

    int getNumTracks() const noexcept { return (int) tracks.size(); }
    juce::StringArray getTrackNames() const;

    TargetCurve build (const juce::String& curveName) const;

    static constexpr double relativeGateDb = 10.0;

private:
    struct Track
    {
        juce::String name;
        std::vector<SpectrumSnapshot> observations;
    };

    std::vector<Track> tracks;
    std::optional<Track> current;
};

} // namespace ms
