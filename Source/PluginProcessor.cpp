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
    for (int i = 0; i < 5; ++i) pPedalX[i] = apvts.getRawParameterValue(ParamIDs::extraPedals[i]);
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

    {   // the double neck's extra pedals (added 2026-11-12, version 2 so hosts keep the older ids stable)
        const char* names[5] = { "Pedal D (root -1, maj7)", "Pedal E (3rd +1, sus4)", "Pedal F (5th +1, aug)",
                                 "Pedal G (5th -1, b5)", "Pedal H (3rd -1, minor)" };
        for (int i = 0; i < 5; ++i)
            layout.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID{ ParamIDs::extraPedals[i], 2 }, names[i], juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    }

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
    // a non-finite value (a hand-edited preset, a corrupted session) must never reach the engine: it would index tables
    // with NaN and poison every filter; each float falls back to its default
    PedalSteelParams p; const PedalSteelParams d;
    // a MIDI-learned CC counts from the block it arrives in (not only once the UI timer has written it to the parameter)
    auto fin = [this](const std::atomic<float>* a, float fallback) noexcept {
        const float v = midiLearn.liveValue(a, a->load(std::memory_order_relaxed)); return std::isfinite(v) ? v : fallback; };
    p.presetStyle   = static_cast<int>(fin(pPresetStyle, static_cast<float>(d.presetStyle)));
    p.playMode      = static_cast<int>(fin(pPlayMode, static_cast<float>(d.playMode)));
    p.fxCharacter   = static_cast<int>(fin(pFxCharacter, static_cast<float>(d.fxCharacter)));
    p.pedalA        = fin(pPedalA, d.pedalA);
    p.pedalB        = fin(pPedalB, d.pedalB);
    p.pedalC        = fin(pPedalC, d.pedalC);
    p.leverLKL      = fin(pLeverLKL, d.leverLKL);
    p.leverLKR      = fin(pLeverLKR, d.leverLKR);
    for (int i = 0; i < 5; ++i) p.pedalX[i] = fin(pPedalX[i], 0.0f);
    p.volumePedal   = fin(pVolumePedal, d.volumePedal);
    p.scoopDepth    = fin(pScoopDepth, d.scoopDepth);
    p.pedalSpeed    = fin(pPedalSpeed, d.pedalSpeed);
    p.barGlide      = fin(pBarGlide, d.barGlide);
    p.barSlideNoise = fin(pBarSlideNoise, d.barSlideNoise);
    p.vibratoDepth  = fin(pVibratoDepth, d.vibratoDepth);
    p.vibratoRate   = fin(pVibratoRate, d.vibratoRate);
    p.autoSwell     = fin(pAutoSwell, d.autoSwell);
    p.swellTime     = fin(pSwellTime, d.swellTime);
    p.sampleBlend   = fin(pSampleBlend, d.sampleBlend);
    p.pickSnap      = fin(pPickSnap, d.pickSnap);
    p.pickupPos     = fin(pPickupPos, d.pickupPos);
    p.coilPeakFreq  = fin(pCoilPeakFreq, d.coilPeakFreq);
    p.coilResonance = fin(pCoilResonance, d.coilResonance);
    p.sustainTime   = fin(pSustainTime, d.sustainTime);
    p.cabinetDrop   = fin(pCabinetDrop, d.cabinetDrop);
    p.stereoWidth   = fin(pStereoWidth, d.stereoWidth);
    p.ampDrive      = fin(pAmpDrive, d.ampDrive);
    p.toneWarmth    = fin(pToneWarmth, d.toneWarmth);
    p.delayMix      = fin(pDelayMix, d.delayMix);
    p.delayTime     = fin(pDelayTime, d.delayTime);
    p.tempoSync     = (fin(pTempoSync, 0.0f) >= 0.5f);
    p.hostBpm       = currentHostBpm.load(std::memory_order_relaxed);
    p.reverbMix     = fin(pReverbMix, d.reverbMix);
    p.reverbDecay   = fin(pReverbDecay, d.reverbDecay);
    p.outputLevel   = fin(pOutputLevel, d.outputLevel);
    return p;
}

void PedalSteelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    upFactor = sampleRate >= 176000.0 ? 4 : (sampleRate >= 88000.0 ? 2 : 1);
    engine.prepare(sampleRate / upFactor, samplesPerBlock / upFactor + 2);
    if (upFactor > 1) {
        // windowed-sinc low-pass at 0.46 of the engine's Nyquist-to-rate ratio, Kaiser beta 9 (~-90 dB images)
        const int n = kUpTaps * upFactor; upFir.assign((size_t) n, 0.0f);
        auto bessel0 = [](double x) { double sum = 1.0, term = 1.0; for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; } return sum; };
        const double fc = 0.46 / upFactor, beta = 9.0, mid = (n - 1) * 0.5;
        for (int i = 0; i < n; ++i) {
            const double t = i - mid, x = juce::MathConstants<double>::twoPi * fc * t;
            const double sinc = std::abs(t) < 1.0e-9 ? 1.0 : std::sin(x) / x, r = t / mid;
            upFir[(size_t) i] = (float) (2.0 * fc * sinc * bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel0(beta));
        }
        for (int ph = 0; ph < upFactor; ++ph) {   // every phase passes DC at exactly unity: no ripple at the factor rate
            double sum = 0; for (int j = 0; j < kUpTaps; ++j) sum += upFir[(size_t) (j * upFactor + ph)];
            for (int j = 0; j < kUpTaps; ++j) upFir[(size_t) (j * upFactor + ph)] = (float) (upFir[(size_t) (j * upFactor + ph)] / sum);
        }
        lowBuf.setSize(2, juce::jmax(2048, samplesPerBlock) / upFactor + 8, false, true, false);
    }
    for (auto& h : upHist) std::fill(std::begin(h), std::end(h), 0.0f);
    upHistPos = 0; upHostPos = 0;
    setLatencySamples(upFactor > 1 ? (kUpTaps / 2) * upFactor : 0);
    statusInterval = juce::jmax(1, (int) (sampleRate * 0.002)); statusSamples = 1 << 30; meterDecayN = -1;
    keyboardState.reset();
    {   // clicks made while audio was stopped must not all fire in the first block
        const juce::SpinLock::ScopedLockType lock(uiCmdLock);
        pendingUiCmds.clear();
    }
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
    if (buffer.getNumChannels() == 0) return;

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
                    case UiCmdType::FretDown: engine.triggerFretboardNote(cmd.intArg, cmd.floatArg, cmd.styleArg); break;
                    case UiCmdType::FretDrag: engine.slideFretboardNote(cmd.intArg, cmd.floatArg); break;
                    case UiCmdType::FretUp:   engine.releaseFretboardNote(cmd.intArg); break;
                    case UiCmdType::Pedal:    engine.setManualPedal(cmd.intArg, cmd.floatArg); break;
                    case UiCmdType::HeldSlide: engine.slideHeldMidiNotes(cmd.floatArg); break;
                }
            }
            pendingUiCmds.clear();
        }
    }

    midiLearn.handleMidi(midiMessages);
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    engine.setParams(readParamsFromAtomics());

    const int numSamples = buffer.getNumSamples();
    renderEngine(buffer, midiMessages);

    // last line of defence: never hand the host NaN / Inf (it can mute the whole track or worse); start clean instead
    for (int c = 0; c < buffer.getNumChannels(); ++c) {
        const float* d = buffer.getReadPointer(c); bool bad = false;
        for (int i = 0; i < numSamples; ++i) if (! std::isfinite(d[i])) { bad = true; break; }
        if (bad) { buffer.clear(); engine.reset(); break; }
    }

    // Update stereo output peak meters with smooth ballistics
    if (numSamples > 0) {
        const float blkL = buffer.getMagnitude(0, 0, numSamples);
        const float blkR = (buffer.getNumChannels() > 1) ? buffer.getMagnitude(1, 0, numSamples) : blkL;
        if (numSamples != meterDecayN) {   // the exp only when the block size changes (tiny blocks: once per call adds up)
            meterDecayN = numSamples;
            meterDecay = std::exp(-static_cast<float>(numSamples) / (0.22f * static_cast<float>(std::max(8000.0, getSampleRate()))));
        }
        const float decay = meterDecay;
        const float prevL = outputPeakL.load(std::memory_order_relaxed);
        const float prevR = outputPeakR.load(std::memory_order_relaxed);
        outputPeakL.store(std::max(blkL, prevL * decay), std::memory_order_relaxed);
        outputPeakR.store(std::max(blkR, prevR * decay), std::memory_order_relaxed);
    }

    blocksProcessed.fetch_add(1, std::memory_order_relaxed);

    // Publish live copedent status snapshot for UI visualizer: the UI reads it at 60 Hz, so ~every 2 ms is plenty
    // (with 1-sample blocks, building it on every call was a large share of the plugin's cost)
    statusSamples += numSamples;
    if (statusSamples >= statusInterval) {
        statusSamples = 0;
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

void PedalSteelAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    // note-offs that arrive while bypassed would be lost and the notes would ring on when un-bypassed: let go of everything
    juce::ignoreUnused(midi);
    buffer.clear();
    engine.allNotesOff();
    keyboardState.reset();
}

