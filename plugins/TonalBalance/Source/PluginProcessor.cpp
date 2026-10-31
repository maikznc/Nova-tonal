#include "PluginProcessor.h"
#include "PluginEditor.h"

TonalBalanceProcessor::TonalBalanceProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

TonalBalanceProcessor::~TonalBalanceProcessor()
{
    analysisJob.cancel();
}

void TonalBalanceProcessor::prepareToPlay (double sampleRate, int)
{
    liveAnalyser.prepare (sampleRate);
}

bool TonalBalanceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void TonalBalanceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();

    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    liveAnalyser.push (buffer, numIn);   // Audio bleibt unverändert
}

juce::AudioProcessorEditor* TonalBalanceProcessor::createEditor()
{
    return new TonalBalanceEditor (*this);
}

//==============================================================================
std::shared_ptr<const ms::TargetCurve> TonalBalanceProcessor::getTarget() const
{
    const std::scoped_lock lock (targetLock);
    return target;
}

void TonalBalanceProcessor::setTarget (std::shared_ptr<const ms::TargetCurve> newTarget)
{
    {
        const std::scoped_lock lock (targetLock);
        target = std::move (newTarget);
    }

    ++targetVersion;
}

bool TonalBalanceProcessor::analyseReferences (const juce::Array<juce::File>& files)
{
    juce::Array<juce::File> audioFiles;

    for (const auto& f : files)
        if (ms::isSupportedAudioFile (f))
            audioFiles.add (f);

    if (audioFiles.isEmpty())
    {
        const std::scoped_lock lock (messagesLock);
        analysisMessages = { ms::de ("Keine unterstützten Audiodateien gefunden.") };
        return false;
    }

    const auto name = audioFiles.size() == 1 ? audioFiles.getFirst().getFileNameWithoutExtension()
                                             : ms::de ("Referenzen (") + juce::String (audioFiles.size()) + " Tracks)";

    return analysisJob.start (audioFiles, name, [this] (ms::ReferenceAnalysisJob::Result result)
    {
        if (result.curve.has_value())
            setTarget (std::make_shared<const ms::TargetCurve> (std::move (*result.curve)));

        const std::scoped_lock lock (messagesLock);
        analysisMessages = result.errors;
    });
}

juce::StringArray TonalBalanceProcessor::takeAnalysisMessages()
{
    const std::scoped_lock lock (messagesLock);
    auto messages = analysisMessages;
    analysisMessages.clear();
    return messages;
}

//==============================================================================
void TonalBalanceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("TonalBalanceState");
    state.setProperty ("version", 1, nullptr);
    state.setProperty ("showIntegrated", view.showIntegrated.load(), nullptr);
    state.setProperty ("narrowRange", view.narrowRange.load(), nullptr);
    state.setProperty ("tilt", view.tiltDbPerOct.load(), nullptr);

    if (auto t = getTarget())
        state.setProperty ("target", t->toJson(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TonalBalanceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.hasType ("TonalBalanceState"))
        return;

    view.showIntegrated = (bool) state.getProperty ("showIntegrated", false);
    view.narrowRange    = (bool) state.getProperty ("narrowRange", false);
    view.tiltDbPerOct   = (float) state.getProperty ("tilt", 0.0f);

    std::shared_ptr<const ms::TargetCurve> restored;

    if (state.hasProperty ("target"))
        if (auto t = ms::TargetCurve::fromJson (state["target"].toString()))
            restored = std::make_shared<const ms::TargetCurve> (std::move (*t));

    setTarget (std::move (restored));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TonalBalanceProcessor();
}
