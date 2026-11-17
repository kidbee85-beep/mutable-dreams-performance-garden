// 0.26: FIRST STEPS – the interactive tour walks a newcomer through PLAY, a pad, a macro, NEXT, the gesture, the pages
// and the inputs, each step moving on when it was done; HOME ends it; it is remembered; SYSTEM F1, the stage's ? and
// the console's ? start it again. Also MOVES (the input's loudness lifts a macro) and KEY (a sung note becomes the key).
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, TRANSPORT = 8, MENU = 19, ENTER = 9, F1 = 0, SAVE = 18, PAD = 1, HOME = 17, STAGE = 19;
const LISTEN = 25, SING = 26, SYSTEM = 45, rate = 48000;

function session(files = new Map()) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  const cstr = p => { const b = new Uint8Array(e.memory.buffer, p); let n = 0; while (b[n]) n++; return Buffer.from(b.subarray(0, n)).toString(); };
  const ids = []; for (let i = 0; i < e.vibe_wasm_param_count(); i++) ids.push(cstr(new Uint32Array(e.memory.buffer, e.vibe_wasm_param_info(i), 1)[0]));
  let t = 0;
  const s = {
    e,
    press: id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); },
    pad: (id, down) => e.vibe_wasm_event(PAD, id, down ? 1 : 0, down ? .9 : 0, 0),
    turn: (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0),
    open: v => { s.press(MENU); s.turn(0, -60); s.turn(0, v); s.press(ENTER); },
    param: id => e.vibe_wasm_param_value(ids.indexOf(id)),
    run: (sec, source = null) => {
      for (let k = 0; k < rate * sec; k += 128) {
        if (source) { const b = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024); for (let i = 0; i < 128; i++) { const v = source(t + i / rate); b[i] = b[768 + i] = v; } }
        e.vibe_wasm_audio(128, source ? 3 : 0); t += 128 / rate;
        if (k % 2048 === 0) e.vibe_wasm_draw();
      }
    },
    // a fresh status packet (the outbox keeps the oldest untaken one: take it, then wait for the next)
    tour: () => {
      let got = null;
      for (let pass = 0; pass < 2; pass++) for (let i = 0; i < 60; i++) {
        s.run(.01); e.vibe_wasm_draw();
        const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), n = dv.getUint32(box, true);
        if (!n) continue;
        const b = new Uint8Array(e.memory.buffer, box + 4, n).slice(); dv.setUint32(box, 0, true);
        if (b[0] !== 100) continue;
        let k = 32; for (let j = 0; j < 10; j++) k += 1 + b[k];
        got = {step: (b[k + 11] ?? 0) - 1, help: !!(b[k + 1] & 2)}; break;
      }
      return got;
    },
  };
  return s;
}
const R = {steps: []};

// 1. A first start: the tour runs and moves on with every step done.
const files = new Map();
const s = session(files);
let st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 0, "the very first start opens FIRST STEPS at PLAY");
s.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); s.run(.2);                          // PLAY
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 1, "PLAY done -> hold BLOOM");
s.pad(8, 1); s.run(1); s.pad(8, 0); s.run(.2);                                   // BLOOM held
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 2, "BLOOM done -> turn a macro");
s.turn(0, 10); s.run(.3);                                                        // encoder 1 on LIVE
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 3, "macro done -> NEXT");
s.pad(14, 1); s.pad(14, 0); s.run(.5);                                           // NEXT
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 4, "NEXT done -> the gesture");
s.pad(15, 1); s.pad(15, 0); s.run(.2); s.pad(4, 1); s.pad(8, 1); s.run(2); s.pad(4, 0); s.pad(8, 0);
for (let i = 0; i < 40 && s.tour().step === 4; i++) s.run(.5);                   // the capture ends after its bars
if (s.tour().step === 4) { s.pad(15, 1); s.pad(15, 0); s.run(.5); }               // (tap 16 again: it loops)
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 5, "the gesture loops -> the pages");
s.press(MENU); s.run(.2);
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 6, "MENU opened -> the inputs");
s.press(MENU); s.open(LISTEN); s.press(F1); s.run(.3);                           // LISTEN: input on
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, 7, "input on -> the last card");
s.run(9);
st = s.tour(); R.steps.push(st.step); assert.equal(st.step, -1, "the last card closes by itself");
s.e.vibe_wasm_event(SAVE, 0, 0, 0, 0);

