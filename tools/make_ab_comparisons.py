import os
import subprocess
import numpy as np
import scipy.io.wavfile as wav
import scipy.signal as signal

ref_dir = r"C:\Users\Joshua\Documents\pedal_steel\references"
ren_dir = r"C:\Users\Joshua\Documents\pedal_steel\renders"

pairs = [
    ("01_sweet_georgia_EMaj", "CO_FC_83_pedal_steel_sweetgeorgia_verse_EMaj.wav", "01_sweet_georgia_EMaj.wav", 10.5),
    ("02_hedonist_bootgaze_GMaj", "CO_BG_90_pedal_steel_melody_hedonist_Gmaj.wav", "02_hedonist_bootgaze_GMaj.wav", 10.5),
    ("03_mikeys_grit_vibey_DMaj", "CO_MCS_120_pedal_steel_grit_vibey_Dmaj.wav", "03_mikeys_grit_vibey_DMaj.wav", 10.5),
    ("04_montana_swell_GMaj", "CO_MCS_126_pedal_steel_montana_Gmaj.wav", "04_montana_swell_GMaj.wav", 10.5),
]

def load_stereo(path):
    sr, data = wav.read(path)
    if data.dtype == np.int16:
        x = data.astype(np.float32) / 32768.0
    elif data.dtype == np.int32:
        x = data.astype(np.float32) / 2147483648.0
    else:
        x = data.astype(np.float32)
    if x.ndim == 1:
        x = np.column_stack([x, x])
    return sr, x

def analyze(x, sr):
    mono = 0.5 * (x[:, 0] + x[:, 1])
    rms_db = 20 * np.log10(np.sqrt(np.mean(mono**2)) + 1e-9)
    f, Pxx = signal.welch(mono, fs=sr, nperseg=4096)
    valid = (f >= 50) & (f <= 12000)
    centroid = float(np.sum(f[valid] * Pxx[valid]) / (np.sum(Pxx[valid]) + 1e-18))
    return rms_db, centroid

for slug, ref_name, ren_name, window_sec in pairs:
    ref_path = os.path.join(ref_dir, ref_name)
    ren_path = os.path.join(ren_dir, ren_name)
    if not os.path.exists(ren_path):
        print(f"Missing {ren_path}")
        continue

    sr_ref, x_ref = load_stereo(ref_path)
    sr_ren, x_ren = load_stereo(ren_path)

    n_win = int(window_sec * sr_ref)
    seg_ref = x_ref[:min(len(x_ref), n_win)].copy()
    seg_ren = x_ren[:min(len(x_ren), n_win)].copy()

    # Match RMS for fair A/B splice
    rms_ref, cent_ref = analyze(seg_ref, sr_ref)
    rms_ren, cent_ren = analyze(seg_ren, sr_ren)
    gain_adj = 10.0 ** ((rms_ref - rms_ren) / 20.0)
    seg_ren = np.clip(seg_ren * gain_adj, -0.98, 0.98)
    rms_ren2, cent_ren2 = analyze(seg_ren, sr_ren)

    # Apply 25ms fade out/in around splice with 0.15s gap
    fade = int(0.025 * sr_ref)
    seg_ref[-fade:] *= np.linspace(1.0, 0.0, fade)[:, None]
    seg_ren[:fade]  *= np.linspace(0.0, 1.0, fade)[:, None]
    gap = np.zeros((int(0.15 * sr_ref), 2), dtype=np.float32)

    ab_audio = np.vstack([seg_ref, gap, seg_ren])
    ab_wav = os.path.join(ren_dir, f"{slug}_AB_stem_then_preset.wav")
    ab_mp3 = os.path.join(ren_dir, f"{slug}_AB_stem_then_preset.mp3")
    ren_mp3 = os.path.join(ren_dir, f"{slug}.mp3")

    wav.write(ab_wav, sr_ref, (np.clip(ab_audio, -0.99, 0.99) * 32767.0).astype(np.int16))
    subprocess.run(["ffmpeg", "-y", "-i", ab_wav, "-b:a", "192k", ab_mp3], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["ffmpeg", "-y", "-i", ren_path, "-b:a", "192k", ren_mp3], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    print(f"[{slug}]")
    print(f"  Reference : RMS={rms_ref:5.1f} dBFS | Centroid={cent_ref:5.0f} Hz")
    print(f"  Our Plugin: RMS={rms_ren2:5.1f} dBFS | Centroid={cent_ren2:5.0f} Hz")
    print(f"  Created   : {ab_mp3}")
