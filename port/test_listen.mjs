// 0.24 Garden Listens: IN 1 / IN 2 into the garden's effects, SING (the garden answers a voice), CV / gate, saved state.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, TRANSPORT = 8, MENU = 19, ENTER = 9, F1 = 0, SAVE = 18;
const LISTEN = 25, SING = 26, CV = 49;

function session(rate = 48000, files = new Map()) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  const io = () => new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 4 * 256);
  const rear = () => new Float32Array(e.memory.buffer, e.vibe_wasm_rear(), 6);
  let t = 0;
  const s = {
    e, rate, out: [],
    press: (id, n = 1) => { for (let i = 0; i < n; i++) { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); } },
    turn: (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0),
    open: v => { s.press(MENU); s.turn(0, -60); s.turn(0, v); s.press(ENTER); },
    rear,
    // source(t) -> [l, r] or null (no input this block); returns rms of the output
    run: (sec, source = null, each = null) => {
      let sum = 0, n = 0;
      for (let k = 0; k < s.rate * sec; k += 128) {
        let has = 0;
        if (source) {
          const b = io();
          for (let i = 0; i < 128; i++) { const v = source(t + i / s.rate); b[i] = v[0]; b[768 + i] = v[1]; }
          has = 3;
        }
        e.vibe_wasm_audio(128, has);
        const b = io();
        for (let i = 0; i < 128; i++) { const x = b[256 + i], y = b[512 + i]; assert(Number.isFinite(x) && Number.isFinite(y), "finite"); sum += x * x; n++; s.out.push(x); }
        t += 128 / s.rate;
        if (each) each(t);
        if (k % 4096 === 0) e.vibe_wasm_draw();
      }
      return Math.sqrt(sum / n);
    },
  };
  return s;
}
const sine = (f, a = .3) => t => { const v = a * Math.sin(2 * Math.PI * f * t); return [v, v]; };
const R = {};

// 1. Input off: the garden stopped is silent even with a signal at IN 1.
{ const s = session(); s.run(.3); R.offRms = +s.run(1, sine(220)).toFixed(5); assert(R.offRms < 1e-4, `input off stays silent (${R.offRms})`); }

// 2. LISTEN F1: input on. THROUGH 0: what sounds is the cloud, the echo and the room alone. The dry through adds on top.
{ const s = session(); s.run(.2); s.open(LISTEN); s.press(F1); s.turn(0, -40);    // THROUGH 0
  s.run(2.5, sine(220));
  const wet = s.run(1.5, sine(220));
  const tail = s.run(1.5, () => [0, 0]);                                        // the echo and the room ring on after it
  s.turn(0, 40);                                                                 // THROUGH 100
  const through = s.run(1, sine(220));
  R.wetRms = +wet.toFixed(4); R.tailRms = +tail.toFixed(5); R.throughRms = +through.toFixed(4);
  assert(wet > .01, `cloud, echo and room sound with the garden stopped (${wet})`);
  assert(tail > .0005, `effects ring on after the input stops (${tail})`);
  assert(through > wet, "THROUGH adds the dry input");
  for (const k of [1, 2, 3]) s.turn(k, -40);                                     // all sends 0
  s.turn(0, -40); s.run(2.5, () => [0, 0]);
  const none = s.run(1, sine(220));
  R.noSendRms = +none.toFixed(5);
  assert(none < wet * .2, `with every send and THROUGH at 0 the input is gone (${none})`);
}

// 3. DUCK: a loud input makes room in the playing garden.
{ const play = (duck) => { const s = session(); s.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); s.open(LISTEN); s.press(F1);
    for (const k of [0, 1, 2, 3]) s.turn(k, -40);                                 // no through, no sends: only the garden
    s.open(SING); s.turn(3, duck ? 40 : -40); s.run(3);
    return s.run(4, sine(330, .5)); };
  const free = play(false), ducked = play(true);
  R.duck = {free: +free.toFixed(4), ducked: +ducked.toFixed(4)};
  assert(ducked < free * .6, `the garden ducks under the input (${ducked} vs ${free})`);
}

// 4. A broken input (NaN, inf, huge) never reaches the output.
{ const s = session(); s.open(LISTEN); s.press(F1);
  let k = 0; s.run(1, () => { k++; return k % 3 === 0 ? [NaN, Infinity] : k % 3 === 1 ? [1e9, -1e9] : [.2, .2]; });
  s.run(.5, sine(220)); R.nanSafe = true; }

