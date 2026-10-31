#pragma once

namespace ms
{

//==============================================================================
/** Echtzeit-Analyse für Plugins.

    Der Audio-Thread schreibt nur in einen lock-freien FIFO (push). Ein eigener
    Hintergrund-Thread rechnet FFTs, Kurzzeit- und Gesamtspektrum und legt das
    Ergebnis ab, das der Editor per getResults() abholt. Läuft auch, wenn das
    Plugin-Fenster geschlossen ist (Gesamtmessung geht weiter).
*/
class LiveAnalyser : private juce::Thread
{
public:
    struct Results
    {
        SpectrumSnapshot shortTerm;     // letzte 3 s
        SpectrumSnapshot integrated;    // seit Reset (Stille ausgenommen)
        double integratedSeconds = 0.0;
    };

    LiveAnalyser();
    ~LiveAnalyser() override;

    /** Aus prepareToPlay aufrufen (nicht Echtzeit). */
    void prepare (double sampleRate);

    /** Aus processBlock aufrufen: kopiert nur, keine Allokation, keine Locks. */
    void push (const juce::AudioBuffer<float>& buffer, int numInputChannels) noexcept;

    void resetIntegrated() noexcept { resetRequested = true; }

    Results getResults() const;
    uint32_t getSequence() const noexcept { return sequence.load(); }

private:
    void run() override;

    static constexpr int numFifoChannels = 2;

    juce::AbstractFifo fifo { 1 };
    juce::AudioBuffer<float> fifoBuffer, scratch;

    SpectrumEngine engine;
    SpectrumIntegrator shortTerm;
    SpectrumAccumulator integrated;

    std::atomic<bool> resetRequested { false };
    std::atomic<uint32_t> sequence { 0 };

    mutable std::mutex resultsLock;
    Results results;
};

} // namespace ms
