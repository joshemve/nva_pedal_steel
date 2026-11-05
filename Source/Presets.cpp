#include "Presets.h"

const std::vector<PedalSteelPreset>& getPedalSteelPresets() {
    static const std::vector<PedalSteelPreset> kPresets = {
        {
            "01 - Sweet Georgia E9 (Flyover Country)",
            "Reference Match",
            "Intimate, close-miked modern country E9th pedal steel matched to Flyover Country (83 BPM, E Maj). Sweet 2.12 kHz Emmons single-coil vocal sing, fingerpick snap, oblique B-pedal & A-pedal suspensions, and warm studio plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0; // Smart Auto-Pedal E9
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.72f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 92.0f;
                p.vibratoDepth  = 0.38f;
                p.vibratoRate   = 5.20f;
                p.autoSwell     = 0.14f;
                p.swellTime     = 120.0f;
                p.sampleBlend   = 0.76f;
                p.pickSnap      = 0.64f;
                p.pickupPos     = 2.15f;
                p.coilPeakFreq  = 2120.0f;
                p.coilResonance = 0.74f;
                p.sustainTime   = 6.8f;
                p.cabinetDrop   = 0.45f;
                p.ampDrive      = 0.14f;
                p.toneWarmth    = 0.52f;
                p.reverbMix     = 0.30f;
                p.reverbDecay   = 0.54f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "02 - Hedonist Bootgaze (Indie Americana)",
            "Reference Match",
            "Dreamy, octave-blooming indie Americana pedal steel matched to Bootgaze Hedonist (90 BPM, G Maj). Warm thumbpick attack, rich 2nd-harmonic tube bloom, fluid tonebar glides, and wide atmospheric reverb.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1;
                p.playMode      = 0; // Smart Auto-Pedal E9
                p.volumePedal   = 0.92f;
                p.scoopDepth    = 0.68f;
                p.pedalSpeed    = 175.0f;
                p.barGlide      = 135.0f;
                p.vibratoDepth  = 0.46f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.32f;
                p.swellTime     = 210.0f;
                p.sampleBlend   = 0.78f;
                p.pickSnap      = 0.42f;
                p.pickupPos     = 2.55f;
                p.coilPeakFreq  = 1880.0f;
                p.coilResonance = 0.62f;
                p.sustainTime   = 7.8f;
                p.cabinetDrop   = 0.50f;
                p.ampDrive      = 0.22f;
                p.toneWarmth    = 0.68f;
                p.reverbMix     = 0.42f;
                p.reverbDecay   = 0.68f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "03 - Mikey's Grit Vibey (Tweed Breakup)",
            "Reference Match",
            "High-singing, weeping D-major pedal steel through a pushed Tweed Deluxe amp matched to Mikey's Country Mojo (120 BPM, D Maj). Upper-neck vocal chime, crying downward bar slides, and wide stereo spring/plate space.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2;
                p.playMode      = 1; // Crying Lead Gliss
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.78f;
                p.pedalSpeed    = 135.0f;
                p.barGlide      = 115.0f;
                p.vibratoDepth  = 0.45f;
                p.vibratoRate   = 5.25f;
                p.autoSwell     = 0.42f;
                p.swellTime     = 190.0f;
                p.sampleBlend   = 0.74f;
                p.pickSnap      = 0.40f;
                p.pickupPos     = 2.65f;
                p.coilPeakFreq  = 1960.0f;
                p.coilResonance = 0.70f;
                p.sustainTime   = 8.5f;
                p.cabinetDrop   = 0.42f;
                p.ampDrive      = 0.42f;
                p.toneWarmth    = 0.66f;
                p.reverbMix     = 0.68f;
                p.reverbDecay   = 0.86f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "04 - Montana Lush Swell (Goodrich Pad)",
            "Reference Match",
            "Velvet Goodrich volume-pedal chord swells matched to Mikey's Montana (126 BPM, G Maj). Zero pick click, creamy 15-inch speaker roll-off, and oblique A+B pedal chord suspensions that bloom like a warm string section.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3;
                p.playMode      = 2; // Lush Swell Pad
                p.volumePedal   = 0.90f;
                p.scoopDepth    = 0.64f;
                p.pedalSpeed    = 210.0f;
                p.barGlide      = 165.0f;
                p.vibratoDepth  = 0.34f;
                p.vibratoRate   = 4.60f;
                p.autoSwell     = 0.88f;
                p.swellTime     = 340.0f;
                p.sampleBlend   = 0.80f;
                p.pickSnap      = 0.15f;
                p.pickupPos     = 2.95f;
                p.coilPeakFreq  = 1680.0f;
                p.coilResonance = 0.52f;
                p.sustainTime   = 9.2f;
                p.cabinetDrop   = 0.55f;
                p.ampDrive      = 0.16f;
                p.toneWarmth    = 0.84f;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.76f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "05 - Nashville 400 Crying Lead",
            "Studio Lead",
            "Classic 1970s push-pull E9th lead voice through a clean Peavey Nashville 400 15-inch Black Widow combo. Fast bellcrank A/B pulls and singing 5th-harmonic cry.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1;
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.82f;
                p.pedalSpeed    = 125.0f;
                p.barGlide      = 80.0f;
                p.vibratoDepth  = 0.48f;
                p.vibratoRate   = 5.35f;
                p.autoSwell     = 0.18f;
                p.swellTime     = 130.0f;
                p.sampleBlend   = 0.72f;
                p.pickSnap      = 0.68f;
                p.pickupPos     = 2.05f;
                p.coilPeakFreq  = 2220.0f;
                p.coilResonance = 0.78f;
                p.sustainTime   = 7.2f;
                p.cabinetDrop   = 0.48f;
                p.ampDrive      = 0.18f;
                p.toneWarmth    = 0.48f;
                p.reverbMix     = 0.34f;
                p.reverbDecay   = 0.58f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "06 - Bakersfield Bright Twang",
            "Vintage Country",
            "Snappy close-bridge fingerpicking with Fender Twin Reverb sparkle, fast 105ms pedal pulls, and vintage spring reverb bounce.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.volumePedal   = 0.98f;
                p.scoopDepth    = 0.60f;
                p.pedalSpeed    = 105.0f;
                p.barGlide      = 65.0f;
                p.vibratoDepth  = 0.32f;
                p.vibratoRate   = 5.60f;
                p.autoSwell     = 0.05f;
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.68f;
                p.pickSnap      = 0.82f;
                p.pickupPos     = 1.75f;
                p.coilPeakFreq  = 2420.0f;
                p.coilResonance = 0.80f;
                p.sustainTime   = 5.8f;
                p.cabinetDrop   = 0.40f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.36f;
                p.reverbMix     = 0.28f;
                p.reverbDecay   = 0.46f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "07 - Slow-Gear Dream Steel (Ambient)",
            "Indie / Ambient",
            "Hands-free optical volume-pedal swells with deep Bootgaze octave warmth and infinite cathedral shimmer.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1;
                p.playMode      = 2;
                p.volumePedal   = 0.94f;
                p.scoopDepth    = 0.74f;
                p.pedalSpeed    = 230.0f;
                p.barGlide      = 190.0f;
                p.vibratoDepth  = 0.52f;
                p.vibratoRate   = 4.70f;
                p.autoSwell     = 0.92f;
                p.swellTime     = 420.0f;
                p.sampleBlend   = 0.82f;
                p.pickSnap      = 0.22f;
                p.pickupPos     = 2.75f;
                p.coilPeakFreq  = 1820.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 10.5f;
                p.cabinetDrop   = 0.52f;
                p.ampDrive      = 0.26f;
                p.toneWarmth    = 0.76f;
                p.reverbMix     = 0.58f;
                p.reverbDecay   = 0.86f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "08 - Manual 10-String E9th Workstation",
            "Workstation",
            "Direct manual copedent control over Pedals A, B, C and Knee Levers LKL (E->F) & LKR (E->D#) via UI pedals, Mod Wheel, or Keyswitches C1-G1.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 3; // Manual Copedent
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.0f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 85.0f;
                p.vibratoDepth  = 0.40f;
                p.vibratoRate   = 5.15f;
                p.autoSwell     = 0.15f;
                p.swellTime     = 140.0f;
                p.sampleBlend   = 0.75f;
                p.pickSnap      = 0.60f;
                p.pickupPos     = 2.20f;
                p.coilPeakFreq  = 2120.0f;
                p.coilResonance = 0.72f;
                p.sustainTime   = 7.0f;
                p.cabinetDrop   = 0.48f;
                p.ampDrive      = 0.16f;
                p.toneWarmth    = 0.54f;
                p.reverbMix     = 0.32f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        }
    };
    return kPresets;
}
