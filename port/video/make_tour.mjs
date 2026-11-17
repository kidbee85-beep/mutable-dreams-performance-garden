// Mutable Dreams · Performance Garden – the feature tour (0.31), by Kid Bunka. About 5 minutes: an overview of the main functions, then a closer look at the performance details.
// Plays the real firmware through a fixed script: stage frames (1280 x 720 RGB565, 30 fps) to stdout, the sound to argv[3]
// (44.1 kHz WAV), the captions (chapter | line) to argv[4]. Feed stdout into encode.swift with the band:
//   node video/make_tour.mjs build/mutable-dreams-garden.wasm tour.wav tour.tsv | md-encode 1280 720 30 tour.wav tour.mp4 tour.tsv 110
import fs from "node:fs";
import {instantiate} from "../test_host.mjs";
const [wasmPath, wavPath, tsvPath] = process.argv.slice(2);
const rate = 44100, fps = 30, perFrame = rate / fps, W = 1280, H = 720;
// a host media store in memory, so REC can write its take (nothing is written to disk)
const media = {store: (op, buf, np, nl, dp, len) => {
  if (op === 6) return 0;
  if (op === 10) { new Uint8Array(buf, dp, len).set(Buffer.from('sd/dream-take.wav\0')); return 1; }
  if (op === 11 || op === 12) return 0;
  return -1; }};
const e = instantiate(new WebAssembly.Module(fs.readFileSync(wasmPath)), new Map(), null, media).exports;
e.vibe_wasm_init(rate);
const SHIFT = 6, RETURN = 16, HOME = 17;
const btn = (id) => { e.vibe_wasm_event(3, id, 1, 0, 0); e.vibe_wasm_event(3, id, 0, 0, 0); };
const hold = (id, down) => e.vibe_wasm_event(3, id, down ? 1 : 0, 0, 0);
const pad = (id, down, v = .85) => e.vibe_wasm_event(1, id, down ? 1 : 0, down ? v : 0, 0);
const tap = (id, v = .85) => { pad(id, 1, v); pad(id, 0); };
const enc = (k, v) => e.vibe_wasm_event(4, k, 0, v, 0);
const open = (v) => { btn(19); enc(0, -60); enc(0, v); btn(9); };
const pick = (v) => { enc(0, -60); enc(0, v); btn(9); };          // the directory is open already (PAGES opens it)
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
const click = (x, y) => { e.vibe_wasm_event(19, 0, 1, at(x, y), 0); e.vibe_wasm_event(19, 2, 0, at(x, y), 0); };
const GARDEN = () => click(660, 26), PLACES = () => click(728, 26), PAGES = () => click(790, 26);
const LIVE = 0, BROWSER = 1, MIXER = 5, ENVELOPE = 6, FILTER = 7, LFO_A = 9, DRIFT = 13, HARMONY = 15, PROGRESSION = 16, PADS = 20, SPACE = 21,
      LISTEN = 25, SING = 26, SEQ_GRID = 32, KIT = 35, RADIO = 39, ITINERA = 40, JOURNEY = 41, BANKS = 44, SYSTEM = 45, MIDI = 46, CV = 49;

// a synthetic singing voice (harmonics, vibrato, breath) for the "garden listens" chapter
let seed = 7; const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296;
const phrases = [[134, [[62, .5], [65, .5], [69, .9]]], [139, [[69, .4], [67, .4], [65, .4], [62, 1.0]]],
  [238.6, [[57, 1.3], [60, 1.3], [64, 1.8]]], [245.2, [[62, 1.6]]], [249.7, [[67, 1.0]]]];   // MOVES, KEY, HOLD
const notes = []; for (const [s, ns] of phrases) { let t = s; for (const [m, l] of ns) { notes.push([t, l, m]); t += l + .06; } }
let phase = 0, breath = 0;
const voice = (t) => {
  const n = notes.find(([s, l]) => t >= s && t < s + l); if (!n) return 0;
  const [s, l, m] = n, within = t - s, f = 440 * 2 ** ((m - 69) / 12) * (1 + .006 * Math.sin(2 * Math.PI * 5.2 * t) * Math.min(1, within / .25));
  phase += f / rate; let v = 0; for (let h = 1; h <= 7; h++) v += Math.sin(2 * Math.PI * phase * h) * (h === 1 ? 1 : .55 / h) * (h === 3 || h === 4 ? 1.4 : 1);
  breath += (rnd() * 2 - 1 - breath) * .2; const env = Math.min(1, within / .04, (l - within) / .08);
  return (.09 * v + .006 * breath) * env;   // (a voice at a natural level, not on top of the music)
};

