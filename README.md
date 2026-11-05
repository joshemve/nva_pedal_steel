# NVA Pedal Steel (`Nvps`)

**NVA Pedal Steel** is a C++20 / JUCE MIDI-playable Nashville E9th 10-String Pedal Steel Guitar synthesizer & physical-modeling hybrid instrument plugin (`VST3` and `Standalone`).

![NVA Pedal Steel UI](renders/ui_screenshot.png)

## Features
- **10-String Nashville E9th Copedent Engine**:
  - Authentic 10-string open tuning (`F#4, D#4, G#4, E4, B3, G#3, F#3, E3, D3, B2`) with real mechanical string-pull matrix for **Pedal A**, **Pedal B**, **Pedal C**, **LKL** (Left Knee Left), and **LKR** (Left Knee Right), plus **A+B** Emmons/Day combination.
  - MIDI Controller mapping:
    - `CC 1` (Mod Wheel): **A + B Pedals** (I -> IV Maj / vi min copedent shift)
    - `CC 7` / `CC 11`: **Optical Volume Pedal Swell**
    - `CC 64` (Sustain Pedal): **Pedal A**
    - `CC 65`: **Pedal B**
    - `CC 66` (Sostenuto): **Pedal C**
    - `CC 67` (Soft Pedal): **LKL** (`E -> F`)
    - `CC 68` (Legato): **LKR** (`E -> Eb`)
    - `Pitch Wheel`: **Chrome Tonebar Glissando** with 2nd-order critically damped hand/bar mass inertia.
- **Hybrid Multi-Sampled + Physical Dispersion / Single-Coil LC Pickup DSP**:
  - Embedded multi-sample wavetables and harmonic envelopes extracted from real boutique Nashville & Americana pedal steel recordings (`Source/dsp/PedalSteelAssets.h`, `Source/dsp/PedalSteelMultiSamples.h`).
  - Position-dependent pick/pickup comb filtering ($24.25\text{ in}$ scale length), stationary high-inductance single-coil LC tank resonance, two-stage thumbpick/fingerpick pluck envelope, and phase-locked solid-bar wrist vibrato.
  - Fender Twin-style 12AX7 tube preamp saturation, 3-band tone stack, stereo tape echo, and unit-gain Schroeder studio spring/plate reverb with dynamic slide ducking.

## Building

Requires CMake 3.22+, Visual Studio 2022 (or clang/Xcode), and JUCE.

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 8
```

Built artifacts:
- **VST3**: `build/PedalSteel_artefacts/Release/VST3/NVA Pedal Steel.vst3`
- **Standalone**: `build/PedalSteel_artefacts/Release/Standalone/NVA Pedal Steel.exe`
