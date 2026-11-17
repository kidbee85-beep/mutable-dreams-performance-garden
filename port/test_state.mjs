// 0.5: DAW parameters, saved state, encoder A/B depth, pedal, automation lanes.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
const EV = {PAD: 1, ENCODER: 4, PEDAL: 6, TRANSPORT: 8, PARAM: 10, SAVE: 18};
const VE_A = 4;

function session(files = new Map()) {
  const inst = instantiate(module, files), e = inst.exports;
  e.vibe_wasm_init(rate);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const ids = [];
  const cstr = p => { const b = new Uint8Array(e.memory.buffer, p); let n = 0; while (b[n]) n++; return Buffer.from(b.subarray(0, n)).toString(); };
  for (let i = 0; i < e.vibe_wasm_param_count(); i++) ids.push(cstr(new Uint32Array(e.memory.buffer, e.vibe_wasm_param_info(i), 1)[0]));
  const s = {
    e, files, ids, out: [],
    send: (type, id, down, value, midi = 0) => e.vibe_wasm_event(type, id, down, value, midi),
    param: id => e.vibe_wasm_param_value(ids.indexOf(id)),
    set: (id, v) => e.vibe_wasm_param_set(ids.indexOf(id), v),
    group: id => cstr(e.vibe_wasm_param_group(ids.indexOf(id))),
    changed: () => [0, 1].map(w => e.vibe_wasm_param_changed(w)),
    run: seconds => {
      for (let n = 0; n < rate * seconds; n += 128) {
        e.vibe_wasm_audio(128, 0);
        for (let i = 0; i < 128; i++) { assert(Number.isFinite(io[256 + i])); s.out.push(io[256 + i]); }
      }
    },
  };
  return s;
}
const diff = (a, b, from, to) => { let d = 0; for (let i = from * rate; i < to * rate; i++) d += Math.abs(a[i] - b[i]); return d; };
const results = {};

// 1. The parameter list: permanent ids, groups for the Editor.
const first = session();
assert.equal(first.ids.length, 30);
for (const id of ["biome", "tempo", "root", "scale", "macro_density", "fx_return", "texture_depth", "space_depth",
  "pad_tail", "gesture_bars", "gesture_grid", "tone_target", "perf_forest", "perf_dusk", "perf_pulse", "cc_out", "wander", "moment", "tone_mode", "gesture_speed", "notes_out"])
  assert(first.ids.includes(id), id);
assert.equal(first.group("perf_bloom"), "PERFORM · SPACE");
results.parameters = first.ids.length;

// 2. The firmware reports its own changes: NEXT -> biome 1, encoder A -> texture depth, pedal -> space depth.
const own = session();
own.send(EV.TRANSPORT, 0, 1, 0);
own.run(.1); own.changed();
own.send(EV.PAD, 14, 1, 1); own.send(EV.PAD, 14, 0, 0);
own.send(EV.ENCODER, VE_A, 0, -20);
own.send(EV.PEDAL, 1, 0, .25);
own.run(.1);
assert.equal(own.param("biome"), 1);
assert.equal(Math.round(own.param("texture_depth")), 60);
assert.equal(Math.round(own.param("space_depth")), 25);
assert(own.changed()[0] & 1, "biome change flagged to the host");

// 3. Automation lanes: perf_bloom alone changes the sound; texture depth 0 silences the FOREST pad.
const dry = session(); dry.send(EV.TRANSPORT, 0, 1, 0); dry.run(4);
const bloom = session(); bloom.send(EV.TRANSPORT, 0, 1, 0); bloom.set("perf_bloom", 1); bloom.run(4);
results.automationBloom = +diff(bloom.out, dry.out, .5, 4).toFixed(2);
assert(results.automationBloom > .05, "perf_bloom audible");
const muted = session(); muted.send(EV.TRANSPORT, 0, 1, 0); muted.set("texture_depth", 0);
muted.send(EV.PAD, 4, 1, 1); muted.run(4);
results.forestAtDepthZero = +diff(muted.out, dry.out, 0, 4).toFixed(6);
assert(results.forestAtDepthZero < 1e-6, "texture depth 0 mutes FOREST");
// Host-set values survive the engine catching up (no echo of the old value).
const echo = session(); echo.send(EV.TRANSPORT, 0, 1, 0);
echo.set("macro_density", 80); echo.set("tempo", 72); echo.run(1);
assert.equal(Math.round(echo.param("macro_density")), 80);
assert.equal(Math.round(echo.param("tempo")), 72);

