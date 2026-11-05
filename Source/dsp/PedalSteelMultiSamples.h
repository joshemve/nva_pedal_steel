#pragma once
#include <cstdint>

namespace PedalSteelMultiSamples {

static constexpr int kSampleRate = 44100;
static constexpr int kFramesPerStyle = 509355;
static constexpr int kNumStyles = 4;
static constexpr int kNumMultiSamples = 19;
static constexpr int kNumPhraseSlices = 14;

struct MultiSample {
    int style;
    int midiNote;
    float baseFreqHz;
    int startFrame;
    int loopStartFrame;
    int loopEndFrame;
    int xfadeFrames;
    float normGain;
};

struct PhraseSlice {
    int style;
    int phraseIndex;
    int triggerMidiNote;
    int startFrame;
    int endFrame;
    float baseFreqHz;
};

extern const uint64_t kStudioStereoPacked[];

inline const int16_t* getStyleStereoData(int style) {
    const int s = (style < 0) ? 0 : ((style >= kNumStyles) ? (kNumStyles - 1) : style);
    return reinterpret_cast<const int16_t*>(kStudioStereoPacked) + static_cast<size_t>(s) * static_cast<size_t>(kFramesPerStyle) * 2;
}

static constexpr MultiSample kMultiSamples[kNumMultiSamples] = {
    { 0, 68, 415.30f, 662, 21191, 81624, 1274, 2.4000f }, // SG_GS4
    { 0, 66, 369.99f, 95476, 102337, 123467, 1430, 1.8335f }, // SG_FS4
    { 0, 64, 329.63f, 125685, 143341, 224873, 1605, 2.4000f }, // SG_E4
    { 0, 71, 493.88f, 254678, 261111, 284927, 1072, 1.9068f }, // SG_B4
    { 1, 69, 440.00f, 662, 6148, 22953, 1203, 1.1385f }, // HB_A4_Pick
    { 1, 71, 493.88f, 24696, 38809, 82013, 1072, 2.0194f }, // HB_B4
    { 1, 74, 587.33f, 87098, 91724, 109336, 901, 1.2631f }, // HB_D5
    { 1, 69, 440.00f, 114660, 134463, 213868, 1203, 2.4000f }, // HB_A4_Long
    { 1, 60, 261.63f, 236376, 240765, 257551, 2023, 0.7730f }, // HB_C4
    { 1, 62, 293.66f, 259308, 268084, 292011, 1802, 1.8750f }, // HB_D4
    { 1, 67, 392.00f, 295029, 303449, 344820, 1350, 2.4000f }, // HB_G4
    { 1, 76, 659.26f, 441441, 446281, 470138, 803, 2.4000f }, // HB_E5
    { 1, 79, 783.99f, 478485, 482909, 503647, 675, 2.4000f }, // HB_G5
    { 2, 81, 880.00f, 662, 4390, 22076, 601, 2.4000f }, // GV_A5
    { 2, 74, 587.33f, 108486, 121252, 189662, 901, 2.0186f }, // GV_D5
    { 2, 78, 739.99f, 351918, 361647, 412321, 715, 1.0721f }, // GV_FS5
    { 3, 67, 392.00f, 8820, 37436, 94871, 1350, 1.2340f }, // MT_G4
    { 3, 74, 587.33f, 176400, 205040, 266826, 901, 1.5133f }, // MT_D5
    { 3, 76, 659.26f, 343980, 372617, 434381, 803, 1.4781f }, // MT_E5
};

static constexpr PhraseSlice kPhraseSlices[kNumPhraseSlices] = {
    { 0, 0, 66, 0, 509355, 415.30f }, // PH_SG_0_FS4_GS4_Pull
    { 0, 1, 64, 94815, 509355, 329.63f }, // PH_SG_1_FS4_E4_Slide
    { 0, 2, 71, 250488, 509355, 493.88f }, // PH_SG_2_B4_Oblique_Dyad
    { 0, 3, 61, 437472, 509355, 329.63f }, // PH_SG_3_E4_FS4_Pickup
    { 1, 0, 69, 0, 509355, 493.88f }, // PH_HB_0_A4_B4_Pull
    { 1, 1, 74, 85995, 509355, 440.00f }, // PH_HB_1_D5_A4_Gliss
    { 1, 2, 60, 234612, 509355, 392.00f }, // PH_HB_2_C4_D4_G4_Chime
    { 1, 3, 71, 350154, 509355, 493.88f }, // PH_HB_3_A4_B4_E5_G5
    { 2, 0, 81, 0, 509355, 587.33f }, // PH_GV_0_A5_DMaj_BarGliss1
    { 2, 1, 78, 215208, 509355, 587.33f }, // PH_GV_1_FS5_DMaj_BarGliss2
    { 2, 2, 74, 349272, 509355, 739.99f }, // PH_GV_2_FS5_GritLead_Tail
    { 3, 0, 67, 0, 509355, 392.00f }, // PH_MT_0_GMaj_Swell
    { 3, 1, 74, 167580, 509355, 587.33f }, // PH_MT_1_DMaj_Swell
    { 3, 2, 76, 335160, 509355, 659.26f }, // PH_MT_2_Em_CMaj_Swell
};

} // namespace PedalSteelMultiSamples
