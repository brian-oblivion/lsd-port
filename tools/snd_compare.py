#!/usr/bin/env python3
"""Compare two recordings of the same moment (raw S16LE stereo, 44.1 kHz:
lsd's SDL disk audio, or DuckStation's capture through
`ffmpeg -i capture.mp4 -vn -f s16le -ac 2 -ar 44100 out.raw`).

    from snd_compare import *
    D = seg(load('ds.raw'), 118.2, 158); P = seg(load('port.raw'), 99.4, 139)
    compare(D, P, 'movie')   # lag, tempo, pitch ratio, loudness, balance
    specimg(D, 'ds.png')     # log-frequency spectrogram

Recordings and their images are game audio: keep them out of the repository.
"""
import numpy as np
SR = 44100
def load(path):
    a = np.fromfile(path, dtype='<i2').astype(np.float32) / 32768
    return a[: len(a) // 2 * 2].reshape(-1, 2)
def marks(path):
    return {l.split()[1]: float(l.split()[0]) for l in open(path) if l.strip()}
def env(x, win=0.05):
    n = int(SR * win); k = len(x) // n
    m = x[: k * n].reshape(k, n, 2)
    return np.sqrt((m ** 2).mean(1))  # per channel rms
def db(v): return 20 * np.log10(np.maximum(v, 1e-6))
def seg(x, t0, t1): return x[int(t0 * SR): int(t1 * SR)]
def peaks(x, n=6, fmin=40):
    m = x.mean(1); w = np.hanning(len(m))
    f = np.abs(np.fft.rfft(m * w)); fr = np.fft.rfftfreq(len(m), 1 / SR)
    f[fr < fmin] = 0
    idx = []
    for i in np.argsort(f)[::-1]:
        if all(abs(fr[i] - fr[j]) > 15 for j in idx): idx.append(i)
        if len(idx) == n: break
    return [(round(fr[i], 1), round(float(db(f[i] / f.max())), 1)) for i in idx]
def lag(a, b, maxlag=3.0, win=0.01):
    """seconds by which b is late relative to a (envelope xcorr)."""
    ea = env(a, win).mean(1); eb = env(b, win).mean(1)
    n = min(len(ea), len(eb)); ea = ea[:n] - ea[:n].mean(); eb = eb[:n] - eb[:n].mean()
    L = int(maxlag / win); best = None
    for l in range(-L, L + 1):
        if l >= 0: c = (ea[: n - l] * eb[l:]).sum()
        else: c = (ea[-l:] * eb[: n + l]).sum()
        if best is None or c > best[0]: best = (c, l)
    return best[1] * win

def plot(rows, path, t_range, ylim=(-80, 0), w=1600, h=180):
    """rows: list of (title, t array, [(series, colour)], {mark: t})"""
    from PIL import Image, ImageDraw
    img = Image.new('RGB', (w, h * len(rows)), 'white'); dr = ImageDraw.Draw(img)
    t0, t1 = t_range
    X = lambda t: int((t - t0) / (t1 - t0) * (w - 1))
    for r, (title, t, series, mk) in enumerate(rows):
        oy = r * h
        Y = lambda v: oy + int((ylim[1] - min(max(v, ylim[0]), ylim[1])) / (ylim[1] - ylim[0]) * (h - 15)) + 12
        for s in range(int(t0), int(t1) + 1):
            dr.line([(X(s), oy + 12), (X(s), oy + h - 3)], fill=(235, 235, 235))
        for k, v in mk.items():
            if t0 <= v <= t1:
                dr.line([(X(v), oy + 12), (X(v), oy + h - 3)], fill=(120, 120, 120)); dr.text((X(v) + 2, oy + 14), k, fill='black')
        for vals, col in series:
            pts = [(X(a), Y(b)) for a, b in zip(t, vals) if t0 <= a <= t1]
            dr.line(pts, fill=col, width=1)
        dr.text((4, oy), f'{title}  [{ylim[0]}..{ylim[1]}]  {t0:.1f}..{t1:.1f}s', fill='black')
        dr.line([(0, oy + h - 1), (w, oy + h - 1)], fill='black')
    img.save(path)

def stft(x, n=4096, hop=1024):
    m = x.mean(1) if x.ndim == 2 else x
    w = np.hanning(n); k = (len(m) - n) // hop
    fr = np.stack([np.abs(np.fft.rfft(m[i * hop: i * hop + n] * w)) for i in range(k)])
    return fr  # frames x bins, hop/SR s per frame

def align_spec(A, B, maxlag_frames):
    """frames by which B lags A, by correlating log spectra (pitch-sensitive)."""
    la = np.log1p(A[:, 5:400] * 100); lb = np.log1p(B[:, 5:400] * 100)
    la = la - la.mean(); lb = lb - lb.mean()
    n = min(len(la), len(lb)) - maxlag_frames
    best = None
    for l in range(-maxlag_frames, maxlag_frames + 1):
        a = la[maxlag_frames: n]; b = lb[maxlag_frames + l: n + l]
        c = (a * b).sum() / np.sqrt((a * a).sum() * (b * b).sum())
        if best is None or c > best[0]: best = (c, l)
    return best[1], best[0]

def specimg(x, path_or_none=None, fmax=3000, n=8192, hop=441, h=300):
    from PIL import Image
    m = x.mean(1); w = np.hanning(n)
    k = (len(m) - n) // hop
    fr = np.fft.rfftfreq(n, 1 / SR)
    # log-frequency rows from 40 Hz to fmax
    rows = np.geomspace(40, fmax, h)
    idx = np.searchsorted(fr, rows)
    S = np.stack([np.abs(np.fft.rfft(m[i * hop: i * hop + n] * w))[idx] for i in range(k)]).T
    L = 20 * np.log10(S + 1e-3); L = np.clip((L + 10) / 60, 0, 1)  # -10..50 dB
    img = Image.fromarray((255 - L[::-1] * 255).astype('uint8'))
    if path_or_none: img.save(path_or_none)
    return img

def compare(D, P, label):
    """D, P: aligned-ish segments. Prints lag, tempo ratio, pitch ratio, balance, loudness."""
    hop = 512 / SR
    A = stft(D, 4096, 512); B = stft(P, 4096, 512)
    la = np.log1p(A[:, 3:700] * 100); lb = np.log1p(B[:, 3:700] * 100)
    def corr_at(l, a=la, b=lb):
        if l >= 0: x, y = a[: len(a) - l], b[l:]
        else: x, y = a[-l:], b[: len(b) + l]
        n = min(len(x), len(y)); x = x[:n] - x[:n].mean(); y = y[:n] - y[:n].mean()
        return (x * y).sum() / np.sqrt((x * x).sum() * (y * y).sum())
    L = int(4 / hop)
    cs = [corr_at(l) for l in range(-L, L + 1)]
    l = int(np.argmax(cs)) - L
    print(f'{label}: best lag {l*hop:+.3f}s corr {max(cs):.3f}')
    # tempo: lag in first vs last third
    n = min(len(la), len(lb))
    third = n // 3
    def lag_in(i0, i1):
        cc = [corr_at(k, la[i0:i1], lb[max(0, i0 + 0):i1 + 2 * L]) for k in range(-L, L + 1)]
        return (int(np.argmax(cc)) - L) * hop
    # time-stretch estimate: lag at start vs end
    l1 = lag_in(0, third); l2 = lag_in(2 * third - L if 2*third-L>0 else 0, n - 2 * L)
    print(f'  lag early {l1:+.3f}s late {l2:+.3f}s over {(n*hop):.1f}s -> tempo ratio {1 + (l2 - l1) / (n * hop * 2 / 3):.5f}')
    # pitch ratio from averaged spectra
    fr = np.fft.rfftfreq(4096, 1 / SR)
    if l >= 0: a, b = A[: len(A) - l], B[l:]
    else: a, b = A[-l:], B[: len(B) + l]
    m = min(len(a), len(b)); sa = np.log1p(a[:m].mean(0) * 100); sb = np.log1p(b[:m].mean(0) * 100)
    best = max(((np.corrcoef(sa[10:900], np.interp(fr, fr * r, sb)[10:900])[0, 1], r) for r in np.arange(0.97, 1.03, 0.0005)))
    print(f'  pitch ratio port/ds {best[1]:.4f} (corr {best[0]:.4f}); 1 semitone = 1.0595')
    for nm, x in (('ds', D), ('port', P)):
        r = np.sqrt((x ** 2).mean(0))
        print(f'  {nm}: rms {db(np.sqrt((x**2).mean())):.2f} dB  L-R {db(r[0])-db(r[1]):+.2f} dB  LR corr {np.corrcoef(x[:,0],x[:,1])[0,1]:.3f}  peaks {peaks(x[:SR*8], 5)}')
    return l * hop
