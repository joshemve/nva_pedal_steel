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

    // Studio baseline values per style (0..3) so default preset positions apply 0.0 dB extra coloration to studio samples
    constexpr float kBaseCoilRes[4]  = { 0.74f, 0.78f, 0.72f, 0.52f };
    constexpr float kBaseWarmth[4]   = { 0.38f, 0.36f, 0.56f, 0.84f };
    constexpr float kBaseDrive[4]    = { 0.24f, 0.62f, 0.56f, 0.16f };
    constexpr float kBaseRevMix[4]   = { 0.30f, 0.46f, 0.42f, 0.48f };

    inline float readFloatBufHermite(const std::vector<float>& buf, int writePos, float delaySamples) {
        const int maxLen = static_cast<int>(buf.size());
        float readPos = static_cast<float>(writePos) - delaySamples;
        while (readPos < 0.0f) readPos += static_cast<float>(maxLen);
        int i1 = static_cast<int>(readPos);
        const float frac = readPos - static_cast<float>(i1);
        i1 %= maxLen;
        const int i0 = (i1 - 1 + maxLen) % maxLen;
        const int i2 = (i1 + 1) % maxLen;
        const int i3 = (i1 + 2) % maxLen;

        const float y0 = buf[static_cast<size_t>(i0)];
        const float y1 = buf[static_cast<size_t>(i1)];
        const float y2 = buf[static_cast<size_t>(i2)];
        const float y3 = buf[static_cast<size_t>(i3)];
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
}

//==============================================================================
double PedalSteelEngine::computeSyncedDelayMs(float delayParamMs, double bpm) {
    const double safeBpm = juce::jlimit(40.0, 260.0, bpm);
    const double quarterMs = 60000.0 / safeBpm;
    double beats = 0.75; // default dotted 8th
    if (delayParamMs < 150.0f)      beats = 0.25;       // 1/16 slap
    else if (delayParamMs < 280.0f) beats = 1.0 / 3.0;  // 1/8T
    else if (delayParamMs < 430.0f) beats = 0.50;       // 1/8
    else if (delayParamMs < 590.0f) beats = 0.75;       // 1/8D (Americana / Bootgaze signature)
    else if (delayParamMs < 750.0f) beats = 1.00;       // 1/4
    else                            beats = 1.50;       // 1/4D
    return juce::jlimit(40.0, 1850.0, quarterMs * beats);
}

juce::String PedalSteelEngine::getSyncedSubdivisionLabel(float delayParamMs) {
    if (delayParamMs < 150.0f)      return "1/16";
    else if (delayParamMs < 280.0f) return "1/8T";
    else if (delayParamMs < 430.0f) return "1/8";
    else if (delayParamMs < 590.0f) return "1/8D";
    else if (delayParamMs < 750.0f) return "1/4";
    return "1/4D";
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
void PedalSteelEngine::StudioTapeDelay::prepare(double sr) {
    maxLen = std::max(4096, static_cast<int>(std::ceil(sr * 2.15)));
    bufL.assign(static_cast<size_t>(maxLen), 0.0f);
    bufR.assign(static_cast<size_t>(maxLen), 0.0f);
    writePos = 0;
    smoothDelaySamples = static_cast<float>(0.34 * sr);
    wowPhase = 0.0;
    fbLpL = fbLpR = fbHpL = fbHpR = duckEnv = 0.0f;
    slideDuckSmooth = 1.0f;
}

void PedalSteelEngine::StudioTapeDelay::reset() {
    std::fill(bufL.begin(), bufL.end(), 0.0f);
    std::fill(bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    fbLpL = fbLpR = fbHpL = fbHpR = duckEnv = 0.0f;
    slideDuckSmooth = 1.0f;
}

void PedalSteelEngine::StudioTapeDelay::processSample(double sr, float inL, float inR, float delayMs, float mix, float slideSpeed, float& outWetL, float& outWetR) {
    if (bufL.empty() || mix <= 0.001f) {
        outWetL = outWetR = 0.0f;
        return;
    }

    const float targetSamples = juce::jlimit(32.0f, static_cast<float>(maxLen - 64), static_cast<float>(delayMs * 0.001 * sr));
    smoothDelaySamples += (targetSamples - smoothDelaySamples) * 0.00055f;

    // Subtle 0.52 Hz quadrature Echoplex capstan wow/flutter (±0.24 ms) for 3D stereo analog warmth
    wowPhase += kTwoPi * 0.52 / sr;
    if (wowPhase >= kTwoPi) wowPhase -= kTwoPi;
    const float wowSamplesL = static_cast<float>(std::sin(wowPhase) * 0.00024 * sr);
    const float wowSamplesR = static_cast<float>(std::cos(wowPhase) * 0.00024 * sr);

    const float delL = juce::jlimit(16.0f, static_cast<float>(maxLen - 8), smoothDelaySamples + wowSamplesL);
    const float delR = juce::jlimit(16.0f, static_cast<float>(maxLen - 8), smoothDelaySamples * 1.065f + wowSamplesR);

    const float tapL = readFloatBufHermite(bufL, writePos, delL);
    const float tapR = readFloatBufHermite(bufR, writePos, delR);

    // Warm analog tape head bandpass (115 Hz HPF + 2.85 kHz LPF) & soft magnetic saturation
    const float lpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2850.0 / sr));
    const float hpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 115.0 / sr));
    fbLpL += lpAlpha * (tapL - fbLpL);
    fbLpR += lpAlpha * (tapR - fbLpR);
    fbHpL += hpAlpha * (fbLpL - fbHpL);
    fbHpR += hpAlpha * (fbLpR - fbHpR);
    const float filteredL = fbLpL - fbHpL;
    const float filteredR = fbLpR - fbHpR;

    // Pedal/bar-slide harmonic guard: ducks stale pre-pull echoes when pedals or tonebar bend chords!
    const float targetSlideDuck = 1.0f / (1.0f + 0.32f * std::max(0.0f, slideSpeed - 0.35f));
    const float sdAlpha = (targetSlideDuck < slideDuckSmooth) ? 0.0045f : 0.00010f;
    slideDuckSmooth += sdAlpha * (targetSlideDuck - slideDuckSmooth);

    const float feedback = (0.26f + 0.24f * juce::jlimit(0.0f, 1.0f, mix)) * (0.55f + 0.45f * slideDuckSmooth);
    // Gentle cross-coupled stereo bounce
    const float writeL = fastTanh(inL * 0.85f + (filteredL * 0.78f + filteredR * 0.22f) * feedback);
    const float writeR = fastTanh(inR * 0.85f + (filteredR * 0.78f + filteredL * 0.22f) * feedback);

    bufL[static_cast<size_t>(writePos)] = writeL;
    bufR[static_cast<size_t>(writePos)] = writeR;
    if (++writePos >= maxLen) writePos = 0;

    // Studio dynamic ducking: echoes tuck gently underneath picked phrases and pedal pulls, then bloom open
    const float inMag = std::max(std::abs(inL), std::abs(inR));
    const float atk = (inMag > duckEnv) ? 0.015f : 0.0004f;
    duckEnv += atk * (inMag - duckEnv);
    const float duckGain = (1.0f / (1.0f + 1.15f * duckEnv)) * slideDuckSmooth;

    outWetL = filteredL * mix * duckGain * 0.92f;
    outWetR = filteredR * mix * duckGain * 0.92f;
}

