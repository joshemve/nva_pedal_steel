import os
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
out_header = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelMultiSamples.h"
out_cpp    = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelStudioBank.cpp"

ref_files = [
    "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",
    "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",
    "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",
    "CO_MCS_126_pedal_steel_montana_Gmaj.wav",
]

TARGET_SR = 44100
FRAMES_PER_STYLE = int(11.55 * TARGET_SR) # 509,355 stereo frames per style (44.1 kHz stereo)

style_stereo = []
style_peaks  = []

for st, fname in enumerate(ref_files):
    path = os.path.join(ref_dir, fname)
    sr, data = wav.read(path)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    if x.ndim == 1:
        x = np.column_stack([x, x])
    if len(x) < FRAMES_PER_STYLE:
        pad = np.zeros((FRAMES_PER_STYLE - len(x), 2), dtype=np.float32)
        x = np.vstack([x, pad])
    else:
        x = x[:FRAMES_PER_STYLE].copy()

    # Smooth 8 ms fade-in at frame 0 and 15 ms fade-out at end of 11.55s bank
    f_in = int(0.008 * TARGET_SR)
    f_out = int(0.015 * TARGET_SR)
    x[:f_in, 0] *= np.linspace(0.0, 1.0, f_in, dtype=np.float32)
    x[:f_in, 1] *= np.linspace(0.0, 1.0, f_in, dtype=np.float32)
    x[-f_out:, 0] *= np.linspace(1.0, 0.0, f_out, dtype=np.float32)
    x[-f_out:, 1] *= np.linspace(1.0, 0.0, f_out, dtype=np.float32)

    q = np.clip(np.round(x * 32767.0), -32767, 32767).astype(np.int16)
    style_stereo.append(q)
    print(f"Loaded Style {st} ({fname}): {len(q)} stereo frames @ {sr} Hz, peak={np.max(np.abs(x)):.4f}")

# 19 Zero-Crossing Phase-Locked Single-Note Chromatic/Copedent Multi-Sample Zones inside the 44.1 kHz Stereo Studio Bank
# (style, t_onset, t_loop_start, t_loop_end, midi_note, f0_hz, label)
zone_specs = [
    # Style 0: Sweet Georgia E9
    (0, 0.015, 0.48, 1.85, 68, 415.30, "SG_GS4"),
    (0, 2.165, 2.32, 2.80, 66, 369.99, "SG_FS4"),
    (0, 2.850, 3.25, 5.10, 64, 329.63, "SG_E4"),
    (0, 5.775, 5.92, 6.46, 71, 493.88, "SG_B4"),
    # Style 1: Hedonist Bootgaze
    (1, 0.015, 0.14, 0.52, 69, 440.00, "HB_A4_Pick"),
    (1, 0.560, 0.88, 1.86, 71, 493.88, "HB_B4"),
    (1, 1.975, 2.08, 2.48, 74, 587.33, "HB_D5"),
    (1, 2.600, 3.05, 4.85, 69, 440.00, "HB_A4_Long"),
    (1, 5.360, 5.46, 5.84, 60, 261.63, "HB_C4"),
    (1, 5.880, 6.08, 6.62, 62, 293.66, "HB_D4"),
    (1, 6.690, 6.88, 7.82, 67, 392.00, "HB_G4"),
    (1, 10.01, 10.12, 10.66, 76, 659.26, "HB_E5"),
    (1, 10.85, 10.95, 11.42, 79, 783.99, "HB_G5"),
    # Style 2: Mikey's Grit Vibey
    (2, 0.015, 0.10, 0.50, 81, 880.00, "GV_A5"),
    (2, 2.460, 2.75, 4.30, 74, 587.33, "GV_D5"),
    (2, 7.980, 8.20, 9.35, 78, 739.99, "GV_FS5"),
    # Style 3: Montana Swell
    (3, 0.200, 0.85, 2.15, 67, 392.00, "MT_G4"),
    (3, 4.000, 4.65, 6.05, 74, 587.33, "MT_D5"),
    (3, 7.800, 8.45, 9.85, 76, 659.26, "MT_E5"),
]

