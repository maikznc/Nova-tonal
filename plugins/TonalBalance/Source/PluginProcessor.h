#pragma once

#include <ms_core/ms_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** NOVA Leitstern (Tonal Balance): reines Analyse-Plugin. Das Audio läuft unverändert durch;
    analysiert wird im Hintergrund (ms::LiveAnalyser). */
class TonalBalanceProcessor : public juce::AudioProcessor
{
public:
    TonalBalanceProcessor();
    ~TonalBalanceProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Schnittstelle für den Editor (alles thread-sicher)

    ms::LiveAnalyser& getLiveAnalyser() noexcept            { return liveAnalyser; }

    std::shared_ptr<const ms::TargetCurve> getTarget() const;
    void setTarget (std::shared_ptr<const ms::TargetCurve>);
    int getTargetVersion() const noexcept                   { return targetVersion.load(); }

    /** Startet die Analyse der Referenztracks; das Ergebnis wird automatisch zum Ziel. */
    bool analyseReferences (const juce::Array<juce::File>& files);
    const ms::ReferenceAnalysisJob& getAnalysisJob() const noexcept { return analysisJob; }
    void cancelAnalysis()                                   { analysisJob.cancel(); }

    /** Fehlermeldungen der letzten Analyse (leer = alles ok). Holt sie ab und leert sie. */
    juce::StringArray takeAnalysisMessages();

    /** Ansichtseinstellungen – werden mit dem Projekt gespeichert. */
    struct ViewSettings
    {
        std::atomic<bool>  showIntegrated { false };   // false = Kurzzeit (3 s), true = Gesamt
        std::atomic<bool>  narrowRange    { false };   // false = 10–90 %, true = 25–75 %
        std::atomic<float> tiltDbPerOct   { 0.0f };    // reine Anzeige-Neigung
    };

    ViewSettings view;

private:
    ms::LiveAnalyser liveAnalyser;

    mutable std::mutex targetLock;
    std::shared_ptr<const ms::TargetCurve> target;
    std::atomic<int> targetVersion { 0 };

    std::mutex messagesLock;
    juce::StringArray analysisMessages;

    // Zuletzt deklariert → wird zuerst zerstört (stoppt seinen Thread, bevor
    // die Member oben verschwinden, die der Callback benutzt).
    ms::ReferenceAnalysisJob analysisJob;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonalBalanceProcessor)
};
