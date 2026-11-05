import os
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir    = r"C:\Users\Joshua\Documents\pedal_steel\references"
out_header = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelMultiSamples.h"
out_cpp    = r"C:\Users\Joshua\Documents\pedal_steel\Source\dsp\PedalSteelStudioBank.cpp"

ref_files = [
    "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav",
    "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav",
    "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav",
    "CO_MCS_126_pedal_steel_montana_Gmaj.wav",
]

TARGET_SR = 44100

style_float  = []
style_stereo = []
style_offsets = []
style_lengths = []

cur_phrase_frame = 0
for st, fname in enumerate(ref_files):
    path = os.path.join(ref_dir, fname)
    sr, data = wav.read(path)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    if x.ndim == 1:
        x = np.column_stack([x, x])

    # Smooth 6 ms fade-in at frame 0 and 15 ms fade-out at end of full studio file
    f_in  = int(0.006 * TARGET_SR)
    f_out = int(0.015 * TARGET_SR)
    x[:f_in, 0]  *= np.linspace(0.0, 1.0, f_in, dtype=np.float32)
    x[:f_in, 1]  *= np.linspace(0.0, 1.0, f_in, dtype=np.float32)
    x[-f_out:, 0] *= np.linspace(1.0, 0.0, f_out, dtype=np.float32)
    x[-f_out:, 1] *= np.linspace(1.0, 0.0, f_out, dtype=np.float32)

    q = np.clip(np.round(x * 32767.0), -32767, 32767).astype(np.int16)
    style_float.append(x)
    style_stereo.append(q)
    style_offsets.append(cur_phrase_frame)
    style_lengths.append(len(q))
    cur_phrase_frame += len(q)
    print(f"Loaded Full Style {st} ({fname}): {len(q)} stereo frames ({len(q)/sr:.2f}s) @ {sr} Hz")


