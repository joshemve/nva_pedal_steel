import os
import glob
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
wav_files = sorted(glob.glob(os.path.join(ref_dir, "*.wav")))

NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
def hz_to_note(hz):
    if hz <= 0: return "---"
    midi = 69 + 12 * np.log2(hz / 440.0)
    m_int = int(round(midi))
    cents = (midi - m_int) * 100
    name = NOTE_NAMES[m_int % 12] + str(m_int // 12 - 1)
    return f"{name}({midi:.2f}, {cents:+.0f}c)"

for wf in wav_files:
    sr, data = wav.read(wf)
    x = data.astype(np.float32) / (32768.0 if data.dtype == np.int16 else 2147483648.0 if data.dtype == np.int32 else 1.0)
    mono = 0.5 * (x[:, 0] + x[:, 1]) if x.ndim == 2 else x

    print(f"================ {os.path.basename(wf)} ================")
    hop = int(sr * 0.1) # 100ms frames
    nfft = 8192
    win = np.hanning(nfft)
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)

    for i in range(0, min(len(mono) - nfft, int(sr * 16)), hop * 2): # every 200ms up to 16s
        seg = mono[i:i+nfft] * win
        rms = np.sqrt(np.mean(mono[i:i+hop]**2))
        if rms < 0.008:
            continue
        spec = np.abs(np.fft.rfft(seg))
        # Find top 3 spectral peaks above 100Hz separated by at least 40Hz
        mask = (freqs >= 110) & (freqs <= 2500)
        f_sub = freqs[mask]
        s_sub = spec[mask]
        peaks, props = signal.find_peaks(s_sub, height=np.max(s_sub)*0.12, distance=int(40 / (sr / nfft)))
        if len(peaks) > 0:
            order = np.argsort(s_sub[peaks])[::-1][:4]
            top_peaks = sorted(peaks[order]) # sort by frequency
            p_strs = []
            for p in top_peaks:
                # Parabolic interpolation
                if 0 < p < len(s_sub) - 1:
                    alpha, beta, gamma = np.log(s_sub[p-1]+1e-12), np.log(s_sub[p]+1e-12), np.log(s_sub[p+1]+1e-12)
                    delta = 0.5 * (alpha - gamma) / (alpha - 2*beta + gamma + 1e-12)
                    f_est = f_sub[p] + delta * (sr / nfft)
                else:
                    f_est = f_sub[p]
                rel_db = 20 * np.log10(s_sub[p] / np.max(s_sub))
                p_strs.append(f"{hz_to_note(f_est)}[{f_est:.1f}Hz, {rel_db:.1f}dB]")
            t_sec = i / sr
            print(f"  t={t_sec:5.2f}s (RMS={20*np.log10(rms):5.1f}dB): " + " | ".join(p_strs))
