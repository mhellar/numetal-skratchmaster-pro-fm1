# SPDX-License-Identifier: GPL-3.0-only
"""Render NU//METAL's kit and guitar offline, in floating point, oversampled, and write them as 16-bit PCM
(firmware/gen/kit_pcm.h). The FM-1 plays these back instead of synthesising them live: its fixed-point
oscillators and one-pole filters could not get past "8-bit". Everything here is made from scratch (no samples).

    python tools/render_kit.py [--wav DIR]     (--wav: also write each sound as a .wav, to listen)
"""
import sys
from pathlib import Path
import numpy as np
from scipy import signal

OS = 176400                                      # render rate: 4 x 44100
rng = np.random.default_rng(808)


def t_of(sec, fs=OS):
    return np.arange(int(sec * fs)) / fs


def env_exp(t, tau):
    return np.exp(-t / tau)


def sine_sweep(t, f0, f1, tau):                  # f(t) = f1 + (f0 - f1) e^(-t/tau), phase integrated
    ph = 2 * np.pi * (f1 * t + (f0 - f1) * tau * (1 - np.exp(-t / tau)))
    return np.sin(ph)


def bq(x, kind, f, fs=OS, q=0.707, gain_db=0.0, order=2):
    if kind == 'peak':
        A = 10 ** (gain_db / 40); w = 2 * np.pi * f / fs; al = np.sin(w) / (2 * q)
        b = [1 + al * A, -2 * np.cos(w), 1 - al * A]; a = [1 + al / A, -2 * np.cos(w), 1 - al / A]
        return signal.lfilter(b, a, x)
    if kind == 'bp':
        sos = signal.butter(order, [f[0], f[1]], 'bandpass', fs=fs, output='sos')
    else:
        sos = signal.butter(order, f, kind, fs=fs, output='sos')
    return signal.sosfilt(sos, x)


def squares(t, freqs):                           # the 808's six detuned squares, band-limited (rendered at 4x)
    return sum(signal.square(2 * np.pi * f * t + rng.uniform(0, 6.28)) for f in freqs) / len(freqs)


def to_rate(x, fs):
    if fs == OS:
        return x
    from math import gcd
    g = gcd(OS, fs)
    return signal.resample_poly(x, fs // g, OS // g)


def fade_end(x, fs, sec=0.03):
    n = min(len(x), int(sec * fs)); x = x.copy(); x[-n:] *= np.linspace(1, 0, n) ** 2; return x


# ------------------------------------------------------------------ drums
def kick():                                      # 808 body falling into ~47 Hz, a beater click, a little tape
    t = t_of(0.65)
    body = sine_sweep(t, 230, 47, 0.028) * env_exp(t, 0.22)
    click = bq(rng.standard_normal(len(t)), "bp", (1500, 6000)) * env_exp(t, 0.0015) * 1.2
    click += np.sin(2 * np.pi * 1100 * t) * env_exp(t, 0.002) * 0.5
    x = np.tanh(1.6 * (body + click)) / np.tanh(1.6)
    return bq(x, 'high', 28)


def bass808():                                   # the long 808: a quick drop into its note, saturated
    t = t_of(1.1)
    x = sine_sweep(t, 98, 49, 0.03) * (0.15 + 0.85 * env_exp(t, 0.45)) * env_exp(t, 2.5)
    x = np.tanh(2.4 * x) / np.tanh(2.4)
    return bq(x, 'high', 30)


def snare():                                     # two tuned heads + the snappy (noise, 1.5..9 kHz)
    t = t_of(0.38)
    heads = sine_sweep(t, 210, 180, 0.02) * env_exp(t, 0.055) + 0.55 * sine_sweep(t, 360, 330, 0.02) * env_exp(t, 0.035)
    n = rng.standard_normal(len(t))
    snappy = bq(bq(n, 'high', 1500, order=2), 'low', 9000, order=2) * env_exp(t, 0.075)
    snappy = bq(snappy, 'peak', 4500, q=0.8, gain_db=3)
    x = 0.75 * heads + 0.55 * snappy / np.abs(snappy).max()
    return np.tanh(1.3 * x)


def clap():                                      # four slaps 10 ms apart, then the tail; band-passed ~1.2 kHz
    t = t_of(0.45)
    n = rng.standard_normal(len(t))
    e = np.zeros(len(t))
    for k, d in enumerate((0.0, 0.010, 0.021, 0.031)):
        tt = t - d; e += np.where(tt >= 0, np.exp(-np.maximum(tt, 0) / 0.0045), 0) * (0.8 if k < 3 else 1.0)
    e += np.where(t >= 0.031, np.exp(-np.maximum(t - 0.031, 0) / 0.11), 0) * 0.55
    x = bq(n, 'bp', (750, 2600), order=2) * e
    x = bq(x, 'peak', 1200, q=1.2, gain_db=4)
    return np.tanh(1.2 * x / np.abs(x).max())


HAT_F = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)


