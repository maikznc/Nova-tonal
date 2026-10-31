namespace ms
{

void SpectrumEngine::prepare (double newSampleRate)
{
    jassert (newSampleRate > 0.0);
    sampleRate = newSampleRate;

    // ~3 Hz Auflösung: 16384 @ 44,1/48 kHz, 32768 @ 88,2/96 kHz …
    fftOrder = juce::jlimit (12, 17, (int) std::ceil (std::log2 (sampleRate / 3.0)));
    fftSize  = 1 << fftOrder;
    hopSize  = fftSize / 4;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    fftData.assign ((size_t) fftSize * 2, 0.0f);

    window.resize ((size_t) fftSize);
    double windowPower = 0.0;

    for (int i = 0; i < fftSize; ++i)
    {
        const auto w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * i / fftSize); // periodisches Hann
        window[(size_t) i] = (float) w;
        windowPower += w * w;
    }

    // Einseitiges Leistungsspektrum, so skaliert, dass Σ Bänder ≈ mittleres Quadrat des Signals
    powerScale = 2.0 / ((double) fftSize * windowPower);

    // Bins anteilig auf Bänder verteilen (wichtig für die schmalen Bassbänder)
    const double binWidth = sampleRate / fftSize;
    const double nyquist  = sampleRate * 0.5;
    const int lastBin     = fftSize / 2 - 1;

    for (int b = 0; b < BandLayout::numBands; ++b)
    {
        auto& bins = bandBins[(size_t) b];
        bins.clear();

        const double lo = BandLayout::getLowerEdge (b);
        const double hi = BandLayout::getUpperEdge (b);

        if (hi > nyquist)
        {
            validBands[(size_t) b] = false;
            continue;
        }

        const int firstK = juce::jmax (1, (int) std::floor (lo / binWidth));
        const int lastK  = juce::jmin (lastBin, (int) std::ceil (hi / binWidth));

        for (int k = firstK; k <= lastK; ++k)
        {
            const double binLo = (k - 0.5) * binWidth;
            const double binHi = (k + 0.5) * binWidth;
            const double overlap = std::min (hi, binHi) - std::max (lo, binLo);

            if (overlap > 0.0)
                bins.push_back ({ k, (float) (overlap / binWidth) });
        }

        validBands[(size_t) b] = ! bins.empty();
    }

    for (auto& h : history)
        h.assign ((size_t) fftSize, 0.0f);

    reset();
}

void SpectrumEngine::reset() noexcept
{
    for (auto& h : history)
        std::fill (h.begin(), h.end(), 0.0f);

    writeIndex = 0;
    samplesUntilFrame = fftSize;   // erster Frame erst, wenn das Fenster voll ist
    bandPower.fill (0.0);
}

void SpectrumEngine::process (const float* const* channels, int numChannels, int numSamples, const FrameCallback& onFrame)
{
    jassert (fft != nullptr);
    const int chans = juce::jlimit (0, maxChannels, numChannels);

    if (chans == 0 || numSamples <= 0)
        return;

    channelsInUse = chans;
    const int mask = fftSize - 1;
    int pos = 0;

    while (pos < numSamples)
    {
        const int todo = juce::jmin (numSamples - pos, samplesUntilFrame);

        for (int c = 0; c < chans; ++c)
        {
            auto* dest = history[(size_t) c].data();
            const auto* src = channels[c] + pos;

            for (int i = 0; i < todo; ++i)
                dest[(writeIndex + i) & mask] = src[i];
        }

        writeIndex = (writeIndex + todo) & mask;
        samplesUntilFrame -= todo;
        pos += todo;

        if (samplesUntilFrame == 0)
        {
            computeFrame (chans, onFrame);
            samplesUntilFrame = hopSize;
        }
    }
}

void SpectrumEngine::computeFrame (int numChannels, const FrameCallback& onFrame)
{
    bandPower.fill (0.0);
    const int mask = fftSize - 1;

    for (int c = 0; c < numChannels; ++c)
    {
        const auto* h = history[(size_t) c].data();

        // writeIndex zeigt auf das älteste Sample im Ringpuffer
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = h[(writeIndex + i) & mask] * window[(size_t) i];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int b = 0; b < BandLayout::numBands; ++b)
        {
            double sum = 0.0;

            for (const auto& bw : bandBins[(size_t) b])
            {
                const double mag = fftData[(size_t) bw.bin];
                sum += bw.weight * mag * mag;
            }

            bandPower[(size_t) b] += sum * powerScale;
        }
    }

    if (onFrame)
        onFrame (bandPower);
}

//==============================================================================
void SpectrumIntegrator::prepare (double hopSeconds, double windowSeconds)
{
    const int frames = juce::jmax (1, juce::roundToInt (windowSeconds / hopSeconds));
    ring.assign ((size_t) frames, BandPowers {});
    reset();
}

void SpectrumIntegrator::reset() noexcept
{
    for (auto& r : ring)
        r.fill (0.0);

    writePos = 0;
    count = 0;
}

void SpectrumIntegrator::push (const BandPowers& frame) noexcept
{
    if (ring.empty())
        return;

    ring[(size_t) writePos] = frame;
    writePos = (writePos + 1) % (int) ring.size();
    count = juce::jmin (count + 1, (int) ring.size());
}

BandPowers SpectrumIntegrator::getAverage() const noexcept
{
    BandPowers avg {};

    if (count == 0)
        return avg;

    // Bei < voller Füllung liegen die gültigen Frames direkt vor writePos
    const int size = (int) ring.size();

    for (int i = 0; i < count; ++i)
    {
        const auto& frame = ring[(size_t) ((writePos - 1 - i + size) % size)];

        for (size_t b = 0; b < avg.size(); ++b)
            avg[b] += frame[b];
    }

    for (auto& v : avg)
        v /= count;

    return avg;
}

//==============================================================================
void SpectrumAccumulator::push (const BandPowers& frame) noexcept
{
    for (size_t b = 0; b < sum.size(); ++b)
        sum[b] += frame[b];

    ++frames;
}

BandPowers SpectrumAccumulator::getAverage() const noexcept
{
    BandPowers avg {};

    if (frames > 0)
        for (size_t b = 0; b < avg.size(); ++b)
            avg[b] = sum[b] / frames;

    return avg;
}

} // namespace ms