// the script: [second, action] and the captions: [from, to, "CHAPTER | line"]
const script = [], captions = [];
const at_ = (t, f) => script.push([t, f]);
const say = (t0, t1, text) => captions.push([t0, t1, text]);

// 0 · intro
at_(0, () => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), 1).set([28]); e.vibe_wasm_editor_data_in(1); });   // FIRST STEPS closed: the intro plays
say(.6, 5.6, "Mutable Dreams · Performance Garden | Kid Bunka · Mutable Dreams 2.1, built with the VIBE FM SDK");
// 1 · play
at_(5.7, () => { enc(4, -38); enc(5, -15); });   // A TEXTURE 24%, B SPACE 70%
at_(6, () => e.vibe_wasm_event(8, 0, 1, 0, 0));
say(6, 11.6, "Play | A Biome wakes up: six tracks of evolving music in one key and tempo");
// 2 · macros
for (let k = 0; k < 4; k++) for (let i = 0; i < 12; i++)
  at_(12 + k * 3 + i * .25, () => enc(k, k === 3 ? -2 : 3));
say(12, 23.6, "Four macros | Density, motion, colour and space shape the whole garden – they are its weather");
// 3 · pads
for (let i = 0; i < 4; i++) {
  at_(24 + i * .9, () => pad(i, 1, .8)); at_(24.8 + i * .9, () => pad(i, 0));
}
say(24, 27.8, "Tone pads 1–4 | Root, fifth, third, ninth – notes, chords or arpeggios in the Biome's key");
for (let i = 0; i < 4; i++) {
  at_(28 + i * 1.65, () => pad(4 + i, 1, i === 3 ? .65 : .4)); at_(28.5 + i * 1.65, () => pad(4 + i, 0));
}
say(28, 34.8, "Texture pads 5–8 | Forest, air and water; stardust follows the Biome's key");
for (let i = 0; i < 4; i++) {
  at_(35 + i * 1.65, () => pad(8 + i, 1, .55)); at_(35.8 + i * 1.65, () => pad(8 + i, 0));
}
say(35, 41.8, "Space pads 9–12 | Bloom, echo, grain, dusk – open the room, let it repeat, scatter it, darken it");
at_(42, () => pad(12, 1, .8)); at_(44, () => { pad(12, 0); pad(13, 1, .8); }); at_(46, () => pad(13, 0));
say(42, 46.8, "Form pads 13–16 | Thin lets tracks rest, pulse breathes on the eighths, next and gesture follow");
// 4 · gesture
at_(47, () => tap(15)); at_(47.4, () => { pad(7, 1, .65); pad(8, 1, .5); });
at_(48.4, () => { pad(7, 0); pad(8, 0); });
at_(49.6, () => { pad(4, 1, .3); pad(9, 1, .4); });
at_(50.2, () => { pad(4, 0); pad(9, 0); }); at_(52.2, () => tap(15));
say(47, 52, "The gesture | Tap pad 16 and play textures and spaces …");
say(52.2, 58.8, "The gesture | … tap 16 again: your hands loop by themselves, the lanes show them");
// 5 · biomes, variation
at_(59, () => tap(14));
say(59, 63.6, "Next | Pad 15 moves to the next of six favourite Biomes – the gesture keeps going");
at_(64, () => { hold(SHIFT, 1); tap(14); hold(SHIFT, 0); });
at_(67, () => { hold(SHIFT, 1); btn(RETURN); hold(SHIFT, 0); });
say(64, 68.6, "Variation | SHIFT + 15: the same Biome, newly seeded – SHIFT + RETURN takes it back");
at_(68.5, () => tap(15));   // the gesture has had sixteen seconds; leave room for the music
// 6 · the whole instrument: pages
at_(69, () => PAGES());
say(69, 74.6, "All 50 pages | The display at twice its size, with page controls beside it");
at_(75, () => pick(BROWSER)); at_(76.5, () => enc(1, 4)); at_(78, () => btn(4));
say(75, 80.6, "Browser | 271 Biomes in 19 families; browse Kid Bunka's 48 worlds");
at_(81, () => { btn(4); open(PADS); }); at_(82.5, () => { hold(SHIFT, 1); btn(0); hold(SHIFT, 0); }); at_(84, () => btn(0));
at_(86, () => btn(0));  // release the recalled moment before continuing
say(81, 86.6, "Moments | On PADS: SHIFT + F stores a performance snapshot; F recalls it");
at_(87, () => open(MIXER)); at_(88.5, () => btn(1)); at_(89.5, () => enc(0, 4));
say(87, 91.6, "Mixer | Six tracks, 24 sound engines from Plaits, Rings, Elements and Mutable Dreams");
at_(92, () => open(FILTER)); at_(93, () => enc(0, -6)); at_(94, () => open(ENVELOPE));
say(92, 95.6, "Filter and envelope | LP12, LP24 or LPG filtering; attack up to 8 s, decay up to 16 s");
at_(96, () => open(LFO_A)); at_(98, () => open(DRIFT));
say(96, 100.6, "Motion | Marbles, Tides, two LFOs, links between tracks, drift between two scenes");
at_(101, () => open(HARMONY)); at_(102.5, () => open(PROGRESSION)); at_(103.5, () => enc(0, 1));
say(101, 105.6, "Harmony | 13 scales, chord progressions, a bassline with its own sixteen steps");
at_(106, () => open(SEQ_GRID)); at_(108, () => open(KIT));
say(106, 110.6, "Sequencer | Euclidean rhythms, steps with rules and fills, generators, a lo-fi drum kit");
at_(111, () => open(SPACE));
say(111, 114.6, "Effects | The echo cave, the spore meadow, the grain cloud, the tape");
// 7 · places
at_(115, () => { btn(HOME); btn(HOME); PLACES(); }); at_(116, () => pad(9, 1, .55)); at_(117.1, () => pad(9, 0)); at_(119, () => pad(10, 1, .45)); at_(120.1, () => pad(10, 0));
say(115, 122.6, "Places | Where the sound goes: cave, meadow, stream, reeds, the moon pond, the singing flower");
// 8 · the garden listens
at_(123, () => { PAGES(); pick(LISTEN); btn(0); enc(0, -70); enc(0, 4); });   // THROUGH 20; keep the detector input at .09, soften only the dry return
say(123, 127.6, "Audio input | LISTEN routes IN 1 / IN 2 through cloud, echo and room");
at_(128, () => { open(SING); btn(3); });
say(128, 133.6, "Sing | Follow, harmony or answer: the garden plays along with your voice");
at_(133.5, () => PLACES());
say(134, 147, "Call and response | A synthetic voice in IN 1 sings a phrase and stops – the garden answers");
at_(147.5, () => { PAGES(); pick(CV); btn(1); }); at_(149, () => enc(3, 2));
say(148, 153.6, "CV and gate | CV OUT and GATE OUT follow a track; CV IN can control a macro");
// 9 · long forms
at_(154, () => { open(RADIO); btn(0); });
at_(154.4, () => btn(1));   // demonstrate SKIP: leave the sparse Six Kicks opening
at_(155, () => GARDEN());
say(154, 161.6, "Mutable Dreams FM | A radio station inside the instrument; SKIP finds the next piece");
at_(162, () => { PAGES(); pick(RADIO); btn(2); open(LIVE); btn(1); open(ITINERA); });   // off air, explicitly restore Dust Afterglow (favourite 2)
say(162, 166.6, "Itinera | Fourteen long dreams, from thirty minutes to four hours");
at_(167, () => open(JOURNEY)); at_(169.5, () => open(BANKS));
say(167, 171.6, "Journeys and banks | Chain Biomes into a journey; sixteen banks keep whole sound projects");
// 10 · studio
at_(172, () => open(MIDI)); at_(173, () => btn(0));
say(172, 177.6, "In the studio | 30 DAW parameters, song position, stems, Dream as MIDI, MIDI learn, REC takes");
at_(178, () => btn(HOME));
at_(180, () => { btn(HOME); hold(0, 1); }); at_(181, () => hold(0, 0));
say(178, 182.6, "Help | HOME explains the page; hold an F key to read its help");
at_(183, () => { btn(HOME); GARDEN(); click(606, 26); });
at_(184, () => pad(8, 1, .8)); at_(185.5, () => pad(8, 0));
at_(187, () => enc(0, 4));
say(183, 189.6, "First steps | A guided tour for new players – each card moves on when you have done it");
// 11 · a closer look (added after Codex's video review: show what the overview only named)
const REC = 21, DRIFT_SCENES = 14, BASSLINE = 17, SEQ_RULES = 34, LEARN = 47;
at_(190, () => { btn(HOME); GARDEN(); hold(SHIFT, 1); pad(8, 1, .55); hold(SHIFT, 0); });          // latch BLOOM
for (let i = 0; i < 8; i++) at_(192 + i * .3, () => enc(5, -2)); for (let i = 0; i < 8; i++) at_(195 + i * .3, () => enc(5, 2));
at_(198, () => { hold(SHIFT, 1); pad(8, 1, .8); hold(SHIFT, 0); });                                    // release the latch
say(190, 198.6, "Latch and depth | SHIFT + pad keeps a pad playing; encoders A and B set how deep the rows go");
at_(199, () => { PAGES(); pick(PADS); hold(SHIFT, 1); enc(0, 1); hold(SHIFT, 0); });                // tone mode: chord
at_(200, () => { pad(0, 1, .8); }); at_(201.5, () => { pad(0, 0); pad(2, 1, .8); }); at_(203, () => { pad(2, 0); hold(SHIFT, 1); enc(0, 1); hold(SHIFT, 0); });
at_(203.5, () => pad(0, 1, .8)); at_(206, () => { pad(0, 0); hold(SHIFT, 1); enc(0, -1); enc(0, -1); hold(SHIFT, 0); });
say(199, 206.6, "Tone modes | Note, chord or arpeggio in time – SHIFT + encoder 1 on PADS");
// Start a fresh short take: the overview loop was stopped at 68.5 s.
at_(206.1, () => tap(15));
at_(206.2, () => { pad(7, 1, .5); pad(9, 1, .3); });
at_(206.7, () => { pad(7, 0); pad(9, 0); }); at_(206.9, () => tap(15));
at_(207, () => { hold(SHIFT, 1); tap(15); hold(SHIFT, 0); });                                         // layer onto the loop
at_(207.3, () => { pad(6, 1, .25); pad(11, 1, .35); }); at_(208.2, () => { pad(6, 0); pad(11, 0); });
at_(211, () => { hold(SHIFT, 1); enc(1, 1); hold(SHIFT, 0); });                                       // playback: double
at_(213.5, () => { hold(SHIFT, 1); enc(1, 1); hold(SHIFT, 0); });                                     // reverse
at_(216, () => { hold(SHIFT, 1); enc(1, -1); enc(1, -1); hold(SHIFT, 0); });
say(207, 216.6, "Layer and speed | SHIFT + 16 layers more moves into the loop; it plays half, double or reversed");
at_(217, () => { tap(15); hold(SHIFT, 1); enc(2, 1); hold(SHIFT, 0); GARDEN(); });                  // loop off, wander on
say(217, 226.6, "Wander | When your hands rest, the garden plays its texture and space pads by itself");
at_(227, () => { PAGES(); pick(PADS); hold(SHIFT, 1); enc(2, -1); hold(SHIFT, 0); open(LIVE); enc(0, -20); enc(3, 10); });   // wander off on PADS
at_(228, () => { open(DRIFT_SCENES); btn(0); open(LIVE); enc(0, 30); enc(2, 20); enc(3, -20); });
at_(229, () => { open(DRIFT_SCENES); btn(1); open(DRIFT); enc(0, 2); enc(2, -60); });   // RATE 0: the quickest glide
// (stays on the DRIFT page: the dot glides between A and B)
say(227, 237.6, "Drift | Capture two scenes, and the garden glides between them by itself");
at_(238, () => { open(LISTEN); btn(4); });                                                  // MOVES: density
at_(244, () => { btn(4); btn(4); btn(4); btn(4); open(SING); btn(4); });                              // MOVES off, KEY armed
at_(249.5, () => { open(LISTEN); btn(3); });                                                           // HOLD: freeze the cloud
at_(252, () => btn(3));
say(238, 243.6, "Moves | The input's loudness lifts a macro – here DENSITY");
say(244, 249.4, "Key | Sing one note: it becomes the Biome's key");
say(249.5, 252.8, "Hold | The cloud freezes with the input inside");
at_(253, () => { open(SING); btn(0); open(LISTEN); btn(0); open(SEQ_RULES); btn(0); }); at_(256, () => { open(BASSLINE); enc(0, 1); });
say(253, 259.6, "Fills and bass | F1 queues a fill on SEQ RULES; the bassline plays Euclid or its own steps");
at_(260, () => { open(JOURNEY); enc(0, 2); enc(3, 3); });
say(260, 265.6, "Journey | Up to eight Biomes in a row, each with bars, repeats and the tracks it keeps");
at_(266, () => { open(LEARN); btn(2); });
for (let i = 0; i <= 16; i++) at_(267 + i * .2, () => e.vibe_wasm_event(5, 0, 0, 0, 0xb0 | (74 << 8) | ((i * 8 > 127 ? 127 : i * 8) << 16)));
at_(271, () => open(LIVE));
for (let i = 16; i >= 6; i--) at_(271.2 + (16 - i) * .2, () => e.vibe_wasm_event(5, 0, 0, 0, 0xb0 | (74 << 8) | ((i * 8 > 127 ? 127 : i * 8) << 16)));
say(266, 273.6, "MIDI learn | Move a controller: a CC now moves DENSITY – or level, cutoff, a send, the drift");
at_(274, () => { GARDEN(); btn(REC); }); at_(281, () => btn(REC));
say(274, 282.6, "REC | A take of what you hear, written as a WAV file to the host's media folder");
// 12 · outro
at_(283, () => pad(8, 1, .5)); at_(288, () => pad(8, 0));
say(283, 293.6, "Mutable Dreams · Performance Garden | Kid Bunka · built with the VIBE FM SDK · free software, GPL-3.0-only");
script.sort((a, b) => a[0] - b[0]);
if (process.env.CAPTIONS_ONLY) { fs.writeFileSync(tsvPath, captions.map(c => c.join("\t")).join("\n") + "\n"); process.exit(0); }

