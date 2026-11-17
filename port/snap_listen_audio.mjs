// 0.24: an audio demo of "the garden listens" – a synthetic voice (harmonics, vibrato, breath) sings phrases into IN 1;
// the garden takes it into its cloud, echo and room and answers each phrase (SING ANSWER), then FOLLOW, then HARMONY.
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";
const [wasmPath, outPath = "listen.wav"] = process.argv.slice(2);
const rate = 44100, BUTTON = 3, ENCODER = 4, MENU = 19, ENTER = 9, F1 = 0;
const e = instantiate(new WebAssembly.Module(fs.readFileSync(wasmPath))).exports; e.vibe_wasm_init(rate);
const press = id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
const open = v => { press(MENU); e.vibe_wasm_event(ENCODER, 0, 0, -60, 0); e.vibe_wasm_event(ENCODER, 0, 0, v, 0); press(ENTER); };
let seed = 7; const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296;
// phrases: [start s, [[midi, length s], ...]]
const phrases = [[2, [[62, .5], [65, .5], [69, .9]]], [7, [[69, .4], [67, .4], [65, .4], [62, 1.0]]], [12.5, [[60, .6], [62, .3], [65, 1.2]]],
                 [19, [[62, .4], [65, .4], [67, .4], [69, .8]]], [24, [[69, .5], [72, .5], [69, .5], [65, 1.0]]],
                 [30, [[62, .6], [64, .6], [65, .6], [69, 1.4]]]];
const notes = []; for (const [s, ns] of phrases) { let t = s; for (const [m, l] of ns) { notes.push([t, l, m]); t += l + .06; } }
let phase = 0, breath = 0;
const voice = t => {
  const n = notes.find(([s, l]) => t >= s && t < s + l);
  if (!n) { phase = 0; return 0; }
  const [s, l, m] = n, within = t - s, f = 440 * 2 ** ((m - 69) / 12) * (1 + .006 * Math.sin(2 * Math.PI * 5.2 * t) * Math.min(1, within / .25));
  phase += f / rate;
  let v = 0; for (let h = 1; h <= 7; h++) v += Math.sin(2 * Math.PI * phase * h) * (h === 1 ? 1 : .55 / h) * (h === 3 || h === 4 ? 1.4 : 1);
  breath += (rnd() * 2 - 1 - breath) * .2;
  const env = Math.min(1, within / .04, (l - within) / .08);
  return (.17 * v + .01 * breath) * env;
};
e.vibe_wasm_event(8, 0, 1, 0, 0);                         // the garden plays
open(25); press(F1);                                       // LISTEN: input on (cloud 55, echo 30, room 45, through 100)
e.vibe_wasm_event(ENCODER, 0, 0, -6, 0);                  // THROUGH 70
open(26); press(F1 + 3);                                   // SING: ANSWER
e.vibe_wasm_event(ENCODER, 3, 0, 8, 0);                   // DUCK 40
const seconds = 40, out = new Int16Array(seconds * rate * 2);
let t = 0, w = 0;
for (let k = 0; k < seconds * rate; k += 128) {
  if (Math.abs(t - 18.5) < 128 / rate) press(F1 + 1);      // FOLLOW from 18.5 s
  if (Math.abs(t - 29.5) < 128 / rate) press(F1 + 2);      // HARMONY from 29.5 s
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
  for (let i = 0; i < 128; i++) { const v = voice(t + i / rate); io[i] = io[768 + i] = v; }
  e.vibe_wasm_audio(128, 3);
  const o = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
  for (let i = 0; i < 128 && w < out.length; i++) { out[w++] = Math.max(-1, Math.min(1, o[256 + i] * 1.6)) * 32767; out[w++] = Math.max(-1, Math.min(1, o[512 + i] * 1.6)) * 32767; }
  t += 128 / rate;
  if (k % 4096 === 0) e.vibe_wasm_draw();
}
const head = Buffer.alloc(44); head.write("RIFF"); head.writeUInt32LE(36 + out.length * 2, 4); head.write("WAVEfmt ", 8); head.writeUInt32LE(16, 16);
head.writeUInt16LE(1, 20); head.writeUInt16LE(2, 22); head.writeUInt32LE(rate, 24); head.writeUInt32LE(rate * 4, 28); head.writeUInt16LE(4, 32); head.writeUInt16LE(16, 34);
head.write("data", 36); head.writeUInt32LE(out.length * 2, 40);
fs.writeFileSync(outPath, Buffer.concat([head, Buffer.from(out.buffer)]));
let peak = 0; for (const s of out) peak = Math.max(peak, Math.abs(s)); console.log(JSON.stringify({outPath, seconds, peak: peak / 32767}));
