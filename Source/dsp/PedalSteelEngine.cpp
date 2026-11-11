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

    // Studio baseline values per style (0..5) so default preset positions apply 0.0 dB extra coloration to studio samples
    constexpr float kBaseCoilRes[6]  = { 0.74f, 0.52f, 0.72f, 0.52f, 0.62f, 0.58f };
    constexpr float kBaseWarmth[6]   = { 0.38f, 0.42f, 0.56f, 0.84f, 0.68f, 0.74f };
    constexpr float kBaseDrive[6]    = { 0.24f, 0.42f, 0.56f, 0.16f, 0.34f, 0.32f };
    constexpr float kBaseRevMix[6]   = { 0.30f, 0.40f, 0.42f, 0.48f, 0.20f, 0.48f };

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
int PedalSteelEngine::getOpenStringMidiForStyle(int style, int stringIdx) {
    const size_t s = static_cast<size_t>(juce::jlimit(0, kNumStrings - 1, stringIdx));
    if (style == 4) return kOpenStringMidiC6[s];
    if (style == 5) return kOpenStringMidiB6[s];
    return kOpenStringMidi[s];
}

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
    lpCoeff = static_cast<float>(1.0 - std::exp(-kTwoPi * 1060.0 / std::max(8000.0, sr)));
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
    combLp[0] += lpCoeff * (sig - combLp[0]);
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

    const float srScale = static_cast<float>(44100.0 / std::max(8000.0, sr));
    const float targetSamples = juce::jlimit(32.0f, static_cast<float>(maxLen - 64), static_cast<float>(delayMs * 0.001 * sr));
    if (duckEnv < 0.0008f && std::abs(targetSamples - smoothDelaySamples) > static_cast<float>(0.020 * sr)) {
        smoothDelaySamples = targetSamples;
    } else {
        smoothDelaySamples += (targetSamples - smoothDelaySamples) * std::min(0.25f, 0.00085f * srScale);
    }

    // Subtle 0.56 Hz quadrature Echoplex capstan wow/flutter (±0.28 ms) + tight 2.6 ms stereo head spread
    // so rhythmic 1/8D echoes lock dead-on the groove pocket with lush 3D width (never flamming off-beat!)
    wowPhase += kTwoPi * 0.56 / sr;
    if (wowPhase >= kTwoPi) wowPhase -= kTwoPi;
    const float wowSamplesL = static_cast<float>(std::sin(wowPhase) * 0.00028 * sr);
    const float wowSamplesR = static_cast<float>(std::cos(wowPhase) * 0.00028 * sr);
    const float stereoHeadSpread = std::min(smoothDelaySamples * 0.03f, static_cast<float>(0.0026 * sr));

    const float delL = juce::jlimit(16.0f, static_cast<float>(maxLen - 8), smoothDelaySamples + wowSamplesL);
    const float delR = juce::jlimit(16.0f, static_cast<float>(maxLen - 8), smoothDelaySamples + stereoHeadSpread + wowSamplesR);

    const float tapL = readFloatBufHermite(bufL, writePos, delL);
    const float tapR = readFloatBufHermite(bufR, writePos, delR);

    // Warm analog Echoplex tape head bandpass (125 Hz HPF + 3.15 kHz LPF) & soft magnetic saturation
    const float lpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 3150.0 / sr));
    const float hpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 125.0 / sr));
    fbLpL += lpAlpha * (tapL - fbLpL);
    fbLpR += lpAlpha * (tapR - fbLpR);
    fbHpL += hpAlpha * (fbLpL - fbHpL);
    fbHpR += hpAlpha * (fbLpR - fbHpR);
    const float filteredL = fbLpL - fbHpL;
    const float filteredR = fbLpR - fbHpR;

    // Pedal/bar-slide harmonic guard: ducks stale pre-pull echoes when pedals or tonebar bend chords!
    const float targetSlideDuck = 1.0f / (1.0f + 0.30f * std::max(0.0f, slideSpeed - 0.40f));
    const float sdAlpha = std::min(0.5f, ((targetSlideDuck < slideDuckSmooth) ? 0.0045f : 0.00014f) * srScale);
    slideDuckSmooth += sdAlpha * (targetSlideDuck - slideDuckSmooth);

    const float feedback = (0.30f + 0.28f * juce::jlimit(0.0f, 1.0f, mix)) * (0.58f + 0.42f * slideDuckSmooth);
    // Wide cross-coupled stereo tape bounce
    const float writeL = fastTanh(inL * 0.88f + (filteredL * 0.66f + filteredR * 0.34f) * feedback);
    const float writeR = fastTanh(inR * 0.88f + (filteredR * 0.66f + filteredL * 0.34f) * feedback);

    bufL[static_cast<size_t>(writePos)] = writeL;
    bufR[static_cast<size_t>(writePos)] = writeR;
    if (++writePos >= maxLen) writePos = 0;

    // Musical dynamic ducking: echoes tuck gently on initial pick transients, then dance brightly in every 1/8D gap!
    const float inMag = std::max(std::abs(inL), std::abs(inR));
    const float atk = std::min(0.5f, ((inMag > duckEnv) ? 0.015f : 0.00055f) * srScale);
    duckEnv += atk * (inMag - duckEnv);
    const float duckGain = (1.0f / (1.0f + 0.68f * duckEnv)) * slideDuckSmooth;

    outWetL = filteredL * mix * duckGain * 1.08f;
    outWetR = filteredR * mix * duckGain * 1.08f;
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
    dampCoeff = static_cast<float>(1.0 - std::exp(-kTwoPi * 1925.5 / std::max(8000.0, sr)));
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

    dampStateL += dampCoeff * (tankEndL - dampStateL);
    dampStateR += dampCoeff * (tankEndR - dampStateR);

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
void PedalSteelEngine::ModernCharacterProcessor::prepare(double sr) {
    modMaxLen = std::max(512, static_cast<int>(sr * 0.068) + 32);
    modBufL.assign(static_cast<size_t>(modMaxLen), 0.0f);
    modBufR.assign(static_cast<size_t>(modMaxLen), 0.0f);

    shimMaxLen = std::max(8192, static_cast<int>(sr * 1.250) + 64);
    shimBufL.assign(static_cast<size_t>(shimMaxLen), 0.0f);
    shimBufR.assign(static_cast<size_t>(shimMaxLen), 0.0f);

    static constexpr int kBaseAp[4] = { 113, 179, 251, 347 };
    const double scale = sr / 44100.0;
    for (int i = 0; i < 4; ++i) {
        const int lenL = std::max(16, static_cast<int>(std::round(kBaseAp[i] * scale)));
        const int lenR = std::max(16, static_cast<int>(std::round((kBaseAp[i] + 11) * scale)));
        springApL[static_cast<size_t>(i)].assign(static_cast<size_t>(lenL), 0.0f);
        springApR[static_cast<size_t>(i)].assign(static_cast<size_t>(lenR), 0.0f);
        springApPosL[static_cast<size_t>(i)] = 0;
        springApPosR[static_cast<size_t>(i)] = 0;
    }
    lastCoeffMode = -1;
    reset();
}

void PedalSteelEngine::ModernCharacterProcessor::reset() {
    std::fill(modBufL.begin(), modBufL.end(), 0.0f);
    std::fill(modBufR.begin(), modBufR.end(), 0.0f);
    std::fill(shimBufL.begin(), shimBufL.end(), 0.0f);
    std::fill(shimBufR.begin(), shimBufR.end(), 0.0f);
    for (int i = 0; i < 4; ++i) {
        std::fill(springApL[static_cast<size_t>(i)].begin(), springApL[static_cast<size_t>(i)].end(), 0.0f);
        std::fill(springApR[static_cast<size_t>(i)].begin(), springApR[static_cast<size_t>(i)].end(), 0.0f);
        springApPosL[static_cast<size_t>(i)] = 0;
        springApPosR[static_cast<size_t>(i)] = 0;
    }
    modWritePos = 0;
    shimWritePos = 0;
    lfoPhase1 = 0.0;
    lfoPhase2 = 0.27;
    lfoPhase3 = 0.61;
    tremPhase = 0.0;
    shimPhasor = 0.0;
    subPhasor = 0.0;
    shimLpL = 0.0f;
    shimLpR = 0.0f;
    gateEnv = 0.0f;
    cassetteSagEnv = 0.0f;
    hornRateHz = 0.78;
    drumRateHz = 0.66;
    xoverLp1L = xoverLp1R = 0.0f;
    xoverLp2L = xoverLp2R = 0.0f;
    hornBeamLpL = hornBeamLpR = 0.0f;
    drumBeamLpL = drumBeamLpR = 0.0f;
    leslieDcL = leslieDcR = 0.0f;
    svf1Ic1L = svf1Ic2L = svf1Ic1R = svf1Ic2R = 0.0f;
    svf2Ic1L = svf2Ic2L = svf2Ic1R = svf2Ic2R = 0.0f;
    phaserStateL.fill(0.0f);
    phaserStateR.fill(0.0f);
    phaserFbL = 0.0f;
    phaserFbR = 0.0f;
    charFilter1.reset();
    charFilter2.reset();
    charFilter3.reset(); charFilter4.reset();
    dimHpFilter.reset();
    subLpFilter.reset();
    currentMode = targetMode;
    wetGain = (currentMode > 0) ? 1.0f : 0.0f;
    lastCoeffMode = -1;
    hasRendered = false;
}

void PedalSteelEngine::ModernCharacterProcessor::setTargetMode(double sr, int mode) {
    targetMode = juce::jlimit(0, 22, mode);
    if (!hasRendered) {
        currentMode = targetMode;
        wetGain = (currentMode > 0) ? 1.0f : 0.0f;
        applyModeCoeffs(sr, currentMode);
    } else if (currentMode == targetMode) {
        applyModeCoeffs(sr, currentMode);
    }
}

void PedalSteelEngine::ModernCharacterProcessor::applyModeCoeffs(double sr, int mode) {
    if (mode != lastCoeffMode) {
        charFilter1.reset();
        charFilter2.reset();
        charFilter3.reset(); charFilter4.reset();
        dimHpFilter.reset();
        subLpFilter.reset();
        phaserStateL.fill(0.0f);
        phaserStateR.fill(0.0f);
        phaserFbL = 0.0f;
        phaserFbR = 0.0f;
        cassetteSagEnv = 0.0f;
        shimLpL = 0.0f;
        shimLpR = 0.0f;
        gateEnv = 0.0f;
        hornRateHz = 0.78;
        drumRateHz = 0.66;
        xoverLp1L = xoverLp1R = 0.0f;
        xoverLp2L = xoverLp2R = 0.0f;
        hornBeamLpL = hornBeamLpR = 0.0f;
        drumBeamLpL = drumBeamLpR = 0.0f;
        leslieDcL = leslieDcR = 0.0f;
        svf1Ic1L = svf1Ic2L = svf1Ic1R = svf1Ic2R = 0.0f;
        svf2Ic1L = svf2Ic2L = svf2Ic1R = svf2Ic2R = 0.0f;
        shimWritePos = 0;
        shimPhasor = 0.0;
        subPhasor = 0.0;
        std::fill(shimBufL.begin(), shimBufL.end(), 0.0f);
        std::fill(shimBufR.begin(), shimBufR.end(), 0.0f);
        for (int i = 0; i < 4; ++i) {
            std::fill(springApL[static_cast<size_t>(i)].begin(), springApL[static_cast<size_t>(i)].end(), 0.0f);
            std::fill(springApR[static_cast<size_t>(i)].begin(), springApR[static_cast<size_t>(i)].end(), 0.0f);
            springApPosL[static_cast<size_t>(i)] = 0;
            springApPosR[static_cast<size_t>(i)] = 0;
        }
        lastCoeffMode = mode;
    }
    if (mode <= 0) return;

    if (mode == 1) {
        // 13: Tascam 424 4-Track Portastudio (138 Hz Head Bump + 880 Hz Intimate Bark + 4-Pole 3.45 kHz Cassette Roll-Off)
        charFilter1.setPeak(sr, 138.0f, 0.76f, 3.4f);
        charFilter2.setLowpass(sr, 3450.0f, 0.80f);
        subLpFilter.setLowpass(sr, 3650.0f, 0.66f);
        charFilter3.setPeak(sr, 880.0f, 0.82f, 2.2f);
    } else if (mode == 2) {
        // 14: Coles 4038 Ribbon + Teletronix LA-2A Opto Velvet (Plush 215 Hz Chest + Vocal Dip + 5.15 kHz Ribbon Silk — Zero Tin/Comb!)
        dimHpFilter.setHighpass(sr, 220.0f, 0.707f);
        subLpFilter.setLowpass(sr, 3600.0f, 0.707f);
        charFilter1.setPeak(sr, 215.0f, 0.72f, 2.8f);
        charFilter2.setPeak(sr, 2850.0f, 0.90f, -2.5f);
        charFilter3.setLowpass(sr, 5150.0f, 0.71f);
    } else if (mode == 3) {
        // 15: 4-Stage Script Phase 90 / Uni-Vibe Analog JFET Optical Phaser + Hiwatt DR103 Class-A Tube Warmth
        dimHpFilter.setHighpass(sr, 145.0f, 0.707f);
        subLpFilter.setLowpass(sr, 3200.0f, 0.707f);
        charFilter1.setPeak(sr, 165.0f, 0.74f, 2.4f);
        charFilter2.setPeak(sr, 2650.0f, 0.90f, -1.8f);
        charFilter3.setLowpass(sr, 4650.0f, 0.72f);
    } else if (mode == 4) {
        // 16: Accutronics 3-Spring Tank Drip + Deep Fender Vibrolux Roach Optical Bias Tremolo
        dimHpFilter.setHighpass(sr, 195.0f, 0.707f);
        charFilter2.setPeak(sr, 2450.0f, 1.35f, 4.2f);
        charFilter3.setLowpass(sr, 4350.0f, 0.76f);
    } else if (mode == 5) {
        // 17: Nocturnal Alt-R&B Resampled Sample Bed (Warm 200 Hz Sub-Chest + True 24 dB/oct 4-Pole 1.35 kHz Underwater Filter)
        dimHpFilter.setHighpass(sr, 180.0f, 0.707f);
        charFilter1.setPeak(sr, 200.0f, 0.74f, 3.2f);
        charFilter2.setLowpass(sr, 1350.0f, 0.86f);
        charFilter3.setLowpass(sr, 1420.0f, 0.68f);
    } else if (mode == 6) {
        // 18: Sleep Walk / Mac DeMarco Sunset Lounge (Plush 195 Hz Tube Cushion + Ribbon Silk Dip + 4.05 kHz Alnico 12" Cone)
        dimHpFilter.setHighpass(sr, 195.0f, 0.707f);
        charFilter1.setPeak(sr, 195.0f, 0.74f, 2.8f);
        charFilter2.setPeak(sr, 2550.0f, 0.88f, -2.6f);
        charFilter3.setLowpass(sr, 4050.0f, 0.72f);
    } else if (mode == 7) {
        // 19: Apollo Shimmer (Daniel Lanois / Brian Eno '83 Eventide H910 +12st Octave Regenerator + Warm Lexicon 224 Silk)
        dimHpFilter.setHighpass(sr, 215.0f, 0.707f);
        subLpFilter.setLowpass(sr, 2750.0f, 0.707f);
        charFilter1.setPeak(sr, 195.0f, 0.74f, 2.5f);
        charFilter2.setPeak(sr, 2750.0f, 0.90f, -2.2f);
        charFilter3.setLowpass(sr, 4750.0f, 0.71f);
    } else if (mode == 8) {
        // 21: 1965 Leslie 147 Rotary (6550 Push-Pull Tube Amp + 800 Hz LR2 Crossover + Dual-Motor Inertia + 3D Horn Beaming)
        charFilter1.setPeak(sr, 165.0f, 0.74f, 3.0f);
        charFilter2.setPeak(sr, 1020.0f, 0.85f, 1.8f);
        charFilter3.setLowpass(sr, 4450.0f, 0.72f);
    } else if (mode == 9) {
        // 22: Gated Diode Bloom (Rainger Reverb-X / Bon Iver Envelope-Ducked Room -> Silicon Diode Saturation + Soft Gate)
        dimHpFilter.setHighpass(sr, 210.0f, 0.707f);
        charFilter1.setPeak(sr, 205.0f, 0.74f, 2.5f);
        subLpFilter.setLowpass(sr, 2180.0f, 0.72f);
        charFilter2.setPeak(sr, 780.0f, 0.80f, 2.8f);
        charFilter3.setLowpass(sr, 4550.0f, 0.72f);
    } else if (mode == 10) {
        // 23: Standel 807 Chamber (1956 Standel 25L15 807 Beam-Tetrode + 15" JBL D130F + Capitol Studios Tile Chamber)
        dimHpFilter.setHighpass(sr, 235.0f, 0.707f);
        subLpFilter.setLowpass(sr, 3150.0f, 0.707f);
        charFilter1.setPeak(sr, 185.0f, 0.72f, 2.8f);
        charFilter2.setPeak(sr, 610.0f, 0.88f, -2.5f);
        charFilter3.setLowpass(sr, 4850.0f, 0.72f);
    } else if (mode == 11) {
        // 25: Bosstone Violin Fuzz (240 Hz Split: Clean Tube Bass Foundation + Jordan Bosstone BJT/Diode Violin Sustainer -> 15" JBL)
        dimHpFilter.setHighpass(sr, 240.0f, 0.72f);
        charFilter1.setLowpass(sr, 250.0f, 0.72f);
        charFilter2.setPeak(sr, 1120.0f, 0.95f, 3.4f);
        subLpFilter.setLowpass(sr, 2680.0f, 0.78f);
        charFilter3.setLowpass(sr, 3050.0f, 0.70f);
    } else if (mode == 12) {
        // 26: Talk-Box Vowel Filter (1964 Pete Drake Horn Driver + Dual-Vactrol F1+F2 + 2.55 kHz F3 Singer's Formant + Mouth Tube)
        dimHpFilter.setHighpass(sr, 140.0f, 0.707f);
        charFilter1.setLowpass(sr, 260.0f, 0.72f);
        charFilter2.setPeak(sr, 2550.0f, 1.35f, 3.2f);
        charFilter3.setLowpass(sr, 4350.0f, 0.74f);
    } else if (mode == 13) {
        // 27: 1937 Rickenbacker B6 Bakelite Lap Steel (1.5" Wrap-Around Horseshoe Magnet + Octal 6L6 Gibson EH-150 + RCA 44 Ribbon)
        dimHpFilter.setHighpass(sr, 145.0f, 0.72f);
        subLpFilter.setLowpass(sr, 2450.0f, 0.707f);
        charFilter1.setPeak(sr, 320.0f, 0.78f, 3.2f);
        charFilter2.setPeak(sr, 960.0f, 1.02f, 3.0f);
        charFilter3.setLowpass(sr, 3450.0f, 0.74f);
    } else if (mode == 14) {
        // 28: Abbey Road Tape ADT (Fairchild 670 / TG12345 Zener Limiter + Ken Townsend 33.5 ms Varispeed Studer J37 ADT)
        dimHpFilter.setHighpass(sr, 155.0f, 0.707f);
        subLpFilter.setLowpass(sr, 3850.0f, 0.72f);
        charFilter1.setPeak(sr, 195.0f, 0.74f, 2.6f);
        charFilter2.setPeak(sr, 2550.0f, 0.90f, 2.2f);
        charFilter3.setLowpass(sr, 5200.0f, 0.72f);
    } else if (mode == 15) {
        // 29: 1985 Bradshaw LA Multi-Rack (DBX 160X Spank + Aphex Type-C Bell Sheen + DyTronics CS-5 3-Phase Tri-Chorus + PCM70 Pan-Delays)
        dimHpFilter.setHighpass(sr, 235.0f, 0.707f);
        subLpFilter.setLowpass(sr, 6800.0f, 0.72f);
        charFilter1.setPeak(sr, 138.0f, 0.76f, 2.4f);
        charFilter2.setPeak(sr, 720.0f, 0.88f, -4.2f);
        charFilter3.setPeak(sr, 5400.0f, 0.78f, 3.8f);
    } else if (mode == 16) {
        // 30: Poly-Octave Organ (Phase-Locked Per-String 16'+8'+4' Studio Steel Choir -> Messina Bellows & Silk POG2 LPF)
        dimHpFilter.setHighpass(sr, 195.0f, 0.707f);
        subLpFilter.setLowpass(sr, 3150.0f, 0.72f);
        charFilter1.setPeak(sr, 185.0f, 0.74f, 2.2f);
        charFilter2.setPeak(sr, 920.0f, 0.85f, -1.6f);
        charFilter3.setLowpass(sr, 3850.0f, 0.72f);
    } else if (mode == 17) {
        // 31: Two Star VG-8 Bowed Steel (Roland VG-8 D62 BowedPad + Boss CE-2 / Env-Flanger + Ducked Reverb-X + Tascam 424 Glue)
        dimHpFilter.setHighpass(sr, 38.0f, 0.707f);       // Full E1 (41.2 Hz) & C#1 (34.6 Hz) -12st divided-string sub-octave foundation
        subLpFilter.setLowpass(sr, 3850.0f, 0.74f);       // Lush MN3007 / VG-8 chorus bandwidth (keeps violin & fingerpick air alive)
        charFilter1.setPeak(sr, 165.0f, 0.76f, 1.8f);     // Warm baritone / Tascam 424 head-bump chest
        charFilter2.setPeak(sr, 520.0f, 0.92f, 1.8f);     // Warm wooden VIO GUITAR cello/viola body resonance (non-boxy!)
        charFilter3.setPeak(sr, 3150.0f, 0.82f, 1.8f);    // Open bowed violin rosin & 4 kHz VG-8 ZDF filter scream
        charFilter4.setLowpass(sr, 10800.0f, 0.707f);     // Full hardware VG-8 console bandwidth for +80ms 4 kHz C1 blasts!
    } else if (mode == 18) {
        // 32: '66 Revolver Backwards Tape (Dual Overlapping -1.000x Reverse Studer J37 Tape Heads + 15-ips Oxide Silk)
        dimHpFilter.setHighpass(sr, 135.0f, 0.707f);
        charFilter1.setPeak(sr, 175.0f, 0.76f, 2.4f);
        charFilter2.setPeak(sr, 1580.0f, 0.92f, 2.6f);
        charFilter3.setLowpass(sr, 4150.0f, 0.74f);
        subLpFilter.setLowpass(sr, 3300.0f, 0.72f);
    } else if (mode == 19) {
        // 33: Mohan Veena Sitar-Slide (Per-String Parabolic Bone Jawari Bridge + Gourd Tumba Formants + Sympathetic Taraf Harp)
        dimHpFilter.setHighpass(sr, 145.0f, 0.707f);
        charFilter1.setPeak(sr, 248.0f, 0.85f, 2.8f);
        charFilter2.setPeak(sr, 1060.0f, 1.25f, 2.6f);
        charFilter3.setPeak(sr, 3250.0f, 1.15f, 3.4f);
        charFilter4.setLowpass(sr, 5600.0f, 0.74f);
        subLpFilter.setLowpass(sr, 3850.0f, 0.74f);
    } else if (mode == 20) {
        // 34: Atlanta Neo-Soul Syrup (Ross/Dyna-Comp Liquid Sustain + Anti-Twang Velvet Dip + 1976 Boss CE-1 MN3002 Ensemble)
        dimHpFilter.setHighpass(sr, 175.0f, 0.707f);
        subLpFilter.setLowpass(sr, 2950.0f, 0.74f);
        charFilter1.setPeak(sr, 218.0f, 0.74f, 2.8f);
        charFilter2.setPeak(sr, 2380.0f, 0.92f, -4.4f);
        charFilter3.setPeak(sr, 680.0f, 0.85f, 2.0f);
        charFilter4.setLowpass(sr, 3350.0f, 0.74f);
    } else if (mode == 21) {
        // 35: Binson Echorec Space-Fury (Pete Cornish + Hiwatt DR103 Singing Lead -> 4-Head Rotating Magnetic Steel Drum Swirl)
        dimHpFilter.setHighpass(sr, 175.0f, 0.72f);
        charFilter1.setLowpass(sr, 195.0f, 0.72f);
        charFilter2.setPeak(sr, 1280.0f, 0.88f, 2.8f);
        charFilter3.setLowpass(sr, 4150.0f, 0.74f);
        charFilter4.setBandpass(sr, 1080.0f, 0.80f);
        subLpFilter.setLowpass(sr, 2750.0f, 0.72f);
    } else if (mode == 22) {
        // 36: CS-80 Stadium Synth-Steel (Per-String Polyphonic IG0015 Sawtooth/Sub Brass + 3-Phase CS-80 Chorus + Stadium Echo)
        dimHpFilter.setHighpass(sr, 145.0f, 0.707f);
        subLpFilter.setLowpass(sr, 5200.0f, 0.74f);
        charFilter1.setPeak(sr, 132.0f, 0.76f, 2.6f);
        charFilter2.setPeak(sr, 980.0f, 0.88f, 1.8f);
        charFilter3.setLowpass(sr, 5800.0f, 0.74f);
    }
}