zones_out = []
for st, t0, tls, tle, midi, f0, name in zone_specs:
    q = style_stereo[st]
    mono = 0.5 * (q[:, 0].astype(np.float32) + q[:, 1].astype(np.float32)) / 32767.0
    s0  = int(round(t0 * TARGET_SR))
    sls = int(round(tls * TARGET_SR))
    sle = int(round(tle * TARGET_SR))

    sos = signal.butter(2, [f0 * 0.85 / (0.5 * TARGET_SR), f0 * 1.15 / (0.5 * TARGET_SR)], btype='bandpass', output='sos')
    bp = signal.sosfiltfilt(sos, mono[s0 : min(len(mono), sle + 1000)])
    zc = np.where((bp[:-1] <= 0) & (bp[1:] > 0))[0] + s0
    sls_snap = int(zc[np.argmin(np.abs(zc - sls))])
    sle_snap = int(zc[np.argmin(np.abs(zc - sle))])
    period = TARGET_SR / f0
    xfade_frames = int(round(12.0 * period))

    # Measure RMS in loop so single-note voices have balanced level
    loop_rms = float(np.sqrt(np.mean(mono[sls_snap:sle_snap]**2) + 1e-12))
    norm_gain = float(np.clip(0.18 / loop_rms, 0.45, 2.40))
    zones_out.append((st, midi, f0, s0, sls_snap, sle_snap, xfade_frames, norm_gain, name))
    print(f"Zone {name:12s}: style={st} MIDI={midi} start={s0} loop=[{sls_snap}..{sle_snap}] xfade={xfade_frames} gain={norm_gain:.2f}")

# 14 Studio Phrase Articulation Slices covering the 4 Reference Performances
# (style, phrase_index, midi_trigger_note, start_sec, end_sec, base_freq_hz, name)
phrase_specs = [
    # Style 0: Sweet Georgia E9 (CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav)
    (0, 0, 66, 0.00,  2.15, 415.30, "PH_SG_0_FS4_GS4_Pull"),
    (0, 1, 64, 2.15,  5.68, 329.63, "PH_SG_1_FS4_E4_Slide"),
    (0, 2, 71, 5.68,  9.92, 493.88, "PH_SG_2_B4_Oblique_Dyad"),
    (0, 3, 61, 9.92, 11.50, 329.63, "PH_SG_3_E4_FS4_Pickup"),

    # Style 1: Hedonist Bootgaze (CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav)
    (1, 0, 69, 0.00,  1.95, 493.88, "PH_HB_0_A4_B4_Pull"),
    (1, 1, 74, 1.95,  5.32, 440.00, "PH_HB_1_D5_A4_Gliss"),
    (1, 2, 60, 5.32,  7.94, 392.00, "PH_HB_2_C4_D4_G4_Chime"),
    (1, 3, 71, 7.94, 11.50, 493.88, "PH_HB_3_A4_B4_E5_G5"),

    # Style 2: Mikey's Grit Vibey (CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav)
    (2, 0, 81, 0.00,  4.88, 587.33, "PH_GV_0_A5_DMaj_BarGliss1"),
    (2, 1, 78, 4.88,  7.92, 587.33, "PH_GV_1_FS5_DMaj_BarGliss2"),
    (2, 2, 74, 7.92, 11.50, 739.99, "PH_GV_2_FS5_GritLead_Tail"),

    # Style 3: Montana Volume Swell (CO_MCS_126_pedal_steel_montana_Gmaj.wav)
    (3, 0, 67, 0.00,  3.80, 392.00, "PH_MT_0_GMaj_Swell"),
    (3, 1, 74, 3.80,  7.60, 587.33, "PH_MT_1_DMaj_Swell"),
    (3, 2, 76, 7.60, 11.50, 659.26, "PH_MT_2_Em_CMaj_Swell"),
]

