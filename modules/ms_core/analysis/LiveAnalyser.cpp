namespace ms
{

LiveAnalyser::LiveAnalyser()
    : juce::Thread ("MS Live Analysis")
{
}

LiveAnalyser::~LiveAnalyser()
{
    stopThread (2000);
}

void LiveAnalyser::prepare (double sampleRate)
{
    stopThread (2000);

    engine.prepare (sampleRate);
    shortTerm.prepare (engine.getHopSeconds());
    integrated.reset();

    const int fifoSize = juce::jmax (8192, (int) (sampleRate * 2.0));   // 2 s Puffer
    fifo.setTotalSize (fifoSize);
    fifo.reset();
    fifoBuffer.setSize (numFifoChannels, fifoSize, false, true, false);
    scratch.setSize (numFifoChannels, 4096, false, true, false);

    {
        const std::scoped_lock lock (resultsLock);
        results = {};
    }

    resetRequested = false;
    startThread (juce::Thread::Priority::low);
}

void LiveAnalyser::push (const juce::AudioBuffer<float>& buffer, int numInputChannels) noexcept
{
    const int inputs = juce::jmin (numInputChannels, buffer.getNumChannels());
    const int n = buffer.getNumSamples();

    if (inputs <= 0 || n <= 0 || fifoBuffer.getNumSamples() == 0)
        return;

    const auto scope = fifo.write (n);   // schreibt nur so viel, wie frei ist

    for (int c = 0; c < numFifoChannels; ++c)
    {
        const auto* src = buffer.getReadPointer (juce::jmin (c, inputs - 1));   // Mono → beide Kanäle

        if (scope.blockSize1 > 0)
            juce::FloatVectorOperations::copy (fifoBuffer.getWritePointer (c, scope.startIndex1), src, scope.blockSize1);

        if (scope.blockSize2 > 0)
            juce::FloatVectorOperations::copy (fifoBuffer.getWritePointer (c, scope.startIndex2), src + scope.blockSize1, scope.blockSize2);
    }
}

LiveAnalyser::Results LiveAnalyser::getResults() const
{
    const std::scoped_lock lock (resultsLock);
    return results;
}

void LiveAnalyser::run()
{
    while (! threadShouldExit())
    {
        wait (20);

        if (resetRequested.exchange (false))
            integrated.reset();

        bool newFrames = false;

        const auto onFrame = [&] (const BandPowers& frame)
        {
            shortTerm.push (frame);

            if (SpectrumSnapshot::computeLoudness (frame, engine.getValidBands()) > SpectrumSnapshot::silenceThresholdLufs)
                integrated.push (frame);

            newFrames = true;
        };

        while (fifo.getNumReady() > 0 && ! threadShouldExit())
        {
            const int n = juce::jmin (fifo.getNumReady(), scratch.getNumSamples());

            {
                const auto scope = fifo.read (n);

                for (int c = 0; c < numFifoChannels; ++c)
                {
                    if (scope.blockSize1 > 0)
                        scratch.copyFrom (c, 0, fifoBuffer, c, scope.startIndex1, scope.blockSize1);

                    if (scope.blockSize2 > 0)
                        scratch.copyFrom (c, scope.blockSize1, fifoBuffer, c, scope.startIndex2, scope.blockSize2);
                }
            }

            engine.process (scratch.getArrayOfReadPointers(), numFifoChannels, n, onFrame);
        }

        if (! newFrames)
            continue;

        Results r;
        r.shortTerm = SpectrumSnapshot::fromBandPowers (shortTerm.getAverage(), engine.getValidBands());

        if (integrated.getNumFrames() > 0)
            r.integrated = SpectrumSnapshot::fromBandPowers (integrated.getAverage(), engine.getValidBands());

        r.integratedSeconds = integrated.getNumFrames() * engine.getHopSeconds();

        {
            const std::scoped_lock lock (resultsLock);
            results = r;
        }

        ++sequence;
    }
}

} // namespace ms
