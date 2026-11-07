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
            "Joyous Robert Randolph & Abbey Road rotary steel: pushed 6550 tube preamp into an 800 Hz crossover spinning a 3D Doppler phenolic horn & opposing wooden bass drum.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 2; // Mikey's Grit Tweed
                p.playMode      = 1; // Crying Lead Gliss
                p.fxCharacter   = 8; // Leslie 147 Rotary
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.68f;
                p.pedalSpeed    = 115.0f;
                p.barGlide      = 115.0f;
                p.barSlideNoise = 0.36f;
                p.vibratoDepth  = 0.24f;
                p.vibratoRate   = 5.10f;
                p.autoSwell     = 0.08f;
                p.swellTime     = 80.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.76f;
                p.pickupPos     = 2.12f;
                p.coilPeakFreq  = 2160.0f;
                p.coilResonance = 0.72f;
                p.sustainTime   = 8.4f;
                p.cabinetDrop   = 0.42f;
                p.stereoWidth   = 1.22f;
                p.ampDrive      = 0.62f;
                p.toneWarmth    = 0.54f;
                p.delayMix      = 0.00f;
                p.delayTime     = 340.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.30f;
                p.reverbDecay   = 0.56f;
                p.outputLevel   = -0.6f;
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
            "Back-neck D-10 C6th copedent (Speedy West / Buddy Emmons): smoky 6/9-to-dominant-9th pedal pulls, woody 325 Hz octal-tube Tweed Pro punch, and bouncy jazz swing.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 4; // Western Swing C6th Back-Neck
                p.playMode      = 0;
                p.fxCharacter   = 0;
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.34f;
                p.pedalSpeed    = 105.0f;
                p.barGlide      = 85.0f;
                p.barSlideNoise = 0.24f;
                p.vibratoDepth  = 0.30f;
                p.vibratoRate   = 5.35f;
                p.autoSwell     = 0.04f;
                p.swellTime     = 65.0f;
                p.sampleBlend   = 0.84f;
                p.pickSnap      = 0.78f;
                p.pickupPos     = 2.42f;
                p.coilPeakFreq  = 1720.0f;
                p.coilResonance = 0.58f;
                p.sustainTime   = 4.4f;
                p.cabinetDrop   = 0.38f;
                p.stereoWidth   = 0.88f;
                p.ampDrive      = 0.38f;
                p.toneWarmth    = 0.72f;
                p.delayMix      = 0.12f;
                p.delayTime     = 96.0f; // Tight Texas swing room slap
                p.tempoSync     = false;
                p.reverbMix     = 0.20f;
                p.reverbDecay   = 0.38f;
                p.outputLevel   = 0.0f;
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
        }
    };
    return kPresets;
}
