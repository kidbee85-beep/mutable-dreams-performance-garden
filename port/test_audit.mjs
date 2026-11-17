// 0.18 the audit's fixes, one scenario each (see ÜBERGABE 0.18).
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
const EV = {PAD: 1, BUTTON: 3, ENCODER: 4, MIDI: 5, TRANSPORT: 8, PARAM: 10, SAVE: 18, STAGE: 19};
const B = {F1: 0, F2: 1, F3: 2, SHIFT: 6, ENTER: 9, MENU: 19, STOP: 22, PLAY: 23, HOME: 17, RETURN: 16};
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);       // stage layout units -> pixels
function session(files = new Map()) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  const s = {
    e, files,
    ev: (t, id, down, v = 0, m = 0) => e.vibe_wasm_event(t, id, down, v, m),
    press: id => { s.ev(EV.BUTTON, id, 1); s.ev(EV.BUTTON, id, 0); },
    pad: (id, down, v = .9) => s.ev(EV.PAD, id, down, down ? v : 0),
    shiftPad: id => { s.ev(EV.BUTTON, B.SHIFT, 1); s.pad(id, 1); s.pad(id, 0); s.ev(EV.BUTTON, B.SHIFT, 0); },
    enc: (id, v) => s.ev(EV.ENCODER, id, 0, v),
    run: sec => { for (let n = 0; n < rate * sec; n += 128) e.vibe_wasm_audio(128, 0); },
    send: (...b) => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), b.length).set(b); e.vibe_wasm_editor_data_in(b.length); },
    page: v => { s.press(B.MENU); s.enc(0, -60); s.enc(0, v); s.press(B.ENTER); },
    status: () => {
      for (let i = 0; i < 40; i++) {
        e.vibe_wasm_draw();
        const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), len = dv.getUint32(box, true);
        if (!len) continue;
        const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true);
        if (b[0] !== 100) continue;
        let k = 32; const t = [];
        for (let j = 0; j < 10; j++) { t.push(Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString()); k += 1 + b[k]; }
        return {b, page: t[3], favs: t.slice(4), playing: b[1], onAir: b[2], gmode: b[7], armed: b[8], fav: b[12],
                latched: b[13] | (b[14] << 8), menu: !!(b[k + 1] & 1), help: !!(b[k + 1] & 2), shift: !!(b[k + 1] & 4)};
      }
      throw new Error("no status");
    },
  };
  return s;
}
const R = {};
const RADIO = 39, BROWSER = 1, MIDI = 46;   // (0.24: LISTEN and SING after TAPE)

// 1. SHIFT + F2 on BROWSER stores the Biome shown, without stepping (twice = the same Biome)
{ const s = session(); s.page(BROWSER); s.run(.2);
  s.ev(EV.BUTTON, B.SHIFT, 1); s.press(B.F2); s.ev(EV.BUTTON, B.SHIFT, 0); s.run(.1); const a = s.status().favs[1];
  s.ev(EV.BUTTON, B.SHIFT, 1); s.press(B.F2); s.ev(EV.BUTTON, B.SHIFT, 0); s.run(.1); const b = s.status().favs[1];
  assert.equal(a, b, "SHIFT + F2 stores, it does not step"); R.browserFavourite = a; }

// 2. MENU open: F-keys and encoders 2-4 no longer act on the hidden page; HOME does not open a hidden help
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.page(RADIO); s.press(B.F1); s.run(.5);
  assert.equal(s.status().onAir, 1);
  s.press(B.MENU); s.press(B.F3); s.press(B.HOME); s.run(.2);
  let st = s.status(); assert.equal(st.onAir, 1, "F3 under MENU does not take the station off air"); assert(st.menu && !st.help);
  s.press(B.MENU); s.run(.1); st = s.status(); assert(!st.help, "no help appears after MENU closes"); R.menuGuard = true; }

// 3. STOP on the 8 x 8 grid / ITINERA F2 ends the broadcast too (one STOP everywhere)
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.page(RADIO); s.press(B.F1); s.run(.3);
  s.page(40); s.press(B.F2); s.run(2);                    // ITINERA, F2 = STOP
  assert.equal(s.status().onAir, 0, "ITINERA STOP ends the broadcast"); R.stopEverywhere = true; }

// 4. a pad held on the stage is released although the stage switched to ALL PAGES meanwhile
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.run(.3);
  s.ev(EV.STAGE, 0, 1, at(700 + 27, 72 + 70 + 27), 0);    // pointer 0 holds FOREST (pad 5)
  s.run(.5); assert(s.status().b[16 + 4] > 100, "FOREST sounds");
  s.ev(EV.STAGE, 0, 1, at(790, 26), 1); s.ev(EV.STAGE, 2, 0, at(790, 26), 1);   // pointer 1: PAGES
  s.ev(EV.STAGE, 2, 0, at(700 + 27, 72 + 70 + 27), 0);    // pointer 0 lets go
  s.run(3); assert(s.status().b[16 + 4] < 20, "FOREST is released"); R.stagePointer = true; }

