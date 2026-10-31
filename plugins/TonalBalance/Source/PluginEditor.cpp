#include "PluginEditor.h"

namespace
{
    constexpr int noTargetId    = 1;
    constexpr int openFolderId  = 2;
    constexpr int firstFileId   = 100;

    constexpr float tiltValues[] = { 0.0f, 1.5f, 3.0f, 4.5f };

    juce::String formatLufs (float lufs)
    {
        if (lufs < -69.0f)
            return ms::de ("– LUFS");

        return ms::de ("\xe2\x89\x88 ") + (lufs < 0.0f ? ms::de ("−") : juce::String())
             + juce::String (std::abs (lufs), 1).replaceCharacter ('.', ',') + " LUFS";
    }

    juce::String formatDuration (double seconds)
    {
        const int s = (int) seconds;
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    void setupSegment (juce::TextButton& b, const juce::String& text, int group, int connectedEdges)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (group, juce::dontSendNotification);
        b.setConnectedEdges (connectedEdges);
        b.setColour (juce::TextButton::textColourOffId, ms::Colours::textDim);   // nicht gewählte Segmente gedämpft
    }
}

//==============================================================================
TonalBalanceEditor::TonalBalanceEditor (TonalBalanceProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lookAndFeel);
    resetSmoothing();

    addAndMakeVisible (curveView);
    addAndMakeVisible (regionMeters);

    // --- Kopfzeile
    addAndMakeVisible (targetBox);
    targetBox.setTooltip (ms::de ("Gespeicherte Zielkurven (gemeinsam für alle NOVA-Plugins)"));
    targetBox.beforePopup = [this] { refreshTargetList(); };
    targetBox.onChange = [this] { targetSelected(); };

    addAndMakeVisible (loadButton);
    loadButton.setButtonText (ms::de ("Referenzen laden \xe2\x80\xa6"));
    loadButton.setTooltip (ms::de ("Einen oder mehrere Referenztracks analysieren und daraus eine Zielkurve bauen"));
    loadButton.onClick = [this] { chooseReferences(); };

    addAndMakeVisible (saveButton);
    saveButton.setButtonText (ms::de ("Ziel speichern \xe2\x80\xa6"));
    saveButton.setTooltip (ms::de ("Aktuelle Zielkurve speichern – danach auch im Mastering-Assistenten verfügbar"));
    saveButton.onClick = [this] { saveTarget(); };

    addChildComponent (cancelButton);
    cancelButton.setButtonText (ms::de ("Abbrechen"));
    cancelButton.onClick = [this] { proc.cancelAnalysis(); };

    // --- Fußzeile: Ansicht
    using CE = juce::Button;
    setupSegment (shortTermButton, ms::de ("Kurzzeit"), 1001, CE::ConnectedOnRight);
    setupSegment (integratedButton, ms::de ("Gesamt"), 1001, CE::ConnectedOnLeft);
    setupSegment (wideButton, ms::de ("Weit"), 1002, CE::ConnectedOnRight);
    setupSegment (narrowButton, ms::de ("Eng"), 1002, CE::ConnectedOnLeft);

    shortTermButton.setTooltip (ms::de ("Live-Kurve über die letzten 3 Sekunden"));
    integratedButton.setTooltip (ms::de ("Mittelwert seit dem letzten Reset – ideal: Song einmal komplett durchlaufen lassen"));
    wideButton.setTooltip (ms::de ("Zielbereich = 10.–90. Perzentil der Referenzen"));
    narrowButton.setTooltip (ms::de ("Zielbereich = 25.–75. Perzentil (strenger)"));

    for (auto* b : { &shortTermButton, &integratedButton, &wideButton, &narrowButton })
        addAndMakeVisible (*b);

    shortTermButton.onClick  = [this] { proc.view.showIntegrated = false; resetSmoothing(); };
    integratedButton.onClick = [this] { proc.view.showIntegrated = true;  resetSmoothing(); };
    wideButton.onClick       = [this] { proc.view.narrowRange = false; syncViewControls(); };
    narrowButton.onClick     = [this] { proc.view.narrowRange = true;  syncViewControls(); };

    addAndMakeVisible (tiltBox);
    tiltBox.addItem (ms::de ("Neigung 0 dB/Okt"), 1);
    tiltBox.addItem (ms::de ("Neigung 1,5 dB/Okt"), 2);
    tiltBox.addItem (ms::de ("Neigung 3 dB/Okt"), 3);
    tiltBox.addItem (ms::de ("Neigung 4,5 dB/Okt"), 4);
    tiltBox.setTooltip (ms::de ("Kippt nur die Anzeige (um 1 kHz), damit Höhen und Bässe besser lesbar sind. Vergleich bleibt gleich."));
    tiltBox.onChange = [this]
    {
        const int index = juce::jlimit (0, 3, tiltBox.getSelectedId() - 1);
        proc.view.tiltDbPerOct = tiltValues[index];
        syncViewControls();
    };

    addAndMakeVisible (resetButton);
    resetButton.setButtonText ("Reset");
    resetButton.setTooltip (ms::de ("Gesamtmessung neu starten"));
    resetButton.onClick = [this]
    {
        proc.getLiveAnalyser().resetIntegrated();
        resetSmoothing();
    };

    lastReferenceFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    setResizable (true, true);
    setResizeLimits (780, 460, 1800, 1100);
    setSize (1000, 580);

    syncViewControls();
    refresh();
    startTimerHz (30);
}