def comb_isolate_single_string(seg_st: np.ndarray, sr: int, f0_nom: float, do_comb: bool, keep_atk_sec: float, ls_idx: int, le_idx: int):
    """
    Measures exact concert fundamental f0 inside the steady loop segment (to < 0.05 cents)
    and optionally applies a wideband raised-cosine harmonic passband comb (+/-24 Hz flat-top)
    that removes companion chord thirds/fifths while preserving 100% of stereo plate modulation,
    natural vibrato, string decay, and raw pick attack.
    """
    n = len(seg_st)
    mono = 0.5 * (seg_st[:, 0] + seg_st[:, 1])
    loop_slice = mono[max(0, ls_idx):min(n, le_idx)]
    if len(loop_slice) < 256:
        loop_slice = mono

    nfft_f0 = int(2 ** np.ceil(np.log2(max(32768, len(loop_slice) * 4))))
    df_f0 = sr / nfft_f0
    S = np.abs(np.fft.rfft(loop_slice * np.hanning(len(loop_slice)), n=nfft_f0))

    lo_f = f0_nom * (2.0 ** (-0.45 / 12.0))
    hi_f = f0_nom * (2.0 ** ( 0.45 / 12.0))
    f_grid = np.linspace(lo_f, hi_f, 800)
    best_f0 = f0_nom
    best_val = -1.0
    for fg in f_grid:
        val = 0.0
        for h in range(1, 5):
            idx = int(round(h * fg / df_f0))
            if 1 <= idx < len(S) - 1:
                val += (S[idx] ** 2) / h
        if val > best_val:
            best_val = val
            best_f0 = float(fg)

    if not do_comb:
        return seg_st.copy(), best_f0

    nfft = int(2 ** np.ceil(np.log2(max(16384, n * 2))))
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)

    mask = np.full(len(freqs), 0.015, dtype=np.float32)  # -36.5 dB floor preserves subtle studio room air without chord bleed
    max_h = int(min(30, (sr * 0.46) // best_f0))
    for h in range(1, max_h + 1):
        fc = h * best_f0
        bw_flat  = max(24.0, fc * 0.018)
        bw_skirt = max(18.0, fc * 0.014)
        dist = np.abs(freqs - fc)
        in_flat  = dist <= bw_flat
        in_skirt = (dist > bw_flat) & (dist <= bw_flat + bw_skirt)
        mask[in_flat] = 1.0
        if np.any(in_skirt):
            t = (dist[in_skirt] - bw_flat) / bw_skirt
            m_val = 0.015 + 0.985 * 0.5 * (1.0 + np.cos(np.pi * t))
            mask[in_skirt] = np.maximum(mask[in_skirt], m_val.astype(np.float32))

    mask[freqs < 0.82 * best_f0] = 0.004

    out = np.zeros_like(seg_st)
    for ch in range(2):
        X = np.fft.rfft(seg_st[:, ch], n=nfft)
        out[:, ch] = np.fft.irfft(X * mask, n=nfft)[:n]

    if keep_atk_sec > 0.002:
        na = min(n, int(keep_atk_sec * sr))
        w = (0.5 * (1.0 + np.cos(np.pi * np.arange(na) / na)))[:, None].astype(np.float32)
        out[:na] = w * seg_st[:na] + (1.0 - w) * out[:na]

    return out, best_f0


# 26 Pure Concert-Calibrated Single-String Stereo Multi-Sample Zones across all 4 Styles (MIDI 55..81)
# (style, t_onset, t_loop_start, t_loop_end, midi_note, vel_layer, do_comb, label)
# vel_layer: 0 = any/medium, 1 = high-velocity accent variant
zone_specs = [
    # Style 0: Sweet Georgia E9 (Flyover Country, 23.13s)
    (0, 14.76, 14.92, 16.58, 61, 0, True,  "SG_CS4"),
    (0,  3.06,  3.22,  4.92, 64, 0, True,  "SG_E4"),
    (0,  2.06,  2.20,  2.68, 66, 0, True,  "SG_FS4"),
    (0,  0.04,  0.46,  1.70, 68, 0, True,  "SG_GS4"),
    (0,  7.18,  7.34,  7.86, 68, 1, True,  "SG_GS4_Accent"),
    (0, 21.52, 21.66, 22.64, 71, 0, True,  "SG_B4"),
    (0, 17.18, 17.32, 17.82, 76, 0, True,  "SG_E5"),

    # Style 1: Hedonist Bootgaze (Indie Americana, 21.33s)
    (1,  7.88,  7.98,  8.28, 57, 0, True,  "HB_A3"),
    (1,  5.18,  5.32,  5.72, 60, 0, True,  "HB_C4"),
    (1,  6.70,  6.86,  7.46, 67, 0, True,  "HB_G4"),
    (1,  2.86,  3.02,  3.48, 69, 0, True,  "HB_A4"),
    (1,  8.95,  9.08,  9.60, 71, 0, True,  "HB_B4"),
    (1, 19.12, 19.24, 19.72, 74, 0, True,  "HB_D5"),
    (1, 14.90, 15.05, 15.78, 79, 0, True,  "HB_G5"),

    # Style 2: Mikey's Grit Vibey (Tweed Breakup, 16.00s)
    (2,  9.52,  9.64, 10.12, 71, 0, True,  "GV_B4"),
    (2,  3.45,  3.62,  4.58, 74, 0, True,  "GV_D5"),
    (2,  0.42,  0.56,  1.14, 78, 1, True,  "GV_FS5_Accent"),
    (2,  7.98,  8.14,  8.90, 78, 0, False, "GV_FS5"),
    (2, 10.62, 10.76, 11.66, 81, 1, True,  "GV_A5_Grit"),
    (2, 13.68, 13.82, 14.76, 81, 0, True,  "GV_A5"),

    # Style 3: Montana Lush Swell (Goodrich Pad, 15.24s)
    (3,  0.95,  1.15,  2.20, 55, 0, True,  "MT_G3"),
    (3,  1.00,  1.20,  2.25, 62, 0, True,  "MT_D4"),
    (3,  8.85,  9.05, 10.10, 64, 0, True,  "MT_E4"),
    (3,  1.05,  1.25,  2.25, 67, 0, True,  "MT_G4"),
    (3, 12.20, 12.40, 13.45, 72, 0, True,  "MT_C5"),
    (3, 12.65, 12.85, 13.85, 76, 0, True,  "MT_E5"),
]

single_note_chunks = []
zones_out = []
cur_ms_frame = 0

for st, t0, tls, tle, midi, vel_layer, do_comb, name in zone_specs:
    x_full = style_float[st]
    i0  = max(0, int(round(t0 * TARGET_SR)))
    i1  = min(len(x_full), int(round((tle + 0.05) * TARGET_SR)))
    seg = x_full[i0:i1].copy()

    rel_ls = int(round((tls - t0) * TARGET_SR))
    rel_le = int(round((tle - t0) * TARGET_SR))
    keep_atk = 0.004 if st == 3 else 0.030
    iso_st, f0_exact = comb_isolate_single_string(seg, TARGET_SR, 440.0 * (2.0 ** ((midi - 69.0) / 12.0)), do_comb, keep_atk, rel_ls, rel_le)

    # Smooth 3 ms onset & tail guard on isolated single-note clip
    f_edge = min(len(iso_st) // 4, int(0.003 * TARGET_SR))
    iso_st[:f_edge]  *= np.linspace(0.0, 1.0, f_edge, dtype=np.float32)[:, None]
    iso_st[-f_edge:] *= np.linspace(1.0, 0.0, f_edge, dtype=np.float32)[:, None]

    mono = 0.5 * (iso_st[:, 0] + iso_st[:, 1])

    sos = signal.butter(2, [f0_exact * 0.85 / (0.5 * TARGET_SR), f0_exact * 1.15 / (0.5 * TARGET_SR)], btype='bandpass', output='sos')
    bp  = signal.sosfiltfilt(sos, mono)
    zc  = np.where((bp[:-1] <= 0.0) & (bp[1:] > 0.0))[0]
    sls_rel = int(zc[np.argmin(np.abs(zc - rel_ls))])
    sle_rel = int(zc[np.argmin(np.abs(zc - rel_le))])
    if sle_rel <= sls_rel + 256:
        sle_rel = min(len(iso_st) - 16, sls_rel + int(0.35 * TARGET_SR))

    period = TARGET_SR / f0_exact
    xfade_frames = int(round(16.0 * period))

    # Level slow decay slope across the ENTIRE [sls_rel - xfade_frames .. sle_rel] loop+crossfade buffer,
    # while blending smoothly from 1.0 BEFORE (sls_rel - xfade_frames) so the crossfade pre-roll has 0.0 dB step!
    xf_safe   = min(xfade_frames, sls_rel // 2)
    xfade_frames = max(32, xf_safe)
    pre_blend = min(xfade_frames, sls_rel - xfade_frames)
    reg_s = sls_rel - xfade_frames - pre_blend
    reg_e = sle_rel
    if reg_e - reg_s > 512:
        reg_mono = 0.5 * (iso_st[reg_s:reg_e, 0] + iso_st[reg_s:reg_e, 1])
        env_win = min(int(0.11 * TARGET_SR), max(384, (sle_rel - sls_rel) // 4))
        ker = np.hanning(env_win)
        ker /= np.sum(ker)
        pwr = np.convolve(np.pad(reg_mono ** 2, (env_win // 2, env_win // 2), mode='edge'), ker, mode='same')[env_win // 2 : env_win // 2 + len(reg_mono)]
        slow_env = np.sqrt(pwr + 1e-10)
        target_env = float(np.median(slow_env[pre_blend:]))
        comp = np.clip(target_env / slow_env, 0.30, 3.20).astype(np.float32)
        if pre_blend > 16:
            ramp = 0.5 * (1.0 - np.cos(np.pi * np.linspace(0.0, 1.0, pre_blend, dtype=np.float32)))
            comp[:pre_blend] = 1.0 + ramp * (comp[:pre_blend] - 1.0)
        iso_st[reg_s:reg_e] *= comp[:, None]
        mono = 0.5 * (iso_st[:, 0] + iso_st[:, 1])

    loop_rms  = float(np.sqrt(np.mean(mono[sls_rel:sle_rel] ** 2) + 1e-12))
    pre_gain  = float(np.clip(0.165 / loop_rms, 0.45, 4.80))
    iso_st   *= pre_gain
    peak_val  = float(np.max(np.abs(iso_st)) + 1e-12)
    if peak_val > 0.92:
        iso_st *= (0.92 / peak_val)
    final_rms = float(np.sqrt(np.mean((0.5 * (iso_st[sls_rel:sle_rel, 0] + iso_st[sls_rel:sle_rel, 1])) ** 2) + 1e-12))
    norm_gain = float(np.clip(0.165 / final_rms, 0.70, 1.50))

    q_iso = np.clip(np.round(iso_st * 32767.0), -32767, 32767).astype(np.int16)
    start_f = cur_ms_frame
    ls_f    = cur_ms_frame + sls_rel
    le_f    = cur_ms_frame + sle_rel
    end_f   = cur_ms_frame + len(q_iso)
    cur_ms_frame += len(q_iso)
    single_note_chunks.append(q_iso)

    cents_off = 1200.0 * np.log2(f0_exact / (440.0 * (2.0 ** ((midi - 69.0) / 12.0))))
    zones_out.append((st, midi, vel_layer, f0_exact, start_f, ls_f, le_f, end_f, xfade_frames, norm_gain, name))
    print(f"Zone {name:14s}: style={st} MIDI={midi:2d} f0={f0_exact:6.2f}Hz ({cents_off:+5.1f}c) loop={(sle_rel-sls_rel)/TARGET_SR:.2f}s gain={norm_gain:.2f}")

ms_all = np.vstack(single_note_chunks)
phrase_all = np.vstack(style_stereo)
print(f"Total Phrase Stereo Frames : {len(phrase_all)} ({len(phrase_all)/TARGET_SR:.2f}s)")
print(f"Total Single-Note Frames   : {len(ms_all)} ({len(ms_all)/TARGET_SR:.2f}s)")

# 20 Studio Phrase Articulation Slices (5 per style: Lick 1, Lick 2, Lick 3, Lick 4, and Full Loop!)
# Triggered via Channel 1 Low Keyswitches (C1=36, D1=38, E1=40, F1=41, G1=43), Play Mode 4 ("Studio Phrase Licks"), UI "PLAY LICK", or MIDI Ch 2!
# (style, slot_idx 0..4, keyswitch_midi, root_midi, ref_bpm, start_sec, end_sec, name)
phrase_specs = [
    # Style 0: Sweet Georgia E9 (83 BPM, E Major, root E4=64 / G#4=68)
    (0, 0, 36, 68,  83.0,  0.00,  2.84, "PH_SG_1_B_Pedal_Cry"),
    (0, 1, 38, 64,  83.0,  2.84,  5.72, "PH_SG_2_Weeping_Slide"),
    (0, 2, 40, 71,  83.0,  5.72, 11.48, "PH_SG_3_Harmonic_Chime"),
    (0, 3, 41, 64,  83.0, 11.48, 23.05, "PH_SG_4_Verse_Turnaround"),
    (0, 4, 43, 66,  83.0,  0.00, 23.05, "PH_SG_Full_Performance"),

    # Style 1: Hedonist Bootgaze (90 BPM, G Major, root G4=67 / A4=69)
    (1, 0, 36, 69,  90.0,  0.00,  1.95, "PH_HB_1_Tube_Pedal_Pull"),
    (1, 1, 38, 74,  90.0,  1.95,  5.25, "PH_HB_2_Soaring_Gliss"),
    (1, 2, 40, 60,  90.0,  5.25, 11.45, "PH_HB_3_Octave_Arpeggio"),
    (1, 3, 41, 71,  90.0, 12.70, 21.20, "PH_HB_4_Shimmer_Climax"),
    (1, 4, 43, 69,  90.0,  0.00, 21.20, "PH_HB_Full_Performance"),

    # Style 2: Mikey's Grit Tweed (120 BPM, D Major, root D5=74 / A5=81)
    (2, 0, 36, 78, 120.0,  0.00,  4.88, "PH_GV_1_Tweed_Crying_Lead"),
    (2, 1, 38, 74, 120.0,  4.88,  7.92, "PH_GV_2_Open_D_Resonance"),
    (2, 2, 40, 78, 120.0,  7.92, 11.95, "PH_GV_3_DoubleStop_Grit"),
    (2, 3, 41, 81, 120.0, 11.95, 15.92, "PH_GV_4_High_Bar_Finale"),
    (2, 4, 43, 81, 120.0,  0.00, 15.92, "PH_GV_Full_Performance"),

    # Style 3: Montana Lush Swell (126 BPM, G Major, root G4=67)
    (3, 0, 36, 67, 126.0,  0.00,  3.81, "PH_MT_1_GMaj_Swell"),
    (3, 1, 38, 74, 126.0,  3.81,  7.62, "PH_MT_2_DMaj_Swell"),
    (3, 2, 40, 76, 126.0,  7.62, 11.43, "PH_MT_3_Em_Swell"),
    (3, 3, 41, 72, 126.0, 11.43, 15.18, "PH_MT_4_CMaj_Swell"),
    (3, 4, 43, 67, 126.0,  0.00, 15.18, "PH_MT_Full_Performance"),
]

# Write Header
with open(out_header, "w", encoding="utf-8") as f:
    f.write("#pragma once\n")
    f.write("#include <cstdint>\n\n")
    f.write("// Auto-generated 44.1 kHz 16-Bit Stereo Studio Pedal-Steel Sample & Phrase Bank\n")
    f.write("namespace PedalSteelMultiSamples {\n\n")
    f.write(f"static constexpr double kSampleRate = {TARGET_SR}.0;\n")
    f.write(f"static constexpr int kTotalPhraseFrames = {len(phrase_all)};\n")
    f.write(f"static constexpr int kTotalMultiSampleFrames = {len(ms_all)};\n")
    f.write(f"static constexpr int kNumMultiSamples = {len(zones_out)};\n")
    f.write(f"static constexpr int kNumPhraseSlices = {len(phrase_specs)};\n\n")

    f.write("struct MultiSampleZone {\n")
    f.write("    int style;\n")
    f.write("    int midiNote;\n")
    f.write("    int velLayer;\n")
    f.write("    float baseFreqHz;\n")
    f.write("    int startFrame;\n")
    f.write("    int loopStartFrame;\n")
    f.write("    int loopEndFrame;\n")
    f.write("    int endFrame;\n")
    f.write("    int xfadeFrames;\n")
    f.write("    float normGain;\n")
    f.write("    const char* name;\n")
    f.write("};\n\n")

    f.write("struct PhraseSlice {\n")
    f.write("    int style;\n")
    f.write("    int slotIndex;\n")
    f.write("    int keyswitchMidi;\n")
    f.write("    int rootMidiNote;\n")
    f.write("    float referenceBpm;\n")
    f.write("    int startFrame;\n")
    f.write("    int endFrame;\n")
    f.write("    const char* name;\n")
    f.write("};\n\n")

    f.write("const int16_t* getStyleStereoData(int styleIndex) noexcept;\n")
    f.write("int getStyleStereoFrames(int styleIndex) noexcept;\n")
    f.write("const int16_t* getSingleNoteStereoData() noexcept;\n\n")

    f.write(f"static constexpr MultiSampleZone kMultiSamples[{len(zones_out)}] = {{\n")
    for st, midi, vlay, f0, s0, sls, sle, send, xf, ng, name in zones_out:
        f.write(f"    {{ {st}, {midi}, {vlay}, {f0:.3f}f, {s0}, {sls}, {sle}, {send}, {xf}, {ng:.4f}f, \"{name}\" }},\n")
    f.write("};\n\n")

    f.write(f"static constexpr PhraseSlice kPhraseSlices[{len(phrase_specs)}] = {{\n")
    for st, slot, ks, rmidi, rbpm, ts, te, name in phrase_specs:
        max_f = style_lengths[st]
        sf = min(max_f - 1, int(round(ts * TARGET_SR)))
        ef = min(max_f, int(round(te * TARGET_SR)))
        f.write(f"    {{ {st}, {slot}, {ks}, {rmidi}, {rbpm:.1f}f, {sf}, {ef}, \"{name}\" }},\n")
    f.write("};\n\n")
    f.write("} // namespace PedalSteelMultiSamples\n")


def write_u64_array(fp, arr_i16: np.ndarray, symbol_name: str):
    raw_bytes = arr_i16.tobytes()
    pad_rem = (-len(raw_bytes)) % 8
    if pad_rem:
        raw_bytes += b"\x00" * pad_rem
    u64_arr = np.frombuffer(raw_bytes, dtype=np.uint64)
    fp.write(f"alignas(8) static const uint64_t {symbol_name}[{len(u64_arr)}] = {{\n")
    chunk = 16
    lines = []
    for i in range(0, len(u64_arr), chunk):
        part = u64_arr[i : i + chunk]
        lines.append(",".join(f"0x{int(v):x}ULL" for v in part) + ",\n")
        if len(lines) >= 4096:
            fp.writelines(lines)
            lines.clear()
    if lines:
        fp.writelines(lines)
    fp.write("};\n\n")
    return len(u64_arr) * 8


with open(out_cpp, "w", encoding="utf-8") as f:
    f.write('#include "PedalSteelMultiSamples.h"\n\n')
    f.write("namespace PedalSteelMultiSamples {\n\n")
    b1 = write_u64_array(f, phrase_all, "kPhraseBankPacked")
    b2 = write_u64_array(f, ms_all, "kSingleNoteBankPacked")

    f.write(f"static constexpr int kStyleFrameOffsets[4] = {{ {style_offsets[0]}, {style_offsets[1]}, {style_offsets[2]}, {style_offsets[3]} }};\n")
    f.write(f"static constexpr int kStyleFrameLengths[4] = {{ {style_lengths[0]}, {style_lengths[1]}, {style_lengths[2]}, {style_lengths[3]} }};\n\n")

    f.write("const int16_t* getStyleStereoData(int styleIndex) noexcept {\n")
    f.write("    int s = (styleIndex < 0) ? 0 : ((styleIndex > 3) ? 3 : styleIndex);\n")
    f.write("    const int16_t* base = reinterpret_cast<const int16_t*>(kPhraseBankPacked);\n")
    f.write("    return base + static_cast<size_t>(kStyleFrameOffsets[s]) * 2;\n")
    f.write("}\n\n")

    f.write("int getStyleStereoFrames(int styleIndex) noexcept {\n")
    f.write("    int s = (styleIndex < 0) ? 0 : ((styleIndex > 3) ? 3 : styleIndex);\n")
    f.write("    return kStyleFrameLengths[s];\n")
    f.write("}\n\n")

    f.write("const int16_t* getSingleNoteStereoData() noexcept {\n")
    f.write("    return reinterpret_cast<const int16_t*>(kSingleNoteBankPacked);\n")
    f.write("}\n\n")
    f.write("} // namespace PedalSteelMultiSamples\n")

print(f"Wrote {out_header} and {out_cpp} (raw audio={(b1+b2)/(1024*1024):.2f} MB, cpp={os.path.getsize(out_cpp)/(1024*1024):.2f} MB)")