//==============================================================================
void PedalSteelEngine::StudioReverb::prepare(double sr) {
    const double scale = std::max(8000.0, sr) / 44100.0;
    maxVelvetLen = std::max(4096, static_cast<int>(66150 * scale));
    velvetBufL.assign(static_cast<size_t>(maxVelvetLen), 0.0f);
    velvetBufR.assign(static_cast<size_t>(maxVelvetLen), 0.0f);
    velvetPos = 0;

    static constexpr int kBaseTapsL[kNumTaps] = {
        499, 787, 1103, 1459, 1847, 2269, 2719, 3203, 3719, 4261, 4831, 5437
    };
    static constexpr int kBaseTapsR[kNumTaps] = {
        547, 839, 1163, 1523, 1913, 2347, 2803, 3299, 3823, 4373, 4957, 5573
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

    static constexpr int kAllpassLensL[kNumAllpass] = { 139, 223, 347, 521, 887, 1327, 1931, 2797 };
    static constexpr int kAllpassLensR[kNumAllpass] = { 151, 241, 373, 557, 937, 1399, 2017, 2909 };
    for (int i = 0; i < kNumAllpass; ++i) {
        allpassL[i].assign(static_cast<size_t>(std::max(32, static_cast<int>(kAllpassLensL[i] * scale))), 0.0f);
        allpassR[i].assign(static_cast<size_t>(std::max(32, static_cast<int>(kAllpassLensR[i] * scale))), 0.0f);
        allpassPosL[i] = 0;
        allpassPosR[i] = 0;
    }

    dampStateL = 0.0f;
    dampStateR = 0.0f;
    reverbToneFilter.setLowpass(sr, 2850.0f, 0.707f);
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

    float diffL = (inL * 0.80f + inR * 0.20f) * 0.65f;
    float diffR = (inR * 0.80f + inL * 0.20f) * 0.65f;
    const float apInG = 0.62f;
    for (int i = 0; i < 4; ++i) {
        {
            auto& buf = allpassL[i];
            int pos = allpassPosL[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = diffL + apInG * bufOut;
            diffL = -apInG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosL[i] = pos;
        }
        {
            auto& buf = allpassR[i];
            int pos = allpassPosR[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = diffR + apInG * bufOut;
            diffR = -apInG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosR[i] = pos;
        }
    }

    const int endTapL = (velvetPos - tapDelaysL[10] + maxVelvetLen) % maxVelvetLen;
    const int endTapR = (velvetPos - tapDelaysR[10] + maxVelvetLen) % maxVelvetLen;
    float tankEndL = velvetBufL[static_cast<size_t>(endTapL)];
    float tankEndR = velvetBufR[static_cast<size_t>(endTapR)];

    dampStateL += 0.24f * (tankEndL - dampStateL);
    dampStateR += 0.24f * (tankEndR - dampStateR);

    const float tailFb = 0.62f + 0.26f * juce::jlimit(0.0f, 1.0f, decay);
    float loopL = diffL + dampStateR * tailFb;
    float loopR = diffR + dampStateL * tailFb;

    const float apTankG = 0.56f;
    for (int i = 4; i < kNumAllpass; ++i) {
        {
            auto& buf = allpassL[i];
            int pos = allpassPosL[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = loopL + apTankG * bufOut;
            loopL = -apTankG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosL[i] = pos;
        }
        {
            auto& buf = allpassR[i];
            int pos = allpassPosR[i];
            float bufOut = buf[static_cast<size_t>(pos)];
            float w = loopR + apTankG * bufOut;
            loopR = -apTankG * w + bufOut;
            buf[static_cast<size_t>(pos)] = w;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
            allpassPosR[i] = pos;
        }
    }

    velvetBufL[static_cast<size_t>(velvetPos)] = loopL;
    velvetBufR[static_cast<size_t>(velvetPos)] = loopR;

    auto readL = [&](int idx) -> float {
        int r = velvetPos - tapDelaysL[idx];
        if (r < 0) r += maxVelvetLen;
        return velvetBufL[static_cast<size_t>(r)];
    };
    auto readR = [&](int idx) -> float {
        int r = velvetPos - tapDelaysR[idx];
        if (r < 0) r += maxVelvetLen;
        return velvetBufR[static_cast<size_t>(r)];
    };

    float sumL = 0.38f * (readL(1) + readL(4) + readR(7)  + 0.75f * readL(9)  - 0.55f * readR(3) - 0.45f * readL(11));
    float sumR = 0.38f * (readR(1) + readR(4) + readL(7)  + 0.75f * readR(9)  - 0.55f * readL(3) - 0.45f * readR(11));

    if (++velvetPos >= maxVelvetLen) velvetPos = 0;

    sumL = reverbToneFilter.processL(sumL);
    sumR = reverbToneFilter.processR(sumR);

    const float wet1 = 0.5f * (1.0f + juce::jlimit(0.0f, 1.5f, width));
    const float wet2 = 0.5f * (1.0f - juce::jlimit(0.0f, 1.5f, width));
    outL = (sumL * wet1 + sumR * wet2) * 1.15f;
    outR = (sumR * wet1 + sumL * wet2) * 1.15f;
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
    tapeDelay.prepare(sampleRate);
    reverb.prepare(sampleRate);
    dcBlockerPhys.setHighpass(sampleRate, 32.0f, 0.707f);
    dcBlockerStudio.setHighpass(sampleRate, 28.0f, 0.707f);
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
    mpeChannelPitchNorm.fill(0.0f);
    mpeChannelPressure.fill(0.0f);
    modWheelNorm = 0.0f;
    expressionNorm = 1.0f;
    keyswitchPedals.fill(0.0f);
    smoothPedalA = smoothPedalB = smoothPedalC = smoothLKL = smoothLKR = 0.0f;
    smoothVolPedal = params.volumePedal;
    smoothPitchWheel = 0.0f;
    smoothPitchWheelVel = 0.0f;
    globalVibratoPhase = 0.0;
    globalSwellEnv = 1.0f;

    midScoopFilter.reset();
    pickupPeakFilter.reset();
    speakerBodyFilter.reset();
    speakerPresenceFilter.reset();
    dcBlockerPhys.reset();

    studioDeltaCoilFilter.reset();
    studioDeltaWarmthFilter.reset();
    studioDeltaTrebleFilter.reset();
    dcBlockerStudio.reset();

    sympathetic.reset();
    tapeDelay.reset();
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

void PedalSteelEngine::allNotesOff() {
    for (auto& v : voices) {
        v.held = false;
        v.sustainedByPedal = false;
        v.uiTriggered = false;
        v.releaseAgeSec = 0.0;
    }
    keyswitchPedals.fill(0.0f);
    sustainPedalDown = false;
}

void PedalSteelEngine::updateFilterCoeffs() {
    const int st = juce::jlimit(0, 3, params.presetStyle);
    const float coilFreq = juce::jlimit(1200.0f, 3400.0f, params.coilPeakFreq);
    const float coilQ    = 0.90f + 0.75f * params.coilResonance;
    const float coilGain = 1.6f + 2.4f * params.coilResonance;
    pickupPeakFilter.setPeak(sampleRate, coilFreq, coilQ, coilGain);

    float bodyFreq = 265.0f;
    float bodyGain = 2.2f + 2.6f * params.toneWarmth;

    if (st == 0) {
        midScoopFilter.setPeak(sampleRate, 1280.0f, 0.95f, -1.6f);
        bodyFreq = 285.0f;
        speakerPresenceFilter.setPeak(sampleRate, 2450.0f, 0.92f, 5.2f);
    } else if (st == 1) {
        midScoopFilter.setPeak(sampleRate, 1240.0f, 0.90f, -2.4f);
        bodyFreq = 275.0f;
        speakerPresenceFilter.setPeak(sampleRate, 2650.0f, 0.88f, 5.0f + 2.4f * params.ampDrive);
    } else if (st == 2) {
        midScoopFilter.setPeak(sampleRate, 1260.0f, 0.85f, -2.2f);
        bodyFreq = 245.0f;
        bodyGain = 4.6f + 2.6f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 2480.0f, 0.90f, 4.4f + 2.0f * params.ampDrive);
    } else {
        midScoopFilter.setPeak(sampleRate, 1150.0f, 0.80f, -2.0f);
        bodyFreq = 240.0f;
        speakerPresenceFilter.setPeak(sampleRate, 2250.0f, 0.85f, 2.8f);
    }
    speakerBodyFilter.setPeak(sampleRate, bodyFreq, 0.78f, bodyGain);

    // Delta-calibrated filters for the 44.1 kHz Stereo Studio Bus (0.0 dB transparent at preset baseline!)
    const float deltaCoilDb   = juce::jlimit(-6.0f, 6.0f, (params.coilResonance - kBaseCoilRes[st]) * 10.0f);
    const float deltaWarmthDb = juce::jlimit(-6.5f, 6.5f, (params.toneWarmth - kBaseWarmth[st]) * 9.5f);
    const float deltaDriveAir = juce::jlimit(-3.5f, 4.5f, (params.ampDrive - kBaseDrive[st]) * 4.5f);

    studioDeltaCoilFilter.setPeak(sampleRate, coilFreq, 1.05f, deltaCoilDb);
    studioDeltaWarmthFilter.setPeak(sampleRate, 255.0f, 0.75f, deltaWarmthDb);
    studioDeltaTrebleFilter.setPeak(sampleRate, 3100.0f, 0.80f, -0.65f * deltaWarmthDb + deltaDriveAir);
}

float PedalSteelEngine::nextRandomSigned() {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(static_cast<int32_t>(rngState)) * (1.0f / 2147483648.0f);
}

int PedalSteelEngine::selectBestMultiSample(int midiNote, int style, float velocity) const {
    int bestIdx = 0;
    float bestScore = 1e9f;
    const bool wantAccent = (velocity >= 0.93f);
    for (int i = 0; i < PedalSteelMultiSamples::kNumMultiSamples; ++i) {
        const auto& ms = PedalSteelMultiSamples::kMultiSamples[i];
        float d = static_cast<float>(std::abs(midiNote - ms.midiNote));
        float stylePenalty = 0.0f;
        if (ms.style != style) {
            if (style == 3 || ms.style == 3) {
                stylePenalty = 26.0f; // Never mix bowed/swell pad zones with fingerpicked styles
            } else {
                stylePenalty = 5.5f;  // Allow clean plucked styles to share extreme bass/treble zones (>5 semis away)
            }
        }
        float velPenalty = (ms.velLayer == 1 && !wantAccent) ? 1.2f : ((ms.velLayer == 1 && wantAccent) ? -0.6f : 0.0f);
        float score = d + stylePenalty + velPenalty;
        if (score < bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    return bestIdx;
}

int PedalSteelEngine::selectPhraseSliceBySlot(int slotIndex, int style) const {
    const int st = juce::jlimit(0, 3, style);
    const int sl = juce::jlimit(0, 4, slotIndex);
    for (int i = 0; i < PedalSteelMultiSamples::kNumPhraseSlices; ++i) {
        const auto& ps = PedalSteelMultiSamples::kPhraseSlices[i];
        if (ps.style == st && ps.slotIndex == sl) return i;
    }
    return 0;
}

static inline void readStereoFrameHermiteRaw(const int16_t* stereoData, int maxFrames, double absFramePos, float& outL, float& outR) {
    int i1 = static_cast<int>(std::floor(absFramePos));
    float frac = static_cast<float>(absFramePos - std::floor(absFramePos));
    auto clampIdx = [&](int idx) -> int {
        return (idx < 0) ? 0 : ((idx >= maxFrames) ? (maxFrames - 1) : idx);
    };
    int i0  = clampIdx(i1 - 1);
    int i1c = clampIdx(i1);
    int i2  = clampIdx(i1 + 1);
    int i3  = clampIdx(i1 + 2);

    const float scale = 1.0f / 32767.0f;
    {
        const float y0 = stereoData[i0 * 2]  * scale;
        const float y1 = stereoData[i1c * 2] * scale;
        const float y2 = stereoData[i2 * 2]  * scale;
        const float y3 = stereoData[i3 * 2]  * scale;
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        outL = ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
    {
        const float y0 = stereoData[i0 * 2 + 1]  * scale;
        const float y1 = stereoData[i1c * 2 + 1] * scale;
        const float y2 = stereoData[i2 * 2 + 1]  * scale;
        const float y3 = stereoData[i3 * 2 + 1]  * scale;
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        outR = ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
}

void PedalSteelEngine::readStereoMultiSampleHermite(int sampleIdx, double relPos, float& outL, float& outR) const {
    const auto& ms = PedalSteelMultiSamples::kMultiSamples[juce::jlimit(0, PedalSteelMultiSamples::kNumMultiSamples - 1, sampleIdx)];
    const int16_t* stereoData = PedalSteelMultiSamples::getSingleNoteStereoData();
    const int maxFrames = PedalSteelMultiSamples::kTotalMultiSampleFrames;

    const double loopStartRel = static_cast<double>(ms.loopStartFrame - ms.startFrame);
    const double loopLen      = static_cast<double>(std::max(64, ms.loopEndFrame - ms.loopStartFrame));
    const double xfadeLen     = std::min({ static_cast<double>(ms.xfadeFrames), loopLen * 0.45, loopStartRel });

    if (relPos < loopStartRel) {
        readStereoFrameHermiteRaw(stereoData, maxFrames, static_cast<double>(ms.startFrame) + relPos, outL, outR);
    } else {
        double loopOffset = std::fmod(relPos - loopStartRel, loopLen);
        if (loopOffset < 0.0) loopOffset += loopLen;
        double absPosA = static_cast<double>(ms.loopStartFrame) + loopOffset;
        readStereoFrameHermiteRaw(stereoData, maxFrames, absPosA, outL, outR);

        // Zero-crossing phase-locked 12-period raised-cosine dual-reader crossfade
        if (loopOffset >= loopLen - xfadeLen && xfadeLen > 1.0) {
            double xfProg = (loopOffset - (loopLen - xfadeLen)) / xfadeLen;
            float alpha = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * xfProg)));
            double absPosB = static_cast<double>(ms.loopStartFrame) - xfadeLen + (loopOffset - (loopLen - xfadeLen));
            float bL = 0.0f, bR = 0.0f;
            readStereoFrameHermiteRaw(stereoData, maxFrames, absPosB, bL, bR);
            outL = (1.0f - alpha) * outL + alpha * bL;
            outR = (1.0f - alpha) * outR + alpha * bR;
        }
    }
    outL *= ms.normGain;
    outR *= ms.normGain;
}

void PedalSteelEngine::readStereoPhraseSliceHermite(int phraseIdx, double relPos, float& outL, float& outR) const {
    const auto& ps = PedalSteelMultiSamples::kPhraseSlices[juce::jlimit(0, PedalSteelMultiSamples::kNumPhraseSlices - 1, phraseIdx)];
    const int16_t* stereoData = PedalSteelMultiSamples::getStyleStereoData(ps.style);
    const int maxFrames = PedalSteelMultiSamples::getStyleStereoFrames(ps.style);
    const double sliceLen = static_cast<double>(std::max(1, ps.endFrame - ps.startFrame));
    if (relPos < 0.0 || relPos >= sliceLen) {
        outL = outR = 0.0f;
        return;
    }
    readStereoFrameHermiteRaw(stereoData, maxFrames, static_cast<double>(ps.startFrame) + relPos, outL, outR);

    // Smooth 3 ms onset ramp & 10 ms tail ramp at phrase boundaries
    if (ps.startFrame > 0 && relPos < 132.0) {
        const float edgeGain = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * (relPos / 132.0))));
        outL *= edgeGain;
        outR *= edgeGain;
    }
    if (ps.endFrame < maxFrames && relPos > sliceLen - 441.0 && sliceLen > 900.0) {
        const double rem = sliceLen - relPos;
        const float tailGain = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * (rem / 441.0))));
        outL *= tailGain;
        outR *= tailGain;
    }
}