TonalBalanceEditor::~TonalBalanceEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TonalBalanceEditor::resetSmoothing()
{
    latestBands.fill (ms::SpectrumSnapshot::invalid);
    shownBands.fill (ms::SpectrumSnapshot::invalid);
    latestRegions.fill (ms::SpectrumSnapshot::invalid);
    shownRegions.fill (ms::SpectrumSnapshot::invalid);
    latestLoudness = shownLoudness = -100.0f;
    hasLiveData = false;
    ticksSinceValid = 1000;
    lastSequence = 0;   // erzwingt Neuabholung
}

void TonalBalanceEditor::syncViewControls()
{
    const bool integrated = proc.view.showIntegrated.load();
    const bool narrow = proc.view.narrowRange.load();
    const float tilt = proc.view.tiltDbPerOct.load();

    shortTermButton.setToggleState (! integrated, juce::dontSendNotification);
    integratedButton.setToggleState (integrated, juce::dontSendNotification);
    wideButton.setToggleState (! narrow, juce::dontSendNotification);
    narrowButton.setToggleState (narrow, juce::dontSendNotification);
    resetButton.setEnabled (integrated);

    int tiltId = 1;
    for (int i = 0; i < 4; ++i)
        if (std::abs (tiltValues[i] - tilt) < 0.01f)
            tiltId = i + 1;

    tiltBox.setSelectedId (tiltId, juce::dontSendNotification);

    curveView.setNarrowRange (narrow);
    curveView.setTilt (tilt);
    regionMeters.setNarrowRange (narrow);
    lastShowIntegrated = integrated;
}

void TonalBalanceEditor::refresh()
{
    // Ansichtseinstellungen können sich durch Laden eines Projekts ändern
    if (proc.view.showIntegrated.load() != lastShowIntegrated)
    {
        resetSmoothing();
        syncViewControls();
    }

    // Live-Daten
    auto& analyser = proc.getLiveAnalyser();
    const auto sequence = analyser.getSequence();
    bool gotValid = false;

    if (sequence != lastSequence)
    {
        lastSequence = sequence;
        const auto results = analyser.getResults();
        const auto& snapshot = proc.view.showIntegrated.load() ? results.integrated : results.shortTerm;
        integratedSeconds = results.integratedSeconds;

        if (snapshot.valid)
        {
            latestBands = snapshot.bands;
            latestRegions = snapshot.regions;
            latestLoudness = snapshot.loudness;
            gotValid = true;
        }
    }

    ticksSinceValid = gotValid ? 0 : ticksSinceValid + 1;

    // weich nachführen (Daten kommen mit ~12 Hz, Anzeige läuft mit 30 Hz)
    constexpr float smoothing = 0.3f;

    auto follow = [] (float& shown, float latest)
    {
        if (! std::isfinite (latest))      shown = latest;
        else if (! std::isfinite (shown))  shown = latest;
        else                               shown += smoothing * (latest - shown);
    };

    for (size_t b = 0; b < shownBands.size(); ++b)     follow (shownBands[b], latestBands[b]);
    for (size_t r = 0; r < shownRegions.size(); ++r)   follow (shownRegions[r], latestRegions[r]);
    follow (shownLoudness, latestLoudness);

    hasLiveData = hasLiveData || gotValid;
    const bool stale = ticksSinceValid > 45;   // > 1,5 s kein gültiges Signal

    curveView.setLive (shownBands, hasLiveData, stale);
    regionMeters.setLive (shownRegions, hasLiveData, stale);

    // Zielkurve
    const int targetVersion = proc.getTargetVersion();

    if (targetVersion != lastTargetVersion)
    {
        lastTargetVersion = targetVersion;
        const auto target = proc.getTarget();
        curveView.setTarget (target);
        regionMeters.setTarget (target);
        refreshTargetList();
        saveButton.setEnabled (target != nullptr);
    }

    // Referenz-Analyse
    const bool analysing = proc.getAnalysisJob().isBusy();
    cancelButton.setVisible (analysing);
    loadButton.setEnabled (! analysing);

    const auto messages = proc.takeAnalysisMessages();

    if (! messages.isEmpty())
        showMessage (messages.joinIntoString (ms::de (" · ")), true);
    else if (wasAnalysing && ! analysing)
        if (const auto target = proc.getTarget())
            showMessage (ms::de ("Zielkurve erstellt: ") + target->name
                         + ms::de (" (") + juce::String (target->sources.size()) + (target->sources.size() == 1 ? " Track)" : " Tracks)"), false);

    wasAnalysing = analysing;

    if (messageTicks > 0 && --messageTicks == 0)
        message.clear();

    repaint (headerArea);
    repaint (footerArea);
}