void PedalSteelAudioProcessor::renderEngine(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    const int numSamples = buffer.getNumSamples();
    if (upFactor <= 1 || lowBuf.getNumSamples() == 0) {     // 44.1 / 48 kHz: the engine renders straight into the host buffer
        int currentSample = 0;
        for (const auto metadata : midiMessages) {
            const int msgSample = juce::jlimit(0, numSamples, metadata.samplePosition);
            if (msgSample > currentSample) {
                engine.renderBlock(buffer, currentSample, msgSample - currentSample);
                currentSample = msgSample;
            }
            if (metadata.numBytes <= 3) engine.handleMidiMessage(metadata.getMessage());   // SysEx would allocate; the engine ignores it anyway
        }
        if (currentSample < numSamples) engine.renderBlock(buffer, currentSample, numSamples - currentSample);
        return;
    }

    // host sample k uses engine samples up to index k / upFactor; a new engine sample is needed whenever k % upFactor == 0
    const int f = upFactor, numCh = juce::jmin(2, buffer.getNumChannels());
    const int maxLow = lowBuf.getNumSamples();
    auto midiIt = midiMessages.cbegin(); const auto midiEnd = midiMessages.cend();
    int done = 0;
    while (done < numSamples || midiIt != midiEnd) {
        const int chunk = juce::jmin(numSamples - done, (maxLow - 1) * f);     // host samples in this pass
        const juce::int64 k0 = upHostPos;
        const int nLow = (int) ((k0 + chunk + f - 1) / f - (k0 + f - 1) / f);     // engine samples those host samples start
        juce::AudioBuffer<float> low(lowBuf.getArrayOfWritePointers(), numCh, juce::jmax(1, nLow));
        if (nLow > 0) low.clear(0, nLow);
        int lowDone = 0;
        while (midiIt != midiEnd) {
            const auto metadata = *midiIt;
            const int pos = juce::jlimit(0, numSamples, metadata.samplePosition);
            if (pos >= done + chunk && chunk > 0 && done + chunk < numSamples) break;      // belongs to a later pass
            const int lowPos = juce::jlimit(lowDone, nLow, (int) ((k0 + (pos - done) + f - 1) / f - (k0 + f - 1) / f));
            if (lowPos > lowDone) { engine.renderBlock(low, lowDone, lowPos - lowDone); lowDone = lowPos; }
            if (metadata.numBytes <= 3) engine.handleMidiMessage(metadata.getMessage());
            ++midiIt;
        }
        if (lowDone < nLow) engine.renderBlock(low, lowDone, nLow - lowDone);

        for (int c = 0; c < numCh; ++c) {
            const float* in = low.getReadPointer(c); float* out = buffer.getWritePointer(c, done); float* hist = upHist[c];
            int hp = upHistPos, li = 0; juce::int64 k = k0;
            for (int i = 0; i < chunk; ++i, ++k) {
                const int ph = (int) (k % f);
                if (ph == 0) { hp = (hp + 1) % kUpTaps; hist[hp] = in[li++]; }
                const float* h = upFir.data() + ph; float acc = 0.0f; int idx = hp;
                for (int j = 0; j < kUpTaps; ++j) { acc += h[j * f] * hist[idx]; idx = idx == 0 ? kUpTaps - 1 : idx - 1; }
                out[i] = acc;
            }
            if (c == numCh - 1) upHistPos = hp;
        }
        upHostPos += chunk; done += chunk;
        if (chunk == 0 && midiIt == midiEnd) break;
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
    for (int i = 0; i < 5; ++i) setParam(ParamIDs::extraPedals[i], p.pedalX[i]);
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
    juce::StringArray applied;
    for (auto* child : xml->getChildWithTagNameIterator("PARAM")) {
        const juce::String id = child->getStringAttribute("id");
        if (id.isNotEmpty() && child->hasAttribute("value")) {
            if (auto* param = apvts.getParameter(id)) {
                const float val = static_cast<float>(child->getDoubleAttribute("value", param->convertFrom0to1(param->getValue())));
                if (! std::isfinite(val)) continue;                       // a hand-edited "nan" / "inf": keep the default
                param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, param->convertTo0to1(val)));
                applied.add(id); appliedAny = true;
            }
        }
    }
    if (!appliedAny) return false;   // no parameters in it: not one of our presets
    // a preset saved before a parameter existed (e.g. the double neck's pedals D-H) must not inherit whatever the last
    // preset left there
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
            if (! applied.contains(rp->getParameterID())) rp->setValueNotifyingHost(rp->getDefaultValue());

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

