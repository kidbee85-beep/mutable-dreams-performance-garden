#!/usr/bin/env python3
"""The 48 worlds of the Trimui Brick port, for the MPC. (Copied into the VIBE FM port so its worlds can be rebuilt
from this folder alone: tools/make_worlds.py uses parse() and convert(); main() is the MPC's and is not run here.)

    python3 MPC/tools/make_worlds.py   ->  MPC/core/garden_worlds.inc, MPC/Welten-MPC.md

Reads Trimui-Brick/port/package/knulli/mutabledreams/worlds/worlds.txt (read only) and keeps what the MPC instrument
has: the Biome, tempo, macros, the engine settings and the texture lanes. The Brick's Sound Lab layers become a
POLY layer (eight Plaits voices) that plays with the dream:
  - POLY.* settings are taken as they are
  - SYNTH (six engines of its own) becomes the nearest Plaits model, played as a drone or with the melody
  - LOGIC (mathematical melodies) becomes POLY doubling the Biome's melody track
  - SHIMMER becomes the BLOOM lane, CLOUD / PARTICLES / SEEDS the GRAIN lane, at reduced depth
Everything else of the Lab (voices, samples, colour, crossmod, sends) has no counterpart and is listed in the report.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
MPC = os.path.dirname(HERE)
SRC = os.path.join(HERE, "worlds.txt")   # (VIBE FM release: a copy of the Brick port's worlds.txt, kept beside this converter)

ENGINE = {"MUTE": ("MD_MUTE", True, 1), "LEVEL": ("MD_LEVEL", True, 10), "SEND": ("MD_SEND", True, 10),
          "CUTOFF": ("MD_CUTOFF", True, 10), "RESONANCE": ("MD_RESONANCE", True, 10), "DECAY": ("MD_DECAY", True, 10),
          "TONE": ("MD_TONE", True, 10), "HARM": ("MD_HARM", True, 10), "PAN": ("MD_PAN", True, 10),
          "ATTACK": ("MD_ATTACK", True, 10), "BREATHE": ("MD_BREATHE", True, 10), "PROB": ("MD_PROB", True, 10),
          "SWING": ("MD_SWING", False, 10), "DELAY_TIME": ("MD_DELAY_TIME", False, 1), "DELAY_FB": ("MD_DELAY_FB", False, 10),
          "REVERB_TIME": ("MD_REVERB_TIME", False, 10), "FX_LEVEL": ("MD_FX_LEVEL", False, 10),
          "GRAIN_SIZE": ("MD_GRAIN_SIZE", False, 10), "GRAIN_POS": ("MD_GRAIN_POS", False, 10),
          "FEEDBACK": ("MD_FEEDBACK", False, 10), "GRAIN_PITCH": ("MD_GRAIN_PITCH", False, 1), "REVERSE": ("MD_REVERSE", False, 1),
          "FREEZE": ("MD_FREEZE", False, 1), "ROOT": ("MD_ROOT", False, 1), "TRANSPOSE": ("MD_TRANSPOSE", False, 1)}
# Trimui POLY defaults (brick_lab.inc) and the follow modes of the MPC: 0 off, 1-6 a track, 7 the melody, 8 a drone
POLY_DEFAULT = {"LEVEL": 0, "MODEL": 8, "HARMONICS": 40, "TIMBRE": 45, "MORPH": 40, "ATTACK": 25, "DECAY": 45, "SUSTAIN": 70,
                "RELEASE": 55, "GATE_COLOUR_(LPG)": 50, "VOICES": 6, "STEREO_SPREAD": 50, "OCTAVE": 0}
FOLLOW_MELODY, FOLLOW_DRONE = 7, 8
SYNTH_ENGINES = ["Just Drone", "Modal Garden", "Shepard", "Karplus Rain", "Golden Bells", "Formant Choir"]
# SYNTH engine -> Plaits model, drone (True) or melodic (False), attack, sustain
SYNTH_TO_PLAITS = {0: (12, True, 70, 90), 1: (20, False, 0, 0), 2: (13, True, 80, 90), 3: (19, False, 0, 0),
                   4: (20, False, 0, 0), 5: (11, True, 60, 85)}
PLAITS = ["Analog Filter", "Phase Bloom", "Six-Op FM I", "Six-Op FM II", "Six-Op FM III", "Wave Terrain", "String Machine",
          "Chiptune", "Twin Analog", "Waveshaper", "Two-Op FM", "Grain Formant", "Additive", "Wavetable", "Chords",
          "Speech", "Swarm", "Filtered Noise", "Particle Noise", "Plucked String", "Modal Bell", "Bass Drum", "Snare", "Hi-Hat"]
LANES = ["FOREST", "AIR", "WATER", "STARDUST", "BLOOM", "ECHO", "GRAIN", "DUSK"]


def parse():
    worlds, w = [], None
    for line in open(SRC, encoding="utf-8"):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = re.match(r"\[(.+?)\]\s*(.+)", s)
        if m:
            w = {"family": m[1].title(), "name": m[2].strip(), "story": "", "biome": -1, "tempo": 0, "macros": None,
                 "engine": [], "lab": []}
            worlds.append(w)
            continue
        k, _, v = s.partition("=")
        k, v = k.strip(), v.strip()
        if k == "biome":
            w["biome"] = int(v)
        elif k == "story":
            w["story"] = v
        elif k == "tempo":
            w["tempo"] = int(v)
        elif k == "macros":
            w["macros"] = [int(x) for x in v.split()]
        elif k == "engine":
            w["engine"] = [x.split() for x in v.split(";") if x.strip()]
        elif k == "lab":
            w["lab"] = [x.strip().rsplit(" ", 1) for x in v.split(";") if x.strip()]
    return worlds


def convert(w):
    lab = {k: float(v) for k, v in w["lab"]}
    kept, changed, dropped = [], [], []
    engine = []
    for item in w["engine"]:
        name = item[0]
        if name not in ENGINE:
            dropped.append("engine " + name)
            continue
        cmd, tracked, scale = ENGINE[name]
        track = int(item[1]) - 1 if tracked else 0
        value = int(round(float(item[-1]) * scale))
        engine.append((cmd, track, value))
    lanes = [-1] * 8
    for key, v in lab.items():
        mod, _, field = key.partition(".")
        if mod == "TEXTURES" and field in LANES[:4]:
            lanes[LANES.index(field)] = int(v)
            kept.append(key)
    poly = None
    logic_drives_poly = lab.get("LOGIC.PLAY", 0) > 0 and (lab.get("LOGIC.VOICE_1_TO", 6) == 6 or lab.get("LOGIC.VOICE_2_TO", 0) == 7)
    if lab.get("POLY.LEVEL", 0) > 0:
        poly = {k: lab.get("POLY." + k, d) for k, d in POLY_DEFAULT.items()}
        # 0.9.1: LOGIC plays POLY itself now; only a POLY without LOGIC follows the melody (else it would be silent)
        poly["FOLLOW"] = int(lab.get("POLY.FOLLOW_TRACK", 0)) or (0 if logic_drives_poly else FOLLOW_MELODY)
        poly["GATE"] = int(lab.get("POLY.FOLLOW_GATE", 600))
        kept.append("POLY")
    elif False and lab.get("SYNTH.LEVEL", 0) > 0:   # (until 0.9: SYNTH became a POLY layer; now SYNTH is SYNTH)
        eng = int(lab.get("SYNTH.ENGINE", 0))
        model, drone, attack, sustain = SYNTH_TO_PLAITS.get(eng, (12, True, 60, 85))
        played = int(lab.get("SYNTH.PLAYED_BY", 0))
        follow = played - 1 if played >= 2 else (FOLLOW_DRONE if drone and played == 0 else FOLLOW_MELODY)
        decay = lab.get("SYNTH.DECAY", 55)
        poly = {"LEVEL": lab["SYNTH.LEVEL"] * 0.8, "MODEL": model, "HARMONICS": lab.get("SYNTH.CHARACTER", 30),
                "TIMBRE": lab.get("SYNTH.BRIGHTNESS", 50), "MORPH": lab.get("SYNTH.MOTION", 40), "ATTACK": attack,
                "DECAY": decay, "SUSTAIN": sustain, "RELEASE": max(decay, 50 if drone else 40), "GATE_COLOUR_(LPG)": 50,
                "VOICES": 4 if follow == FOLLOW_DRONE else 6, "STEREO_SPREAD": lab.get("SYNTH.SPREAD", 50),
                "OCTAVE": lab.get("SYNTH.OCTAVE", 0), "FOLLOW": follow, "GATE": 900 if drone else 500}
        changed.append("SYNTH %s → POLY %s (%s)" % (SYNTH_ENGINES[eng] if eng < 6 else eng, PLAITS[model],
                       "Bordun" if follow == FOLLOW_DRONE else "Melodie" if follow == FOLLOW_MELODY else "folgt Spur %d" % follow))
    # 0.9.1: LOGIC and SYNTH as they are (the Trimui lab's defaults where a world says nothing)
    ld = {"PLAY": 0, "RULE": 0, "VOICE_1_TO": 6, "VOICE_2_TO": 0, "RATE": 1, "DENSITY": 70, "SHAPE": 30, "RANGE_(OCTAVES)": 2,
          "SEED": 1, "VARIATION": 20, "GATE": 80}
    sd = {"LEVEL": 0, "ENGINE": 0, "PLAYED_BY": 0, "DENSITY": 40, "OCTAVE": 0, "DECAY": 55, "BRIGHTNESS": 50, "CHARACTER": 30,
          "MOTION": 40, "SPREAD": 50, "MODEL": 0}
    lg = {k: lab.get("LOGIC." + k, d) for k, d in ld.items()}
    sy = {k: lab.get("SYNTH." + k, d) for k, d in sd.items()}
    labs = [1] + [int(round(lg[k])) for k in ld] + [int(round(sy[k])) for k in sd]
    if lg["PLAY"]:
        kept.append("LOGIC " + ["Tintinnabuli", "Phasing", "Fibonacci", "Automaton", "Markov", "L-System", "Harmonic"][int(lg["RULE"]) % 7])
    if sy["LEVEL"] > 0:
        kept.append("SYNTH " + ["Just Drone", "Modal Garden", "Shepard", "Karplus Rain", "Golden Bells", "Formant Choir", "Rings",
                                "Elements"][int(sy["ENGINE"]) % 8])
    for key, v in lab.items():
        mod = key.split(".")[0]
        if mod == "SHIMMER" and key == "SHIMMER.MIX":
            lanes[4] = max(lanes[4], int(v * 0.7)); changed.append("SHIMMER → BLOOM")
        elif key in ("CLOUD.MIX", "PARTICLES.MIX", "SEEDS.MIX"):
            lanes[6] = max(lanes[6], int(v * 0.6)); changed.append(mod + " → GRAIN")
        elif mod in ("POLY", "SYNTH", "LOGIC", "TEXTURES", "SHIMMER", "CLOUD", "PARTICLES", "SEEDS"):
            continue
        elif mod not in [d.split()[0] for d in dropped]:
            dropped.append(mod)
    return engine, lanes, poly, kept, sorted(set(changed)), sorted(set(dropped)), labs


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    worlds = parse()
    families = []
    for w in worlds:
        if w["family"] not in families:
            families.append(w["family"])
    out = ["/* Generated by MPC/tools/make_worlds.py from the Trimui Brick port's worlds.txt - do not edit by hand.",
           " * Kid Bunka's 48 worlds: Biome, tempo, macros, engine, lanes and a POLY layer (see Welten-MPC.md). */",
           "static const char *const kWorldFamilies[] = {%s};" % ", ".join(c_str(f) for f in families),
           "static const WorldSpec kWorlds[] = {"]
    report = ["# Die 48 Welten auf dem MPC", "",
              "Erzeugt von `tools/make_worlds.py` aus `Trimui-Brick/port/package/knulli/mutabledreams/worlds/worlds.txt`. Projekt **Kid Bunka**.", "",
              "Die Welten der Trimui-Fassung bauen auf dem Biome und dem Sound Lab auf. Das MPC hat kein Sound Lab. "
              "Biome, Tempo, Makros, Engine-Einstellungen und Textur-Spuren gelten unverändert; dafür spielt eine **POLY-Schicht** "
              "(Plaits-Stimmen) mit dem Traum mit. Seit 0.9.1 laufen LOGIC und SYNTH wie in der Trimui-Fassung; "
              "SHIMMER wird zur BLOOM-Spur, CLOUD/PARTICLES/SEEDS zur GRAIN-Spur.", "",
              "| Welt | Biome | MPC-Fassung | Ohne Gegenstück |", "|---|---|---|---|"]
    for w in worlds:
        engine, lanes, poly, kept, changed, dropped, labs = convert(w)
        eng = ", ".join("{%s,%d,%d}" % e for e in engine) or "{0,0,0}"
        macros = w["macros"] or [-1, -1, -1, -1]
        if poly:
            p = poly
            pl = "{1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d}" % (
                int(p["MODEL"]), round(p["HARMONICS"]), round(p["TIMBRE"]), round(p["MORPH"]), round(p["GATE_COLOUR_(LPG)"]),
                round(p["ATTACK"]), round(p["DECAY"]), round(p["SUSTAIN"]), round(p["RELEASE"]), round(p["LEVEL"]),
                int(p["VOICES"]), round(p["OCTAVE"]), round(p["STEREO_SPREAD"]), int(p["FOLLOW"]), int(p["GATE"]))
        else:
            pl = "{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}"
        out.append("    {%d, %s, %s, %d, %d, {%s}, %d, {%s}, {%s}, %s, {%s}}," % (
            families.index(w["family"]), c_str(w["name"]), c_str(w["story"]), w["biome"], w["tempo"],
            ",".join(str(m) for m in macros), len(engine), eng, ",".join(str(x) for x in lanes), pl, ",".join(str(x) for x in labs)))
        what = [k for k in kept if k.startswith(("LOGIC", "SYNTH"))]
        if poly:
            what.append("POLY " + PLAITS[int(poly["MODEL"])])
        what += changed
        if any(x >= 0 for x in lanes):
            what.append("Spuren " + ", ".join("%s %d" % (LANES[i].title(), x) for i, x in enumerate(lanes) if x >= 0))
        report.append("| %s · *%s* | %d | %s | %s |" % (w["name"], w["family"], w["biome"], "; ".join(what) or "Biome, Tempo, Makros",
                                                       ", ".join(dropped) or "–"))
    out.append("};")
    with open(os.path.join(MPC, "core", "garden_worlds.inc"), "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    with open(os.path.join(MPC, "Welten-MPC.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print("make_worlds: %d worlds in %d families" % (len(worlds), len(families)))


if __name__ == "__main__":
    main()