//==============================================================================
void TonalBalanceEditor::refreshTargetList()
{
    const auto folder = ms::TargetCurve::getUserTargetsFolder();
    targetFiles = folder.findChildFiles (juce::File::findFiles, false, "*.json");
    targetFiles.sort();

    targetBox.clear (juce::dontSendNotification);
    targetBox.addItem (ms::de ("Kein Ziel"), noTargetId);

    if (! targetFiles.isEmpty())
    {
        targetBox.addSeparator();
        targetBox.addSectionHeading (ms::de ("Gespeicherte Ziele"));

        for (int i = 0; i < targetFiles.size(); ++i)
            targetBox.addItem (targetFiles[i].getFileNameWithoutExtension(), firstFileId + i);
    }

    targetBox.addSeparator();
    targetBox.addItem (ms::de ("Zielordner öffnen \xe2\x80\xa6"), openFolderId);

    const auto target = proc.getTarget();

    if (target == nullptr)
    {
        targetBox.setSelectedId (noTargetId, juce::dontSendNotification);
        return;
    }

    for (int i = 0; i < targetFiles.size(); ++i)
        if (targetFiles[i].getFileNameWithoutExtension() == target->name)
        {
            targetBox.setSelectedId (firstFileId + i, juce::dontSendNotification);
            return;
        }

    targetBox.setText (target->name, juce::dontSendNotification);   // ungespeichertes Ziel
}

void TonalBalanceEditor::targetSelected()
{
    const int id = targetBox.getSelectedId();

    if (id == noTargetId)
    {
        proc.setTarget (nullptr);
        return;
    }

    if (id == openFolderId)
    {
        const auto folder = ms::TargetCurve::getUserTargetsFolder();
        folder.createDirectory();
        folder.revealToUser();
        refreshTargetList();   // Auswahl wiederherstellen
        return;
    }

    const int index = id - firstFileId;

    if (juce::isPositiveAndBelow (index, targetFiles.size()))
    {
        if (auto curve = ms::TargetCurve::loadFromFile (targetFiles[index]))
        {
            curve->name = targetFiles[index].getFileNameWithoutExtension();
            proc.setTarget (std::make_shared<const ms::TargetCurve> (std::move (*curve)));
        }
        else
        {
            showMessage (ms::de ("Zielkurve konnte nicht geladen werden: ") + targetFiles[index].getFileName(), true);
            refreshTargetList();
        }
    }
}

void TonalBalanceEditor::chooseReferences()
{
    fileChooser = std::make_unique<juce::FileChooser> (ms::de ("Referenztracks auswählen"), lastReferenceFolder, ms::getSupportedAudioWildcard());

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto files = chooser.getResults();

                                  if (! files.isEmpty())
                                  {
                                      lastReferenceFolder = files.getFirst().getParentDirectory();
                                      startAnalysis (files);
                                  }
                              });
}

void TonalBalanceEditor::startAnalysis (const juce::Array<juce::File>& files)
{
    if (! proc.analyseReferences (files))
    {
        const auto messages = proc.takeAnalysisMessages();
        showMessage (messages.isEmpty() ? ms::de ("Es läuft bereits eine Analyse.") : messages.joinIntoString (" · "), true);
    }
    else
    {
        message.clear();
        messageTicks = 0;
    }
}

