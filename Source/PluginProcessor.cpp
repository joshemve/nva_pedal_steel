#include "PluginProcessor.h"
#include "PluginEditor.h"

PedalSteelAudioProcessor::PedalSteelAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    pPresetStyle   = apvts.getRawParameterValue(ParamIDs::presetStyle);
    pPlayMode      = apvts.getRawParameterValue(ParamIDs::playMode);
    pPedalA        = apvts.getRawParameterValue(ParamIDs::pedalA);
    pPedalB        = apvts.getRawParameterValue(ParamIDs::pedalB);
    pPedalC        = apvts.getRawParameterValue(ParamIDs::pedalC);
    pLeverLKL      = apvts.getRawParameterValue(ParamIDs::leverLKL);
    pLeverLKR      = apvts.getRawParameterValue(ParamIDs::leverLKR);
    pVolumePedal   = apvts.getRawParameterValue(ParamIDs::volumePedal);
    pScoopDepth    = apvts.getRawParameterValue(ParamIDs::scoopDepth);
    pPedalSpeed    = apvts.getRawParameterValue(ParamIDs::pedalSpeed);
    pBarGlide      = apvts.getRawParameterValue(ParamIDs::barGlide);
    pVibratoDepth  = apvts.getRawParameterValue(ParamIDs::vibratoDepth);
    pVibratoRate   = apvts.getRawParameterValue(ParamIDs::vibratoRate);
    pAutoSwell     = apvts.getRawParameterValue(ParamIDs::autoSwell);
    pSwellTime     = apvts.getRawParameterValue(ParamIDs::swellTime);
    pSampleBlend   = apvts.getRawParameterValue(ParamIDs::sampleBlend);
    pPickSnap      = apvts.getRawParameterValue(ParamIDs::pickSnap);
    pPickupPos     = apvts.getRawParameterValue(ParamIDs::pickupPos);
    pCoilPeakFreq  = apvts.getRawParameterValue(ParamIDs::coilPeakFreq);
    pCoilResonance = apvts.getRawParameterValue(ParamIDs::coilResonance);
    pSustainTime   = apvts.getRawParameterValue(ParamIDs::sustainTime);
    pCabinetDrop   = apvts.getRawParameterValue(ParamIDs::cabinetDrop);
    pAmpDrive      = apvts.getRawParameterValue(ParamIDs::ampDrive);
    pToneWarmth    = apvts.getRawParameterValue(ParamIDs::toneWarmth);
    pReverbMix     = apvts.getRawParameterValue(ParamIDs::reverbMix);
    pReverbDecay   = apvts.getRawParameterValue(ParamIDs::reverbDecay);
    pOutputLevel   = apvts.getRawParameterValue(ParamIDs::outputLevel);

    loadPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout PedalSteelAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamIDs::presetStyle, 1 }, "Steel Voice Style",
        juce::StringArray{ "Sweet Georgia E9", "Hedonist Bootgaze", "Mikey's Grit Tweed", "Montana Lush Swell" }, 0));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamIDs::playMode, 1 }, "Performance Mode",
        juce::StringArray{ "Smart Auto-Pedal E9", "Crying Lead Gliss", "Lush Swell Pad", "Manual Copedent" }, 0));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pedalA, 1 }, "Pedal A (B->C# +2)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pedalB, 1 }, "Pedal B (G#->A +1)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pedalC, 1 }, "Pedal C (E->F# +2)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::leverLKL, 1 }, "Knee LKL (E->F +1)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::leverLKR, 1 }, "Knee LKR (E->D# -1)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::volumePedal, 1 }, "Volume Pedal",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.92f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::scoopDepth, 1 }, "Auto Pedal Scoop",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.68f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pedalSpeed, 1 }, "Pedal Pull Speed",
        juce::NormalisableRange<float>(40.0f, 350.0f, 1.0f), 145.0f, "ms"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::barGlide, 1 }, "Tonebar Glide",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 95.0f, "ms"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::vibratoDepth, 1 }, "Bar Vibrato Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.40f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::vibratoRate, 1 }, "Bar Vibrato Rate",
        juce::NormalisableRange<float>(3.0f, 7.5f, 0.01f), 5.15f, "Hz"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::autoSwell, 1 }, "Auto Swell Bloom",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.18f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::swellTime, 1 }, "Swell Rise Time",
        juce::NormalisableRange<float>(30.0f, 800.0f, 1.0f), 160.0f, "ms"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::sampleBlend, 1 }, "Reference DNA Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pickSnap, 1 }, "Fingerpick & Bar Noise",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.58f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pickupPos, 1 }, "Pickup Distance",
        juce::NormalisableRange<float>(1.2f, 4.0f, 0.01f), 2.20f, "in"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::coilPeakFreq, 1 }, "Pickup Coil Peak",
        juce::NormalisableRange<float>(1400.0f, 3200.0f, 1.0f), 2120.0f, "Hz"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::coilResonance, 1 }, "Pickup Sing Q",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.70f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::sustainTime, 1 }, "String Sustain",
        juce::NormalisableRange<float>(1.5f, 12.0f, 0.1f), 6.8f, "s"));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::cabinetDrop, 1 }, "Cabinet Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.45f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::ampDrive, 1 }, "Tube / Tweed Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.16f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::toneWarmth, 1 }, "15\" Speaker Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.54f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::reverbMix, 1 }, "Spring + Plate Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.34f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::reverbDecay, 1 }, "Reverb Decay & Width",
        juce::NormalisableRange<float>(0.1f, 1.0f, 0.001f), 0.58f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::outputLevel, 1 }, "Output Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

    return { layout.begin(), layout.end() };
}

