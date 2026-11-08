#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UiPreload.h"

PedalSteelAudioProcessor::PedalSteelAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()),
      midiLearn(apvts, { ParamIDs::toneWarmth, ParamIDs::pickSnap, ParamIDs::barGlide, ParamIDs::autoSwell, ParamIDs::delayMix,
                         ParamIDs::reverbMix, ParamIDs::outputLevel, ParamIDs::volumePedal, ParamIDs::pedalA, ParamIDs::pedalB,
                         ParamIDs::pedalC, ParamIDs::leverLKL, ParamIDs::leverLKR, ParamIDs::playMode })
{
    UiPreload::acquire();   // decode the editor's bitmaps in the background so the window opens quickly
    pendingUiCmds.reserve(64);

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
    pBarSlideNoise = apvts.getRawParameterValue(ParamIDs::barSlideNoise);
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
    pStereoWidth   = apvts.getRawParameterValue(ParamIDs::stereoWidth);
    pAmpDrive      = apvts.getRawParameterValue(ParamIDs::ampDrive);
    pToneWarmth    = apvts.getRawParameterValue(ParamIDs::toneWarmth);
    pDelayMix      = apvts.getRawParameterValue(ParamIDs::delayMix);
    pDelayTime     = apvts.getRawParameterValue(ParamIDs::delayTime);
    pTempoSync     = apvts.getRawParameterValue(ParamIDs::tempoSync);
    pReverbMix     = apvts.getRawParameterValue(ParamIDs::reverbMix);
    pReverbDecay   = apvts.getRawParameterValue(ParamIDs::reverbDecay);
    pFxCharacter   = apvts.getRawParameterValue(ParamIDs::fxCharacter);
    pOutputLevel   = apvts.getRawParameterValue(ParamIDs::outputLevel);

    loadPreset(0);
}

juce::AudioProcessorValueTreeState::ParameterLayout PedalSteelAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamIDs::presetStyle, 1 }, "Steel Voice Style",
        juce::StringArray{ "Sweet Georgia E9", "Hedonist Bootgaze", "Mikey's Grit Tweed", "Montana Lush Swell", "Western Swing C6th", "Baritone Canyon B6" }, 0));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamIDs::playMode, 1 }, "Performance Mode",
        juce::StringArray{ "Smart Auto-Pedal E9", "Crying Lead Gliss", "Lush Swell Pad", "Manual Copedent", "Studio Phrase Licks", "Palm Chime Harmonics" }, 0));

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
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.42f));

    auto withUnit = [](const char* unit) {
        return juce::AudioParameterFloatAttributes().withLabel(unit);
    };

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pedalSpeed, 1 }, "Pedal Pull Speed",
        juce::NormalisableRange<float>(40.0f, 350.0f, 1.0f), 165.0f, withUnit("ms")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::barGlide, 1 }, "Tonebar Glide",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 145.0f, withUnit("ms")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::barSlideNoise, 1 }, "Bar Slide Sheen",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::vibratoDepth, 1 }, "Bar Vibrato Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.38f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::vibratoRate, 1 }, "Bar Vibrato Rate",
        juce::NormalisableRange<float>(3.0f, 7.5f, 0.01f), 5.20f, withUnit("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::autoSwell, 1 }, "Auto Swell Bloom",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.24f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::swellTime, 1 }, "Swell Rise Time",
        juce::NormalisableRange<float>(30.0f, 800.0f, 1.0f), 85.0f, withUnit("ms")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::sampleBlend, 1 }, "Studio DNA Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.92f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pickSnap, 1 }, "Fingerpick Snap",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.76f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::pickupPos, 1 }, "Pickup Distance",
        juce::NormalisableRange<float>(1.2f, 4.0f, 0.01f), 2.02f, withUnit("in")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::coilPeakFreq, 1 }, "Pickup Coil Peak",
        juce::NormalisableRange<float>(1400.0f, 3200.0f, 1.0f), 2280.0f, withUnit("Hz")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::coilResonance, 1 }, "Pickup Sing Q",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.74f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::sustainTime, 1 }, "String Sustain",
        juce::NormalisableRange<float>(1.5f, 12.0f, 0.1f), 6.8f, withUnit("s")));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::cabinetDrop, 1 }, "Cabinet Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::stereoWidth, 1 }, "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f), 1.00f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::ampDrive, 1 }, "Tube / Tweed Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.24f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::toneWarmth, 1 }, "15\" Speaker Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.38f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::delayMix, 1 }, "Tape Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.00f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::delayTime, 1 }, "Tape Delay Time",
        juce::NormalisableRange<float>(40.0f, 900.0f, 1.0f), 360.0f, withUnit("ms")));

    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ ParamIDs::tempoSync, 1 }, "Host Tempo Sync", false));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::reverbMix, 1 }, "Plate Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.30f));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::reverbDecay, 1 }, "Reverb Decay",
        juce::NormalisableRange<float>(0.1f, 1.0f, 0.001f), 0.54f));

    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ ParamIDs::fxCharacter, 1 }, "Analog Character",
        juce::StringArray{
            "Studio Standard",
            "4-Track Cassette",
            "Ribbon Opto Velvet",
            "Analog Phaser",
            "Spring + Bias Trem",
            "Underwater R&B",
            "Vintage Warble",
            "Apollo Shimmer",
            "Leslie 147 Rotary",
            "Gated Diode Bloom",
            "Standel 807 Chamber"
        }, 0));

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ ParamIDs::outputLevel, 1 }, "Output Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, withUnit("dB")));

    return { layout.begin(), layout.end() };
}