void TonalBalanceEditor::saveTarget()
{
    const auto target = proc.getTarget();

    if (target == nullptr)
        return;

    const auto folder = ms::TargetCurve::getUserTargetsFolder();
    folder.createDirectory();
    const auto suggested = folder.getChildFile (juce::File::createLegalFileName (target->name) + ".json");

    fileChooser = std::make_unique<juce::FileChooser> (ms::de ("Zielkurve speichern"), suggested, "*.json");

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, target] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();

                                  if (file == juce::File())
                                      return;

                                  file = file.withFileExtension ("json");

                                  auto renamed = *target;
                                  renamed.name = file.getFileNameWithoutExtension();

                                  if (renamed.saveToFile (file))
                                  {
                                      proc.setTarget (std::make_shared<const ms::TargetCurve> (std::move (renamed)));
                                      showMessage (ms::de ("Gespeichert: ") + file.getFileName(), false);
                                  }
                                  else
                                  {
                                      showMessage (ms::de ("Speichern fehlgeschlagen: ") + file.getFullPathName(), true);
                                  }
                              });
}

void TonalBalanceEditor::importTarget (const juce::File& file)
{
    auto curve = ms::TargetCurve::loadFromFile (file);

    if (! curve.has_value())
    {
        showMessage (file.getFileName() + ms::de (" ist keine gültige Zielkurve."), true);
        return;
    }

    // In den Zielordner übernehmen, damit es in der Liste auftaucht
    const auto folder = ms::TargetCurve::getUserTargetsFolder();
    const auto dest = folder.getChildFile (juce::File::createLegalFileName (curve->name.isNotEmpty() ? curve->name : file.getFileNameWithoutExtension()) + ".json");

    if (dest != file)
        curve->saveToFile (dest);

    curve->name = dest.getFileNameWithoutExtension();
    proc.setTarget (std::make_shared<const ms::TargetCurve> (std::move (*curve)));
    showMessage (ms::de ("Zielkurve importiert: ") + dest.getFileNameWithoutExtension(), false);
}

void TonalBalanceEditor::showMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    messageTicks = isError ? 30 * 8 : 30 * 4;
    repaint (footerArea);
}

//==============================================================================
bool TonalBalanceEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (ms::isSupportedAudioFile (juce::File (f)) || juce::File (f).hasFileExtension ("json"))
            return true;

    return false;
}

void TonalBalanceEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    dragIsJson = files.size() == 1 && juce::File (files[0]).hasFileExtension ("json");
    dragActive = true;
    repaint();
}

void TonalBalanceEditor::fileDragExit (const juce::StringArray&)
{
    dragActive = false;
    repaint();
}

void TonalBalanceEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragActive = false;
    repaint();

    // Eine exportierte Zielkurve (.json, z. B. aus der Browser-Version) → importieren
    for (const auto& f : files)
    {
        const juce::File file (f);

        if (! file.hasFileExtension ("json"))
            continue;

        importTarget (file);
        return;
    }

    juce::Array<juce::File> list;

    for (const auto& f : files)
        list.add (juce::File (f));

    startAnalysis (list);
}

