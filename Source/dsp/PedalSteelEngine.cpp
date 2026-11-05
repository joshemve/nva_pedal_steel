#include "PedalSteelEngine.h"
#include "PedalSteelAssets.h"
#include "PedalSteelMultiSamples.h"
#include <cmath>
#include <algorithm>

namespace {
    constexpr double kTwoPi = juce::MathConstants<double>::twoPi;
    constexpr double kPi    = juce::MathConstants<double>::pi;

    inline float midiToHz(float midi) {
        return 440.0f * std::pow(2.0f, (midi - 69.0f) * (1.0f / 12.0f));
    }

    inline float fastTanh(float x) {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

//==============================================================================
void PedalSteelEngine::Biquad::setLowpass(double sr, float freq, float q) {
    freq = juce::jlimit(20.0f, static_cast<float>(sr * 0.47), freq);
    q = std::max(0.1f, q);
    const double w0 = kTwoPi * freq / sr;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = static_cast<float>(((1.0 - cosw) * 0.5) / a0);
    b1 = static_cast<float>((1.0 - cosw) / a0);
    b2 = b0;
    a1 = static_cast<float>((-2.0 * cosw) / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

void PedalSteelEngine::Biquad::setHighpass(double sr, float freq, float q) {
    freq = juce::jlimit(10.0f, static_cast<float>(sr * 0.47), freq);
    q = std::max(0.1f, q);
    const double w0 = kTwoPi * freq / sr;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = static_cast<float>(((1.0 + cosw) * 0.5) / a0);
    b1 = static_cast<float>(-(1.0 + cosw) / a0);
    b2 = b0;
    a1 = static_cast<float>((-2.0 * cosw) / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

void PedalSteelEngine::Biquad::setPeak(double sr, float freq, float q, float gainDb) {
    freq = juce::jlimit(40.0f, static_cast<float>(sr * 0.47), freq);
    q = std::max(0.1f, q);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = kTwoPi * freq / sr;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / A;
    b0 = static_cast<float>((1.0 + alpha * A) / a0);
    b1 = static_cast<float>((-2.0 * cosw) / a0);
    b2 = static_cast<float>((1.0 - alpha * A) / a0);
    a1 = b1;
    a2 = static_cast<float>((1.0 - alpha / A) / a0);
}

void PedalSteelEngine::Biquad::setBandpass(double sr, float freq, float q) {
    freq = juce::jlimit(40.0f, static_cast<float>(sr * 0.47), freq);
    q = std::max(0.1f, q);
    const double w0 = kTwoPi * freq / sr;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    b0 = static_cast<float>(alpha / a0);
    b1 = 0.0f;
    b2 = static_cast<float>(-alpha / a0);
    a1 = static_cast<float>((-2.0 * cosw) / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

//==============================================================================
void PedalSteelEngine::SympatheticBank::prepare(double sr) {
    // Short diffuse wooden/aluminum cabinet body reflections (non-pitch-comb so it never beats against dyads)
    static constexpr int kTapMs[4] = { 11, 17, 23, 31 };
    for (int i = 0; i < 4; ++i) {
        int len = std::max(32, static_cast<int>(std::round(sr * kTapMs[i] * 0.001)));
        combBufs[i].assign(static_cast<size_t>(len), 0.0f);
        combPos[i] = 0;
        combLp[i] = 0.0f;
    }
}

void PedalSteelEngine::SympatheticBank::reset() {
    for (int i = 0; i < 4; ++i) {
        std::fill(combBufs[i].begin(), combBufs[i].end(), 0.0f);
        combPos[i] = 0;
        combLp[i] = 0.0f;
    }
}

float PedalSteelEngine::SympatheticBank::process(float input, float amount) {
    if (amount <= 0.001f) return 0.0f;
    float sig = input;
    for (int i = 0; i < 4; ++i) {
        auto& buf = combBufs[i];
        if (buf.empty()) continue;
        int pos = combPos[i];
        float d = buf[static_cast<size_t>(pos)];
        float w = sig + 0.50f * d;
        buf[static_cast<size_t>(pos)] = w;
        sig = -0.50f * w + d;
        if (++pos >= static_cast<int>(buf.size())) pos = 0;
        combPos[i] = pos;
    }
    combLp[0] += 0.14f * (sig - combLp[0]);
    return combLp[0] * (0.12f * amount);
}

//==============================================================================
void PedalSteelEngine::StudioReverb::prepare(double sr) {
    const double scale = std::max(8000.0, sr) / 44100.0;
    maxVelvetLen = std::max(4096, static_cast<int>(66150 * scale)); // 1.5 seconds of velvet multi-tap space
    velvetBufL.assign(static_cast<size_t>(maxVelvetLen), 0.0f);
    velvetBufR.assign(static_cast<size_t>(maxVelvetLen), 0.0f);
    velvetPos = 0;

    // 12 mutually-prime Velvet reflection tap delays (in 44.1 kHz samples, spanning 24 ms .. 1180 ms)
    static constexpr int kBaseTapsL[kNumTaps] = {
        1061, 2267, 3697, 5381, 7349, 9643, 12289, 15331, 18803, 22741, 27197, 32213
    };
    static constexpr int kBaseTapsR[kNumTaps] = {
        1187, 2441, 3917, 5659, 7687, 10037, 12743, 15859, 19391, 23417, 27961, 33073
    };
    static constexpr float kSignsL[kNumTaps] = {
        +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, -1.0f
    };
    static constexpr float kSignsR[kNumTaps] = {
        +1.0f, +1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f
    };

    for (int i = 0; i < kNumTaps; ++i) {
        tapDelaysL[i] = juce::jlimit(16, maxVelvetLen - 4, static_cast<int>(kBaseTapsL[i] * scale));
        tapDelaysR[i] = juce::jlimit(16, maxVelvetLen - 4, static_cast<int>(kBaseTapsR[i] * scale));
        tapSignsL[i] = kSignsL[i];
        tapSignsR[i] = kSignsR[i];
    }

    // 8-stage series Unit-Gain Schroeder Allpass Diffuser cascade (|H(w)| === 1.000 at all frequencies!)
    // Multiplies the 12 velvet taps into 12 * 2^8 = 3,072 dense reflections with ZERO comb-tooth warble!
    static constexpr int kAllpassLensL[kNumAllpass] = { 179, 283, 431, 643, 947, 1381, 1997, 2857 };
    static constexpr int kAllpassLensR[kNumAllpass] = { 197, 311, 463, 683, 997, 1439, 2069, 2953 };
    for (int i = 0; i < kNumAllpass; ++i) {
        allpassL[i].assign(static_cast<size_t>(std::max(32, static_cast<int>(kAllpassLensL[i] * scale))), 0.0f);
        allpassR[i].assign(static_cast<size_t>(std::max(32, static_cast<int>(kAllpassLensR[i] * scale))), 0.0f);
        allpassPosL[i] = 0;
        allpassPosR[i] = 0;
    }

    dampStateL = 0.0f;
    dampStateR = 0.0f;
    reverbToneFilter.setLowpass(sr, 2400.0f, 0.707f);
}

void PedalSteelEngine::StudioReverb::reset() {
    std::fill(velvetBufL.begin(), velvetBufL.end(), 0.0f);
    std::fill(velvetBufR.begin(), velvetBufR.end(), 0.0f);
    velvetPos = 0;
    for (int i = 0; i < kNumAllpass; ++i) {
        std::fill(allpassL[i].begin(), allpassL[i].end(), 0.0f);
        std::fill(allpassR[i].begin(), allpassR[i].end(), 0.0f);
        allpassPosL[i] = 0;
        allpassPosR[i] = 0;
    }
    dampStateL = 0.0f;
    dampStateR = 0.0f;
    reverbToneFilter.reset();
}

void PedalSteelEngine::StudioReverb::processSample(float inL, float inR, float decay, float width, float& outL, float& outR) {
    if (velvetBufL.empty()) {
        outL = outR = 0.0f;
        return;
    }

    // 1. Write input + very gentle long-tail velvet recirculation (lowpassed so zero high-Q comb teeth!)
    const int tailIdxL = (velvetPos - tapDelaysL[kNumTaps - 1] + maxVelvetLen) % maxVelvetLen;
    const int tailIdxR = (velvetPos - tapDelaysR[kNumTaps - 1] + maxVelvetLen) % maxVelvetLen;
    dampStateL += 0.08f * (velvetBufL[static_cast<size_t>(tailIdxL)] - dampStateL);
    dampStateR += 0.08f * (velvetBufR[static_cast<size_t>(tailIdxR)] - dampStateR);

    const float tailFb = 0.18f + 0.18f * juce::jlimit(0.0f, 1.0f, decay);
    velvetBufL[static_cast<size_t>(velvetPos)] = inL + dampStateR * tailFb;
    velvetBufR[static_cast<size_t>(velvetPos)] = inR + dampStateL * tailFb;

    // 2. Sum 12 exponentially-weighted Velvet reflections
    float sumL = 0.0f;
    float sumR = 0.0f;
    const float tapBaseDecay = 0.78f + 0.16f * juce::jlimit(0.0f, 1.0f, decay);
    float tapWeight = 0.36f;

    for (int i = 0; i < kNumTaps; ++i) {
        int rL = velvetPos - tapDelaysL[i];
        if (rL < 0) rL += maxVelvetLen;
        int rR = velvetPos - tapDelaysR[i];
        if (rR < 0) rR += maxVelvetLen;

        sumL += velvetBufL[static_cast<size_t>(rL)] * tapSignsL[i] * tapWeight;
        sumR += velvetBufR[static_cast<size_t>(rR)] * tapSignsR[i] * tapWeight;
        tapWeight *= tapBaseDecay;
    }

    if (++velvetPos >= maxVelvetLen) velvetPos = 0;

    // 3. 8-Stage Unit-Gain Schroeder Allpass Cascade (|H(w)| === 1.000 across all frequencies!)
    const float apG = 0.62f + 0.08f * juce::jlimit(0.0f, 1.0f, decay);
    for (int i = 0; i < kNumAllpass; ++i) {
        {
            auto& buf = allpassL[i];
            int pos = allpassPosL[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = sumL + apG * bufOut;
            sumL = -apG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosL[i] = pos;
        }
        {
            auto& buf = allpassR[i];
            int pos = allpassPosR[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = sumR + apG * bufOut;
            sumR = -apG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosR[i] = pos;
        }
    }

    sumL = reverbToneFilter.processL(sumL);
    sumR = reverbToneFilter.processR(sumR);

    const float wet1 = 0.5f * (1.0f + juce::jlimit(0.0f, 1.2f, width));
    const float wet2 = 0.5f * (1.0f - juce::jlimit(0.0f, 1.2f, width));
    outL = (sumL * wet1 + sumR * wet2) * 0.95f;
    outR = (sumR * wet1 + sumL * wet2) * 0.95f;
}

//==============================================================================
PedalSteelEngine::PedalSteelEngine() {
    for (int i = 0; i < kNumStrings; ++i) {
        voices[i].stringIndex = i;
    }
    prepare(44100.0, 512);
}

void PedalSteelEngine::prepare(double sr, int /*maxBlockSize*/) {
    sampleRate = std::max(8000.0, sr);
    sympathetic.prepare(sampleRate);
    reverb.prepare(sampleRate);
    dcBlocker.setHighpass(sampleRate, 32.0f, 0.707f);
    updateFilterCoeffs();
    reset();
}

void PedalSteelEngine::reset() {
    for (int i = 0; i < kNumStrings; ++i) {
        voices[i] = StringVoice{};
        voices[i].stringIndex = i;
    }
    sustainPedalDown = false;
    pitchWheelNorm = 0.0f;
    modWheelNorm = 0.0f;
    expressionNorm = 1.0f;
    keyswitchPedals.fill(0.0f);
    smoothPedalA = smoothPedalB = smoothPedalC = smoothLKL = smoothLKR = 0.0f;
    smoothVolPedal = params.volumePedal;
    smoothPitchWheel = 0.0f;
    smoothPitchWheelVel = 0.0f;
    globalVibratoPhase = 0.0;
    globalSwellEnv = 1.0f;

    pickupPeakFilter.reset();
    speakerBodyFilter.reset();
    speakerPresenceFilter.reset();
    speakerLowpass1.reset();
    speakerLowpass2.reset();
    dcBlocker.reset();
    sympathetic.reset();
    reverb.reset();
}

void PedalSteelEngine::setParams(const PedalSteelParams& newParams) {
    params = newParams;
    uiPedals[0] = params.pedalA;
    uiPedals[1] = params.pedalB;
    uiPedals[2] = params.pedalC;
    uiPedals[3] = params.leverLKL;
    uiPedals[4] = params.leverLKR;
    updateFilterCoeffs();
}

void PedalSteelEngine::setManualPedal(int pedalIndex, float value) {
    if (pedalIndex >= 0 && pedalIndex < 5) {
        uiPedals[static_cast<size_t>(pedalIndex)] = juce::jlimit(0.0f, 1.0f, value);
    }
}

void PedalSteelEngine::updateFilterCoeffs() {
    const float coilFreq = juce::jlimit(1200.0f, 3400.0f, params.coilPeakFreq);
    const float coilQ    = 0.85f + 0.90f * params.coilResonance;
    const float coilGain = 1.2f + 2.6f * params.coilResonance;
    pickupPeakFilter.setPeak(sampleRate, coilFreq, coilQ, coilGain);

    float bodyFreq = 265.0f;
    float bodyGain = 2.0f + 2.5f * params.toneWarmth;
    float lpCutoff = 3600.0f - 1300.0f * params.toneWarmth;

    if (params.presetStyle == 0) {
        // Sweet Georgia E9: clear 1.95 kHz Emmons single-coil sing (Centroid ~1220-1280 Hz)
        bodyFreq = 310.0f;
        lpCutoff = 4400.0f - 800.0f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 2050.0f, 1.15f, 4.2f);
    } else if (params.presetStyle == 1) {
        // Hedonist Bootgaze: warm 720 Hz octave bloom (Centroid ~715 Hz)
        bodyFreq = 285.0f;
        lpCutoff = 2500.0f - 650.0f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 760.0f, 0.95f, 2.5f);
    } else if (params.presetStyle == 2) {
        // Mikey's Grit Vibey: singing 950-1500 Hz tweed lead core (Centroid ~880 Hz)
        bodyFreq = 330.0f;
        lpCutoff = 2650.0f - 700.0f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 1180.0f, 1.0f, 2.4f);
    } else {
        // Montana Swell: deep 575 Hz velvet pad (Centroid ~575 Hz)
        bodyFreq = 240.0f;
        lpCutoff = 2050.0f - 500.0f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 640.0f, 0.90f, 2.2f);
    }

    speakerBodyFilter.setPeak(sampleRate, bodyFreq, 0.80f, bodyGain);
    speakerLowpass1.setLowpass(sampleRate, lpCutoff, 0.75f);
    speakerLowpass2.setLowpass(sampleRate, lpCutoff * 1.15f, 0.70f);
}

float PedalSteelEngine::nextRandomSigned() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(static_cast<int32_t>(rngState)) * (1.0f / 2147483648.0f);
}

int PedalSteelEngine::selectBestMultiSample(int midiNote, int /*style*/) const {
    int bestIdx = 0;
    int bestDist = 999;
    for (int i = 0; i < PedalSteelMultiSamples::kNumMultiSamples; ++i) {
        int d = std::abs(midiNote - PedalSteelMultiSamples::kMultiSamples[i].midiNote);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

float PedalSteelEngine::readMultiSampleHermite(int sampleIdx, double pos) const {
    const auto& ms = PedalSteelMultiSamples::kMultiSamples[juce::jlimit(0, PedalSteelMultiSamples::kNumMultiSamples - 1, sampleIdx)];
    const int len = ms.length;
    if (pos < 0.0 || pos >= static_cast<double>(len - 3)) return 0.0f;

    int i1 = static_cast<int>(std::floor(pos));
    float frac = static_cast<float>(pos - std::floor(pos));
    int i0 = std::max(0, i1 - 1);
    int i2 = std::min(len - 1, i1 + 1);
    int i3 = std::min(len - 1, i1 + 2);

    const float y0 = ms.data[i0] * (1.0f / 32000.0f);
    const float y1 = ms.data[i1] * (1.0f / 32000.0f);
    const float y2 = ms.data[i2] * (1.0f / 32000.0f);
    const float y3 = ms.data[i3] * (1.0f / 32000.0f);

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void PedalSteelEngine::reassignChordRoles() {
    std::array<int, kNumStrings> activeIdx{};
    int count = 0;
    for (int i = 0; i < kNumStrings; ++i) {
        if (voices[i].active && (voices[i].held || voices[i].sustainedByPedal)) {
            activeIdx[count++] = i;
        }
    }
    if (count == 0) return;

    std::sort(activeIdx.begin(), activeIdx.begin() + count, [&](int a, int b) {
        return voices[a].midiNote < voices[b].midiNote;
    });

    if (count == 1) {
        voices[activeIdx[0]].role = StringRole::Third;
        return;
    }

    if (count == 2) {
        int nLow  = voices[activeIdx[0]].midiNote;
        int nHigh = voices[activeIdx[1]].midiNote;
        int iv = (nHigh - nLow) % 12;
        if (iv == 5) {
            voices[activeIdx[0]].role = StringRole::Third;
            voices[activeIdx[1]].role = StringRole::Root;
        } else if (iv == 3 || iv == 4) {
            voices[activeIdx[0]].role = StringRole::Third;
            voices[activeIdx[1]].role = StringRole::Fifth;
        } else {
            voices[activeIdx[0]].role = StringRole::Root;
            voices[activeIdx[1]].role = StringRole::Third;
        }
        return;
    }

    int rootPitchClass = voices[activeIdx[0]].midiNote % 12;
    for (int i = 0; i < count; ++i) {
        int pc = voices[activeIdx[i]].midiNote % 12;
        bool hasThird = false, hasFifth = false;
        for (int j = 0; j < count; ++j) {
            int diff = (voices[activeIdx[j]].midiNote % 12 - pc + 12) % 12;
            if (diff == 3 || diff == 4) hasThird = true;
            if (diff == 7) hasFifth = true;
        }
        if (hasThird && hasFifth) {
            rootPitchClass = pc;
            break;
        }
    }

    for (int i = 0; i < count; ++i) {
        auto& v = voices[activeIdx[i]];
        int rel = (v.midiNote % 12 - rootPitchClass + 12) % 12;
        if (rel == 0) v.role = StringRole::Root;
        else if (rel == 3 || rel == 4 || rel == 2) v.role = StringRole::Third;
        else if (rel == 7 || rel == 9) v.role = StringRole::Fifth;
        else v.role = StringRole::SixthOr9th;
    }
}

int PedalSteelEngine::chooseBestStringForNote(int midiNote, float /*velocity*/, bool& outIsLegatoTransfer, float& outPrevBarMidi) {
    outIsLegatoTransfer = false;
    outPrevBarMidi = static_cast<float>(midiNote);

    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (v.active && v.midiNote == midiNote) {
            return i;
        }
    }

    int heldCount = 0;
    int recentlyReleasedCount = 0;
    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (!v.active) continue;
        if (v.held || v.sustainedByPedal) ++heldCount;
        else if (v.releaseAgeSec < 0.16 && v.ampEnv > 0.05f) ++recentlyReleasedCount;
    }

    // Only glide from a released note if it was a single-voice release (melody or single oblique pedal voice),
    // NOT when an entire multi-note chord was just released!
    const bool allowReleasedGlide = (recentlyReleasedCount == 1) || (heldCount >= 1 && recentlyReleasedCount <= 1);
    const int maxReleaseGlideSemis = (params.playMode == 2 || params.autoSwell > 0.45f) ? 2 : ((params.playMode == 1) ? 12 : 7);
    int bestLegatoIdx = -1;
    int bestLegatoDist = 999;

    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (!v.active) continue;

        int dist = std::abs(midiNote - v.midiNote);
        if (dist == 0) continue;

        bool canTransferReleased = (allowReleasedGlide && !v.held && !v.sustainedByPedal && v.releaseAgeSec < 0.14 && v.ampEnv > 0.05f && dist <= maxReleaseGlideSemis);
        bool canTransferOverlappedPedal = (v.held && v.noteAgeSec > 0.075 && dist <= 2);

        if ((canTransferReleased || canTransferOverlappedPedal) && dist < bestLegatoDist) {
            bestLegatoDist = dist;
            bestLegatoIdx = i;
        }
    }

    if (bestLegatoIdx >= 0) {
        outIsLegatoTransfer = true;
        const auto& v = voices[bestLegatoIdx];
        outPrevBarMidi = v.barCurrentMidi + v.pedalCurrentSemis + v.scoopOffsetSemis;
        return bestLegatoIdx;
    }

    int bestFreeIdx = -1;
    float bestScore = 1e9f;
    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        int openMidi = kOpenStringMidi[i];
        int fret = midiNote - openMidi;
        float score = std::abs(static_cast<float>(fret - 7)) * 0.5f;
        if (fret < -2 || fret > 26) score += 40.0f;
        if (v.active && (v.held || v.sustainedByPedal)) score += 500.0f;
        else if (v.active) score += 30.0f * v.ampEnv;

        if (score < bestScore) {
            bestScore = score;
            bestFreeIdx = i;
        }
    }

    if (bestFreeIdx >= 0) return bestFreeIdx;

    int oldestIdx = 0;
    uint64_t oldestOrder = UINT64_MAX;
    for (int i = 0; i < kNumStrings; ++i) {
        if (voices[i].triggerOrder < oldestOrder) {
            oldestOrder = voices[i].triggerOrder;
            oldestIdx = i;
        }
    }
    return oldestIdx;
}

void PedalSteelEngine::handleMidiMessage(const juce::MidiMessage& msg) {
    if (msg.isNoteOn()) {
        const int note = msg.getNoteNumber();
        const float vel = msg.getFloatVelocity();

        if (note >= 24 && note <= 31) {
            if (note == 24) keyswitchPedals[0] = 1.0f;
            else if (note == 26) keyswitchPedals[1] = 1.0f;
            else if (note == 28) { keyswitchPedals[0] = 1.0f; keyswitchPedals[1] = 1.0f; }
            else if (note == 29) keyswitchPedals[3] = 1.0f;
            else if (note == 31) keyswitchPedals[4] = 1.0f;
            return;
        }

        int previouslyHeld = 0;
        for (const auto& v : voices) {
            if (v.active && v.held && v.noteAgeSec > 0.035) ++previouslyHeld;
        }

        bool isLegatoTransfer = false;
        float prevMidi = static_cast<float>(note);
        int strIdx = chooseBestStringForNote(note, vel, isLegatoTransfer, prevMidi);
        auto& v = voices[strIdx];

        v.active = true;
        v.held = true;
        v.sustainedByPedal = false;
        v.midiNote = note;
        v.channel = msg.getChannel();
        v.velocity = vel;
        v.releaseAgeSec = 0.0;
        v.triggerOrder = ++noteCounter;
        v.multiSampleIdx = selectBestMultiSample(note, params.presetStyle);

        if (isLegatoTransfer) {
            float deltaSemis = static_cast<float>(note) - prevMidi;
            if (std::abs(deltaSemis) <= 2.2f && previouslyHeld >= 1) {
                v.barTargetMidi = static_cast<float>(note);
                v.barCurrentMidi = static_cast<float>(note);
                v.scoopOffsetSemis = -deltaSemis;
                v.scoopVel = 0.0f;
            } else {
                v.barCurrentMidi = prevMidi;
                v.barTargetMidi = static_cast<float>(note);
                v.scoopOffsetSemis = 0.0f;
                v.scoopVel = 0.0f;
            }
            v.ampEnv = std::max(v.ampEnv, 0.78f * vel);
            v.vibratoEnv *= 0.2f;
            if (v.noteAgeSec > 0.16) v.noteAgeSec = 0.16;
            v.transientGain = 0.0f;
        } else {
            v.noteAgeSec = 0.0;
            v.barTargetMidi = static_cast<float>(note);
            v.barCurrentMidi = static_cast<float>(note);
            v.barVelocity = 0.0f;
            v.pedalCurrentSemis = 0.0f;
            v.pedalVel = 0.0f;
            v.ampEnv = std::max(v.ampEnv * 0.25f, 0.01f);
            v.swellEnv = 1.0f - 0.96f * params.autoSwell;
            v.vibratoEnv = 0.0f;
            v.samplePos = 0.0;

            // Schroeder minimum-crest harmonic phase initialization so harmonics never spike or buzz!
            for (int h = 0; h < kMaxHarmonics; ++h) {
                v.harmPhases[static_cast<size_t>(h)] = std::fmod(kPi * static_cast<double>(h * (h + 1)) / static_cast<double>(kMaxHarmonics) + static_cast<double>(strIdx) * 0.57, kTwoPi);
            }

            v.transientIdx = (params.presetStyle * 2 + (strIdx % 2)) % PedalSteelAssets::kNumTransients;
            v.transientPos = 0.0;
            v.transientGain = params.pickSnap * (1.0f - 0.85f * params.autoSwell) * (0.40f + 0.60f * vel);

            reassignChordRoles();

            v.scoopOffsetSemis = 0.0f;
            v.scoopVel = 0.0f;
            if (params.scoopDepth > 0.02f && params.playMode != 2 && params.playMode != 3 && params.autoSwell < 0.65f) {
                int totalActiveNow = 0;
                for (const auto& ov : voices) if (ov.active && ov.held) ++totalActiveNow;

                if (totalActiveNow == 1) {
                    if (vel >= 0.55f) {
                        v.scoopOffsetSemis = -1.0f * params.scoopDepth * 0.60f;
                    }
                } else {
                    for (auto& ov : voices) {
                        if (ov.active && ov.held && ov.noteAgeSec < 0.025) {
                            if (ov.role == StringRole::Third) {
                                ov.scoopOffsetSemis = -1.0f * params.scoopDepth * 0.70f;
                            } else {
                                ov.scoopOffsetSemis = 0.0f;
                            }
                        }
                    }
                }
            }
            return;
        }

        reassignChordRoles();
    } else if (msg.isNoteOff()) {
        const int note = msg.getNoteNumber();
        if (note >= 24 && note <= 31) {
            if (note == 24) keyswitchPedals[0] = 0.0f;
            else if (note == 26) keyswitchPedals[1] = 0.0f;
            else if (note == 28) { keyswitchPedals[0] = 0.0f; keyswitchPedals[1] = 0.0f; }
            else if (note == 29) keyswitchPedals[3] = 0.0f;
            else if (note == 31) keyswitchPedals[4] = 0.0f;
            return;
        }

        for (auto& v : voices) {
            if (v.active && v.held && v.midiNote == note) {
                v.held = false;
                if (sustainPedalDown) {
                    v.sustainedByPedal = true;
                } else {
                    v.releaseAgeSec = 0.0;
                }
            }
        }
    } else if (msg.isPitchWheel()) {
        pitchWheelNorm = static_cast<float>(msg.getPitchWheelValue() - 8192) * (1.0f / 8192.0f);
    } else if (msg.isController()) {
        const int cc = msg.getControllerNumber();
        const float val = static_cast<float>(msg.getControllerValue()) * (1.0f / 127.0f);
        if (cc == 1) modWheelNorm = val;
        else if (cc == 11 || cc == 7) expressionNorm = val;
        else if (cc == 64) {
            sustainPedalDown = (val >= 0.5f);
            if (!sustainPedalDown) {
                for (auto& v : voices) {
                    if (v.active && v.sustainedByPedal) {
                        v.sustainedByPedal = false;
                        v.releaseAgeSec = 0.0;
                    }
                }
            }
        } else if (cc == 120 || cc == 123) {
            for (auto& v : voices) {
                v.held = false;
                v.sustainedByPedal = false;
            }
        }
    }
}

void PedalSteelEngine::triggerAuditionChord(int chordIndex) {
    for (auto& v : voices) {
        v.held = false;
        v.sustainedByPedal = false;
    }
    static constexpr int kChords[4][3] = {
        { 64, 68, 71 }, // E Major
        { 62, 67, 71 }, // G Major
        { 62, 66, 69 }, // D Major
        { 60, 64, 67 }  // C Major
    };
    const int idx = juce::jlimit(0, 3, chordIndex);
    for (int i = 0; i < 3; ++i) {
        handleMidiMessage(juce::MidiMessage::noteOn(1, kChords[idx][i], 0.82f));
    }
}

CopedentStatus PedalSteelEngine::getStatus() const {
    CopedentStatus st;
    st.pedalA = smoothPedalA;
    st.pedalB = smoothPedalB;
    st.pedalC = smoothPedalC;
    st.leverLKL = smoothLKL;
    st.leverLKR = smoothLKR;
    st.volumePedal = smoothVolPedal * globalSwellEnv;

    int activeCount = 0;
    float barSum = 0.0f;
    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        st.stringEnergy[static_cast<size_t>(i)] = v.active ? juce::jlimit(0.0f, 1.0f, v.ampEnv) : 0.0f;
        st.stringPitchSemis[static_cast<size_t>(i)] = v.barCurrentMidi + v.pedalCurrentSemis + v.scoopOffsetSemis;
        st.stringBendSemis[static_cast<size_t>(i)] = v.pedalCurrentSemis + v.scoopOffsetSemis;
        if (v.active && v.ampEnv > 0.02f) {
            ++activeCount;
            barSum += std::max(0.0f, v.barCurrentMidi - static_cast<float>(kOpenStringMidi[i]));
        }
    }
    st.activeVoiceCount = activeCount;
    st.barPositionFret = (activeCount > 0) ? (barSum / static_cast<float>(activeCount)) : 0.0f;
    return st;
}

void PedalSteelEngine::renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const double dt = 1.0 / sampleRate;
    const float fSr = static_cast<float>(sampleRate);

    const float targetA   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalA, uiPedals[0], keyswitchPedals[0], modWheelNorm }));
    const float targetB   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalB, uiPedals[1], keyswitchPedals[1], juce::jlimit(0.0f, 1.0f, (modWheelNorm - 0.12f) * 1.14f) }));
    const float targetC   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalC, uiPedals[2], keyswitchPedals[2] }));
    const float targetLKL = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKL, uiPedals[3], keyswitchPedals[3] }));
    const float targetLKR = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKR, uiPedals[4], keyswitchPedals[4] }));
    const float targetVol = juce::jlimit(0.0f, 1.0f, params.volumePedal * expressionNorm);

    const float pedalTimeSec = std::max(0.030f, params.pedalSpeed * 0.001f);
    const float pedalOmega   = 4.6f / pedalTimeSec;

    const float barTimeSec   = std::max(0.010f, params.barGlide * 0.001f);
    const float barOmega     = 4.2f / barTimeSec;

    const float swellRate    = static_cast<float>(dt / std::max(0.025, params.swellTime * 0.001));
    // Realistic plucked pedal-steel string amplitude decay (~6.5 - 8 dB/sec after initial pluck peak)
    const double stringTauSec = 0.65 + 0.15 * std::max(1.0, static_cast<double>(params.sustainTime));
    const float sustainDecayRate = static_cast<float>(1.0 - std::exp(-dt / stringTauSec));
    const float releaseTimeSec = (params.playMode == 2) ? 0.55f : 0.28f;
    const float releaseDecayRate = static_cast<float>(1.0 - std::exp(-dt / releaseTimeSec));

    const float outGain = juce::Decibels::decibelsToGain(params.outputLevel);
    const float coilFreq = juce::jlimit(1200.0f, 3400.0f, params.coilPeakFreq);
    const float coilQ    = 0.9f + 1.1f * params.coilResonance;

    // Per-string tube/pickup saturation amount (prevents polyphonic intermodulation buzz between strings!)
    const float strDrive = 1.0f + 1.8f * params.ampDrive;
    const float strBias  = 0.05f * params.ampDrive + (params.presetStyle == 1 ? 0.04f : 0.0f);

    float* outL = buffer.getWritePointer(0, startSample);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1, startSample) : nullptr;

    for (int s = 0; s < numSamples; ++s) {
        const float pSmooth = static_cast<float>(1.0 - std::exp(-dt * 35.0));
        smoothPedalA += (targetA - smoothPedalA) * pSmooth;
        smoothPedalB += (targetB - smoothPedalB) * pSmooth;
        smoothPedalC += (targetC - smoothPedalC) * pSmooth;
        smoothLKL    += (targetLKL - smoothLKL) * pSmooth;
        smoothLKR    += (targetLKR - smoothLKR) * pSmooth;
        smoothVolPedal += (targetVol - smoothVolPedal) * pSmooth;

        // 2nd-order critically-damped chrome tonebar inertia for pitch-wheel slides (100% C^2 smooth, zero staircase FM!)
        const float pwOmega = std::min(barOmega, 8.2f);
        const float pwAccel = pwOmega * pwOmega * (pitchWheelNorm - smoothPitchWheel) - 2.0f * pwOmega * smoothPitchWheelVel;
        smoothPitchWheelVel += pwAccel * static_cast<float>(dt);
        smoothPitchWheel    += smoothPitchWheelVel * static_cast<float>(dt);

        // Single rigid chrome tonebar vibrato LFO (all strings under the bar vibrate in 100% phase lock!)
        globalVibratoPhase += kTwoPi * params.vibratoRate * dt;
        if (globalVibratoPhase >= kTwoPi) globalVibratoPhase -= kTwoPi;
        const float barVibratoSin = static_cast<float>(std::sin(globalVibratoPhase));

        const float totalPedalLoad = smoothPedalA + smoothPedalB + smoothPedalC;
        const float cabinetDropSemis = -0.022f * params.cabinetDrop * juce::jlimit(0.0f, 2.0f, totalPedalLoad);

        float mixStringsL = 0.0f;
        float mixStringsR = 0.0f;
        float maxSwell = 0.0f;
        float maxSlideSpeed = 0.0f;

        for (int i = 0; i < kNumStrings; ++i) {
            auto& v = voices[i];
            if (!v.active) continue;

            v.noteAgeSec += dt;
            if (!v.held && !v.sustainedByPedal) {
                v.releaseAgeSec += dt;
                v.ampEnv -= v.ampEnv * releaseDecayRate;
                if (v.ampEnv < 0.0002f) {
                    v.active = false;
                    v.ampEnv = 0.0f;
                    continue;
                }
            } else {
                if (v.noteAgeSec < 0.014) {
                    const float atkRate = static_cast<float>(1.0 - std::exp(-dt / 0.0032));
                    v.ampEnv += (v.velocity - v.ampEnv) * atkRate;
                } else {
                    v.ampEnv -= v.ampEnv * sustainDecayRate;
                }
            }

            if (v.swellEnv < 1.0f) {
                v.swellEnv = std::min(1.0f, v.swellEnv + swellRate * (0.30f + 1.4f * v.swellEnv * (1.05f - v.swellEnv)));
            }
            maxSwell = std::max(maxSwell, v.swellEnv);

            // 1. Tonebar Inertial Glide
            const float barErr = v.barTargetMidi - v.barCurrentMidi;
            const float barAccel = barOmega * barOmega * barErr - 2.0f * barOmega * v.barVelocity;
            v.barVelocity += barAccel * static_cast<float>(dt);
            v.barCurrentMidi += v.barVelocity * static_cast<float>(dt);

            // 2. Mechanical Bellcrank Pedal Pull & Grace Scoop S-Curve
            float roleTargetSemis = 0.0f;
            if (v.role == StringRole::Third) {
                roleTargetSemis = 1.0f * smoothPedalB - 0.5f * smoothLKR;
            } else if (v.role == StringRole::Fifth) {
                roleTargetSemis = 2.0f * smoothPedalA + 2.0f * smoothPedalC;
            } else if (v.role == StringRole::Root) {
                roleTargetSemis = 1.0f * smoothLKL - 1.0f * smoothLKR + 2.0f * smoothPedalC;
            } else {
                roleTargetSemis = 1.0f * smoothPedalB + 1.0f * smoothPedalA;
            }

            const float pedalErr = roleTargetSemis - v.pedalCurrentSemis;
            const float pedalAccel = pedalOmega * pedalOmega * pedalErr - 1.95f * pedalOmega * v.pedalVel;
            v.pedalVel += pedalAccel * static_cast<float>(dt);
            v.pedalCurrentSemis += v.pedalVel * static_cast<float>(dt);

            if (std::abs(v.scoopOffsetSemis) > 0.0001f || std::abs(v.scoopVel) > 0.0001f) {
                const float scoopAccel = -pedalOmega * pedalOmega * v.scoopOffsetSemis - 1.95f * pedalOmega * v.scoopVel;
                v.scoopVel += scoopAccel * static_cast<float>(dt);
                v.scoopOffsetSemis += v.scoopVel * static_cast<float>(dt);
            }

            // 3. Delayed Wrist-Roll Tonebar Vibrato (pauses automatically during bar slides & pedal pulls!)
            const float pitchSpeedSemisPerSec = std::abs(v.barVelocity) + std::abs(v.pedalVel) + std::abs(v.scoopVel) + std::abs(smoothPitchWheelVel) * 7.0f;
            maxSlideSpeed = std::max(maxSlideSpeed, pitchSpeedSemisPerSec);
            if (pitchSpeedSemisPerSec < 0.8f && v.noteAgeSec > 0.30) {
                v.vibratoEnv = std::min(1.0f, v.vibratoEnv + static_cast<float>(dt * 2.0));
            } else {
                v.vibratoEnv = std::max(0.0f, v.vibratoEnv - static_cast<float>(dt * 6.0));
            }
            const float vibratoSemis = barVibratoSin * (0.065f * params.vibratoDepth * v.vibratoEnv);

            const float cabDrop = (std::abs(roleTargetSemis) < 0.05f) ? cabinetDropSemis : 0.0f;
            // Pitch wheel covers ±7 semitones (Perfect 5th) with continuous chrome-tonebar smoothing
            const float pitchWheelSemis = smoothPitchWheel * 7.0f;
            const float instMidi = v.barCurrentMidi + v.pedalCurrentSemis + v.scoopOffsetSemis + vibratoSemis + cabDrop + pitchWheelSemis;
            const float instFreqHz = juce::jlimit(40.0f, 4000.0f, midiToHz(instMidi));

            //==================================================================
            // Plucked Steel-String 2-Stage Harmonic Evolution + Fixed-Inch Pickup Comb & LC Tank
            //==================================================================
            const float fretSemis = juce::jlimit(-4.0f, 26.0f, v.barCurrentMidi - static_cast<float>(kOpenStringMidi[i]));
            const float barStringLenInches = 24.25f * std::pow(2.0f, -fretSemis * (1.0f / 12.0f));
            const float betaPickup = juce::jlimit(0.05f, 0.46f, params.pickupPos / barStringLenInches);
            const float betaPick   = juce::jlimit(0.07f, 0.48f, (params.pickupPos + 0.95f) / barStringLenInches);

            // Smoothly interpolate between the 4 register harmonic profiles (zero step-switching on slides/vibrato!)
            const float regPos = juce::jlimit(0.0f, 2.999f, (instMidi - 54.0f) * (3.0f / 21.0f));
            const int regIdx0 = static_cast<int>(regPos);
            const int regIdx1 = std::min(3, regIdx0 + 1);
            const float regFrac = regPos - static_cast<float>(regIdx0);
            const int baseProf = juce::jlimit(0, 3, params.presetStyle) * 4;
            const float* refHarm0 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx0];
            const float* refHarm1 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx1];

            float harmSum = 0.0f;
            const float nyquistLimit = fSr * 0.44f;
            const float ageF = static_cast<float>(v.noteAgeSec);

            // Plucked-string initial pick snap envelope (decays over ~135ms into singing sustain body)
            const float pluckSnapEnv = std::exp(-ageF / (0.11f + 0.08f * params.pickSnap)) * (1.0f - 0.85f * params.autoSwell) * v.swellEnv;

            for (int h = 1; h <= kMaxHarmonics; ++h) {
                const float fh = instFreqHz * static_cast<float>(h);
                if (fh >= nyquistLimit) break;

                double ph = v.harmPhases[static_cast<size_t>(h - 1)] + kTwoPi * static_cast<double>(fh) * dt;
                if (ph >= kTwoPi) ph -= kTwoPi;
                v.harmPhases[static_cast<size_t>(h - 1)] = ph;

                const float spatialComb = std::abs(std::sin(static_cast<float>(kPi) * h * betaPick) *
                                                   std::sin(static_cast<float>(kPi) * h * betaPickup));

                // Natural harmonic decay rate (higher partials decay faster than H1/H2/H3)
                const float hDecayTau = (h <= 2)
                    ? 12.0f
                    : (params.sustainTime / (1.0f + 0.32f * static_cast<float>((h - 2) * (h - 2))));
                const float hSustainDecay = std::exp(-ageF / std::max(0.22f, hDecayTau));

                const float refH = refHarm0[h - 1] + regFrac * (refHarm1[h - 1] - refHarm0[h - 1]);
                float sustainWeight = 0.0f;
                if (h == 1) {
                    sustainWeight = std::max(0.65f, refH);
                } else if (h == 2) {
                    float h2Base = (params.presetStyle == 1) ? 0.82f : (params.presetStyle == 3 ? 0.55f : 0.45f);
                    sustainWeight = std::max(h2Base, refH * 0.85f);
                } else if (h <= 7) {
                    // Preserve Sweet Georgia's singing 1.2 - 2.5 kHz Emmons single-coil harmonics (H3..H6)!
                    float styleBright = (params.presetStyle == 0) ? 1.18f : (params.presetStyle == 2 ? 0.45f : 0.30f);
                    sustainWeight = (refH * styleBright + 0.16f * spatialComb / static_cast<float>(h)) * hSustainDecay;
                } else {
                    sustainWeight = 0.0f;
                }

                const float twangWeight = (h >= 2)
                    ? (spatialComb * (0.42f / std::pow(static_cast<float>(h), 0.65f)) * pluckSnapEnv * params.pickSnap)
                    : 0.0f;

                // Stationary High-Inductance Pickup LC Tank evaluated at fh = h * f(t) (100% C^inf smooth!)
                const float fNorm = fh / coilFreq;
                const float denom = (1.0f - fNorm * fNorm) * (1.0f - fNorm * fNorm) + (fNorm / coilQ) * (fNorm / coilQ);
                const float lcGain = 1.0f / std::sqrt(std::max(0.32f, denom));

                // Upper register singing purity (above 2200 Hz, harmonics roll off smoothly like a 15" speaker)
                const float highRegPurity = (fh > 2200.0f && h >= 3)
                    ? std::exp(-(fh - 2200.0f) * 0.0011f)
                    : 1.0f;

                const float hAmp = (sustainWeight + twangWeight) * lcGain * highRegPurity;
                harmSum += hAmp * static_cast<float>(std::sin(ph));
            }

            // Real Fingerpick Transient Grain
            float transientSig = 0.0f;
            if (v.transientPos < static_cast<double>(PedalSteelAssets::kTransientSize - 1) && v.transientGain > 0.001f) {
                int ti = static_cast<int>(v.transientPos);
                float tFrac = static_cast<float>(v.transientPos - ti);
                const float* tData = PedalSteelAssets::kTransients[v.transientIdx];
                float rawT = tData[ti] + tFrac * (tData[ti + 1] - tData[ti]);
                v.pickLpState += (0.25f + 0.45f * v.velocity) * (rawT - v.pickLpState);
                transientSig = v.pickLpState * v.transientGain;
                v.transientPos += 44100.0 / sampleRate;
            }

            // Plucked-string 2-stage dynamic contour: initial thumbpick crest + tonebar glide singing bloom
            const float pickCrest = 1.0f + 0.58f * params.pickSnap * (1.0f - 0.85f * params.autoSwell) * std::exp(-ageF / 0.24f);
            const float glideBloom = 1.0f + 0.16f * juce::jlimit(0.0f, 1.0f, pitchSpeedSemisPerSec * 0.15f);
            const float swellShape = (params.autoSwell > 0.4f) ? (v.swellEnv * v.swellEnv) : v.swellEnv;

            // Per-string tube/coil saturation in glassy low-THD sweet spot (zero intermodulation or crunch!)
            float strRaw = (harmSum * 0.28f + transientSig * 0.22f) * pickCrest * glideBloom;
            float strSat = (fastTanh(strRaw * strDrive + strBias) - fastTanh(strBias)) / (1.0f + 0.5f * params.ampDrive);
            float voiceOut = strSat * v.ampEnv * swellShape;
            v.lastOutput = voiceOut;

            const float pan = (static_cast<float>(i) - 4.5f) * 0.045f;
            mixStringsL += voiceOut * (0.5f - pan);
            mixStringsR += voiceOut * (0.5f + pan);
        }

        globalSwellEnv = (maxSwell > 0.0f) ? maxSwell : 1.0f;

        // Pure solid-body magnetic pickup signal (no 36ms comb/allpass phaser on the dry string!)
        float sigL = mixStringsL;
        float sigR = mixStringsR;

        sigL = pickupPeakFilter.processL(sigL);
        sigR = pickupPeakFilter.processR(sigR);

        const float volGain = smoothVolPedal * smoothVolPedal * (1.5f - 0.5f * smoothVolPedal);
        sigL *= volGain;
        sigR *= volGain;

        // Linear High-Headroom 15" JBL / Black Widow Speaker Cabinet & Rolloff Chain
        sigL = speakerBodyFilter.processL(sigL);
        sigR = speakerBodyFilter.processR(sigR);
        sigL = speakerPresenceFilter.processL(sigL);
        sigR = speakerPresenceFilter.processR(sigR);
        sigL = speakerLowpass1.processL(sigL);
        sigR = speakerLowpass1.processR(sigR);
        sigL = speakerLowpass2.processL(sigL);
        sigR = speakerLowpass2.processR(sigR);
        sigL = dcBlocker.processL(sigL);
        sigR = dcBlocker.processR(sigR);

        // Studio Spring + Lush Stereo Plate Reverb (with glissando-ducked send so old frequencies never beat against fast bar slides!)
        float revL = 0.0f, revR = 0.0f;
        const float stereoWidth = (params.presetStyle >= 2) ? (0.90f + 0.30f * params.reverbDecay) : (0.65f + 0.25f * params.reverbDecay);
        const float slideReverbDuck = 1.0f / (1.0f + 0.35f * maxSlideSpeed);
        reverb.processSample(sigL * slideReverbDuck, sigR * slideReverbDuck, params.reverbDecay, stereoWidth, revL, revR);

        const float wet = juce::jlimit(0.0f, 1.0f, params.reverbMix);
        const float dry = 1.0f - 0.28f * wet;
        // Generous polyphonic clean headroom (0.18f) so multi-string chords + resonant LC peak never clip!
        float finalL = (sigL * dry + revL * wet * 1.35f) * outGain * 0.18f;
        float finalR = (sigR * dry + revR * wet * 1.35f) * outGain * 0.18f;

        outL[s] = juce::jlimit(-0.98f, 0.98f, finalL);
        if (outR) outR[s] = juce::jlimit(-0.98f, 0.98f, finalR);
    }
}