void PedalSteelEngine::ModernCharacterProcessor::processSample(
    double sr, int desiredMode, double hostBpm, bool tempoSync, float pedalActivity,
    float& ioL, float& ioR, float& outExtraRevL, float& outExtraRevR)
{
    outExtraRevL = 0.0f;
    outExtraRevR = 0.0f;
    if (modMaxLen < 32) return;

    targetMode = juce::jlimit(0, 22, desiredMode);
    if (!hasRendered) {
        currentMode = targetMode;
        wetGain = (currentMode > 0) ? 1.0f : 0.0f;
        applyModeCoeffs(sr, currentMode);
        hasRendered = true;
    }

    const float dryInL = ioL;
    const float dryInR = ioR;
    modBufL[static_cast<size_t>(modWritePos)] = dryInL;
    modBufR[static_cast<size_t>(modWritePos)] = dryInR;

    const double dt   = 1.0 / sr;
    const float fadeStep = static_cast<float>(dt / 0.0085); // Click-free 8.5 ms out + 8.5 ms in live mode switch!

    if (currentMode != targetMode) {
        wetGain = std::max(0.0f, wetGain - fadeStep);
        if (wetGain <= 0.0001f) {
            wetGain = 0.0f;
            currentMode = targetMode;
            applyModeCoeffs(sr, currentMode);
        }
    } else {
        const float goalWet = (currentMode > 0) ? 1.0f : 0.0f;
        if (wetGain < goalWet) wetGain = std::min(goalWet, wetGain + fadeStep);
        else if (wetGain > goalWet) wetGain = std::max(goalWet, wetGain - fadeStep);
    }

    const int mode = currentMode;
    if (mode <= 0 || wetGain <= 0.00001f) {
        if (++modWritePos >= modMaxLen) modWritePos = 0;
        return;
    }

    const float srMs    = static_cast<float>(sr * 0.001);
    const float srScale = static_cast<float>(44100.0 / std::max(8000.0, sr));

    if (mode == 1) {
        // 13: 4-Track Cassette (Dual-capstan wow/flutter + level-reactive Tascam 424 preamp sag & 4-pole 3.45 kHz cassette tape)
        lfoPhase1 += 0.58 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 1.37 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float wowL = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float wowR = static_cast<float>(std::sin(kTwoPi * lfoPhase1 + 0.28));
        const float drfL = static_cast<float>(std::sin(kTwoPi * lfoPhase2));
        const float drfR = static_cast<float>(std::cos(kTwoPi * lfoPhase2));
        const float flt  = static_cast<float>(std::sin(kTwoPi * lfoPhase1 * 9.5));

        const float dL = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (3.8f + 0.46f * wowL + 0.15f * drfL + 0.025f * flt) * srMs);
        const float dR = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (4.05f + 0.46f * wowR + 0.15f * drfR - 0.025f * flt) * srMs);

        float wL = readFloatBufHermite(modBufL, modWritePos, dL);
        float wR = readFloatBufHermite(modBufR, modWritePos, dR);

        const float pk = std::max(std::abs(wL), std::abs(wR));
        const float sagAlpha = std::min(0.5f, ((pk > cassetteSagEnv) ? 0.010f : 0.00022f) * srScale);
        cassetteSagEnv += sagAlpha * (pk - cassetteSagEnv);
        const float drv = 1.86f / (1.0f + 0.68f * cassetteSagEnv);

        const float xL = juce::jlimit(-3.0f, 3.0f, wL * drv);
        const float xR = juce::jlimit(-3.0f, 3.0f, wR * drv);
        wL = (fastTanh(xL + 0.24f * xL * xL) - 0.042f * xL * xL) * 0.64f;
        wR = (fastTanh(xR + 0.24f * xR * xR) - 0.042f * xR * xR) * 0.64f;

        wL = charFilter3.processL(subLpFilter.processL(charFilter2.processL(charFilter1.processL(wL))));
        wR = charFilter3.processR(subLpFilter.processR(charFilter2.processR(charFilter1.processR(wR))));
        ioL = wL;
        ioR = wR;
    } else if (mode == 2) {
        // 14: Coles 4038 Ribbon + Teletronix LA-2A Opto Velvet (0% dry comb-filtering; modulated pre-delay feeds ONLY Hall Reverb!)
        lfoPhase1 += 0.34 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float optoAlpha = std::min(0.5f, ((pk > cassetteSagEnv) ? 0.0032f : 0.000085f) * srScale);
        cassetteSagEnv += optoAlpha * (pk - cassetteSagEnv);
        const float la2aGain = 1.28f / (1.0f + 0.58f * cassetteSagEnv);

        const float xL = juce::jlimit(-2.6f, 2.6f, ioL * la2aGain);
        const float xR = juce::jlimit(-2.6f, 2.6f, ioR * la2aGain);
        float vL = (fastTanh(xL + 0.11f * xL * xL) - 0.020f * xL * xL) * 0.90f;
        float vR = (fastTanh(xR + 0.11f * xR * xR) - 0.020f * xR * xR) * 0.90f;

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(vL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(vR)));

        // Wide stereo Lexicon 480L velvet pre-delay (28 ms L / 41 ms R) sent strictly to Reverb (zero dry comb-filtering!)
        const float s1 = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float c1 = static_cast<float>(std::cos(kTwoPi * lfoPhase1));
        const float dRevL = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (28.0f + 1.35f * s1) * srMs);
        const float dRevR = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (41.0f + 1.35f * c1) * srMs);
        outExtraRevL = subLpFilter.processL(dimHpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dRevL))) * 0.48f;
        outExtraRevR = subLpFilter.processR(dimHpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dRevR))) * 0.48f;
    } else if (mode == 3) {
        // 15: 4-Stage Script Phase 90 / Uni-Vibe Analog JFET Phaser (0.24 Hz Breathe Sweep + Hiwatt DR103 Tube Stage)
        lfoPhase1 += 0.24 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float modL = 0.5f + 0.5f * static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float modR = 0.5f + 0.5f * static_cast<float>(std::sin(kTwoPi * lfoPhase1 + 0.52));

        const float fcL = 260.0f * std::pow(4.9f, modL); // 260..1274 Hz twin-notch optical sweep
        const float fcR = 260.0f * std::pow(4.9f, modR);
        const float gL  = std::tan(static_cast<float>(kPi) * fcL / static_cast<float>(sr));
        const float gR  = std::tan(static_cast<float>(kPi) * fcR / static_cast<float>(sr));
        const float a1L = (gL - 1.0f) / (gL + 1.0f);
        const float a1R = (gR - 1.0f) / (gR + 1.0f);

        float phL = juce::jlimit(-2.5f, 2.5f, dimHpFilter.processL(ioL) - 0.18f * fastTanh(phaserFbL));
        float phR = juce::jlimit(-2.5f, 2.5f, dimHpFilter.processR(ioR) - 0.18f * fastTanh(phaserFbR));

        for (size_t st = 0; st < 4; ++st) {
            const float yL = a1L * phL + phaserStateL[st];
            phaserStateL[st] = phL - a1L * yL;
            phL = yL;

            const float yR = a1R * phR + phaserStateR[st];
            phaserStateR[st] = phR - a1R * yR;
            phR = yR;
        }
        phaserFbL = phL;
        phaserFbR = phR;

        // Mix dry steel + 4-stage allpass phaser (creates 2 deep velvety optical notches while keeping <145 Hz bass solid!)
        const float uL = juce::jlimit(-2.6f, 2.6f, (ioL * 0.68f + phL * 0.52f) * 1.18f);
        const float uR = juce::jlimit(-2.6f, 2.6f, (ioR * 0.68f + phR * 0.52f) * 1.18f);
        const float hiwattL = (fastTanh(uL + 0.10f * uL * uL) - 0.018f * uL * uL) * 0.92f;
        const float hiwattR = (fastTanh(uR + 0.10f * uR * uR) - 0.018f * uR * uR) * 0.92f;

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(hiwattL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(hiwattR)));
        outExtraRevL = subLpFilter.processL(phL) * 0.34f;
        outExtraRevR = subLpFilter.processR(phR) * 0.34f;
    } else if (mode == 4) {
        // 16: Desert Mirage Spring + Deep Fender Optical Roach Bias Tremolo
        const float dwellL = readFloatBufHermite(modBufL, modWritePos, juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), 39.0f * srMs));
        const float dwellR = readFloatBufHermite(modBufR, modWritePos, juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), 43.0f * srMs));
        float spL = dimHpFilter.processL(ioL * 0.72f + dwellR * 0.46f);
        float spR = dimHpFilter.processR(ioR * 0.72f + dwellL * 0.46f);

        for (size_t i = 0; i < 4; ++i) {
            auto& bL = springApL[i];
            auto& bR = springApR[i];
            int idxL = springApPosL[i];
            int idxR = springApPosR[i];
            const float dL = bL[static_cast<size_t>(idxL)];
            const float dR = bR[static_cast<size_t>(idxR)];
            const float g = 0.64f;
            const float inApL = spL + dL * g;
            const float inApR = spR + dR * g;
            bL[static_cast<size_t>(idxL)] = inApL;
            bR[static_cast<size_t>(idxR)] = inApR;
            spL = dL - inApL * g;
            spR = dR - inApR * g;
            if (++idxL >= static_cast<int>(bL.size())) idxL = 0;
            if (++idxR >= static_cast<int>(bR.size())) idxR = 0;
            springApPosL[i] = idxL;
            springApPosR[i] = idxR;
        }
        spL = charFilter3.processL(charFilter2.processL(spL));
        spR = charFilter3.processR(charFilter2.processR(spR));

        const double tremHz = tempoSync
            ? juce::jlimit(1.5, 8.0, (juce::jlimit(50.0, 220.0, hostBpm) / 60.0) * 2.0)
            : 3.75;
        tremPhase += tremHz * dt;
        if (tremPhase >= 1.0) tremPhase -= 1.0;

        // Deep unmistakable optical roach cell wave (warm tube bias heartbeat)
        const float sL = 0.5f + 0.5f * static_cast<float>(std::sin(kTwoPi * tremPhase));
        const float sR = 0.5f + 0.5f * static_cast<float>(std::sin(kTwoPi * tremPhase + 0.26));
        const float modL = 0.34f + 0.66f * (sL * sL * (3.0f - 2.0f * sL));
        const float modR = 0.34f + 0.66f * (sR * sR * (3.0f - 2.0f * sR));

        const float drvL = (ioL * 0.88f + spL * 0.30f) * modL * 1.52f;
        const float drvR = (ioR * 0.88f + spR * 0.30f) * modR * 1.52f;
        ioL = fastTanh(juce::jlimit(-2.6f, 2.6f, drvL)) * 1.05f;
        ioR = fastTanh(juce::jlimit(-2.6f, 2.6f, drvR)) * 1.05f;
        outExtraRevL = spL * 0.48f * modL;
        outExtraRevR = spR * 0.48f * modR;
    } else if (mode == 5) {
        // 17: Late-Night R&B Sample Bed (33-RPM resampled vinyl drift + 24 dB/oct 1.35 kHz underwater sample-chop LP)
        lfoPhase1 += 0.33 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.46 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float dL = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (5.2f + 0.38f * static_cast<float>(std::sin(kTwoPi * lfoPhase1))) * srMs);
        const float dR = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (6.4f + 0.38f * static_cast<float>(std::cos(kTwoPi * lfoPhase2))) * srMs);

        const float chorL = dimHpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dL));
        const float chorR = dimHpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dR));

        float lpL = ioL * 0.80f + chorL * 0.34f;
        float lpR = ioR * 0.80f + chorR * 0.34f;
        lpL = charFilter3.processL(charFilter2.processL(charFilter1.processL(lpL))) * 1.34f;
        lpR = charFilter3.processR(charFilter2.processR(charFilter1.processR(lpR))) * 1.34f;
        ioL = lpL;
        ioR = lpR;
    } else if (mode == 6) {
        // 18: Sunset Slack-Key Lounge (Boss VB-2 1.55 Hz +/-9.8c Pitch Vibrato + Stereo CE-2 Halo + Warm 12AX7 Tube Honey)
        lfoPhase1 += 1.55 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.39 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;
        lfoPhase3 += 0.54 * dt; if (lfoPhase3 >= 1.0) lfoPhase3 -= 1.0;

        const float vibL  = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float vibR  = static_cast<float>(std::sin(kTwoPi * lfoPhase1 + 0.20));
        const float drfL  = static_cast<float>(std::sin(kTwoPi * lfoPhase2));
        const float drfR  = static_cast<float>(std::cos(kTwoPi * lfoPhase2));
        const float chorS = static_cast<float>(std::sin(kTwoPi * lfoPhase3));
        const float chorC = static_cast<float>(std::cos(kTwoPi * lfoPhase3));

        const float dVibL  = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (4.6f + 0.52f * vibL + 0.22f * drfL) * srMs);
        const float dVibR  = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (4.85f + 0.52f * vibR + 0.22f * drfR) * srMs);
        const float dChorL = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (10.4f + 1.15f * chorS) * srMs);
        const float dChorR = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (11.1f + 1.15f * chorC) * srMs);

        const float vL = readFloatBufHermite(modBufL, modWritePos, dVibL);
        const float vR = readFloatBufHermite(modBufR, modWritePos, dVibR);
        const float cL = dimHpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dChorL));
        const float cR = dimHpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dChorR));

        float uL = (vL * 0.84f + cL * 0.28f) * 1.32f;
        float uR = (vR * 0.84f + cR * 0.28f) * 1.32f;
        uL = (fastTanh(juce::jlimit(-2.8f, 2.8f, uL + 0.16f * uL * uL)) - 0.028f * uL * uL) * 0.86f;
        uR = (fastTanh(juce::jlimit(-2.8f, 2.8f, uR + 0.16f * uR * uR)) - 0.028f * uR * uR) * 0.86f;

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(uL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(uR)));
    } else if (mode == 7 && shimMaxLen > 64) {
        // 19: Apollo Shimmer (Daniel Lanois / Brian Eno Eventide H910 +12st Dual-Grain Shimmer -> Warm Lexicon 224 Hall)
        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float optoAlpha = std::min(0.5f, ((pk > cassetteSagEnv) ? 0.0030f : 0.000080f) * srScale);
        cassetteSagEnv += optoAlpha * (pk - cassetteSagEnv);
        const float laGain = 1.22f / (1.0f + 0.52f * cassetteSagEnv);

        const float xL = juce::jlimit(-2.6f, 2.6f, ioL * laGain);
        const float xR = juce::jlimit(-2.6f, 2.6f, ioR * laGain);
        const float warmDryL = charFilter3.processL(charFilter2.processL(charFilter1.processL(fastTanh(xL) * 0.94f)));
        const float warmDryR = charFilter3.processR(charFilter2.processR(charFilter1.processR(fastTanh(xR) * 0.94f)));

        // Pre-filter input into +12st pitch-regenerator (215 Hz..2.75 kHz) so shifted octave stays velvety & aurora-like
        const float preL = subLpFilter.processL(dimHpFilter.processL(warmDryL));
        const float preR = subLpFilter.processR(dimHpFilter.processR(warmDryR));

        shimBufL[static_cast<size_t>(shimWritePos)] = fastTanh(preL + shimLpR * 0.25f);
        shimBufR[static_cast<size_t>(shimWritePos)] = fastTanh(preR + shimLpL * 0.25f);

        const float grainSamples = juce::jlimit(256.0f, static_cast<float>(shimMaxLen - 32), static_cast<float>(0.092 * sr));
        shimPhasor += 1.0035 / static_cast<double>(grainSamples);
        if (shimPhasor >= 1.0) shimPhasor -= 1.0;
        subPhasor += 0.9965 / static_cast<double>(grainSamples);
        if (subPhasor >= 1.0) subPhasor -= 1.0;

        const double pLa = shimPhasor;
        const double pLb = (pLa + 0.5 >= 1.0) ? (pLa - 0.5) : (pLa + 0.5);
        const float wLa = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pLa)));
        const float wLb = 1.0f - wLa;
        const float dLa = static_cast<float>(1.0 - pLa) * grainSamples + 12.0f;
        const float dLb = static_cast<float>(1.0 - pLb) * grainSamples + 12.0f;
        const float octL = wLa * readFloatBufHermite(shimBufL, shimWritePos, dLa)
                         + wLb * readFloatBufHermite(shimBufL, shimWritePos, dLb);

        const double pRa = subPhasor;
        const double pRb = (pRa + 0.5 >= 1.0) ? (pRa - 0.5) : (pRa + 0.5);
        const float wRa = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pRa)));
        const float wRb = 1.0f - wRa;
        const float dRa = static_cast<float>(1.0 - pRa) * grainSamples + 12.0f;
        const float dRb = static_cast<float>(1.0 - pRb) * grainSamples + 12.0f;
        const float octR = wRa * readFloatBufHermite(shimBufR, shimWritePos, dRa)
                         + wRb * readFloatBufHermite(shimBufR, shimWritePos, dRb);

        const float silkCoeff = static_cast<float>(1.0 - std::exp(-kTwoPi * 2350.0 / sr));
        shimLpL += silkCoeff * (octL - shimLpL);
        shimLpR += silkCoeff * (octR - shimLpR);

        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        ioL = warmDryL;
        ioR = warmDryR;
        outExtraRevL = shimLpL * 0.78f;
        outExtraRevR = shimLpR * 0.78f;
    } else if (mode == 8 && shimMaxLen > 64) {
        // 21: Authentic 1965 Leslie 147 Rotary Speaker:
        // Dynamic 6550 Tube Amp + 800 Hz Crossover + Dual-Motor Chorale<->Tremolo Inertia + 3D Horn Throat Beaming & Counter-Bell
        const float inPk = std::max(std::abs(ioL), std::abs(ioR));
        const float envAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > cassetteSagEnv) ? 0.018 : 0.280)));
        cassetteSagEnv += envAlpha * (inPk - cassetteSagEnv);

        const float tubeDrv = 2.05f / (1.0f + 0.45f * cassetteSagEnv);
        const float drvL = juce::jlimit(-2.8f, 2.8f, ioL * tubeDrv);
        const float drvR = juce::jlimit(-2.8f, 2.8f, ioR * tubeDrv);
        const float rawTubeL = (fastTanh(drvL + 0.24f * drvL * drvL) - 0.042f * drvL * drvL) * 0.72f;
        const float rawTubeR = (fastTanh(drvR + 0.24f * drvR * drvR) - 0.042f * drvR * drvR) * 0.72f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (rawTubeL - leslieDcL);
        leslieDcR += dcAlpha * (rawTubeR - leslieDcR);
        const float tubeL = charFilter2.processL(charFilter1.processL(rawTubeL - leslieDcL));
        const float tubeR = charFilter2.processR(charFilter1.processR(rawTubeR - leslieDcR));

        // Phase-coherent 2-stage 800 Hz dividing network (Jensen C15L lower drum vs Jensen V21 upper phenolic horn)
        const float xoverAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 800.0 / sr));
        xoverLp1L += xoverAlpha * (tubeL - xoverLp1L);
        xoverLp2L += xoverAlpha * (xoverLp1L - xoverLp2L);
        xoverLp1R += xoverAlpha * (tubeR - xoverLp1R);
        xoverLp2R += xoverAlpha * (xoverLp1R - xoverLp2R);
        const float drumMono = 0.5f * (xoverLp2L + xoverLp2R);
        const float hornMono = 0.5f * ((tubeL - xoverLp2L) + (tubeR - xoverLp2R));

        shimBufL[static_cast<size_t>(shimWritePos)] = hornMono;
        shimBufR[static_cast<size_t>(shimWritePos)] = drumMono;

        // Dual-Motor Relay & Independent Belt Inertia:
        // Pressing Pedals/ModWheel/Aftertouch, fast bar slides, or hard Gospel accents kicks relay into Fast Tremolo
        // (phenolic horn whips up in 0.78s / coasts down in 1.35s; heavy wooden bass drum lags at 2.45s / 2.95s!)
        const bool fastRelay = (pedalActivity > 0.18f) || (cassetteSagEnv > 0.22f);
        const double fastHornHz = tempoSync
            ? juce::jlimit(5.4, 6.9, (juce::jlimit(60.0, 200.0, hostBpm) / 60.0) * 4.0)
            : 6.35;
        const double targetHorn = fastRelay ? fastHornHz : 0.78;
        const double targetDrum = fastRelay ? (fastHornHz * 0.855) : 0.66;
        const double hBallistic = 1.0 - std::exp(-dt / ((targetHorn > hornRateHz) ? 0.78 : 1.35));
        const double dBallistic = 1.0 - std::exp(-dt / ((targetDrum > drumRateHz) ? 2.45 : 2.95));
        hornRateHz += hBallistic * (targetHorn - hornRateHz);
        drumRateHz += dBallistic * (targetDrum - drumRateHz);

        lfoPhase1 += hornRateHz * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0; // Clockwise upper phenolic horn
        lfoPhase2 -= drumRateHz * dt; if (lfoPhase2 < 0.0)  lfoPhase2 += 1.0; // Counter-clockwise lower wooden drum!

        const double thL = kTwoPi * lfoPhase1;
        const double thR = thL + 2.094; // 120-degree Abbey Road stereo louver mic pair
        const double psL = kTwoPi * lfoPhase2;
        const double psR = psL + 2.094;

        // Louver-mic'd cabinet Doppler excursion (±0.21 ms horn = ±14 cents silky 3D swirl without seasick pitch warble!)
        const float sinHL = static_cast<float>(std::sin(thL));
        const float sinHR = static_cast<float>(std::sin(thR));
        const float dHornL  = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (2.15f + 0.21f * sinHL) * srMs);
        const float dHornR  = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (2.15f + 0.21f * sinHR) * srMs);
        const float dCountL = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (3.05f - 0.18f * sinHL) * srMs);
        const float dCountR = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (3.05f - 0.18f * sinHR) * srMs);
        const float dDrumL  = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (2.75f + 0.12f * static_cast<float>(std::sin(psL))) * srMs);
        const float dDrumR  = juce::jlimit(2.0f, static_cast<float>(shimMaxLen - 8), (2.75f + 0.12f * static_cast<float>(std::sin(psR))) * srMs);

        const float dirHL = 0.5f + 0.5f * static_cast<float>(std::cos(thL));
        const float dirHR = 0.5f + 0.5f * static_cast<float>(std::cos(thR));
        const float dirDL = 0.5f + 0.5f * static_cast<float>(std::cos(psL));
        const float dirDR = 0.5f + 0.5f * static_cast<float>(std::cos(psR));

        const float amHornL = 0.44f + 0.56f * dirHL * (0.52f + 0.48f * dirHL);
        const float amHornR = 0.44f + 0.56f * dirHR * (0.52f + 0.48f * dirHR);
        const float amDrumL = 0.66f + 0.34f * dirDL;
        const float amDrumR = 0.66f + 0.34f * dirDR;

        // Open horn bell + plugged counterweight bell rear louver reflection
        const float hRawL = readFloatBufHermite(shimBufL, shimWritePos, dHornL) * amHornL
                          + readFloatBufHermite(shimBufL, shimWritePos, dCountL) * (0.22f * (1.0f - 0.65f * dirHL));
        const float hRawR = readFloatBufHermite(shimBufL, shimWritePos, dHornR) * amHornR
                          + readFloatBufHermite(shimBufL, shimWritePos, dCountR) * (0.22f * (1.0f - 0.65f * dirHR));
        const float bRawL = readFloatBufHermite(shimBufR, shimWritePos, dDrumL) * amDrumL;
        const float bRawR = readFloatBufHermite(shimBufR, shimWritePos, dDrumR) * amDrumR;

        // Angle-dependent directional acoustic beaming (4.75 kHz on-axis -> 1.05 kHz rear-shadowed louver diffraction!)
        const float hBeamAlphaL = static_cast<float>(1.0 - std::exp(-kTwoPi * (1050.0 + 3700.0 * dirHL) / sr));
        const float hBeamAlphaR = static_cast<float>(1.0 - std::exp(-kTwoPi * (1050.0 + 3700.0 * dirHR) / sr));
        const float dBeamAlphaL = static_cast<float>(1.0 - std::exp(-kTwoPi * (350.0 + 620.0 * dirDL) / sr));
        const float dBeamAlphaR = static_cast<float>(1.0 - std::exp(-kTwoPi * (350.0 + 620.0 * dirDR) / sr));
        hornBeamLpL += hBeamAlphaL * (hRawL - hornBeamLpL);
        hornBeamLpR += hBeamAlphaR * (hRawR - hornBeamLpR);
        drumBeamLpL += dBeamAlphaL * (bRawL - drumBeamLpL);
        drumBeamLpR += dBeamAlphaR * (bRawR - drumBeamLpR);

        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        float rotL = charFilter3.processL(hornBeamLpL * 1.28f + drumBeamLpL * 1.12f);
        float rotR = charFilter3.processR(hornBeamLpR * 1.28f + drumBeamLpR * 1.12f);

        // Wooden Leslie cabinet internal louver reflection
        {
            auto& bL = springApL[0];
            auto& bR = springApR[0];
            int iL = springApPosL[0], iR = springApPosR[0];
            const float dL = bL[static_cast<size_t>(iL)], dR = bR[static_cast<size_t>(iR)];
            const float g = 0.46f;
            const float wL = rotL + dL * g, wR = rotR + dR * g;
            bL[static_cast<size_t>(iL)] = wL;
            bR[static_cast<size_t>(iR)] = wR;
            const float apL = dL - wL * g, apR = dR - wR * g;
            if (++iL >= static_cast<int>(bL.size())) iL = 0;
            if (++iR >= static_cast<int>(bR.size())) iR = 0;
            springApPosL[0] = iL;
            springApPosR[0] = iR;
            ioL = (rotL * 0.85f + apR * 0.20f) * 1.12f;
            ioR = (rotR * 0.85f + apL * 0.20f) * 1.12f;
        }
        outExtraRevL = ioL * 0.28f;
        outExtraRevR = ioR * 0.28f;
    } else if (mode == 9 && shimMaxLen > 64) {
        // 22: Gated Diode Bloom (Rainger FX Reverb-X / Bon Iver: Upfront Dry Steel Ducks Diode-Crunched Room Reverb + Soft Gate)
        const float dryPk = std::max(std::abs(ioL), std::abs(ioR));
        const float trigPk = std::max(0.0f, dryPk - 0.032f);
        const float duckAlpha = std::min(0.5f, ((trigPk > cassetteSagEnv) ? 0.0120f : 0.00042f) * srScale);
        cassetteSagEnv += duckAlpha * (trigPk - cassetteSagEnv);

        // Intimate console FET dry voice
        const float fetL = charFilter3.processL(charFilter1.processL(fastTanh(juce::jlimit(-2.6f, 2.6f, ioL * 1.12f)) * 0.86f));
        const float fetR = charFilter3.processR(charFilter1.processR(fastTanh(juce::jlimit(-2.6f, 2.6f, ioR * 1.12f)) * 0.86f));

        // Dense live-room diffuser + wide cross-coupled multi-tap studio room tank (67..197 ms, ~1.1s hold into soft gate)
        float rmL = dimHpFilter.processL(fetL * 0.82f);
        float rmR = dimHpFilter.processR(fetR * 0.82f);
        for (size_t i = 0; i < 4; ++i) {
            auto& bL = springApL[i];
            auto& bR = springApR[i];
            int idxL = springApPosL[i], idxR = springApPosR[i];
            const float dL = bL[static_cast<size_t>(idxL)], dR = bR[static_cast<size_t>(idxR)];
            const float g = 0.62f;
            const float wL = rmL + dL * g, wR = rmR + dR * g;
            bL[static_cast<size_t>(idxL)] = wL;
            bR[static_cast<size_t>(idxR)] = wR;
            rmL = dL - wL * g;
            rmR = dR - wR * g;
            if (++idxL >= static_cast<int>(bL.size())) idxL = 0;
            if (++idxR >= static_cast<int>(bR.size())) idxR = 0;
            springApPosL[i] = idxL;
            springApPosR[i] = idxR;
        }

        const float t1L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), 67.0f * srMs));
        const float t1R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), 89.0f * srMs));
        const float t2L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), 151.0f * srMs));
        const float t2R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), 197.0f * srMs));

        const float fbRawL = t2L * 0.54f + t1R * 0.34f;
        const float fbRawR = t2R * 0.54f + t1L * 0.34f;
        const float dampAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 1950.0 / sr));
        shimLpL += dampAlpha * (fbRawL - shimLpL);
        shimLpR += dampAlpha * (fbRawR - shimLpR);

        const float tankEnergy = std::max({ dryPk, std::abs(shimLpL), std::abs(shimLpR) });
        const float gateOpen = (tankEnergy > 0.0065f) ? 1.0f : 0.0f;
        const float gateAlpha = std::min(0.5f, ((gateOpen > gateEnv) ? 0.022f : 0.00018f) * srScale);
        gateEnv += gateAlpha * (gateOpen - gateEnv);

        shimBufL[static_cast<size_t>(shimWritePos)] = fastTanh(rmL * 0.64f + shimLpR * gateEnv);
        shimBufR[static_cast<size_t>(shimWritePos)] = fastTanh(rmR * 0.64f + shimLpL * gateEnv);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        const float roomOutL = rmL * 0.35f + t1L * 0.62f + t2R * 0.72f;
        const float roomOutR = rmR * 0.35f + t1R * 0.62f + t2L * 0.72f;

        // Warm asymmetric silicon-diode console saturation on the room tail (filtered at 2.18 kHz + 22 Hz coupling cap, zero fizz!)
        const float dInL = juce::jlimit(-4.0f, 4.0f, roomOutL * 4.65f);
        const float dInR = juce::jlimit(-4.0f, 4.0f, roomOutR * 4.65f);
        float diodeL = (fastTanh(dInL + 0.42f * dInL * dInL) - 0.075f * dInL * dInL) * 0.72f;
        float diodeR = (fastTanh(dInR + 0.42f * dInR * dInR) - 0.075f * dInR * dInR) * 0.72f;
        const float dcCapAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        phaserStateL[0] += dcCapAlpha * (diodeL - phaserStateL[0]);
        phaserStateR[0] += dcCapAlpha * (diodeR - phaserStateR[0]);
        diodeL = charFilter2.processL(subLpFilter.processL(diodeL - phaserStateL[0]));
        diodeR = charFilter2.processR(subLpFilter.processR(diodeR - phaserStateR[0]));

        // Dry transient ducks the diode room (-16 dB); as note decays/lifts, console-blown room swells up like a reverse tidal wave!
        const float duckGain = (1.0f / (1.0f + 18.5f * cassetteSagEnv)) * gateEnv;
        const float bloomL = diodeL * duckGain * 1.48f;
        const float bloomR = diodeR * duckGain * 1.48f;

        ioL = fetL + bloomL;
        ioR = fetR + bloomR;
        outExtraRevL = bloomL * 0.42f;
        outExtraRevR = bloomR * 0.42f;
    } else if (mode == 10) {
        // 23: Standel 807 Chamber (1956 Standel 25L15 807 Beam-Tetrode + 15" JBL D130F + Capitol Studios Subterranean Tile Chamber)
        lfoPhase1 += 0.28 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float optoAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.008 : 0.220)));
        cassetteSagEnv += optoAlpha * (pk - cassetteSagEnv);
        const float standelGain = 1.32f / (1.0f + 0.48f * cassetteSagEnv);

        const float xL = juce::jlimit(-2.6f, 2.6f, ioL * standelGain);
        const float xR = juce::jlimit(-2.6f, 2.6f, ioR * standelGain);
        const float rawL = (fastTanh(xL + 0.16f * xL * xL) - 0.028f * xL * xL) * 0.88f;
        const float rawR = (fastTanh(xR + 0.16f * xR * xR) - 0.028f * xR * xR) * 0.88f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (rawL - leslieDcL);
        leslieDcR += dcAlpha * (rawR - leslieDcR);

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(rawL - leslieDcL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(rawR - leslieDcR)));

        // Capitol Studios Hollywood subterranean tile echo-chamber figure-8 ribbon pre-reflections (0% dry comb-filtering!)
        const float s1 = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float c1 = static_cast<float>(std::cos(kTwoPi * lfoPhase1));
        const float dChambL = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (33.0f + 1.15f * s1) * srMs);
        const float dChambR = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (49.0f + 1.15f * c1) * srMs);
        outExtraRevL = subLpFilter.processL(dimHpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dChambL))) * 0.52f;
        outExtraRevR = subLpFilter.processR(dimHpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dChambR))) * 0.52f;
    } else if (mode == 11) {
        // 25: Bosstone Violin Fuzz (240 Hz Split: Clean Tube Bass Foundation + Jordan Bosstone BJT/Diode Violin Sustainer -> 15" JBL)
        const float inPk = std::max(std::abs(ioL), std::abs(ioR));
        const float envAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > cassetteSagEnv) ? 0.003 : 0.220)));
        cassetteSagEnv += envAlpha * (inPk - cassetteSagEnv);
        const float gateTarget = (inPk > 0.0008f) ? 1.0f : 0.0f;
        const float gateAlpha = static_cast<float>(1.0 - std::exp(-dt / ((gateTarget > gateEnv) ? 0.0025 : 0.160)));
        gateEnv += gateAlpha * (gateTarget - gateEnv);

        // Warm intermodulation-free tube bass foundation (< 250 Hz)
        const float bassL = fastTanh(charFilter1.processL(ioL) * 1.65f) * 0.42f;
        const float bassR = fastTanh(charFilter1.processR(ioR) * 1.65f) * 0.42f;

        // Pre-tightened (> 240 Hz) singing BJT/diode violin sustainer
        const float sustGain = juce::jlimit(3.6f, 9.8f, 2.65f / (0.20f + cassetteSagEnv)) * gateEnv;
        const float hpL = juce::jlimit(-3.6f, 3.6f, dimHpFilter.processL(ioL) * sustGain);
        const float hpR = juce::jlimit(-3.6f, 3.6f, dimHpFilter.processR(ioR) * sustGain);

        // Stage 1: Complementary BJT collector push (singing 2nd-harmonic violin bloom) + 28 Hz coupling cap
        const float st1L = fastTanh(hpL + 0.22f * hpL * hpL) - 0.11f * fastTanh(hpL * hpL);
        const float st1R = fastTanh(hpR + 0.22f * hpR * hpR) - 0.11f * fastTanh(hpR * hpR);
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 28.0 / sr));
        leslieDcL += dcAlpha * (st1L - leslieDcL);
        leslieDcR += dcAlpha * (st1R - leslieDcR);

        // Stage 2: Silicon/germanium clipping diodes + post-clipper coupling cap -> 1120 Hz violin throat & 15" JBL cone
        const float rawFzL = fastTanh((st1L - leslieDcL) * 2.65f) * 0.155f;
        const float rawFzR = fastTanh((st1R - leslieDcR) * 2.65f) * 0.155f;
        phaserStateL[0] += dcAlpha * (rawFzL - phaserStateL[0]);
        phaserStateR[0] += dcAlpha * (rawFzR - phaserStateR[0]);
        const float fzL = charFilter3.processL(subLpFilter.processL(charFilter2.processL(rawFzL - phaserStateL[0])));
        const float fzR = charFilter3.processR(subLpFilter.processR(charFilter2.processR(rawFzR - phaserStateR[0])));

        ioL = bassL + fzL;
        ioR = bassR + fzR;
        outExtraRevL = fzL * 0.34f;
        outExtraRevR = fzR * 0.34f;
    } else if (mode == 12) {
        // 26: Talk-Box Vowel Filter (1964 Pete Drake Horn Driver Mouth-Tube + Mu-Tron III Dual Vactrol Vocal Tract)
        const float inPk = std::max(std::abs(ioL), std::abs(ioR));
        const float vacAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > cassetteSagEnv) ? 0.011 : 0.145)));
        cassetteSagEnv += vacAlpha * (inPk - cassetteSagEnv);

        lfoPhase1 += 0.55 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        const float breath = 0.04f * static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float envRaw = juce::jlimit(0.0f, 1.0f, (cassetteSagEnv - 0.010f) * 9.2f);
        const float envNorm = envRaw * (2.0f - envRaw);
        const float pedNorm = juce::jlimit(0.0f, 1.0f, pedalActivity * 0.45f);
        const float vowel = juce::jlimit(0.0f, 1.0f, 0.82f * envNorm + 0.32f * pedNorm + breath);

        // Human vocal-tract formant trajectory: /u/ (290, 700) -> /o/ (510, 1150) -> /a/ (740, 1720) -> /ae/-/i/ (810, 2390 Hz)
        const double f1Hz = 290.0 + 520.0 * static_cast<double>(vowel);
        const double f2Hz = 700.0 * std::pow(3.42, static_cast<double>(vowel));

        // University Sound Phenolic Horn Compression Driver saturation (generates rich upper harmonics to feed F2 & F3!)
        const float drv = 3.15f / (0.65f + 1.15f * cassetteSagEnv);
        const float xL = juce::jlimit(-3.2f, 3.2f, dimHpFilter.processL(ioL) * drv);
        const float xR = juce::jlimit(-3.2f, 3.2f, dimHpFilter.processR(ioR) * drv);
        const float rawL = (fastTanh(xL + 0.22f * xL * xL) - 0.11f * fastTanh(xL * xL)) * 0.62f;
        const float rawR = (fastTanh(xR + 0.22f * xR * xR) - 0.11f * fastTanh(xR * xR)) * 0.62f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 26.0 / sr));
        leslieDcL += dcAlpha * (rawL - leslieDcL);
        leslieDcR += dcAlpha * (rawR - leslieDcR);
        const float excL = rawL - leslieDcL;
        const float excR = rawR - leslieDcR;

        // Sample-rate-invariant Trapezoidal ZDF Dual-Formant Bandpass SVFs (F1 Q=5.6, F2 Q=6.6)
        const float g1 = static_cast<float>(std::tan(kPi * juce::jlimit(180.0, sr * 0.40, f1Hz) / sr));
        const float k1 = 1.0f / 5.6f;
        const float a1_1 = 1.0f / (1.0f + g1 * (g1 + k1));
        const float a2_1 = g1 * a1_1;
        const float a3_1 = g1 * a2_1;

        const float g2 = static_cast<float>(std::tan(kPi * juce::jlimit(400.0, sr * 0.42, f2Hz) / sr));
        const float k2 = 1.0f / 6.6f;
        const float a1_2 = 1.0f / (1.0f + g2 * (g2 + k2));
        const float a2_2 = g2 * a1_2;
        const float a3_2 = g2 * a2_2;

        const float v3_1L = excL - svf1Ic2L;
        const float v1_1L = a1_1 * svf1Ic1L + a2_1 * v3_1L;
        const float v2_1L = svf1Ic2L + a2_1 * svf1Ic1L + a3_1 * v3_1L;
        svf1Ic1L = 2.0f * v1_1L - svf1Ic1L;
        svf1Ic2L = 2.0f * v2_1L - svf1Ic2L;

        const float v3_1R = excR - svf1Ic2R;
        const float v1_1R = a1_1 * svf1Ic1R + a2_1 * v3_1R;
        const float v2_1R = svf1Ic2R + a2_1 * svf1Ic1R + a3_1 * v3_1R;
        svf1Ic1R = 2.0f * v1_1R - svf1Ic1R;
        svf1Ic2R = 2.0f * v2_1R - svf1Ic2R;

        const float v3_2L = excL - svf2Ic2L;
        const float v1_2L = a1_2 * svf2Ic1L + a2_2 * v3_2L;
        const float v2_2L = svf2Ic2L + a2_2 * svf2Ic1L + a3_2 * v3_2L;
        svf2Ic1L = 2.0f * v1_2L - svf2Ic1L;
        svf2Ic2L = 2.0f * v2_2L - svf2Ic2L;

        const float v3_2R = excR - svf2Ic2R;
        const float v1_2R = a1_2 * svf2Ic1R + a2_2 * v3_2R;
        const float v2_2R = svf2Ic2R + a2_2 * svf2Ic1R + a3_2 * v3_2R;
        svf2Ic1R = 2.0f * v1_2R - svf2Ic1R;
        svf2Ic2R = 2.0f * v2_2R - svf2Ic2R;

        // 90% resonant human oral-cavity formants (F1 + F2 + 2.55 kHz F3 singer's formant) + 10% warm chest foundation!
        const float formL = (v1_1L * k1 * 2.15f) + (v1_2L * k2 * 2.65f);
        const float formR = (v1_1R * k1 * 2.15f) + (v1_2R * k2 * 2.65f);
        const float chestL = charFilter1.processL(ioL) * 0.22f;
        const float chestR = charFilter1.processR(ioR) * 0.22f;

        ioL = charFilter3.processL(charFilter2.processL(formL)) + chestL;
        ioR = charFilter3.processR(charFilter2.processR(formR)) + chestR;
        outExtraRevL = ioL * 0.26f;
        outExtraRevR = ioR * 0.26f;
    } else if (mode == 13) {
        // 27: 1937 Rickenbacker B6 Bakelite Lap Steel (1.5" Horseshoe Magnet + Gibson EH-150 Octal 6L6 + RCA 44 Waikiki Studio)
        lfoPhase1 += 0.34 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float sagAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.007 : 0.210)));
        cassetteSagEnv += sagAlpha * (pk - cassetteSagEnv);
        const float octalDrv = 1.62f / (1.0f + 0.52f * cassetteSagEnv);

        const float hpL = juce::jlimit(-2.8f, 2.8f, dimHpFilter.processL(ioL) * octalDrv);
        const float hpR = juce::jlimit(-2.8f, 2.8f, dimHpFilter.processR(ioR) * octalDrv);
        const float rawL = (fastTanh(hpL + 0.20f * hpL * hpL) - 0.10f * fastTanh(hpL * hpL)) * 0.82f;
        const float rawR = (fastTanh(hpR + 0.20f * hpR * hpR) - 0.10f * fastTanh(hpR * hpR)) * 0.82f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (rawL - leslieDcL);
        leslieDcR += dcAlpha * (rawR - leslieDcR);

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(rawL - leslieDcL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(rawR - leslieDcR)));

        // Vintage 1940s Waikiki wood broadcast studio RCA 44-BX figure-8 early reflections
        const float wob = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float dRmL = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (21.5f + 0.45f * wob) * srMs);
        const float dRmR = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (31.0f - 0.45f * wob) * srMs);
        outExtraRevL = subLpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dRmL)) * 0.42f;
        outExtraRevR = subLpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dRmR)) * 0.42f;
    } else if (mode == 14) {
        // 28: Abbey Road Tape ADT (Fairchild 670 / TG12345 Zener Limiter + Ken Townsend 33.5 ms Varispeed Studer J37 ADT)
        lfoPhase1 += 0.37 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.91 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float limAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.0035 : 0.260)));
        cassetteSagEnv += limAlpha * (pk - cassetteSagEnv);

        // Fairchild 670 / TG12345 Zener Limiter squash for vocal sustain
        const float limGain = 1.62f / (0.62f + 1.55f * cassetteSagEnv);
        const float xL = juce::jlimit(-2.8f, 2.8f, ioL * limGain);
        const float xR = juce::jlimit(-2.8f, 2.8f, ioR * limGain);
        const float rawL = (fastTanh(xL + 0.18f * xL * xL) - 0.09f * fastTanh(xL * xL)) * 0.86f;
        const float rawR = (fastTanh(xR + 0.18f * xR * xR) - 0.09f * fastTanh(xR * xR)) * 0.86f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (rawL - leslieDcL);
        leslieDcR += dcAlpha * (rawR - leslieDcR);

        const float dirL = charFilter3.processL(charFilter2.processL(charFilter1.processL(rawL - leslieDcL)));
        const float dirR = charFilter3.processR(charFilter2.processR(charFilter1.processR(rawR - leslieDcR)));

        // Ken Townsend's Varispeed Second Studer J37 Sync-Head Tape Machine (~33.5 ms Haas twin + independent VFO drift)
        const float vfo1 = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float vfo2 = static_cast<float>(std::sin(kTwoPi * lfoPhase2));
        const float adtMsL = 32.5f + 2.65f * vfo1 + 1.05f * vfo2 + 1.40f * cassetteSagEnv;
        const float adtMsR = 34.8f - 2.45f * vfo1 + 1.15f * vfo2 + 1.40f * cassetteSagEnv;
        const float dAdtL = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), adtMsL * srMs);
        const float dAdtR = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), adtMsR * srMs);

        const float adtTapeL = subLpFilter.processL(dimHpFilter.processL(fastTanh(readFloatBufHermite(modBufL, modWritePos, dAdtL) * 1.48f) * 0.84f));
        const float adtTapeR = subLpFilter.processR(dimHpFilter.processR(fastTanh(readFloatBufHermite(modBufR, modWritePos, dAdtR) * 1.48f) * 0.84f));

        // Wide Abbey Road stereo twin-tracking: Direct J37 Left-Center + Varispeed ADT J37 Right-Center!
        ioL = dirL * 0.84f + adtTapeL * 0.22f + adtTapeR * 0.16f;
        ioR = dirR * 0.28f + adtTapeR * 0.78f + adtTapeL * 0.18f;
        outExtraRevL = (dirL + adtTapeL) * 0.22f;
        outExtraRevR = (dirR + adtTapeR) * 0.22f;
    } else if (mode == 15 && shimMaxLen > 64) {
        // 29: 1985 Bradshaw LA Multi-Rack:
        // Unit 1: DBX 160X Slow-Attack Spanky Bell VCA + Aphex Aural Exciter Type-C (>3.3 kHz Harmonic Air)
        // Unit 2: DyTronics CS-5 Symmetrical 120-Deg 3-Phase Tri-Stereo Chorus (sin(0)+sin(120)+sin(240) == 0 -> 0.00-cent pitch lock!)
        // Unit 3: Lexicon PCM70 Circular Pan-Delays (1/8 Left + Dotted-1/8 Right modulated crystal repeats)
        lfoPhase1 += 0.58 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0; // Preset A lush wave
        lfoPhase2 += 1.62 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0; // Preset B liquid shimmer

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float fastAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > gateEnv) ? 0.0012 : 0.032)));
        const float slowAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.026 : 0.155)));
        gateEnv        += fastAlpha * (pk - gateEnv);
        cassetteSagEnv += slowAlpha * (pk - cassetteSagEnv);

        // DBX 160X slow-attack glassy transient pop + plush sustain leveling
        const float spankSnap = juce::jlimit(0.0f, 0.55f, (gateEnv - cassetteSagEnv) * 2.15f);
        const float vcaGain   = (1.16f + 0.72f * spankSnap) / (1.0f + 0.68f * cassetteSagEnv);

        const float vcaL = fastTanh(ioL * vcaGain) * 0.90f;
        const float vcaR = fastTanh(ioR * vcaGain) * 0.90f;

        // Aphex Type-C high-band harmonic sheen (>3.3 kHz zero-mean even-harmonic diamond air, DC-blocked)
        const float exHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 3300.0 / sr));
        xoverLp1L += exHpAlpha * (vcaL - xoverLp1L);
        xoverLp1R += exHpAlpha * (vcaR - xoverLp1R);
        const float hiL = juce::jlimit(-1.5f, 1.5f, (vcaL - xoverLp1L) * 1.85f);
        const float hiR = juce::jlimit(-1.5f, 1.5f, (vcaR - xoverLp1R) * 1.85f);
        const float rawExL = fastTanh(hiL + 0.34f * hiL * hiL) - 0.17f * fastTanh(hiL * hiL);
        const float rawExR = fastTanh(hiR + 0.34f * hiR * hiR) - 0.17f * fastTanh(hiR * hiR);
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 28.0 / sr));
        leslieDcL += dcAlpha * (rawExL - leslieDcL);
        leslieDcR += dcAlpha * (rawExR - leslieDcR);

        const float dryCleanL = charFilter3.processL(charFilter2.processL(charFilter1.processL(vcaL + 0.24f * (rawExL - leslieDcL))));
        const float dryCleanR = charFilter3.processR(charFilter2.processR(charFilter1.processR(vcaR + 0.24f * (rawExR - leslieDcR))));

        // Write pre-EQ+Exciter signal into modBuf so the 3 Tri-Chorus BBDs shimmer with full 5.4 kHz bell glass!
        modBufL[static_cast<size_t>(modWritePos)] = dryCleanL;
        modBufR[static_cast<size_t>(modWritePos)] = dryCleanR;

        // Symmetrical 120-degree 3-Phase Tri-Stereo Chorus (exact zero-sum instantaneous pitch centroid!)
        const double th = kTwoPi * lfoPhase1;
        const double ps = kTwoPi * lfoPhase2;
        const float w1 = 1.62f * static_cast<float>(std::sin(th))            + 0.32f * static_cast<float>(std::sin(ps));
        const float w2 = 1.62f * static_cast<float>(std::sin(th + 2.094395)) + 0.32f * static_cast<float>(std::sin(ps + 2.094395));
        const float w3 = 1.62f * static_cast<float>(std::sin(th + 4.188790)) + 0.32f * static_cast<float>(std::sin(ps + 4.188790));

        const float dTri1 = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (6.50f  + w1) * srMs);
        const float dTri2 = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (9.45f  + w2) * srMs);
        const float dTri3 = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (12.40f + w3) * srMs);

        const float tap1L = readFloatBufHermite(modBufL, modWritePos, dTri1);
        const float tap2R = readFloatBufHermite(modBufR, modWritePos, dTri2);
        const float tap3C = 0.5f * (readFloatBufHermite(modBufL, modWritePos, dTri3)
                                  + readFloatBufHermite(modBufR, modWritePos, dTri3));

        const float triL = dimHpFilter.processL(subLpFilter.processL(tap1L + 0.64f * tap3C - 0.34f * tap2R));
        const float triR = dimHpFilter.processR(subLpFilter.processR(tap2R + 0.64f * tap3C - 0.34f * tap1L));

        // Lexicon PCM70 Dynamically Ducked Circular Pan-Delays (1/8 Left + Dotted-1/8 Right — clean on chords, blooms in pauses!)
        const double safeBpm = tempoSync ? juce::jlimit(68.0, 175.0, hostBpm) : 96.0;
        const float eighthMs = static_cast<float>(30000.0 / safeBpm);
        const float pcmMsL   = juce::jlimit(140.0f, 345.0f, eighthMs) + 0.32f * w2;
        const float pcmMsR   = juce::jlimit(210.0f, 515.0f, eighthMs * 1.5f) + 0.32f * w3;
        const float dPcmL    = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), pcmMsL * srMs);
        const float dPcmR    = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), pcmMsR * srMs);

        const float rawPcmL = readFloatBufHermite(shimBufL, shimWritePos, dPcmL);
        const float rawPcmR = readFloatBufHermite(shimBufR, shimWritePos, dPcmR);

        const float pcmLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 3950.0 / sr));
        const float pcmHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 240.0 / sr));
        shimLpL += pcmLpAlpha * (rawPcmL - shimLpL);
        shimLpR += pcmLpAlpha * (rawPcmR - shimLpR);
        hornBeamLpL += pcmHpAlpha * (shimLpL - hornBeamLpL);
        hornBeamLpR += pcmHpAlpha * (shimLpR - hornBeamLpR);
        const float pcmTapL = shimLpL - hornBeamLpL;
        const float pcmTapR = shimLpR - hornBeamLpR;

        // True 85%-wet DyTronics CS-5 Tri-Stereo Chorus balance + PCM70 dynamic ducking!
        const float triMixL  = dryCleanL * 0.34f + triL * 0.88f;
        const float triMixR  = dryCleanR * 0.34f + triR * 0.88f;
        const float duckGain = 1.0f / (1.0f + 4.8f * gateEnv);
        const float pcmGain  = 0.11f + 0.32f * duckGain;

        shimBufL[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.5f, 2.5f, triMixL * 0.85f + pcmTapR * 0.27f);
        shimBufR[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.5f, 2.5f, triMixR * 0.85f + pcmTapL * 0.27f);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        ioL = triMixL + pcmTapL * pcmGain;
        ioR = triMixR + pcmTapR * pcmGain;
        outExtraRevL = (triMixL + pcmTapL * 0.70f) * 0.28f;
        outExtraRevR = (triMixR + pcmTapR * 0.70f) * 0.28f;
    } else if (mode == 16) {
        // 30: Poly-Octave Organ (Messina Harmonium Bellows + Voix-Celeste ±1.0c Stereo Air + Cathedral Reflections)
        // Note: 0.00-cent phase-locked 16' Sub + 8' Steel + silk-filtered 4' Octave ranks are generated per-string in renderBlock()!
        lfoPhase1 += 0.27 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.15 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float celSin = static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float celCos = static_cast<float>(std::cos(kTwoPi * lfoPhase1));
        const float dCelL = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (5.6f + 0.32f * celSin) * srMs);
        const float dCelR = juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (6.5f + 0.32f * celCos) * srMs);
        const float celL = dimHpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dCelL));
        const float celR = dimHpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dCelR));

        const float mixL = juce::jlimit(-2.8f, 2.8f, ioL * 0.88f + celL * 0.26f - celR * 0.12f);
        const float mixR = juce::jlimit(-2.8f, 2.8f, ioR * 0.88f + celR * 0.26f - celL * 0.12f);
        const float reedL = (fastTanh(mixL * 1.10f + 0.08f * mixL * mixL) - 0.04f * fastTanh(mixL * mixL)) * 0.92f;
        const float reedR = (fastTanh(mixR * 1.10f + 0.08f * mixR * mixR) - 0.04f * fastTanh(mixR * mixR)) * 0.92f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (reedL - leslieDcL);
        leslieDcR += dcAlpha * (reedR - leslieDcR);

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(reedL - leslieDcL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(reedR - leslieDcR)));

        const float catWob = static_cast<float>(std::sin(kTwoPi * lfoPhase2));
        const float dCatL = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (29.0f + 0.75f * catWob) * srMs);
        const float dCatR = juce::jlimit(4.0f, static_cast<float>(modMaxLen - 4), (43.0f - 0.75f * catWob) * srMs);
        outExtraRevL = subLpFilter.processL(readFloatBufHermite(modBufL, modWritePos, dCatL)) * 0.38f + ioL * 0.20f;
        outExtraRevR = subLpFilter.processR(readFloatBufHermite(modBufR, modWritePos, dCatR)) * 0.38f + ioR * 0.20f;
    } else if (mode == 17 && shimMaxLen > 64) {
        // 31: Two Star VG-8 Bowed Steel (100% Real-String Processed Guitar Rig:
        //     VG-8 D62 BowedPad -> Feedback Boss CE-2 / Env-Flanger Jet -> Ducked Rainger Reverb-X -> Post-Reverb Tascam 424 Sag & Tape Glue)
        lfoPhase1 += 0.92 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.34 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;
        lfoPhase3 += 2.18 * dt; if (lfoPhase3 >= 1.0) lfoPhase3 -= 1.0;

        const float inPk = std::max(std::abs(ioL), std::abs(ioR));
        const float fastAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > gateEnv) ? 0.0022 : 0.095)));
        const float slowAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > cassetteSagEnv) ? 0.0420 : 0.300)));
        gateEnv        += fastAlpha * (inPk - gateEnv);
        cassetteSagEnv += slowAlpha * (inPk - cassetteSagEnv);

        // 1. VG-8 / Baritone console body voicing + BBD Chorus/Flanger Regeneration (phaserFbL/R)
        const float preL = charFilter3.processL(charFilter2.processL(charFilter1.processL(ioL)));
        const float preR = charFilter3.processR(charFilter2.processR(charFilter1.processR(ioR)));
        modBufL[static_cast<size_t>(modWritePos)] = juce::jlimit(-2.6f, 2.6f, preL + phaserFbL * 0.30f);
        modBufR[static_cast<size_t>(modWritePos)] = juce::jlimit(-2.6f, 2.6f, preR + phaserFbR * 0.30f);

        // 2. Lush Feedback Boss CE-2 MN3007 Stereo Chorus + Envelope-Coupled Flanger Jet Sweep:
        //    0.92 Hz + 2.18 Hz + 0.34 Hz multi-rate BBD with 30% regeneration creates authentic liquid comb undulation
        //    even in mono (ChorAM ~ 200-270 matching A1/C1!) and a chewy flanger jet whoosh on hard C1 strums!
        const double th1 = kTwoPi * lfoPhase1;
        const double th2 = kTwoPi * lfoPhase2;
        const double th3 = kTwoPi * lfoPhase3;
        const float jetPullMs = 3.10f * juce::jlimit(0.0f, 1.0f, (gateEnv - 0.15f) * 2.65f);
        const float w1 = 2.15f * static_cast<float>(std::sin(th1))            + 0.55f * static_cast<float>(std::sin(th2))            + 0.28f * static_cast<float>(std::sin(th3));
        const float w2 = 2.15f * static_cast<float>(std::sin(th1 + 2.094395)) + 0.55f * static_cast<float>(std::sin(th2 + 1.570796)) - 0.28f * static_cast<float>(std::cos(th3));
        const float w3 = 1.85f * static_cast<float>(std::sin(th1 + 4.188790)) - 0.48f * static_cast<float>(std::cos(th2))            + 0.22f * static_cast<float>(std::sin(th3 + 1.047));

        const float d1 = juce::jlimit(1.1f, static_cast<float>(modMaxLen - 4), (5.35f - jetPullMs + w1) * srMs);
        const float d2 = juce::jlimit(1.1f, static_cast<float>(modMaxLen - 4), (7.15f - jetPullMs + w2) * srMs);
        const float d3 = juce::jlimit(1.1f, static_cast<float>(modMaxLen - 4), (9.80f - 0.70f * jetPullMs + w3) * srMs);

        const float tap1L = readFloatBufHermite(modBufL, modWritePos, d1);
        const float tap2R = readFloatBufHermite(modBufR, modWritePos, d2);
        const float tap3C = 0.5f * (readFloatBufHermite(modBufL, modWritePos, d3) + readFloatBufHermite(modBufR, modWritePos, d3));

        const float bbdL = subLpFilter.processL(dimHpFilter.processL(0.76f * tap1L + 0.46f * tap3C + 0.22f * tap2R));
        const float bbdR = subLpFilter.processR(dimHpFilter.processR(0.76f * tap2R + 0.46f * tap3C + 0.22f * tap1L));
        phaserFbL = fastTanh(bbdL * 0.92f);
        phaserFbR = fastTanh(bbdR * 0.92f);

        // Subtle organic rotary/chorus undulation (pure 0.88 Hz slow BBD breathe without fast 2.2 Hz flutter on held solo notes!)
        lfoPhase1 -= 0.04 * dt; if (lfoPhase1 < 0.0) lfoPhase1 += 1.0;
        const float modPulse = 1.0f + 0.075f * static_cast<float>(std::sin(th1 + 0.6)) + 0.012f * static_cast<float>(std::sin(th3));
        const float dryChorL = (preL * 0.68f + bbdL * 0.62f) * modPulse;
        const float dryChorR = (preR * 0.68f + bbdR * 0.62f) * modPulse;

        // 3. Gauzy Pre-Delayed Rainger FX Reverb-X / VG-8 Modulated Dark Hall (1.25s tank, ~2.2s decay):
        //    Highpassed at 135 Hz so E1/D1 sub-octaves stay tight; ducks on dense chord blasts so rhythm stays tight,
        //    while wrapping single-note cello/violin solos and chord decays in lush 3D room/hall space!
        const float revHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 135.0 / sr));
        drumBeamLpL += revHpAlpha * (dryChorL - drumBeamLpL);
        drumBeamLpR += revHpAlpha * (dryChorR - drumBeamLpR);
        float diffL = (dryChorL - drumBeamLpL) * 0.54f;
        float diffR = (dryChorR - drumBeamLpR) * 0.54f;
        for (size_t i = 0; i < 4; ++i) {
            auto& bL = springApL[i];
            auto& bR = springApR[i];
            int idxL = springApPosL[i], idxR = springApPosR[i];
            const float dL = bL[static_cast<size_t>(idxL)], dR = bR[static_cast<size_t>(idxR)];
            const float g = 0.62f;
            const float uApL = diffL + dL * g, uApR = diffR + dR * g;
            bL[static_cast<size_t>(idxL)] = uApL;
            bR[static_cast<size_t>(idxR)] = uApR;
            diffL = dL - uApL * g;
            diffR = dR - uApR * g;
            if (++idxL >= static_cast<int>(bL.size())) idxL = 0;
            if (++idxR >= static_cast<int>(bR.size())) idxR = 0;
            springApPosL[i] = idxL;
            springApPosR[i] = idxR;
        }

        const float tankWob1 = 0.95f * static_cast<float>(std::sin(th2));
        const float tankWob2 = 0.95f * static_cast<float>(std::cos(th2));
        const float r1L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (43.0f + tankWob1) * srMs));
        const float r1R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (67.0f - tankWob1) * srMs));
        const float r2L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (137.0f - tankWob2) * srMs));
        const float r2R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (211.0f + tankWob2) * srMs));
        const float revRawL = diffL * 0.36f + r1L * 0.45f + r2R * 0.43f;
        const float revRawR = diffR * 0.36f + r1R * 0.45f + r2L * 0.43f;
        const float dampAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2250.0 / sr));
        shimLpL += dampAlpha * (revRawL - shimLpL);
        shimLpR += dampAlpha * (revRawR - shimLpR);

        shimBufL[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.2f, 2.2f, diffL * 0.64f + shimLpR * 0.57f);
        shimBufR[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.2f, 2.2f, diffR * 0.64f + shimLpL * 0.57f);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        // Rainger FX Reverb-X ducking: stays lush on singing solos/soft chords, ducks on heavy chord blasts, blooms on decay!
        const float gateOpen  = juce::jlimit(0.0f, 1.0f, (cassetteSagEnv - 0.00020f) * 150.0f);
        const float blastOver = std::max(0.0f, gateEnv - 0.18f);
        const float revDuck   = (0.28f + 0.72f / (1.0f + 11.0f * blastOver)) * gateOpen;
        const float busInL    = dryChorL + (0.76f * shimLpL + 0.24f * shimLpR) * 0.40f * revDuck;
        const float busInR    = dryChorR + (0.76f * shimLpR + 0.24f * shimLpL) * 0.40f * revDuck;

        // 4. Hardware VG-8 Output Sustainer-Compressor + Post-Reverb Tascam 424 Saturation & Dynamic Tape/Cabinet Contour:
        //    Tightly levels soft A1 fingerpicking/solos up near hard C1 chord blasts (matching hardware -18.9 vs -16.4 dB!),
        //    so hard strums explode in TIMBRE (4 kHz AD filter whoosh + diode grit) rather than raw volume spikes!
        const float compGain = 1.56f / (0.48f + 2.35f * gateEnv + 0.65f * cassetteSagEnv);
        const float driveBoost = 1.22f + 0.72f * juce::jlimit(0.0f, 1.0f, (gateEnv - 0.17f) * 2.4f);
        const float uL = juce::jlimit(-3.6f, 3.6f, busInL * compGain * driveBoost);
        const float uR = juce::jlimit(-3.6f, 3.6f, busInR * compGain * driveBoost);
        const float satL = (fastTanh(uL + 0.22f * uL * uL) - 0.10f * fastTanh(uL * uL)) / (1.04f + 0.30f * (driveBoost - 1.22f));
        const float satR = (fastTanh(uR + 0.22f * uR * uR) - 0.10f * fastTanh(uR * uR)) / (1.04f + 0.30f * (driveBoost - 1.22f));
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 22.0 / sr));
        leslieDcL += dcAlpha * (satL - leslieDcL);
        leslieDcR += dcAlpha * (satR - leslieDcR);

        const float busGlue = 0.90f / (0.84f + 0.36f * cassetteSagEnv);
        const float cleanSatL = (satL - leslieDcL) * busGlue;
        const float cleanSatR = (satR - leslieDcR) * busGlue;

        const double dynTapeHz = juce::jlimit(4850.0, 11200.0, 4900.0 + 16500.0 * std::max(0.0, static_cast<double>(gateEnv) - 0.11));
        const float tapeLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * dynTapeHz / sr));
        hornBeamLpL += tapeLpAlpha * (cleanSatL - hornBeamLpL);
        hornBeamLpR += tapeLpAlpha * (cleanSatR - hornBeamLpR);

        ioL = charFilter4.processL(hornBeamLpL);
        ioR = charFilter4.processR(hornBeamLpR);
        outExtraRevL = (shimLpL * 0.52f * revDuck + ioL * 0.14f) * 0.62f;
        outExtraRevR = (shimLpR * 0.52f * revDuck + ioR * 0.14f) * 0.62f;
    } else if (mode == 18 && shimMaxLen > 64) {
        // 32: '66 Revolver Backwards Tape (Abbey Road Room 2 Dual -1.000x Backwards-Reading Studer J37 Tape Heads!)
        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float limAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.004 : 0.220)));
        cassetteSagEnv += limAlpha * (pk - cassetteSagEnv);
        const float recGain = 1.24f / (0.65f + 1.15f * cassetteSagEnv);

        const float recInL = juce::jlimit(-2.6f, 2.6f, dimHpFilter.processL(ioL) * recGain);
        const float recInR = juce::jlimit(-2.6f, 2.6f, dimHpFilter.processR(ioR) * recGain);
        const float tapeRecL = fastTanh(recInL + 0.16f * recInL * recInL) - 0.08f * fastTanh(recInL * recInL);
        const float tapeRecR = fastTanh(recInR + 0.16f * recInR * recInR) - 0.08f * fastTanh(recInR * recInR);

        shimBufL[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.2f, 2.2f, tapeRecL + shimLpR * 0.16f);
        shimBufR[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.2f, 2.2f, tapeRecR + shimLpL * 0.16f);

        const double revDurSec = tempoSync
            ? juce::jlimit(0.32, 0.48, (60.0 / juce::jlimit(65.0, 175.0, hostBpm)) * 0.52)
            : 0.40;
        const double grainSamplesD = revDurSec * sr;
        const float grainSamples   = juce::jlimit(256.0f, static_cast<float>(shimMaxLen - 36) * 0.48f, static_cast<float>(grainSamplesD));

        shimPhasor += 1.0 / static_cast<double>(grainSamples);
        if (shimPhasor >= 1.0) shimPhasor -= 1.0;

        // Dual C1 reverse-whip tape heads (dD/dn = +2.0000 => exact -1.000x playback at 0.00-cent concert pitch + zwoooop-SNAP!)
        const double pLa = shimPhasor;
        const double pLb = (pLa + 0.5 >= 1.0) ? (pLa - 0.5) : (pLa + 0.5);
        const double pRa = (pLa + 0.12 >= 1.0) ? (pLa - 0.88) : (pLa + 0.12);
        const double pRb = (pRa + 0.5 >= 1.0) ? (pRa - 0.5) : (pRa + 0.5);

        const float fLa = static_cast<float>(pLa), fLb = static_cast<float>(pLb);
        const float fRa = static_cast<float>(pRa), fRb = static_cast<float>(pRb);
        const float wLa = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pLa))) * (0.76f + 0.52f * fLa * fLa);
        const float wLb = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pLb))) * (0.76f + 0.52f * fLb * fLb);
        const float wRa = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pRa))) * (0.76f + 0.52f * fRa * fRa);
        const float wRb = 0.5f * (1.0f - static_cast<float>(std::cos(kTwoPi * pRb))) * (0.76f + 0.52f * fRb * fRb);

        const float dLa = 12.0f + 2.0f * fLa * grainSamples;
        const float dLb = 12.0f + 2.0f * fLb * grainSamples;
        const float dRa = 12.0f + 2.0f * fRa * grainSamples;
        const float dRb = 12.0f + 2.0f * fRb * grainSamples;

        const float revRawL = wLa * readFloatBufHermite(shimBufL, shimWritePos, dLa)
                            + wLb * readFloatBufHermite(shimBufL, shimWritePos, dLb);
        const float revRawR = wRa * readFloatBufHermite(shimBufR, shimWritePos, dRa)
                            + wRb * readFloatBufHermite(shimBufR, shimWritePos, dRb);

        const float oxAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2750.0 / sr));
        shimLpL += oxAlpha * (revRawL - shimLpL);
        shimLpR += oxAlpha * (revRawR - shimLpR);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        const float mixRawL = revRawL * 1.02f;
        const float mixRawR = revRawR * 1.02f;
        leslieDcL += dcAlpha * (mixRawL - leslieDcL);
        leslieDcR += dcAlpha * (mixRawR - leslieDcR);

        ioL = charFilter3.processL(charFilter2.processL(charFilter1.processL(mixRawL - leslieDcL)));
        ioR = charFilter3.processR(charFilter2.processR(charFilter1.processR(mixRawR - leslieDcR)));
        outExtraRevL = subLpFilter.processL(ioL) * 0.36f;
        outExtraRevR = subLpFilter.processR(ioR) * 0.36f;
    } else if (mode == 19 && shimMaxLen > 64) {
        // 33: Mohan Veena Sitar-Slide (Gourd Tumba & Jawari Throat + Dispersive Sympathetic Taraf Harp Halo!)
        // Note: Per-string parabolic bone Jawari grazing-bridge buzz is computed in renderBlock() with 0% chord intermod!
        lfoPhase1 += 0.29 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float bodyL = charFilter4.processL(charFilter3.processL(charFilter2.processL(charFilter1.processL(ioL))));
        const float bodyR = charFilter4.processR(charFilter3.processR(charFilter2.processR(charFilter1.processR(ioR))));

        // Dispersive metallic Taraf sympathetic string resonator network (tracks any key & slide at 0.00-cent fidelity!)
        float tarafL = dimHpFilter.processL(bodyL * 0.65f);
        float tarafR = dimHpFilter.processR(bodyR * 0.65f);
        for (size_t i = 0; i < 4; ++i) {
            auto& bL = springApL[i];
            auto& bR = springApR[i];
            int idxL = springApPosL[i], idxR = springApPosR[i];
            const float dL = bL[static_cast<size_t>(idxL)], dR = bR[static_cast<size_t>(idxR)];
            const float g = 0.67f;
            const float wL = tarafL + dL * g, wR = tarafR + dR * g;
            bL[static_cast<size_t>(idxL)] = wL;
            bR[static_cast<size_t>(idxR)] = wR;
            tarafL = dL - wL * g;
            tarafR = dR - wR * g;
            if (++idxL >= static_cast<int>(bL.size())) idxL = 0;
            if (++idxR >= static_cast<int>(bR.size())) idxR = 0;
            springApPosL[i] = idxL;
            springApPosR[i] = idxR;
        }

        const float wob = 0.14f * static_cast<float>(std::sin(kTwoPi * lfoPhase1));
        const float t1L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (37.0f + wob) * srMs));
        const float t1R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (53.0f - wob) * srMs));
        const float t2L = readFloatBufHermite(shimBufL, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (83.0f - wob) * srMs));
        const float t2R = readFloatBufHermite(shimBufR, shimWritePos, juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (113.0f + wob) * srMs));

        const float harpL = subLpFilter.processL(tarafL * 0.38f + t1L * 0.44f + t2R * 0.34f);
        const float harpR = subLpFilter.processR(tarafR * 0.38f + t1R * 0.44f + t2L * 0.34f);

        shimBufL[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.0f, 2.0f, tarafL * 0.58f + harpR * 0.30f);
        shimBufR[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.0f, 2.0f, tarafR * 0.58f + harpL * 0.30f);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        ioL = bodyL * 0.72f + harpL * 0.32f;
        ioR = bodyR * 0.72f + harpR * 0.32f;
        outExtraRevL = harpL * 0.40f + bodyL * 0.14f;
        outExtraRevR = harpR * 0.40f + bodyR * 0.14f;
    } else if (mode == 20) {
        // 34: Atlanta Neo-Soul Syrup (Faye Webster "Kingston" / Matt Stoessel: Ross Comp + Velvet Scoop + 1976 Boss CE-1)
        lfoPhase1 += 0.44 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.19 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float otaAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.0022 : 0.265)));
        cassetteSagEnv += otaAlpha * (pk - cassetteSagEnv);

        // Liquid OTA sustain + 1976 Boss CE-1 low-headroom analog JFET preamp warmth
        const float compGain = 1.25f / (0.55f + 1.35f * cassetteSagEnv);
        const float xL = juce::jlimit(-2.6f, 2.6f, ioL * compGain);
        const float xR = juce::jlimit(-2.6f, 2.6f, ioR * compGain);
        const float preRawL = (fastTanh(xL + 0.18f * xL * xL) - 0.09f * fastTanh(xL * xL)) * 0.78f;
        const float preRawR = (fastTanh(xR + 0.18f * xR * xR) - 0.09f * fastTanh(xR * xR)) * 0.78f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (preRawL - leslieDcL);
        leslieDcR += dcAlpha * (preRawR - leslieDcR);

        const float velvetL = charFilter4.processL(charFilter3.processL(charFilter2.processL(charFilter1.processL(preRawL - leslieDcL))));
        const float velvetR = charFilter4.processR(charFilter3.processR(charFilter2.processR(charFilter1.processR(preRawR - leslieDcR))));

        modBufL[static_cast<size_t>(modWritePos)] = velvetL;
        modBufR[static_cast<size_t>(modWritePos)] = velvetR;

        // Symmetrical 120-Deg 3-Phase Boss CE-1 MN3002 Ensemble (0.00-cent concert pitch lock + syrupy stereo melt!)
        const double th = kTwoPi * lfoPhase1;
        const double ps = kTwoPi * lfoPhase2;
        const float w1 = 1.85f * static_cast<float>(std::sin(th))            + 0.28f * static_cast<float>(std::sin(ps));
        const float w2 = 1.85f * static_cast<float>(std::sin(th + 2.094395)) + 0.28f * static_cast<float>(std::sin(ps + 2.094395));
        const float w3 = 1.85f * static_cast<float>(std::sin(th + 4.188790)) + 0.28f * static_cast<float>(std::sin(ps + 4.188790));

        const float tap1L = readFloatBufHermite(modBufL, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (7.40f  + w1) * srMs));
        const float tap2R = readFloatBufHermite(modBufR, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (10.10f + w2) * srMs));
        const float tap3C = 0.5f * (readFloatBufHermite(modBufL, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (12.80f + w3) * srMs))
                                  + readFloatBufHermite(modBufR, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (12.80f + w3) * srMs)));

        const float ce1L = subLpFilter.processL(dimHpFilter.processL(tap1L + 0.62f * tap3C - 0.30f * tap2R));
        const float ce1R = subLpFilter.processR(dimHpFilter.processR(tap2R + 0.62f * tap3C - 0.30f * tap1L));

        ioL = velvetL * 0.50f + ce1L * 0.70f;
        ioR = velvetR * 0.50f + ce1R * 0.70f;
        outExtraRevL = ioL * 0.28f;
        outExtraRevR = ioR * 0.28f;
    } else if (mode == 21 && shimMaxLen > 64) {
        // 35: Binson Echorec Space-Fury (Gilmour Cornish + Hiwatt DR103 Singing Lead -> 4-Head Rotating Steel Drum Swirl!)
        lfoPhase1 += 0.62 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;

        const float inPk = std::max(std::abs(ioL), std::abs(ioR));
        const float fastAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > gateEnv) ? 0.0020 : 0.085)));
        const float slowAlpha = static_cast<float>(1.0 - std::exp(-dt / ((inPk > cassetteSagEnv) ? 0.0050 : 0.240)));
        gateEnv        += fastAlpha * (inPk - gateEnv);
        cassetteSagEnv += slowAlpha * (inPk - cassetteSagEnv);

        // Clean Hiwatt 100W low-end punch (<195 Hz) + creamy Tube-Driver soaring lap-steel sustain (>175 Hz)
        const float bassL = fastTanh(charFilter1.processL(ioL) * 1.35f) * 0.36f;
        const float bassR = fastTanh(charFilter1.processR(ioR) * 1.35f) * 0.36f;

        const float drvGain = juce::jlimit(1.65f, 3.45f, 1.82f / (0.44f + cassetteSagEnv));
        const float hpL = juce::jlimit(-2.8f, 2.8f, dimHpFilter.processL(ioL) * drvGain);
        const float hpR = juce::jlimit(-2.8f, 2.8f, dimHpFilter.processR(ioR) * drvGain);
        const float rawDrvL = (fastTanh(hpL + 0.18f * hpL * hpL) - 0.09f * fastTanh(hpL * hpL)) * 0.46f;
        const float rawDrvR = (fastTanh(hpR + 0.18f * hpR * hpR) - 0.09f * fastTanh(hpR * hpR)) * 0.46f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 26.0 / sr));
        leslieDcL += dcAlpha * (rawDrvL - leslieDcL);
        leslieDcR += dcAlpha * (rawDrvR - leslieDcR);

        const float leadL = bassL + charFilter3.processL(charFilter2.processL(rawDrvL - leslieDcL));
        const float leadR = bassR + charFilter3.processR(charFilter2.processR(rawDrvR - leslieDcR));

        // 1968 Binson Echorec B2 Rotating Magnetic Steel Drum (Heads 1:2:3:4 = 1/16, 1/8, 1/8D, 1/4 Swirl — unconditionally stable!)
        const double safeBpm = juce::jlimit(75.0, 180.0, hostBpm);
        const float drumMs = tempoSync
            ? juce::jlimit(280.0f, 560.0f, static_cast<float>(60000.0 / safeBpm))
            : 380.0f;
        const float wow = 0.11f * static_cast<float>(std::sin(kTwoPi * lfoPhase1));

        const float dH1 = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (drumMs * 0.25f + wow) * srMs);
        const float dH2 = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (drumMs * 0.50f - wow) * srMs);
        const float dH3 = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (drumMs * 0.75f + wow) * srMs);
        const float dH4 = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), (drumMs * 1.00f - wow) * srMs);

        const float h1 = 0.5f * (readFloatBufHermite(shimBufL, shimWritePos, dH1) + readFloatBufHermite(shimBufR, shimWritePos, dH1));
        const float h2 = 0.5f * (readFloatBufHermite(shimBufL, shimWritePos, dH2) + readFloatBufHermite(shimBufR, shimWritePos, dH2));
        const float h3 = readFloatBufHermite(shimBufL, shimWritePos, dH3);
        const float h4 = readFloatBufHermite(shimBufR, shimWritePos, dH4);

        const float rawEchoL = h1 * 0.30f + h2 * 0.14f + h3 * 0.42f + h4 * 0.18f;
        const float rawEchoR = h1 * 0.14f + h2 * 0.36f + h3 * 0.16f + h4 * 0.40f;

        const float echoToneL = subLpFilter.processL(charFilter4.processL(rawEchoL) * 0.62f + rawEchoL * 0.34f);
        const float echoToneR = subLpFilter.processR(charFilter4.processR(rawEchoR) * 0.62f + rawEchoR * 0.34f);

        shimBufL[static_cast<size_t>(shimWritePos)] = fastTanh(leadL * 0.72f + echoToneR * 0.46f);
        shimBufR[static_cast<size_t>(shimWritePos)] = fastTanh(leadR * 0.72f + echoToneL * 0.46f);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        const float echoDuck = 0.52f + 0.48f / (1.0f + 2.8f * gateEnv);
        ioL = leadL * 0.76f + echoToneL * 0.62f * echoDuck;
        ioR = leadR * 0.76f + echoToneR * 0.62f * echoDuck;
        outExtraRevL = (leadL * 0.18f + echoToneL * 0.55f) * 0.30f;
        outExtraRevR = (leadR * 0.18f + echoToneR * 0.55f) * 0.30f;
    } else if (mode == 22 && shimMaxLen > 64) {
        // 36: CS-80 Stadium Synth-Steel (Daft Punk RAM / Mike Dean / Vangelis: 3-Phase CS-80 Chorus + Stadium Echo)
        // Note: 0.00-cent alias-free per-string CS-80 Sawtooth + Sub + IG0015 Brass Filter Envelope are synthesized in renderBlock()!
        lfoPhase1 += 0.64 * dt; if (lfoPhase1 >= 1.0) lfoPhase1 -= 1.0;
        lfoPhase2 += 0.21 * dt; if (lfoPhase2 >= 1.0) lfoPhase2 -= 1.0;

        const float pk = std::max(std::abs(ioL), std::abs(ioR));
        const float envAlpha = static_cast<float>(1.0 - std::exp(-dt / ((pk > cassetteSagEnv) ? 0.014 : 0.240)));
        cassetteSagEnv += envAlpha * (pk - cassetteSagEnv);

        const float drv = 1.12f / (0.76f + 0.85f * cassetteSagEnv);
        const float xL = juce::jlimit(-2.6f, 2.6f, ioL * drv);
        const float xR = juce::jlimit(-2.6f, 2.6f, ioR * drv);
        const float rawL = (fastTanh(xL + 0.14f * xL * xL) - 0.07f * fastTanh(xL * xL)) * 0.82f;
        const float rawR = (fastTanh(xR + 0.14f * xR * xR) - 0.07f * fastTanh(xR * xR)) * 0.82f;
        const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sr));
        leslieDcL += dcAlpha * (rawL - leslieDcL);
        leslieDcR += dcAlpha * (rawR - leslieDcR);

        const float brassDryL = charFilter3.processL(charFilter2.processL(charFilter1.processL(rawL - leslieDcL)));
        const float brassDryR = charFilter3.processR(charFilter2.processR(charFilter1.processR(rawR - leslieDcR)));

        modBufL[static_cast<size_t>(modWritePos)] = brassDryL;
        modBufR[static_cast<size_t>(modWritePos)] = brassDryR;

        // Symmetrical 120-Deg 3-Phase CS-80 Analog Chorus (0.00-cent concert pitch lock, massive widescreen width!)
        const double th = kTwoPi * lfoPhase1;
        const double ps = kTwoPi * lfoPhase2;
        const float w1 = 1.72f * static_cast<float>(std::sin(th))            + 0.30f * static_cast<float>(std::sin(ps));
        const float w2 = 1.72f * static_cast<float>(std::sin(th + 2.094395)) + 0.30f * static_cast<float>(std::sin(ps + 2.094395));
        const float w3 = 1.72f * static_cast<float>(std::sin(th + 4.188790)) + 0.30f * static_cast<float>(std::sin(ps + 4.188790));

        const float tap1L = readFloatBufHermite(modBufL, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (6.80f  + w1) * srMs));
        const float tap2R = readFloatBufHermite(modBufR, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (9.60f  + w2) * srMs));
        const float tap3C = 0.5f * (readFloatBufHermite(modBufL, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (12.40f + w3) * srMs))
                                  + readFloatBufHermite(modBufR, modWritePos, juce::jlimit(2.0f, static_cast<float>(modMaxLen - 4), (12.40f + w3) * srMs)));

        const float csChorL = subLpFilter.processL(dimHpFilter.processL(tap1L + 0.64f * tap3C - 0.32f * tap2R));
        const float csChorR = subLpFilter.processR(dimHpFilter.processR(tap2R + 0.64f * tap3C - 0.32f * tap1L));

        const float synthMixL = brassDryL * 0.48f + csChorL * 0.72f;
        const float synthMixR = brassDryR * 0.48f + csChorR * 0.72f;

        const double safeBpm = tempoSync ? juce::jlimit(68.0, 175.0, hostBpm) : 88.0;
        const float eighthMs = static_cast<float>(30000.0 / safeBpm);
        const float dStadL = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), juce::jlimit(150.0f, 440.0f, eighthMs) * srMs);
        const float dStadR = juce::jlimit(8.0f, static_cast<float>(shimMaxLen - 8), juce::jlimit(280.0f, 820.0f, eighthMs * 2.0f) * srMs);

        const float stadRawL = readFloatBufHermite(shimBufL, shimWritePos, dStadL);
        const float stadRawR = readFloatBufHermite(shimBufR, shimWritePos, dStadR);
        const float stadLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2650.0 / sr));
        shimLpL += stadLpAlpha * (stadRawL - shimLpL);
        shimLpR += stadLpAlpha * (stadRawR - shimLpR);

        shimBufL[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.4f, 2.4f, synthMixL * 0.78f + shimLpR * 0.32f);
        shimBufR[static_cast<size_t>(shimWritePos)] = juce::jlimit(-2.4f, 2.4f, synthMixR * 0.78f + shimLpL * 0.32f);
        if (++shimWritePos >= shimMaxLen) shimWritePos = 0;

        const float stadDuck = 0.14f + 0.28f / (1.0f + 4.2f * cassetteSagEnv);
        ioL = synthMixL + shimLpL * stadDuck;
        ioR = synthMixR + shimLpR * stadDuck;
        outExtraRevL = (synthMixL * 0.45f + shimLpL * 0.75f) * 0.34f;
        outExtraRevR = (synthMixR * 0.45f + shimLpR * 0.75f) * 0.34f;
    }

    if (wetGain < 0.9999f) {
        const float w = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * wetGain)));
        ioL = dryInL + (ioL - dryInL) * w;
        ioR = dryInR + (ioR - dryInR) * w;
        outExtraRevL *= w;
        outExtraRevR *= w;
    }

    if (++modWritePos >= modMaxLen) modWritePos = 0;
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
    modernChar.prepare(sampleRate);
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
    for (auto& ct : chokeTails) ct = StringVoice{};
    nextChokeSlot = 0;
    sustainPedalDown = false;
    pitchWheelNorm = 0.0f;
    mpeChannelPitchNorm.fill(0.0f);
    mpeChannelPressure.fill(0.0f);
    modWheelNorm = 0.0f;
    expressionNorm = 1.0f;
    keyswitchPedals.fill(0.0f);
    ccPedals.fill(0.0f);
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

    studioMidClarifierFilter.reset();
    studioDeltaCoilFilter.reset();
    studioDeltaWarmthFilter.reset();
    studioDeltaTrebleFilter.reset();
    studioTweedBarkFilter.reset();
    studioTweedCabFilter.reset();
    dcBlockerStudio.reset();
    tweedSagEnv = 0.0f;
    rackBellSmooth  = (params.fxCharacter == 15) ? 1.0f : 0.0f;
    organRankSmooth = (params.fxCharacter == 16) ? 1.0f : 0.0f;
    vg8BowedSmooth  = (params.fxCharacter == 17) ? 1.0f : 0.0f;
    jawariSmooth    = (params.fxCharacter == 19) ? 1.0f : 0.0f;
    cs80BrassSmooth = (params.fxCharacter == 22) ? 1.0f : 0.0f;

    sympathetic.reset();
    modernChar.reset();
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
    ccPedals.fill(0.0f);
    sustainPedalDown = false;
}

