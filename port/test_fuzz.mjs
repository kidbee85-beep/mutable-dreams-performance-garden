// 0.31: a fuzzer – thousands of random events (pads, keys, buttons, encoders, MIDI, transport, params, stage clicks,
// Editor commands, saves and restores), odd block sizes, every sample rate, wild input (spikes, NaN, Inf, denormals).
// The firmware must never trap, its outputs must stay finite and bounded, and a saved garden must load again.
// usage: node test_fuzz.mjs <wasm> [steps per rate = 6000] [seed = 1]
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const STEPS = +(process.argv[3] || 6000), SEED = +(process.argv[4] || 1);
let seed = SEED >>> 0;
const rnd = () => (seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296;
const ri = (n) => Math.floor(rnd() * n);
const pick = (a) => a[ri(a.length)];
const R = {seed: SEED, steps: STEPS, rates: {}};

function session(rate, files) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  return e;
}

for (const rate of [44100, 48000, 96000, 192000]) {
  const files = new Map();
  let e = session(rate, files);
  const nParams = e.vibe_wasm_param_count();
  const log = [];
  let peak = 0, blocks = 0, restores = 0, inputMode = 0;
  const ev = (...a) => { log.push(a.join(",")); if (log.length > 60) log.shift(); e.vibe_wasm_event(...a); };
  try {
    for (let step = 0; step < STEPS; step++) {
      const r = rnd();
      if (r < .16) ev(1, ri(16), rnd() < .6 ? 1 : 0, rnd(), 0);                       // pads
      else if (r < .34) { const id = ri(24); ev(3, id, 1, 0, 0); if (rnd() < .9) ev(3, id, 0, 0, 0); }   // buttons (some held)
      else if (r < .48) ev(4, ri(8), 0, (rnd() - .5) * pick([2, 20, 200]), 0);          // encoders, incl. huge turns
      else if (r < .54) ev(2, ri(25), rnd() < .5 ? 1 : 0, rnd(), 0);                   // keys
      else if (r < .62) {                                                              // MIDI: any status, any data
        const st = pick([0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf8, 0xfa, 0xfb, 0xfc, 0xf2, 0xfe, 0xff]) | (rnd() < .8 ? ri(16) : 0);
        ev(5, 0, 0, 0, st | (ri(128) << 8) | (ri(128) << 16));
      }
      else if (r < .65) ev(8, 0, rnd() < .5 ? 1 : 0, 0, 0);                            // transport
      else if (r < .70) e.vibe_wasm_param_set(ri(nParams), pick([rnd(), -1, 2, 1e9, -1e9, NaN, 0, 1]));
      else if (r < .76) { const v = ri(960) + ri(720) * 4096; ev(19, 0, 1, v, 0); if (rnd() < .5) ev(19, 1, 1, v + ri(40), 0); ev(19, 2, 0, v, 0); }
      else if (r < .80) {                                                              // Editor commands: random bytes
        const n = 1 + ri(12), box = e.vibe_wasm_editor_inbox();
        const b = new Uint8Array(e.memory.buffer, box, n); for (let i = 0; i < n; i++) b[i] = rnd() < .5 ? ri(40) : ri(256);
        log.push("editor," + Array.from(b).join(" ")); e.vibe_wasm_editor_data_in(n);
      }
      else if (r < .81) ev(6, 0, rnd() < .5 ? 1 : 0, rnd(), 0);                        // pedal
      else if (r < .82) ev(7, 0, 0, rnd() * .6, 0);                                    // clock pulse
      else if (r < .83) ev(10, ri(nParams), 0, rnd(), 0);                              // param from outside
      else if (r < .835) {                                                             // save, then load into a fresh session
        ev(18, 0, 0, 0, 0);
        e = session(rate, files); restores++;
      }
      else if (r < .84) inputMode = ri(4);
      else if (r < .845) {                                                             // LISTEN / SING on, so the input is heard
        const press = id => { ev(3, id, 1, 0, 0); ev(3, id, 0, 0, 0); };
        press(19); ev(4, 0, 0, -60, 0); ev(4, 0, 0, pick([25, 26, 49]), 0); press(9); press(ri(6)); inputMode = 1 + ri(3);
      }                                             // what comes into IN 1 / IN 2
      // audio: an odd block, the input as chosen
      const n = 1 + ri(256);
      const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
      for (let i = 0; i < n; i++) {
        let v = 0;
        if (inputMode === 1) v = Math.sin(step * .37 + i * .05) * .5;
        else if (inputMode === 2) v = rnd() < .002 ? pick([NaN, Infinity, -Infinity, 1e30, 1e-42]) : (rnd() * 2 - 1);
        else if (inputMode === 3) v = rnd() < .01 ? pick([8, -8]) : 0;
        io[i] = v; io[768 + i] = -v;
      }
      e.vibe_wasm_audio(n, inputMode ? 3 : 0); blocks++;
      const o = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
      for (let i = 0; i < n; i++) for (const c of [256, 512]) {
        const x = o[c + i];
        if (!Number.isFinite(x)) throw Error(`output not finite (${x}) at step ${step}, sample ${i}, channel ${c}`);
        const a = Math.abs(x); if (a > peak) peak = a;
      }
      if (step % 37 === 0) e.vibe_wasm_draw();
      if (step % 211 === 0) e.vibe_wasm_stage_draw();
    }
  } catch (err) {
    console.error(`rate ${rate}, seed ${SEED}: ${err.message}\nlast events:\n${log.join("\n")}`);
    throw err;
  }
  R.rates[rate] = {blocks, restores, peak: +peak.toFixed(3)};
  assert(peak <= 1, `rate ${rate}: output peak ${peak} beyond full scale`);   // (0.31, Codex review 11)
}
console.log(JSON.stringify(R));
console.log(`PASSED: fuzz – ${STEPS} random steps at each of 4 rates; no trap, finite and bounded outputs, saves load again.`);