void PedalSteelAudioProcessor::triggerFretboardNote(int stringIdx, float fret, int style) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::FretDown, stringIdx, fret, style });
}

void PedalSteelAudioProcessor::slideFretboardNote(int stringIdx, float fret) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::FretDrag, stringIdx, fret });
}

void PedalSteelAudioProcessor::slideHeldMidiNotes(float semis) {
    const juce::SpinLock::ScopedLockType lock(uiCmdLock);
    if (! pendingUiCmds.empty() && pendingUiCmds.back().type == UiCmdType::HeldSlide) { pendingUiCmds.back().floatArg = semis; return; }   // only the latest drag position matters
    if (pendingUiCmds.size() < 64) pendingUiCmds.push_back({ UiCmdType::HeldSlide, 0, semis });
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
        xml->setAttribute("editorWidth", editorWidth.load());
        xml->setAttribute("showMore", showMore.load());
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
        editorWidth = juce::jlimit(800, 2400, xmlState->getIntAttribute("editorWidth", editorWidth.load()));
        xmlState->removeAttribute("editorWidth");
        showMore = xmlState->getBoolAttribute("showMore", showMore.load());
        xmlState->removeAttribute("showMore");
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        // switches and choices hold whatever raw value a host last sent (e.g. 0.3 for "off"); replaceState leaves those
        // alone when the meaning doesn't change, so snap them to their legal value, and hosts read back what they saved
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
                const float raw = rp->getValue();
                if (! std::isfinite(raw)) { rp->setValueNotifyingHost(rp->getDefaultValue()); continue; }   // "nan" in a session
                const float legal = juce::jlimit(0.0f, 1.0f, rp->convertTo0to1(rp->convertFrom0to1(raw)));
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