const seconds = 295, frames = seconds * fps;
const raw = new Float32Array(seconds * rate * 2);   // (the mix as rendered; levelled at the end)
let w = 0, t = 0, si = 0;
const frameBuf = Buffer.alloc(W * H * 2);
for (let f = 0; f < frames; f++) {
  while (si < script.length && script[si][0] <= t + 1e-9) { script[si][1](); si++; }
  for (let k = 0; k < perFrame; k += 147) {
    const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
    for (let i = 0; i < 147; i++) { const v = voice(t + i / rate); io[i] = io[768 + i] = v; }
    e.vibe_wasm_audio(147, 3);
    const o = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
    for (let i = 0; i < 147 && w < raw.length; i++) for (const c of [256, 512]) raw[w++] = o[c + i];
    t += 147 / rate;
  }
  e.vibe_wasm_draw();
  frameBuf.set(new Uint8Array(e.memory.buffer, e.vibe_wasm_stage_draw(), W * H * 2));
  fs.writeSync(1, frameBuf);
}
// Offline master: K-weighted BS.1770 stereo integrated loudness, 400 ms blocks / 100 ms hop,
// absolute -70 LUFS and relative -10 LU gates. Never infer LUFS from unweighted RMS.
function integratedLoudness(a) {
  const shelf = (f, g, q) => {
    const k = Math.tan(Math.PI * f / rate), vh = 10 ** (g / 20), vb = vh ** .4996667741545416;
    const d = 1 + k / q + k * k;
    return [(vh + vb * k / q + k * k) / d, 2 * (k * k - vh) / d,
      (vh - vb * k / q + k * k) / d, 2 * (k * k - 1) / d, (1 - k / q + k * k) / d];
  };
  const k = Math.tan(Math.PI * 38.13547087602444 / rate), q = .5003270373238773, d = 1 + k / q + k * k;
  const filters = [shelf(1681.974450955533, 3.999843853973347, .7071752369554196),
    [1, -2, 1, 2 * (k * k - 1) / d, (1 - k / q + k * k) / d]];
  const state = Array.from({length: 2}, () => filters.map(() => [0, 0]));
  const bins = [], hop = Math.round(.1 * rate); let power = 0, n = 0;
  for (let i = 0; i < a.length; i += 2) {
    for (let c = 0; c < 2; c++) {
      let y = a[i + c];
      for (let j = 0; j < filters.length; j++) {
        const [b0, b1, b2, a1, a2] = filters[j], z = state[c][j], v = b0 * y + z[0];
        z[0] = b1 * y - a1 * v + z[1]; z[1] = b2 * y - a2 * v; y = v;
      }
      power += y * y;
    }
    if (++n === hop) { bins.push(power / hop); n = 0; power = 0; }
  }
  const blocks = []; for (let i = 0; i + 3 < bins.length; i++) blocks.push((bins[i] + bins[i + 1] + bins[i + 2] + bins[i + 3]) / 4);
  const level = p => -.691 + 10 * Math.log10(p);
  const mean = ps => ps.reduce((s, p) => s + p, 0) / Math.max(1, ps.length);
  const absolute = blocks.filter(p => level(p) > -70), gate = level(mean(absolute)) - 10;
  return level(mean(absolute.filter(p => level(p) > gate)));
}
// Four-times polyphase reconstruction, 64-tap Blackman-windowed sinc; both channels.
// Leave 0.25 dB margin under -1 dBTP, and verify the delivered PCM independently at 8x.
function reconstructedPeak(a) {
  let peak = 0; for (const v of a) { if (!Number.isFinite(v)) throw new Error("Non-finite firmware audio"); peak = Math.max(peak, Math.abs(v)); }
  const taps = 64, frames = a.length / 2;
  for (let phase = 1; phase < 4; phase++) {
    const h = new Float64Array(taps); let sum = 0;
    for (let j = 0; j < taps; j++) {
      const x = j - taps / 2 + 1 - phase / 4;
      h[j] = Math.sin(Math.PI * x) / (Math.PI * x) * (.42 + .5 * Math.cos(2 * Math.PI * x / taps) + .08 * Math.cos(4 * Math.PI * x / taps));
      sum += h[j];
    }
    for (let j = 0; j < taps; j++) h[j] /= sum;
    for (let f = 0; f < frames; f++) {
      let l = 0, r = 0;
      for (let j = 0; j < taps; j++) {
        const k = f + j - taps / 2 + 1;
        if (k >= 0 && k < frames) { l += a[2 * k] * h[j]; r += a[2 * k + 1] * h[j]; }
      }
      peak = Math.max(peak, Math.abs(l), Math.abs(r));
    }
  }
  return peak;
}
// The first sound starts after the silent title. Fade that entrance, then give the outro 3.5 s.
for (let f = 0; f < raw.length / 2; f++) {
  const second = f / rate;
  const fade = Math.max(0, Math.min(1, (second - 5.6) / 1, (seconds - second) / 3.5));
  const smooth = fade * fade * (3 - 2 * fade);
  raw[2 * f] *= smooth; raw[2 * f + 1] *= smooth;
}
const before = integratedLoudness(raw), peak = reconstructedPeak(raw);
const gain = Number.isFinite(before) ? Math.min(10 ** ((-16 - before) / 20), 10 ** (-1.25 / 20) / Math.max(peak, 1e-12)) : 1;
const pcm = new Int16Array(raw.length);
for (let i = 0; i < raw.length; i++) {
  const sample = raw[i] * gain;
  if (!Number.isFinite(sample) || Math.abs(sample) >= 1) throw new Error("Invalid master: clipping");
  pcm[i] = Math.round(sample * 32767);
}
process.stderr.write(`master: ${(before + 20 * Math.log10(gain)).toFixed(2)} LUFS-I, ${(20 * Math.log10(peak * gain)).toFixed(2)} dBTP (4x), gain ${(20 * Math.log10(gain)).toFixed(2)} dB\n`);

const head = Buffer.alloc(44); head.write("RIFF"); head.writeUInt32LE(36 + pcm.length * 2, 4); head.write("WAVEfmt ", 8); head.writeUInt32LE(16, 16);
head.writeUInt16LE(1, 20); head.writeUInt16LE(2, 22); head.writeUInt32LE(rate, 24); head.writeUInt32LE(rate * 4, 28); head.writeUInt16LE(4, 32); head.writeUInt16LE(16, 34);
head.write("data", 36); head.writeUInt32LE(pcm.length * 2, 40);
fs.writeFileSync(wavPath, Buffer.concat([head, Buffer.from(pcm.buffer)]));
if (tsvPath) fs.writeFileSync(tsvPath, captions.map(c => c.join("\t")).join("\n") + "\n");
process.stderr.write(`frames ${frames}, audio ${(w / 2 / rate).toFixed(1)} s, captions ${captions.length}\n`);
