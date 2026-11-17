// 0.25: the findings of Codex reviews 02-05 – headroom of the dry input, a long soft-key press explains instead of
// acting, SING ANSWER timing independent of the block size, the input resampler suppresses ultrasound, a damaged
// garden.bin (NaN in a saved float) never reaches the audio.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, TRANSPORT = 8, MENU = 19, ENTER = 9, F1 = 0, SAVE = 18, PAD = 1;
const LISTEN = 25, SING = 26;

function session({rate = 48000, files = new Map(), blocks = [128]} = {}) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  const io = () => new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 4 * 256);
  let t = 0, bi = 0;
  const s = {
    e, rate, t: () => t,
    press: (id) => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); },
    turn: (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0),
    open: v => { s.press(MENU); s.turn(0, -60); s.turn(0, v); s.press(ENTER); },
    // returns {peak, rms, out[], maxStep}
    run: (sec, source = null) => {
      let peak = 0, sum = 0, n = 0, maxStep = 0, last = null; const out = [];
      const end = t + sec;
      while (t < end - 1e-9) {
        const f = blocks[bi++ % blocks.length];
        let has = 0;
        if (source) { const b = io(); for (let i = 0; i < f; i++) { const v = source(t + i / s.rate); b[i] = v[0]; b[768 + i] = v[1]; } has = 3; }
        e.vibe_wasm_audio(f, has);
        const b = io();
        for (let i = 0; i < f; i++) {
          const x = b[256 + i], y = b[512 + i];
          assert(Number.isFinite(x) && Number.isFinite(y), "finite output");
          peak = Math.max(peak, Math.abs(x), Math.abs(y)); sum += x * x; n++; out.push(x);
          if (last !== null) maxStep = Math.max(maxStep, Math.abs(x - last)); last = x;
        }
        t += f / s.rate;
        if (Math.floor(t * 30) !== Math.floor((t - f / s.rate) * 30)) e.vibe_wasm_draw();
      }
      return {peak, rms: Math.sqrt(sum / Math.max(1, n)), out, maxStep};
    },
  };
  return s;
}
const sine = (f, a) => t => { const v = a * Math.sin(2 * Math.PI * f * t); return [v, v]; };
const R = {};

// 1. Headroom: a hot input (0.98) through 100 % over a loud playing garden never leaves [-1, 1], at every rate and
//    with ragged blocks; switching the input on and off makes no jump.
R.headroom = {};
for (const rate of [44100, 48000, 96000, 192000]) {
  const s = session({rate, blocks: [1, 17, 73, 128, 256]});
  s.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
  for (const p of [0, 4, 5, 8, 9]) s.e.vibe_wasm_event(PAD, p, 1, 1, 0);       // a loud garden
  s.run(2);
  s.open(LISTEN); s.press(F1);
  const on = s.run(3, sine(220, .98));
  s.press(F1);                                                                  // off: THROUGH fades out
  const off = s.run(1, sine(220, .98));
  R.headroom[rate] = {peakOn: +on.peak.toFixed(4), stepOn: +on.maxStep.toFixed(4), stepOff: +off.maxStep.toFixed(4)};
  assert(on.peak <= 1.0, `${rate}: the output stays inside [-1, 1] (${on.peak})`);
  assert(off.peak <= 1.0, `${rate}: also while THROUGH fades out`);
  assert(on.maxStep < .5 && off.maxStep < .5, `${rate}: no jump when the input goes on / off`);
}

// 2. A long press on F1 of LISTEN explains the key and does NOT switch the input on; a short press does.
{ const s = session();
  s.open(LISTEN);
  s.e.vibe_wasm_event(BUTTON, F1, 1, 0, 0); s.run(.9); s.e.vibe_wasm_event(BUTTON, F1, 0, 0, 0);
  s.turn(0, -40);                                                               // THROUGH 0: only the sends speak
  s.run(1.5, sine(220, .4)); const afterLong = s.run(1, sine(220, .4)).rms;
  s.press(F1);                                                                  // (0.26) the long press opened the help: this closes it
  s.press(F1); s.run(2, sine(220, .4)); const afterShort = s.run(1, sine(220, .4)).rms;
  R.longPress = {afterLong: +afterLong.toFixed(5), afterShort: +afterShort.toFixed(4)};
  assert(afterLong < 1e-4, `a long press does not switch the input on (${afterLong})`);
  assert(afterShort > .01, "a short press does");
}

