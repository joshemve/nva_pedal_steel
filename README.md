# NVA Pedal Steel (`Nvps`)

**NVA Pedal Steel** (`v1.5.0`) is a C++20 / JUCE 8 MIDI & MPE-playable 10-String Pedal Steel Guitar instrument plugin (`VST3`, `AU` on macOS, and `Standalone`) featuring **Nashville E9th**, **Western Swing Back-Neck C6th**, and **26.75" Baritone Canyon B6** copedents.

![NVA Pedal Steel UI](renders/ui_screenshot.png)

## Highlights

- **Concert-Calibrated 44.1 kHz Stereo Studio Bank + Physical E9th String Engine**:
  - **26 de-chorded, concert-A440-calibrated, zero-crossing seamless stereo multi-sample zones** (`G3–A5`) plus **20 full-fidelity studio performance & lick slices** (`102.7 seconds` total 44.1 kHz 16-bit stereo studio audio embedded directly in the binary—zero external sample folders required).
  - **Dual-Bus Studio / Physical Architecture**: Pure studio recordings pass through delta-calibrated tone shaping (`0.0 dB` transparent at reference baseline so studio room air and pickup character are never double-filtered), while physical 16-harmonic E9th dispersion, real fingerpick transients, chrome-bar winding slide sheen, sympathetic cabinet resonance, and asymmetric 12AX7 Tweed saturation blend seamlessly underneath.
- **3 Authentic 10-String Copedents & 6 Steel Voice Styles**:
  - **Nashville E9th Front-Neck** (`Sweet Georgia E9`, `Hedonist Bootgaze`, `Mikey's Grit Tweed`, `Montana Swell`): Authentic 10-string open tuning (`F#4, D#4, G#4, E4, B3, G#3, F#3, E3, D3, B2`) with real bellcrank string-pull matrix for **Pedal A** (`+2` on `B` strings), **Pedal B** (`+1` on `G#` strings), **Pedal C** (`+2` on `E/B`), **LKL** (`+1` `E -> F`), **LKR** (`-1` `E -> D#`), **A+B Emmons/Day Slant** (`I -> IV` / `vi`), mechanical cabinet drop, and delayed wrist-roll tonebar vibrato.
  - **Western Swing Back-Neck C6th**: Classic D-10 back-neck jazz tuning (`G4, E4, C4, A3, G3, E3, C3, A2, F2, C2`) voiced for velvety 1956 Standel 25L15 (`807` beam-tetrode + `15"` JBL D130F) clarity and authentic **Pedals 5, 6, 7** copedent pulls (`C6/9 -> C9/13` and `Cm6/9 / F9` jazz slants).
  - **26.75" Baritone Canyon B6**: Heavy wound desert baritone steel (`C#4, A#3, D#4, B3, F#3, D#3, C#3, B2, A2, F#2`) with `-12st` acoustic resonator body gravity and heightened wound-string chrome-bar slide texture.
- **6 Playable Performance Modes**:
  1. **Smart Auto-Pedal E9 / C6**: Automatically assigns chord roots, thirds, fifths, and 6th/9ths to their true copedent roles so live keyboard triads & 7th/6th chords bend obliquely with Pedals A/B/C (or C6th Pedals 5/6/7) and Knee Levers.
  2. **Crying Lead Gliss**: Expressive single-string legato transfers with 2nd-order critically damped chrome-bar glide inertia and upward bellcrank scoops.
  3. **Lush Swell Pad**: Velvet Goodrich optical volume-pedal chord swells with softened pick transients.
  4. **Manual Pedals Only**: Pure manual copedent workstation controlled via CCs, keyswitches, MPE, or UI buttons.
  5. **Studio Phrase Licks**: Chromatically maps and transposes real recorded Nashville, Bootgaze, Tweed, and Montana pedal-steel licks across the entire MIDI keyboard.
  6. **Palm Chime Harmonics**: Physical 12th-fret right-palm node damping ahead of the all-pull changer — crystalline octave bell harmonics that still bend naturally through Pedals A/B/C and tonebar glides.
- **DAW Host Tempo-Synced Analog Tape / BBD Delay & Dattorro Figure-8 Plate Reverb**:
  - Quadrature-modulated Echoplex tape echo (`SYNC` locks to DAW `AudioPlayHead` BPM across `1/16`, `1/8T`, `1/8`, `1/8D`, `1/4`, `1/4D`) with dynamic phrase & pedal-slant harmonic ducking, feeding a wide figure-8 stereo plate reverb.