void PedalSteelEngine::updateFilterCoeffs() {
    const int st = juce::jlimit(0, 5, params.presetStyle);
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
    } else if (st == 4) {
        // Western Swing C6th Back-Neck: Plush 205 Hz 15" JBL D130F Standel body, scooped 590 Hz anti-boxy clarity & 2.15 kHz jazz silk
        midScoopFilter.setPeak(sampleRate, 1180.0f, 0.85f, -1.8f);
        bodyFreq = 205.0f;
        bodyGain = 3.2f + 2.4f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 2150.0f, 0.86f, 2.8f);
    } else if (st == 5) {
        // Baritone Canyon B6: Deep 185 Hz Mojave wound-resonator gravity & warm 1.95 kHz baritone throat
        midScoopFilter.setPeak(sampleRate, 1120.0f, 0.82f, -2.2f);
        bodyFreq = 185.0f;
        bodyGain = 5.0f + 2.8f * params.toneWarmth;
        speakerPresenceFilter.setPeak(sampleRate, 1950.0f, 0.86f, 3.4f);
    } else {
        midScoopFilter.setPeak(sampleRate, 1150.0f, 0.80f, -2.0f);
        bodyFreq = 240.0f;
        speakerPresenceFilter.setPeak(sampleRate, 2250.0f, 0.85f, 2.8f);
    }
    speakerBodyFilter.setPeak(sampleRate, bodyFreq, 0.78f, bodyGain);

    // Delta-calibrated filters for the 44.1 kHz Stereo Studio Bus
    const float midClarifyDb  = (st == 1) ? -3.2f : ((st == 4) ? -2.2f : ((st == 5) ? -1.8f : 0.0f));
    const float deltaCoilDb   = juce::jlimit(-6.0f, 6.0f, (params.coilResonance - kBaseCoilRes[st]) * 10.0f);
    const float deltaWarmthDb = juce::jlimit(-6.5f, 6.5f, (params.toneWarmth - kBaseWarmth[st]) * 9.5f)
                              + ((st == 5) ? 3.2f : ((st == 4) ? 1.4f : 0.0f));
    const float deltaDriveAir = juce::jlimit(-3.5f, 5.0f, (params.ampDrive - kBaseDrive[st]) * 5.2f)
                              - ((st == 4) ? 0.6f : ((st == 5) ? 1.8f : 0.0f));

    studioMidClarifierFilter.setPeak(sampleRate, (st == 4) ? 590.0f : 740.0f, 0.90f, midClarifyDb);
    studioDeltaCoilFilter.setPeak(sampleRate, coilFreq, 1.05f, deltaCoilDb);
    studioDeltaWarmthFilter.setPeak(sampleRate, (st == 5) ? 185.0f : ((st == 4) ? 195.0f : 255.0f), 0.75f, deltaWarmthDb);
    studioDeltaTrebleFilter.setPeak(sampleRate, 3650.0f, 0.78f, -0.65f * deltaWarmthDb + deltaDriveAir);

    const float extraDrive = std::max(0.0f, params.ampDrive - kBaseDrive[st]);
    studioTweedBarkFilter.setPeak(sampleRate, 940.0f, 0.78f, juce::jlimit(0.0f, 3.6f, extraDrive * 10.5f));
    studioTweedCabFilter.setLowpass(sampleRate, 4550.0f, 0.82f);
    modernChar.setTargetMode(sampleRate, params.fxCharacter);
    if (!modernChar.hasRendered) {
        rackBellSmooth  = (params.fxCharacter == 15) ? 1.0f : 0.0f;
        organRankSmooth = (params.fxCharacter == 16) ? 1.0f : 0.0f;
        vg8BowedSmooth  = (params.fxCharacter == 17) ? 1.0f : 0.0f;
        jawariSmooth    = (params.fxCharacter == 19) ? 1.0f : 0.0f;
        cs80BrassSmooth = (params.fxCharacter == 22) ? 1.0f : 0.0f;
    }
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
    // Styles 1 (Bootgaze), 4 (Western Swing C6th), and 5 (Baritone B6) build on clean, long-loop studio strings
    const int targetCleanStyle = (style == 1 || style == 4 || style == 5) ? 0 : style;
    for (int i = 0; i < PedalSteelMultiSamples::kNumMultiSamples; ++i) {
        const auto& ms = PedalSteelMultiSamples::kMultiSamples[i];
        float d = static_cast<float>(std::abs(midiNote - ms.midiNote));
        float stylePenalty = 0.0f;
        if (ms.style != targetCleanStyle) {
            if (targetCleanStyle == 3 || ms.style == 3) {
                stylePenalty = 26.0f; // Never mix Goodrich swell pad zones with fingerpicked styles
            } else if (style == 1 && ms.style == 2 && midiNote >= 73) {
                stylePenalty = 0.5f;  // High Bootgaze leads (D5..A5) love Style-2 high-bar steel ring!
            } else if (style == 5 && ms.style == 1 && ms.midiNote <= 60) {
                stylePenalty = 1.2f;  // Baritone low register can blend low A3/C4 zones smoothly
            } else if (ms.style == 1) {
                stylePenalty = 9.0f;  // Deprioritize short pre-overdriven loops in favor of clean long E9th studio strings
            } else {
                stylePenalty = 4.0f;
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
    const int mappedStyle = (style == 4) ? 0 : ((style == 5) ? 2 : style);
    const int st = juce::jlimit(0, 3, mappedStyle);
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

void PedalSteelEngine::pushVoiceToChokeTail(const StringVoice& v, int strIdx) {
    if (!v.active || v.ampEnv <= 0.0002f) return;
    auto& ct = chokeTails[static_cast<size_t>(nextChokeSlot)];
    nextChokeSlot = (nextChokeSlot + 1) % kNumChokeTails;
    ct = v;
    ct.held = false;
    ct.sustainedByPedal = false;
    ct.uiTriggered = false;
    ct.stringIndex = juce::jlimit(0, kNumStrings - 1, strIdx);
    ct.chokeFade = 1.0f;
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
    pushVoiceToChokeTail(v, strIdx);
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
    v.activePitchWheel = smoothPitchWheel;
    v.scoopOffsetSemis = 0.0f;
    v.scoopVel = 0.0f;
    v.phraseTargetAmp = juce::jlimit(0.35f, 1.0f, 0.30f + 0.70f * vel);
    v.ampEnv = 0.0f;
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
        if (params.presetStyle == 4) {
            // Back-Neck C6th Copedent role mapping: Root (C), Third (E/Eb), Fifth (G), Sixth/9th (A/D)
            if (rel == 0) v.role = StringRole::Root;
            else if (rel == 3 || rel == 4) v.role = StringRole::Third;
            else if (rel == 7) v.role = StringRole::Fifth;
            else v.role = StringRole::SixthOr9th;
        } else {
            // Standard E9th Copedent role mapping: Root (0), Third (3/4 -> Pedal B +1), Fifth (7 -> Pedal A +2),
            // while 9ths (2) & 6ths (9) stay pure in SixthOr9th so add9/maj9 chords never clash on A+B pulls!
            if (rel == 0) v.role = StringRole::Root;
            else if (rel == 3 || rel == 4) v.role = StringRole::Third;
            else if (rel == 7) v.role = StringRole::Fifth;
            else v.role = StringRole::SixthOr9th;
        }
    }
}

int PedalSteelEngine::chooseBestStringForNote(int midiNote, int channel, float /*velocity*/, bool& outIsLegatoTransfer, float& outPrevBarMidi) {
    outIsLegatoTransfer = false;
    outPrevBarMidi = static_cast<float>(midiNote);

    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (v.active && !v.isPhraseSlice && v.midiNote == midiNote && v.channel == channel) {
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
    const int maxReleaseGlideSemis = (params.fxCharacter == 17) ? 0 : ((params.playMode == 1) ? 7 : 2);
    int bestLegatoIdx = -1;
    int bestLegatoDist = 999;

    for (int i = 0; i < kNumStrings; ++i) {
        const auto& v = voices[i];
        if (!v.active || v.isPhraseSlice || v.channel != channel) continue;

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
        int openMidi = getOpenStringMidiForStyle(params.presetStyle, i);
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

        // 2. Channel-1 Low Studio Phrase Lick Keyswitches (MIDI 36..43 = C1..G1 on E9th; bypassed on low C6th/Baritone necks unless in Lick Mode)
        if (ch == 1 && note >= 36 && note <= 43 && ((params.presetStyle < 4 && params.fxCharacter == 0 && params.playMode != 5) || params.playMode == 4)) {
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
        // In Two Star VG-8 Bowed mode (fxCharacter == 17), Ch >= 3 is a monophonic single-string VG-8 VIO GUITAR solo voice:
        // overlapping notes perform continuous guitar hammer-on/pull-off/slide legato, while detached notes pick a fresh bow stroke!
        bool forceVg8MonoLegato = false;
        int vg8LegatoStrIdx = -1;
        float vg8PrevMidi = static_cast<float>(note);
        if (params.fxCharacter == 17 && ch >= 3) {
            for (int i = 0; i < kNumStrings; ++i) {
                if (voices[i].active && !voices[i].isPhraseSlice && voices[i].channel == ch && voices[i].held && voices[i].midiNote != note) {
                    if (std::abs(note - voices[i].midiNote) <= 12) {
                        forceVg8MonoLegato = true;
                        vg8LegatoStrIdx = i;
                        vg8PrevMidi = voices[i].barCurrentMidi + voices[i].pedalCurrentSemis + voices[i].scoopOffsetSemis;
                        break;
                    }
                }
            }
            if (!forceVg8MonoLegato) {
                for (int i = 0; i < kNumStrings; ++i) {
                    if (voices[i].active && !voices[i].isPhraseSlice && voices[i].channel == ch && voices[i].midiNote != note) {
                        pushVoiceToChokeTail(voices[i], i);
                        voices[i].active = false;
                        voices[i].held = false;
                        voices[i].ampEnv = 0.0f;
                    }
                }
            }
        }
        bool isLegatoTransfer = false;
        float prevMidi = static_cast<float>(note);
        int strIdx = 0;
        if (forceVg8MonoLegato) {
            isLegatoTransfer = true;
            prevMidi = vg8PrevMidi;
            strIdx = vg8LegatoStrIdx;
        } else {
            strIdx = chooseBestStringForNote(note, ch, vel, isLegatoTransfer, prevMidi);
        }
        auto& v = voices[strIdx];
        const bool wasSameNoteRinging = (!isLegatoTransfer && params.fxCharacter == 17 && v.active && !v.isPhraseSlice
                                         && v.midiNote == note && v.ampEnv > 0.06f);
        const float prevVg8BowEnv = wasSameNoteRinging ? v.vg8BowEnv : 0.0f;
        if (!isLegatoTransfer) {
            pushVoiceToChokeTail(v, strIdx);
        }

        v.active = true;
        v.held = true;
        v.sustainedByPedal = false;
        v.uiTriggered = false;
        v.isPhraseSlice = false;
        v.isPalmHarmonic = (params.playMode == 5);
        v.midiNote = note;
        v.channel = ch;
        v.velocity = vel;
        v.releaseAgeSec = 0.0;
        v.activePitchWheel = smoothPitchWheel;
        v.triggerOrder = ++noteCounter;

        if (isLegatoTransfer) {
            v.barCurrentMidi = prevMidi;
            v.barTargetMidi = static_cast<float>(note);
            v.scoopOffsetSemis = 0.0f;
            v.scoopVel = 0.0f;
            v.ampEnv = std::min(1.0f, std::max(v.ampEnv, (params.fxCharacter == 17 ? 0.92f : 0.45f) * vel) * 1.03f);
            v.vibratoEnv *= (params.fxCharacter == 17) ? 0.84f : 0.2f;
            v.transientGain = 0.0f;
            if (params.fxCharacter == 17) {
                const float slurredTouch = std::pow(juce::jlimit(0.0f, 1.0f, (vel - 0.36f) * (1.0f / 0.64f)), 1.45f);
                v.vg8BowEnv = std::max(v.vg8BowEnv, 0.92f);
                v.vg8TouchEnv = std::max(v.vg8TouchEnv, 0.36f * slurredTouch);
            }
        } else {
            const int sampleQueryMidi = v.isPalmHarmonic ? std::min(81, note + 12) : note;
            v.multiSampleIdx = selectBestMultiSample(sampleQueryMidi, params.presetStyle, vel);
            v.noteAgeSec = 0.0;
            v.barTargetMidi = static_cast<float>(note);
            v.barCurrentMidi = static_cast<float>(note);
            v.barVelocity = 0.0f;
            v.pedalCurrentSemis = 0.0f;
            v.pedalVel = 0.0f;
            v.mpePitchSemis = (ch >= 3) ? (mpeChannelPitchNorm[static_cast<size_t>(ch)] * 12.0f) : 0.0f;
            v.mpePitchVel = 0.0f;
            v.ampEnv = 0.0f;
            v.swellEnv = 1.0f - 0.96f * params.autoSwell;
            v.vibratoEnv = 0.0f;
            v.samplePos = 0.0;
            v.slideSvfBand = 0.0f;
            v.slideSvfLow = 0.0f;
            v.subHarmPhase = 0.0;
            v.chimeHpStateL = 0.0f;
            v.chimeHpStateR = 0.0f;
            v.chimeLpStateL = 0.0f;
            v.chimeLpStateR = 0.0f;

            v.vg8PhaseA = std::fmod(static_cast<double>(strIdx) * 0.37 + 0.11, 1.0);
            v.vg8PhaseB = std::fmod(static_cast<double>(strIdx) * 0.37 + 0.53, 1.0);
            v.vg8SubPhase = std::fmod(static_cast<double>(strIdx) * 0.23 + 0.19, 1.0);
            v.vg8AgcEnv = 0.06f;
            v.vg8BowEnv = wasSameNoteRinging ? (0.70f * prevVg8BowEnv) : 0.0f;
            v.vg8TouchEnv = 1.0f;
            v.vg8DcL = 0.0f;
            v.vg8DcR = 0.0f;
            v.vg8Svf1Ic1L = v.vg8Svf1Ic2L = 0.0f;
            v.vg8Svf2Ic1L = v.vg8Svf2Ic2L = 0.0f;
            v.vg8Svf1Ic1R = v.vg8Svf1Ic2R = 0.0f;
            v.vg8Svf2Ic1R = v.vg8Svf2Ic2R = 0.0f;

            for (int h = 0; h < kMaxHarmonics; ++h) {
                v.harmPhases[static_cast<size_t>(h)] = std::fmod(kPi * static_cast<double>(h * (h + 1)) / static_cast<double>(kMaxHarmonics) + static_cast<double>(strIdx) * 0.57, kTwoPi);
            }

            v.transientIdx = (params.presetStyle * 2 + (strIdx % 2)) % PedalSteelAssets::kNumTransients;
            v.transientPos = 0.0;
            const float palmTransientScale = v.isPalmHarmonic ? 0.22f : 1.0f;
            v.transientGain = params.pickSnap * palmTransientScale * (1.0f - 0.85f * params.autoSwell) * (0.40f + 0.60f * vel);

            reassignChordRoles();

            v.scoopOffsetSemis = 0.0f;
            v.scoopVel = 0.0f;
            if (params.scoopDepth > 0.02f && params.playMode != 3) {
                int totalActiveNow = 0;
                for (const auto& ov : voices) if (ov.active && !ov.isPhraseSlice && ov.held) ++totalActiveNow;

                const bool isSwellPad = (params.playMode == 2 || params.autoSwell >= 0.65f);
                if (totalActiveNow == 1 && !isSwellPad) {
                    if (vel >= 0.72f) {
                        v.scoopOffsetSemis = -0.35f * params.scoopDepth;
                    }
                } else if (totalActiveNow >= (isSwellPad ? 2 : 3)) {
                    const float thirdScoop = isSwellPad ? (-0.92f * params.scoopDepth) : (-0.45f * params.scoopDepth);
                    for (auto& ov : voices) {
                        if (ov.active && !ov.isPhraseSlice && ov.held && ov.noteAgeSec < 0.035) {
                            if (ov.role == StringRole::Third) {
                                ov.scoopOffsetSemis = thirdScoop;
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
        } else if (cc >= 65 && cc <= 69) {
            ccPedals[static_cast<size_t>(cc - 65)] = val;
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
    static constexpr int kChordsE9[4][3] = {
        { 64, 68, 71 }, // E Major
        { 62, 67, 71 }, // G Major
        { 62, 66, 69 }, // D Major
        { 60, 64, 67 }  // C Major
    };
    static constexpr int kChordsC6[4][4] = {
        { 56, 61, 66, 71 }, // E6/9 Western Swing
        { 55, 59, 64, 69 }, // G6/9 Western Swing
        { 54, 59, 64, 69 }, // D6/9 Western Swing
        { 52, 57, 62, 67 }  // C6/9 Western Swing
    };
    static constexpr int kChordsB6[4][3] = {
        { 40, 47, 56 }, // E Baritone Open Canyon Shell
        { 43, 50, 59 }, // G Baritone Open Canyon Shell
        { 38, 45, 54 }, // D Baritone Open Canyon Shell
        { 36, 43, 52 }  // C Baritone Open Canyon Shell
    };
    const int idx = juce::jlimit(0, 3, chordIndex);
    const int savedMode = params.playMode;
    if (savedMode == 4) params.playMode = 0;
    if (params.presetStyle == 4) {
        for (int i = 0; i < 4; ++i) handleMidiMessage(juce::MidiMessage::noteOn(1, kChordsC6[idx][i], 0.82f));
    } else if (params.presetStyle == 5) {
        for (int i = 0; i < 3; ++i) handleMidiMessage(juce::MidiMessage::noteOn(1, kChordsB6[idx][i], 0.84f));
    } else {
        for (int i = 0; i < 3; ++i) handleMidiMessage(juce::MidiMessage::noteOn(1, kChordsE9[idx][i], 0.82f));
    }
    params.playMode = savedMode;
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
    const float openMidi = static_cast<float>(getOpenStringMidiForStyle(params.presetStyle, s));
    const float targetMidi = openMidi + juce::jlimit(0.0f, 24.0f, fret);
    const int nearMidi = juce::jlimit(36, 96, static_cast<int>(std::round(targetMidi)));

    auto& v = voices[static_cast<size_t>(s)];
    pushVoiceToChokeTail(v, s);
    v.active = true;
    v.held = true;
    v.sustainedByPedal = false;
    v.uiTriggered = true;
    v.isPhraseSlice = false;
    v.isPalmHarmonic = (params.playMode == 5);
    v.midiNote = nearMidi;
    v.channel = 1;
    v.velocity = 0.84f;
    v.noteAgeSec = 0.0;
    v.releaseAgeSec = 0.0;
    v.triggerOrder = ++noteCounter;
    v.multiSampleIdx = selectBestMultiSample(v.isPalmHarmonic ? std::min(81, nearMidi + 12) : nearMidi, params.presetStyle, 0.84f);
    v.barTargetMidi = targetMidi;
    v.barCurrentMidi = targetMidi;
    v.barVelocity = 0.0f;
    v.pedalCurrentSemis = 0.0f;
    v.pedalVel = 0.0f;
    v.mpePitchSemis = 0.0f;
    v.mpePitchVel = 0.0f;
    v.activePitchWheel = smoothPitchWheel;
    v.scoopOffsetSemis = 0.0f;
    v.scoopVel = 0.0f;
    v.ampEnv = 0.0f;
    v.swellEnv = 1.0f - 0.90f * params.autoSwell;
    v.vibratoEnv = 0.0f;
    v.samplePos = 0.0;
    v.slideSvfBand = 0.0f;
    v.slideSvfLow = 0.0f;
    v.subHarmPhase = 0.0;
    v.chimeHpStateL = 0.0f;
    v.chimeHpStateR = 0.0f;
    for (int h = 0; h < kMaxHarmonics; ++h) {
        v.harmPhases[static_cast<size_t>(h)] = std::fmod(kPi * static_cast<double>(h * (h + 1)) / static_cast<double>(kMaxHarmonics) + static_cast<double>(s) * 0.57, kTwoPi);
    }
    v.transientIdx = (params.presetStyle * 2 + (s % 2)) % PedalSteelAssets::kNumTransients;
    v.transientPos = 0.0;
    v.transientGain = params.pickSnap * (v.isPalmHarmonic ? 0.22f : 1.0f) * (1.0f - 0.85f * params.autoSwell) * 0.90f;
    reassignChordRoles();
}

void PedalSteelEngine::slideFretboardNote(int stringIndex, float fret) {
    const int s = juce::jlimit(0, kNumStrings - 1, stringIndex);
    auto& v = voices[static_cast<size_t>(s)];
    if (v.active && v.held && !v.isPhraseSlice) {
        const float openMidi = static_cast<float>(getOpenStringMidiForStyle(params.presetStyle, s));
        v.barTargetMidi = openMidi + juce::jlimit(0.0f, 24.0f, fret);
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
    st.presetStyle = params.presetStyle;
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
            const float openMidi = static_cast<float>(getOpenStringMidiForStyle(params.presetStyle, i));
            barSum += std::max(0.0f, v.barCurrentMidi - openMidi);
        }
    }
    st.activeVoiceCount = activeCount;
    st.barPositionFret = (activeCount > 0) ? (barSum / static_cast<float>(activeCount)) : 0.0f;
    return st;
}

void PedalSteelEngine::renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const double dt = 1.0 / sampleRate;
    const float fSr = static_cast<float>(sampleRate);
    const int stStyle = juce::jlimit(0, 5, params.presetStyle);

    float maxAftertouch = 0.0f;
    for (float p : mpeChannelPressure) maxAftertouch = std::max(maxAftertouch, p);

    const float targetA   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalA, uiPedals[0], keyswitchPedals[0], ccPedals[0], modWheelNorm }));
    const float targetB   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalB, uiPedals[1], keyswitchPedals[1], ccPedals[1], juce::jlimit(0.0f, 1.0f, (modWheelNorm - 0.12f) * 1.14f) }));
    const float targetC   = juce::jlimit(0.0f, 1.0f, std::max({ params.pedalC, uiPedals[2], keyswitchPedals[2], ccPedals[2] }));
    const float targetLKL = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKL, uiPedals[3], keyswitchPedals[3], ccPedals[3] }));
    const float targetLKR = juce::jlimit(0.0f, 1.0f, std::max({ params.leverLKR, uiPedals[4], keyswitchPedals[4], ccPedals[4] }));
    const float targetVol = juce::jlimit(0.0f, 1.0f, params.volumePedal * expressionNorm + 0.15f * maxAftertouch);

    const float pedalTimeSec = std::max(0.045f, params.pedalSpeed * 0.001f);
    const float pedalOmega   = 3.10f / pedalTimeSec;

    const float minBarSec    = (params.fxCharacter == 17) ? 0.0085f : 0.030f;
    const float barTimeSec   = std::max(minBarSec, params.barGlide * 0.001f);
    const float barOmega     = 2.85f / barTimeSec;

    const double effectiveSwellMs = params.tempoSync
        ? juce::jlimit(30.0, 1200.0, (60000.0 / juce::jlimit(40.0, 260.0, params.hostBpm)) * (params.swellTime / 400.0))
        : static_cast<double>(params.swellTime);
    const float swellRate = static_cast<float>(dt / std::max(0.025, effectiveSwellMs * 0.001));

    const double stringTauSec = 0.65 + 0.15 * std::max(1.0, static_cast<double>(params.sustainTime));
    const float sustainDecayRate = static_cast<float>(1.0 - std::exp(-dt / stringTauSec));
    const float releaseTimeSec = (params.fxCharacter == 17) ? 0.215f : ((params.playMode == 2) ? 0.55f : 0.28f);
    const float releaseDecayRate = static_cast<float>(1.0 - std::exp(-dt / releaseTimeSec));

    const float effectiveDelayMs = params.tempoSync
        ? static_cast<float>(computeSyncedDelayMs(params.delayTime, params.hostBpm))
        : params.delayTime;

    const float outGain  = juce::Decibels::decibelsToGain(params.outputLevel);
    const float coilFreq = juce::jlimit(1200.0f, 3400.0f, params.coilPeakFreq);
    const float coilQ    = 0.85f + 0.85f * params.coilResonance;

    float* outL = buffer.getWritePointer(0, startSample);
    float* outR = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1, startSample) : nullptr;

    bool anyVoiceHeld = false;
    for (const auto& v : voices) {
        if (v.active && (v.held || v.sustainedByPedal)) { anyVoiceHeld = true; break; }
    }

    for (int s = 0; s < numSamples; ++s) {
        const float pSmooth = static_cast<float>(1.0 - std::exp(-dt * 35.0));
        smoothPedalA += (targetA - smoothPedalA) * pSmooth;
        smoothPedalB += (targetB - smoothPedalB) * pSmooth;
        smoothPedalC += (targetC - smoothPedalC) * pSmooth;
        smoothLKL    += (targetLKL - smoothLKL) * pSmooth;
        smoothLKR    += (targetLKR - smoothLKR) * pSmooth;
        smoothVolPedal += (targetVol - smoothVolPedal) * pSmooth;
        const float targetRack  = (params.fxCharacter == 15) ? 1.0f : 0.0f;
        const float targetOrgan = (params.fxCharacter == 16) ? 1.0f : 0.0f;
        const float rankAlpha   = static_cast<float>(1.0 - std::exp(-dt / 0.0085));
        rackBellSmooth  += (targetRack  - rackBellSmooth)  * rankAlpha;
        organRankSmooth += (targetOrgan - organRankSmooth) * rankAlpha;
        vg8BowedSmooth  += (((params.fxCharacter == 17) ? 1.0f : 0.0f) - vg8BowedSmooth)  * rankAlpha;
        jawariSmooth    += (((params.fxCharacter == 19) ? 1.0f : 0.0f) - jawariSmooth)    * rankAlpha;
        cs80BrassSmooth += (((params.fxCharacter == 22) ? 1.0f : 0.0f) - cs80BrassSmooth) * rankAlpha;

        const float pwOmega = (!anyVoiceHeld) ? 28.0f : std::min(barOmega, 10.5f);
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
                    v.ampEnv -= v.ampEnv * static_cast<float>(1.0 - std::exp(-dt / 0.18));
                    if (v.ampEnv < 0.0002f) {
                        v.active = false;
                        v.ampEnv = 0.0f;
                        continue;
                    }
                } else {
                    const float pAtk = static_cast<float>(1.0 - std::exp(-dt / 0.0035));
                    v.ampEnv += (v.phraseTargetAmp - v.ampEnv) * pAtk;
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

                if (v.held || v.sustainedByPedal) {
                    v.activePitchWheel = smoothPitchWheel;
                }
                const float liveBendSemis = v.phraseTransposeSemis + v.pedalCurrentSemis + v.activePitchWheel * 7.0f;
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
                const double atkWin = v.isPalmHarmonic ? 0.026 : 0.016;
                const float targetAmp = (params.fxCharacter == 17) ? (0.56f + 0.44f * v.velocity) : v.velocity;
                if (v.noteAgeSec < atkWin) {
                    const double atkTau = v.isPalmHarmonic ? 0.0110 : 0.0040;
                    const float atkRate = static_cast<float>(1.0 - std::exp(-dt / atkTau));
                    v.ampEnv += (targetAmp - v.ampEnv) * atkRate;
                } else {
                    v.ampEnv -= v.ampEnv * sustainDecayRate * (1.0f - 0.78f * vg8BowedSmooth);
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

            // 2. Mechanical Bellcrank Pedal Pull & MPE Per-Note Oblique Bend (E9th Front-Neck vs C6th Back-Neck)
            float roleTargetSemis = 0.0f;
            if (stStyle == 4) {
                // Authentic Emmons D-10 Back-Neck C6th Copedent (Pedals 5, 6, 7 jazz 6/9 -> C9 & minor-6th slants!)
                if (v.role == StringRole::Root) {
                    roleTargetSemis = 2.0f * smoothPedalA + 1.0f * smoothLKL - 1.0f * smoothLKR; // C -> D (9th!)
                } else if (v.role == StringRole::Third) {
                    roleTargetSemis = -1.0f * smoothPedalC - 0.5f * smoothLKR; // E -> Eb (Pedal 6 minor-6th jazz slant!)
                } else if (v.role == StringRole::Fifth) {
                    roleTargetSemis = 2.0f * smoothPedalC; // G -> A
                } else {
                    roleTargetSemis = 1.0f * smoothPedalB; // A -> Bb (Pedal 7 dominant b7th -> instant rootless V9!)
                }
            } else {
                if (v.role == StringRole::Third) {
                    roleTargetSemis = 1.0f * smoothPedalB - 0.5f * smoothLKR;
                } else if (v.role == StringRole::Fifth) {
                    roleTargetSemis = 2.0f * smoothPedalA + 2.0f * smoothPedalC;
                } else if (v.role == StringRole::Root) {
                    roleTargetSemis = 1.0f * smoothLKL - 1.0f * smoothLKR + 2.0f * smoothPedalC;
                } else {
                    roleTargetSemis = 0.0f;
                }
            }

            const float pedalErr = roleTargetSemis - v.pedalCurrentSemis;
            const float pedalAccel = pedalOmega * pedalOmega * pedalErr - 1.95f * pedalOmega * v.pedalVel;
            v.pedalVel += pedalAccel * static_cast<float>(dt);
            v.pedalCurrentSemis += v.pedalVel * static_cast<float>(dt);

            if (v.channel >= 3) {
                if (v.held || v.sustainedByPedal) {
                    const float mpeTarget = mpeChannelPitchNorm[static_cast<size_t>(v.channel)] * 12.0f;
                    const float mpeAccel = pedalOmega * pedalOmega * (mpeTarget - v.mpePitchSemis) - 1.95f * pedalOmega * v.mpePitchVel;
                    v.mpePitchVel += mpeAccel * static_cast<float>(dt);
                    v.mpePitchSemis += v.mpePitchVel * static_cast<float>(dt);
                }
            } else {
                v.mpePitchSemis = 0.0f;
                v.mpePitchVel = 0.0f;
            }

            if (std::abs(v.scoopOffsetSemis) > 0.0001f || std::abs(v.scoopVel) > 0.0001f) {
                const float scoopAccel = -pedalOmega * pedalOmega * v.scoopOffsetSemis - 1.95f * pedalOmega * v.scoopVel;
                v.scoopVel += scoopAccel * static_cast<float>(dt);
                v.scoopOffsetSemis += v.scoopVel * static_cast<float>(dt);
            }

            // 3. Delayed Wrist-Roll Tonebar Vibrato (boosted expressively by Aftertouch, gently tamed on fast Leslie rotary!)
            const float chPress = mpeChannelPressure[static_cast<size_t>(v.channel)];
            const float pitchSpeedSemisPerSec = std::abs(v.barVelocity) + std::abs(v.pedalVel) + std::abs(v.mpePitchVel) + std::abs(v.scoopVel) + std::abs(smoothPitchWheelVel) * 7.0f;
            maxSlideSpeed = std::max(maxSlideSpeed, pitchSpeedSemisPerSec);
            const double vibDelaySec = (params.fxCharacter == 17 && v.channel >= 3) ? 0.085 : 0.24;
            const double vibRiseRate = (params.fxCharacter == 17 && v.channel >= 3) ? 6.2 : 2.4;
            const float vibSlideThresh = (params.fxCharacter == 17 && v.channel >= 3) ? 2.2f : 0.8f;
            if (pitchSpeedSemisPerSec < vibSlideThresh && v.noteAgeSec > vibDelaySec) {
                v.vibratoEnv = std::min(1.0f, v.vibratoEnv + static_cast<float>(dt * vibRiseRate));
            } else {
                const double vibFallRate = (params.fxCharacter == 17 && v.channel >= 3) ? 2.2 : 5.0;
                v.vibratoEnv = std::max(0.0f, v.vibratoEnv - static_cast<float>(dt * vibFallRate));
            }
            const float atVibScale   = (params.fxCharacter == 8) ? 0.10f : ((params.fxCharacter == 17 && v.channel >= 3) ? 1.15f : 0.40f);
            const float leadVibBoost = (params.fxCharacter == 17 && v.channel >= 3) ? 0.34f : 0.0f;
            const float vibAmt       = juce::jlimit(0.0f, 2.10f, params.vibratoDepth + leadVibBoost + atVibScale * chPress);
            const float vibDepthSemi = (params.fxCharacter == 17 && v.channel >= 3) ? 0.155f : 0.072f;
            const float vibratoSemis = barVibratoSin * (vibDepthSemi * vibAmt * v.vibratoEnv);

            if (v.held || v.sustainedByPedal) {
                v.activePitchWheel = smoothPitchWheel;
            }
            const float cabDrop = (std::abs(roleTargetSemis) < 0.05f) ? cabinetDropSemis : 0.0f;
            const float pitchWheelSemis = v.activePitchWheel * 7.0f;
            const float instMidi = v.barCurrentMidi + v.pedalCurrentSemis + v.mpePitchSemis + v.scoopOffsetSemis + vibratoSemis + cabDrop + pitchWheelSemis;
            const float instFreqHz = juce::jlimit(40.0f, 4000.0f, midiToHz(instMidi));

            //==================================================================
            // Physical E9th/C6th/B6 String Harmonics + Palm Chime + Chrome-Bar Slide
            //==================================================================
            const float openStrMidi = static_cast<float>(getOpenStringMidiForStyle(stStyle, i));
            const float scaleInches = (stStyle == 5) ? 26.75f : 24.25f;
            const float fretSemis = juce::jlimit(-4.0f, 26.0f, v.barCurrentMidi - openStrMidi);
            const float barStringLenInches = scaleInches * std::pow(2.0f, -fretSemis * (1.0f / 12.0f));
            const float betaPickup = juce::jlimit(0.05f, 0.46f, params.pickupPos / barStringLenInches);
            const float betaPick   = juce::jlimit(0.07f, 0.48f, (params.pickupPos + 0.95f) / barStringLenInches);

            const float regPos = juce::jlimit(0.0f, 2.999f, (instMidi - 54.0f) * (3.0f / 21.0f));
            const int regIdx0 = static_cast<int>(regPos);
            const int regIdx1 = std::min(3, regIdx0 + 1);
            const float regFrac = regPos - static_cast<float>(regIdx0);
            const int profStyle = (stStyle >= 4) ? 0 : stStyle;
            const int baseProf = profStyle * 4;
            const float* refHarm0 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx0];
            const float* refHarm1 = PedalSteelAssets::kHarmonicProfiles[baseProf + regIdx1];

            float harmSum = 0.0f;
            const float nyquistLimit = fSr * 0.44f;
            const float ageF = static_cast<float>(v.noteAgeSec);

            // Baritone Canyon B6 (-12st resonator) & Poly-Octave Organ Sub-Octave Foundation
            float organPipeMono = 0.0f;
            float vg8BowedL = 0.0f, vg8BowedR = 0.0f;
            const bool isVg8LeadVoice = (v.channel >= 3);
            if ((stStyle == 5 || organRankSmooth > 0.0005f || cs80BrassSmooth > 0.0005f) && !v.isPalmHarmonic) {
                const float subHz = instFreqHz * 0.5f;
                if (subHz >= 24.0f) {
                    v.subHarmPhase += kTwoPi * static_cast<double>(subHz) * dt;
                    if (v.subHarmPhase >= kTwoPi) v.subHarmPhase -= kTwoPi;
                    const float subSin = static_cast<float>(std::sin(v.subHarmPhase));
                    if (stStyle == 5) {
                        harmSum += 0.34f * subSin;
                    }
                    if (organRankSmooth > 0.0005f) {
                        organPipeMono += 0.085f * subSin * organRankSmooth;
                    }
                    if (cs80BrassSmooth > 0.0005f) {
                        organPipeMono += 0.056f * subSin * cs80BrassSmooth;
                    }
                }
            }

            const float barGlideSheen = juce::jlimit(0.0f, 1.0f, pitchSpeedSemisPerSec * 0.14f);
            const float pluckSnapEnv = std::exp(-ageF / (0.12f + 0.08f * params.pickSnap)) * (1.0f - 0.78f * params.autoSwell) * v.swellEnv;
            const float barMetalEnv  = (0.22f + 0.78f * std::exp(-ageF / 0.28f) + 0.45f * barGlideSheen) * (0.55f + 0.45f * params.pickSnap + 0.35f * params.ampDrive);

            float csAtk = 0.0f, csCutoffHz = 1200.0f;
            if (cs80BrassSmooth > 0.0005f && !v.isPalmHarmonic) {
                csAtk = 1.0f - std::exp(-ageF / 0.078f);
                const float csSettle = 0.42f + 0.58f * std::exp(-ageF / 1.65f);
                csCutoffHz = juce::jlimit(360.0f, 4400.0f,
                    340.0f + (920.0f + 2350.0f * v.velocity + 1250.0f * chPress + 720.0f * barGlideSheen) * csAtk * csSettle);
            }

            for (int h = 1; h <= 12; ++h) {
                const float hFloat = static_cast<float>(h);
                const float dispCoeff = (stStyle == 5) ? 0.000018f : 0.000010f;
                const float disp = 1.0f + dispCoeff * static_cast<float>((h - 1) * (h - 1));
                const float fh = instFreqHz * hFloat * disp;
                if (fh >= nyquistLimit) break;

                double ph = v.harmPhases[static_cast<size_t>(h - 1)] + kTwoPi * static_cast<double>(fh) * dt;
                if (ph >= kTwoPi) ph -= kTwoPi;
                v.harmPhases[static_cast<size_t>(h - 1)] = ph;

                const float spatialComb = std::abs(std::sin(static_cast<float>(kPi) * hFloat * betaPick) *
                                                   std::sin(static_cast<float>(kPi) * hFloat * betaPickup));

                float sustainWeight = 0.0f;
                float twangWeight = 0.0f;

                if (v.isPalmHarmonic) {
                    // 12th-Fret Right-Palm Harmonic Node Damping: suppresses fundamental (h=1) while exciting
                    // warm, round water-drop 2nd, 3rd & 4th bell chimes with flesh-damped upper partials!
                    const float palmSilk = (fh > 1750.0f && h >= 3) ? std::exp(-(fh - 1750.0f) * 0.00085f) : 1.0f;
                    if (h == 1)      sustainWeight = 0.10f;
                    else if (h == 2) sustainWeight = 1.32f;
                    else if (h == 3) sustainWeight = 0.24f * palmSilk * std::exp(-ageF / std::max(1.2f, params.sustainTime * 0.65f));
                    else if (h == 4) sustainWeight = 0.28f * palmSilk * std::exp(-ageF / std::max(0.9f, params.sustainTime * 0.48f));
                    else             sustainWeight = 0.05f * palmSilk * std::exp(-1.10f * static_cast<float>(h - 4)) * std::exp(-ageF / 0.30f);
                } else {
                    const float hDecayTau = (h <= 2)
                        ? 12.0f
                        : ((h == 3) ? (params.sustainTime * 0.65f) : (0.36f / static_cast<float>(h - 2)));
                    const float hSustainDecay = (h <= 2)
                        ? 1.0f
                        : ((h == 3) ? std::exp(-ageF / std::max(0.8f, hDecayTau))
                                    : (0.14f + 0.34f * barGlideSheen + 0.86f * std::exp(-ageF / std::max(0.09f, hDecayTau))));

                    const int refIdx = std::min(11, h - 1);
                    const float refH = refHarm0[refIdx] + regFrac * (refHarm1[refIdx] - refHarm0[refIdx]);
                    if (h == 1) {
                        sustainWeight = (stStyle == 0) ? 0.86f : ((stStyle == 5) ? 0.98f : ((stStyle == 4) ? 0.94f : std::max(0.75f, refH)));
                    } else if (h == 2) {
                        float h2Base = (stStyle == 1) ? 0.92f : (stStyle == 0 ? 0.82f : (stStyle == 4 ? 0.78f : (stStyle == 3 ? 0.56f : 0.62f)));
                        sustainWeight = std::max(h2Base, refH * 0.94f);
                    } else if (h == 3) {
                        float h3Scale = (stStyle == 0) ? 0.86f : (stStyle == 1 ? 0.44f : (stStyle == 4 ? 0.48f : 0.56f));
                        sustainWeight = (refH * h3Scale + 0.14f * spatialComb) * hSustainDecay;
                    } else {
                        float styleMetal = (stStyle == 0) ? 0.62f : (stStyle == 1 ? 0.42f : (stStyle == 2 ? 0.52f : (stStyle == 4 ? 0.24f : 0.32f)));
                        if (stStyle == 0 && h == 5) styleMetal *= 0.65f;
                        const float metalRing = (0.30f + 0.70f * spatialComb) * (1.0f / std::pow(hFloat - 1.5f, 1.25f));
                        sustainWeight = (refH * styleMetal * 0.38f + metalRing * styleMetal * barMetalEnv) * hSustainDecay;
                    }

                    twangWeight = (h >= 2)
                        ? ((0.25f + 0.75f * spatialComb) * (0.68f / std::pow(hFloat, 0.55f)) * pluckSnapEnv * (0.40f + 0.60f * params.pickSnap))
                        : 0.0f;
                }

                const float fNorm = fh / coilFreq;
                const float denom = (1.0f - fNorm * fNorm) * (1.0f - fNorm * fNorm) + (fNorm / coilQ) * (fNorm / coilQ);
                const float lcGain = 1.0f / std::sqrt(std::max(0.42f, denom));
                const float highRegPurity = (fh > 4200.0f && h >= 4) ? std::exp(-(fh - 4200.0f) * 0.0009f) : 1.0f;
                const float vg8HarmScale = (!v.isPalmHarmonic) ? (1.0f - vg8BowedSmooth) : 1.0f;

                harmSum += (sustainWeight + twangWeight * vg8HarmScale) * vg8HarmScale * lcGain * highRegPurity * static_cast<float>(std::sin(ph));
                if (organRankSmooth > 0.0005f && !v.isPalmHarmonic && h == 2) {
                    organPipeMono += 0.045f * organRankSmooth * static_cast<float>(std::sin(ph));
                }
                if (rackBellSmooth > 0.0005f && !v.isPalmHarmonic && (h == 2 || h == 4 || h == 6)) {
                    const float bellEnv = std::exp(-ageF / 0.28f) * (0.35f + 0.65f * v.velocity) * (1.0f - 0.65f * params.autoSwell);
                    const float bellW   = (h == 2) ? (0.026f + 0.058f * bellEnv)
                                                   : ((h == 4) ? (0.012f + 0.046f * bellEnv) : (0.032f * bellEnv));
                    organPipeMono += bellW * highRegPurity * rackBellSmooth * static_cast<float>(std::sin(ph));
                }
                if (cs80BrassSmooth > 0.0005f && !v.isPalmHarmonic) {
                    const float rCut = fh / csCutoffHz;
                    const float rCut2 = rCut * rCut;
                    const float lpDen = (1.0f - rCut2) * (1.0f - rCut2) + (rCut / 1.75f) * (rCut / 1.75f);
                    const float csLpGain = 1.0f / std::sqrt(std::max(0.16f, lpDen));
                    const float sawSign = (h & 1) ? 1.0f : -1.0f;
                    const float csHarmW = (0.108f / static_cast<float>(std::pow(hFloat, 0.85f))) * sawSign * csLpGain * csAtk;
                    organPipeMono += csHarmW * highRegPurity * cs80BrassSmooth * static_cast<float>(std::sin(ph));
                }
            }

            // Physical Chrome-Bar Winding Slide Layer (bypassed during VG-8 VIO GUITAR mode so fret slurs stay pure)
            float slideWhistle = 0.0f;
            const float barSlideRate = std::abs(v.barVelocity) + std::abs(smoothPitchWheelVel) * 7.0f + 0.35f * std::abs(v.pedalVel);
            if (params.barSlideNoise > 0.01f && barSlideRate > 1.1f && vg8BowedSmooth < 0.5f) {
                const float woundFactor = (stStyle == 5) ? ((i >= 3) ? 1.65f : 0.95f) : ((i >= 5) ? 1.25f : 0.55f);
                const float slideDrive = juce::jlimit(0.0f, 1.0f, (barSlideRate - 1.1f) * 0.065f) * params.barSlideNoise * woundFactor;
                const float mult = (stStyle == 5) ? 2.8f : 4.0f;
                const float svfCutoff = juce::jlimit(520.0f, 3200.0f, instFreqHz * mult);
                const float svfF = std::min(0.65f, 2.0f * std::sin(static_cast<float>(kPi) * svfCutoff / fSr));
                const float svfQ = 0.085f;
                const float noiseIn = nextRandomSigned() * slideDrive;
                v.slideSvfLow  += svfF * v.slideSvfBand;
                const float svfHigh = noiseIn - v.slideSvfLow - svfQ * v.slideSvfBand;
                v.slideSvfBand += svfF * svfHigh;
                slideWhistle = v.slideSvfBand * 0.028f;
            } else {
                const float svfDecay = std::pow(0.988f, 44100.0f / fSr);
                v.slideSvfBand *= svfDecay;
                v.slideSvfLow  *= svfDecay;
            }

            // Real Metal Fingerpick (or Flesh-Damped Thumb Chime) Transient Grain
            float transientSig = 0.0f;
            float highPickScrape = 0.0f;
            if (v.transientPos < static_cast<double>(PedalSteelAssets::kTransientSize - 1) && v.transientGain > 0.001f) {
                int ti = static_cast<int>(v.transientPos);
                float tFrac = static_cast<float>(v.transientPos - ti);
                const float* tData = PedalSteelAssets::kTransients[v.transientIdx];
                float rawT = tData[ti] + tFrac * (tData[ti + 1] - tData[ti]);
                const float baseCut = v.isPalmHarmonic ? 0.14f : (0.42f + 0.50f * v.velocity);
                const float pickCoeff = std::min(0.99f, baseCut * (44100.0f / fSr));
                v.pickLpState += pickCoeff * (rawT - v.pickLpState);
                transientSig = v.pickLpState * v.transientGain;
                highPickScrape = (rawT - v.pickLpState) * v.transientGain;
                v.transientPos += 44100.0 / sampleRate;
            }

            // Concert-Calibrated De-Chorded 44.1 kHz Stereo Single-Note Multi-Sample Core
            float msL = 0.0f, msR = 0.0f;
            float rawVg8MsL = 0.0f, rawVg8MsR = 0.0f;
            float ensVg8AL = 0.0f, ensVg8AR = 0.0f;
            float ensVg8BL = 0.0f, ensVg8BR = 0.0f;
            float subVg8MsL = 0.0f, subVg8MsR = 0.0f;
            float upVg8MsL = 0.0f, upVg8MsR = 0.0f;
            if (params.sampleBlend > 0.01f) {
                const auto& ms = PedalSteelMultiSamples::kMultiSamples[juce::jlimit(0, PedalSteelMultiSamples::kNumMultiSamples - 1, v.multiSampleIdx)];
                readStereoMultiSampleHermite(v.multiSampleIdx, v.samplePos, msL, msR);
                const float targetSampleHz = v.isPalmHarmonic ? std::min(2650.0f, instFreqHz * 2.0f) : instFreqHz;
                if (v.isPalmHarmonic) {
                    // Node highpass + 2.05 kHz palm-flesh lowpass shapes studio string into warm liquid bell chime!
                    const float nodeHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * std::min(1550.0f, instFreqHz * 1.40f) / sampleRate));
                    const float palmLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2050.0 / sampleRate));
                    v.chimeHpStateL += nodeHpAlpha * (msL - v.chimeHpStateL);
                    v.chimeHpStateR += nodeHpAlpha * (msR - v.chimeHpStateR);
                    const float hpL = msL - v.chimeHpStateL;
                    const float hpR = msR - v.chimeHpStateR;
                    v.chimeLpStateL += palmLpAlpha * (hpL - v.chimeLpStateL);
                    v.chimeLpStateR += palmLpAlpha * (hpR - v.chimeLpStateR);
                    const float palmLift = std::min(1.0f, ageF / 0.022f);
                    msL = v.chimeLpStateL * params.sampleBlend * 1.02f * palmLift;
                    msR = v.chimeLpStateR * params.sampleBlend * 1.02f * palmLift;
                } else {
                    msL *= params.sampleBlend;
                    msR *= params.sampleBlend;
                    rawVg8MsL = msL;
                    rawVg8MsR = msR;
                    if (vg8BowedSmooth > 0.0005f) {
                        // 100% Real Studio String Multi-Tap Readers for Roland VG-8 D62 BowedPad (zero std::sin oscillators!):
                        //   - ensA (-3.8c) & ensB (+3.8c) real bowed-string ensemble
                        //   - subVg8Ms (-12st 0.5x) real pitch-divided cello string
                        //   - upVg8Ms  (+12st 2.0x) real polyphonic pitch-shifted shimmer & pick-rake string
                        const double loopOffsetBase = static_cast<double>(std::max(0, ms.loopStartFrame - ms.startFrame)) + 512.0;
                        const double phaseOffA = static_cast<double>(i) * 137.0;
                        const double phaseOffB = static_cast<double>(i) * 293.0 + 419.0;
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + phaseOffA + v.samplePos * 0.9971, ensVg8AL, ensVg8AR);
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + phaseOffB + v.samplePos * 1.0029, ensVg8BL, ensVg8BR);
                        if (instMidi <= 53.0f) {
                            readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + v.samplePos * 0.5, subVg8MsL, subVg8MsR);
                        }
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + v.samplePos * 2.0, upVg8MsL, upVg8MsR);
                        ensVg8AL  *= params.sampleBlend; ensVg8AR  *= params.sampleBlend;
                        ensVg8BL  *= params.sampleBlend; ensVg8BR  *= params.sampleBlend;
                        subVg8MsL *= params.sampleBlend; subVg8MsR *= params.sampleBlend;
                        upVg8MsL  *= params.sampleBlend; upVg8MsR  *= params.sampleBlend;
                    }
                    if (organRankSmooth > 0.0005f) {
                        const double loopOffsetBase = static_cast<double>(std::max(0, ms.loopStartFrame - ms.startFrame)) + 512.0;
                        float subMsL = 0.0f, subMsR = 0.0f, upMsL = 0.0f, upMsR = 0.0f;
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + v.samplePos * 0.5, subMsL, subMsR);
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + v.samplePos * 2.0, upMsL, upMsR);
                        // Silk 1950 Hz 1-pole POG2/Messina lowpass on the +12st 4' steel rank so it floats like a velvety wooden flute!
                        const float upSilkAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 1950.0 / sampleRate));
                        v.chimeLpStateL += upSilkAlpha * (upMsL - v.chimeLpStateL);
                        v.chimeLpStateR += upSilkAlpha * (upMsR - v.chimeLpStateR);
                        const float upTame = juce::jlimit(0.24f, 1.0f, (1650.0f - targetSampleHz) / 900.0f);
                        msL = msL * (1.0f - 0.12f * organRankSmooth)
                            + (subMsL * 0.46f + v.chimeLpStateL * 0.44f * upTame) * params.sampleBlend * organRankSmooth;
                        msR = msR * (1.0f - 0.12f * organRankSmooth)
                            + (subMsR * 0.46f + v.chimeLpStateR * 0.44f * upTame) * params.sampleBlend * organRankSmooth;
                    } else if (rackBellSmooth > 0.0005f) {
                        const double loopOffsetBase = static_cast<double>(std::max(0, ms.loopStartFrame - ms.startFrame)) + 512.0;
                        float upMsL = 0.0f, upMsR = 0.0f;
                        readStereoMultiSampleHermite(v.multiSampleIdx, loopOffsetBase + v.samplePos * 2.0, upMsL, upMsR);
                        const float glassAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2950.0 / sampleRate));
                        v.chimeLpStateL += glassAlpha * (upMsL - v.chimeLpStateL);
                        v.chimeLpStateR += glassAlpha * (upMsR - v.chimeLpStateR);
                        const float upTame = juce::jlimit(0.22f, 1.0f, (1750.0f - targetSampleHz) / 950.0f);
                        const float crystalBloom = (0.14f + 0.28f * std::exp(-ageF / 0.34f)) * upTame * rackBellSmooth;
                        msL += v.chimeLpStateL * params.sampleBlend * crystalBloom;
                        msR += v.chimeLpStateR * params.sampleBlend * crystalBloom;
                    } else if (jawariSmooth > 0.0005f) {
                        // Parabolic deer-horn Jawari grazing bridge: unilateral string-bridge wrap blooms ~90 ms after pluck
                        // into 0.00-cent harmonic sitar buzz with zero intermodulation between chord strings!
                        const float jawariBloom = 0.38f + 0.62f * (1.0f - std::exp(-ageF / 0.090f));
                        const float strMono = 0.5f * (msL + msR) + harmSum * 0.045f;
                        const float uPos = std::max(0.0f, strMono);
                        const float uNeg = std::min(0.0f, strMono);
                        const float rawGraze = (fastTanh(uPos * (5.6f + 3.8f * jawariBloom)) - uPos * 0.90f)
                                             - 0.22f * (fastTanh((-uNeg) * 3.4f) - (-uNeg) * 0.70f);
                        const float jHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 920.0 / sampleRate));
                        const float jLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 4450.0 / sampleRate));
                        v.chimeHpStateL += jHpAlpha * (rawGraze - v.chimeHpStateL);
                        const float jHp = rawGraze - v.chimeHpStateL;
                        v.chimeLpStateL += jLpAlpha * (jHp - v.chimeLpStateL);
                        organPipeMono += v.chimeLpStateL * 0.56f * jawariSmooth;
                    }
                }

                const double pitchRatio = static_cast<double>(targetSampleHz / std::max(50.0f, ms.baseFreqHz));
                v.samplePos += (PedalSteelMultiSamples::kSampleRate / sampleRate) * pitchRatio;
            }

            if (vg8BowedSmooth > 0.0005f && !v.isPalmHarmonic) {
                // =========================================================================
                // 100% Real-String Processed Roland VG-8 COSM HRM 'D62 BowedPad' + Parallel Jag/Tascam String:
                //   ZERO synthetic std::sin / Chebyshev oscillators — every component is real studio string audio!
                //   1. Parallel Direct Guitar String (warm Alnico thumb-pluck body + >2 kHz electric pick-rake on vel >= 0.76)
                //   2. Real-String POLY SUSTAINER AGC -> Full-Wave Rectified String Ensemble (0c + -3.8c + +3.8c real strings)
                //      + Real -12st Pitch-Divided Cello String + Real +12st Pitch-Shifted Shimmer String
                //   3. Per-String Slow-Gear Bow Swell + Deep-to-Explosive 24 dB/oct ZDF Touch-Filter
                //      (250-540 Hz murky underwater lo-fi pad on soft chords -> 5.0-octave 4+ kHz scream on C1 blasts!)
                // =========================================================================
                const float velNorm = juce::jlimit(0.0f, 1.0f, (v.velocity - 0.42f) * (1.0f / 0.56f));
                const float snapDrive = (v.velocity > 0.76f)
                    ? std::pow(juce::jlimit(0.0f, 1.0f, (v.velocity - 0.76f) * (1.0f / 0.24f)), 1.25f)
                    : 0.0f;

                // 1. Parallel GK-2A / Baritone Direct Guitar String
                const float rawMonoStr = 0.5f * (rawVg8MsL + rawVg8MsR);
                const float ensMonoA   = 0.5f * (ensVg8AL  + ensVg8AR);
                const float ensMonoB   = 0.5f * (ensVg8BL  + ensVg8BR);
                const float subMonoStr = 0.5f * (subVg8MsL + subVg8MsR);
                const float upMonoStr  = 0.5f * (upVg8MsL  + upVg8MsR);

                const float directCutHz = isVg8LeadVoice
                    ? (720.0f + 1350.0f * velNorm * velNorm)
                    : (1780.0f + 1750.0f * velNorm); // Crisp warm Alnico fingerpick guitar clarity floating above the murky BowedPad!
                const float directLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * directCutHz / sampleRate));
                v.chimeLpStateL += directLpAlpha * (rawMonoStr - v.chimeLpStateL);
                // Smooth 24ms bowed Slow-Gear entrance on picked solo notes (prevents accordion 'PEH' stabs on C1 E4-F#4-G#4-B4!)
                const float leadBowIn = isVg8LeadVoice ? (1.0f - 0.84f * std::exp(-ageF / 0.024f)) : 1.0f;
                const float thumbBodyEnv = isVg8LeadVoice
                    ? ((0.10f + 0.18f * std::exp(-ageF / 0.18f)) * leadBowIn)
                    : ((0.32f + 0.54f * std::exp(-ageF / 0.42f)) * (1.0f - 0.58f * snapDrive));

                float bridgeSnap = 0.0f;
                if (snapDrive > 0.001f && ageF < 0.30f) {
                    // Wideband >1.95 kHz electric guitar string pick-rake crunch on chord strums; subtle silky rosin chiff on high-vel leads!
                    const float rakeHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 2450.0 / sampleRate));
                    const float strHpAlpha  = static_cast<float>(1.0 - std::exp(-kTwoPi * 1880.0 / sampleRate));
                    v.chimeHpStateL += rakeHpAlpha * (upMonoStr - v.chimeHpStateL);
                    v.chimeHpStateR += strHpAlpha  * (rawMonoStr - v.chimeHpStateR);
                    const float strHigh = rawMonoStr - v.chimeHpStateR;
                    const float upHigh  = upMonoStr - v.chimeHpStateL;
                    const float rectRake = (std::sqrt(strHigh * strHigh + 1e-6f) - 0.001f) * 3.45f + upHigh * 4.40f;
                    const float pickEdge = (instMidi >= 48.0f) ? (highPickScrape * 3.05f + (transientSig - v.chimeLpStateR) * 0.52f) : 0.0f;
                    v.chimeLpStateR += strHpAlpha * (transientSig - v.chimeLpStateR);
                    const float brightStr = rectRake + pickEdge;
                    const float snapEnv = std::exp(-ageF / 0.092f) * snapDrive;
                    const float snapScale = isVg8LeadVoice ? (0.14f * leadBowIn) : 1.92f;
                    bridgeSnap = (fastTanh(brightStr * 3.75f + 0.48f * brightStr * brightStr) - 0.08f) * snapScale * snapEnv;
                }
                const float vg8Direct = v.chimeLpStateL * thumbBodyEnv + bridgeSnap;

                // 2. Per-String POLY SUSTAINER AGC + 100% Real-String COSM HRM Wave-Reshaping (0c, -5c, +5c, -12st, +12st)
                const float strFloor = std::max({ std::abs(rawMonoStr), 0.75f * std::abs(ensMonoA), 0.75f * std::abs(ensMonoB) });
                const float agcAlpha = static_cast<float>(1.0 - std::exp(-dt / ((strFloor > v.vg8AgcEnv) ? 0.0025 : 0.240)));
                v.vg8AgcEnv += (strFloor - v.vg8AgcEnv) * agcAlpha;
                const float invAgc = 1.0f / std::max(0.016f, v.vg8AgcEnv);

                const float normC = juce::jlimit(-2.8f, 2.8f, rawMonoStr * invAgc);
                const float normA = juce::jlimit(-2.8f, 2.8f, ensMonoA   * invAgc);
                const float normB = juce::jlimit(-2.8f, 2.8f, ensMonoB   * invAgc);

                // Organic per-string COSM wave-shaper symmetry drift (living analog string motion!)
                v.vg8PhaseA += (0.95 + 0.14 * static_cast<double>(i)) * dt;
                if (v.vg8PhaseA >= 1.0) v.vg8PhaseA -= 1.0;
                const float symDrift = 0.18f * static_cast<float>(std::sin(kTwoPi * v.vg8PhaseA));

                // Clean fundamental extractor (1.45 * f0) + Full-Wave Octave Rectifier (|x| -> pure organic H2, H4, H6!)
                const float fundLpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * std::min(3200.0, static_cast<double>(instFreqHz) * 1.45) / sampleRate));
                v.vg8DcR += fundLpAlpha * (normC - v.vg8DcR);
                const float fundS = v.vg8DcR;

                const float hrmDrive = 1.36f + 0.72f * velNorm + 0.48f * snapDrive;
                const float shC = fastTanh(normC * hrmDrive + symDrift);
                const float shA = fastTanh(normA * hrmDrive - symDrift);
                const float shB = fastTanh(normB * hrmDrive + 0.7f * symDrift);

                const float rectC = std::sqrt(shC * shC + 0.008f) - 0.090f;
                const float rectA = std::sqrt(shA * shA + 0.008f) - 0.090f;
                const float rectB = std::sqrt(shB * shB + 0.008f) - 0.090f;

                // Real +12st polyphonic pitch-shifted string shimmer inside the BowedPad HRM core
                const float upTame = juce::jlimit(0.28f, 1.0f, (2650.0f - instFreqHz) / 1450.0f);
                const float upNorm = juce::jlimit(-2.6f, 2.6f, upMonoStr * invAgc);
                const float upShimmer = fastTanh(upNorm * (1.15f + 0.55f * velNorm))
                                      * ((isVg8LeadVoice ? (0.14f + 0.28f * chPress) : (0.20f + 0.28f * velNorm)) + 0.36f * snapDrive) * upTame;

                const float rosinGrip = isVg8LeadVoice ? (1.0f + 0.065f * (0.4f + 0.6f * chPress) * nextRandomSigned()) : 1.0f;
                const float highViolinTrim = (isVg8LeadVoice && instMidi > 62.0f)
                    ? juce::jlimit(0.52f, 1.0f, 1.0f - (instMidi - 62.0f) * 0.042f)
                    : 1.0f;
                const float rawHrmL = (fundS * 0.76f * highViolinTrim + (0.44f * rectC + 0.56f * rectA) * (1.46f + 0.58f * velNorm) + shA * (0.22f + 0.16f * snapDrive) + upShimmer) * rosinGrip;
                const float rawHrmR = (fundS * 0.76f * highViolinTrim + (0.44f * rectC + 0.56f * rectB) * (1.46f + 0.58f * velNorm) + shB * (0.22f + 0.16f * snapDrive) + upShimmer) * rosinGrip;

                const float dcAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 24.0 / sampleRate));
                v.vg8DcL += dcAlpha * (0.5f * (rawHrmL + rawHrmR) - v.vg8DcL);
                const float hrmStrL = (rawHrmL - v.vg8DcL) * 0.74f;
                const float hrmStrR = (rawHrmR - v.vg8DcL) * 0.74f;

                // 3. True Fast-Plunging Hardware Roland VG-8 AD Envelope-Follower Swell & 24 dB/oct ZDF Touch-Filter:
                //    Whips up from 0..72ms ("chk -> WWOOOOAAAHH" ~4000 Hz at +80ms!) and plunges rapidly by +380ms (~1450 Hz)
                //    down into the deep 245-480 Hz murky underwater pad in groove pockets (5x C1 groove contrast!)
                const double bowTau = isVg8LeadVoice
                    ? (0.052 - 0.022 * static_cast<double>(velNorm))
                    : (0.082 - 0.042 * static_cast<double>(snapDrive));
                v.vg8BowEnv += (1.0f - v.vg8BowEnv) * static_cast<float>(1.0 - std::exp(-dt / bowTau));
                const float bowSwell = v.vg8BowEnv * v.vg8BowEnv * (1.5f - 0.5f * v.vg8BowEnv);

                const double touchDecayTau = isVg8LeadVoice ? 0.32 : 0.225;
                v.vg8TouchEnv *= static_cast<float>(std::exp(-dt / touchDecayTau));
                const float riseTau   = isVg8LeadVoice ? 0.028f : (0.036f - 0.011f * snapDrive);
                const float touchRise = 1.0f - std::exp(-ageF / riseTau);
                const float warmTail  = (!isVg8LeadVoice) ? (0.14f * std::exp(-ageF / 0.65f) * touchRise) : 0.0f;
                const float touchAd   = std::min(1.16f, 1.46f * touchRise * v.vg8TouchEnv + warmTail);
                const float fastPeak  = touchAd * touchAd;
                const float touchMod  = isVg8LeadVoice ? (0.65f * touchAd) : (0.46f * touchAd + 0.54f * fastPeak);
                const float touchCurve = isVg8LeadVoice
                    ? std::pow(juce::jlimit(0.0f, 1.0f, (v.velocity - 0.56f) * (1.0f / 0.44f)), 1.65f)
                    : std::pow(juce::jlimit(0.0f, 1.0f, (v.velocity - 0.48f) * (1.0f / 0.52f)), 1.45f);

                // Real -12st pitch-divided cello string (replaces synthetic std::sin vg8SubPhase!)
                float subCello = 0.0f;
                const float subTransDuck = 1.0f - 0.90f * snapDrive * fastPeak;
                const float subVelScale  = 0.54f + 0.46f * velNorm;
                const float subWeight = ((!isVg8LeadVoice && instMidi <= 51.5f)
                    ? (0.36f * subVelScale)
                    : ((isVg8LeadVoice && instMidi <= 52.5f) ? 0.16f : 0.0f)) * subTransDuck;
                if (subWeight > 0.001f) {
                    const float subNorm = juce::jlimit(-2.5f, 2.5f, subMonoStr * invAgc);
                    const float subShaped = fastTanh(subNorm * 1.35f);
                    const float subOct = std::sqrt(subShaped * subShaped + 0.01f) - 0.10f;
                    subCello = (0.76f * subShaped + 0.34f * subOct) * subWeight;
                }

                const float strumBodyClear = 1.0f - 0.54f * snapDrive * fastPeak;
                const float bowInL = hrmStrL * strumBodyClear + subCello;
                const float bowInR = hrmStrR * strumBodyClear + subCello;

                // Solo: open woody 700 Hz cello on A1 (vel~0.68, chPress~0.14) -> singing 1720 Hz violin on C1 (vel~0.91, chPress~0.85)!
                // Chords: deep 240-540 Hz underwater lo-fi pad floor -> explosive 5.9-octave 11.4 kHz AD filter scream on C1 blasts!
                const float octSpan = isVg8LeadVoice
                    ? (0.28f + 1.25f * touchCurve + 0.52f * chPress + 0.16f * barGlideSheen)
                    : (0.20f + 5.90f * touchCurve + 0.36f * chPress + 0.15f * barGlideSheen);
                const float fFloor = isVg8LeadVoice
                    ? juce::jlimit(460.0f, 2950.0f, 215.0f + instFreqHz * (1.62f + 1.75f * velNorm * velNorm + 1.55f * chPress))
                    : juce::jlimit(235.0f, 540.0f, 120.0f + instFreqHz * (0.78f + 0.26f * velNorm));
                const float vibFilterShimmer = isVg8LeadVoice ? (1.0f + 0.16f * barVibratoSin * v.vibratoEnv) : 1.0f;
                const float maxFc = std::min(11600.0f, fSr * 0.45f);
                const float fc = juce::jlimit(85.0f, maxFc, fFloor * std::pow(2.0f, octSpan * touchMod) * vibFilterShimmer);

                const float g = std::tan(static_cast<float>(kPi) * fc / fSr);
                const float k1 = isVg8LeadVoice ? 0.94f : 0.78f;
                const float a1_1 = 1.0f / (1.0f + g * (g + k1));
                const float a2_1 = g * a1_1;
                const float a3_1 = g * a2_1;

                const float v3_1L = bowInL - v.vg8Svf1Ic2L;
                const float v1_1L = a1_1 * v.vg8Svf1Ic1L + a2_1 * v3_1L;
                const float v2_1L = v.vg8Svf1Ic2L + a2_1 * v.vg8Svf1Ic1L + a3_1 * v3_1L;
                v.vg8Svf1Ic1L = 2.0f * v1_1L - v.vg8Svf1Ic1L;
                v.vg8Svf1Ic2L = 2.0f * v2_1L - v.vg8Svf1Ic2L;

                const float v3_1R = bowInR - v.vg8Svf1Ic2R;
                const float v1_1R = a1_1 * v.vg8Svf1Ic1R + a2_1 * v3_1R;
                const float v2_1R = v.vg8Svf1Ic2R + a2_1 * v.vg8Svf1Ic1R + a3_1 * v3_1R;
                v.vg8Svf1Ic1R = 2.0f * v1_1R - v.vg8Svf1Ic1R;
                v.vg8Svf1Ic2R = 2.0f * v2_1R - v.vg8Svf1Ic2R;

                // Chewy COSM inter-stage hex-fuzz saturation when driven hard by velocity strums!
                const float tvfDrive = 1.32f + 1.95f * snapDrive * (0.25f + 0.75f * touchMod);
                const float tvfComp  = 1.0f / (1.08f + 0.62f * snapDrive * (0.25f + 0.75f * touchMod));
                const float lp1L = (fastTanh(v2_1L * tvfDrive + 0.22f * v2_1L * v2_1L) - 0.06f) * tvfComp;
                const float lp1R = (fastTanh(v2_1R * tvfDrive + 0.22f * v2_1R * v2_1R) - 0.06f) * tvfComp;

                // Broad, silky Q=1.05 on single-string cello/violin leads (zero nasal harmonica formant!); chewy Q=2.65 on chord sweeps!
                const float q2 = isVg8LeadVoice ? (0.98f + 0.22f * chPress) : (1.65f + 1.02f * touchCurve);
                const float k2 = 1.0f / q2;
                const float a1_2 = 1.0f / (1.0f + g * (g + k2));
                const float a2_2 = g * a1_2;
                const float a3_2 = g * a2_2;

                const float v3_2L = lp1L - v.vg8Svf2Ic2L;
                const float v1_2L = a1_2 * v.vg8Svf2Ic1L + a2_2 * v3_2L;
                const float v2_2L = v.vg8Svf2Ic2L + a2_2 * v.vg8Svf2Ic1L + a3_2 * v3_2L;
                v.vg8Svf2Ic1L = 2.0f * v1_2L - v.vg8Svf2Ic1L;
                v.vg8Svf2Ic2L = 2.0f * v2_2L - v.vg8Svf2Ic2L;

                const float v3_2R = lp1R - v.vg8Svf2Ic2R;
                const float v1_2R = a1_2 * v.vg8Svf2Ic1R + a2_2 * v3_2R;
                const float v2_2R = v.vg8Svf2Ic2R + a2_2 * v.vg8Svf2Ic1R + a3_2 * v3_2R;
                v.vg8Svf2Ic1R = 2.0f * v1_2R - v.vg8Svf2Ic1R;
                v.vg8Svf2Ic2R = 2.0f * v2_2R - v.vg8Svf2Ic2R;

                const float bpPeak = (isVg8LeadVoice ? 0.06f : 0.48f) * (0.22f * touchCurve + 0.78f * snapDrive) * fastPeak;
                const float hpAir  = isVg8LeadVoice
                    ? (0.14f * chPress * (0.45f + 0.55f * fastPeak))
                    : (0.46f * snapDrive * fastPeak);
                const float tvfOutL = v2_2L * (1.0f - 0.22f * snapDrive * fastPeak) + v1_2L * bpPeak + (lp1L - v2_2L - k2 * v1_2L) * hpAir;
                const float tvfOutR = v2_2R * (1.0f - 0.22f * snapDrive * fastPeak) + v1_2R * bpPeak + (lp1R - v2_2R - k2 * v1_2R) * hpAir;

                const float bowLevel = (isVg8LeadVoice ? 0.56f : (0.38f + 0.18f * touchCurve)) * bowSwell;
                const float vg8TotalL = vg8Direct + tvfOutL * bowLevel;
                const float vg8TotalR = vg8Direct + tvfOutR * bowLevel;

                msL = msL + vg8BowedSmooth * (vg8TotalL - msL);
                msR = msR + vg8BowedSmooth * (vg8TotalR - msR);
                transientSig *= (1.0f - vg8BowedSmooth);
            }

            const float swellShape = (params.autoSwell > 0.3f) ? (v.swellEnv * v.swellEnv) : v.swellEnv;
            // Smooth >=3.4 ms S-curve organic attack floor on Two Star VG-8 Bowed mode so thumb plucks never step-click!
            const float sCurveFloor = (vg8BowedSmooth > 0.0005f && ageF < 0.0034f)
                ? (0.5f * (1.0f - std::cos(static_cast<float>(kPi) * (ageF / 0.0034f))))
                : 1.0f;
            const float vg8LeadGain = (vg8BowedSmooth > 0.0005f && isVg8LeadVoice) ? (1.0f + 1.45f * vg8BowedSmooth) : 1.0f;
            const float envGain = v.ampEnv * swellShape * (1.0f + vg8BowedSmooth * (sCurveFloor - 1.0f)) * vg8LeadGain;
            const float pan = (static_cast<float>(i) - 4.5f) * 0.045f * (1.0f - 0.90f * vg8BowedSmooth);

            // Route pure 44.1 kHz studio multi-sample (+ phase-locked Poly-Octave pipe ranks & stereo VG-8 Bowed HRM) to Studio Bus
            const float sVoiceL = (msL + organPipeMono + vg8BowedL) * envGain * (0.5f - pan) * 1.18f;
            const float sVoiceR = (msR + organPipeMono + vg8BowedR) * envGain * (0.5f + pan) * 1.18f;
            msStringsL += sVoiceL;
            msStringsR += sVoiceR;

            // Route physical harmonics + fingerpick snap + chrome-bar slide whistle to Physical Amp Bus
            const float physHarmBlend = v.isPalmHarmonic
                ? 0.16f
                : ((stStyle == 5 ? 0.055f : (stStyle == 4 ? 0.036f : 0.022f)) + (1.0f - 0.86f * params.sampleBlend) * 0.24f);
            const float pMono = (harmSum * physHarmBlend + transientSig * 0.14f + slideWhistle) * envGain;
            physStringsL += pMono * (0.5f - pan) * 1.85f;
            physStringsR += pMono * (0.5f + pan) * 1.85f;

            v.lastOutput = 0.5f * (sVoiceL + sVoiceR) + pMono;
        }

        // Smooth 24 ms C1 raised-cosine palm-block crossfade tails for any re-triggered strings (100% click-free!)
        for (auto& ct : chokeTails) {
            if (!ct.active || ct.chokeFade <= 0.0001f) continue;

            ct.chokeFade = std::max(0.0f, ct.chokeFade - static_cast<float>(dt / 0.024));
            const float chokeGain = 0.5f * (1.0f - std::cos(static_cast<float>(kPi * ct.chokeFade)));

            if (ct.isPhraseSlice) {
                const auto& ps = PedalSteelMultiSamples::kPhraseSlices[juce::jlimit(0, PedalSteelMultiSamples::kNumPhraseSlices - 1, ct.phraseSliceIdx)];
                const double sliceLen = static_cast<double>(std::max(1, ps.endFrame - ps.startFrame));
                if (ct.samplePos < sliceLen) {
                    const float liveBendSemis = ct.phraseTransposeSemis + ct.pedalCurrentSemis + ct.activePitchWheel * 7.0f;
                    const double pitchRatio = std::pow(2.0, static_cast<double>(liveBendSemis) / 12.0);
                    float sliceL = 0.0f, sliceR = 0.0f;
                    readStereoPhraseSliceHermite(ct.phraseSliceIdx, ct.samplePos, sliceL, sliceR);
                    ct.samplePos += (PedalSteelMultiSamples::kSampleRate / sampleRate) * pitchRatio;
                    phraseBusL += sliceL * ct.ampEnv * chokeGain;
                    phraseBusR += sliceR * ct.ampEnv * chokeGain;
                }
            } else {
                const float pitchWheelSemis = ct.activePitchWheel * 7.0f;
                const float instMidi = ct.barCurrentMidi + ct.pedalCurrentSemis + ct.mpePitchSemis + ct.scoopOffsetSemis + pitchWheelSemis;
                const float instFreqHz = juce::jlimit(40.0f, 4000.0f, midiToHz(instMidi));

                float msL = 0.0f, msR = 0.0f;
                if (params.sampleBlend > 0.01f) {
                    const auto& ms = PedalSteelMultiSamples::kMultiSamples[juce::jlimit(0, PedalSteelMultiSamples::kNumMultiSamples - 1, ct.multiSampleIdx)];
                    readStereoMultiSampleHermite(ct.multiSampleIdx, ct.samplePos, msL, msR);
                    const float targetSampleHz = ct.isPalmHarmonic ? std::min(2650.0f, instFreqHz * 2.0f) : instFreqHz;
                    if (ct.isPalmHarmonic) {
                        const float nodeHpAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * std::min(1800.0f, instFreqHz * 1.45f) / sampleRate));
                        ct.chimeHpStateL += nodeHpAlpha * (msL - ct.chimeHpStateL);
                        ct.chimeHpStateR += nodeHpAlpha * (msR - ct.chimeHpStateR);
                        msL = (msL - ct.chimeHpStateL) * params.sampleBlend * 0.88f;
                        msR = (msR - ct.chimeHpStateR) * params.sampleBlend * 0.88f;
                    } else {
                        msL *= params.sampleBlend;
                        msR *= params.sampleBlend;
                        if (organRankSmooth > 0.0005f) {
                            const double loopOffsetBase = static_cast<double>(std::max(0, ms.loopStartFrame - ms.startFrame)) + 512.0;
                            float subMsL = 0.0f, subMsR = 0.0f, upMsL = 0.0f, upMsR = 0.0f;
                            readStereoMultiSampleHermite(ct.multiSampleIdx, loopOffsetBase + ct.samplePos * 0.5, subMsL, subMsR);
                            readStereoMultiSampleHermite(ct.multiSampleIdx, loopOffsetBase + ct.samplePos * 2.0, upMsL, upMsR);
                            const float upSilkAlpha = static_cast<float>(1.0 - std::exp(-kTwoPi * 1950.0 / sampleRate));
                            ct.chimeHpStateL += upSilkAlpha * (upMsL - ct.chimeHpStateL);
                            ct.chimeHpStateR += upSilkAlpha * (upMsR - ct.chimeHpStateR);
                            const float upTame = juce::jlimit(0.24f, 1.0f, (1650.0f - targetSampleHz) / 900.0f);
                            msL = msL * (1.0f - 0.12f * organRankSmooth)
                                + (subMsL * 0.46f + ct.chimeHpStateL * 0.44f * upTame) * params.sampleBlend * organRankSmooth;
                            msR = msR * (1.0f - 0.12f * organRankSmooth)
                                + (subMsR * 0.46f + ct.chimeHpStateR * 0.44f * upTame) * params.sampleBlend * organRankSmooth;
                        } else if (vg8BowedSmooth > 0.0005f) {
                            const bool isCtLead = (ct.channel >= 3);
                            const double loopOffsetBase = static_cast<double>(std::max(0, ms.loopStartFrame - ms.startFrame)) + 512.0;
                            float ctSubL = 0.0f, ctSubR = 0.0f;
                            if (instMidi <= 52.5f) {
                                readStereoMultiSampleHermite(ct.multiSampleIdx, loopOffsetBase + ct.samplePos * 0.5, ctSubL, ctSubR);
                                ctSubL *= params.sampleBlend;
                                ctSubR *= params.sampleBlend;
                            }
                            const float invAgc = 1.0f / std::max(0.018f, ct.vg8AgcEnv);
                            const float nL = juce::jlimit(-2.6f, 2.6f, msL * invAgc);
                            const float nR = juce::jlimit(-2.6f, 2.6f, msR * invAgc);
                            const float shL = fastTanh(nL * 1.45f);
                            const float shR = fastTanh(nR * 1.45f);
                            const float rectL = std::sqrt(shL * shL + 0.008f) - 0.09f;
                            const float rectR = std::sqrt(shR * shR + 0.008f) - 0.09f;
                            const float subW = (!isCtLead && instMidi <= 51.5f) ? 0.36f : ((isCtLead && instMidi <= 52.5f) ? 0.14f : 0.0f);
                            const float subCello = (subW > 0.001f) ? (fastTanh(0.5f * (ctSubL + ctSubR) * invAgc * 1.3f) * subW) : 0.0f;
                            const float bowInL = (0.55f * shL + 1.15f * rectL) * 0.72f + subCello;
                            const float bowInR = (0.55f * shR + 1.15f * rectR) * 0.72f + subCello;
                            const float touchCurve = std::pow(juce::jlimit(0.0f, 1.0f, (ct.velocity - 0.36f) * (1.0f / 0.64f)), 2.05f);
                            const float fFloor = isCtLead
                                ? juce::jlimit(340.0f, 1650.0f, 185.0f + instFreqHz * 1.65f)
                                : juce::jlimit(245.0f, 660.0f, 130.0f + instFreqHz * 0.98f);
                            const float fc = juce::jlimit(75.0f, std::min(7600.0f, fSr * 0.42f), fFloor * std::pow(2.0f, (0.20f + 4.20f * touchCurve) * ct.vg8TouchEnv));
                            const float g = std::tan(static_cast<float>(kPi) * fc / fSr);
                            const float a1 = 1.0f / (1.0f + g * (g + 0.85f));
                            const float a2 = g * a1, a3 = g * a2;
                            const float v3L = bowInL - ct.vg8Svf2Ic2L;
                            const float v1L = a1 * ct.vg8Svf2Ic1L + a2 * v3L;
                            const float v2L = ct.vg8Svf2Ic2L + a2 * ct.vg8Svf2Ic1L + a3 * v3L;
                            ct.vg8Svf2Ic1L = 2.0f * v1L - ct.vg8Svf2Ic1L;
                            ct.vg8Svf2Ic2L = 2.0f * v2L - ct.vg8Svf2Ic2L;
                            const float v3R = bowInR - ct.vg8Svf2Ic2R;
                            const float v1R = a1 * ct.vg8Svf2Ic1R + a2 * v3R;
                            const float v2R = ct.vg8Svf2Ic2R + a2 * ct.vg8Svf2Ic1R + a3 * v3R;
                            ct.vg8Svf2Ic1R = 2.0f * v1R - ct.vg8Svf2Ic1R;
                            ct.vg8Svf2Ic2R = 2.0f * v2R - ct.vg8Svf2Ic2R;
                            const float bowLvl = (isCtLead ? 0.42f : (0.34f + 0.10f * touchCurve)) * ct.vg8BowEnv * ct.vg8BowEnv;
                            msL = msL + vg8BowedSmooth * ((ct.chimeLpStateL * 0.14f + v2L * bowLvl) - msL);
                            msR = msR + vg8BowedSmooth * ((ct.chimeLpStateR * 0.14f + v2R * bowLvl) - msR);
                        }
                    }
                    const double pitchRatio = static_cast<double>(targetSampleHz / std::max(50.0f, ms.baseFreqHz));
                    ct.samplePos += (PedalSteelMultiSamples::kSampleRate / sampleRate) * pitchRatio;
                }

                float harmTail = 0.0f;
                float organPipeTail = 0.0f;
                const float nyquistLimit = fSr * 0.44f;
                if ((organRankSmooth > 0.0005f || cs80BrassSmooth > 0.0005f) && !ct.isPalmHarmonic) {
                    const float subHz = instFreqHz * 0.5f;
                    if (subHz >= 24.0f) {
                        ct.subHarmPhase += kTwoPi * static_cast<double>(subHz) * dt;
                        if (ct.subHarmPhase >= kTwoPi) ct.subHarmPhase -= kTwoPi;
                        const float subTailSin = static_cast<float>(std::sin(ct.subHarmPhase));
                        organPipeTail += (0.085f * organRankSmooth + 0.072f * cs80BrassSmooth) * subTailSin;
                    }
                }
                for (int h = 1; h <= 4; ++h) {
                    const float fh = instFreqHz * static_cast<float>(h);
                    if (fh >= nyquistLimit) break;
                    double ph = ct.harmPhases[static_cast<size_t>(h - 1)] + kTwoPi * static_cast<double>(fh) * dt;
                    if (ph >= kTwoPi) ph -= kTwoPi;
                    ct.harmPhases[static_cast<size_t>(h - 1)] = ph;
                    const float w = ct.isPalmHarmonic ? ((h == 2) ? 1.15f : ((h >= 3) ? 0.40f : 0.08f))
                                                      : (((h <= 2) ? 0.80f : 0.25f) * (1.0f - vg8BowedSmooth));
                    harmTail += w * static_cast<float>(std::sin(ph));
                    if (organRankSmooth > 0.0005f && !ct.isPalmHarmonic && h == 2) {
                        organPipeTail += 0.045f * organRankSmooth * static_cast<float>(std::sin(ph));
                    }
                    if (cs80BrassSmooth > 0.0005f && !ct.isPalmHarmonic) {
                        const float sawSign = (h & 1) ? 1.0f : -1.0f;
                        organPipeTail += (0.085f / static_cast<float>(h)) * sawSign * cs80BrassSmooth * static_cast<float>(std::sin(ph));
                    }
                }

                const float swellShape = (params.autoSwell > 0.3f) ? (ct.swellEnv * ct.swellEnv) : ct.swellEnv;
                const float ctVg8Lead = (vg8BowedSmooth > 0.0005f && ct.channel >= 3) ? (1.0f + 1.45f * vg8BowedSmooth) : 1.0f;
                const float envGain = ct.ampEnv * swellShape * chokeGain * ctVg8Lead;
                const float pan = (static_cast<float>(ct.stringIndex) - 4.5f) * 0.045f * (1.0f - 0.65f * vg8BowedSmooth);

                msStringsL += (msL + organPipeTail) * envGain * (0.5f - pan) * 1.18f;
                msStringsR += (msR + organPipeTail) * envGain * (0.5f + pan) * 1.18f;

                const float physHarmBlend = ct.isPalmHarmonic
                    ? 0.16f
                    : ((stStyle == 5 ? 0.055f : (stStyle == 4 ? 0.036f : 0.022f)) + (1.0f - 0.86f * params.sampleBlend) * 0.24f);
                const float pMono = (harmTail * physHarmBlend) * envGain;
                physStringsL += pMono * (0.5f - pan) * 1.85f;
                physStringsR += pMono * (0.5f + pan) * 1.85f;
            }

            if (ct.chokeFade <= 0.0001f) {
                ct.active = false;
            }
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

        if (stStyle == 1 || stStyle == 2 || stStyle == 4 || stStyle == 5) {
            const float symAmt = (stStyle == 5) ? 0.24f : ((stStyle == 2) ? 0.20f : ((stStyle == 4) ? 0.12f : 0.10f));
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

        float stuTotalL = studioMidClarifierFilter.processL(stuSingleL + stuPhraseL);
        float stuTotalR = studioMidClarifierFilter.processR(stuSingleR + stuPhraseR);
        stuTotalL = studioDeltaCoilFilter.processL(stuTotalL);
        stuTotalR = studioDeltaCoilFilter.processR(stuTotalR);
        stuTotalL = studioDeltaWarmthFilter.processL(stuTotalL);
        stuTotalR = studioDeltaWarmthFilter.processR(stuTotalR);
        stuTotalL = studioDeltaTrebleFilter.processL(stuTotalL);
        stuTotalR = studioDeltaTrebleFilter.processR(stuTotalR);

        // Jumped-channel '57 Fender 5E3 Tweed Deluxe 6V6 push + midrange bark + cathode sag + Jensen P12R Alnico cone!
        const float extraDrive = std::max(0.0f, params.ampDrive - kBaseDrive[stStyle]);
        if (extraDrive > 0.01f) {
            float preL = studioTweedBarkFilter.processL(stuTotalL);
            float preR = studioTweedBarkFilter.processR(stuTotalR);

            const float inPeak = std::max(std::abs(preL), std::abs(preR));
            const float sagAlpha = std::min(0.5f, ((inPeak > tweedSagEnv) ? 0.010f : 0.00030f) * (44100.0f / fSr));
            tweedSagEnv += sagAlpha * (inPeak - tweedSagEnv);
            const float sagComp = 1.0f / (1.0f + 0.48f * extraDrive * tweedSagEnv);

            const float g = (1.0f + 4.60f * extraDrive) * sagComp;
            const float asym = 0.36f * extraDrive;
            const float makeup = 1.0f / (1.0f + 2.85f * extraDrive);
            float drvL = (fastTanh(preL * g + asym * preL * preL) - fastTanh(asym * preL * preL) * 0.18f) * makeup;
            float drvR = (fastTanh(preR * g + asym * preR * preR) - fastTanh(asym * preR * preR) * 0.18f) * makeup;

            // Blend through 4.95 kHz Jensen P12R Alnico cone roll-off so pushed 6V6 breakup snarls warmly with zero fizz!
            const float cabBlend = juce::jlimit(0.0f, 0.86f, extraDrive * 2.2f);
            const float cabL = studioTweedCabFilter.processL(drvL);
            const float cabR = studioTweedCabFilter.processR(drvR);
            stuTotalL = drvL * (1.0f - cabBlend) + cabL * cabBlend;
            stuTotalR = drvR * (1.0f - cabBlend) + cabR * cabBlend;
        }
        stuTotalL = dcBlockerStudio.processL(stuTotalL);
        stuTotalR = dcBlockerStudio.processR(stuTotalR);

        //======================================================================
        // Master Character Circuit + Stereo Tape Delay + Dattorro Figure-8 Plate
        //======================================================================
        float dryCombinedL = physL + stuTotalL;
        float dryCombinedR = physR + stuTotalR;
        float extraRevL = 0.0f, extraRevR = 0.0f;
        const float pedalActivity = totalPedalLoad + smoothLKL + smoothLKR + maxAftertouch
                                  + juce::jlimit(0.0f, 0.35f, maxSlideSpeed * 0.035f);
        modernChar.processSample(sampleRate, params.fxCharacter, params.hostBpm, params.tempoSync, pedalActivity,
                                 dryCombinedL, dryCombinedR, extraRevL, extraRevR);

        float dlyL = 0.0f, dlyR = 0.0f;
        tapeDelay.processSample(sampleRate, dryCombinedL + extraRevL * 0.42f, dryCombinedR + extraRevR * 0.42f,
                                effectiveDelayMs, params.delayMix, maxSlideSpeed, dlyL, dlyR);

        // Chromatic single-note releases & physical strings feed full plate reverb; full studio phrases only add delta reverb above baseline!
        const float singleWet = juce::jlimit(0.0f, 1.0f, params.reverbMix);
        const float phraseExtraWet = juce::jlimit(0.0f, 1.0f, std::max(0.0f, params.reverbMix - kBaseRevMix[stStyle]) * 1.4f);
        const float slideReverbDuck = 1.0f / (1.0f + 0.05f * maxSlideSpeed);

        const float revPureL = ((physL + stuSingleL * 0.72f) * singleWet + stuPhraseL * phraseExtraWet + dlyL * 0.42f * singleWet) * slideReverbDuck;
        const float revPureR = ((physR + stuSingleR * 0.72f) * singleWet + stuPhraseR * phraseExtraWet + dlyR * 0.42f * singleWet) * slideReverbDuck;
        const float revCharL = (dryCombinedL * 0.76f * singleWet + extraRevL + dlyL * 0.44f * singleWet) * slideReverbDuck;
        const float revCharR = (dryCombinedR * 0.76f * singleWet + extraRevR + dlyR * 0.44f * singleWet) * slideReverbDuck;
        const float charMix  = modernChar.getWetMix();
        const float revSendL = revPureL + (revCharL - revPureL) * charMix;
        const float revSendR = revPureR + (revCharR - revPureR) * charMix;

        float revL = 0.0f, revR = 0.0f;
        reverb.processSample(revSendL, revSendR, params.reverbDecay, params.stereoWidth, revL, revR);

        const int activeCharMode = modernChar.getCurrentMode();
        const float dryExtraGain = (activeCharMode == 2 || activeCharMode == 3 || activeCharMode >= 8)
            ? 0.0f
            : ((activeCharMode == 7) ? 0.06f : 0.22f);
        float finalL = (dryCombinedL + extraRevL * dryExtraGain + dlyL + revL * 1.25f) * outGain;
        float finalR = (dryCombinedR + extraRevR * dryExtraGain + dlyL + revR * 1.25f) * outGain;

        // Mid/Side Stereo Panorama Width Control (1.0 = natural studio stereo, 0.0 = mono, 1.5 = wide)
        const float mid  = 0.5f * (finalL + finalR);
        const float side = 0.5f * (finalL - finalR) * juce::jlimit(0.0f, 1.5f, params.stereoWidth);
        finalL = mid + side;
        finalR = mid - side;

        if (outR) {
            outL[s] = juce::jlimit(-0.98f, 0.98f, finalL);
            outR[s] = juce::jlimit(-0.98f, 0.98f, finalR);
        } else {
            outL[s] = juce::jlimit(-0.98f, 0.98f, mid);
        }
    }
}
