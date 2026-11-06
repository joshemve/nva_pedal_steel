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
            "Dusty Tascam 424 Portastudio 4-track preamp saturation & dynamic sag, dual-capstan tape wow/flutter, thumb-softened mute, and intimate bedroom booth.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.fxCharacter   = 1; // 4-Track Cassette
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.52f;
                p.pedalSpeed    = 160.0f;
                p.barGlide      = 155.0f;
                p.barSlideNoise = 0.26f;
                p.vibratoDepth  = 0.34f;
                p.vibratoRate   = 4.85f;
                p.autoSwell     = 0.28f;
                p.swellTime     = 115.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.28f;
                p.pickupPos     = 2.68f;
                p.coilPeakFreq  = 1860.0f;
                p.coilResonance = 0.62f;
                p.sustainTime   = 7.2f;
                p.cabinetDrop   = 0.44f;
                p.stereoWidth   = 0.96f;
                p.ampDrive      = 0.34f;
                p.toneWarmth    = 0.68f;
                p.delayMix      = 0.08f;
                p.delayTime     = 92.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.22f;
                p.reverbDecay   = 0.38f;
                p.outputLevel   = -0.6f;
                return p;
            }()
        },
        {
            "14 - Golden Hour Velvet",
            "Modern Pop-Folk",
            "Vocal-friendly modern Nashville/LA pop-folk steel through a 1979 Roland Dimension D stereo BBD chorus, feather-soft Goodrich entry, and silky condenser air.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.fxCharacter   = 2; // Dimension Chorus
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.50f;
                p.pedalSpeed    = 170.0f;
                p.barGlide      = 165.0f;
                p.barSlideNoise = 0.24f;
                p.vibratoDepth  = 0.38f;
                p.vibratoRate   = 4.95f;
                p.autoSwell     = 0.46f;
                p.swellTime     = 145.0f;
                p.sampleBlend   = 0.90f;
                p.pickSnap      = 0.34f;
                p.pickupPos     = 2.45f;
                p.coilPeakFreq  = 2040.0f;
                p.coilResonance = 0.66f;
                p.sustainTime   = 8.4f;
                p.cabinetDrop   = 0.38f;
                p.stereoWidth   = 1.32f;
                p.ampDrive      = 0.20f;
                p.toneWarmth    = 0.54f;
                p.delayMix      = 0.16f;
                p.delayTime     = 500.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.44f;
                p.reverbDecay   = 0.74f;
                p.outputLevel   = 0.5f;
                return p;
            }()
        },
        {
            "15 - Apollo Deep Space",
            "Cinematic Ambient",
            "Inspired by Brian Eno & Daniel Lanois's Apollo: weightless 420 ms Goodrich swells feeding a dual-grain Hann-windowed +12st octave shimmer and infinite cathedral plate.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3;
                p.playMode      = 2; // Lush Swell Pad
                p.fxCharacter   = 3; // Apollo Shimmer
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.78f;
                p.pedalSpeed    = 295.0f;
                p.barGlide      = 220.0f;
                p.barSlideNoise = 0.16f;
                p.vibratoDepth  = 0.44f;
                p.vibratoRate   = 4.55f;
                p.autoSwell     = 0.94f;
                p.swellTime     = 420.0f;
                p.sampleBlend   = 0.86f;
                p.pickSnap      = 0.12f;
                p.pickupPos     = 2.60f;
                p.coilPeakFreq  = 1980.0f;
                p.coilResonance = 0.64f;
                p.sustainTime   = 11.5f;
                p.cabinetDrop   = 0.50f;
                p.stereoWidth   = 1.42f;
                p.ampDrive      = 0.32f;
                p.toneWarmth    = 0.66f;
                p.delayMix      = 0.34f;
                p.delayTime     = 520.0f; // 1/8D synced
                p.tempoSync     = true;
                p.reverbMix     = 0.64f;
                p.reverbDecay   = 0.92f;
                p.outputLevel   = 0.2f;
                return p;
            }()
        },
        {
            "16 - Desert Mirage Spring",
            "Psych-Soul / Noir",
            "Sultry minor-key desert steel with dispersive 3-spring tank drip, synced Fender Vibrolux optical roach bias tremolo, and smoky 1/8 tape echo.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 1; // Crying Lead Gliss (pure minor sixths & triads!)
                p.fxCharacter   = 4; // Spring + Bias Trem
                p.volumePedal   = 0.96f;
                p.scoopDepth    = 0.62f;
                p.pedalSpeed    = 140.0f;
                p.barGlide      = 140.0f;
                p.barSlideNoise = 0.42f;
                p.vibratoDepth  = 0.44f;
                p.vibratoRate   = 5.10f;
                p.autoSwell     = 0.08f;
                p.swellTime     = 75.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.72f;
                p.pickupPos     = 2.08f;
                p.coilPeakFreq  = 2240.0f;
                p.coilResonance = 0.76f;
                p.sustainTime   = 7.8f;
                p.cabinetDrop   = 0.40f;
                p.stereoWidth   = 1.14f;
                p.ampDrive      = 0.30f;
                p.toneWarmth    = 0.48f;
                p.delayMix      = 0.18f;
                p.delayTime     = 360.0f; // 1/8 synced
                p.tempoSync     = true;
                p.reverbMix     = 0.48f;
                p.reverbDecay   = 0.68f;
                p.outputLevel   = 0.3f;
                return p;
            }()
        },
        {
            "17 - Late-Night R&B Sample Bed",
            "Alt-R&B / Soul",
            "Dark, underwater 24 dB/oct 1.88 kHz low-passed chords with 33-RPM resampled vinyl drift and warm 215 Hz chest — ready to drop straight over 808s and R&B vocals.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 3;
                p.playMode      = 1; // Crying Lead Gliss (pure min9/maj9 voicings + liquid portamento!)
                p.fxCharacter   = 5; // Underwater R&B
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.68f;
                p.pedalSpeed    = 190.0f;
                p.barGlide      = 185.0f;
                p.barSlideNoise = 0.18f;
                p.vibratoDepth  = 0.40f;
                p.vibratoRate   = 4.65f;
                p.autoSwell     = 0.62f;
                p.swellTime     = 220.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.18f;
                p.pickupPos     = 2.85f;
                p.coilPeakFreq  = 1720.0f;
                p.coilResonance = 0.56f;
                p.sustainTime   = 9.5f;
                p.cabinetDrop   = 0.48f;
                p.stereoWidth   = 1.24f;
                p.ampDrive      = 0.22f;
                p.toneWarmth    = 0.82f;
                p.delayMix      = 0.16f;
                p.delayTime     = 620.0f; // 1/4 synced
                p.tempoSync     = true;
                p.reverbMix     = 0.46f;
                p.reverbDecay   = 0.78f;
                p.outputLevel   = -1.4f;
                return p;
            }()
        },
        {
            "18 - Sunset Slack-Key Lounge",
            "Indie Slacker / Lounge",
            "Hollow 1950s Valco/Oahu lap-steel pickup midrange, lazy 2.32 Hz pitch warble, and sunny dry living-room slapback for indie-slacker & Hawaiian C6-style sixths.",
            [] {
                PedalSteelParams p;
                p.presetStyle   = 0;
                p.playMode      = 0;
                p.fxCharacter   = 6; // Vintage Warble
                p.volumePedal   = 0.95f;
                p.scoopDepth    = 0.58f;
                p.pedalSpeed    = 150.0f;
                p.barGlide      = 160.0f;
                p.barSlideNoise = 0.32f;
                p.vibratoDepth  = 0.30f;
                p.vibratoRate   = 4.80f;
                p.autoSwell     = 0.12f;
                p.swellTime     = 85.0f;
                p.sampleBlend   = 0.88f;
                p.pickSnap      = 0.54f;
                p.pickupPos     = 2.52f;
                p.coilPeakFreq  = 1820.0f;
                p.coilResonance = 0.68f;
                p.sustainTime   = 6.2f;
                p.cabinetDrop   = 0.42f;
                p.stereoWidth   = 0.94f;
                p.ampDrive      = 0.28f;
                p.toneWarmth    = 0.62f;
                p.delayMix      = 0.14f;
                p.delayTime     = 84.0f;
                p.tempoSync     = false;
                p.reverbMix     = 0.22f;
                p.reverbDecay   = 0.38f;
                p.outputLevel   = 0.8f;
                return p;
            }()
        }
    };
    return kPresets;
}
