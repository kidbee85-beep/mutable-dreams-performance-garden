// 0.13 big grids: on an 8 x 8 device the top-left 4 x 4 are the performance pads, the bottom four rows the
// selected track's 32 steps, the top-right block tracks / play / moments / variation / REC. 4 x 4 unchanged.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
function session(device) {
  const e = instantiate(module, new Map(), null, null, device).exports;
  e.vibe_wasm_init(rate);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const s = { e, out: [], tap: id => { e.vibe_wasm_event(1, id, 1, .9, 0); e.vibe_wasm_event(1, id, 0, 0, 0); },
    run: sec => { for (let n = 0; n < rate * sec; n += 128) { e.vibe_wasm_audio(128, 0); for (let i = 0; i < 128; i++) s.out.push(io[256 + i]); } },
    hash: () => { const f = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320); let h = 2166136261; for (const p of f) h = Math.imul(h ^ p, 16777619); return h >>> 0; } };
  return s;
}
const diff = (a, b, f, t) => { let d = 0; for (let i = f * rate; i < t * rate; i++) d += Math.abs(a[i] - b[i]); return d; };
const grid = {pads: 64, cols: 8, rows: 8};

// 1. Grid pad (row 0, col 0..3) = performance pads 0..3; grid id 8 (row 1, col 0) = pad 4 FOREST.
const dry = session(grid); dry.e.vibe_wasm_event(8, 0, 1, 0, 0); dry.run(3);
const forest = session(grid); forest.e.vibe_wasm_event(8, 0, 1, 0, 0); forest.e.vibe_wasm_event(1, 8, 1, 1, 0); forest.run(3);
assert(diff(forest.out, dry.out, .5, 3) > .5, "grid id 8 plays FOREST");
// grid id 4 (row 0, col 4) is T1 select – not a sound pad
const notpad = session(grid); notpad.e.vibe_wasm_event(8, 0, 1, 0, 0); notpad.e.vibe_wasm_event(1, 4, 1, 1, 0); notpad.run(3);
assert(diff(notpad.out, dry.out, 0, 3) < 1e-6, "grid id 4 is a function, not a tone");

// 2. Steps: tapping grid row 4 (ids 32..39) writes notes into T1's sequence and they sound.
const steps = session(grid); steps.e.vibe_wasm_event(8, 0, 1, 0, 0); steps.run(.2);
for (const id of [32, 34, 36, 38, 40, 42]) steps.tap(id);
steps.run(5);
const stepsDiff = diff(steps.out, dry.out, 1, 3);
assert(stepsDiff > .5, "tapped steps play");

// 3. Function block: T2 (id 5) then a step goes to track 2; PLAY / STOP (ids 14/15); REC (id 31) starts a take.
const fn = session(grid); fn.run(.1);
fn.tap(14); fn.run(1);
let peak = 0; for (let i = .5 * rate; i < rate; i++) peak = Math.max(peak, Math.abs(fn.out[i]));
assert(peak > .001, "grid PLAY starts the dream");
const before = fn.hash(); fn.tap(31); fn.run(.3);
assert.notEqual(fn.hash(), before, "grid REC shows a take");
fn.tap(31); fn.tap(15); fn.run(.2);

// 4. The VIBE's own 4 x 4 is unchanged: pad 4 is FOREST.
const base = session(null); base.e.vibe_wasm_event(8, 0, 1, 0, 0); base.e.vibe_wasm_event(1, 4, 1, 1, 0); base.run(3);
const baseDry = session(null); baseDry.e.vibe_wasm_event(8, 0, 1, 0, 0); baseDry.run(3);
assert(diff(base.out, baseDry.out, .5, 3) > .5, "4 x 4: pad 4 is FOREST");
console.log(JSON.stringify({stepsDiff: +stepsDiff.toFixed(2), playPeak: +peak.toFixed(4)}));
console.log("PASSED: 8 x 8 grid – performance block, 32 steps, function block; 4 x 4 unchanged.");
