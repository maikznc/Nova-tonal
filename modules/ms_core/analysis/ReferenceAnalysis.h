#pragma once

namespace ms
{

//==============================================================================
/** Analysiert eine Audiodatei exakt wie die Live-Analyse (gleiches Bandraster,
    gleiche 3-s-Fenster, gleiche Normierung) und fügt die Messfenster dem Builder
    als einen Track hinzu. Ein Messfenster pro Sekunde.

    shouldContinue (Fortschritt 0…1) → false bricht ab.
*/
juce::Result analyseReferenceFile (juce::AudioFormatManager& formats,
                                   const juce::File& file,
                                   TargetBuilder& builder,
                                   const std::function<bool (double)>& shouldContinue = {});

/** Dateiendungen, die als Referenz akzeptiert werden (für Drag & Drop / Dateiauswahl). */
juce::String getSupportedAudioWildcard();
bool isSupportedAudioFile (const juce::File&);

//==============================================================================
/** Analysiert mehrere Referenzdateien im Hintergrund und baut daraus eine Zielkurve. */
class ReferenceAnalysisJob : private juce::Thread
{
public:
    struct Result
    {
        std::optional<TargetCurve> curve;
        juce::StringArray errors;
        bool cancelled = false;
    };

    /** Wird im Analyse-Thread aufgerufen, nicht im Message-Thread! */
    using Callback = std::function<void (Result)>;

    ReferenceAnalysisJob();
    ~ReferenceAnalysisJob() override;

    /** false, wenn gerade schon eine Analyse läuft. */
    bool start (const juce::Array<juce::File>& files, const juce::String& targetName, Callback onFinished);
    void cancel();

    bool isBusy() const noexcept        { return busy.load(); }
    float getProgress() const noexcept  { return progress.load(); }
    juce::String getStatusText() const;

private:
    void run() override;
    void setStatus (const juce::String&);

    juce::AudioFormatManager formats;
    juce::Array<juce::File> pendingFiles;
    juce::String pendingName;
    Callback callback;

    std::atomic<bool> busy { false };
    std::atomic<float> progress { 0.0f };
    mutable std::mutex statusLock;
    juce::String status;
};

} // namespace ms