PedalSteelParams PedalSteelAudioProcessor::readParamsFromAtomics() const {
    PedalSteelParams p;
    p.presetStyle   = static_cast<int>(pPresetStyle->load());
    p.playMode      = static_cast<int>(pPlayMode->load());
    p.fxCharacter   = static_cast<int>(pFxCharacter->load());
    p.pedalA        = pPedalA->load();
    p.pedalB        = pPedalB->load();
    p.pedalC        = pPedalC->load();
    p.leverLKL      = pLeverLKL->load();
    p.leverLKR      = pLeverLKR->load();
    p.volumePedal   = pVolumePedal->load();
    p.scoopDepth    = pScoopDepth->load();
    p.pedalSpeed    = pPedalSpeed->load();
    p.barGlide      = pBarGlide->load();
    p.barSlideNoise = pBarSlideNoise->load();
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
    p.stereoWidth   = pStereoWidth->load();
    p.ampDrive      = pAmpDrive->load();
    p.toneWarmth    = pToneWarmth->load();
    p.delayMix      = pDelayMix->load();
    p.delayTime     = pDelayTime->load();
    p.tempoSync     = (pTempoSync->load() >= 0.5f);
    p.hostBpm       = currentHostBpm.load(std::memory_order_relaxed);
    p.reverbMix     = pReverbMix->load();
    p.reverbDecay   = pReverbDecay->load();
    p.outputLevel   = pOutputLevel->load();
    return p;
}

void PedalSteelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine.prepare(sampleRate, samplesPerBlock);
    keyboardState.reset();
    outputPeakL.store(0.0f, std::memory_order_relaxed);
    outputPeakR.store(0.0f, std::memory_order_relaxed);
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

    // Query DAW host tempo if available
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm()) {
                if (*bpm >= 30.0 && *bpm <= 300.0) {
                    currentHostBpm.store(*bpm, std::memory_order_relaxed);
                }
            }
        }
    }

    // Drain any lock-free UI commands safely on the audio thread
    {
        const juce::SpinLock::ScopedTryLockType lock(uiCmdLock);
        if (lock.isLocked() && !pendingUiCmds.empty()) {
            for (const auto& cmd : pendingUiCmds) {
                switch (cmd.type) {
                    case UiCmdType::Chord:    engine.triggerAuditionChord(cmd.intArg); break;
                    case UiCmdType::Phrase:   engine.triggerPhraseAudition(); break;
                    case UiCmdType::Panic:    engine.allNotesOff(); break;
                    case UiCmdType::FretDown: engine.triggerFretboardNote(cmd.intArg, cmd.floatArg); break;
                    case UiCmdType::FretDrag: engine.slideFretboardNote(cmd.intArg, cmd.floatArg); break;
                    case UiCmdType::FretUp:   engine.releaseFretboardNote(cmd.intArg); break;
                    case UiCmdType::Pedal:    engine.setManualPedal(cmd.intArg, cmd.floatArg); break;
                }
            }
            pendingUiCmds.clear();
        }
    }

    midiLearn.handleMidi(midiMessages);
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

    // Update stereo output peak meters with smooth ballistics
    if (numSamples > 0) {
        const float blkL = buffer.getMagnitude(0, 0, numSamples);
        const float blkR = (buffer.getNumChannels() > 1) ? buffer.getMagnitude(1, 0, numSamples) : blkL;
        const float decay = std::exp(-static_cast<float>(numSamples) / (0.22f * static_cast<float>(std::max(8000.0, getSampleRate()))));
        const float prevL = outputPeakL.load(std::memory_order_relaxed);
        const float prevR = outputPeakR.load(std::memory_order_relaxed);
        outputPeakL.store(std::max(blkL, prevL * decay), std::memory_order_relaxed);
        outputPeakR.store(std::max(blkR, prevR * decay), std::memory_order_relaxed);
    }

    blocksProcessed.fetch_add(1, std::memory_order_relaxed);

    // Publish live copedent status snapshot for UI visualizer
    {
        const juce::SpinLock::ScopedTryLockType lock(statusLock);
        if (lock.isLocked()) {
            cachedStatus = engine.getStatus();
        }
    }
}

CopedentStatus PedalSteelAudioProcessor::getLiveStatus() const {
    const juce::SpinLock::ScopedLockType lock(statusLock);
    return cachedStatus;
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

void PedalSteelAudioProcessor::stepPreset(int delta) {
    const int total = getNumPrograms();
    if (total <= 0) return;
    int next = (currentPresetIndex.load() + delta) % total;
    if (next < 0) next += total;
    loadPreset(next);
}

void PedalSteelAudioProcessor::loadPreset(int index) {
    const auto& presets = getPedalSteelPresets();
    if (index < 0 || index >= static_cast<int>(presets.size())) return;
    currentPresetIndex.store(index);
    const auto& p = presets[static_cast<size_t>(index)].params;

    auto setParam = [&](const char* id, float plainVal) {
        if (auto* param = apvts.getParameter(id)) {
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
        }
    };

    setParam(ParamIDs::presetStyle,   static_cast<float>(p.presetStyle));
    setParam(ParamIDs::playMode,      static_cast<float>(p.playMode));
    setParam(ParamIDs::fxCharacter,   static_cast<float>(p.fxCharacter));
    setParam(ParamIDs::pedalA,        p.pedalA);
    setParam(ParamIDs::pedalB,        p.pedalB);
    setParam(ParamIDs::pedalC,        p.pedalC);
    setParam(ParamIDs::leverLKL,      p.leverLKL);
    setParam(ParamIDs::leverLKR,      p.leverLKR);
    setParam(ParamIDs::volumePedal,   p.volumePedal);
    setParam(ParamIDs::scoopDepth,    p.scoopDepth);
    setParam(ParamIDs::pedalSpeed,    p.pedalSpeed);
    setParam(ParamIDs::barGlide,      p.barGlide);
    setParam(ParamIDs::barSlideNoise, p.barSlideNoise);
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
    setParam(ParamIDs::stereoWidth,   p.stereoWidth);
    setParam(ParamIDs::ampDrive,      p.ampDrive);
    setParam(ParamIDs::toneWarmth,    p.toneWarmth);
    setParam(ParamIDs::delayMix,      p.delayMix);
    setParam(ParamIDs::delayTime,     p.delayTime);
    setParam(ParamIDs::tempoSync,     p.tempoSync ? 1.0f : 0.0f);
    setParam(ParamIDs::reverbMix,     p.reverbMix);
    setParam(ParamIDs::reverbDecay,   p.reverbDecay);
    setParam(ParamIDs::outputLevel,   p.outputLevel);

    {
        const juce::ScopedLock sl(presetMetaLock);
        activeUserPresetName.clear();
    }
    captureCurrentAsBaseline();
    presetRevision.fetch_add(1, std::memory_order_relaxed);
}

void PedalSteelAudioProcessor::captureCurrentAsBaseline() {
    const juce::ScopedLock sl(presetMetaLock);
    for (auto* p : getParameters()) {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            baselineParamValues[rp->getParameterID()] = rp->convertFrom0to1(rp->getValue());
        }
    }
}