void PedalSteelEngine::startPhraseVoice(int phraseIdx, int midiNote, int channel, float vel, float transposeSemis, bool fromUi) {
    // Stop any previous phrase voice cleanly so overlapping phrase triggers don't double up
    for (auto& ov : voices) {
        if (ov.active && ov.isPhraseSlice) {
            ov.held = false;
            ov.sustainedByPedal = false;
            ov.uiTriggered = false;
            ov.releaseAgeSec = 0.0;
        }
    }

    int strIdx = 0;
    uint64_t oldestOrder = UINT64_MAX;
    for (int i = 0; i < kNumStrings; ++i) {
        if (!voices[i].active) { strIdx = i; break; }
        if (voices[i].triggerOrder < oldestOrder) { oldestOrder = voices[i].triggerOrder; strIdx = i; }
    }

    const auto& ps = PedalSteelMultiSamples::kPhraseSlices[juce::jlimit(0, PedalSteelMultiSamples::kNumPhraseSlices - 1, phraseIdx)];
    auto& v = voices[strIdx];
    v.active = true;
    v.held = true;
    v.sustainedByPedal = false;
    v.uiTriggered = fromUi;
    v.isPhraseSlice = true;
    v.phraseSliceIdx = phraseIdx;
    v.phraseTransposeSemis = transposeSemis;
    v.midiNote = midiNote;
    v.channel = channel;
    v.velocity = vel;
    v.noteAgeSec = 0.0;
    v.releaseAgeSec = 0.0;
    v.triggerOrder = ++noteCounter;
    v.barTargetMidi = static_cast<float>(ps.rootMidiNote) + transposeSemis;
    v.barCurrentMidi = v.barTargetMidi;
    v.barVelocity = 0.0f;
    v.pedalCurrentSemis = 0.0f;
    v.pedalVel = 0.0f;
    v.mpePitchSemis = 0.0f;
    v.mpePitchVel = 0.0f;
    v.scoopOffsetSemis = 0.0f;
    v.scoopVel = 0.0f;
    v.ampEnv = juce::jlimit(0.35f, 1.0f, 0.30f + 0.70f * vel);
    v.swellEnv = 1.0f;
    v.vibratoEnv = 0.0f;
    v.samplePos = 0.0;
    v.transientGain = 0.0f;
}