// 4. SAVE then restart with the files: Biome, settings, macro and the looping gesture come back.
const files = new Map();
const a = session(files);
a.send(EV.TRANSPORT, 0, 1, 0);
a.set("biome", 3); a.set("pad_tail", 2); a.set("gesture_bars", 1); a.set("macro_density", 80);
a.run(.5);
a.send(EV.PAD, 15, 1, 1); a.send(EV.PAD, 15, 0, 0);
a.send(EV.PAD, 4, 1, 1); a.run(1.5); a.send(EV.PAD, 4, 0, 0);
a.send(EV.PAD, 15, 1, 1); a.send(EV.PAD, 15, 0, 0);      // stop the take early: it loops what it has
a.run(.5);
a.send(EV.SAVE, 0, 0, 0);
assert(files.has("patch.bin") && files.has("garden.bin"), "two files written");
results.savedBytes = Object.fromEntries([...files].map(([k, v]) => [k, v.length]));
const b = session(files);
b.run(.1);
assert.equal(b.param("biome"), 3);
assert.equal(+b.param("pad_tail").toFixed(2), 2);
assert.equal(b.param("gesture_bars"), 1);
assert.equal(Math.round(b.param("macro_density")), 80);
const c = session(); c.set("biome", 3); c.set("macro_density", 80); c.run(.1);
b.run(3); c.run(3);
results.restoredGestureDifference = +diff(b.out, c.out, .5, 3).toFixed(2);
assert(results.restoredGestureDifference > .05, "restored gesture loops at once");
// A damaged garden file is ignored, the firmware still starts and plays.
const broken = new Map(files); broken.set("garden.bin", new Uint8Array(7)); broken.set("patch.bin", new Uint8Array(13));
const d = session(broken); d.send(EV.TRANSPORT, 0, 1, 0); d.run(1);
assert(Math.max(...d.out.slice(rate * .5).map(Math.abs)) > .001, "plays with damaged files");

// 5. 0.6: SHIFT + pad latches; the pad keeps sounding after release and a tap lets it go.
const SHIFT = 6, BUTTON = 3;
const latch = session(); latch.send(EV.TRANSPORT, 0, 1, 0);
latch.send(BUTTON, SHIFT, 1, 0); latch.send(EV.PAD, 5, 1, .9); latch.send(BUTTON, SHIFT, 0, 0); latch.send(EV.PAD, 5, 0, 0);
latch.run(4);
results.latchedAirDifference = +diff(latch.out, dry.out, 2, 4).toFixed(2);
assert(results.latchedAirDifference > .5, "latched AIR keeps sounding after the hand left");
latch.send(EV.PAD, 5, 1, .9); latch.send(EV.PAD, 5, 0, 0);
const plain = session(); plain.send(EV.TRANSPORT, 0, 1, 0); plain.send(EV.PAD, 5, 1, .9); plain.send(EV.PAD, 5, 0, 0); plain.run(4);
results.unlatchedAirDifference = +diff(plain.out, dry.out, 2, 4).toFixed(4);
assert(results.unlatchedAirDifference < results.latchedAirDifference / 20, "without SHIFT the pad fades");
// Stage right-click = SHIFT: latches too.
const right = session(); right.send(EV.TRANSPORT, 0, 1, 0);
right.send(19, 0, 1, Math.ceil((72 + 70) * 4 / 3) * 4096 + Math.ceil((700 + 60 + 27) * 4 / 3), 2 << 8);   // (stage pixels 1280 x 720) right.send(19, 2, 0, 0, 0);
right.run(4);
assert(diff(right.out, dry.out, 2, 4) > .5, "right-click latches on the stage");

// 6. Overdub: record FOREST, loop, SHIFT + 16, play BLOOM through one pass: the loop now carries both.
function take(overdub) {
  const s = session(); s.send(EV.TRANSPORT, 0, 1, 0); s.set("tempo", 120);
  s.run(.2);
  s.send(EV.PAD, 15, 1, 1); s.send(EV.PAD, 15, 0, 0);
  s.send(EV.PAD, 4, 1, 1); s.run(1); s.send(EV.PAD, 4, 0, 0);
  s.send(EV.PAD, 15, 1, 1); s.send(EV.PAD, 15, 0, 0);           // stop the take: loops one second
  s.run(.5);
  if (overdub) { s.send(BUTTON, SHIFT, 1, 0); s.send(EV.PAD, 15, 1, 1); s.send(EV.PAD, 15, 0, 0); s.send(BUTTON, SHIFT, 0, 0); }
  s.send(EV.PAD, 8, 1, 1); s.run(1.1); s.send(EV.PAD, 8, 0, 0);
  s.run(6);
  return s;
}
const dub = take(true), nodub = take(false);
results.overdubDifference = +diff(dub.out, nodub.out, 4, 8.8).toFixed(2);
assert(results.overdubDifference > .5, "overdubbed BLOOM keeps returning in the loop");

