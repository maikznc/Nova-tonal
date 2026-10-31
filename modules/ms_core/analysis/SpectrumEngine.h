#pragma once

namespace ms
{

//==============================================================================
/** Zerlegt einen Audiostrom in überlappende FFT-Frames und fasst die Leistung
    pro 1/6-Oktav-Band zusammen.

    - FFT-Größe passt sich der Samplerate an (~3 Hz Auflösung, z. B. 16384 @ 48 kHz),
      damit auch die untersten Bänder um 20–40 Hz sauber aufgelöst werden.
    - 75 % Überlappung, Hann-Fenster.
    - Bandleistung ist kalibriert als mittleres Quadrat (Summe über alle Bänder ≈
      Signalleistung), Kanäle werden leistungsaddiert (wie in BS.1770).

    Nicht für den Audio-Thread gedacht: läuft im Analyse-Thread bzw. offline.
*/
class SpectrumEngine
{
public:
    static constexpr int maxChannels = 2;

    void prepare (double sampleRate);
    void reset() noexcept;

    using FrameCallback = std::function<void (const BandPowers&)>;

    /** Füttert Samples nach; onFrame wird für jeden fertigen Frame aufgerufen. */
    void process (const float* const* channels, int numChannels, int numSamples, const FrameCallback& onFrame);

    double getSampleRate() const noexcept   { return sampleRate; }
    int getFftSize() const noexcept         { return fftSize; }
    int getHopSize() const noexcept         { return hopSize; }
    double getHopSeconds() const noexcept   { return hopSize / sampleRate; }

    /** Bänder, die bei dieser Samplerate gemessen werden können (unterhalb Nyquist). */
    const BandFlags& getValidBands() const noexcept { return validBands; }

private:
    void computeFrame (int numChannels, const FrameCallback& onFrame);

    struct BinWeight { int bin; float weight; };

    double sampleRate = 0.0;
    int fftOrder = 14, fftSize = 1 << 14, hopSize = 1 << 12;
    int writeIndex = 0, samplesUntilFrame = 0, channelsInUse = 1;
    double powerScale = 1.0;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, fftData;
    std::array<std::vector<float>, maxChannels> history;
    std::array<std::vector<BinWeight>, BandLayout::numBands> bandBins;
    BandFlags validBands {};
    BandPowers bandPower {};
};

//==============================================================================
/** Gleitender Mittelwert über die letzten N Sekunden (Standard 3 s – entspricht
    dem "Short-Term"-Fenster der Lautheitsmessung). Wird live und offline gleich
    verwendet, damit Live-Kurve und Zielkurve vergleichbar sind. */
class SpectrumIntegrator
{
public:
    void prepare (double hopSeconds, double windowSeconds = 3.0);
    void reset() noexcept;
    void push (const BandPowers& frame) noexcept;

    bool isFull() const noexcept        { return count >= (int) ring.size(); }
    int getNumFrames() const noexcept   { return count; }
    int getCapacity() const noexcept    { return (int) ring.size(); }
    BandPowers getAverage() const noexcept;

private:
    std::vector<BandPowers> ring;
    int writePos = 0, count = 0;
};

//==============================================================================
/** Langzeit-Mittelwert über alles seit dem letzten Reset ("Gesamt"-Ansicht). */
class SpectrumAccumulator
{
public:
    void reset() noexcept               { sum.fill (0.0); frames = 0; }
    void push (const BandPowers& frame) noexcept;
    int getNumFrames() const noexcept   { return frames; }
    BandPowers getAverage() const noexcept;

private:
    BandPowers sum {};
    int frames = 0;
};

} // namespace ms