// 5. SING FOLLOW: a sung melody (C4 E4 G4 C5) plays notes on the track, with no other sound path open.
const melody = (notes, len = .32, gap = .08) => t => {
  const slot = Math.floor(t / (len + gap)), within = t - slot * (len + gap);
  if (slot >= notes.length || within > len) return [0, 0];
  const f = 440 * 2 ** ((notes[slot] - 69) / 12);
  let v = 0; for (let h = 1; h <= 5; h++) v += Math.sin(2 * Math.PI * f * h * t) / h;
  const env = Math.min(1, within / .01, (len - within) / .02);
  return [.25 * v * env, .25 * v * env];
};
const singRun = (mode, extra = 0) => { const s = session(); s.open(LISTEN); s.press(F1);
  for (const k of [0, 1, 2, 3]) s.turn(k, -40);
  s.open(SING); s.press(F1 + mode); s.run(.3);
  const during = s.run(1.8, melody([60, 64, 67, 72]));
  const after = s.run(2.5 + extra, () => [0, 0]);
  return {during, after}; };
{ const off = singRun(0), follow = singRun(1);
  R.sing = {off: +off.during.toFixed(5), follow: +follow.during.toFixed(4)};
  assert(off.during < 1e-4, "SING OFF: nothing plays");
  assert(follow.during > .003, `SING FOLLOW: the garden plays what is sung (${follow.during})`);
  const harmony = singRun(2); R.sing.harmony = +harmony.during.toFixed(4);
  assert(harmony.during > .003, "SING HARMONY sounds");
  // ANSWER: silent while singing, then replies in the silence after the phrase
  const answer = singRun(3); R.sing.answer = {during: +answer.during.toFixed(5), after: +answer.after.toFixed(4)};
  assert(answer.after > .002, `SING ANSWER replies after the phrase (${answer.after})`);
  assert(answer.after > answer.during, "the answer comes after the call");
}

// 6. CV / GATE: CV OUT follows track 1's notes, the gate opens; CV IN moves a macro (the LIVE page changes).
{ const s = session(); s.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); s.open(CV); s.turn(0, 1);   // CV TRACK: T1
  let gateSeen = 0, cvMax = 0;
  s.run(6, null, () => { const r = s.rear(); if (r[1] > 4) gateSeen++; cvMax = Math.max(cvMax, r[0]); });
  R.cv = {gateBlocks: gateSeen, cvMax: +cvMax.toFixed(3)};
  assert(gateSeen > 0, "GATE OUT opens on notes");
  assert(cvMax > 0, "CV OUT carries the note");
  s.turn(3, 1);                                                                  // CV IN: DENSITY
  const hashLive = () => { const f = new Uint16Array(s.e.memory.buffer, s.e.vibe_wasm_draw(), 480 * 320); let h = 0; for (let i = 0; i < f.length; i += 3) h = (h * 31 + f[i]) | 0; return h; };
  s.rear()[2] = 0; s.run(.3); s.open(0); s.run(.2); const low = hashLive();
  s.rear()[2] = 5; s.run(.3); for (let i = 0; i < 6; i++) { s.e.vibe_wasm_draw(); s.run(.05); } const high = hashLive();
  R.cv.inMoves = low !== high;
  assert(low !== high, "CV IN moves the DENSITY macro");
}

// 7. Saved: input on, MIC, STEREO, SING ANSWER on track 3 come back after a restart (garden.bin 10).
{ const files = new Map(); const a = session(48000, files);
  a.open(LISTEN); a.press(F1); a.press(F1 + 1); a.press(F1 + 2);                 // on, MIC, STEREO
  a.open(SING); a.press(F1 + 3); a.turn(0, 2);
  a.e.vibe_wasm_event(SAVE, 0, 0, 0, 0);
  const g = files.get("garden.bin"); assert(g && new DataView(g.buffer, g.byteOffset).getUint32(4, true) === 11, "garden.bin version 11");
  const b = session(48000, files); b.run(.1);
  const r = b.rear();
  R.saved = {mode: r[3], link: r[5]};
  assert.equal(r[3], 1, "MIC comes back"); assert.equal(r[5], 1, "STEREO comes back");
}

// 8. Every rate: the input is heard and stays finite.
R.rates = {};
for (const rate of [44100, 96000, 192000]) {
  const s = session(rate); s.open(LISTEN); s.press(F1); s.turn(0, -40);
  s.run(2, sine(220)); const w = s.run(1, sine(220));
  R.rates[rate] = +w.toFixed(4);
  assert(w > .01, `${rate} Hz: the cloud hears the input (${w})`);
}

console.log(JSON.stringify(R, null, 1));
console.log("PASSED: LISTEN (cloud, echo, room, through, duck, broken input), SING (follow, harmony, answer), CV / gate, garden.bin 11, 44.1-192 kHz.");