// 7. CC out: off by default, CC 20-28 on channel 1 when on.
function midiOut(s) {
  const m = new DataView(s.e.memory.buffer, s.e.vibe_wasm_midi_out());
  const w = m.getUint32(0, true), r = m.getUint32(4, true), msgs = [];
  for (let i = r; i < w; i++) { const o = 16 + (i % 256) * 6; msgs.push([m.getUint8(o + 2), m.getUint8(o + 3), m.getUint8(o + 4)]); }
  m.setUint32(4, w, true);
  return msgs;
}
const quiet = session(); quiet.send(EV.TRANSPORT, 0, 1, 0); quiet.send(EV.PAD, 4, 1, 1); quiet.run(.5);
assert.equal(midiOut(quiet).length, 0, "no CC while cc_out is off");
const cc = session(); cc.set("cc_out", 1); cc.send(EV.TRANSPORT, 0, 1, 0); cc.send(EV.PAD, 4, 1, 1);
let seen = [];
for (let k = 0; k < 20; k++) { cc.run(.025); seen = seen.concat(midiOut(cc)); }
const forest = seen.filter(([st, d1]) => st === 0xb0 && d1 === 20);
results.ccMessages = seen.length;
assert(forest.length > 3 && forest.at(-1)[2] > 100, "FOREST rises on CC 20");
assert(seen.every(([st, d1]) => st === 0xb0 && d1 >= 20 && d1 <= 28), "only CC 20-28 on channel 1");

// 8. 0.7 MOMENTS: hold FOREST, SHIFT + F1 on the PADS layer stores it; F1 later glides back to it.
const K5 = 14, F1 = 0;
const dry12 = session(); dry12.send(EV.TRANSPORT, 0, 1, 0); dry12.run(12);
function padsLayer(s) { s.send(BUTTON, K5, 1, 0); s.send(BUTTON, K5, 0, 0); s.send(BUTTON, K5, 1, 0); s.send(BUTTON, K5, 0, 0); }
const mfiles = new Map();
const mo = session(mfiles); mo.send(EV.TRANSPORT, 0, 1, 0); padsLayer(mo);
mo.send(EV.PAD, 4, 1, 1); mo.run(1.5);
mo.send(BUTTON, SHIFT, 1, 0); mo.send(BUTTON, F1, 1, 0); mo.send(BUTTON, F1, 0, 0); mo.send(BUTTON, SHIFT, 0, 0);
mo.send(EV.PAD, 4, 0, 0); mo.run(3);                      // the hand has left, FOREST fades
const before = diff(mo.out, dry12.out, 4, 4.5);
mo.send(BUTTON, F1, 1, 0); mo.send(BUTTON, F1, 0, 0);    // recall
mo.run(3);
assert.equal(mo.param("moment"), 1, "moment 1 reported to the host");
const during = diff(mo.out, dry12.out, 7, 7.5);
results.momentRecall = {before: +before.toFixed(2), during: +during.toFixed(2)};
assert(during > before * 5 + .5, "F1 glides back to the stored FOREST");
mo.send(BUTTON, F1, 1, 0); mo.send(BUTTON, F1, 0, 0);    // same key: glide out
mo.run(4);
assert.equal(mo.param("moment"), 0);
assert(diff(mo.out, dry12.out, 11, 11.5) < during / 5, "same key glides out");
mo.send(EV.SAVE, 0, 0, 0);
// Moments survive a restart and recall from the DAW parameter.
const mr = session(mfiles); mr.send(EV.TRANSPORT, 0, 1, 0); mr.set("moment", 1); mr.run(4);
results.momentAfterRestart = +diff(mr.out, dry12.out, 3, 4).toFixed(2);
assert(results.momentAfterRestart > .5, "stored moment recalls after restart");
// An empty slot does nothing.
const empty = session(); empty.send(EV.TRANSPORT, 0, 1, 0); padsLayer(empty);
empty.send(BUTTON, 3, 1, 0); empty.send(BUTTON, 3, 0, 0); empty.run(4);
assert(diff(empty.out, dry12.out, 0, 4) < 1e-6, "empty moment slot is silent");