PedalSteelParams PedalSteelAudioProcessor::readParamsFromAtomics() const {
    PedalSteelParams p;
    p.presetStyle   = static_cast<int>(pPresetStyle->load());
    p.playMode      = static_cast<int>(pPlayMode->load());
    p.pedalA        = pPedalA->load();
    p.pedalB        = pPedalB->load();
    p.pedalC        = pPedalC->load();
    p.leverLKL      = pLeverLKL->load();
    p.leverLKR      = pLeverLKR->load();
    p.volumePedal   = pVolumePedal->load();
    p.scoopDepth    = pScoopDepth->load();
    p.pedalSpeed    = pPedalSpeed->load();
    p.barGlide      = pBarGlide->load();
    p.vibratoDepth  = pVibratoDepth->load();
    p.vibratoRate   = pVibratoRate->load();
    p.autoSwell     = pAutoSwell->load();
    p.swellTime     = pSwellTime->load();
    p.sampleBlend   = pSampleBlend->load();
    p.pickSnap      = pPickSnap->load();
    p.pickupPos     = pPickupPos->load();
    p.coilPeakFreq  = pCoilPeakFreq->load();
    p.coilResonance = pCoilResonance->load();
    p.sustainTime   = pSustainTime->load();
    p.cabinetDrop   = pCabinetDrop->load();
    p.ampDrive      = pAmpDrive->load();
    p.toneWarmth    = pToneWarmth->load();
    p.reverbMix     = pReverbMix->load();
    p.reverbDecay   = pReverbDecay->load();
    p.outputLevel   = pOutputLevel->load();
    return p;
}

void PedalSteelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void PedalSteelAudioProcessor::releaseResources() {
    engine.reset();
}

bool PedalSteelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PedalSteelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    engine.setParams(readParamsFromAtomics());

    int currentSample = 0;
    const int numSamples = buffer.getNumSamples();

    for (const auto metadata : midiMessages) {
        const int msgSample = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (msgSample > currentSample) {
            engine.renderBlock(buffer, currentSample, msgSample - currentSample);
            currentSample = msgSample;
        }
        engine.handleMidiMessage(metadata.getMessage());
    }

    if (currentSample < numSamples) {
        engine.renderBlock(buffer, currentSample, numSamples - currentSample);
    }
}

int PedalSteelAudioProcessor::getNumPrograms() {
    return static_cast<int>(getPedalSteelPresets().size());
}

void PedalSteelAudioProcessor::setCurrentProgram(int index) {
    loadPreset(index);
}

const juce::String PedalSteelAudioProcessor::getProgramName(int index) {
    const auto& presets = getPedalSteelPresets();
    if (index >= 0 && index < static_cast<int>(presets.size()))
        return presets[static_cast<size_t>(index)].name;
    return {};
}

void PedalSteelAudioProcessor::loadPreset(int index) {
    const auto& presets = getPedalSteelPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    currentPresetIndex = index;
    const auto& p = presets[static_cast<size_t>(index)].params;

    auto setParam = [&](const char* id, float plainVal) {
        if (auto* param = apvts.getParameter(id)) {
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
        }
    };

    setParam(ParamIDs::presetStyle,   static_cast<float>(p.presetStyle));
    setParam(ParamIDs::playMode,      static_cast<float>(p.playMode));
    setParam(ParamIDs::pedalA,        p.pedalA);
    setParam(ParamIDs::pedalB,        p.pedalB);
    setParam(ParamIDs::pedalC,        p.pedalC);
    setParam(ParamIDs::leverLKL,      p.leverLKL);
    setParam(ParamIDs::leverLKR,      p.leverLKR);
    setParam(ParamIDs::volumePedal,   p.volumePedal);
    setParam(ParamIDs::scoopDepth,    p.scoopDepth);
    setParam(ParamIDs::pedalSpeed,    p.pedalSpeed);
    setParam(ParamIDs::barGlide,      p.barGlide);
    setParam(ParamIDs::vibratoDepth,  p.vibratoDepth);
    setParam(ParamIDs::vibratoRate,   p.vibratoRate);
    setParam(ParamIDs::autoSwell,     p.autoSwell);
    setParam(ParamIDs::swellTime,     p.swellTime);
    setParam(ParamIDs::sampleBlend,   p.sampleBlend);
    setParam(ParamIDs::pickSnap,      p.pickSnap);
    setParam(ParamIDs::pickupPos,     p.pickupPos);
    setParam(ParamIDs::coilPeakFreq,  p.coilPeakFreq);
    setParam(ParamIDs::coilResonance, p.coilResonance);
    setParam(ParamIDs::sustainTime,   p.sustainTime);
    setParam(ParamIDs::cabinetDrop,   p.cabinetDrop);
    setParam(ParamIDs::ampDrive,      p.ampDrive);
    setParam(ParamIDs::toneWarmth,    p.toneWarmth);
    setParam(ParamIDs::reverbMix,     p.reverbMix);
    setParam(ParamIDs::reverbDecay,   p.reverbDecay);
    setParam(ParamIDs::outputLevel,   p.outputLevel);
}

void PedalSteelAudioProcessor::setLivePedalState(int pedalIdx, float val) {
    engine.setManualPedal(pedalIdx, val);
    const char* ids[5] = { ParamIDs::pedalA, ParamIDs::pedalB, ParamIDs::pedalC, ParamIDs::leverLKL, ParamIDs::leverLKR };
    if (pedalIdx >= 0 && pedalIdx < 5) {
        if (auto* param = apvts.getParameter(ids[pedalIdx])) {
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, val));
        }
    }
}

void PedalSteelAudioProcessor::triggerChordAudition(int chordIdx) {
    engine.triggerAuditionChord(chordIdx);
}

void PedalSteelAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PedalSteelAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessorEditor* PedalSteelAudioProcessor::createEditor() {
    return new PedalSteelAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PedalSteelAudioProcessor();
}
