// 0.31: functions the other tests leave out, end to end: MIDI LEARN (a controller's CC moves the learned macro) and
// DRIFT (two captured scenes, the garden glides between them; without scenes it says why it cannot start).
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, MIDI = 5, TRANSPORT = 8, MENU = 19, ENTER = 9, HOME = 17, F1 = 0;
const LEARN = 47, DRIFT = 13, SCENES = 14, LIVE = 0;
function session() {
  const e = instantiate(module, new Map()).exports;
  e.vibe_wasm_init(48000);
  const cstr = p => { const b = new Uint8Array(e.memory.buffer, p); let n = 0; while (b[n]) n++; return Buffer.from(b.subarray(0, n)).toString(); };
  const ids = []; for (let i = 0; i < e.vibe_wasm_param_count(); i++) ids.push(cstr(new Uint32Array(e.memory.buffer, e.vibe_wasm_param_info(i), 1)[0]));
  const s = {
    e,
    press: id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); },
    turn: (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0),
    cc: (n, v, ch = 0) => e.vibe_wasm_event(MIDI, 0, 0, 0, (0xb0 | ch) | (n << 8) | (v << 16)),
    run: sec => { for (let k = 0; k < 48000 * sec; k += 128) e.vibe_wasm_audio(128, 0); e.vibe_wasm_draw(); },
    open: v => { s.press(MENU); s.turn(0, -60); s.turn(0, v); s.press(ENTER); s.run(.05); },
    param: id => e.vibe_wasm_param_value(ids.indexOf(id)),
    screen: () => new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(),
  };
  s.press(HOME); s.press(HOME); s.run(.1);
  return s;
}
const R = {};

// 1. MIDI LEARN: slot 1, target DENSITY (the default), F3 arms, a CC 74 arrives: it is learned; CC 74 then moves DENSITY.
{ const s = session();
  s.open(LEARN); s.press(F1 + 2); s.run(.05);
  s.cc(74, 10); s.run(.1);
  for (let v = 0; v <= 127; v += 8) { s.cc(74, v); s.run(.02); }            // turned through its range (a pickup waits
  s.cc(74, 127); s.run(.2); const high = s.param("macro_density");          // for the knob to meet the value; values in %)
  for (let v = 120; v >= 0; v -= 8) { s.cc(74, v); s.run(.02); }
  s.run(.2); const low = s.param("macro_density");
  R.learn = {high, low};
  assert(high > 90 && low < 10, `a learned CC 74 moves DENSITY across its range (${high} -> ${low})`); }

// 2. DRIFT: without scenes it refuses and says so; with A and B captured it moves the macros between them.
{ const s = session();
  s.open(DRIFT); const before = s.screen(); s.turn(0, 1); s.run(.1); const after = s.screen();
  let changed = 0; for (let i = 0; i < 480 * 40; i++) if (before[i] !== after[i]) changed++;
  R.driftRefusal = changed; assert(changed > 40, "DRIFT without scenes shows why it does not start");
  s.open(LIVE); s.turn(0, -50); s.run(.1); const a = s.param("macro_density");
  s.open(SCENES); s.press(F1); s.run(.1);                                         // scene A: DENSITY low
  s.open(LIVE); s.turn(0, 50); s.run(.1); const b = s.param("macro_density");
  s.open(SCENES); s.press(F1 + 1); s.run(.1);                                     // scene B: DENSITY high
  s.open(DRIFT); s.turn(0, 2); s.turn(2, 50); s.run(.1);                           // drift AUTO (mode 2), rate up
  s.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
  // (the drift moves the macros the engine plays, not the stored values: watch the dot between A and B on the page)
  const band = () => s.screen().subarray(150 * 480, 200 * 480);
  const shots = []; for (let i = 0; i < 12; i++) { s.run(1); shots.push(band()); }
  let moved = 0; for (let i = 1; i < shots.length; i++) { let d = 0; for (let j = 0; j < shots[i].length; j++) if (shots[i][j] !== shots[0][j]) d++; if (d > 40) moved++; }
  R.drift = {a, b, moved};
  assert(moved >= 6, `DRIFT AUTO glides between scene A and B (${moved} of 11 pictures moved)`); }

// 3. (Codex review 11 B5) a host that refuses to write garden.bin: SAVE says so instead of failing silently.
{ const files = new Map(); let inst = null;
  const mem = () => inst.exports.memory.buffer;
  const nm = (p, n) => Buffer.from(mem(), p, n).toString();
  inst = new WebAssembly.Instance(module, {env: {
    vibe_host_log: () => {}, vibe_host_request: () => {},
    vibe_host_store: (op, np, nl, dp, len) => {
      if (op === 1) { if (nm(np, nl) === "garden.bin") return -1; files.set(nm(np, nl), new Uint8Array(new Uint8Array(mem(), dp, len))); return 0; }
      if (op === 0) { const f = files.get(nm(np, nl)); if (!f) return -1; const n = Math.min(len, f.length); new Uint8Array(mem(), dp, n).set(f.subarray(0, n)); return n; }
      if (op === 2) { const f = files.get(nm(np, nl)); return f ? f.length : -1; }
      return -1; }}});
  const e = inst.exports; e.vibe_wasm_init(48000);
  for (let k = 0; k < 48000 * .2; k += 128) e.vibe_wasm_audio(128, 0);
  const head = () => new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(0, 34 * 480);
  const before = head(); e.vibe_wasm_event(18, 0, 0, 0, 0); const after = head();
  let changed = 0; for (let i = 0; i < before.length; i++) if (before[i] !== after[i]) changed++;
  R.saveRefused = changed; assert(changed > 80, "a refused SAVE shows a message in the header"); }

console.log(JSON.stringify(R));
console.log("PASSED: MIDI LEARN moves the learned macro; DRIFT refuses without scenes (and says so) and glides between A and B; a refused SAVE is reported.");
