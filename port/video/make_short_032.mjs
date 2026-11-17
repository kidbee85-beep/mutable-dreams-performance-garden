// Mutable Dreams · Performance Garden – a demo straight from the firmware (0.30), by Kid Bunka.
// SHORT=1 in the environment: the 60-second cut (captions-short.tsv); otherwise the 84-second tour (captions.tsv).
// Plays the real firmware through a fixed script and writes the stage (1280 x 720 RGB565, 30 fps) to stdout and its
// sound (44.1 kHz stereo WAV) to argv[3]. Feed stdout into encode.swift.
import fs from "node:fs";
import {instantiate} from "../test_host.mjs";
import {master} from "./master.mjs";
const [wasmPath, wavPath] = [process.argv[2], process.argv[3]];
const rate = 44100, fps = 30, perFrame = rate / fps, W = 1280, H = 720;
const e = instantiate(new WebAssembly.Module(fs.readFileSync(wasmPath))).exports;
e.vibe_wasm_init(rate);
const btn = (id) => { e.vibe_wasm_event(3, id, 1, 0, 0); e.vibe_wasm_event(3, id, 0, 0, 0); };
const pad = (id, down, v = .85) => e.vibe_wasm_event(1, id, down ? 1 : 0, down ? v : 0, 0);
const enc = (k, v) => e.vibe_wasm_event(4, k, 0, v, 0);
const open = (v) => { btn(19); enc(0, -60); enc(0, v); btn(9); };
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
const click = (x, y) => { e.vibe_wasm_event(19, 0, 1, at(x, y), 0); e.vibe_wasm_event(19, 2, 0, at(x, y), 0); };
// a synthetic singing voice (harmonics, vibrato, breath) – the phrases of the "garden listens" demo
let seed = 7; const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296;
const SHORT = !!process.env.SHORT;
const phrases = SHORT ? [[33.5, [[62, .5], [65, .5], [69, .9]]], [38, [[69, .4], [67, .4], [65, .4], [62, 1.0]]], [42.5, [[60, .6], [62, .3], [65, 1.2]]]] : [[46, [[62, .5], [65, .5], [69, .9]]], [51, [[69, .4], [67, .4], [65, .4], [62, 1.0]]], [56.5, [[60, .6], [62, .3], [65, 1.2]]]];
const notes = []; for (const [s, ns] of phrases) { let t = s; for (const [m, l] of ns) { notes.push([t, l, m]); t += l + .06; } }
let phase = 0, breath = 0;
const voice = (t) => {
  const n = notes.find(([s, l]) => t >= s && t < s + l); if (!n) return 0;
  const [s, l, m] = n, within = t - s, f = 440 * 2 ** ((m - 69) / 12) * (1 + .006 * Math.sin(2 * Math.PI * 5.2 * t) * Math.min(1, within / .25));
  phase += f / rate; let v = 0; for (let h = 1; h <= 7; h++) v += Math.sin(2 * Math.PI * phase * h) * (h === 1 ? 1 : .55 / h) * (h === 3 || h === 4 ? 1.4 : 1);
  breath += (rnd() * 2 - 1 - breath) * .2; const env = Math.min(1, within / .04, (l - within) / .08);
  return (.11 * Math.sin(2 * Math.PI * phase) + .025 * Math.sin(4 * Math.PI * phase)) * env;
};
// the script: [second, action]
const scriptShort = [
  [0.0, () => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), 1).set([28]); e.vibe_wasm_editor_data_in(1); }],
  [3.15, () => { enc(4, -38); enc(5, -15); }],                          // (Codex's mix) A TEXTURE 24%, B SPACE 70%
  [3.2, () => e.vibe_wasm_event(8, 0, 1, 0, 0)],                        // PLAY
  [5.0, () => pad(8, 1, .55)], [7.5, () => pad(7, 1, .65)], [9.0, () => pad(6, 1, .4)],
  [11.0, () => { pad(8, 0); pad(7, 0); pad(6, 0); }],
  [12.0, () => pad(15, 1)], [12.05, () => pad(15, 0)],                  // the gesture
  [12.4, () => { pad(7, 1, .65); pad(8, 1, .55); }], [14.6, () => { pad(7, 0); pad(8, 0); pad(6, 1, .4); pad(9, 1, .5); }],
  [16.8, () => { pad(6, 0); pad(9, 0); }],
  [17.0, () => { pad(15, 1); pad(15, 0); }],                           // finish capture: loop during its caption
  [21.0, () => { pad(14, 1); pad(14, 0); }],                            // NEXT
  [23.8, () => { pad(15, 1); pad(15, 0); }],                            // the loop ends after its chapters
  [24.0, () => click(728, 26)],                                         // PLACES
  [25.0, () => pad(9, 1, .55)], [27.5, () => { pad(9, 0); pad(10, 1, .5); }], [29.0, () => pad(10, 0)],
  [31.0, () => { open(25); btn(0); enc(0, -70); enc(0, 4); open(26); btn(3); open(0); }],   // LISTEN on (THROUGH 20), SING: ANSWER
  [31.5, () => click(660, 26)],                                         // GARDEN: the rain of light
  [40.5, () => click(728, 26)],                                         // PLACES: moon pond, singing flower
  [46.0, () => click(790, 26)],                                         // PAGES
  [48.0, () => enc(0, 6)], [49.5, () => btn(9)],
  [51.0, () => enc(1, 12)], [52.5, () => enc(1, -12)],
  [54.0, () => click(660, 26)],                                         // back to the garden
  [55.0, () => pad(8, 1, .55)], [59.0, () => pad(8, 0)],
];
const script = [
  [0.0, () => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), 1).set([28]); e.vibe_wasm_editor_data_in(1); }],   // FIRST STEPS closed (console ?), the intro plays on
  [3.15, () => { enc(4, -38); enc(5, -15); }],                          // (Codex's mix) A TEXTURE 24%, B SPACE 70%
  [3.2, () => e.vibe_wasm_event(8, 0, 1, 0, 0)],                        // PLAY
  [6.0, () => pad(8, 1, .55)], [10.5, () => pad(7, 1, .65)], [12.5, () => pad(6, 1, .4)],
  [15.0, () => { pad(8, 0); pad(7, 0); pad(6, 0); }],
  [16.0, () => pad(15, 1)], [16.05, () => pad(15, 0)],                  // the gesture: tap 16 …
  [16.4, () => { pad(7, 1, .65); pad(8, 1, .55); }], [18.6, () => { pad(7, 0); pad(8, 0); pad(6, 1, .4); pad(9, 1, .5); }],
  [20.8, () => { pad(6, 0); pad(9, 0); }],                             // … it loops by itself
  [21.0, () => { pad(15, 1); pad(15, 0); }],                           // finish capture; 32.8 stops the loop
  [29.0, () => { pad(14, 1); pad(14, 0); }],                            // NEXT: another Biome
  [32.8, () => { pad(15, 1); pad(15, 0); }],                            // the loop ends
  [33.0, () => click(728, 26)],                                         // PLACES
  [35.0, () => pad(9, 1, .55)], [38.0, () => { pad(9, 0); pad(10, 1, .5); }], [40.0, () => pad(10, 0)],
  [43.0, () => { open(25); btn(0); enc(0, -70); enc(0, 4); open(26); btn(3); open(0); }],       // LISTEN on, SING: ANSWER
  [43.5, () => click(660, 26)],                                         // GARDEN: the rain of light while singing
  [53.5, () => click(728, 26)],                                         // PLACES: the moon pond and the singing flower
  [62.0, () => click(790, 26)],                                         // PAGES: the whole instrument
  [65.0, () => enc(0, 6)], [66.5, () => btn(9)],                       // (PAGES opened the directory) choose, open ENVELOPE
  [69.0, () => enc(1, 12)], [70.5, () => enc(1, -12)],
  [73.0, () => click(660, 26)],                                         // back to the garden
  [74.0, () => pad(8, 1, .55)], [78.0, () => pad(8, 0)],
];
const run = SHORT ? scriptShort : script;
const seconds = Number(process.env.DEMO_SECONDS || (SHORT ? 60 : 84)),  frames = seconds * fps;
const raw = new Float32Array(seconds * rate * 2);
let w = 0, t = 0, si = 0;
const frameBuf = Buffer.alloc(W * H * 2);
for (let f = 0; f < frames; f++) {
  while (si < run.length && run[si][0] <= t + 1e-9) { run[si][1](); si++; }
  for (let k = 0; k < perFrame; k += 147) {
    const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
    for (let i = 0; i < 147; i++) { const v = voice(t + i / rate); io[i] = io[768 + i] = v; }
    e.vibe_wasm_audio(147, 3);
    const o = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
    for (let i = 0; i < 147 && w < raw.length; i++) for (const c of [256, 512]) raw[w++] = o[c + i];
    t += 147 / rate;
  }
  e.vibe_wasm_draw();
  const st = new Uint8Array(e.memory.buffer, e.vibe_wasm_stage_draw(), W * H * 2);
  frameBuf.set(st);
  fs.writeSync(1, frameBuf);
}
const {pcm, lufs, dbtp} = master(raw, rate, seconds, 3.1);
process.stderr.write(`master: ${lufs.toFixed(2)} LUFS-I, ${dbtp.toFixed(2)} dBTP\n`);
const head = Buffer.alloc(44); head.write("RIFF"); head.writeUInt32LE(36 + pcm.length * 2, 4); head.write("WAVEfmt ", 8); head.writeUInt32LE(16, 16);
head.writeUInt16LE(1, 20); head.writeUInt16LE(2, 22); head.writeUInt32LE(rate, 24); head.writeUInt32LE(rate * 4, 28); head.writeUInt16LE(4, 32); head.writeUInt16LE(16, 34);
head.write("data", 36); head.writeUInt32LE(pcm.length * 2, 40);
fs.writeFileSync(wavPath, Buffer.concat([head, Buffer.from(pcm.buffer)]));
process.stderr.write(`frames ${frames}, audio ${(w / 2 / rate).toFixed(1)} s\n`);