void PedalSteelEngine::reassignChordRoles() {
    std::array<int, kNumStrings> activeIdx{};
    int count = 0;
    for (int i = 0; i < kNumStrings; ++i) {
        if (voices[i].active && !voices[i].isPhraseSlice && (voices[i].held || voices[i].sustainedByPedal)) {
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
        if (v.active && !v.isPhraseSlice && v.midiNote == midiNote) {
            return i;
        }
    }

    int heldCount = 0;
    int recentlyReleasedCount = 0;
    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (!v.active || v.isPhraseSlice) continue;
        if (v.held || v.sustainedByPedal) ++heldCount;
        else if (v.releaseAgeSec < 0.16 && v.ampEnv > 0.05f) ++recentlyReleasedCount;
    }

    const bool allowReleasedGlide = (recentlyReleasedCount == 1) || (heldCount >= 1 && recentlyReleasedCount <= 1);
    const int maxOverlapGlideSemis = (params.playMode == 1) ? 0 : 2;
    const int maxReleaseGlideSemis = (params.playMode == 1) ? 7 : ((heldCount == 0) ? 5 : 2);
    int bestLegatoIdx = -1;
    int bestLegatoDist = 999;

    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (!v.active || v.isPhraseSlice) continue;

        int dist = std::abs(midiNote - v.midiNote);
        if (dist == 0) continue;

        const double maxRelAge = (dist <= 2) ? 0.025 : 0.016;
        bool canTransferReleased = (allowReleasedGlide && !v.held && !v.sustainedByPedal && v.releaseAgeSec < maxRelAge && v.ampEnv > 0.05f && dist <= maxReleaseGlideSemis);
        bool canTransferOverlappedPedal = (v.held && v.noteAgeSec > 0.030 && v.noteAgeSec < 0.140 && dist <= maxOverlapGlideSemis);

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
        const int ch = juce::jlimit(1, 16, msg.getChannel());

        // 1. Mechanical Pedal & Knee Lever Keyswitches (MIDI 24..31 = C0..G0)
        if (note >= 24 && note <= 31) {
            if (note == 24)      keyswitchPedals[0] = 1.0f; // Pedal A (+2)
            else if (note == 25) keyswitchPedals[2] = 1.0f; // Pedal C (+2)
            else if (note == 26) keyswitchPedals[1] = 1.0f; // Pedal B (+1)
            else if (note == 28) { keyswitchPedals[0] = 1.0f; keyswitchPedals[1] = 1.0f; } // A+B Slant
            else if (note == 29) keyswitchPedals[3] = 1.0f; // Knee LKL (+1)
            else if (note == 31) keyswitchPedals[4] = 1.0f; // Knee LKR (-1)
            return;
        }

        // 2. Channel-1 Low Studio Phrase Lick Keyswitches (MIDI 36..43 = C1..G1)
        if (ch == 1 && note >= 36 && note <= 43) {
            int slot = 0;
            if (note <= 36)      slot = 0;
            else if (note <= 38) slot = 1;
            else if (note <= 40) slot = 2;
            else if (note == 41 || note == 42) slot = 3;
            else                 slot = 4; // G1 (43) = Full Studio Loop
            if (slot < 4) selectedLickSlot = slot;
            const int pIdx = selectPhraseSliceBySlot(slot, params.presetStyle);
            startPhraseVoice(pIdx, note, ch, vel, 0.0f, false);
            return;
        }

        // 3. Legacy MIDI Channel 2 Full Studio Phrase Trigger
        if (ch == 2) {
            const int slot = (note >= 45) ? 4 : juce::jlimit(0, 4, (note - 36) / 2);
            const int pIdx = selectPhraseSliceBySlot(slot, params.presetStyle);
            startPhraseVoice(pIdx, note, ch, vel, 0.0f, false);
            return;
        }

        // 4. Play Mode 4 ("Studio Phrase Licks"): Chromatically play & transpose active Studio Lick across the keyboard!
        if (params.playMode == 4) {
            const int pIdx = selectPhraseSliceBySlot(selectedLickSlot, params.presetStyle);
            const auto& ps = PedalSteelMultiSamples::kPhraseSlices[pIdx];
            const float semiShift = juce::jlimit(-14.0f, 14.0f, static_cast<float>(note - ps.rootMidiNote));
            startPhraseVoice(pIdx, note, ch, vel, semiShift, false);
            return;
        }

        // 5. Standard Chromatic / Copedent / MPE 10-String E9th Voice Allocation
        bool isLegatoTransfer = false;
        float prevMidi = static_cast<float>(note);
        int strIdx = chooseBestStringForNote(note, vel, isLegatoTransfer, prevMidi);
        auto& v = voices[strIdx];

        v.active = true;
        v.held = true;
        v.sustainedByPedal = false;
        v.uiTriggered = false;
        v.isPhraseSlice = false;
        v.midiNote = note;
        v.channel = ch;
        v.velocity = vel;
        v.releaseAgeSec = 0.0;
        v.triggerOrder = ++noteCounter;

        if (isLegatoTransfer) {
            v.barCurrentMidi = prevMidi;
            v.barTargetMidi = static_cast<float>(note);
            v.scoopOffsetSemis = 0.0f;
            v.scoopVel = 0.0f;
            v.ampEnv = std::min(1.0f, std::max(v.ampEnv, 0.45f * vel) * 1.05f);
            v.vibratoEnv *= 0.2f;
            v.transientGain = 0.0f;
        } else {
            v.multiSampleIdx = selectBestMultiSample(note, params.presetStyle, vel);
            v.noteAgeSec = 0.0;
            v.barTargetMidi = static_cast<float>(note);
            v.barCurrentMidi = static_cast<float>(note);
            v.barVelocity = 0.0f;
            v.pedalCurrentSemis = 0.0f;
            v.pedalVel = 0.0f;
            v.mpePitchSemis = (ch >= 3) ? (mpeChannelPitchNorm[static_cast<size_t>(ch)] * 12.0f) : 0.0f;
            v.mpePitchVel = 0.0f;
            v.ampEnv = std::max(v.ampEnv * 0.25f, 0.01f);
            v.swellEnv = 1.0f - 0.96f * params.autoSwell;
            v.vibratoEnv = 0.0f;
            v.samplePos = 0.0;
            v.slideSvfBand = 0.0f;
            v.slideSvfLow = 0.0f;

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
                for (const auto& ov : voices) if (ov.active && !ov.isPhraseSlice && ov.held) ++totalActiveNow;

                if (totalActiveNow == 1) {
                    if (vel >= 0.72f) {
                        v.scoopOffsetSemis = -0.35f * params.scoopDepth;
                    }
                } else if (totalActiveNow >= 3) {
                    for (auto& ov : voices) {
                        if (ov.active && !ov.isPhraseSlice && ov.held && ov.noteAgeSec < 0.025) {
                            if (ov.role == StringRole::Third) {
                                ov.scoopOffsetSemis = -0.45f * params.scoopDepth;
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
        const int ch = juce::jlimit(1, 16, msg.getChannel());
        if (note >= 24 && note <= 31) {
            if (note == 24)      keyswitchPedals[0] = 0.0f;
            else if (note == 25) keyswitchPedals[2] = 0.0f;
            else if (note == 26) keyswitchPedals[1] = 0.0f;
            else if (note == 28) { keyswitchPedals[0] = 0.0f; keyswitchPedals[1] = 0.0f; }
            else if (note == 29) keyswitchPedals[3] = 0.0f;
            else if (note == 31) keyswitchPedals[4] = 0.0f;
            return;
        }

        for (auto& v : voices) {
            if (v.active && v.held && v.midiNote == note && (v.channel == ch || ch == 1)) {
                v.held = false;
                if (sustainPedalDown) {
                    v.sustainedByPedal = true;
                } else {
                    v.releaseAgeSec = 0.0;
                }
            }
        }
    } else if (msg.isPitchWheel()) {
        const int ch = juce::jlimit(1, 16, msg.getChannel());
        const float norm = static_cast<float>(msg.getPitchWheelValue() - 8192) * (1.0f / 8192.0f);
        mpeChannelPitchNorm[static_cast<size_t>(ch)] = norm;
        if (ch == 1 || ch == 2) {
            pitchWheelNorm = norm;
        }
    } else if (msg.isChannelPressure()) {
        const int ch = juce::jlimit(1, 16, msg.getChannel());
        mpeChannelPressure[static_cast<size_t>(ch)] = static_cast<float>(msg.getChannelPressureValue()) * (1.0f / 127.0f);
    } else if (msg.isAftertouch()) {
        const int ch = juce::jlimit(1, 16, msg.getChannel());
        mpeChannelPressure[static_cast<size_t>(ch)] = static_cast<float>(msg.getAfterTouchValue()) * (1.0f / 127.0f);
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
            allNotesOff();
        }
    }
}

void PedalSteelEngine::triggerAuditionChord(int chordIndex) {
    for (auto& v : voices) {
        v.held = false;
        v.sustainedByPedal = false;
        v.uiTriggered = false;
    }
    static constexpr int kChords[4][3] = {
        { 64, 68, 71 }, // E Major
        { 62, 67, 71 }, // G Major
        { 62, 66, 69 }, // D Major
        { 60, 64, 67 }  // C Major
    };
    const int idx = juce::jlimit(0, 3, chordIndex);
    const int savedMode = params.playMode;
    if (savedMode == 4) params.playMode = 0; // Ensure chord buttons always play real 3-string E9th triads!
    for (int i = 0; i < 3; ++i) {
        handleMidiMessage(juce::MidiMessage::noteOn(1, kChords[idx][i], 0.82f));
    }
    params.playMode = savedMode;
    // Let UI chord audition decay naturally like a plucked chord without sticking forever
    for (auto& v : voices) {
        if (v.active && !v.isPhraseSlice && v.noteAgeSec < 0.01) {
            v.uiTriggered = true;
        }
    }
}

void PedalSteelEngine::triggerPhraseAudition() {
    // If a UI-triggered phrase is currently playing, toggle it off cleanly
    for (auto& v : voices) {
        if (v.active && v.isPhraseSlice && v.uiTriggered && v.held) {
            v.held = false;
            v.uiTriggered = false;
            v.releaseAgeSec = 0.0;
            return;
        }
    }
    const int slot = uiAuditionLickCycle % 4;
    selectedLickSlot = slot;
    ++uiAuditionLickCycle;
    const int pIdx = selectPhraseSliceBySlot(slot, params.presetStyle);
    startPhraseVoice(pIdx, 36 + slot * 2, 1, 0.90f, 0.0f, true);
}

void PedalSteelEngine::triggerFretboardNote(int stringIndex, float fret) {
    const int s = juce::jlimit(0, kNumStrings - 1, stringIndex);
    const float targetMidi = static_cast<float>(kOpenStringMidi[static_cast<size_t>(s)]) + juce::jlimit(0.0f, 24.0f, fret);
    const int nearMidi = juce::jlimit(36, 96, static_cast<int>(std::round(targetMidi)));

    auto& v = voices[static_cast<size_t>(s)];
    v.active = true;
    v.held = true;
    v.sustainedByPedal = false;
    v.uiTriggered = true;
    v.isPhraseSlice = false;
    v.midiNote = nearMidi;
    v.channel = 1;
    v.velocity = 0.84f;
    v.noteAgeSec = 0.0;
    v.releaseAgeSec = 0.0;
    v.triggerOrder = ++noteCounter;
    v.multiSampleIdx = selectBestMultiSample(nearMidi, params.presetStyle, 0.84f);
    v.barTargetMidi = targetMidi;
    v.barCurrentMidi = targetMidi;
    v.barVelocity = 0.0f;
    v.pedalCurrentSemis = 0.0f;
    v.pedalVel = 0.0f;
    v.mpePitchSemis = 0.0f;
    v.mpePitchVel = 0.0f;
    v.scoopOffsetSemis = 0.0f;
    v.scoopVel = 0.0f;
    v.ampEnv = 0.02f;
    v.swellEnv = 1.0f - 0.90f * params.autoSwell;
    v.vibratoEnv = 0.0f;
    v.samplePos = 0.0;
    v.slideSvfBand = 0.0f;
    v.slideSvfLow = 0.0f;
    for (int h = 0; h < kMaxHarmonics; ++h) {
        v.harmPhases[static_cast<size_t>(h)] = std::fmod(kPi * static_cast<double>(h * (h + 1)) / static_cast<double>(kMaxHarmonics) + static_cast<double>(s) * 0.57, kTwoPi);
    }
    v.transientIdx = (params.presetStyle * 2 + (s % 2)) % PedalSteelAssets::kNumTransients;
    v.transientPos = 0.0;
    v.transientGain = params.pickSnap * (1.0f - 0.85f * params.autoSwell) * 0.90f;
    reassignChordRoles();
}

void PedalSteelEngine::slideFretboardNote(int stringIndex, float fret) {
    const int s = juce::jlimit(0, kNumStrings - 1, stringIndex);
    auto& v = voices[static_cast<size_t>(s)];
    if (v.active && v.held && !v.isPhraseSlice) {
        v.barTargetMidi = static_cast<float>(kOpenStringMidi[static_cast<size_t>(s)]) + juce::jlimit(0.0f, 24.0f, fret);
        v.midiNote = juce::jlimit(36, 96, static_cast<int>(std::round(v.barTargetMidi)));
    }
}

void PedalSteelEngine::releaseFretboardNote(int stringIndex) {
    const int s = juce::jlimit(0, kNumStrings - 1, stringIndex);
    auto& v = voices[static_cast<size_t>(s)];
    if (v.active && v.uiTriggered) {
        v.held = false;
        v.uiTriggered = false;
        v.releaseAgeSec = 0.0;
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
    st.activeLickSlot = selectedLickSlot;

    int activeCount = 0;
    float barSum = 0.0f;
    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (v.active && v.isPhraseSlice && v.ampEnv > 0.02f) {
            st.phrasePlaying = true;
        }
        st.stringEnergy[static_cast<size_t>(i)] = v.active ? juce::jlimit(0.0f, 1.0f, v.ampEnv) : 0.0f;
        st.stringPitchSemis[static_cast<size_t>(i)] = v.barCurrentMidi + v.pedalCurrentSemis + v.scoopOffsetSemis + v.mpePitchSemis;
        st.stringBendSemis[static_cast<size_t>(i)] = v.pedalCurrentSemis + v.scoopOffsetSemis + v.mpePitchSemis;
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
    const int stStyle = juce::jlimit(0, 3, params.presetStyle);

    float maxAftertouch = 0.0f;
    for (float p : mpeChannelPressure) maxAftertouch = std::max(maxAftertouch, p);

    const float targetA   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalA, uiPedals[0], keyswitchPedals[0], modWheelNorm }));
    const float targetB   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalB, uiPedals[1], keyswitchPedals[1], juce::jlimit(0.0f, 1.0f, (modWheelNorm - 0.12f) * 1.14f) }));
    const float targetC   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalC, uiPedals[2], keyswitchPedals[2] }));
    const float targetLKL = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKL, uiPedals[3], keyswitchPedals[3] }));
    const float targetLKR = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKR, uiPedals[4], keyswitchPedals[4] }));
    const float targetVol = juce::jlimit(0.0f, 1.0f, params.volumePedal * expressionNorm + 0.15f * maxAftertouch);

    const float pedalTimeSec = std::max(0.045f, params.pedalSpeed * 0.001f);
    const float pedalOmega   = 3.10f / pedalTimeSec;

    const float barTimeSec   = std::max(0.030f, params.barGlide * 0.001f);
    const float barOmega     = 2.85f / barTimeSec;

    const double effectiveSwellMs = params.tempoSync
        ? juce::jlimit(30.0, 1200.0, (60000.0 / juce::jlimit(40.0, 260.0, params.hostBpm)) * (params.swellTime / 400.0))
        : static_cast<double>(params.swellTime);
    const float swellRate = static_cast<float>(dt / std::max(0.025, effectiveSwellMs * 0.001));

    const double stringTauSec = 0.65 + 0.15 * std::max(1.0, static_cast<double>(params.sustainTime));
    const float sustainDecayRate = static_cast<float>(1.0 - std::exp(-dt / stringTauSec));
    const float releaseTimeSec = (params.playMode == 2) ? 0.55f : 0.28f;
    const float releaseDecayRate = static_cast<float>(1.0 - std::exp(-dt / releaseTimeSec));

    const float effectiveDelayMs = params.tempoSync
        ? static_cast<float>(computeSyncedDelayMs(params.delayTime, params.hostBpm))
        : params.delayTime;

    const float outGain  = juce::Decibels::decibelsToGain(params.outputLevel);
    const float coilFreq = juce::jlimit(1200.0f, 3400.0f, params.coilPeakFreq);
    const float coilQ    = 0.85f + 0.85f * params.coilResonance;

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

        const float pwOmega = std::min(barOmega, 8.2f);
        const float pwAccel = pwOmega * pwOmega * (pitchWheelNorm - smoothPitchWheel) - 2.0f * pwOmega * smoothPitchWheelVel;
        smoothPitchWheelVel += pwAccel * static_cast<float>(dt);
        smoothPitchWheel    += smoothPitchWheelVel * static_cast<float>(dt);

        globalVibratoPhase += kTwoPi * params.vibratoRate * dt;
        if (globalVibratoPhase >= kTwoPi) globalVibratoPhase -= kTwoPi;
        const float barVibratoSin = static_cast<float>(std::sin(globalVibratoPhase));

        const float totalPedalLoad = smoothPedalA + smoothPedalB + smoothPedalC;
        const float cabinetDropSemis = -0.022f * params.cabinetDrop * juce::jlimit(0.0f, 2.0f, totalPedalLoad);

        float physStringsL = 0.0f, physStringsR = 0.0f;
        float msStringsL   = 0.0f, msStringsR   = 0.0f;
        float phraseBusL   = 0.0f, phraseBusR   = 0.0f;
        float maxSwell = 0.0f;
        float maxSlideSpeed = 0.0f;

        for (int i = 0; i < kNumStrings; ++i) {
            auto& v = voices[i];
            if (!v.active) continue;

            v.noteAgeSec += dt;

            // Auto-release UI chord audition buttons after 2.4 seconds so they ring naturally without hanging forever
            if (v.uiTriggered && !v.isPhraseSlice && v.held && v.noteAgeSec > 2.4) {
                v.held = false;
                v.uiTriggered = false;
                v.releaseAgeSec = 0.0;
            }

            if (v.isPhraseSlice) {
                if (!v.held && !v.sustainedByPedal) {
                    v.releaseAgeSec += dt;
                    v.ampEnv -= v.ampEnv * static_cast<float>(1.0 - std::exp(-dt / 0.055));
                    if (v.ampEnv < 0.0002f) {
                        v.active = false;
                        v.ampEnv = 0.0f;
                        continue;
                    }
                }
                const auto& ps = PedalSteelMultiSamples::kPhraseSlices[juce::jlimit(0, PedalSteelMultiSamples::kNumPhraseSlices - 1, v.phraseSliceIdx)];
                const double sliceLen = static_cast<double>(std::max(1, ps.endFrame - ps.startFrame));
                if (v.samplePos >= sliceLen) {
                    v.active = false;
                    v.held = false;
                    v.uiTriggered = false;
                    v.ampEnv = 0.0f;
                    continue;
                }
                float roleTargetSemis = (v.role == StringRole::Third) ? (1.0f * smoothPedalB - 0.5f * smoothLKR)
                                      : (v.role == StringRole::Fifth) ? (2.0f * smoothPedalA + 2.0f * smoothPedalC)
                                      : (1.0f * smoothLKL - 1.0f * smoothLKR + 2.0f * smoothPedalC);
                const float pedalErr = roleTargetSemis - v.pedalCurrentSemis;
                const float pedalAccel = pedalOmega * pedalOmega * pedalErr - 1.95f * pedalOmega * v.pedalVel;
                v.pedalVel += pedalAccel * static_cast<float>(dt);
                v.pedalCurrentSemis += v.pedalVel * static_cast<float>(dt);

                const float liveBendSemis = v.phraseTransposeSemis + v.pedalCurrentSemis + smoothPitchWheel * 7.0f;
                const double pitchRatio = std::pow(2.0, static_cast<double>(liveBendSemis) / 12.0);
                float sliceL = 0.0f, sliceR = 0.0f;
                readStereoPhraseSliceHermite(v.phraseSliceIdx, v.samplePos, sliceL, sliceR);
                v.samplePos += (PedalSteelMultiSamples::kSampleRate / sampleRate) * pitchRatio;
                v.lastOutput = 0.5f * (sliceL + sliceR) * v.ampEnv;
                phraseBusL += sliceL * v.ampEnv;
                phraseBusR += sliceR * v.ampEnv;
                maxSwell = std::max(maxSwell, 1.0f);
                continue;
            }

            if (!v.held && !v.sustainedByPedal) {
                v.releaseAgeSec += dt;
                v.ampEnv -= v.ampEnv * releaseDecayRate;
                if (v.ampEnv < 0.0002f) {
                    v.active = false;
                    v.ampEnv = 0.0f;
                    continue;
                }
            } else {
                if (v.noteAgeSec < 0.016) {
                    const float atkRate = static_cast<float>(1.0 - std::exp(-dt / 0.0040));
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

            // 2. Mechanical Bellcrank Pedal Pull & MPE Per-Note Oblique Bend
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

            if (v.channel >= 3) {
                const float mpeTarget = mpeChannelPitchNorm[static_cast<size_t>(v.channel)] * 12.0f;
                const float mpeAccel = pedalOmega * pedalOmega * (mpeTarget - v.mpePitchSemis) - 1.95f * pedalOmega * v.mpePitchVel;
                v.mpePitchVel += mpeAccel * static_cast<float>(dt);
                v.mpePitchSemis += v.mpePitchVel * static_cast<float>(dt);
            } else {
                v.mpePitchSemis = 0.0f;
                v.mpePitchVel = 0.0f;
            }

            if (std::abs(v.scoopOffsetSemis) > 0.0001f || std::abs(v.scoopVel) > 0.0001f) {
                const float scoopAccel = -pedalOmega * pedalOmega * v.scoopOffsetSemis - 1.95f * pedalOmega * v.scoopVel;
                v.scoopVel += scoopAccel * static_cast<float>(dt);
                v.scoopOffsetSemis += v.scoopVel * static_cast<float>(dt);
            }

            // 3. Delayed Wrist-Roll Tonebar Vibrato (boosted expressively by Aftertouch!)
            const float chPress = mpeChannelPressure[static_cast<size_t>(v.channel)];
            const float pitchSpeedSemisPerSec = std::abs(v.barVelocity) + std::abs(v.pedalVel) + std::abs(v.mpePitchVel) + std::abs(v.scoopVel) + std::abs(smoothPitchWheelVel) * 7.0f;
            maxSlideSpeed = std::max(maxSlideSpeed, pitchSpeedSemisPerSec);
            if (pitchSpeedSemisPerSec < 0.8f && v.noteAgeSec > 0.24) {
                v.vibratoEnv = std::min(1.0f, v.vibratoEnv + static_cast<float>(dt * 2.4));
            } else {
                v.vibratoEnv = std::max(0.0f, v.vibratoEnv - static_cast<float>(dt * 6.0));
            }
            const float vibAmt = juce::jlimit(0.0f, 1.25f, params.vibratoDepth + 0.40f * chPress);
            const float vibratoSemis = barVibratoSin * (0.072f * vibAmt * v.vibratoEnv);

            const float cabDrop = (std::abs(roleTargetSemis) < 0.05f) ? cabinetDropSemis : 0.0f;
            const float pitchWheelSemis = smoothPitchWheel * 7.0f;
            const float instMidi = v.barCurrentMidi + v.pedalCurrentSemis + v.mpePitchSemis + v.scoopOffsetSemis + vibratoSemis + cabDrop + pitchWheelSemis;
            const float instFreqHz = juce::jlimit(40.0f, 4000.0f, midiToHz(instMidi));

            //==================================================================
            // Physical E9th String Harmonics + Chrome-Bar Winding Slide Whistle
            //==================================================================
            const float fretSemis = juce::jlimit(-4.0f, 26.0f, v.barCurrentMidi - static_cast<float>(kOpenStringMidi[i]));
            const float barStringLenInches = 24.25f * std::pow(2.0f, -fretSemis * (1.0f / 12.0f));
            const float betaPickup = juce::jlimit(0.05f, 0.46f, params.pickupPos / barStringLenInches);
            const float betaPick   = juce::jlimit(0.07f, 0.48f, (params.pickupPos + 0.95f) / barStringLenInches);

            const float regPos = juce::jlimit(0.0f, 2.999f, (instMidi - 54.0f) * (3.0f / 21.0f));
            const int regIdx0 = static_cast<int>(regPos);
            const int regIdx1 = std::min(3, regIdx0 + 1);
            const float regFrac = regPos - static_cast<float>(regIdx0);
            const int baseProf = stStyle * 4;
            const float* refHarm0 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx0];
            const float* refHarm1 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx1];

            float harmSum = 0.0f;
            const float nyquistLimit = fSr * 0.44f;
            const float ageF = static_cast<float>(v.noteAgeSec);

            const float barGlideSheen = juce::jlimit(0.0f, 1.0f, pitchSpeedSemisPerSec * 0.14f);
            const float pluckSnapEnv = std::exp(-ageF / (0.12f + 0.08f * params.pickSnap)) * (1.0f - 0.78f * params.autoSwell) * v.swellEnv;
            const float barMetalEnv  = (0.22f + 0.78f * std::exp(-ageF / 0.28f) + 0.45f * barGlideSheen) * (0.55f + 0.45f * params.pickSnap + 0.35f * params.ampDrive);

            for (int h = 1; h <= 12; ++h) {
                const float hFloat = static_cast<float>(h);
                const float disp = 1.0f + 0.000010f * static_cast<float>((h - 1) * (h - 1));
                const float fh = instFreqHz * hFloat * disp;
                if (fh >= nyquistLimit) break;

                double ph = v.harmPhases[static_cast<size_t>(h - 1)] + kTwoPi * static_cast<double>(fh) * dt;
                if (ph >= kTwoPi) ph -= kTwoPi;
                v.harmPhases[static_cast<size_t>(h - 1)] = ph;

                const float spatialComb = std::abs(std::sin(static_cast<float>(kPi) * hFloat * betaPick) *
                                                   std::sin(static_cast<float>(kPi) * hFloat * betaPickup));

                const float hDecayTau = (h <= 2)
                    ? 12.0f
                    : ((h == 3) ? (params.sustainTime * 0.65f) : (0.36f / static_cast<float>(h - 2)));
                const float hSustainDecay = (h <= 2)
                    ? 1.0f
                    : ((h == 3) ? std::exp(-ageF / std::max(0.8f, hDecayTau))
                                : (0.14f + 0.34f * barGlideSheen + 0.86f * std::exp(-ageF / std::max(0.09f, hDecayTau))));

                const float refH = refHarm0[h - 1] + regFrac * (refHarm1[h - 1] - refHarm0[h - 1]);
                float sustainWeight = 0.0f;
                if (h == 1) {
                    sustainWeight = (stStyle == 0) ? 0.86f : std::max(0.75f, refH);
                } else if (h == 2) {
                    float h2Base = (stStyle == 1) ? 0.92f : (stStyle == 0 ? 0.82f : (stStyle == 3 ? 0.56f : 0.62f));
                    sustainWeight = std::max(h2Base, refH * 0.94f);
                } else if (h == 3) {
                    float h3Scale = (stStyle == 0) ? 0.86f : (stStyle == 1 ? 0.44f : 0.56f);
                    sustainWeight = (refH * h3Scale + 0.14f * spatialComb) * hSustainDecay;
                } else {
                    float styleMetal = (stStyle == 0) ? 0.62f : (stStyle == 1 ? 0.42f : (stStyle == 2 ? 0.52f : 0.32f));
                    if (stStyle == 0 && h == 5) styleMetal *= 0.65f;
                    const float metalRing = (0.30f + 0.70f * spatialComb) * (1.0f / std::pow(hFloat - 1.5f, 1.25f));
                    sustainWeight = (refH * styleMetal * 0.38f + metalRing * styleMetal * barMetalEnv) * hSustainDecay;
                }

                const float twangWeight = (h >= 2)
                    ? ((0.25f + 0.75f * spatialComb) * (0.68f / std::pow(hFloat, 0.55f)) * pluckSnapEnv * (0.40f + 0.60f * params.pickSnap))
                    : 0.0f;

                const float fNorm = fh / coilFreq;
                const float denom = (1.0f - fNorm * fNorm) * (1.0f - fNorm * fNorm) + (fNorm / coilQ) * (fNorm / coilQ);
                const float lcGain = 1.0f / std::sqrt(std::max(0.42f, denom));
                const float highRegPurity = (fh > 3600.0f && h >= 4) ? std::exp(-(fh - 3600.0f) * 0.0010f) : 1.0f;

                harmSum += (sustainWeight + twangWeight) * lcGain * highRegPurity * static_cast<float>(std::sin(ph));
            }

            // Physical Chrome-Bar Winding Slide Layer (delicate high-Q metallic string-harmonic glint during glides!)
            float slideWhistle = 0.0f;
            const float barSlideRate = std::abs(v.barVelocity) + std::abs(smoothPitchWheelVel) * 7.0f + 0.35f * std::abs(v.pedalVel);
            if (params.barSlideNoise > 0.01f && barSlideRate > 1.1f) {
                const float woundFactor = (i >= 5) ? 1.25f : 0.55f;
                const float slideDrive = juce::jlimit(0.0f, 1.0f, (barSlideRate - 1.1f) * 0.065f) * params.barSlideNoise * woundFactor;
                const float svfCutoff = juce::jlimit(750.0f, 3200.0f, instFreqHz * 4.0f);
                const float svfF = 2.0f * std::sin(static_cast<float>(kPi) * svfCutoff / fSr);
                const float svfQ = 0.085f; // High-Q (~12) metallic string-winding harmonic whistle, never breathy white noise!
                const float noiseIn = nextRandomSigned() * slideDrive;
                v.slideSvfLow  += svfF * v.slideSvfBand;
                const float svfHigh = noiseIn - v.slideSvfLow - svfQ * v.slideSvfBand;
                v.slideSvfBand += svfF * svfHigh;
                slideWhistle = v.slideSvfBand * 0.028f;
            } else {
                v.slideSvfBand *= 0.988f;
                v.slideSvfLow  *= 0.988f;
            }

            // Concert-Calibrated De-Chorded 44.1 kHz Stereo Single-Note Multi-Sample Core
            float msL = 0.0f, msR = 0.0f;
            if (params.sampleBlend > 0.01f) {
                const auto& ms = PedalSteelMultiSamples::kMultiSamples[juce::jlimit(0, PedalSteelMultiSamples::kNumMultiSamples - 1, v.multiSampleIdx)];
                readStereoMultiSampleHermite(v.multiSampleIdx, v.samplePos, msL, msR);
                msL *= params.sampleBlend;
                msR *= params.sampleBlend;

                const double pitchRatio = static_cast<double>(instFreqHz / std::max(50.0f, ms.baseFreqHz));
                v.samplePos += (PedalSteelMultiSamples::kSampleRate / sampleRate) * pitchRatio;
            }

            // Real Metal Fingerpick Transient Grain
            float transientSig = 0.0f;
            if (v.transientPos < static_cast<double>(PedalSteelAssets::kTransientSize - 1) && v.transientGain > 0.001f) {
                int ti = static_cast<int>(v.transientPos);
                float tFrac = static_cast<float>(v.transientPos - ti);
                const float* tData = PedalSteelAssets::kTransients[v.transientIdx];
                float rawT = tData[ti] + tFrac * (tData[ti + 1] - tData[ti]);
                v.pickLpState += (0.42f + 0.50f * v.velocity) * (rawT - v.pickLpState);
                transientSig = v.pickLpState * v.transientGain;
                v.transientPos += 44100.0 / sampleRate;
            }

            const float swellShape = (params.autoSwell > 0.3f) ? (v.swellEnv * v.swellEnv) : v.swellEnv;
            const float envGain = v.ampEnv * swellShape;
            const float pan = (static_cast<float>(i) - 4.5f) * 0.045f;

            // Route pure 44.1 kHz studio multi-sample to Studio Bus (preserving un-colored studio tone!)
            const float sVoiceL = msL * envGain * (0.5f - pan) * 1.18f;
            const float sVoiceR = msR * envGain * (0.5f + pan) * 1.18f;
            msStringsL += sVoiceL;
            msStringsR += sVoiceR;

            // Route physical harmonics + fingerpick snap + chrome-bar slide whistle to Physical Amp Bus
            const float physHarmBlend = (1.0f - 0.86f * params.sampleBlend) * 0.24f + 0.022f;
            const float pMono = (harmSum * physHarmBlend + transientSig * 0.14f + slideWhistle) * envGain;
            physStringsL += pMono * (0.5f - pan) * 1.85f;
            physStringsR += pMono * (0.5f + pan) * 1.85f;

            v.lastOutput = 0.5f * (sVoiceL + sVoiceR) + pMono;
        }

        globalSwellEnv = (maxSwell > 0.0f) ? maxSwell : 1.0f;
        const float volGain = smoothVolPedal * smoothVolPedal * (1.5f - 0.5f * smoothVolPedal);

        //======================================================================
        // Bus A: Physical String, Pick & Chrome-Bar Slide -> Full Amp & Cabinet
        //======================================================================
        float physL = midScoopFilter.processL(physStringsL);
        float physR = midScoopFilter.processR(physStringsR);
        physL = pickupPeakFilter.processL(physL);
        physR = pickupPeakFilter.processR(physR);

        if (stStyle == 1 || stStyle == 2) {
            const float symAmt = (stStyle == 2) ? 0.20f : 0.10f;
            const float symSig = sympathetic.process(0.5f * (physL + physR), symAmt);
            physL += symSig;
            physR += symSig;
        }

        physL *= volGain;
        physR *= volGain;

        if (params.ampDrive > 0.02f) {
            const float busDrive = 1.0f + 1.55f * params.ampDrive;
            const float quadAsym = (stStyle == 2) ? (0.32f * params.ampDrive) : (0.18f * params.ampDrive);
            const float inL = physL * busDrive;
            const float inR = physR * busDrive;
            physL = (fastTanh(inL + quadAsym * inL * inL) - fastTanh(quadAsym * inL * inL) * 0.15f) / (1.0f + 0.50f * params.ampDrive);
            physR = (fastTanh(inR + quadAsym * inR * inR) - fastTanh(quadAsym * inR * inR) * 0.15f) / (1.0f + 0.50f * params.ampDrive);
        }

        physL = speakerBodyFilter.processL(physL);
        physR = speakerBodyFilter.processR(physR);
        physL = speakerPresenceFilter.processL(physL);
        physR = speakerPresenceFilter.processR(physR);
        physL = dcBlockerPhys.processL(physL) * 0.22f;
        physR = dcBlockerPhys.processR(physR) * 0.22f;

        //======================================================================
        // Bus B: 44.1 kHz Stereo Studio Bus -> Delta-Calibrated Tone & Extra Drive
        //======================================================================
        float stuSingleL = msStringsL * volGain * 0.62f;
        float stuSingleR = msStringsR * volGain * 0.62f;
        float stuPhraseL = phraseBusL * volGain;
        float stuPhraseR = phraseBusR * volGain;

        float stuTotalL = studioDeltaCoilFilter.processL(stuSingleL + stuPhraseL);
        float stuTotalR = studioDeltaCoilFilter.processR(stuSingleR + stuPhraseR);
        stuTotalL = studioDeltaWarmthFilter.processL(stuTotalL);
        stuTotalR = studioDeltaWarmthFilter.processR(stuTotalR);
        stuTotalL = studioDeltaTrebleFilter.processL(stuTotalL);
        stuTotalR = studioDeltaTrebleFilter.processR(stuTotalR);

        // Extra Tweed push when user turns TWEED DRIVE above the preset's studio baseline!
        const float extraDrive = std::max(0.0f, params.ampDrive - kBaseDrive[stStyle]);
        if (extraDrive > 0.01f) {
            const float g = 1.0f + 2.4f * extraDrive;
            const float asym = 0.22f * extraDrive;
            stuTotalL = (fastTanh(stuTotalL * g + asym * stuTotalL * stuTotalL) - fastTanh(asym * stuTotalL * stuTotalL) * 0.15f) / (1.0f + 0.55f * extraDrive);
            stuTotalR = (fastTanh(stuTotalR * g + asym * stuTotalR * stuTotalR) - fastTanh(asym * stuTotalR * stuTotalR) * 0.15f) / (1.0f + 0.55f * extraDrive);
        }
        stuTotalL = dcBlockerStudio.processL(stuTotalL);
        stuTotalR = dcBlockerStudio.processR(stuTotalR);

        //======================================================================
        // Master Stereo Tape / BBD Delay + Dattorro Figure-8 Plate Reverb
        //======================================================================
        const float dryCombinedL = physL + stuTotalL;
        const float dryCombinedR = physR + stuTotalR;

        float dlyL = 0.0f, dlyR = 0.0f;
        tapeDelay.processSample(sampleRate, dryCombinedL, dryCombinedR, effectiveDelayMs, params.delayMix, maxSlideSpeed, dlyL, dlyR);

        // Chromatic single-note releases & physical strings feed full plate reverb; full studio phrases only add delta reverb above baseline!
        const float singleWet = juce::jlimit(0.0f, 1.0f, params.reverbMix);
        const float phraseExtraWet = juce::jlimit(0.0f, 1.0f, std::max(0.0f, params.reverbMix - kBaseRevMix[stStyle]) * 1.4f);
        const float slideReverbDuck = 1.0f / (1.0f + 0.05f * maxSlideSpeed);

        const float revSendL = ((physL + stuSingleL * 0.72f) * singleWet + stuPhraseL * phraseExtraWet + dlyL * 0.42f * singleWet) * slideReverbDuck;
        const float revSendR = ((physR + stuSingleR * 0.72f) * singleWet + stuPhraseR * phraseExtraWet + dlyR * 0.42f * singleWet) * slideReverbDuck;

        float revL = 0.0f, revR = 0.0f;
        reverb.processSample(revSendL, revSendR, params.reverbDecay, params.stereoWidth, revL, revR);

        float finalL = (dryCombinedL + dlyL + revL * 1.25f) * outGain;
        float finalR = (dryCombinedR + dlyR + revR * 1.25f) * outGain;

        // Mid/Side Stereo Panorama Width Control (1.0 = natural studio stereo, 0.0 = mono, 1.5 = wide)
        const float mid  = 0.5f * (finalL + finalR);
        const float side = 0.5f * (finalL - finalR) * juce::jlimit(0.0f, 1.5f, params.stereoWidth);
        finalL = mid + side;
        finalR = mid - side;

        outL[s] = juce::jlimit(-0.98f, 0.98f, finalL);
        if (outR) outR[s] = juce::jlimit(-0.98f, 0.98f, finalR);
    }
}