// 3. SING ANSWER: the same short phrase is answered with the same rhythm at block sizes 17, 73, 128 and 256.
const phrase = t => {                                                           // A4 0.20-0.29 s, B4 0.32-0.41 s
  const n = t >= .2 && t < .29 ? 69 : t >= .32 && t < .41 ? 71 : -1;
  if (n < 0) return [0, 0];
  const f = 440 * 2 ** ((n - 69) / 12); let v = 0; for (let h = 1; h <= 5; h++) v += Math.sin(2 * Math.PI * f * h * t) / h;
  return [.3 * v, .3 * v];
};
const midiOut = (s) => {                                                         // note-ons the dream sends (MIDI page F1: notes out)
  const m = new DataView(s.e.memory.buffer, s.e.vibe_wasm_midi_out());
  const w = m.getUint32(0, true), r = m.getUint32(4, true), msgs = [];
  for (let i = r; i < w; i++) { const o = 16 + (i % 256) * 6; msgs.push([m.getUint8(o + 2), m.getUint8(o + 3), m.getUint8(o + 4)]); }
  m.setUint32(4, w, true); return msgs; };
R.answer = {};
for (const f of [17, 73, 128, 256]) {
  const s = session({blocks: [f]});
  s.open(46); s.press(F1);                                                      // MIDI: notes out
  s.open(LISTEN); s.press(F1); for (const k of [0, 1, 2, 3]) s.turn(k, -40);
  s.open(SING); s.press(F1 + 3);
  s.run(.1); midiOut(s);
  const o = [];
  for (let k = 0; k < 250; k++) { s.run(.02, t => phrase(t - .1)); for (const [st, , v] of midiOut(s)) if ((st & 0xf0) === 0x90 && v > 0) o.push(s.t()); }
  R.answer[f] = o.map(x => +x.toFixed(3));
  assert(o.length >= 2, `block ${f}: the garden answers the phrase with two notes (${o})`);
}
{ const sp = Object.values(R.answer).map(o => o[1] - o[0]);
  const spread = Math.max(...sp) - Math.min(...sp);
  R.answerSpread = +spread.toFixed(3);
  assert(spread < .06, `the answer's rhythm does not depend on the block size (spread ${spread.toFixed(3)} s)`); }

// 4. The input resampler: at 96 and 192 kHz a 30 kHz tone (above the engine's band) is suppressed in the effects,
//    a 1 kHz tone of the same level passes.
R.alias = {};
for (const rate of [96000, 192000]) {
  const wet = (freq) => { const s = session({rate, blocks: [1, 128, 256, 73]}); s.open(LISTEN); s.press(F1); s.turn(0, -40);
    s.run(2, sine(freq, .5)); return s.run(2, sine(freq, .5)).rms; };
  const pass = wet(1000), ultra = wet(30000);
  const db = 20 * Math.log10(ultra / pass);
  R.alias[rate] = {pass: +pass.toFixed(4), ultra: +ultra.toFixed(6), db: +db.toFixed(1)};
  assert(db < -40, `${rate}: 30 kHz is ${db.toFixed(1)} dB under 1 kHz (needs < -40 dB)`);
}

// 5. A damaged garden.bin: NaN / inf in each saved depth and tail never reaches the audio (Codex review 03).
R.damaged = [];
for (const [offset, bits] of [[20, 0x7fc00000], [24, 0x7fc00000], [28, 0x7fc00000], [24, 0x7f800000], [24, 0xff800000]]) {
  const files = new Map(); const a = session({files}); a.e.vibe_wasm_event(SAVE, 0, 0, 0, 0);
  const g = files.get("garden.bin"); new DataView(g.buffer, g.byteOffset).setUint32(offset, bits, true);
  const b = session({files, blocks: [1, 17, 73, 128, 256]}); b.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
  b.e.vibe_wasm_event(PAD, 4, 1, .9, 0); const r = b.run(2); b.e.vibe_wasm_event(PAD, 4, 0, 0, 0); b.run(.5);
  R.damaged.push({offset, bits: bits.toString(16), peak: +r.peak.toFixed(4)});
  assert(r.peak > .001, "it still plays");
}

console.log(JSON.stringify(R, null, 1));
console.log("PASSED: headroom at every rate, no jump on / off, long press explains, ANSWER timing block-independent, input resampler (30 kHz < -40 dB), damaged garden.bin safe.");