// 5. an 8-bar gesture at 30 BPM records all 8 bars (64 s), not 41 s
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1);
  s.ev(EV.PARAM, 1, 0, 30); s.ev(EV.PARAM, 12, 0, 1); s.ev(EV.PARAM, 13, 0, 0); s.run(.5);
  s.pad(15, 1); s.pad(15, 0); s.pad(4, 1); let t = 0;
  while (s.status().gmode === 1 && t < 90) { s.run(1); t++; }
  assert(t >= 62 && t <= 66, `8 bars at 30 BPM take about 64 s (${t} s)`); R.gestureSeconds8BarsAt30 = t; }

// 6. ON AIR borrows Wander and OFF AIR gives it back
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.page(RADIO);
  assert.equal(s.e.vibe_wasm_param_value(25), 0); s.press(B.F1); s.run(.2);
  s.press(B.F3); s.run(.2); s.status();
  assert.equal(Math.round(s.e.vibe_wasm_param_value(25)), 0, "Wander is off again after OFF AIR"); R.wanderRestored = true; }

// 7. latched THIN ends with a new Biome (no dead latch dot)
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.run(.3); s.shiftPad(12); s.run(.2);
  assert(s.status().latched & (1 << 12)); s.pad(14, 1); s.pad(14, 0); s.run(1);
  assert(!(s.status().latched & (1 << 12)), "THIN latch cleared by NEXT"); R.thinLatch = true; }

// 8. a DAW all-notes-off keeps latches and the gesture
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.run(.3); s.shiftPad(5); s.run(.2);
  s.ev(EV.MIDI, 0, 0, 0, 0xb0 | (123 << 8)); s.run(.2);
  assert(s.status().latched & (1 << 5), "CC 123 leaves the latch"); R.allNotesOff = true; }

// 9. a Biome from the library that is a favourite moves the highlight
{ const s = session(); s.run(.2); s.send(21, 94, 0); s.run(1.5);
  assert.equal(s.status().fav, 2, "library load of favourite F3 highlights F3"); R.favouriteHighlight = true; }

// 10. soft keys on the mirrored display: x = 84 is inside F1 (it used to press F2)
{ const s = session(); s.ev(EV.TRANSPORT, 0, 1); s.page(RADIO); s.run(.2);
  s.send(19, 42, 150); s.run(.3);                         // display (84, 300)
  assert.equal(s.status().onAir, 1, "a click at x 84 presses F1 ON AIR"); R.softKeyHit = true; }

// 11. SHIFT from the Dream Console: SHIFT then F4 on BROWSER stores favourite 4
{ const s = session(); s.page(BROWSER); s.run(.2); const before = s.status().favs[3];
  s.send(16, 6); assert(s.status().shift, "sticky SHIFT shows"); s.send(16, 3); s.run(.1);
  const st = s.status(); assert(!st.shift, "SHIFT is used up"); R.consoleShift = {before, after: st.favs[3]};
  assert.equal(st.page, "BROWSER", "SHIFT + F4 did not change the family / page"); }

// 12. MIDI page settings survive SAVE and a restart
{ const files = new Map(); const a = session(files); a.page(MIDI); a.enc(0, 1); a.enc(2, 3); a.run(.2);
  const pxA = new Uint16Array(a.e.memory.buffer, a.e.vibe_wasm_draw(), 480 * 120).slice();
  a.ev(EV.SAVE, 0, 1); a.e.vibe_wasm_draw();
  const b = session(files); b.page(MIDI); b.run(.2);
  const pxB = new Uint16Array(b.e.memory.buffer, b.e.vibe_wasm_draw(), 480 * 120).slice();
  let diff = 0; for (let i = 0; i < pxA.length; i++) if (pxA[i] !== pxB[i]) diff++;
  const c = session(); c.page(MIDI); c.run(.2);
  const pxC = new Uint16Array(c.e.memory.buffer, c.e.vibe_wasm_draw(), 480 * 120).slice();
  let changed = 0; for (let i = 0; i < pxA.length; i++) if (pxA[i] !== pxC[i]) changed++;
  assert(changed > 200, `the settings were really changed (${changed} px against a fresh start)`);
  assert(diff < 200, `the MIDI page comes back as it was (${diff} px differ)`); R.midiSaved = {diff, changed}; }

// 13. HOME help open: F1 closes the help instead of acting (BANKS F1 would save)
{ const s = session(); s.page(44); s.press(B.HOME); s.run(.1); assert(s.status().help);
  s.press(B.F1); s.run(.1); const st = s.status(); assert(!st.help, "F1 closes the help"); R.helpGuard = true; }

console.log(JSON.stringify({passed: true, ...R}, null, 1));
