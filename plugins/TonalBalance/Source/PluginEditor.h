#pragma once

#include "PluginProcessor.h"
#include "BalanceCurveView.h"
#include "RegionMetersView.h"

//==============================================================================
/** ComboBox, die vor dem Aufklappen ihre Liste aktualisiert (neue Zielkurven im Ordner). */
class RefreshingComboBox : public juce::ComboBox
{
public:
    std::function<void()> beforePopup;

    void showPopup() override
    {
        if (beforePopup)
            beforePopup();

        juce::ComboBox::showPopup();
    }
};

//==============================================================================
class TonalBalanceEditor : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
public:
    explicit TonalBalanceEditor (TonalBalanceProcessor&);
    ~TonalBalanceEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    /** Holt neue Daten vom Prozessor (wird per Timer mit 30 Hz aufgerufen). */
    void refresh();

private:
    void timerCallback() override { refresh(); }

    void refreshTargetList();
    void targetSelected();
    void chooseReferences();
    void saveTarget();
    void startAnalysis (const juce::Array<juce::File>&);
    void importTarget (const juce::File&);
    void showMessage (const juce::String&, bool isError);
    void syncViewControls();
    void resetSmoothing();

    TonalBalanceProcessor& proc;
    ms::SuiteLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 700 };

    BalanceCurveView curveView;
    RegionMetersView regionMeters;

    RefreshingComboBox targetBox;
    juce::TextButton loadButton, saveButton, cancelButton;
    juce::TextButton shortTermButton, integratedButton, wideButton, narrowButton, resetButton;
    juce::ComboBox tiltBox;

    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::Array<juce::File> targetFiles;
    juce::File lastReferenceFolder;

    // Anzeige-Zustand
    std::array<float, ms::BandLayout::numBands> latestBands {}, shownBands {};
    std::array<float, ms::numRegions> latestRegions {}, shownRegions {};
    float latestLoudness = -100.0f, shownLoudness = -100.0f;
    double integratedSeconds = 0.0;
    bool hasLiveData = false;
    int ticksSinceValid = 1000;
    uint32_t lastSequence = 0;
    int lastTargetVersion = -1;
    bool lastShowIntegrated = false;
    bool wasAnalysing = false;

    bool dragActive = false, dragIsJson = false;
    juce::String message;
    bool messageIsError = false;
    int messageTicks = 0;

    juce::Rectangle<int> headerArea, footerArea, statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonalBalanceEditor)
};