// 2. Remembered: the next start has no tour. SYSTEM F1 starts it again (on LIVE), HOME ends it and opens the help.
{ const b = session(files); let x = b.tour(); R.remembered = x.step; assert.equal(x.step, -1, "a known garden starts without the tour");
  b.open(SYSTEM); b.press(F1); x = b.tour(); assert.equal(x.step, 0, "SYSTEM F1 starts FIRST STEPS");
  b.press(HOME); x = b.tour(); assert.equal(x.step, -1, "HOME ends the tour"); assert(x.help, "... and opens the help as always");
  const at = (px, py) => Math.ceil(py * 4 / 3) * 4096 + Math.ceil(px * 4 / 3);
  b.press(HOME); b.e.vibe_wasm_event(STAGE, 0, 1, at(606, 26), 0); b.e.vibe_wasm_event(STAGE, 2, 0, at(606, 26), 0);
  x = b.tour(); assert.equal(x.step, 0, "the stage's ? starts it");
  b.e.vibe_wasm_event(STAGE, 0, 1, at(606, 26), 0); b.e.vibe_wasm_event(STAGE, 2, 0, at(606, 26), 0);
  x = b.tour(); assert.equal(x.step, -1, "... and ends it");
  new Uint8Array(b.e.memory.buffer, b.e.vibe_wasm_editor_inbox(), 1).set([28]); b.e.vibe_wasm_editor_data_in(1);
  b.run(.1); x = b.tour(); assert.equal(x.step, 0, "the console's ? starts it"); R.restart = true; }

// 3. A garden saved by 0.24 / 0.25 (version 10) is a known garden: no tour; its LISTEN settings still load.
{ const f = new Map(); const a = session(f); a.open(LISTEN); a.press(F1); a.e.vibe_wasm_event(SAVE, 0, 0, 0, 0);
  const g = f.get("garden.bin"); const dv = new DataView(g.buffer, g.byteOffset); dv.setUint32(4, 10, true);
  const old = new Uint8Array(g.length - 12);                                     // a v10 file: 8 bytes MOVES + 4 bytes tour less
  const head10 = 552 + 68; old.set(g.subarray(0, head10)); old.set(g.subarray(head10 + 12), head10);
  f.set("garden.bin", old);
  const b = session(f); const x = b.tour(); R.v10 = x.step; assert.equal(x.step, -1, "a v10 garden: no tour"); }

// 4. MOVES: the input's loudness lifts DENSITY above its resting value; off: it returns.
{ const m = session(); m.press(HOME); m.press(HOME);
  const rest = m.param("macro_density");
  m.open(LISTEN); m.press(F1); m.press(F1 + 4);                                  // input on, MOVES: DENSITY
  m.run(.5, () => 0); const quiet = m.param("macro_density");
  m.run(1, t => .5 * Math.sin(2 * Math.PI * 220 * t)); const loud = m.param("macro_density");
  for (let i = 0; i < 4; i++) m.press(F1 + 4);                                   // through MOTION, COLOUR, SPACE to OFF
  m.run(.5, () => 0); const back = m.param("macro_density");
  R.moves = {rest, quiet, loud, back};
  assert(loud > quiet + 15, `a loud input lifts DENSITY (${quiet} -> ${loud})`);
  assert(Math.abs(back - rest) < 2, `MOVES off: DENSITY back at rest (${back} vs ${rest})`); }

// 5. KEY: SING F5, sing D: the Biome's root becomes D.
{ const k = session(); k.press(HOME); k.press(HOME);
  k.open(SING); k.press(F1 + 4);
  const d = 440 * 2 ** ((62 - 69) / 12);
  k.run(1.2, t => { let v = 0; for (let h = 1; h <= 4; h++) v += Math.sin(2 * Math.PI * d * h * t) / h; return .3 * v; });
  k.run(.3);
  R.key = k.param("root"); assert.equal(R.key, 2, "the sung D became the key"); }

console.log(JSON.stringify(R));
console.log("PASSED: FIRST STEPS (8 cards, each done by playing; remembered; SYSTEM F1, stage ?, console ? start it; HOME ends it), MOVES, KEY.");