float PedalSteelAudioProcessor::getBaselineParamValue(const juce::String& paramId) const {
    {
        const juce::ScopedLock sl(presetMetaLock);
        auto it = baselineParamValues.find(paramId);
        if (it != baselineParamValues.end()) return it->second;
    }
    if (auto* rp = apvts.getParameter(paramId)) {
        return rp->convertFrom0to1(rp->getDefaultValue());
    }
    return 0.0f;
}

juce::File PedalSteelAudioProcessor::getUserPresetsFolder() {
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("NVA Pedal Steel")
                   .getChildFile("Presets");
    if (!dir.exists()) {
        dir.createDirectory();
    }
    return dir;
}

std::vector<juce::File> PedalSteelAudioProcessor::getUserPresetFiles() const {
    std::vector<juce::File> result;
    const auto dir = getUserPresetsFolder();
    if (!dir.isDirectory()) return result;

    juce::Array<juce::File> found;
    dir.findChildFiles(found, juce::File::findFiles, false, "*.nvps;*.xml");
    found.sort();
    for (const auto& f : found) result.push_back(f);
    return result;
}

bool PedalSteelAudioProcessor::saveUserPreset(const juce::File& file) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr) return false;

    const juce::String cleanName = file.getFileNameWithoutExtension().trim();
    xml->setAttribute("pluginVersion", "1.2.0");
    xml->setAttribute("presetName", cleanName);
    xml->setAttribute("currentPresetIndex", currentPresetIndex.load());

    auto targetFile = file.hasFileExtension("nvps;xml") ? file : file.withFileExtension(".nvps");
    targetFile.getParentDirectory().createDirectory();
    if (!xml->writeTo(targetFile)) return false;

    {
        const juce::ScopedLock sl(presetMetaLock);
        activeUserPresetName = cleanName;
    }
    captureCurrentAsBaseline();
    presetRevision.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool PedalSteelAudioProcessor::loadUserPreset(const juce::File& file) {
    if (!file.existsAsFile()) return false;
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return false;

    // Apply parameter-by-parameter via setValueNotifyingHost so DAW host automation lanes stay synced!
    bool appliedAny = false;
    for (auto* child : xml->getChildWithTagNameIterator("PARAM")) {
        const juce::String id = child->getStringAttribute("id");
        if (id.isNotEmpty() && child->hasAttribute("value")) {
            if (auto* param = apvts.getParameter(id)) {
                const float val = static_cast<float>(child->getDoubleAttribute("value", param->convertFrom0to1(param->getValue())));
                param->setValueNotifyingHost(param->convertTo0to1(val));
                appliedAny = true;
            }
        }
    }
    if (!appliedAny) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    {
        const juce::ScopedLock sl(presetMetaLock);
        activeUserPresetName = xml->getStringAttribute("presetName", file.getFileNameWithoutExtension()).trim();
    }
    captureCurrentAsBaseline();
    presetRevision.fetch_add(1, std::memory_order_relaxed);
    return true;
}

