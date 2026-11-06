#include "Presets.h"

const std::vector<PedalSteelPreset>& getPedalSteelPresets() {
    static const std::vector<PedalSteelPreset> kPresets = {
        {
            "01 - Sweet Georgia E9 (Flyover Country)",
            "Reference Match",
            "Intimate, close-miked modern country E9th pedal steel matched to Flyover Country (83 BPM, E Maj). Sweet 2.28 kHz Emmons single-coil vocal sing, fingerpick snap, oblique B-pedal & A-pedal suspensions, and warm studio plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0; // Smart Auto-Pedal E9
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.42f;
                p.pedalSpeed    = 165.0f;
                p.barGlide      = 145.0f;
                p.barSlideNoise = 0.30f;
                p.vibratoDepth  = 0.38f;
                p.vibratoRate   = 5.20f;
                p.autoSwell     = 0.24f;
                p.swellTime     = 85.0f;
                p.sampleBlend   = 0.92f;
                p.pickSnap      = 0.76f;
                p.pickupPos     = 2.02f;
                p.coilPeakFreq  = 2280.0f;
                p.coilResonance = 0.74f;
                p.sustainTime   = 6.8f;
                p.cabinetDrop   = 0.35f;
                p.stereoWidth   = 1.00f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.38f;
                p.delayMix      = 0.00f;
                p.delayTime     = 360.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.30f;
                p.reverbDecay   = 0.54f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "02 - Hedonist Bootgaze (Indie Americana)",
            "Reference Match",
            "Dreamy, octave-blooming indie Americana pedal steel (90 BPM, G Maj). De-honked 2.42 kHz Emmons single-coil metallic bite, crisp fingerpick snap, pushed 12AX7 tube harmonic air, synced 1/8D Echoplex tape halo, and wide figure-8 plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1;
                p.playMode      = 0; // Smart Auto-Pedal E9
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.54f;
                p.pedalSpeed    = 175.0f;
                p.barGlide      = 155.0f;
                p.barSlideNoise = 0.36f;
                p.vibratoDepth  = 0.46f;
                p.vibratoRate   = 5.10f;
                p.autoSwell     = 0.16f;
                p.swellTime     = 110.0f;
                p.sampleBlend   = 0.82f;
                p.pickSnap      = 0.84f;
                p.pickupPos     = 1.95f;
                p.coilPeakFreq  = 2420.0f;
                p.coilResonance = 0.84f;
                p.sustainTime   = 8.2f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.18f;
                p.ampDrive      = 0.68f;
                p.toneWarmth    = 0.34f;
                p.delayMix      = 0.20f;
                p.delayTime     = 500.0f; // 1/8D analog tape halo when synced
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.74f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "03 - Mikey's Grit Vibey (Tweed Breakup)",
            "Reference Match",
            "Close-miked, weeping D-major pedal steel through a pushed Tweed Deluxe amp matched to Mikey's Country Mojo (120 BPM, D Maj). Upfront metallic string bite, crying downward bar slides, and wide stereo spring/plate space.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2;
                p.playMode      = 1; // Crying Lead Gliss
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.65f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 135.0f;
                p.barSlideNoise = 0.42f;
                p.vibratoDepth  = 0.48f;
                p.vibratoRate   = 5.25f;
                p.autoSwell     = 0.16f;
                p.swellTime     = 120.0f;
                p.sampleBlend   = 0.90f;
                p.pickSnap      = 0.74f;
                p.pickupPos     = 2.18f;
                p.coilPeakFreq  = 2220.0f;
                p.coilResonance = 0.72f;
                p.sustainTime   = 8.5f;
                p.cabinetDrop   = 0.38f;
                p.stereoWidth   = 1.08f;
                p.ampDrive      = 0.56f;
                p.toneWarmth    = 0.56f;
                p.delayMix      = 0.00f;
                p.delayTime     = 375.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.42f;
                p.reverbDecay   = 0.76f;
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
                p.barSlideNoise = 0.12f;
                p.vibratoDepth  = 0.34f;
                p.vibratoRate   = 4.60f;
                p.autoSwell     = 0.88f;
                p.swellTime     = 340.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.15f;
                p.pickupPos     = 2.95f;
                p.coilPeakFreq  = 1680.0f;
                p.coilResonance = 0.52f;
                p.sustainTime   = 9.2f;
                p.cabinetDrop   = 0.55f;
                p.stereoWidth   = 1.12f;
                p.ampDrive      = 0.16f;
                p.toneWarmth    = 0.84f;
                p.delayMix      = 0.00f;
                p.delayTime     = 475.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.76f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "05 - Nashville 400 Crying Lead",
            "Studio Lead",
            "Classic 1970s push-pull E9th lead voice through a Peavey Nashville 400 15-inch Black Widow combo with warm Echoplex quarter-note tape echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1;
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.82f;
                p.pedalSpeed    = 125.0f;
                p.barGlide      = 85.0f;
                p.barSlideNoise = 0.38f;
                p.vibratoDepth  = 0.48f;
                p.vibratoRate   = 5.35f;
                p.autoSwell     = 0.18f;
                p.swellTime     = 130.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.68f;
                p.pickupPos     = 2.05f;
                p.coilPeakFreq  = 2220.0f;
                p.coilResonance = 0.78f;
                p.sustainTime   = 7.2f;
                p.cabinetDrop   = 0.48f;
                p.stereoWidth   = 1.05f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.42f;
                p.delayMix      = 0.22f;
                p.delayTime     = 640.0f; // 1/4 note when synced
                p.tempoSync     = true;
                p.reverbMix     = 0.36f;
                p.reverbDecay   = 0.60f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "06 - Bakersfield Tape Slapback",
            "Vintage Country",
            "Snappy close-bridge fingerpicking with Fender Twin Reverb sparkle, fast 105ms bellcrank pulls, and 115ms analog tape slapback bounce.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.volumePedal   = 0.98f;
                p.scoopDepth    = 0.56f;
                p.pedalSpeed    = 105.0f;
                p.barGlide      = 65.0f;
                p.barSlideNoise = 0.28f;
                p.vibratoDepth  = 0.32f;
                p.vibratoRate   = 5.60f;
                p.autoSwell     = 0.05f;
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.85f;
                p.pickSnap      = 0.84f;
                p.pickupPos     = 1.75f;
                p.coilPeakFreq  = 2420.0f;
                p.coilResonance = 0.82f;
                p.sustainTime   = 5.8f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 0.90f;
                p.ampDrive      = 0.30f;
                p.toneWarmth    = 0.32f;
                p.delayMix      = 0.28f;
                p.delayTime     = 115.0f; // Classic studio slapback
                p.tempoSync     = false;
                p.reverbMix     = 0.26f;
                p.reverbDecay   = 0.46f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "07 - Cosmic Bootgaze Dotted-8th",
            "Indie / Shoegaze",
            "Interlocking E9th sixths, harp chimes & soaring bar cries locked to tempo-synced 1/8D Echoplex stereo tape delay, pushed 12AX7 harmonic sparkle, and wide figure-8 plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1;
                p.playMode      = 0;
                p.volumePedal   = 0.97f;
                p.scoopDepth    = 0.54f;
                p.pedalSpeed    = 155.0f;
                p.barGlide      = 125.0f;
                p.barSlideNoise = 0.36f;
                p.vibratoDepth  = 0.46f;
                p.vibratoRate   = 5.15f;
                p.autoSwell     = 0.14f;
                p.swellTime     = 95.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.78f;
                p.pickupPos     = 1.96f;
                p.coilPeakFreq  = 2420.0f;
                p.coilResonance = 0.84f;
                p.sustainTime   = 8.4f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.28f;
                p.ampDrive      = 0.60f;
                p.toneWarmth    = 0.34f;
                p.delayMix      = 0.46f;
                p.delayTime     = 500.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.46f;
                p.reverbDecay   = 0.78f;
                p.outputLevel   = 0.6f;
                return p;
            }()
        },
        {
            "08 - Slow-Gear Dream Steel (Ambient)",
            "Indie / Ambient",
            "Goodrich optical volume-pedal swells with automatic oblique B-pedal third-string harmonic blooms, shimmering 2.32 kHz steel overtones, synced tape halo, and cathedral plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3;
                p.playMode      = 2;
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.84f;
                p.pedalSpeed    = 285.0f;
                p.barGlide      = 195.0f;
                p.barSlideNoise = 0.22f;
                p.vibratoDepth  = 0.48f;
                p.vibratoRate   = 4.75f;
                p.autoSwell     = 0.92f;
                p.swellTime     = 335.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.20f;
                p.pickupPos     = 2.25f;
                p.coilPeakFreq  = 2320.0f;
                p.coilResonance = 0.78f;
                p.sustainTime   = 10.2f;
                p.cabinetDrop   = 0.48f;
                p.stereoWidth   = 1.32f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.52f;
                p.delayMix      = 0.28f;
                p.delayTime     = 520.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.56f;
                p.reverbDecay   = 0.86f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "09 - Cryin' In My Beer Ballad",
            "Classic Ballad",
            "Deep vocal tonebar glide with physical stainless-steel winding slide whistle, warm 15-inch speaker chest, and lush late-night plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2;
                p.playMode      = 1;
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.78f;
                p.pedalSpeed    = 175.0f;
                p.barGlide      = 185.0f;
                p.barSlideNoise = 0.58f;
                p.vibratoDepth  = 0.56f;
                p.vibratoRate   = 4.95f;
                p.autoSwell     = 0.30f;
                p.swellTime     = 165.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.64f;
                p.pickupPos     = 2.25f;
                p.coilPeakFreq  = 2160.0f;
                p.coilResonance = 0.74f;
                p.sustainTime   = 8.8f;
                p.cabinetDrop   = 0.45f;
                p.stereoWidth   = 1.15f;
                p.ampDrive      = 0.48f;
                p.toneWarmth    = 0.64f;
                p.delayMix      = 0.24f;
                p.delayTime     = 660.0f; // 1/4 synced
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.78f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "10 - Hot Tweed Roadhouse Grit",
            "Roots / Southern Rock",
            "Jumped-channel '57 Fender 5E3 Tweed Deluxe 6V6 breakup with woody 940 Hz pine-cab bark, dynamic cathode sag, 4.55 kHz Jensen P12R Alnico cone, and tight 118 ms slapback.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2;
                p.playMode      = 1; // Crying Lead Gliss (authentic chrome-bar slides!)
                p.volumePedal   = 0.94f;
                p.scoopDepth    = 0.72f;
                p.pedalSpeed    = 120.0f;
                p.barGlide      = 135.0f;
                p.barSlideNoise = 0.40f;
                p.vibratoDepth  = 0.48f;
                p.vibratoRate   = 5.25f;
                p.autoSwell     = 0.16f;
                p.swellTime     = 105.0f;
                p.sampleBlend   = 0.90f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.18f;
                p.coilPeakFreq  = 2060.0f;
                p.coilResonance = 0.72f;
                p.sustainTime   = 8.2f;
                p.cabinetDrop   = 0.44f;
                p.stereoWidth   = 1.06f;
                p.ampDrive      = 0.86f;
                p.toneWarmth    = 0.64f;
                p.delayMix      = 0.14f;
                p.delayTime     = 118.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.30f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = -1.6f;
                return p;
            }()
        },
        {
            "11 - Manual 10-String E9th Workstation",
            "Workstation",
            "Direct manual copedent & MPE control over Pedals A, B, C and Knee Levers LKL (E->F) & LKR (E->D#) via UI pedals, Mod Wheel, MPE, or Keyswitches C0-G0.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 3; // Manual Copedent
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.0f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 85.0f;
                p.barSlideNoise = 0.36f;
                p.vibratoDepth  = 0.40f;
                p.vibratoRate   = 5.15f;
                p.autoSwell     = 0.15f;
                p.swellTime     = 140.0f;
                p.sampleBlend   = 0.90f;
                p.pickSnap      = 0.66f;
                p.pickupPos     = 2.10f;
                p.coilPeakFreq  = 2240.0f;
                p.coilResonance = 0.74f;
                p.sustainTime   = 7.4f;
                p.cabinetDrop   = 0.46f;
                p.stereoWidth   = 1.00f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.42f;
                p.delayMix      = 0.14f;
                p.delayTime     = 360.0f;
                p.tempoSync     = true;
                p.reverbMix     = 0.32f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "12 - Studio Phrase Lick Machine (Chromatic)",
            "Phrase Workstation",
            "Selects studio licks via C1-F1 and transposes real recorded pedal-steel articulations chromatically across any key on your MIDI keyboard!",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 4; // Studio Phrase Licks
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.45f;
                p.pedalSpeed    = 155.0f;
                p.barGlide      = 130.0f;
                p.barSlideNoise = 0.32f;
                p.vibratoDepth  = 0.38f;
                p.vibratoRate   = 5.20f;
                p.autoSwell     = 0.20f;
                p.swellTime     = 100.0f;
                p.sampleBlend   = 0.92f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.02f;
                p.coilPeakFreq  = 2280.0f;
                p.coilResonance = 0.74f;
                p.sustainTime   = 7.0f;
                p.cabinetDrop   = 0.35f;
                p.stereoWidth   = 1.04f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.38f;
                p.delayMix      = 0.12f;
                p.delayTime     = 480.0f;
                p.tempoSync     = true;
                p.reverbMix     = 0.32f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        }
    };
    return kPresets;
}
