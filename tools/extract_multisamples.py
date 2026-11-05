import os
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
out_header = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelMultiSamples.h"

def load_mono(filename):
    path = os.path.join(ref_dir, filename)
    sr, data = wav.read(path)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    mono = 0.5 * (x[:, 0] + x[:, 1]) if x.ndim == 2 else x
    return sr, mono

# Clean sustained note segments across the E9th range from the 4 reference WAVs
multisample_specs = [
    # (filename, t_start, t_end, target_midi, exact_f0_hint, name)
    ("CO_MCS_126_pedal_steel_montana_Gmaj.wav",         13.35, 14.35, 48, 130.81, "MS_0_C3_Montana"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",    5.35,  5.88, 60, 262.37, "MS_1_C4_Hedonist"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",14.80, 15.80, 61, 276.67, "MS_2_CS4_SweetGeorgia"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",18.80, 19.50, 64, 330.10, "MS_3_E4_SweetGeorgia"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 2.20,  2.80, 66, 370.65, "MS_4_FS4_SweetGeorgia"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 0.32,  1.32, 68, 415.68, "MS_5_GS4_SweetGeorgia"),
    ("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",    0.04,  0.54, 69, 438.19, "MS_6_A4_Hedonist"),
    ("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 5.82,  6.38, 71, 496.73, "MS_7_B4_SweetGeorgia"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",       4.05,  5.05, 74, 587.28, "MS_8_D5_GritVibey"),
    ("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",       8.02,  8.82, 78, 740.00, "MS_9_FS5_GritVibey"),
]

TARGET_SR = 22050
BUF_LEN = 12288 # ~0.557 seconds at 22050 Hz

extracted_samples = []

for fname, t0, t1, midi_note, f0_hint, label in multisample_specs:
    sr, mono = load_mono(fname)
    seg = mono[int(t0*sr):int(t1*sr)].copy()

    # Resample to 22050 Hz
    num_out = int(round(len(seg) * TARGET_SR / sr))
    seg_22k = signal.resample(seg, num_out)

    # Apply harmonic comb bandpass filter around h * f0_hint (h=1..16, bandwidth +-16 Hz)
    # This isolates the pure played pedal steel string + its natural vibrato & shimmer while removing room reverb bleed
    cleaned = np.zeros_like(seg_22k)
    for h in range(1, 17):
        fh = f0_hint * h
        if fh >= TARGET_SR * 0.46:
            break
        bw = min(22.0, max(12.0, fh * 0.025))
        f_lo = max(30.0, fh - bw)
        f_hi = min(TARGET_SR * 0.48, fh + bw)
        sos = signal.butter(2, [f_lo / (0.5 * TARGET_SR), f_hi / (0.5 * TARGET_SR)], btype='bandpass', output='sos')
        cleaned += signal.sosfiltfilt(sos, seg_22k)

    # Normalize amplitude envelope slightly so sustain loop is level and seamless
    # Find period in samples at TARGET_SR
    period = TARGET_SR / f0_hint
    # Ensure we have at least BUF_LEN + 2048 samples by tiling/crossfading if segment is shorter
    if len(cleaned) < BUF_LEN + 2048:
        # Repeat middle section with phase alignment
        mid = cleaned[int(len(cleaned)*0.25):int(len(cleaned)*0.85)]
        n_periods = max(1, int(round(len(mid) / period)))
        aligned_len = int(round(n_periods * period))
        mid = mid[:aligned_len]
        while len(cleaned) < BUF_LEN + 2048:
            cleaned = np.concatenate([cleaned[:int(len(cleaned)*0.75)], mid])

    # Pick loop start and loop end aligned to an exact integer number of fundamental cycles
    loop_start = int(round(32 * period))
    if loop_start > BUF_LEN // 3:
        loop_start = int(round(16 * period))
    loop_cycles = int(round((BUF_LEN - loop_start - int(6 * period)) / period))
    loop_len = int(round(loop_cycles * period))
    loop_end = loop_start + loop_len

    # Crossfade loop_end region with loop_start region over 8 periods so looping from loop_end -> loop_start is 100% click-free
    xfade_len = int(round(8 * period))
    buf = cleaned[:BUF_LEN].copy()
    # Align phase at loop_end to loop_start via local cross-correlation
    search_rad = int(round(period))
    best_lag = 0
    best_corr = -1e18
    ref_win = buf[loop_start:loop_start + xfade_len]
    for lag in range(-search_rad, search_rad + 1):
        cand_idx = loop_end + lag
        if cand_idx - xfade_len >= loop_start and cand_idx + xfade_len < len(cleaned):
            cand_win = cleaned[cand_idx:cand_idx + xfade_len]
            c = np.sum(ref_win * cand_win)
            if c > best_corr:
                best_corr = c
                best_lag = lag
    loop_end += best_lag

    # Perform equal-linear phase-locked crossfade right before loop_end so buf[loop_end - xfade_len : loop_end] blends into cleaned[loop_start - xfade_len : loop_start]
    alpha = np.linspace(0.0, 1.0, xfade_len, endpoint=False)
    buf[loop_end - xfade_len : loop_end] = (
        (1.0 - alpha) * cleaned[loop_end - xfade_len : loop_end] +
        alpha * buf[loop_start - xfade_len : loop_start]
    )

    # Smooth 3ms attack fade-in
    atk = int(0.003 * TARGET_SR)
    buf[:atk] *= np.linspace(0.0, 1.0, atk)
    buf /= (np.max(np.abs(buf[:loop_end])) + 1e-12)
    buf_i16 = np.clip(np.round(buf[:loop_end] * 32000.0), -32767, 32767).astype(np.int16)

    # Check loop discontinuity
    disc = abs(float(buf_i16[-1]) - float(buf_i16[loop_start])) / 32000.0
    print(f"Extracted {label:25s}: f0={f0_hint:6.2f}Hz | len={len(buf_i16)} samples ({len(buf_i16)/TARGET_SR:.3f}s) | loopStart={loop_start} | loopStepDisc={disc:.4f}")
    extracted_samples.append((label, midi_note, f0_hint, loop_start, len(buf_i16), buf_i16))

with open(out_header, "w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n\n")
    f.write("// Real harmonic-isolated Pedal Steel multi-samples extracted from the 4 Splice reference WAVs\n")
    f.write("namespace PedalSteelMultiSamples {\n\n")
    f.write(f"static constexpr double kSampleRate = {TARGET_SR}.0;\n")
    f.write(f"static constexpr int kNumMultiSamples = {len(extracted_samples)};\n\n")
    f.write("struct MultiSampleInfo {\n")
    f.write("    int midiNote;\n")
    f.write("    float baseFreqHz;\n")
    f.write("    int loopStart;\n")
    f.write("    int length;\n")
    f.write("    const int16_t* data;\n")
    f.write("};\n\n")

    for label, midi_note, f0, loop_start, length, data_i16 in extracted_samples:
        f.write(f"alignas(16) static constexpr int16_t {label}_data[{length}] = {{\n    ")
        for i, v in enumerate(data_i16):
            f.write(f"{int(v)}, ")
            if (i + 1) % 24 == 0 and i + 1 < len(data_i16):
                f.write("\n    ")
        f.write("\n};\n\n")

    f.write("static constexpr MultiSampleInfo kMultiSamples[kNumMultiSamples] = {\n")
    for label, midi_note, f0, loop_start, length, _ in extracted_samples:
        f.write(f"    {{ {midi_note}, {f0:.3f}f, {loop_start}, {length}, {label}_data }},\n")
    f.write("};\n\n")
    f.write("} // namespace PedalSteelMultiSamples\n")

print(f"\nWrote {out_header} ({os.path.getsize(out_header)/1024:.1f} KB)")