juce::String PedalSteelAudioProcessor::getActiveUserPresetName() const {
    const juce::ScopedLock sl(presetMetaLock);
    return activeUserPresetName;
}

void PedalSteelAudioProcessor::setLivePedalState(int pedalIdx, float val, bool beginGesture, bool endGesture) {
    const float clamped = juce::jlimit(0.0f, 1.0f, val);
    {
        const juce::SpinLock::ScopedLockType lock(uiCmdLock);
        if (pendingUiCmds.size() < 64) {
            pendingUiCmds.push_back({ UiCmdType::Pedal, pedalIdx, clamped });
        }
    }
    const char* ids[5] = { ParamIDs::pedalA, ParamIDs::pedalB, ParamIDs::pedalC, ParamIDs::leverLKL, ParamIDs::leverLKR };
    if (pedalIdx >= 0 && pedalIdx < 5) {
        if (auto* param = apvts.getParameter(ids[pedalIdx])) {
            if (beginGesture) param->beginChangeGesture();
            param->setValueNotifyingHost(clamped);
            if (endGesture) param->endChangeGesture();
        }
    }
}

void PedalSteelAudioProcessor::triggerChordAudition(int chordIdx) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::Chord, chordIdx, 0.0f });
}

void PedalSteelAudioProcessor::triggerPhraseAudition() {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::Phrase, 0, 0.0f });
}

void PedalSteelAudioProcessor::triggerPanic() {
    keyboardState.allNotesOff(0);
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::Panic, 0, 0.0f });
}

void PedalSteelAudioProcessor::triggerFretboardNote(int stringIdx, float fret) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::FretDown, stringIdx, fret });
}

void PedalSteelAudioProcessor::slideFretboardNote(int stringIdx, float fret) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::FretDrag, stringIdx, fret });
}

void PedalSteelAudioProcessor::releaseFretboardNote(int stringIdx) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::FretUp, stringIdx, 0.0f });
}

void PedalSteelAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        xml->setAttribute("currentPresetIndex", currentPresetIndex.load());
        xml->setAttribute("midiMap", midiLearn.toString());
        xml->setAttribute("editorWidth", editorWidth);
        const juce::String userName = getActiveUserPresetName();
        if (userName.isNotEmpty()) {
            xml->setAttribute("userPresetName", userName);
        }
        copyXmlToBinary(*xml, destData);
    }
}

void PedalSteelAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        const int savedPreset = xmlState->getIntAttribute("currentPresetIndex", 0);
        currentPresetIndex.store(juce::jlimit(0, std::max(0, getNumPrograms() - 1), savedPreset));
        {
            const juce::ScopedLock sl(presetMetaLock);
            activeUserPresetName = xmlState->getStringAttribute("userPresetName", "");
        }
        midiLearn.fromString(xmlState->getStringAttribute("midiMap", ""));
        xmlState->removeAttribute("midiMap");
        editorWidth = juce::jlimit(800, 2400, xmlState->getIntAttribute("editorWidth", editorWidth));
        xmlState->removeAttribute("editorWidth");
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        // switches and choices hold whatever raw value a host last sent (e.g. 0.3 for "off"); replaceState leaves those
        // alone when the meaning doesn't change, so snap them to their legal value, and hosts read back what they saved
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
                const float raw = rp->getValue(), legal = rp->convertTo0to1(rp->convertFrom0to1(raw));
                if (std::abs(raw - legal) > 1e-6f) rp->setValueNotifyingHost(legal);
            }
        captureCurrentAsBaseline();
        presetRevision.fetch_add(1, std::memory_order_relaxed);
    }
}

PedalSteelAudioProcessor::~PedalSteelAudioProcessor() { UiPreload::release(); }

juce::AudioProcessorEditor* PedalSteelAudioProcessor::createEditor() {
    return new PedalSteelAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PedalSteelAudioProcessor();
}
