#pragma once
#include <cstdint>

// Auto-generated 44.1 kHz 16-Bit Stereo Studio Pedal-Steel Sample & Phrase Bank
namespace PedalSteelMultiSamples {

static constexpr double kSampleRate = 44100.0;
static constexpr int kTotalPhraseFrames = 3338545;
static constexpr int kTotalMultiSampleFrames = 1192464;
static constexpr int kNumMultiSamples = 26;
static constexpr int kNumPhraseSlices = 20;

struct MultiSampleZone {
    int style;
    int midiNote;
    int velLayer;
    float baseFreqHz;
    int startFrame;
    int loopStartFrame;
    int loopEndFrame;
    int endFrame;
    int xfadeFrames;
    float normGain;
    const char* name;
};

struct PhraseSlice {
    int style;
    int slotIndex;
    int keyswitchMidi;
    int rootMidiNote;
    float referenceBpm;
    int startFrame;
    int endFrame;
    const char* name;
};

const int16_t* getStyleStereoData(int styleIndex) noexcept;
int getStyleStereoFrames(int styleIndex) noexcept;
const int16_t* getSingleNoteStereoData() noexcept;

static constexpr MultiSampleZone kMultiSamples[26] = {
    { 0, 61, 0, 277.015f, 0, 7123, 80246, 82467, 2547, 1.2467f, "SG_CS4" },
    { 0, 64, 0, 329.514f, 82467, 89524, 164496, 166698, 2141, 1.4083f, "SG_E4" },
    { 0, 66, 0, 370.252f, 166698, 172831, 194051, 196245, 1906, 1.0000f, "SG_FS4" },
    { 0, 68, 0, 416.053f, 196245, 214717, 269427, 271656, 1696, 1.0000f, "SG_GS4" },
    { 0, 68, 1, 415.404f, 271656, 278704, 301629, 303849, 1699, 1.0000f, "SG_GS4_Accent" },
    { 0, 71, 0, 492.556f, 303849, 310049, 353208, 355446, 1433, 1.3834f, "SG_B4" },
    { 0, 76, 0, 665.934f, 355446, 361637, 383689, 385875, 1060, 1.0000f, "SG_E5" },
    { 1, 57, 0, 220.225f, 385875, 390320, 403513, 405720, 2222, 1.0000f, "HB_A3" },
    { 1, 60, 0, 262.608f, 405720, 411863, 429464, 431739, 2687, 1.0000f, "HB_C4" },
    { 1, 67, 0, 389.513f, 431739, 438808, 465288, 467460, 1811, 1.1211f, "HB_G4" },
    { 1, 69, 0, 440.507f, 467460, 474505, 494852, 497007, 1602, 1.0000f, "HB_A4" },
    { 1, 71, 0, 494.677f, 497007, 502724, 525672, 527877, 1426, 1.0423f, "HB_B4" },
    { 1, 74, 0, 591.331f, 527877, 533161, 554340, 556542, 1193, 1.0000f, "HB_D5" },
    { 1, 79, 0, 786.169f, 556542, 563184, 595337, 597555, 898, 1.0000f, "HB_G5" },
    { 2, 71, 0, 493.456f, 597555, 602823, 623990, 626220, 1430, 1.1700f, "GV_B4" },
    { 2, 74, 0, 587.394f, 626220, 633702, 676047, 678258, 1201, 1.4282f, "GV_D5" },
    { 2, 78, 1, 739.733f, 678258, 684427, 710003, 712215, 954, 1.0242f, "GV_FS5_Accent" },
    { 2, 78, 0, 740.118f, 712215, 719259, 752762, 754992, 953, 1.0444f, "GV_FS5" },
    { 2, 81, 1, 877.921f, 754992, 761181, 800836, 803061, 804, 1.0000f, "GV_A5_Grit" },
    { 2, 81, 0, 877.520f, 803061, 809223, 850706, 852894, 804, 1.0000f, "GV_A5" },
    { 3, 55, 0, 196.415f, 852894, 861817, 908063, 910224, 3592, 1.0000f, "MT_G3" },
    { 3, 62, 0, 294.633f, 910224, 919053, 965358, 967554, 2395, 1.0000f, "MT_D4" },
    { 3, 64, 0, 329.857f, 967554, 976362, 1022739, 1024884, 2139, 1.0000f, "MT_E4" },
    { 3, 67, 0, 392.906f, 1024884, 1033713, 1077823, 1080009, 1796, 1.0000f, "MT_G4" },
    { 3, 72, 0, 524.943f, 1080009, 1088855, 1135154, 1137339, 1344, 1.0000f, "MT_C5" },
    { 3, 76, 0, 663.575f, 1137339, 1146137, 1190240, 1192464, 1063, 1.0000f, "MT_E5" },
};

static constexpr PhraseSlice kPhraseSlices[20] = {
    { 0, 0, 36, 68, 83.0f, 0, 125244, "PH_SG_1_B_Pedal_Cry" },
    { 0, 1, 38, 64, 83.0f, 125244, 252252, "PH_SG_2_Weeping_Slide" },
    { 0, 2, 40, 71, 83.0f, 252252, 506268, "PH_SG_3_Harmonic_Chime" },
    { 0, 3, 41, 64, 83.0f, 506268, 1016505, "PH_SG_4_Verse_Turnaround" },
    { 0, 4, 43, 66, 83.0f, 0, 1016505, "PH_SG_Full_Performance" },
    { 1, 0, 36, 69, 90.0f, 0, 85995, "PH_HB_1_Tube_Pedal_Pull" },
    { 1, 1, 38, 74, 90.0f, 85995, 231525, "PH_HB_2_Soaring_Gliss" },
    { 1, 2, 40, 60, 90.0f, 231525, 504945, "PH_HB_3_Octave_Arpeggio" },
    { 1, 3, 41, 71, 90.0f, 560070, 934920, "PH_HB_4_Shimmer_Climax" },
    { 1, 4, 43, 69, 90.0f, 0, 934920, "PH_HB_Full_Performance" },
    { 2, 0, 36, 78, 120.0f, 0, 215208, "PH_GV_1_Tweed_Crying_Lead" },
    { 2, 1, 38, 74, 120.0f, 215208, 349272, "PH_GV_2_Open_D_Resonance" },
    { 2, 2, 40, 78, 120.0f, 349272, 526995, "PH_GV_3_DoubleStop_Grit" },
    { 2, 3, 41, 81, 120.0f, 526995, 702072, "PH_GV_4_High_Bar_Finale" },
    { 2, 4, 43, 81, 120.0f, 0, 702072, "PH_GV_Full_Performance" },
    { 3, 0, 36, 67, 126.0f, 0, 168021, "PH_MT_1_GMaj_Swell" },
    { 3, 1, 38, 74, 126.0f, 168021, 336042, "PH_MT_2_DMaj_Swell" },
    { 3, 2, 40, 76, 126.0f, 336042, 504063, "PH_MT_3_Em_Swell" },
    { 3, 3, 41, 72, 126.0f, 504063, 669438, "PH_MT_4_CMaj_Swell" },
    { 3, 4, 43, 67, 126.0f, 0, 669438, "PH_MT_Full_Performance" },
};

} // namespace PedalSteelMultiSamples
