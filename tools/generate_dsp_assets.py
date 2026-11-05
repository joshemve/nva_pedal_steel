import os
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
out_header = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelAssets.h"
os.makedirs(os.path.dirname(out_header), exist_ok=True)

def load_mono(filename):
    path = os.path.join(ref_dir, filename)
    sr, data = wav.read(path)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    mono = 0.5 * (x[:, 0] + x[:, 1]) if x.ndim == 2 else x
    return sr, mono

# 1. Extract 16 single-cycle 2048-point wavetables from exact harmonic spectra of the 4 reference files
# (4 styles x 4 pitch registers: Low [Strings 8-10], LowMid [Strings 6-7], MidSing [Strings 4-5], HighCry [Strings 1-3])
wavetable_specs = [
    # Style 0: Sweet Georgia (E9th Clean Country - Emmons / Twin Reverb)
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 15.00, 15.60, 276.6, "SweetGeorgia_Low_CS4"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 18.90, 19.40, 329.8, "SweetGeorgia_LowMid_E4"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  0.40,  1.00, 415.3, "SweetGeorgia_MidSing_GS4"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  5.95,  6.35, 496.5, "SweetGeorgia_HighCry_B4"),

    # Style 1: Hedonist (Bootgaze Indie Americana - Warm Octave Bloom)
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", 15.10, 15.35, 196.2, "Hedonist_Low_G3"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",  5.45,  5.80, 262.3, "Hedonist_LowMid_C4"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",  0.10,  0.45, 438.5, "Hedonist_MidSing_A4"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",  2.05,  2.32, 591.2, "Hedonist_HighCry_D5"),

    # Style 2: Mikey's Grit Vibey (Tweed Breakup & Singing Upper Neck)
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",  1.65,  2.00, 493.8, "GritVibey_Low_B4"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",  4.20,  4.80, 587.2, "GritVibey_LowMid_D5"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",  8.10,  8.65, 740.0, "GritVibey_MidSing_FS5"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", 14.15, 14.65, 878.0, "GritVibey_HighCry_A5"),

    # Style 3: Montana Swell (Lush Volume-Pedal Chord Pad)
    ("CO_MCS_126_pedal_steel_montana_Gmaj.wav", 13.50, 14.30, 261.5, "Montana_Low_C4"),
    ("CO_MCS_126_pedal_steel_montana_Gmaj.wav",  5.75,  6.35, 293.3, "Montana_LowMid_D4"),
    ("CO_MCS_126_pedal_steel_montana_Gmaj.wav",  2.10,  2.65, 392.5, "Montana_MidSing_G4"),
    ("CO_MCS_126_pedal_steel_montana_Gmaj.wav",  5.25,  5.75, 738.5, "Montana_HighCry_FS5"),
]

WT_SIZE = 2048
wavetables = []
harmonic_dbs = []

for fname, t0, t1, f0_hint, label in wavetable_specs:
    sr, mono = load_mono(fname)
    seg = mono[int(t0*sr):int(t1*sr)]
    nfft = 32768
    win = np.hanning(len(seg))
    spec = np.fft.rfft(seg * win, n=nfft)
    mag = np.abs(spec)
    freqs = np.fft.rfftfreq(nfft, 1.0/sr)

    # Refine f0 around f0_hint (+-4%)
    best_f0 = f0_hint
    best_e = -1.0
    for cand in np.linspace(f0_hint * 0.96, f0_hint * 1.04, 160):
        e = 0.0
        for h in range(1, 9):
            idx = int(round(cand * h / (sr / nfft)))
            if idx < len(mag):
                e += mag[idx] / (h ** 0.25)
        if e > best_e:
            best_e = e
            best_f0 = cand

    # Extract first 20 harmonic amplitudes & phases
    amps = np.zeros(20, dtype=np.float32)
    phases = np.zeros(20, dtype=np.float32)
    for h in range(1, 21):
        fh = best_f0 * h
        if fh >= sr * 0.45:
            break
        idx_lo = max(1, int(round((fh - 14.0) / (sr / nfft))))
        idx_hi = min(len(mag) - 1, int(round((fh + 14.0) / (sr / nfft))))
        pk_idx = idx_lo + np.argmax(mag[idx_lo:idx_hi+1])
        amps[h-1] = mag[pk_idx]
        phases[h-1] = np.angle(spec[pk_idx])

    # Normalize amps so energy is 1, preserve natural harmonic balance
    # Ensure H1 has a healthy floor so low notes never sound thin
    if amps[0] < 0.35 * np.max(amps):
        amps[0] = 0.35 * np.max(amps)
    amps /= (np.linalg.norm(amps) + 1e-12)
    harmonic_dbs.append((label, best_f0, amps.copy()))

    # Synthesize minimum-crest phase-aligned 2048-sample wavetable
    t_phase = np.linspace(0, 2 * np.pi, WT_SIZE, endpoint=False)
    wt = np.zeros(WT_SIZE, dtype=np.float32)
    base_p = phases[0]
    for h in range(1, 21):
        if amps[h-1] > 1e-5:
            rel_p = phases[h-1] - h * base_p
            wt += amps[h-1] * np.sin(h * t_phase + rel_p)
    wt /= (np.max(np.abs(wt)) + 1e-12)
    wavetables.append((label, wt))
    print(f"Extracted WT {label:28s}: f0={best_f0:6.2f}Hz | Top harmonics: " +
          " ".join([f"H{h+1}:{amps[h]:.2f}" for h in range(8)]))

