import os
import glob
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal
import json

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
wav_files = sorted(glob.glob(os.path.join(ref_dir, "*.wav")))

for wf in wav_files:
    sr, data = wav.read(wf)
    if data.dtype == np.int16:
        x = data.astype(np.float32) / 32768.0
    elif data.dtype == np.int32:
        x = data.astype(np.float32) / 2147483648.0
    elif data.dtype == np.uint8:
        x = (data.astype(np.float32) - 128.0) / 128.0
    else:
        x = data.astype(np.float32)
    if x.ndim == 2:
        mono = 0.5 * (x[:, 0] + x[:, 1])
        side = 0.5 * (x[:, 0] - x[:, 1])
        stereo_width = float(np.sqrt(np.mean(side**2)) / (np.sqrt(np.mean(mono**2)) + 1e-9))
    else:
        mono = x
        stereo_width = 0.0

    dur = len(mono) / sr
    rms_db = 20 * np.log10(np.sqrt(np.mean(mono**2)) + 1e-9)
    peak_db = 20 * np.log10(np.max(np.abs(mono)) + 1e-9)

    # Compute Welch PSD for spectral profile
    f, Pxx = signal.welch(mono, fs=sr, nperseg=4096)
    Pxx_db = 10 * np.log10(Pxx + 1e-18)

    # Band energies
    bands = [
        ("Sub/Low (60-200Hz)", 60, 200),
        ("Body/Warmth (200-500Hz)", 200, 500),
        ("Fundamental Core (500-1200Hz)", 500, 1200),
        ("Pickup Sing/Nasal (1200-2800Hz)", 1200, 2800),
        ("Pick/Chime (2800-5500Hz)", 2800, 5500),
        ("Air/Sizzle (5500-12000Hz)", 5500, 12000),
    ]
    band_str = []
    for name, f_lo, f_hi in bands:
        idx = (f >= f_lo) & (f < f_hi)
        e_db = 10 * np.log10(np.sum(Pxx[idx]) * (f[1] - f[0]) + 1e-18)
        band_str.append(f"  {name}: {e_db:.1f} dB")

    # Spectral centroid
    valid = (f >= 50) & (f <= 12000)
    centroid = float(np.sum(f[valid] * Pxx[valid]) / (np.sum(Pxx[valid]) + 1e-18))

    print(f"=== {os.path.basename(wf)} ===")
    print(f"  SR: {sr} Hz | Duration: {dur:.2f}s | RMS: {rms_db:.1f} dBFS | Peak: {peak_db:.1f} dBFS | Stereo Side/Mid: {stereo_width:.2f} | Centroid: {centroid:.0f} Hz")
    print("\n".join(band_str))
    print()
