import os
import glob
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"

# Let's inspect specific time windows in Sweet Georgia and Hedonist where single notes are cleanest
def analyze_window(filename, t_start, t_end, label):
    path = os.path.join(ref_dir, filename)
    sr, data = wav.read(path)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    mono = 0.5 * (x[:, 0] + x[:, 1]) if x.ndim == 2 else x
    seg = mono[int(t_start*sr):int(t_end*sr)]
    # Estimate fundamental via autocorrelation in [150Hz, 1000Hz]
    win = seg * np.hanning(len(seg))
    spec = np.abs(np.fft.rfft(win, n=32768))
    freqs = np.fft.rfftfreq(32768, 1.0/sr)
    # Check harmonics for candidate f0
    best_f0 = 0
    best_score = -1
    for f0 in np.linspace(120, 950, 1660):
        score = 0
        for h in range(1, 8):
            idx = int(round(f0 * h / (sr / 32768)))
            if idx < len(spec):
                score += spec[idx] * (1.0 / (h**0.3))
        if score > best_score:
            best_score = score
            best_f0 = f0
    # Refine f0
    harm_amps = []
    for h in range(1, 17):
        f_target = best_f0 * h
        idx_lo = max(0, int((f_target - 15) / (sr / 32768)))
        idx_hi = min(len(spec), int((f_target + 15) / (sr / 32768)))
        pk = np.max(spec[idx_lo:idx_hi]) if idx_hi > idx_lo else 0.0
        harm_amps.append(pk)
    harm_amps = np.array(harm_amps)
    harm_db = 20 * np.log10(harm_amps / (np.max(harm_amps) + 1e-12) + 1e-12)
    print(f"[{label}] {filename} ({t_start:.2f}-{t_end:.2f}s) -> f0={best_f0:.2f} Hz")
    print("  Harmonics dB (H1..H12):", " ".join([f"H{i+1}:{harm_db[i]:+5.1f}" for i in range(12)]))

analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 0.02, 0.14, "SweetGeorgia F#4 onset")
analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 0.35, 1.20, "SweetGeorgia G#4 sustain")
analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 2.22, 2.65, "SweetGeorgia F#4 sustain")
analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 3.80, 4.90, "SweetGeorgia E4 sustain")
analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 5.85, 6.35, "SweetGeorgia B4 sustain")
analyze_window("CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", 14.80, 15.60, "SweetGeorgia C#4 sustain")

analyze_window("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", 0.05, 0.45, "Hedonist A4 sustain")
analyze_window("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", 0.85, 1.45, "Hedonist B4 sustain")
analyze_window("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", 2.02, 2.35, "Hedonist D5 sustain")
analyze_window("CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", 5.42, 5.85, "Hedonist C4/C5 sustain")

analyze_window("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", 0.55, 0.90, "GritVibey F#5 sustain")
analyze_window("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", 4.10, 5.20, "GritVibey D5 sustain")
analyze_window("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", 8.05, 8.80, "GritVibey F#5 high sustain")
analyze_window("CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", 14.10, 14.90, "GritVibey A5 sustain")
