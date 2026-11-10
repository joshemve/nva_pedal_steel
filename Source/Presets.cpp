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
        },
        {
            "13 - 4-Track Cassette Steel",
            "Lo-Fi / Indie R&B",
            "Bone-dry bedroom Tascam 424 Portastudio 4-track preamp grit & dynamic headroom sag, 3.45 kHz cassette tape roll-off, and dual-capstan wow/flutter.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.fxCharacter   = 1; // 4-Track Cassette
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.56f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 145.0f;
                p.barSlideNoise = 0.34f;
                p.vibratoDepth  = 0.26f;
                p.vibratoRate   = 4.75f;
                p.autoSwell     = 0.05f;
                p.swellTime     = 70.0f;
                p.sampleBlend   = 0.78f;
                p.pickSnap      = 0.48f;
                p.pickupPos     = 2.55f;
                p.coilPeakFreq  = 1760.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 6.4f;
                p.cabinetDrop   = 0.46f;
                p.stereoWidth   = 0.84f;
                p.ampDrive      = 0.48f;
                p.toneWarmth    = 0.78f;
                p.delayMix      = 0.06f;
                p.delayTime     = 78.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.12f;
                p.reverbDecay   = 0.28f;
                p.outputLevel   = -1.6f;
                return p;
            }()
        },
        {
            "14 - Golden Hour Velvet",
            "Modern Pop-Folk",
            "Warm Coles 4038 figure-8 ribbon mic & Teletronix LA-2A opto-tube velvet: plush 215 Hz body, silky 5.15 kHz ribbon roll-off, and floating Lexicon 480L cloud.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.fxCharacter   = 2; // Ribbon Opto Velvet (0% dry comb-filtering!)
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.48f;
                p.pedalSpeed    = 165.0f;
                p.barGlide      = 160.0f;
                p.barSlideNoise = 0.24f;
                p.vibratoDepth  = 0.36f;
                p.vibratoRate   = 4.90f;
                p.autoSwell     = 0.18f;
                p.swellTime     = 110.0f;
                p.sampleBlend   = 0.92f;
                p.pickSnap      = 0.42f;
                p.pickupPos     = 2.45f;
                p.coilPeakFreq  = 1960.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 8.6f;
                p.cabinetDrop   = 0.38f;
                p.stereoWidth   = 1.16f;
                p.ampDrive      = 0.22f;
                p.toneWarmth    = 0.64f;
                p.delayMix      = 0.14f;
                p.delayTime     = 500.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.44f;
                p.reverbDecay   = 0.76f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        },
        {
            "15 - Dark Side Phase Steel",
            "Psych-Rock / Space",
            "Liquid 0.24 Hz 4-stage Script Phase 90 / Uni-Vibe analog optical phaser into warm Hiwatt Class-A tubes and synced 1/4-note Echoplex tape echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1; // Crying Lead Gliss (floating Gilmour Breathe glides!)
                p.fxCharacter   = 3; // Analog Phaser (4-Stage JFET Optical Phaser + Hiwatt Tubes)
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.28f;
                p.pedalSpeed    = 165.0f;
                p.barGlide      = 180.0f;
                p.barSlideNoise = 0.28f;
                p.vibratoDepth  = 0.30f;
                p.vibratoRate   = 4.75f;
                p.autoSwell     = 0.16f;
                p.swellTime     = 105.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.46f;
                p.pickupPos     = 2.42f;
                p.coilPeakFreq  = 2000.0f;
                p.coilResonance = 0.66f;
                p.sustainTime   = 9.8f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.24f;
                p.ampDrive      = 0.30f;
                p.toneWarmth    = 0.60f;
                p.delayMix      = 0.26f;
                p.delayTime     = 620.0f; // 1/4 synced Echoplex echo!
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.82f;
                p.outputLevel   = -0.3f;
                return p;
            }()
        },
        {
            "16 - Desert Mirage Spring",
            "Psych-Soul / Noir",
            "Twangy bridge-pickup desert noir steel with dispersive 3-spring tank splash, deep BPM-synced Vibrolux optical bias tremolo pulse, and smoky 1/8 echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2;
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 4; // Spring + Bias Trem
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.64f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 140.0f;
                p.barSlideNoise = 0.44f;
                p.vibratoDepth  = 0.42f;
                p.vibratoRate   = 5.10f;
                p.autoSwell     = 0.00f;
                p.swellTime     = 65.0f;
                p.sampleBlend   = 0.74f;
                p.pickSnap      = 0.86f;
                p.pickupPos     = 1.85f;
                p.coilPeakFreq  = 2440.0f;
                p.coilResonance = 0.82f;
                p.sustainTime   = 7.8f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.16f;
                p.ampDrive      = 0.34f;
                p.toneWarmth    = 0.38f;
                p.delayMix      = 0.18f;
                p.delayTime     = 360.0f; // 1/8 synced
                p.tempoSync     = true;
                p.reverbMix     = 0.50f;
                p.reverbDecay   = 0.70f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "17 - Late-Night R&B Sample Bed",
            "Alt-R&B / Soul",
            "Ultra-dark underwater 24 dB/oct 1.35 kHz sample-chop filter, 33-RPM resampled vinyl drift, and plush 200 Hz sub-chest — ready to drop under 808s and vocals.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3;
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 5; // Underwater R&B
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.68f;
                p.pedalSpeed    = 190.0f;
                p.barGlide      = 185.0f;
                p.barSlideNoise = 0.14f;
                p.vibratoDepth  = 0.36f;
                p.vibratoRate   = 4.60f;
                p.autoSwell     = 0.56f;
                p.swellTime     = 210.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.06f;
                p.pickupPos     = 3.05f;
                p.coilPeakFreq  = 1580.0f;
                p.coilResonance = 0.50f;
                p.sustainTime   = 9.8f;
                p.cabinetDrop   = 0.48f;
                p.stereoWidth   = 1.24f;
                p.ampDrive      = 0.20f;
                p.toneWarmth    = 0.88f;
                p.delayMix      = 0.16f;
                p.delayTime     = 620.0f; // 1/4 synced
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.80f;
                p.outputLevel   = -1.5f;
                return p;
            }()
        },
        {
            "18 - Sunset Slack-Key Lounge",
            "Indie Dream / Lounge",
            "Syrupy Sleep Walk & Mac DeMarco sunset steel: Boss VB-2 1.72 Hz analog pitch vibrato, stereo CE-2 halo, warm 12AX7 tube honey, and golden 1959 chamber.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1;
                p.playMode      = 1; // Crying Lead Gliss (singing liquid Sleep Walk glides!)
                p.fxCharacter   = 6; // Vintage Warble (VB-2 Pitch Vibrato + CE-2 + Tube Honey)
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.72f;
                p.pedalSpeed    = 165.0f;
                p.barGlide      = 195.0f;
                p.barSlideNoise = 0.26f;
                p.vibratoDepth  = 0.14f;
                p.vibratoRate   = 4.60f;
                p.autoSwell     = 0.06f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.80f;
                p.pickSnap      = 0.44f;
                p.pickupPos     = 2.88f;
                p.coilPeakFreq  = 1860.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 9.6f;
                p.cabinetDrop   = 0.42f;
                p.stereoWidth   = 1.22f;
                p.ampDrive      = 0.34f;
                p.toneWarmth    = 0.64f;
                p.delayMix      = 0.20f;
                p.delayTime     = 315.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.44f;
                p.reverbDecay   = 0.72f;
                p.outputLevel   = 0.4f;
                return p;
            }()
        },
        {
            "19 - Apollo Ascent Shimmer",
            "Ambient / Space",
            "Daniel Lanois & Brian Eno '83 homage: velvet Goodrich volume-pedal chord swells into a dual-grain +12st Eventide octave regenerator and 2.35 kHz-damped Lexicon cathedral hall.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3; // Montana Lush Swell
                p.playMode      = 2; // Lush Swell Pad
                p.fxCharacter   = 7; // Apollo Shimmer (+12st Dual-Grain Octave Halo)
                p.volumePedal   = 0.94f;
                p.scoopDepth    = 0.72f;
                p.pedalSpeed    = 255.0f;
                p.barGlide      = 210.0f;
                p.barSlideNoise = 0.14f;
                p.vibratoDepth  = 0.36f;
                p.vibratoRate   = 4.55f;
                p.autoSwell     = 0.90f;
                p.swellTime     = 360.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.08f;
                p.pickupPos     = 2.75f;
                p.coilPeakFreq  = 1880.0f;
                p.coilResonance = 0.56f;
                p.sustainTime   = 11.0f;
                p.cabinetDrop   = 0.48f;
                p.stereoWidth   = 1.38f;
                p.ampDrive      = 0.18f;
                p.toneWarmth    = 0.74f;
                p.delayMix      = 0.22f;
                p.delayTime     = 640.0f; // 1/4 synced halo
                p.tempoSync     = true;
                p.reverbMix     = 0.62f;
                p.reverbDecay   = 0.90f;
                p.outputLevel   = 0.4f;
                return p;
            }()
        },
        {
            "20 - Starlight Palm Chimes",
            "Bell Harmonics",
            "Authentic 12th-fret right-palm damped steel harmonics (Buddy Emmons / Space Cowboy): soft flesh-thumb attack, crystalline octave bell overtones that bend through Pedals A & B, and 1/8D tape halo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 5; // Palm Chime Harmonics
                p.fxCharacter   = 0;
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.36f;
                p.pedalSpeed    = 155.0f;
                p.barGlide      = 135.0f;
                p.barSlideNoise = 0.18f;
                p.vibratoDepth  = 0.30f;
                p.vibratoRate   = 4.95f;
                p.autoSwell     = 0.08f;
                p.swellTime     = 85.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.44f;
                p.pickupPos     = 2.38f;
                p.coilPeakFreq  = 1780.0f;
                p.coilResonance = 0.48f;
                p.sustainTime   = 9.8f;
                p.cabinetDrop   = 0.36f;
                p.stereoWidth   = 1.26f;
                p.ampDrive      = 0.14f;
                p.toneWarmth    = 0.68f;
                p.delayMix      = 0.24f;
                p.delayTime     = 500.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.50f;
                p.reverbDecay   = 0.80f;
                p.outputLevel   = 0.8f;
                return p;
            }()
        },
        {
            "21 - Sacred Gospel Leslie",
            "Soul / Sacred Steel",
            "Authentic 1965 Leslie 147 Sacred Steel: dynamic 6550 push-pull tube amp, phase-coherent 800 Hz dividing network, Chorale-to-Tremolo dual-motor inertia, and directional horn beaming.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0; // Full-range Studio E9th feeding the 6550 Leslie 147 amp cleanly!
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 8; // Leslie 147 Rotary (Dual-Motor Chorale <-> Tremolo + Horn Beaming)
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.62f;
                p.pedalSpeed    = 120.0f;
                p.barGlide      = 115.0f;
                p.barSlideNoise = 0.32f;
                p.vibratoDepth  = 0.08f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.06f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.25f;
                p.coilPeakFreq  = 2040.0f;
                p.coilResonance = 0.58f;
                p.sustainTime   = 8.8f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.28f;
                p.ampDrive      = 0.30f;
                p.toneWarmth    = 0.60f;
                p.delayMix      = 0.00f;
                p.delayTime     = 340.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.28f;
                p.reverbDecay   = 0.54f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "22 - Blown Console Reverb-X",
            "Alt-Indie / Textural",
            "Intimate Bon Iver & Mk.gee studio steel: upfront FET dry string ducks a dense live room that swells up through warm silicon-diode console saturation and gates on silence.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 9; // Gated Diode Bloom
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.58f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 150.0f;
                p.barSlideNoise = 0.38f;
                p.vibratoDepth  = 0.34f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.06f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.70f;
                p.pickupPos     = 2.30f;
                p.coilPeakFreq  = 1980.0f;
                p.coilResonance = 0.66f;
                p.sustainTime   = 6.2f;
                p.cabinetDrop   = 0.42f;
                p.stereoWidth   = 1.20f;
                p.ampDrive      = 0.38f;
                p.toneWarmth    = 0.66f;
                p.delayMix      = 0.06f;
                p.delayTime     = 112.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.22f;
                p.reverbDecay   = 0.48f;
                p.outputLevel   = -1.0f;
                return p;
            }()
        },
        {
            "23 - Western Swing C6th Lounge",
            "Vintage Swing / Jazz",
            "Smoky 1956 Standel 25L15 (807 tubes + 15\" JBL D130F) & Capitol tile chamber C6th jazz steel (Buddy Emmons / Night Life): velvety 6/9-to-13th pedal slants and zero boxy honk.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 4;  // Western Swing C6th Back-Neck
                p.playMode      = 0;
                p.fxCharacter   = 10; // Standel 807 Chamber (807 Beam-Tetrode + 15" D130F + Capitol Tile Chamber)
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.24f;
                p.pedalSpeed    = 135.0f;
                p.barGlide      = 120.0f;
                p.barSlideNoise = 0.22f;
                p.vibratoDepth  = 0.25f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.05f;
                p.swellTime     = 70.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.52f;
                p.pickupPos     = 2.62f;
                p.coilPeakFreq  = 1880.0f;
                p.coilResonance = 0.50f;
                p.sustainTime   = 7.4f;
                p.cabinetDrop   = 0.38f;
                p.stereoWidth   = 1.20f;
                p.ampDrive      = 0.22f;
                p.toneWarmth    = 0.64f;
                p.delayMix      = 0.14f;
                p.delayTime     = 118.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.38f;
                p.reverbDecay   = 0.64f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "24 - Paris Texas Baritone",
            "Cinematic Score",
            "Lonely Mojave desert baritone steel (Ry Cooder / Santaolalla): 26.75-inch heavy wound scale, -12st acoustic resonator body gravity, rich bar-winding scrape, and vast canyon echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 5; // Baritone Canyon B6
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 0;
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.54f;
                p.pedalSpeed    = 175.0f;
                p.barGlide      = 195.0f;
                p.barSlideNoise = 0.58f;
                p.vibratoDepth  = 0.46f;
                p.vibratoRate   = 4.65f;
                p.autoSwell     = 0.10f;
                p.swellTime     = 95.0f;
                p.sampleBlend   = 0.82f;
                p.pickSnap      = 0.68f;
                p.pickupPos     = 2.52f;
                p.coilPeakFreq  = 1680.0f;
                p.coilResonance = 0.58f;
                p.sustainTime   = 9.6f;
                p.cabinetDrop   = 0.50f;
                p.stereoWidth   = 1.28f;
                p.ampDrive      = 0.38f;
                p.toneWarmth    = 0.74f;
                p.delayMix      = 0.26f;
                p.delayTime     = 440.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.52f;
                p.reverbDecay   = 0.84f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        },
        {
            "25 - Country-Gaze Violin Fuzz",
            "Indie Shoegaze",
            "1969 Jordan Bosstone BJT/diode sustainer + Big Muff violin mid-throat into a 15-inch JBL speaker (Wednesday / MJ Lenderman / Sneaky Pete Kleinow): infinite singing fuzz sustain and shoegaze pedal bloom.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1; // Hedonist Bootgaze
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 11; // Bosstone Violin Fuzz
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.58f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 155.0f;
                p.barSlideNoise = 0.38f;
                p.vibratoDepth  = 0.30f;
                p.vibratoRate   = 4.90f;
                p.autoSwell     = 0.08f;
                p.swellTime     = 85.0f;
                p.sampleBlend   = 0.82f;
                p.pickSnap      = 0.66f;
                p.pickupPos     = 2.18f;
                p.coilPeakFreq  = 2120.0f;
                p.coilResonance = 0.68f;
                p.sustainTime   = 10.6f;
                p.cabinetDrop   = 0.36f;
                p.stereoWidth   = 1.24f;
                p.ampDrive      = 0.42f;
                p.toneWarmth    = 0.62f;
                p.delayMix      = 0.28f;
                p.delayTime     = 500.0f; // 1/8D
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.78f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        },
        {
            "26 - Talking Opto-Vowel Steel",
            "70s Funk / Talk-Box",
            "1964 Pete Drake horn-driver mouth-tube + Mu-Tron III CdS vactrol dual-formant (F1+F2) vocal tract (Pete Drake 'Forever' / Steely Dan / Robert Randolph): chords and glides literally form human /u/->/o/->/a/->/ae/ vowels.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0; // Nashville Gold
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 12; // Talk-Box Vowel
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.62f;
                p.pedalSpeed    = 125.0f;
                p.barGlide      = 130.0f;
                p.barSlideNoise = 0.26f;
                p.vibratoDepth  = 0.26f;
                p.vibratoRate   = 5.05f;
                p.autoSwell     = 0.22f;
                p.swellTime     = 110.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.18f;
                p.coilPeakFreq  = 2180.0f;
                p.coilResonance = 0.72f;
                p.sustainTime   = 8.6f;
                p.cabinetDrop   = 0.34f;
                p.stereoWidth   = 1.18f;
                p.ampDrive      = 0.28f;
                p.toneWarmth    = 0.52f;
                p.delayMix      = 0.16f;
                p.delayTime     = 360.0f; // 1/8
                p.tempoSync     = true;
                p.reverbMix     = 0.30f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = 0.4f;
                return p;
            }()
        },
        {
            "27 - Waikiki Bakelite Lap",
            "Vintage Hawaiian",
            "1937 Rickenbacker B6 Bakelite C6th lap steel + 1.5-inch wrap-around Horseshoe Magnet + octal 6L6 Gibson EH-150 amp & RCA 44 ribbon room (Jerry Byrd / Sol Ho'opi'i / Jeremy Wakefield): sun-soaked shimmering island sixths.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 4; // Western Swing C6th (C-E-G-A-C-E-G-A-F-C!)
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 13; // 1937 Bakelite Lap
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.34f;
                p.pedalSpeed    = 115.0f;
                p.barGlide      = 145.0f;
                p.barSlideNoise = 0.28f;
                p.vibratoDepth  = 0.28f;
                p.vibratoRate   = 5.55f; // Sweet singing Hawaiian bar shimmer (zero siren wobble!)
                p.autoSwell     = 0.04f;
                p.swellTime     = 60.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.60f;
                p.pickupPos     = 2.55f;
                p.coilPeakFreq  = 1620.0f;
                p.coilResonance = 0.62f;
                p.sustainTime   = 8.0f;
                p.cabinetDrop   = 0.10f; // Non-pedal Bakelite lap body stability
                p.stereoWidth   = 1.04f;
                p.ampDrive      = 0.24f;
                p.toneWarmth    = 0.66f;
                p.delayMix      = 0.08f;
                p.delayTime     = 105.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.26f;
                p.reverbDecay   = 0.48f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "28 - Abbey Road ADT Twin-Slide",
            "70s Pop-Rock Slide",
            "Fairchild 670 / EMI TG12345 Zener limiter + Ken Townsend 33.5 ms Studer J37 varispeed Artificial Double Tracking (George Harrison 'My Sweet Lord' / B.J. Cole 'Tiny Dancer'): creamy double-tracked weeping twin-slide.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0; // Nashville Gold
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 14; // Abbey Road Tape ADT
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.50f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 150.0f;
                p.barSlideNoise = 0.24f;
                p.vibratoDepth  = 0.22f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.06f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.58f;
                p.pickupPos     = 2.32f;
                p.coilPeakFreq  = 2080.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 9.4f;
                p.cabinetDrop   = 0.34f;
                p.stereoWidth   = 1.26f;
                p.ampDrive      = 0.26f;
                p.toneWarmth    = 0.58f;
                p.delayMix      = 0.12f;
                p.delayTime     = 135.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.32f;
                p.reverbDecay   = 0.58f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "29 - 80s Rack Dimension Clean",
            "80s Studio Pop",
            "1985 Bradshaw LA multi-rack: DBX 160X spanky VCA + Aphex Type-C bell air + symmetrical 3-phase DyTronics Tri-Stereo Chorus & Lexicon PCM70 circular pan-delays (Michael Landau / Paul Franklin / Prefab Sprout).",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0; // Nashville Gold
                p.playMode      = 1; // Lead Legato (prevents raked-chord overlap stealing!)
                p.fxCharacter   = 15; // 80s Tri-Chorus & PCM70 Rack
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.08f; // Crisp concert-locked studio chord attacks
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 125.0f;
                p.barSlideNoise = 0.18f;
                p.vibratoDepth  = 0.10f;
                p.vibratoRate   = 4.75f;
                p.autoSwell     = 0.04f; // Instant DBX 160X glassy pick spank!
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.90f;
                p.pickSnap      = 0.76f; // Crystalline 1985 LA studio bell attack
                p.pickupPos     = 2.08f;
                p.coilPeakFreq  = 2580.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 9.4f;
                p.cabinetDrop   = 0.03f;
                p.stereoWidth   = 1.38f;
                p.ampDrive      = 0.12f;
                p.toneWarmth    = 0.36f; // Glossy hi-fi rack top-end clarity
                p.delayMix      = 0.08f; // Complements internal PCM70 circular 1/8 + 1/8D pan-delays
                p.delayTime     = 500.0f; // 1/8D
                p.tempoSync     = true;
                p.reverbMix     = 0.40f;
                p.reverbDecay   = 0.76f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "30 - Poly-Octave Messina Choir",
            "Cathedral / Organ",
            "Phase-locked per-string 16' Bourdon Sub + 8' Studio Steel + silk-filtered 4' Flute Octave through a Messina harmonium bellows & Voix-Celeste air (Bon Iver Messina / Chuck Johnson 'Balsams'): luminous breathing pipe-organ steel.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0; // Crisp concert-calibrated studio string core
                p.playMode      = 1; // Clean raked chords + expressive lead glides
                p.fxCharacter   = 16; // Poly-Octave Organ
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.10f; // Concert-locked pipe-organ intonation
                p.pedalSpeed    = 165.0f;
                p.barGlide      = 155.0f;
                p.barSlideNoise = 0.14f;
                p.vibratoDepth  = 0.10f;
                p.vibratoRate   = 4.60f;
                p.autoSwell     = 0.30f; // Responsive breathing reed/pipe attack
                p.swellTime     = 130.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.36f;
                p.pickupPos     = 2.35f;
                p.coilPeakFreq  = 2180.0f;
                p.coilResonance = 0.58f;
                p.sustainTime   = 10.8f;
                p.cabinetDrop   = 0.0f; // Pipe organs never sag flat on pedal pulls!
                p.stereoWidth   = 1.34f;
                p.ampDrive      = 0.18f;
                p.toneWarmth    = 0.46f; // Open, luminous upper pipe harmonics
                p.delayMix      = 0.14f;
                p.delayTime     = 620.0f; // 1/4
                p.tempoSync     = true;
                p.reverbMix     = 0.45f;
                p.reverbDecay   = 0.80f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        },
        {
            "31 - Two Star VG-8 Bowed Steel",
            "Alt-Indie / VG-8",
            "Direct Roland VG-8 D62 BowedPad & Baritone rig (A1 & C1 hardware reference): warm thumb-plucked baritone body + 95 ms COSM HRM BowedPad swell, singing bowed cello/horn lead, and velocity-gated 42 ms bridge-pickup strum blast.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1; // Crying Lead Gliss + Open-Shell Polyphony
                p.fxCharacter   = 17; // Two Star VG-8 Bowed
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.04f; // Concert-locked VG-8 chord intonation (no country pitch scoop on chords!)
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 135.0f;
                p.barSlideNoise = 0.16f;
                p.vibratoDepth  = 0.08f; // Rock-solid chords; solo lead notes auto-boost to singing bowed vibrato!
                p.vibratoRate   = 5.15f;
                p.autoSwell     = 0.04f; // Warm thumb pluck up front while 95ms VG-8 D62 BowedPad HRM blooms behind it!
                p.swellTime     = 95.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.48f; // Soft thumb warmth on A1 notes; explodes with 42ms tin-strum on C1 vel >= 0.86 blasts!
                p.pickupPos     = 2.35f;
                p.coilPeakFreq  = 2550.0f;
                p.coilResonance = 0.56f;
                p.sustainTime   = 9.6f;
                p.cabinetDrop   = 0.0f;
                p.stereoWidth   = 1.38f;
                p.ampDrive      = 0.18f;
                p.toneWarmth    = 0.34f; // Open Studio Bus so A1 Bowed Cello Lead & C1 D62 BowedPad Strum Blasts shine!
                p.delayMix      = 0.03f; // Tight direct hardware VG-8 room slap (no long echo smear between phrases!)
                p.delayTime     = 280.0f;
                p.tempoSync     = true;
                p.reverbMix     = 0.18f; // Tight studio room around the internal Ducked Rainger Reverb-X
                p.reverbDecay   = 0.38f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "32 - '66 Revolver Backwards Tape",
            "Psychedelic Tape",
            "Abbey Road Room 2 real-time -1.000x backwards-reading Studer J37 tape heads + 15-ips Zonal oxide compression & EMT 140 plate (George Harrison 'I'm Only Sleeping' / Castles Made of Sand / Beck): concert-locked reverse envelopes and backwards glides.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1;
                p.fxCharacter   = 18; // Revolver Reverse Tape
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.34f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 145.0f;
                p.barSlideNoise = 0.22f;
                p.vibratoDepth  = 0.14f;
                p.vibratoRate   = 4.90f;
                p.autoSwell     = 0.0f; // Crisp input pluck creates unmistakable backwards zwoooop-SNAP!
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.84f;
                p.pickupPos     = 2.12f;
                p.coilPeakFreq  = 2280.0f;
                p.coilResonance = 0.68f;
                p.sustainTime   = 7.4f;
                p.cabinetDrop   = 0.08f;
                p.stereoWidth   = 1.28f;
                p.ampDrive      = 0.28f;
                p.toneWarmth    = 0.52f;
                p.delayMix      = 0.12f;
                p.delayTime     = 380.0f; // 1/8
                p.tempoSync     = true;
                p.reverbMix     = 0.36f;
                p.reverbDecay   = 0.64f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "33 - Mohan Veena Sitar-Slide",
            "World / Psychedelic",
            "Grammy-winning Indian slide guitar physics (Vishwa Mohan Bhatt & Ry Cooder / George Harrison / Steely Dan Coral Sitar): per-string parabolic deer-horn Jawari grazing-bridge buzz, hollow gourd Tumba resonance, and shimmering Taraf sympathetic strings.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0; // Poly Copedent: crisp mizrab plucks over open Tambura drone strings + MPE meend bends
                p.fxCharacter   = 19; // Mohan Veena Jawari
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.44f; // Vocal Indian meend grace scoops
                p.pedalSpeed    = 135.0f;
                p.barGlide      = 160.0f;
                p.barSlideNoise = 0.34f;
                p.vibratoDepth  = 0.24f;
                p.vibratoRate   = 5.45f;
                p.autoSwell     = 0.02f;
                p.swellTime     = 65.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.78f; // Wire mizrab plectrum bite
                p.pickupPos     = 1.92f;
                p.coilPeakFreq  = 2480.0f;
                p.coilResonance = 0.66f;
                p.sustainTime   = 8.8f;
                p.cabinetDrop   = 0.0f;
                p.stereoWidth   = 1.26f;
                p.ampDrive      = 0.20f;
                p.toneWarmth    = 0.46f;
                p.delayMix      = 0.08f;
                p.delayTime     = 320.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.38f;
                p.reverbDecay   = 0.68f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "34 - Atlanta Neo-Soul Syrup",
            "Modern R&B / Indie",
            "Modern indie-R&B & 808-ballad steel (Faye Webster 'Kingston' / Matt Stoessel / Blood Orange): heavy Ross/Dyna-Comp liquid sustain, anti-twang velvet thumb warmth, and a 1976 Boss CE-1 Chorus Ensemble JFET preamp + MN3002 stereo melt.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1;
                p.fxCharacter   = 20; // Neo-Soul CE-1 Syrup
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.46f;
                p.pedalSpeed    = 155.0f;
                p.barGlide      = 165.0f;
                p.barSlideNoise = 0.14f;
                p.vibratoDepth  = 0.14f;
                p.vibratoRate   = 4.65f;
                p.autoSwell     = 0.08f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.92f;
                p.pickSnap      = 0.32f; // Soft flesh/thumb warmth — zero country twang!
                p.pickupPos     = 2.65f;
                p.coilPeakFreq  = 1820.0f;
                p.coilResonance = 0.50f;
                p.sustainTime   = 9.2f;
                p.cabinetDrop   = 0.08f;
                p.stereoWidth   = 1.32f;
                p.ampDrive      = 0.20f;
                p.toneWarmth    = 0.74f;
                p.delayMix      = 0.14f;
                p.delayTime     = 620.0f; // 1/4
                p.tempoSync     = true;
                p.reverbMix     = 0.34f;
                p.reverbDecay   = 0.62f;
                p.outputLevel   = 0.0f;
                return p;
            }()
        },
        {
            "35 - Binson Echorec Space-Fury",
            "Prog / Psych Lead",
            "David Gilmour Pompeii & 'High Hopes' soaring lap-steel rig: Pete Cornish tube-driver sustain into a 100W Hiwatt DR103 / Fane stack and an authentic 4-Head 1968 Binson Echorec B2 rotating magnetic steel drum (1/16 + 1/8 + 1/8D + 1/4 Swirl).",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 1; // Harmonic-rich Bootgaze steel voice
                p.playMode      = 1;
                p.fxCharacter   = 21; // Binson Echorec Drum
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.52f;
                p.pedalSpeed    = 145.0f;
                p.barGlide      = 170.0f;
                p.barSlideNoise = 0.36f;
                p.vibratoDepth  = 0.32f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.06f;
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.15f;
                p.coilPeakFreq  = 2160.0f;
                p.coilResonance = 0.68f;
                p.sustainTime   = 10.8f;
                p.cabinetDrop   = 0.12f;
                p.stereoWidth   = 1.34f;
                p.ampDrive      = 0.38f;
                p.toneWarmth    = 0.56f;
                p.delayMix      = 0.06f;
                p.delayTime     = 500.0f; // 1/8D
                p.tempoSync     = true;
                p.reverbMix     = 0.44f;
                p.reverbDecay   = 0.78f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        },
        {
            "36 - CS-80 Stadium Synth-Steel",
            "Analog Synth-Steel",
            "Greg Leisz on Daft Punk 'RAM' x Mike Dean / Vangelis 'Blade Runner': per-string 0.00-cent Yamaha CS-80 sawtooth + sub-oscillator & polyphonic IG0015 resonant brass filter swell co-voiced with real pedal steel into 3-phase chorus and stadium echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1;
                p.fxCharacter   = 22; // CS-80 Synth-Steel
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.16f;
                p.pedalSpeed    = 155.0f;
                p.barGlide      = 150.0f;
                p.barSlideNoise = 0.20f;
                p.vibratoDepth  = 0.14f;
                p.vibratoRate   = 4.75f;
                p.autoSwell     = 0.16f;
                p.swellTime     = 95.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.54f;
                p.pickupPos     = 2.22f;
                p.coilPeakFreq  = 2240.0f;
                p.coilResonance = 0.60f;
                p.sustainTime   = 10.6f;
                p.cabinetDrop   = 0.0f;
                p.stereoWidth   = 1.40f;
                p.ampDrive      = 0.20f;
                p.toneWarmth    = 0.48f;
                p.delayMix      = 0.10f;
                p.delayTime     = 500.0f; // 1/8D
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.84f;
                p.outputLevel   = -0.4f;
                return p;
            }()
        }
    };
    return kPresets;
}
