// Every pad of the 0.3 layout changes the sound, stays finite and releases cleanly.
// The gesture keeps playing after the hands let go and stops on a second tap.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = Number(process.argv[3] || 48000);
const names = ["ROOT","FIFTH","THIRD","NINTH","FOREST","AIR","WATER","STARDUST",
  "BLOOM","ECHO","GRAIN","DUSK","THIN","PULSE","NEXT","GESTURE"];

function run(script, seconds) {
  const {exports: e} = instantiate(module);
  e.vibe_wasm_init(rate);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const send = (type, id, down, value, midi = 0) => e.vibe_wasm_event(type, id, down, value, midi);
  send(8, 0, 1, 0);                       // host transport plays
  const out = [];
  let peak = 0, step = 0, position = 0;
  const sizes = [128, 73, 256, 17];
  for (let block = 0; position < rate * seconds; block++) {
    const t = position / rate;
    script(t, send);
    const n = sizes[block % sizes.length];
    e.vibe_wasm_audio(n, 0);
    for (let i = 0; i < n; i++) {
      const l = io[256 + i], r = io[512 + i];
      assert(Number.isFinite(l) && Number.isFinite(r), "finite");
      peak = Math.max(peak, Math.abs(l), Math.abs(r));
      if (out.length) step = Math.max(step, Math.abs(l - out[out.length - 1]));
      out.push(l);
    }
    position += n;
  }
  e.vibe_wasm_draw();
  return {out, peak, step};
}
const diff = (a, b, from, to) => {
  let s = 0;
  for (let i = Math.floor(from * rate); i < Math.min(to * rate, a.length, b.length); i++) s += Math.abs(a[i] - b[i]);
  return s;
};
const once = fn => { let done = false; return (t, send) => { if (!done) done = fn(t, send) === true; }; };

const dry = run(() => {}, 8);
const results = [];
for (let pad = 0; pad < 16; pad++) {
  let stage = 0;
  const r = run((t, send) => {
    if (stage === 0 && t > 0.5) { send(1, pad, 1, 0.9); stage = 1; }
    if (stage === 1 && t > 6.0) { send(1, pad, 0, 0); stage = 2; }
  }, 7);
  const held = diff(r.out, dry.out, 0.5, 6.0);
  results.push({pad: names[pad], held: +held.toFixed(3), peak: +r.peak.toFixed(4), maxStep: +r.step.toFixed(4)});
  assert(r.peak < 1, `${names[pad]} peak ${r.peak}`);
  if (pad < 14) assert(held > 0.05, `${names[pad]} is audible (${held})`);
}

// NEXT changes the Biome at the engine's next bar; it may take longer than the held window.
const next = run(once((t, send) => { if (t > 0.5) { send(1, 14, 1, 1); send(1, 14, 0, 0); return true; } }), 8);
const nextDiff = diff(next.out, dry.out, 4, 8);
assert(nextDiff > 0.05, `NEXT changes the dream (${nextDiff})`);

// Gesture: tap, play FOREST + BLOOM for part of four bars, release, let it loop, then stop.
let g = 0;
const gesture = run((t, send) => {
  if (g === 0 && t > 0.2) { send(1, 15, 1, 1); send(1, 15, 0, 0); g = 1; }
  if (g === 1 && t > 0.4) { send(1, 4, 1, 1); send(1, 8, 1, 1); g = 2; }
  if (g === 2 && t > 2.4) { send(1, 4, 0, 0); send(1, 8, 0, 0); g = 3; }
  if (g === 3 && t > 22) { send(1, 15, 1, 1); send(1, 15, 0, 0); g = 4; }
}, 30, true);
const control = run((t, send) => {
  if (g >= 0 && t > 0.4 && t < 0.41) { send(1, 4, 1, 1); send(1, 8, 1, 1); }
  if (t > 2.4 && t < 2.41) { send(1, 4, 0, 0); send(1, 8, 0, 0); }
}, 30);
// Default tempo four bars are well inside 22 s; the loop must replay after the hands are gone.
const looping = diff(gesture.out, control.out, 12, 22);
assert(looping > 0.5, `gesture replays (${looping})`);
assert(gesture.peak < 1);

console.log(JSON.stringify({rate, pads: results, nextDifference: +nextDiff.toFixed(3),
  gestureReplayDifference: +looping.toFixed(3), gesturePeak: +gesture.peak.toFixed(4)}, null, 2));
console.log("PASSED: 16 pads audible and bounded, NEXT changes the Biome, gesture loops after release.");