# 2. Extract 8 real fingerpick/thumbpick & bar transient grains (1024 samples @ 44.1kHz = ~23.2ms)
transient_specs = [
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  0.002, "SweetGeorgia_Pick_FS4"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  2.185, "SweetGeorgia_Pick_Mid"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  5.775, "SweetGeorgia_Pick_Bright_B4"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 13.735, "SweetGeorgia_Accent_GS4"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     0.002, "Hedonist_Thumb_A4"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     1.985, "Hedonist_Pick_D5"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     5.340, "Hedonist_LowThumb_C4"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",        7.980, "GritVibey_HighPick_FS5"),
]

TRANS_SIZE = 1024
transients = []
for fname, t0, label in transient_specs:
    sr, mono = load_mono(fname)
    idx0 = int(t0 * sr)
    seg = mono[idx0:idx0 + TRANS_SIZE].copy()
    if len(seg) < TRANS_SIZE:
        seg = np.pad(seg, (0, TRANS_SIZE - len(seg)))
    # High-pass above 180Hz to remove DC/sub thump and apply smooth half-cosine tail fade
    sos = signal.butter(2, 180.0 / (0.5 * sr), btype='highpass', output='sos')
    seg = signal.sosfilt(sos, seg)
    env = np.ones(TRANS_SIZE, dtype=np.float32)
    fade_in = 32
    fade_out = 512
    env[:fade_in] = np.linspace(0, 1, fade_in)
    env[-fade_out:] = 0.5 * (1.0 + np.cos(np.linspace(0, np.pi, fade_out)))
    seg *= env
    seg /= (np.max(np.abs(seg)) + 1e-12)
    transients.append((label, seg))
    print(f"Extracted Transient {label:28s}: peak normalized")

# Write C++ header
with open(out_header, "w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("// Auto-generated from the 4 target Splice Pedal Steel reference WAV files:\n")
    f.write("// 1. CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav\n")
    f.write("// 2. CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav\n")
    f.write("// 3. CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav\n")
    f.write("// 4. CO_MCS_126_pedal_steel_montana_Gmaj.wav\n\n")
    f.write("namespace PedalSteelAssets {\n\n")
    f.write(f"static constexpr int kWavetableSize = {WT_SIZE};\n")
    f.write(f"static constexpr int kNumWavetables = {len(wavetables)};\n")
    f.write(f"static constexpr int kNumHarmonics = 20;\n")
    f.write(f"static constexpr int kTransientSize = {TRANS_SIZE};\n")
    f.write(f"static constexpr int kNumTransients = {len(transients)};\n\n")

    f.write("alignas(16) static constexpr float kHarmonicProfiles[kNumWavetables][kNumHarmonics] = {\n")
    for label, f0, amps in harmonic_dbs:
        vals = ", ".join([f"{v:.6f}f" for v in amps])
        f.write(f"    /* {label} ({f0:.1f}Hz) */ {{ {vals} }},\n")
    f.write("};\n\n")

    f.write("alignas(16) static constexpr float kWavetables[kNumWavetables][kWavetableSize] = {\n")
    for label, wt in wavetables:
        f.write(f"    // {label}\n    {{\n        ")
        for i, v in enumerate(wt):
            f.write(f"{v:.6f}f, ")
            if (i + 1) % 16 == 0 and i + 1 < len(wt):
                f.write("\n        ")
        f.write("\n    },\n")
    f.write("};\n\n")

    f.write("alignas(16) static constexpr float kTransients[kNumTransients][kTransientSize] = {\n")
    for label, tr in transients:
        f.write(f"    // {label}\n    {{\n        ")
        for i, v in enumerate(tr):
            f.write(f"{v:.6f}f, ")
            if (i + 1) % 16 == 0 and i + 1 < len(tr):
                f.write("\n        ")
        f.write("\n    },\n")
    f.write("};\n\n")
    f.write("} // namespace PedalSteelAssets\n")

print(f"\nWrote {out_header} ({os.path.getsize(out_header)/1024:.1f} KB)")
