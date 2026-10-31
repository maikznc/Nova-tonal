/*  Tests für den Analyse-Kern und das Tonal-Balance-Plugin.

    MSTests                          → alle Tests
    MSTests --snapshot out.png       → zusätzlich Screenshot des Editors mit Beispieldaten
    MSTests --parity <ordner>        → schreibt parity.wav + parity_cpp.json (Abgleich mit der Web-Version)
    MSTests --import <ziel.json>     → prüft, ob eine Zielkurve aus der Web-Version gelesen wird
*/

#include <ms_core/ms_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <iostream>
#include <thread>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        // Literale kommen als UTF-8-Bytes an → für die Konsole unverändert durchreichen
        std::string text;
        for (auto c : what)
            text += (char) (c < 256 ? c : '?');

        std::cout << (condition ? "  ok    " : "  FAIL  ") << text << std::endl;

        if (! condition)
            ++failures;
    }

    void sleepMs (int ms) { std::this_thread::sleep_for (std::chrono::milliseconds (ms)); }

    //==============================================================================
    /** Rosa Rauschen nach Paul Kellet – gleicher Zufallsgenerator wie in der Web-Version. */
    struct PinkNoise
    {
        explicit PinkNoise (uint32_t seed) : state (seed) {}

        float white()
        {
            state = state * 1664525u + 1013904223u;
            return (float) ((double) state / 4294967296.0 * 2.0 - 1.0);
        }

        float next()
        {
            const float w = white();
            b0 = 0.99886f * b0 + w * 0.0555179f;
            b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f;
            b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f;
            b5 = -0.7616f * b5 - w * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return pink * 0.11f;
        }

        uint32_t state;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    };

    struct Shape
    {
        float lowShelfDb = 0.0f, lowShelfHz = 100.0f;
        float peakDb = 0.0f, peakHz = 2500.0f, peakQ = 0.9f;
        float highShelfDb = 0.0f, highShelfHz = 5000.0f;
        float airDb = 0.0f, airHz = 12000.0f;
    };

    juce::AudioBuffer<float> makeSignal (double sr, double seconds, uint32_t seed, Shape shape = {}, float gain = 0.25f)
    {
        const int n = (int) (sr * seconds);
        juce::AudioBuffer<float> buffer (2, n);

        for (int ch = 0; ch < 2; ++ch)
        {
            PinkNoise pink (seed * 7919u + (uint32_t) ch);
            auto* d = buffer.getWritePointer (ch);

            for (int i = 0; i < n; ++i)
                d[i] = pink.next() * gain;

            auto apply = [&] (juce::IIRCoefficients c)
            {
                juce::IIRFilter f;
                f.setCoefficients (c);
                f.processSamples (d, n);
            };

            auto g = [] (float db) { return juce::Decibels::decibelsToGain (db); };

            if (shape.lowShelfDb != 0.0f)  apply (juce::IIRCoefficients::makeLowShelf   (sr, shape.lowShelfHz, 0.7, g (shape.lowShelfDb)));
            if (shape.peakDb != 0.0f)      apply (juce::IIRCoefficients::makePeakFilter (sr, shape.peakHz, shape.peakQ, g (shape.peakDb)));
            if (shape.highShelfDb != 0.0f) apply (juce::IIRCoefficients::makeHighShelf  (sr, shape.highShelfHz, 0.7, g (shape.highShelfDb)));
            if (shape.airDb != 0.0f)       apply (juce::IIRCoefficients::makeHighShelf  (sr, shape.airHz, 0.7, g (shape.airDb)));
        }

        return buffer;
    }

    ms::SpectrumSnapshot measureIntegrated (const juce::AudioBuffer<float>& buffer, double sr)
    {
        ms::SpectrumEngine engine;
        engine.prepare (sr);
        ms::SpectrumAccumulator acc;
        engine.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples(),
                        [&] (const ms::BandPowers& p) { acc.push (p); });
        return ms::SpectrumSnapshot::fromBandPowers (acc.getAverage(), engine.getValidBands());
    }

    juce::File writeWav (const juce::File& folder, const juce::String& name, const juce::AudioBuffer<float>& buffer, double sr)
    {
        folder.createDirectory();
        auto file = folder.getChildFile (name + ".wav");
        file.deleteFile();

        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (file);
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (sr)
                                                       .withNumChannels (buffer.getNumChannels())
                                                       .withBitsPerSample (24));
        if (writer != nullptr)
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

        return file;
    }

    bool inRange (int band) { const auto f = ms::BandLayout::getCentre (band); return f >= 31.0 && f <= 16000.0; }

    juce::File tempFolder() { return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ms_tests"); }

    ms::TargetCurve buildFromFiles (const juce::Array<juce::File>& files, const juce::String& name)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        ms::TargetBuilder builder;

        for (const auto& f : files)
            ms::analyseReferenceFile (formats, f, builder);

        return builder.build (name);
    }

    /** Spielt ein Signal blockweise durch den Prozessor, ohne den FIFO zu überfüllen. */
    void feedProcessor (TonalBalanceProcessor& proc, const juce::AudioBuffer<float>& signal, int blockSize = 512)
    {
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, blockSize);
        int sinceSleep = 0;

        for (int pos = 0; pos + blockSize <= signal.getNumSamples(); pos += blockSize)
        {
            for (int ch = 0; ch < 2; ++ch)
                block.copyFrom (ch, 0, signal, ch, pos, blockSize);

            proc.processBlock (block, midi);

            if ((sinceSleep += blockSize) >= 22050)   // alle ~0,5 s Audio kurz Luft lassen
            {
                sinceSleep = 0;
                sleepMs (15);
            }
        }

        sleepMs (300);
    }

    //==============================================================================
    void testPinkIsFlat()
    {
        std::cout << "\nRosa Rauschen → flache Kurve, Pegel-Unabhängigkeit\n";
        constexpr double sr = 44100.0;
        const auto signal = makeSignal (sr, 30.0, 1);
        const auto s = measureIntegrated (signal, sr);

        float lo = 1000.0f, hi = -1000.0f, sum = 0.0f;
        int count = 0;

        for (int b = 0; b < ms::BandLayout::numBands; ++b)
            if (inRange (b))
            {
                lo = juce::jmin (lo, s.bands[(size_t) b]);
                hi = juce::jmax (hi, s.bands[(size_t) b]);
                sum += s.bands[(size_t) b];
                ++count;
            }

        const float mean = sum / (float) count;
        std::cout << "        31 Hz-16 kHz: min " << lo << " / max " << hi << " / Mittel " << mean << " dB\n";
        check (s.valid, "Snapshot gültig");
        check (hi - lo < 2.0f, "Spektrum flach (Spanne < 2 dB)");
        check (std::abs (mean) < 1.5f, "rosa Rauschen liegt bei ~0 dB");

        auto quiet = signal;
        quiet.applyGain (0.25f);
        const auto q = measureIntegrated (quiet, sr);
        float maxDiff = 0.0f;

        for (int b = 0; b < ms::BandLayout::numBands; ++b)
            if (std::isfinite (s.bands[(size_t) b]))
                maxDiff = juce::jmax (maxDiff, std::abs (s.bands[(size_t) b] - q.bands[(size_t) b]));

        check (maxDiff < 0.01f, "Kurve pegelunabhängig (max. " + juce::String (maxDiff, 4) + " dB)");
        check (std::abs ((s.loudness - q.loudness) - 12.04f) < 0.05f, "Lautheit sinkt um 12,04 dB");
    }

    void testSineLoudness()
    {
        std::cout << "\nSinus 1 kHz, 0 dBFS stereo → ~0 LUFS (BS.1770-Referenz)\n";
        constexpr double sr = 48000.0;
        juce::AudioBuffer<float> sine (2, (int) sr * 6);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < sine.getNumSamples(); ++i)
                sine.setSample (ch, i, (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * i / sr));

        const auto s = measureIntegrated (sine, sr);
        int peak = 0;

        for (int b = 1; b < ms::BandLayout::numBands; ++b)
            if (s.bands[(size_t) b] > s.bands[(size_t) peak])
                peak = b;

        std::cout << "        Lautheit " << s.loudness << " LUFS, Spitze bei Band " << peak << "\n";
        check (std::abs (s.loudness) < 0.3f, "Lautheit ~0 LUFS");
        check (ms::BandLayout::getLowerEdge (peak) <= 1000.0 && ms::BandLayout::getUpperEdge (peak) > 1000.0, "Energie im richtigen Band");
    }

    void testReferenceFiles()
    {
        std::cout << "\nReferenzdateien → Zielkurve (echter Datei-Pfad über WAV)\n";
        constexpr double sr = 44100.0;
        const auto folder = tempFolder();

        Shape bass;
        bass.lowShelfDb = 8.0f;
        bass.lowShelfHz = 150.0f;

        const auto pinkFile = writeWav (folder, "rosa", makeSignal (sr, 20.0, 11), sr);
        const auto bassFile = writeWav (folder, "bass", makeSignal (sr, 60.0, 13, bass), sr);

        const auto onlyPink = buildFromFiles ({ pinkFile }, "A");
        const auto onlyBass = buildFromFiles ({ bassFile }, "B");
        const auto both = buildFromFiles ({ pinkFile, bassFile }, "Beide");

        const int band = 9;   // ~57 Hz
        const auto pa = onlyPink.bands[band].p50, pb = onlyBass.bands[band].p50;
        const auto& p = both.bands[band];
        std::cout << "        57 Hz: rosa " << pa << ", Bass-Track " << pb << " → Ziel p10 " << p.p10 << " / p50 " << p.p50 << " / p90 " << p.p90 << "\n";

        check (both.isValid() && both.sources.size() == 2, "Zielkurve aus zwei Dateien gebaut");
        check (p.p50 > pa + 1.0f && p.p50 < pb - 1.0f, "Median zwischen den Tracks (jeder Track zählt gleich, egal wie lang)");
        check (std::abs (p.p10 - pa) < 1.5f && std::abs (p.p90 - pb) < 1.5f, "Streubereich deckt beide Tracks ab");

        bool ordered = true;
        for (const auto& b : both.bands)
            if (b.isValid() && ! (b.p10 <= b.p25 && b.p25 <= b.p50 && b.p50 <= b.p75 && b.p75 <= b.p90))
                ordered = false;
        check (ordered, "Perzentile geordnet");

        // JSON hin und zurück
        const auto back = ms::TargetCurve::fromJson (both.toJson());
        bool same = back.has_value();
        if (same)
            for (size_t b = 0; b < both.bands.size(); ++b)
                if (both.bands[b].isValid() && std::abs (back->bands[b].p50 - both.bands[b].p50) > 0.006f)
                    same = false;
        check (same, "JSON hin und zurück verlustfrei");

        // Stille
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        ms::TargetBuilder builder;
        juce::AudioBuffer<float> silence (2, (int) sr * 5);
        silence.clear();
        writeWav (folder, "stille", silence, sr);
        const auto r = ms::analyseReferenceFile (formats, folder.getChildFile ("stille.wav"), builder);
        check (r.failed() && builder.getNumTracks() == 0, "Stille Datei → Fehlermeldung: " + r.getErrorMessage());

        const auto bogus = folder.getChildFile ("kaputt.wav");
        bogus.replaceWithText ("keine Audiodatei");
        check (ms::analyseReferenceFile (formats, bogus, builder).failed(), "Kaputte Datei → Fehlermeldung");
    }

    void testReferenceJob()
    {
        std::cout << "\nHintergrund-Analyse (Thread)\n";
        const auto folder = tempFolder();
        ms::ReferenceAnalysisJob job;
        std::atomic<bool> done { false };
        ms::ReferenceAnalysisJob::Result result;

        const bool started = job.start ({ folder.getChildFile ("rosa.wav"), folder.getChildFile ("kaputt.wav") }, "Job-Test",
                                        [&] (ms::ReferenceAnalysisJob::Result r) { result = std::move (r); done = true; });
        check (started, "Analyse gestartet");
        check (! job.start ({ folder.getChildFile ("rosa.wav") }, "zweiter", {}), "zweite Analyse während der ersten wird abgelehnt");

        for (int i = 0; i < 200 && ! done; ++i)
            sleepMs (20);

        check (done && result.curve.has_value(), "Ergebnis kommt an");
        check (result.errors.size() == 1, "kaputte Datei als Fehler gemeldet, die gute trotzdem verwendet");
    }

    void testProcessor()
    {
        std::cout << "\nPlugin: Live-Pfad, Zustand speichern/laden\n";
        constexpr double sr = 48000.0;
        const auto folder = tempFolder();

        TonalBalanceProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);

        // Ziel aus rosa Rauschen, Live-Signal ebenfalls rosa (anderer Zufall) → muss im Ziel liegen
        auto target = buildFromFiles ({ writeWav (folder, "rosa48", makeSignal (sr, 20.0, 21), sr) }, "Rosa");
        proc.setTarget (std::make_shared<const ms::TargetCurve> (target));

        const auto live = makeSignal (sr, 6.0, 99, {}, 0.1f);
        juce::AudioBuffer<float> copy (live);
        feedProcessor (proc, live);

        const auto results = proc.getLiveAnalyser().getResults();
        check (results.shortTerm.valid, "Live-Kurzzeitspektrum vorhanden");
        check (results.integrated.valid && results.integratedSeconds > 4.0, "Gesamtmessung läuft (" + juce::String (results.integratedSeconds, 1) + " s)");

        // Live-Pfad (Plugin) und Datei-Pfad (Referenz) müssen dasselbe messen:
        // gleiches Signal → Abstand zum Ziel-Median nur so groß wie die Zufallsstreuung des Rauschens
        float worst = 0.0f;
        for (int r = 0; r < ms::numRegions; ++r)
            worst = juce::jmax (worst, std::abs (results.shortTerm.regions[(size_t) r] - target.regions[(size_t) r].p50));
        check (worst < 0.75f, "Live-Messung = Datei-Messung (größter Abstand " + juce::String (worst, 2) + " dB)");

        // Audio muss unverändert durchlaufen
        juce::AudioBuffer<float> block (2, 512);
        block.copyFrom (0, 0, copy, 0, 0, 512);
        block.copyFrom (1, 0, copy, 1, 0, 512);
        juce::MidiBuffer midi;
        proc.processBlock (block, midi);
        float diff = 0.0f;
        for (int i = 0; i < 512; ++i)
            diff = juce::jmax (diff, std::abs (block.getSample (0, i) - copy.getSample (0, i)));
        check (diff == 0.0f, "Audio läuft bitgenau durch");

        // Zustand
        proc.view.narrowRange = true;
        proc.view.showIntegrated = true;
        proc.view.tiltDbPerOct = 3.0f;
        juce::MemoryBlock state;
        proc.getStateInformation (state);

        TonalBalanceProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto t = restored.getTarget();
        check (t != nullptr && t->name == "Rosa" && std::abs (t->bands[30].p50 - target.bands[30].p50) < 0.01f, "Zielkurve wird mit dem Projekt gespeichert");
        check (restored.view.narrowRange && restored.view.showIntegrated && restored.view.tiltDbPerOct == 3.0f, "Ansicht wird mit dem Projekt gespeichert");

        juce::AudioProcessor::BusesLayout mono, surround;
        mono.inputBuses.add (juce::AudioChannelSet::mono());   mono.outputBuses.add (juce::AudioChannelSet::mono());
        surround.inputBuses.add (juce::AudioChannelSet::create5point1()); surround.outputBuses.add (juce::AudioChannelSet::create5point1());
        check (proc.isBusesLayoutSupported (mono) && ! proc.isBusesLayoutSupported (surround), "Mono und Stereo erlaubt, Surround nicht");
    }

    //==============================================================================
    void writeParity (const juce::File& folder)
    {
        // Gleiches Signal für C++ und Web-Version → Zielkurven müssen übereinstimmen
        constexpr double sr = 44100.0;
        Shape s;
        s.lowShelfDb = 5.0f; s.highShelfDb = -7.0f; s.airDb = -5.0f;
        const auto file = writeWav (folder, "parity", makeSignal (sr, 25.0, 5, s), sr);
        const auto curve = buildFromFiles ({ file }, "parity");
        folder.getChildFile ("parity_cpp.json").replaceWithText (curve.toJson());
        std::cout << "\nParität: " << file.getFullPathName() << " + parity_cpp.json geschrieben\n";
    }

    void testImport (const juce::File& file)
    {
        std::cout << "\nZielkurve aus der Web-Version lesen\n";
        const auto t = ms::TargetCurve::loadFromFile (file);
        check (t.has_value(), "Web-Export wird vom Plugin gelesen (" + file.getFileName() + ")");

        if (t)
            check (t->isValid() && t->bands[30].isValid(), "Werte plausibel (Median 1 kHz-Band: " + juce::String (t->bands[30].p50, 2) + " dB)");
    }

    void makeSnapshot (const juce::File& out)
    {
        std::cout << "\nScreenshot des Editors\n";
        constexpr double sr = 44100.0;
        const auto folder = tempFolder();

        // Drei "Referenzen" mit typischer Musik-Neigung + Abweichungen
        juce::Array<juce::File> refs;
        const float lows[] = { 5.5f, 4.0f, 6.5f }, highs[] = { -8.0f, -9.5f, -7.0f }, airs[] = { -6.0f, -4.0f, -8.0f };

        for (int i = 0; i < 3; ++i)
        {
            Shape s;
            s.lowShelfDb = lows[i]; s.highShelfDb = highs[i]; s.airDb = airs[i];
            refs.add (writeWav (folder, "ref" + juce::String (i), makeSignal (sr, 15.0, 100 + (uint32_t) i, s), sr));
        }

        TonalBalanceProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        proc.setTarget (std::make_shared<const ms::TargetCurve> (buildFromFiles (refs, "Club-Referenzen")));

        Shape mix;
        mix.lowShelfDb = 1.5f; mix.peakDb = 6.0f; mix.peakHz = 2600.0f; mix.highShelfDb = -8.0f; mix.airDb = -6.0f;
        feedProcessor (proc, makeSignal (sr, 5.0, 4242, mix, 0.3f));

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto* tb = dynamic_cast<TonalBalanceEditor*> (editor.get());

        for (int i = 0; i < 20; ++i)   // Glättung einschwingen lassen (Daten gelten als frisch)
            tb->refresh();

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat png;
        check (stream.openedOk() && png.writeImageToStream (image, stream), "PNG geschrieben: " + out.getFullPathName());
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String snapshotPath, parityFolder, importPath;

    for (int i = 1; i + 1 < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--snapshot") snapshotPath = argv[i + 1];
        if (arg == "--parity")   parityFolder = argv[i + 1];
        if (arg == "--import")   importPath = argv[i + 1];
    }

    testPinkIsFlat();
    testSineLoudness();
    testReferenceFiles();
    testReferenceJob();
    testProcessor();

    if (parityFolder.isNotEmpty())  writeParity (juce::File (parityFolder));
    if (importPath.isNotEmpty())    testImport (juce::File (importPath));
    if (snapshotPath.isNotEmpty())  makeSnapshot (juce::File (snapshotPath));

    std::cout << (failures == 0 ? "\nAlle Tests bestanden\n" : "\n" + std::to_string (failures) + " Test(s) fehlgeschlagen\n");
    return failures == 0 ? 0 : 1;
}