- **36 Analog Hardware & Studio Character Circuits (`37` Front-Panel Modes)**:
  - Dedicated first-principles hardware modeling alongside `Studio Standard`: **Tascam 424 4-Track Cassette**, **Coles 4038 Ribbon + Teletronix LA-2A Opto Velvet**, **4-Stage Script Phase 90 / Uni-Vibe Analog Optical Phaser**, **Accutronics 3-Spring Tank Drip + Deep Fender Vibrolux Optical Bias Tremolo**, **24 dB/oct `1.35 kHz` Resampled Alt-R&B Underwater Filter**, **Boss VB-2 `1.55 Hz` Analog Pitch Vibrato + Stereo CE-2 + 12AX7 Tube Honey**, **Apollo `+12st` Shimmer**, **1965 Leslie 147 Rotary Speaker**, **Gated Diode Bloom**, **Standel 807 Chamber**, **Bosstone Violin Fuzz**, **Talk-Box Vowel**, **1937 Bakelite Lap**, **Abbey Road Tape ADT**, **'85 Bradshaw Tri-Rack**, **Poly-Octave Organ**, **Twin Peaks '89 Trem-Noir**, **Revolver Reverse Tape**, **Mohan Veena Jawari**, **Neo-Soul CE-1 Syrup**, **Binson Echorec Drum**, **CS-80 Synth-Steel**, **Scheerhorn Dobro Cone**, **Loveless SPX90 Reverse**, **King Tubby Dub Tank**, **Microcosm Granular**, **Dumble ODS Overdrive**, **'66 Cinecittà Twang**, **Khruangbin Cry-Spring**, **VHS Generation-Drift**, **Mercury7 Sub-Cathedra**, **Golden Hour 224XL Silk**, **Mellotron M400 Flute-Strings**, **'71 Maggot Brain Octavia Fuzz**, **'72 Suitcase Rhodes & Dyno**, **Blonde White-Ferrari Prism**, and **'77 Rumours Pianet & Instant-Flanger**.
- **Full Studio QOL & Interactive UI**:
  - **Interactive 10-String Fretboard**: Dynamically displays `E9th`, `C6th`, or `Baritone B6` open string tunings and pedal pull labels; click any string to pluck and drag horizontally across frets `0–24` to slide the solid chrome bar in real time.
  - **Front-Panel Analog Character Selector**: Mix and match any of the 37 character modes onto any preset with click-free `17 ms` live crossfading.
  - **50 Factory Presets + User Preset Save/Load** (`Documents/NVA Pedal Steel/Presets/*.nvps`), `<`/`>` quick-steppers, preset-aware double-click knob reset, `Ctrl/Cmd + Drag` fine precision mode, `PLAY LICK` & `E / G / D / C` chord audition buttons, live stereo `L/R` peak meter, resizable vector UI (`1060×660` to `1600×980`), hover tooltips, and full DAW state persistence.

## MIDI, Keyswitch & MPE Reference

| Input | Function |
| :--- | :--- |
| **Notes `C0–G0` (`MIDI 24–31`)** | Momentary Copedent Keyswitches (`C0`=Pedal A, `C#0`=Pedal C, `D0`=Pedal B, `E0`=A+B Slant, `F0`=LKL, `G0`=LKR) |
| **Notes `C1–G1` (`MIDI 36–43`, Ch 1)** | Studio Phrase Lick Keyswitches (`C1`=Lick 1, `D1`=Lick 2, `E1`=Lick 3, `F1`=Lick 4, `G1`=Full Studio Performance) |
| **Notes `G#1+` (`MIDI 44+`, Ch 1)** | Polyphonic 10-String E9th Chromatic / Copedent / Lick Performance |
| **MPE (`MIDI Ch 3–16`)** | Per-note independent oblique pitch bends (`±12` semitones) + Channel Pressure / Poly Aftertouch vibrato expression |
| **`CC 1` (Mod Wheel)** | Smooth **A + B Pedal** slant (`I -> IV` / `vi` suspension) |
| **`CC 7` / `CC 11`** | **Goodrich Optical Volume Pedal** swell |
| **`CC 64` (Sustain Pedal)** | Natural 10-string damper sustain |
| **`CC 65 / 66 / 67 / 68 / 69`** | Continuous **Pedal A / Pedal B / Pedal C / LKL / LKR** control |
| **`Pitch Wheel` (Ch 1)** | Solid Chrome Tonebar glissando (`±7` semitones) with hand/bar mass inertia |

## Building & Automated Audit

Requires CMake 3.22+ and Visual Studio 2022 (Windows) or Xcode / Clang (macOS). Uses a local `JUCE_SOURCE` cache if present, or automatically fetches `JUCE 8.0.6` via CMake `FetchContent`.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
.\build\PedalSteelRender_artefacts\Release\PedalSteelRender.exe --audit
```

GitHub Actions CI ([`.github/workflows/build.yml`](.github/workflows/build.yml)) builds and audits both **Windows x64 (`VST3` + `Standalone`)** and **macOS Universal `arm64 + x86_64` (`AU` + `VST3` + `Standalone`)** bundles. Pre-built Windows x64 release binaries are also included in [`dist/`](dist/):
- **VST3**: `dist/NVA Pedal Steel.vst3`
- **Standalone**: `dist/NVA Pedal Steel.exe`
