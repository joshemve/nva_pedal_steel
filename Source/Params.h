#pragma once
#include <JuceHeader.h>

namespace ParamIDs {
    inline constexpr const char* presetStyle   = "presetStyle";
    inline constexpr const char* playMode      = "playMode";
    inline constexpr const char* pedalA        = "pedalA";
    inline constexpr const char* pedalB        = "pedalB";
    inline constexpr const char* pedalC        = "pedalC";
    inline constexpr const char* leverLKL      = "leverLKL";
    inline constexpr const char* leverLKR      = "leverLKR";
    inline constexpr const char* volumePedal   = "volumePedal";
    inline constexpr const char* scoopDepth    = "scoopDepth";
    inline constexpr const char* pedalSpeed    = "pedalSpeed";
    inline constexpr const char* barGlide      = "barGlide";
    inline constexpr const char* barSlideNoise = "barSlideNoise";
    inline constexpr const char* vibratoDepth  = "vibratoDepth";
    inline constexpr const char* vibratoRate   = "vibratoRate";
    inline constexpr const char* autoSwell     = "autoSwell";
    inline constexpr const char* swellTime     = "swellTime";
    inline constexpr const char* sampleBlend   = "sampleBlend";
    inline constexpr const char* pickSnap      = "pickSnap";
    inline constexpr const char* pickupPos     = "pickupPos";
    inline constexpr const char* coilPeakFreq  = "coilPeakFreq";
    inline constexpr const char* coilResonance = "coilResonance";
    inline constexpr const char* sustainTime   = "sustainTime";
    inline constexpr const char* cabinetDrop   = "cabinetDrop";
    inline constexpr const char* stereoWidth   = "stereoWidth";
    inline constexpr const char* ampDrive      = "ampDrive";
    inline constexpr const char* toneWarmth    = "toneWarmth";
    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* tempoSync     = "tempoSync";
    inline constexpr const char* reverbMix     = "reverbMix";
    inline constexpr const char* reverbDecay   = "reverbDecay";
    inline constexpr const char* fxCharacter   = "fxCharacter";
    inline constexpr const char* outputLevel   = "outputLevel";
}

struct PedalSteelParams {
    int presetStyle     = 0;     // 0: Sweet Georgia E9, 1: Hedonist Bootgaze, 2: Mikey's Grit Tweed, 3: Montana Swell, 4: Western Swing C6th, 5: Baritone Canyon B6
    int playMode        = 0;     // 0: Smart Auto-Pedal E9, 1: Crying Lead Gliss, 2: Lush Swell Pad, 3: Manual Copedent, 4: Studio Phrase Licks, 5: Palm Chime Harmonics
    int fxCharacter     = 0;     // 0: Studio Standard, 1..16: Cassette..Poly-Octave Organ, 17: Two Star VG-8 Bowed, 18: Revolver Reverse Tape, 19: Mohan Veena Jawari, 20: Neo-Soul CE-1 Syrup, 21: Binson Echorec Drum, 22: CS-80 Synth-Steel
    float pedalA        = 0.0f;  // 0..1 (+2 semitones on strings 5 & 10)
    float pedalB        = 0.0f;  // 0..1 (+1 semitone on strings 3 & 6)
    float pedalC        = 0.0f;  // 0..1 (+2 semitones on strings 4 & 5)
    float leverLKL      = 0.0f;  // 0..1 (+1 semitone on strings 4 & 8, E -> F)
    float leverLKR      = 0.0f;  // 0..1 (-1 semitone on strings 4 & 8, E -> D#)
    float volumePedal   = 0.92f; // 0..1 Goodrich optical volume pedal
    float scoopDepth    = 0.65f; // 0..1 Auto A/B pedal scoop depth on attacks/oblique moves
    float pedalSpeed    = 145.0f;// ms (40..350ms) mechanical bellcrank S-curve pull time
    float barGlide      = 95.0f; // ms (10..500ms) chrome tonebar portamento time
    float barSlideNoise = 0.34f; // 0..1 chrome tonebar winding friction & harmonic whistle on multi-fret slides
    float vibratoDepth  = 0.42f; // 0..1 delayed wrist-roll tonebar vibrato
    float vibratoRate   = 5.15f; // Hz (3.0..7.5Hz)
    float autoSwell     = 0.22f; // 0..1 automatic Goodrich volume-pedal swell bloom
    float swellTime     = 190.0f;// ms (30..800ms)
    float sampleBlend   = 0.88f; // 0..1 blend of 44.1 kHz Stereo Studio Multi-Samples + Physical E9 core
    float pickSnap      = 0.58f; // 0..1 fingerpick/thumbpick transient + tonebar attack bite
    float pickupPos     = 2.25f; // inches from bridge (1.2..4.0")
    float coilPeakFreq  = 2120.0f;// Hz (1400..3200Hz) high-inductance pickup LC resonance peak
    float coilResonance = 0.68f; // 0..1 LC tank Q / vocal sing
    float sustainTime   = 6.5f;  // seconds (1.5..12.0s)
    float cabinetDrop   = 0.45f; // 0..1 sympathetic open-string resonance & mechanical cabinet drop
    float stereoWidth   = 1.00f; // 0..1.5 stereo panorama width (0 = mono combo, 1.0 = studio stereo, 1.5 = wide)
    float ampDrive      = 0.18f; // 0..1 Twin Reverb clean to Tweed Deluxe grit
    float toneWarmth    = 0.56f; // 0..1 15" JBL / Black Widow speaker warmth
    float delayMix      = 0.00f; // 0..1 warm analog BBD / Echoplex stereo tape delay mix
    float delayTime     = 340.0f;// ms (40..900ms, or synced beat division when tempoSync is true)
    bool  tempoSync     = false; // Locks delayTime & auto-swell pocket to DAW Host BPM
    double hostBpm      = 120.0; // Live DAW Host BPM (fallback 120 BPM in standalone)
    float reverbMix     = 0.34f; // 0..1 stereo spring + figure-8 plate reverb mix
    float reverbDecay   = 0.58f; // 0..1 reverb decay time & stereo width
    float outputLevel   = 0.0f;  // dB (-24..+12dB)
};
