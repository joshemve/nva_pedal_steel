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

# 15 Style-Matched Studio Pedal Steel Multi-Samples across all 4 Splice Reference WAVs
# (style, filename, t0, t1, midi_note, f0_hint, label)
multisample_specs = [
    # Style 0: Sweet Georgia E9 (CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav)
    (0, "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 14.78, 16.20, 61, 277.18, "MS_0_SG_CS4"),
    (0, "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  2.95,  4.40, 64, 329.63, "MS_1_SG_E4"),
    (0, "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  2.16,  2.84, 66, 369.99, "MS_2_SG_FS4"),
    (0, "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  0.28,  1.65, 68, 415.30, "MS_3_SG_GS4"),
    (0, "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",  5.78,  6.50, 71, 493.88, "MS_4_SG_B4"),

    # Style 1: Hedonist Bootgaze (CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav)
    (1, "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     5.38,  5.86, 60, 261.63, "MS_5_HB_C4"),
    (1, "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     6.68,  7.85, 67, 392.00, "MS_6_HB_G4"),
    (1, "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     2.65,  4.10, 69, 440.00, "MS_7_HB_A4"),
    (1, "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     0.85,  1.85, 71, 493.88, "MS_8_HB_B4"),
    (1, "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",     1.98,  2.55, 74, 587.33, "MS_9_HB_D5"),

    # Style 2: Mikey's Grit Vibey (CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav)
    (2, "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",        2.55,  3.90, 74, 587.33, "MS_10_GV_D5"),
    (2, "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",        8.00,  9.20, 78, 739.99, "MS_11_GV_FS5"),
    (2, "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",        0.02,  0.52, 81, 880.00, "MS_12_GV_A5"),

    # Style 3: Montana Swell (CO_MCS_126_pedal_steel_montana_Gmaj.wav)
    (3, "CO_MCS_126_pedal_steel_montana_Gmaj.wav",          13.35, 14.60, 48, 130.81, "MS_13_MT_C3"),
    (3, "CO_MCS_126_pedal_steel_montana_Gmaj.wav",           0.40,  1.70, 67, 392.00, "MS_14_MT_G4"),
]

TARGET_SR = 28000

extracted_samples = []

for style_id, fname, t0, t1, midi_note, f0_hint, label in multisample_specs:
    sr, mono = load_mono(fname)
    seg = mono[int(t0 * sr):int(t1 * sr)].copy()

    # 1. Track instantaneous phase of fundamental (or H2 when H2 dominates like C4 in Hedonist)
    sos_f1 = signal.butter(2, [f0_hint * 0.86 / (0.5 * sr), f0_hint * 1.14 / (0.5 * sr)], btype='bandpass', output='sos')
    sos_f2 = signal.butter(2, [f0_hint * 1.72 / (0.5 * sr), f0_hint * 2.28 / (0.5 * sr)], btype='bandpass', output='sos')
    band1 = signal.sosfiltfilt(sos_f1, seg)
    band2 = signal.sosfiltfilt(sos_f2, seg)

    if np.sqrt(np.mean(band2**2)) > 1.25 * np.sqrt(np.mean(band1**2)):
        analytic = signal.hilbert(band2)
        inst_phase = np.unwrap(np.angle(analytic)) * 0.5
    else:
        analytic = signal.hilbert(band1)
        inst_phase = np.unwrap(np.angle(analytic))

    win_len = max(31, int(0.025 * sr) | 1)
    inst_freq = np.gradient(inst_phase) * sr / (2.0 * np.pi)
    inst_freq_smooth = signal.savgol_filter(inst_freq, win_len, 2)
    inst_freq_smooth = np.clip(inst_freq_smooth, f0_hint * 0.88, f0_hint * 1.12)
    smooth_phase = np.cumsum(2.0 * np.pi * inst_freq_smooth / sr)

    # 2. Resample onto exact integer-samples-per-period grid so harmonic comb filter isolates ONLY this string's harmonics H1..H28!
    int_period = max(24, int(round(TARGET_SR / f0_hint)))
    exact_sr = int_period * f0_hint
    total_cycles = int((smooth_phase[-1] - smooth_phase[0]) / (2.0 * np.pi))
    num_grid = total_cycles * int_period
    target_phase = smooth_phase[0] + np.arange(num_grid) * (2.0 * np.pi / int_period)
    seg_grid = np.interp(target_phase, smooth_phase, seg).astype(np.float32)

    # Measure early metallic attack spectrum (first ~0.16s) AND sustain spectrum (mid ~0.42s) of this studio string
    n_atk_meas = max(12, min(total_cycles // 3, int(round(0.16 * f0_hint))))
    L_atk_meas = n_atk_meas * int_period
    atk_slice  = seg_grid[:L_atk_meas] * np.hanning(L_atk_meas)
    Spec_atk   = np.fft.rfft(atk_slice) / float(L_atk_meas)

    sus_start_cyc = min(total_cycles // 4, n_atk_meas)
    n_loop_cycles = max(24, min(total_cycles - sus_start_cyc - 1, int(round(0.45 * f0_hint))))
    L_grid = n_loop_cycles * int_period
    raw_sustain = seg_grid[sus_start_cyc * int_period : (sus_start_cyc + n_loop_cycles) * int_period]
    Spec_sus = np.fft.rfft(raw_sustain * np.hanning(L_grid))

    # Build C^infinity periodic sustain loop with up to 28 harmonics + gentle organic steel-dispersion shimmer
    CleanSpec = np.zeros_like(Spec_sus)
    max_h = min(28, int((0.46 * exact_sr) / f0_hint))
    atk_ratios = np.ones(max_h + 1, dtype=np.float32)

    for h in range(1, max_h + 1):
        center_bin = h * n_loop_cycles
        if center_bin >= len(Spec_sus) - 3:
            break
        # Boost upper metallic partials H4..H24 slightly to preserve studio pick/bar wire brilliance
        metal_boost = 1.0 + 0.05 * max(0, min(14, h - 3))
        CleanSpec[center_bin] = Spec_sus[center_bin] * 2.0 * metal_boost
        # Subtle organic steel-dispersion sideband (0.07x -> <0.6 dB natural metallic shimmer, zero warble!)
        if h >= 4:
            CleanSpec[center_bin - 1] = Spec_sus[center_bin - 1] * 0.07
            CleanSpec[center_bin + 1] = Spec_sus[center_bin + 1] * 0.07

        # Ratio of real studio attack harmonic energy vs sustain harmonic energy
        atk_bin = h * n_atk_meas
        if atk_bin < len(Spec_atk):
            a_amp = np.abs(Spec_atk[atk_bin]) * 2.0
            s_amp = (np.abs(Spec_sus[center_bin]) * 2.0 / float(L_grid)) + 1e-7
            atk_ratios[h] = np.clip(a_amp / s_amp, 1.0, 3.2)

    # Synthesize 2-stage phase-locked buffer at TARGET_SR:
    # Attack stage (0 .. loop_start_out, ~0.38s) has real studio harmonic decay H4..H28 + real highpassed pick transient,
    # seamlessly reaching 100.0% phase & amplitude lock with loop_out at loop_start_out!
    scale_to_target = float(TARGET_SR) / float(exact_sr)
    L_out = int(round(L_grid * scale_to_target))
    loop_out = np.fft.irfft(CleanSpec, n=L_out).astype(np.float32) * (float(L_out) / float(L_grid))

    # Build high-harmonic metallic attack transient wave (H3..H28 only) phase-locked to loop_out!
    HighMetalSpec = np.zeros_like(CleanSpec)
    for h in range(3, max_h + 1):
        center_bin = h * n_loop_cycles
        if center_bin >= len(CleanSpec) - 2:
            break
        extra = max(0.0, float(atk_ratios[h]) - 1.0) + 0.35 * min(1.0, float(h - 2) / 8.0)
        HighMetalSpec[center_bin] = CleanSpec[center_bin] * extra

    high_metal_loop = np.fft.irfft(HighMetalSpec, n=L_out).astype(np.float32) * (float(L_out) / float(L_grid))

    # Attack pre-roll length = exact L_out samples (~0.45s) so loop_start_out = L_out is 100% phase-aligned with loop_out[0]!
    loop_start_out = L_out
    t_atk = np.arange(loop_start_out, dtype=np.float32) / float(TARGET_SR)
    # Smooth exponential metallic wire decay that reaches 0.000 before loop_start_out
    win_fade = 0.5 * (1.0 + np.cos(np.pi * np.arange(loop_start_out, dtype=np.float32) / float(loop_start_out)))
    metal_env = np.exp(-t_atk / 0.14) * win_fade

    # Also extract the genuine high-passed (>1.8 kHz) studio fingerpick snap from the first 28 ms of the recording
    raw_28ms_len = min(len(seg_grid), int(0.028 * exact_sr))
    sos_pick = signal.butter(2, min(0.85, 1800.0 / (0.5 * exact_sr)), btype='highpass', output='sos')
    pick_snap = signal.sosfiltfilt(sos_pick, seg_grid[:raw_28ms_len]).astype(np.float32)
    pick_out_len = int(round(raw_28ms_len * scale_to_target))
    pick_out = np.interp(np.linspace(0, 1, pick_out_len), np.linspace(0, 1, raw_28ms_len), pick_snap).astype(np.float32)
    pick_out *= np.hanning(pick_out_len * 2)[pick_out_len:] * 0.45

    atk_out = loop_out + metal_env * high_metal_loop
    atk_out[:pick_out_len] += pick_out

    # Soft 2.5 ms raised-cosine onset ramp on first 70 samples so note-on never clicks
    ramp_len = min(70, len(atk_out) // 4)
    atk_out[:ramp_len] *= 0.5 * (1.0 - np.cos(np.pi * np.arange(ramp_len, dtype=np.float32) / float(ramp_len)))

    final_buf = np.concatenate([atk_out, loop_out])
    final_len = len(final_buf)

    # Normalize by sustain RMS so all 15 multi-samples have matched volume across strings and styles!
    sus_rms = np.sqrt(np.mean(loop_out**2) + 1e-12)
    final_buf = final_buf * (0.24 / sus_rms)
    peak = np.max(np.abs(final_buf)) + 1e-9
    if peak > 0.94:
        final_buf *= (0.94 / peak)

    q = np.clip(np.round(final_buf * 32000.0), -32767, 32767).astype(np.int16)
    wrap_jump = abs(int(q[-1]) - int(q[loop_start_out])) / 32000.0
    seam_jump = abs(int(q[loop_start_out - 1]) - int(q[loop_start_out])) / 32000.0
    extracted_samples.append((style_id, label, midi_note, f0_hint, final_len, loop_start_out, q))
    print(f"Extracted {label:14s}: style={style_id} MIDI={midi_note} f0={f0_hint:6.2f}Hz len={final_len} ({final_len/TARGET_SR:.2f}s) loopStart={loop_start_out} wrapJump={wrap_jump:.5f} seamJump={seam_jump:.5f}")

with open(out_header, "w") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n\n")
    f.write("namespace PedalSteelMultiSamples {\n\n")
    f.write(f"static constexpr int kSampleRate = {TARGET_SR};\n")
    f.write(f"static constexpr int kNumMultiSamples = {len(extracted_samples)};\n\n")
    f.write("struct MultiSample {\n")
    f.write("    int style;\n")
    f.write("    int midiNote;\n")
    f.write("    float baseFreqHz;\n")
    f.write("    int length;\n")
    f.write("    int loopStart;\n")
    f.write("    const int16_t* data;\n")
    f.write("};\n\n")

    for style_id, label, midi_note, f0_hint, length, loop_start, q in extracted_samples:
        f.write(f"static const int16_t {label}[{length}] = {{\n    ")
        vals = [str(int(v)) for v in q]
        for i in range(0, len(vals), 32):
            f.write(",".join(vals[i:i+32]))
            if i + 32 < len(vals):
                f.write(",\n    ")
        f.write("\n};\n\n")

    f.write("static const MultiSample kMultiSamples[kNumMultiSamples] = {\n")
    for style_id, label, midi_note, f0_hint, length, loop_start, q in extracted_samples:
        f.write(f"    {{ {style_id}, {midi_note}, {f0_hint:.2f}f, {length}, {loop_start}, {label} }},\n")
    f.write("};\n\n")
    f.write("} // namespace PedalSteelMultiSamples\n")

print("Wrote", out_header)