def hat(sec, tau):
    t = t_of(sec)
    m = squares(t, HAT_F)
    x = bq(bq(m, 'bp', (6500, 11000), order=2), 'high', 7000, order=2) * env_exp(t, tau)
    x += bq(rng.standard_normal(len(t)), 'high', 8000) * env_exp(t, tau * 0.7) * 0.05
    return fade_end(x / np.abs(x).max(), OS, min(0.02, sec / 4))


def cymbal(sec, freqs, low_tau, high_tau, bp):    # three bands, each its own decay
    t = t_of(sec)
    m = squares(t, freqs)
    lo = bq(m, 'bp', (bp, bp * 2.3)) * env_exp(t, low_tau)
    hi = bq(bq(m, 'high', bp * 2.3, order=4), 'low', 15000) * env_exp(t, high_tau)
    nz = bq(bq(rng.standard_normal(len(t)), 'high', 5000), 'low', 14000) * env_exp(t, high_tau * 0.8) * 0.15
    att = bq(rng.standard_normal(len(t)), 'bp', (2000, 9000)) * env_exp(t, 0.012) * 0.6
    x = 0.6 * lo / np.abs(lo).max() + hi / np.abs(hi).max() + nz + att
    return fade_end(x / np.abs(x).max(), OS, 0.08)


def rim():                                       # 808 rimshot: two sines through a band-pass, clipped
    t = t_of(0.12)
    x = (np.sin(2 * np.pi * 1667 * t) + 0.7 * np.sin(2 * np.pi * 455 * t)) * env_exp(t, 0.011)
    x = bq(x, 'bp', (300, 5000))
    return np.tanh(2.0 * x / np.abs(x).max())


def tom(f):                                      # 808 tom: a sine dropping 12 %, a breath of noise
    t = t_of(0.55)
    x = sine_sweep(t, f * 1.12, f, 0.06) * env_exp(t, 0.16)
    x += bq(rng.standard_normal(len(t)), 'bp', (f * 2, f * 20)) * env_exp(t, 0.03) * 0.08
    return np.tanh(1.4 * x) / np.tanh(1.4)


def zap():
    t = t_of(0.3)
    x = sine_sweep(t, 3800, 70, 0.025) * env_exp(t, 0.08)
    return np.tanh(1.5 * x)


# ------------------------------------------------------------------ guitar: plucked strings into an amp and a 4x12
def ks(f, sec, fs, damp, lp):                    # Karplus-Strong: a fractional delay, a two-point average in the loop
    n = int(sec * fs); L = fs / f; Li = int(np.floor(L)); fr = L - Li
    line = np.zeros(n + Li + 4)
    burst = signal.lfilter([1 - lp], [1, -lp], rng.standard_normal(Li + 2))
    p = max(1, int(0.17 * Li)); burst[p:] -= burst[:-p]
    line[:Li + 2] = burst / np.abs(burst).max()
    prev = 0.0
    for i in range(Li + 2, n + Li + 4):
        d = line[i - Li] + (line[i - Li - 1] - line[i - Li]) * fr
        y = damp * (0.5 * d + 0.5 * prev)
        prev = d
        line[i] = y
    return line[Li + 4:]


