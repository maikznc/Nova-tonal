namespace ms
{

juce::String getSupportedAudioWildcard()
{
    return "*.wav;*.wave;*.aif;*.aiff;*.aifc;*.flac;*.mp3;*.m4a;*.aac;*.ogg;*.opus;*.caf";
}

bool isSupportedAudioFile (const juce::File& file)
{
    return file.existsAsFile() && file.hasFileExtension (getSupportedAudioWildcard().replace ("*", {}));
}

juce::Result analyseReferenceFile (juce::AudioFormatManager& formats,
                                   const juce::File& file,
                                   TargetBuilder& builder,
                                   const std::function<bool (double)>& shouldContinue)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
        return juce::Result::fail (de ("Format nicht lesbar: ") + file.getFileName());

    if (reader->sampleRate < 8000.0 || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        return juce::Result::fail (de ("Leere oder ungültige Datei: ") + file.getFileName());

    SpectrumEngine engine;
    engine.prepare (reader->sampleRate);

    SpectrumIntegrator integrator;
    integrator.prepare (engine.getHopSeconds());

    const int framesPerObservation = juce::jmax (1, juce::roundToInt (1.0 / engine.getHopSeconds()));
    int framesSinceFull = 0;
    int observations = 0;

    builder.beginTrack (file.getFileNameWithoutExtension());

    auto addObservation = [&]
    {
        builder.addObservation (SpectrumSnapshot::fromBandPowers (integrator.getAverage(), engine.getValidBands()));
        ++observations;
    };

    const auto onFrame = [&] (const BandPowers& frame)
    {
        integrator.push (frame);

        if (integrator.isFull() && (framesSinceFull++ % framesPerObservation) == 0)
            addObservation();
    };

    const int numChannels = juce::jmin (SpectrumEngine::maxChannels, (int) reader->numChannels);
    constexpr int blockSize = 1 << 15;
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    const auto length = reader->lengthInSamples;

    for (juce::int64 pos = 0; pos < length; pos += blockSize)
    {
        if (shouldContinue && ! shouldContinue ((double) pos / (double) length))
        {
            builder.cancelTrack();
            return juce::Result::fail (de ("Abgebrochen"));
        }

        const int n = (int) juce::jmin ((juce::int64) blockSize, length - pos);

        if (! reader->read (&buffer, 0, n, pos, true, numChannels > 1))
        {
            builder.cancelTrack();
            return juce::Result::fail (de ("Lesefehler in ") + file.getFileName());
        }

        engine.process (buffer.getArrayOfReadPointers(), numChannels, n, onFrame);
    }

    // Sehr kurze Dateien (< 3 s): wenigstens ein Fenster, wenn ≥ 1 s Material da ist
    if (observations == 0 && integrator.getNumFrames() >= framesPerObservation)
        addObservation();

    if (! builder.endTrack())
        return juce::Result::fail (de ("Keine verwertbaren Abschnitte (zu kurz oder still): ") + file.getFileName());

    return juce::Result::ok();
}

//==============================================================================
ReferenceAnalysisJob::ReferenceAnalysisJob()
    : juce::Thread ("MS Reference Analysis")
{
    formats.registerBasicFormats();
}

ReferenceAnalysisJob::~ReferenceAnalysisJob()
{
    stopThread (5000);
}

bool ReferenceAnalysisJob::start (const juce::Array<juce::File>& files, const juce::String& targetName, Callback onFinished)
{
    if (busy.load() || files.isEmpty())
        return false;

    stopThread (1000);   // vorherigen (fertigen) Lauf sauber beenden

    pendingFiles = files;
    pendingName = targetName;
    callback = std::move (onFinished);
    progress = 0.0f;
    busy = true;
    setStatus (de ("Starte Analyse …"));

    if (! startThread (juce::Thread::Priority::low))
    {
        busy = false;
        return false;
    }

    return true;
}

void ReferenceAnalysisJob::cancel()
{
    signalThreadShouldExit();
}

juce::String ReferenceAnalysisJob::getStatusText() const
{
    const std::scoped_lock lock (statusLock);
    return status;
}

void ReferenceAnalysisJob::setStatus (const juce::String& s)
{
    const std::scoped_lock lock (statusLock);
    status = s;
}

void ReferenceAnalysisJob::run()
{
    Result result;
    TargetBuilder builder;
    const int numFiles = pendingFiles.size();

    for (int i = 0; i < numFiles; ++i)
    {
        const auto& file = pendingFiles.getReference (i);
        setStatus (de ("Analysiere ") + juce::String (i + 1) + "/" + juce::String (numFiles) + ": " + file.getFileName());

        const auto r = analyseReferenceFile (formats, file, builder, [this, i, numFiles] (double p)
        {
            progress = (float) ((i + p) / numFiles);
            return ! threadShouldExit();
        });

        if (threadShouldExit())
        {
            result.cancelled = true;
            break;
        }

        if (r.failed())
            result.errors.add (r.getErrorMessage());
    }

    if (! result.cancelled)
    {
        if (builder.getNumTracks() > 0)
        {
            auto curve = builder.build (pendingName);

            if (curve.isValid())
                result.curve = std::move (curve);
            else
                result.errors.add (de ("Zu wenig verwertbare Daten für eine Zielkurve."));
        }

        progress = 1.0f;
    }

    setStatus ({});

    if (callback)
        callback (std::move (result));

    busy = false;
}

} // namespace ms