// 9. WANDER: off by default; on, the resting garden starts moving after two seconds; hands stop it.
const wa = session(); wa.send(EV.TRANSPORT, 0, 1, 0); wa.set("wander", 1); wa.run(12);
results.wander = {firstTwoSeconds: +diff(wa.out, dry12.out, 0, 1.9).toFixed(4), later: +diff(wa.out, dry12.out, 6, 12).toFixed(2)};
assert(results.wander.firstTwoSeconds < 1e-6, "wander waits two seconds");
assert(results.wander.later > .5, "wander moves the garden");
assert(wa.out.every(x => Number.isFinite(x) && Math.abs(x) < 1));

// 10. 0.8 TONE MODES: chord and arpeggio differ from a single note; nothing hangs after STOP.
function tone(mode, hold = 3) {
  const s = session(); s.send(EV.TRANSPORT, 0, 1, 0); s.set("tone_mode", mode); s.run(.3);
  s.send(EV.PAD, 0, 1, .9); s.run(hold); s.send(EV.PAD, 0, 0, 0); s.run(.5);
  s.send(EV.TRANSPORT, 0, 0, 0); s.run(6);
  return s;
}
const note = tone(0), chord = tone(1), arp = tone(2);
results.toneModes = {chordVsNote: +diff(chord.out, note.out, .3, 3.3).toFixed(2), arpVsNote: +diff(arp.out, note.out, .3, 3.3).toFixed(2)};
assert(results.toneModes.chordVsNote > .5, "chord adds notes");
assert(results.toneModes.arpVsNote > .5, "arpeggio walks");
const tail = x => { let p = 0; for (let i = x.out.length - rate; i < x.out.length; i++) p = Math.max(p, Math.abs(x.out[i])); return p; };
results.toneModes.tailPeaks = [note, chord, arp].map(x => +tail(x).toFixed(5));
for (const x of [chord, arp]) assert(tail(x) <= tail(note) * 1.5 + 1e-4, "no hanging notes after STOP");

// 11. 0.8 GESTURE PLAYBACK: reverse and half speed replay the same take differently.
function loopAt(speed) {
  const s = session(); s.send(EV.TRANSPORT, 0, 1, 0); s.set("tempo", 120); s.set("gesture_speed", speed); s.run(.2);
  s.send(EV.PAD, 15, 1, 1); s.send(EV.PAD, 15, 0, 0);
  s.send(EV.PAD, 4, 1, 1); s.run(.6); s.send(EV.PAD, 4, 0, 0); s.send(EV.PAD, 8, 1, 1); s.run(1); s.send(EV.PAD, 8, 0, 0);
  s.send(EV.PAD, 15, 1, 1); s.send(EV.PAD, 15, 0, 0); s.run(8);
  return s;
}
const normal = loopAt(1), reverse = loopAt(3), half = loopAt(0);
results.gestureSpeed = {reverse: +diff(reverse.out, normal.out, 3, 9.5).toFixed(2), half: +diff(half.out, normal.out, 3, 9.5).toFixed(2)};
assert(results.gestureSpeed.reverse > .5 && results.gestureSpeed.half > .5, "playback speed changes the loop");

// 12. 0.12 DREAM AS MIDI: off by default; on, the engine's notes go out on channels 1-6 (kit 10), each with its off.
const silentNotes = session(); silentNotes.send(EV.TRANSPORT, 0, 1, 0); silentNotes.run(3);
assert.equal(midiOut(silentNotes).filter(([st]) => (st & 0xf0) === 0x90).length, 0, "no notes while off");
const nt = session(); nt.set("notes_out", 1); nt.send(EV.TRANSPORT, 0, 1, 0);
let msgs = [];
for (let k = 0; k < 120; k++) { nt.run(.05); msgs = msgs.concat(midiOut(nt)); }
nt.send(EV.TRANSPORT, 0, 0, 0); nt.run(.1); msgs = msgs.concat(midiOut(nt));
const ons = msgs.filter(([st, , v]) => (st & 0xf0) === 0x90 && v > 0), offs = msgs.filter(([st, , v]) => (st & 0xf0) === 0x80 || ((st & 0xf0) === 0x90 && v === 0));
results.notesOut = {on: ons.length, off: offs.length, channels: [...new Set(ons.map(([st]) => (st & 15) + 1))].sort((a, b) => a - b)};
assert(ons.length >= 4, "the dream's notes go out");
assert(ons.every(([st]) => (st & 15) <= 5 || (st & 15) === 9), "channels 1-6 or 10");
assert.equal(ons.length, offs.length, "every note is released, also on STOP");

console.log(JSON.stringify(results, null, 2));
console.log("PASSED: 30 parameters, dream as MIDI (released on STOP), tone modes (no hanging notes), gesture playback speed, moments (store, glide, release, restart, DAW), wander, latch (pad and stage), overdub, CC out, own changes reported, automation lanes, depth, SAVE + restart, damaged files.");