def amp(x, fs, gain, hp=110):                            # at 8x: tight, a tube screamer push, two stages, tone stack, cab
    up = 8; y = signal.resample_poly(x, up, 1); f2 = fs * up
    y = bq(y, 'high', hp, fs=f2, order=2)
    y = bq(y, 'peak', 850, fs=f2, q=0.7, gain_db=5)
    y = np.tanh(gain * y + 0.12) - np.tanh(0.12)
    y = bq(y, 'low', 9000, fs=f2, order=1)
    y = np.tanh(3.0 * y)
    y = signal.resample_poly(y, 1, up)
    y = bq(y, 'peak', 480, fs=fs, q=0.8, gain_db=-5)                           # the metal scoop
    y = bq(y, 'peak', 2600, fs=fs, q=0.9, gain_db=3.5)                          # presence
    y = bq(y, 'high', 75, fs=fs, order=2)                                       # the cab
    y = bq(y, 'peak', 115, fs=fs, q=1.4, gain_db=4)
    y = bq(y, 'low', 5200, fs=fs, order=4)
    y = bq(y, 'peak', 3800, fs=fs, q=3, gain_db=-4)
    return y


def chord(f0, sec, fs, muted):
    damp = 0.975 if muted else 0.9985
    lp = 0.55 if muted else 0.25
    s = sum(ks(f0 * r, sec, fs, damp, lp) * g for r, g in ((1, 1.0), (1.4983, 0.9), (2.0, 0.7)))
    if muted:
        s = bq(s, 'low', 2200, fs=fs, order=1)
    s /= np.abs(s).max()
    y = amp(s * 0.5, fs, 28.0 if not muted else 24.0, 110 if not muted else 70)
    t = np.arange(len(y)) / fs
    if muted:
        y *= np.minimum(1, t / 0.002) * np.exp(-t / 0.09)
    y = fade_end(y, fs, 0.05 if not muted else 0.03)
    return y


# ------------------------------------------------------------------ out
def pcm(x):
    x = x / (np.abs(x).max() + 1e-12) * 0.94
    return np.round(x * 32767).astype(np.int16)


GTR_FS = 22050
GTR_F0 = 61.735                                  # B1: drop B, the riff's root at TUNE 6


def main():
    wav_dir = None
    if '--wav' in sys.argv:
        wav_dir = Path(sys.argv[sys.argv.index('--wav') + 1]); wav_dir.mkdir(parents=True, exist_ok=True)
    kit = [  # name, sound at OS, stored rate
        ('KICK', kick(), 22050), ('SNARE', snare(), 32000), ('CLAP', clap(), 32000),
        ('HAT', hat(0.14, 0.035), 32000), ('OPEN', hat(0.6, 0.22), 32000), ('RIM', rim(), 32000),
        ('TOML', tom(105), 22050), ('TOMH', tom(160), 22050), ('B808', bass808(), 22050),
        ('CHINA', cymbal(0.75, [f * 1.7 for f in HAT_F], 0.18, 0.32, 3000), 32000),
        ('ZAP', zap(), 32000),
        ('CRASH', cymbal(1.15, HAT_F, 0.35, 0.55, 2200), 32000),
    ]
    out = []
    for name, x, fs in kit:
        out.append((name, pcm(to_rate(x, fs)), fs))
    out.append(('GCHUG', pcm(chord(GTR_F0, 0.32, GTR_FS, True)), GTR_FS))
    out.append(('GOPEN', pcm(chord(GTR_F0, 1.45, GTR_FS, False)), GTR_FS))
    total = 0
    lines = ['/* SPDX-License-Identifier: GPL-3.0-only */',
             '/* Generated by tools/render_kit.py: the kit and the guitar, rendered offline, 16-bit PCM. Do not edit. */',
             '#include <stdint.h>']
    for name, d, fs in out:
        total += 2 * len(d)
        lines.append(f'#define PCM_{name}_FS {fs}u')
        lines.append(f'static const int16_t PCM_{name}[{len(d)}] = {{')
        for i in range(0, len(d), 24):
            lines.append('    ' + ','.join(str(int(v)) for v in d[i:i + 24]) + ',')
        lines.append('};')
        if wav_dir:
            import wave
            w = wave.open(str(wav_dir / f'{name.lower()}.wav'), 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs)
            w.writeframes(d.tobytes()); w.close()
        print(f'{name:6s} {fs:5d} Hz {len(d) / fs:5.2f} s {2 * len(d):7d} B')
    print(f'total {total} B')
    Path('firmware/gen/kit_pcm.h').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