# Pack all 4 styles of interleaved stereo int16_t (L, R) into a single flat int16_t array stored as uint64_t literals
all_int16 = np.concatenate([q.reshape(-1) for q in style_stereo])
if len(all_int16) % 4 != 0:
    pad_n = 4 - (len(all_int16) % 4)
    all_int16 = np.concatenate([all_int16, np.zeros(pad_n, dtype=np.int16)])

u64_view = all_int16.view(np.uint64)
print(f"Total interleaved int16 samples: {len(all_int16)} ({len(u64_view)} uint64 words = {len(all_int16)*2/1024/1024:.2f} MB)")

with open(out_header, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n\n")
    f.write("namespace PedalSteelMultiSamples {\n\n")
    f.write(f"static constexpr int kSampleRate = {TARGET_SR};\n")
    f.write(f"static constexpr int kFramesPerStyle = {FRAMES_PER_STYLE};\n")
    f.write(f"static constexpr int kNumStyles = 4;\n")
    f.write(f"static constexpr int kNumMultiSamples = {len(zones_out)};\n")
    f.write(f"static constexpr int kNumPhraseSlices = {len(phrase_specs)};\n\n")

    f.write("struct MultiSample {\n")
    f.write("    int style;\n")
    f.write("    int midiNote;\n")
    f.write("    float baseFreqHz;\n")
    f.write("    int startFrame;\n")
    f.write("    int loopStartFrame;\n")
    f.write("    int loopEndFrame;\n")
    f.write("    int xfadeFrames;\n")
    f.write("    float normGain;\n")
    f.write("};\n\n")

    f.write("struct PhraseSlice {\n")
    f.write("    int style;\n")
    f.write("    int phraseIndex;\n")
    f.write("    int triggerMidiNote;\n")
    f.write("    int startFrame;\n")
    f.write("    int endFrame;\n")
    f.write("    float baseFreqHz;\n")
    f.write("};\n\n")

    f.write("extern const uint64_t kStudioStereoPacked[];\n\n")
    f.write("inline const int16_t* getStyleStereoData(int style) {\n")
    f.write("    const int s = (style < 0) ? 0 : ((style >= kNumStyles) ? (kNumStyles - 1) : style);\n")
    f.write("    return reinterpret_cast<const int16_t*>(kStudioStereoPacked) + static_cast<size_t>(s) * static_cast<size_t>(kFramesPerStyle) * 2;\n")
    f.write("}\n\n")

    f.write("static constexpr MultiSample kMultiSamples[kNumMultiSamples] = {\n")
    for st, midi, f0, s0, sls, sle, xf, ng, name in zones_out:
        f.write(f"    {{ {st}, {midi}, {f0:.2f}f, {s0}, {sls}, {sle}, {xf}, {ng:.4f}f }}, // {name}\n")
    f.write("};\n\n")

    f.write("static constexpr PhraseSlice kPhraseSlices[kNumPhraseSlices] = {\n")
    for st, pidx, trig, t0, t1, f0, name in phrase_specs:
        s0 = int(round(t0 * TARGET_SR))
        s1 = FRAMES_PER_STYLE
        f.write(f"    {{ {st}, {pidx}, {trig}, {s0}, {s1}, {f0:.2f}f }}, // {name}\n")
    f.write("};\n\n")

    f.write("} // namespace PedalSteelMultiSamples\n")

with open(out_cpp, "w") as f:
    f.write('#include "PedalSteelMultiSamples.h"\n\n')
    f.write("namespace PedalSteelMultiSamples {\n\n")
    f.write(f"alignas(8) const uint64_t kStudioStereoPacked[{len(u64_view)}] = {{\n")
    chunk = 16
    hex_strs = [f"0x{int(v):016x}ULL" for v in u64_view]
    lines = [",".join(hex_strs[i:i+chunk]) for i in range(0, len(hex_strs), chunk)]
    f.write(",\n".join(lines))
    f.write("\n};\n\n")
    f.write("} // namespace PedalSteelMultiSamples\n")

print("Wrote", out_header, "and", out_cpp)
