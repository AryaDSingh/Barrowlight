"""Builds the game's combat sound families from the CC0 source packs.

Every family gets a handful of variants: trimmed to start just before the
hit, layered, shaped for punch, normalised, and saved as small OGG files in
assets/sounds/sfx/<family>_<n>.ogg.
"""
from scipy.signal import fftconvolve
import os, glob, numpy as np, soundfile as sf
from scipy.signal import butter, sosfilt, resample_poly

SRC = os.environ.get('SFX_SRC', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sfxsrc'))
OUT = r'C:\school\Personal Project\roguelike\assets\sounds\sfx'
RATE = 44100
os.makedirs(OUT, exist_ok=True)
for old in glob.glob(os.path.join(OUT, '*.ogg')): os.remove(old)

def load(rel):
    data, rate = sf.read(os.path.join(SRC, rel), always_2d=True)
    mono = data.mean(axis=1)
    if rate != RATE:
        g = np.gcd(rate, RATE)
        mono = resample_poly(mono, RATE // g, rate // g)
    return mono.astype(np.float64)

def onset(x, frac=.35):
    peak = np.max(np.abs(x)) + 1e-12
    return int(np.argmax(np.abs(x) >= frac * peak))

def cut(x, length, pre=.012, frac=.6, start=None):
    s = max(0, (onset(x, frac) if start is None else start) - int(pre * RATE))
    y = x[s:s + int(length * RATE)]
    return y

def pitch(x, factor):
    # factor > 1 raises pitch (and shortens), like a faster tape.
    up, down = 100, int(round(100 * factor))
    return resample_poly(x, up, down)

def filt(x, kind, hz, order=4):
    sos = butter(order, hz, btype=kind, fs=RATE, output='sos')
    return sosfilt(sos, x)

def fade(x, tail=.35, head=.003):
    y = x.copy(); n = len(y)
    t = max(1, int(n * tail)); y[n - t:] *= np.linspace(1, 0, t) ** 2
    h = max(1, int(head * RATE)); y[:h] *= np.linspace(0, 1, h)
    return y

def mix(*layers, length=None):
    # layers: (signal, gain_db, offset_seconds)
    n = length and int(length * RATE) or max(len(s) + int(o * RATE) for s, g, o in layers)
    out = np.zeros(n)
    for s, g, o in layers:
        o = int(o * RATE); s = s[: max(0, n - o)]
        out[o:o + len(s)] += s * 10 ** (g / 20)
    return out

def punch(x, drive=2.2):
    # Gentle saturation: lifts the body under the transient, keeps peaks crisp.
    return np.tanh(drive * x) / np.tanh(drive)

def norm(x, db=-1.0):
    return x / (np.max(np.abs(x)) + 1e-12) * 10 ** (db / 20)

LOUDNESS = {'chest_open': -15.0, 'mimic': -11.0, 'voice': -17.0, 'levelup': -13.0, 'death': -10.0, 'crit': -9.0, 'dodge': -24.0, 'light': -14.0, 'water': -14.0, 'rot': -14.0}
LENGTH = {'chest_open': 1.3, 'mimic': 1.4, 'voice': 1.3, 'levelup': 4.2, 'death': 1.4, 'crit': .75, 'dodge': .3, 'slash': .32, 'pierce': .26, 'blunt': .42}
PEAK = {'levelup': -2.0, 'death': -3.0}   # swells and long tails: level by peak, not by the first 200 ms

def save(name, x, db=-1.0):
    family = name.rsplit('_', 1)[0]
    key = 'crit' if family.startswith('crit') else 'voice' if family.startswith('voice') else family
    length = LENGTH.get(key, .7)
    s = 0 if key in ('dodge', 'levelup', 'chest_open') else max(0, onset(x, .5) - int(.012 * RATE))  # the hit, not the build-up (a whoosh is all build-up)
    y = x[s:s + int(length * RATE)]
    head = y[: int(.2 * RATE)]
    rms = np.sqrt(np.mean(head ** 2)) + 1e-12
    y = y * 10 ** (LOUDNESS.get(key, -12.0) / 20) / rms
    if key in PEAK: y = y * 10 ** (PEAK[key] / 20) / (np.max(np.abs(y)) + 1e-12)
    y = np.tanh(y * 1.1) / np.tanh(1.1) if np.max(np.abs(y)) > .94 else y   # soft-limit the peaks
    path = os.path.join(OUT, name + '.ogg')
    sf.write(path, fade(y, .4).astype(np.float32), RATE, format='OGG', subtype='VORBIS')

def onsets(x, min_gap=.22, frac=.25):
    env = np.convolve(np.abs(x), np.ones(int(.005 * RATE)) / (.005 * RATE), mode='same')
    peak = env.max(); found = []; i = 0
    while i < len(env):
        if env[i] >= frac * peak and (not found or i - found[-1] > min_gap * RATE):
            found.append(i); i += int(min_gap * RATE)
        else: i += 1
    return found

K = 'impact/Audio/'
punch_h = [load(f'{K}impactPunch_heavy_00{i}.ogg') for i in range(5)]
punch_m = [load(f'{K}impactPunch_medium_00{i}.ogg') for i in range(5)]
soft_h = [load(f'{K}impactSoft_heavy_00{i}.ogg') for i in range(5)]
soft_m = [load(f'{K}impactSoft_medium_00{i}.ogg') for i in range(5)]
wood_h = [load(f'{K}impactWood_heavy_00{i}.ogg') for i in range(5)]
mining = [load(f'{K}impactMining_00{i}.ogg') for i in range(5)]
bell = [load(f'{K}impactBell_heavy_00{i}.ogg') for i in range(5)]
glass_h = [load(f'{K}impactGlass_heavy_00{i}.ogg') for i in range(5)]
glass_l = [load(f'{K}impactGlass_light_00{i}.ogg') for i in range(5)]
metal_l = [load(f'{K}impactMetal_light_00{i}.ogg') for i in range(5)]
swords = [load(p.replace(SRC + os.sep, '')) for p in sorted(glob.glob(os.path.join(SRC, 'sword', '*', '*.ogg')))]
slices = [load('rpg/Audio/knifeSlice2.ogg'), load('rpg/Audio/chop.ogg'), load('rpg/Audio/knifeSlice.ogg')]
meat = [load('qubo/qubodupImpact/qubodupImpactMeat01.ogg'), load('qubo/qubodupImpact/qubodupImpactMeat02.ogg')]
stone = load('qubo/qubodupImpact/qubodupImpactStone.ogg')
spell = {k: [load(f'spells/{k} Spell Impacts/{k} Spell Impact {i}.wav') for i in range(1, 6)] for k in ['Fire', 'Ice', 'Lightning', 'Water']}
splat = []
for f in ['splat/extreme_splatter/crack11.mp3.flac', 'splat/extreme_splatter/crack12.mp3.flac']:
    x = load(f)
    for o in onsets(x)[:6]: splat.append(x[max(0, o - int(.01 * RATE)): o + int(.45 * RATE)])
splat.sort(key=lambda s: -np.max(np.abs(s)))  # the meatiest first

V = 5
for i in range(V):
    # --- Weapons -----------------------------------------------------------
    sw = filt(cut(swords[i * 2 % len(swords)], .30, pre=.02), 'highpass', 400)
    save(f'slash_{i}', punch(mix((sw, 0, 0), (cut(punch_m[i], .3), -7, .015), (cut(soft_m[i], .2), -10, .01))), -2)
    sl = filt(cut(slices[i % 3], .25), 'highpass', 600)
    save(f'pierce_{i}', punch(mix((cut(soft_m[i], .2), 0, 0), (sl, -5, 0), (cut(metal_l[i], .15), -16, 0))), -2)
    save(f'blunt_{i}', punch(mix((cut(punch_h[i], .4), 0, 0), (filt(cut(wood_h[i], .3), 'lowpass', 1500), -6, .005))), -1.5)
    # --- Elements ------------------------------------------------------------
    save(f'fire_{i}', punch(cut(spell['Fire'][i], .75, pre=.02), 1.6), -2)
    save(f'frost_{i}', punch(cut(spell['Ice'][i], .7, pre=.015), 1.4), -2.5)
    save(f'lightning_{i}', punch(cut(spell['Lightning'][i], .8, pre=.01), 1.6), -2.5)
    save(f'water_{i}', cut(spell['Water'][i], .7, pre=.02), -3)
    arc = mix((pitch(cut(spell['Ice'][i], .9), .72), 0, 0), (pitch(cut(bell[i], .8), 1.45), -9, 0), (cut(glass_l[i], .2), -14, 0))
    save(f'arcane_{i}', punch(filt(arc, 'highpass', 180), 1.4), -2.5)
    dark = mix((filt(pitch(cut(spell['Fire'][i], 1.2), .58), 'lowpass', 1600), 0, 0), (pitch(cut(soft_h[i], .5), .8), -4, 0))
    save(f'shadow_{i}', punch(dark, 1.8), -2)
    lit = mix((pitch(cut(bell[i], .9), 1.6), -2, 0), (cut(glass_l[i], .25), -5, 0), (pitch(cut(spell['Ice'][i], .8), 1.25), -8, 0))
    save(f'light_{i}', filt(lit, 'highpass', 300), -3)
    rock = mix((pitch(cut(mining[i], .7), .8), 0, 0), (pitch(cut(wood_h[i], .4), .7), -3, 0), (cut(stone, .3), -6, 0))
    save(f'earth_{i}', punch(rock, 2.0), -2)
    rot = filt(pitch(cut(spell['Water'][i], 1.0), .62), 'lowpass', 2400)
    save(f'rot_{i}', mix((rot, 0, 0), (cut(meat[i % 2], .3), -8, .02)), -3)
    save(f'blood_{i}', punch(mix((cut(meat[i % 2], .4), 0, 0), (pitch(cut(spell['Water'][i], .7), .6), -7, 0), (cut(soft_m[i], .2), -6, 0))), -2.5)
    # --- Crits: a heavier layer on top of the family's own hit ----------------
    gore = mix((splat[i % len(splat)], 0, 0), (cut(meat[i % 2], .4), -3, 0), (pitch(cut(soft_h[i], .5), .75), -2, 0))
    save(f'crit_gore_{i}', punch(gore, 2.6), -0.5)
    bone = mix((pitch(cut(wood_h[i], .45), .72), 0, 0), (pitch(cut(mining[i], .5), .9), -4, 0), (cut(stone, .3), -3, 0), (pitch(cut(punch_h[i], .4), .8), -5, 0))
    save(f'crit_bone_{i}', punch(bone, 2.4), -0.5)
    save(f'crit_fire_{i}', punch(mix((pitch(cut(spell['Fire'][(i + 2) % 5], 1.1), .8), 0, 0), (pitch(cut(soft_h[i], .5), .7), -2, 0)), 2.2), -0.5)
    save(f'crit_frost_{i}', punch(mix((cut(glass_h[i], .45), 0, 0), (pitch(cut(spell['Ice'][(i + 2) % 5], .8), .85), -3, 0), (pitch(cut(soft_h[i], .4), .8), -5, 0)), 2.0), -0.5)
    save(f'crit_storm_{i}', punch(mix((pitch(cut(spell['Lightning'][(i + 2) % 5], 1.0), .8), 0, 0), (pitch(cut(soft_h[i], .5), .7), -2, 0)), 2.2), -0.5)
    boom = mix((pitch(cut(soft_h[i], .7), .6), 0, 0), (pitch(cut(bell[i], .9), .7), -10, 0), (pitch(cut(punch_h[i], .5), .7), -4, 0))
    save(f'crit_magic_{i}', punch(filt(boom, 'lowpass', 3500), 2.4), -0.5)
    # --- A dodge: the whoosh of the blade before it would have landed --------
    w = swords[(i * 2 + 1) % len(swords)]
    whoosh = w[max(0, onset(w, .9) - int(.28 * RATE)): onset(w, .9) - int(.02 * RATE)]
    save(f'dodge_{i}', filt(whoosh, 'highpass', 500), -6)

# --- Level up: a reversed bell swells into a deep, ringing strike with a low
# boom beneath and a faint shimmer above (in the spirit of Path of Exile's).
def tail(x, seconds=2.2, wet=.35):
    n = int(seconds * RATE); rng = np.random.default_rng(7)
    ir = rng.standard_normal(n) * np.exp(-np.linspace(0, 7, n))
    ir = filt(ir, 'lowpass', 3500)
    wetx = np.pad(fftconvolve(x, ir), (0, 1))[: len(x) + n]
    wetx *= np.max(np.abs(x)) / (np.max(np.abs(wetx)) + 1e-12)
    return np.concatenate([x, np.zeros(n)]) * (1 - wet) + wetx * wet

rise = .9
swell = np.concatenate([pitch(bell[i], r)[::-1] for i, r in ((1, .5), (2, .75))][:1])
swell = swell[-int(rise * RATE):] * np.linspace(0, 1, int(rise * RATE)) ** 3
shimmer = pitch(cut(glass_l[2], .4), 2.0)[::-1]
shimmer = shimmer[-int(rise * RATE):] * np.linspace(0, 1, len(shimmer[-int(rise * RATE):])) ** 2
strike = mix((filt(pitch(cut(soft_h[0], .9), .4), 'lowpass', 400), 0, 0),       # the deep boom
             (pitch(bell[1], .5), -2, 0), (pitch(bell[0], .75), -5, 0),           # root and fifth
             (pitch(bell[2], 1.0), -9, .015), (pitch(bell[3], 1.5), -16, .03))    # octave, and a high glint
lvl = mix((swell, -4, 0), (shimmer, -14, 0), (strike, 0, rise))
save('levelup_0', tail(filt(lvl, 'highpass', 35), 2.4, .4))
for i in range(2):
    save(f'death_{i}', punch(mix((pitch(cut(soft_h[i], .9), .45), 0, 0), (pitch(cut(wood_h[i], .6), .5), -4, 0),
                                 (pitch(bell[i], .45), -12, .05)), 2.0))


# --- Monster voices: voice_<kind>_<alert|hurt|death>_<n> ---------------------
C = lambda n: load(f'creature/{n}.ogg')
def v(name, x, low=60, high=None):
    x = filt(x, 'highpass', low)
    if high: x = filt(x, 'lowpass', high)
    save(name, fade(x, .25))
def rattle(seed, hits, spread, p=1.0):
    # Bones: a scatter of dry clicks, not a voice.
    rng = np.random.default_rng(seed)
    layers = [(pitch(cut(load(f'{K}impactWood_light_00{rng.integers(5)}.ogg'), .12), p * rng.uniform(1.3, 2.2)), -rng.uniform(0, 9), rng.uniform(0, spread))
              for _ in range(hits)]
    return mix(*layers)
GROUPS = {
    'goblin': ([('grunt_02', 1.35), ('grunt_04', 1.4), ('grunt_05', 1.3)],
               [('hurt_01', 1.45), ('hurt_02', 1.4), ('hurt_04', 1.5)],
               [('scream_01', 1.3), ('scream_02', 1.35), ('hurt_03', 1.15)]),
    'brute':  ([('roar_02', .8), ('troll_02', .8), ('roar_03', .75)],
               [('troll_01', .85), ('troll_03', .85), ('grunt_03', .7)],
               [('roar_02', .6), ('monster_04', .65), ('troll_02', .6)]),
    'beast':  ([('howl', .9), ('monster_02', .85), ('barking_02', .7)],
               [('monster_05', .9), ('barking_01', .8), ('misc_06', .8)],
               [('howl', .7), ('monster_06', .7), ('monster_03', .7)]),
    'spider': ([('bug_01', 1.0), ('bug_02', .9), ('bug_04', 1.1)],
               [('bug_03', 1.2), ('spit_03', 1.1), ('bug_04', 1.3)],
               [('bug_01', .7), ('spit_02', .8), ('bug_02', .65)]),
    'spirit': ([('weird_01', .7), ('alien_02', .6), ('weird_05', .65)],
               [('weird_02', .75), ('alien_05', .7), ('weird_03', .7)],
               [('alien_03', .5), ('ooh', .5), ('alien_01', .5)]),
    'human':  ([('grunt_01', 1.0), ('grunt_03', 1.05), ('grunt_02', .95)],
               [('hurt_01', 1.0), ('hurt_03', 1.0), ('hurt_05', .95)],
               [('scream_01', .95), ('scream_02', 1.0), ('hurt_02', .85)]),
    'drowned':([('burble_01', .7), ('monster_07', .7), ('burble_02', .65)],
               [('burp_01', .8), ('cough_03', .7), ('burble_02', .8)],
               [('monster_01', .6), ('burble_01', .55), ('monster_07', .55)]),
}
for kind, events in GROUPS.items():
    for event, picks in zip(('alert', 'hurt', 'death'), events):
        for i, (src, r) in enumerate(picks):
            x = pitch(C(src), r)
            if kind == 'spirit': x = tail(filt(x, 'lowpass', 2600), 1.0, .45)
            if kind == 'drowned': x = filt(x, 'lowpass', 1800)
            if kind == 'brute' and event == 'death': x = mix((x, 0, 0), (filt(pitch(cut(soft_h[i], .6), .45), 'lowpass', 300), -3, .25))
            v(f'voice_{kind}_{event}_{i}', x)
for i in range(3):
    v(f'voice_bones_alert_{i}', rattle(10 + i, 7, .35))
    v(f'voice_bones_hurt_{i}', mix((pitch(cut(wood_h[i], .25), 1.6), 0, 0), (rattle(20 + i, 4, .15), -4, .03)))
    v(f'voice_bones_death_{i}', mix((pitch(cut(wood_h[i + 1], .4), 1.2), 0, 0), (rattle(30 + i, 16, .7, .8), -2, .05)))


# --- Chests: a latch, a creaking lid, and coin as the loot spills; mimics bite.
R = lambda n: load(f'rpg/Audio/{n}.ogg')
for i, (creak, coins) in enumerate((('creak1', 'handleCoins'), ('creak2', 'handleCoins2'), ('creak3', 'handleCoins'))):
    save(f'chest_open_{i}', mix((R('metalLatch'), -2, 0), (pitch(R(creak), .8 + .08 * i), 0, .06),
                                (R(coins), -6, .45 + .05 * i), (pitch(cut(wood_h[i], .3), .9), -10, .5)))
for i, (roar, r) in enumerate((('roar_02', .7), ('monster_04', .75), ('troll_02', .65))):
    save(f'mimic_{i}', punch(mix((pitch(R('creak2'), 1.3), -4, 0), (pitch(cut(wood_h[i], .4), .7), 0, .05),
                                 (pitch(C(roar), r), -1, .08), (pitch(cut(punch_h[i], .3), .8), -4, .05)), 1.8))

print(len(glob.glob(os.path.join(OUT, '*.ogg'))), 'files,', sum(os.path.getsize(p) for p in glob.glob(os.path.join(OUT, '*.ogg'))) // 1024, 'KB')