//==============================================================================
void TonalBalanceEditor::paint (juce::Graphics& g)
{
    g.fillAll (ms::Colours::background);

    // Kopfzeile
    g.setColour (ms::Colours::outline.withAlpha (0.6f));
    g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) getWidth());
    g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) getWidth());

    auto title = headerArea.reduced (16, 0).removeFromLeft (170);
    g.setColour (ms::Colours::text);
    g.setFont (juce::FontOptions (15.0f).withStyle ("Bold").withKerningFactor (0.14f));
    g.drawText ("LEITSTERN", title.removeFromTop (headerArea.getHeight() / 2 + 4).withTrimmedTop (6), juce::Justification::bottomLeft);
    g.setColour (ms::Colours::textFaint);
    g.setFont (juce::FontOptions (11.0f).withKerningFactor (0.08f));
    g.drawText (ms::de ("NOVA · Tonal Balance"), title, juce::Justification::topLeft);

    // Lautheit rechts oben
    auto loud = headerArea.reduced (16, 0).removeFromRight (190);
    const bool integrated = proc.view.showIntegrated.load();
    g.setColour (hasLiveData ? ms::Colours::text : ms::Colours::textFaint);
    g.setFont (juce::FontOptions (17.0f).withStyle ("Bold"));
    g.drawText (formatLufs (hasLiveData ? shownLoudness : -100.0f), loud.removeFromTop (headerArea.getHeight() / 2 + 4).withTrimmedTop (6),
                juce::Justification::bottomRight);
    g.setColour (ms::Colours::textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (integrated ? ms::de ("Gesamt · ") + formatDuration (integratedSeconds) : ms::de ("Kurzzeit (3 s)"),
                loud, juce::Justification::topRight);

    // Status rechts unten
    auto status = statusArea;

    if (proc.getAnalysisJob().isBusy())
    {
        auto bar = status.removeFromBottom (6).toFloat().withTrimmedBottom (2.0f);
        g.setColour (ms::Colours::panelRaised);
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (ms::Colours::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, proc.getAnalysisJob().getProgress())), 2.0f);

        g.setColour (ms::Colours::text);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (proc.getAnalysisJob().getStatusText(), status, juce::Justification::centredRight, true);
    }
    else if (message.isNotEmpty())
    {
        g.setColour (messageIsError ? ms::Colours::warn : ms::Colours::ok);
        g.setFont (juce::FontOptions (12.5f));
        g.drawFittedText (message, status, juce::Justification::centredRight, 2, 0.9f);
    }
    else if (const auto target = proc.getTarget())
    {
        juce::String text = ms::de ("Ziel: ") + target->name;

        if (target->sources.size() > 0)
            text << ms::de (" · ") << target->sources.size() << (target->sources.size() == 1 ? " Track" : " Tracks");

        if (target->loudness.isValid())
            text << ms::de (" · Referenz ") << formatLufs (target->loudness.p50);

        g.setColour (ms::Colours::textDim);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (text, status, juce::Justification::centredRight, true);
    }
    else
    {
        g.setColour (ms::Colours::textFaint);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (ms::de ("Tipp: Referenztracks einfach ins Fenster ziehen"), status, juce::Justification::centredRight, true);
    }
}

void TonalBalanceEditor::paintOverChildren (juce::Graphics& g)
{
    if (! dragActive)
        return;

    auto area = getLocalBounds().toFloat().reduced (10.0f);
    g.setColour (ms::Colours::background.withAlpha (0.82f));
    g.fillRoundedRectangle (area, 10.0f);

    juce::Path outline;
    outline.addRoundedRectangle (area, 10.0f);
    juce::Path dashed;
    const float dashes[] = { 8.0f, 6.0f };
    juce::PathStrokeType (2.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (ms::Colours::accent);
    g.fillPath (dashed);

    g.setColour (ms::Colours::text);
    g.setFont (juce::FontOptions (20.0f));
    g.drawText (dragIsJson ? ms::de ("Loslassen, um diese Zielkurve zu importieren")
                           : ms::de ("Loslassen, um eine Zielkurve aus diesen Tracks zu bauen"),
                area, juce::Justification::centred);
}

void TonalBalanceEditor::resized()
{
    auto bounds = getLocalBounds();

    headerArea = bounds.removeFromTop (54);
    footerArea = bounds.removeFromBottom (50);

    // Kopfzeile
    {
        auto h = headerArea.reduced (16, 13);
        h.removeFromLeft (170);
        targetBox.setBounds (h.removeFromLeft (250));
        h.removeFromLeft (8);
        loadButton.setBounds (h.removeFromLeft (156));
        h.removeFromLeft (8);
        saveButton.setBounds (h.removeFromLeft (132));
    }

    // Fußzeile
    {
        auto f = footerArea.reduced (16, 12);
        shortTermButton.setBounds (f.removeFromLeft (78));
        integratedButton.setBounds (f.removeFromLeft (70));
        f.removeFromLeft (10);
        wideButton.setBounds (f.removeFromLeft (56));
        narrowButton.setBounds (f.removeFromLeft (56));
        f.removeFromLeft (10);
        tiltBox.setBounds (f.removeFromLeft (160));
        f.removeFromLeft (10);
        resetButton.setBounds (f.removeFromLeft (60));
        f.removeFromLeft (16);

        cancelButton.setBounds (f.removeFromRight (92));
        f.removeFromRight (10);
        statusArea = f;
    }

    // Inhalt
    auto content = bounds.reduced (14, 12);
    regionMeters.setBounds (content.removeFromLeft (juce::jlimit (270, 340, juce::roundToInt ((float) content.getWidth() * 0.28f))));
    content.removeFromLeft (12);
    curveView.setBounds (content);
}
