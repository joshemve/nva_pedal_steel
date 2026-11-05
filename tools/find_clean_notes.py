import os
import glob
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
wav_files = sorted(glob.glob(os.path.join(ref_dir, "*.wav")))

NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
def midi_to_name(m):
    return NOTE_NAMES[int(round(m)) % 12] + str(int(round(m)) // 12 - 1)

# Let's inspect all 4 files to find the cleanest sustained single-note or octave-dominated regions
for wf in wav_files:
    sr, data = wav.read(wf)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    mono = 0.5 * (x[:, 0] + x[:, 1]) if x.ndim == 2 else x

    print(f"=== Scanning {os.path.basename(wf)} (len={len(mono)/sr:.2f}s) ===")
    hop = int(sr * 0.05) # 50ms
    nfft = 4096
    win = np.hanning(nfft)
    freqs = np.fft.rfftfreq(nfft, 1.0/sr)

    frames = []
    for start in range(0, len(mono) - nfft, hop):
        seg = mono[start:start+nfft]
        rms = np.sqrt(np.mean(seg**2))
        if rms < 0.015:
            frames.append((start/sr, 0, 0, rms))
            continue
        spec = np.abs(np.fft.rfft(seg * win))
        # Find f0 candidate using harmonic sum
        best_f0 = 0
        best_h_energy = 0
        total_energy = np.sum(spec[(freqs >= 100) & (freqs <= 4000)]**2) + 1e-12
        for midi_cand in np.linspace(48, 84, 361): # C3 to C6 in 0.1 semitone steps
            f0 = 440.0 * (2.0 ** ((midi_cand - 69.0) / 12.0))
            h_e = 0.0
            for h in range(1, 9):
                f_h = f0 * h
                idx = int(round(f_h / (sr / nfft)))
                if 1 <= idx < len(spec) - 1:
                    h_e += np.max(spec[idx-1:idx+2])**2
            if h_e > best_h_energy:
                best_h_energy = h_e
                best_f0 = f0
        purity = best_h_energy / total_energy
        midi_val = 69 + 12 * np.log2(best_f0 / 440.0) if best_f0 > 0 else 0
        frames.append((start/sr, midi_val, purity, rms))

    # Group contiguous stable frames (where midi_val stays within +-0.25 semitones and purity > 0.55)
    i = 0
    while i < len(frames):
        if frames[i][2] > 0.55 and frames[i][3] > 0.02:
            j = i + 1
            base_m = frames[i][1]
            while j < len(frames) and abs(frames[j][1] - base_m) < 0.30 and frames[j][2] > 0.50 and frames[j][3] > 0.015:
                j += 1
            dur = (j - i) * 0.05
            if dur >= 0.25:
                m_avg = np.mean([frames[k][1] for k in range(i, j)])
                p_avg = np.mean([frames[k][2] for k in range(i, j)])
                r_avg = 20*np.log10(np.mean([frames[k][3] for k in range(i, j)]))
                print(f"  t={frames[i][0]:5.2f}s..{frames[j-1][0]+0.05:5.2f}s (dur={dur:.2f}s) | Note={midi_to_name(m_avg)} (MIDI {m_avg:.2f}) | HarmonicPurity={p_avg:.2f} | RMS={r_avg:.1f}dB")
            i = j
        else:
            i += 1
