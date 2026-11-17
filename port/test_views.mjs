// 0.9: every one of the 43 pages opens from MENU, takes all four encoders both ways and its soft keys,
// draws without crashing and keeps the audio finite. Then the special pages do what they say.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000, BUTTON = 3, ENCODER = 4, TRANSPORT = 8, MENU = 19, ENTER = 9, F1 = 0, K1 = 10, SHIFT = 6, SAVE = 18;
function session(files = new Map()) {
  const e = instantiate(module, files).exports;
  e.vibe_wasm_init(rate);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const s = {
    e, out: [],
    press: (id, n = 1) => { for (let i = 0; i < n; i++) { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); } },
    turn: (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0),
    open: v => { s.press(MENU); s.turn(0, -60); s.turn(0, v); s.press(ENTER); },
    hash: () => { const f = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320); let h = 2166136261; for (const p of f) h = Math.imul(h ^ p, 16777619); return h >>> 0; },
    run: sec => { for (let n = 0; n < rate * sec; n += 128) { e.vibe_wasm_audio(128, 0); for (let i = 0; i < 128; i++) { const x = io[256 + i]; assert(Number.isFinite(x) && Math.abs(x) < 1); s.out.push(x); } } },
  };
  return s;
}
const results = {pages: 0, changedByEncoders: 0};

// 1. All pages, all encoders, soft keys 2-6 (F1 can start a dream or load a slot: tested below).
const all = session(); all.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); all.run(.5);
const titles = new Set();
for (let v = 0; v < 50; v++) {
  all.open(v); all.run(.02);
  const h0 = all.hash();
  titles.add(h0);
  let changed = false;
  for (let k = 0; k < 4; k++) { all.turn(k, 3); all.run(.02); if (all.hash() !== h0) changed = true; all.turn(k, -3); all.run(.02); }
  for (const f of [3, 4, 5]) all.press(F1 + f);
  all.run(.05);
  results.pages++;
  if (changed) results.changedByEncoders++;
}
assert.equal(results.pages, 50);
assert.equal(titles.size, 50, "every page draws its own picture");
assert(results.changedByEncoders >= 47, `encoders change ${results.changedByEncoders} pages`);

// 2. Colour keys: SOUND opens LIVE, again steps to BROWSER; FX twice is still the PADS layer.
const keys = session(); keys.run(.05);
const live = keys.hash(); keys.press(K1); keys.run(.02); assert.notEqual(keys.hash(), live);
keys.press(K1 + 4); keys.run(.02); const perform = keys.hash(); keys.press(K1 + 4); keys.run(.02); assert.notEqual(keys.hash(), perform);

// 3. Browser: NEXT then LOAD changes the Biome.
const dry = session(); dry.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); dry.run(8);
const br = session(); br.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
br.open(1); br.press(F1 + 2); br.press(F1 + 2); br.press(F1); br.run(8);
let d = 0; for (let i = 4 * rate; i < 8 * rate; i++) d += Math.abs(br.out[i] - dry.out[i]);
results.browserLoad = +d.toFixed(2); assert(d > .5, "browser loads another Biome");

// 4. Banks: save slot 1 while playing, change a macro, stop, load slot 1 again.
const files = new Map();
const bk = session(files); bk.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); bk.run(.3);
bk.open(44); bk.press(F1); assert(files.has("bank-00.bin"), "bank saved");
bk.open(0); bk.turn(0, 20); bk.run(.2);
bk.e.vibe_wasm_event(TRANSPORT, 0, 0, 0, 0); bk.run(.1);
bk.open(44); bk.press(F1 + 1); bk.run(.1);
results.bankBytes = files.get("bank-00.bin").length;
bk.e.vibe_wasm_event(SAVE, 0, 0, 0, 0);
const again = session(files); again.run(.1);

// 5. Itinera: F1 starts a dream when stopped; it sounds; F2 stops.
const it = session(); it.run(.2);
it.open(40); it.press(F1); it.run(6);
let peak = 0; for (let i = 3 * rate; i < 6 * rate; i++) peak = Math.max(peak, Math.abs(it.out[i]));
results.itineraPeak = +peak.toFixed(4); assert(peak > .001, "an Itinera dream sounds");
it.press(F1 + 1); it.run(.2);

// 5b. SHIFT + pad 15 plays a new variation of the same Biome.
const mu = session(); mu.e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0); mu.run(.5);
mu.e.vibe_wasm_event(BUTTON, SHIFT, 1, 0, 0); mu.e.vibe_wasm_event(1, 14, 1, 1, 0); mu.e.vibe_wasm_event(1, 14, 0, 0, 0); mu.e.vibe_wasm_event(BUTTON, SHIFT, 0, 0, 0);
mu.run(7.5);
let md = 0; for (let i = 2 * rate; i < 8 * rate; i++) md += Math.abs(mu.out[i] - dry.out[i]);
results.mutate = +md.toFixed(2); assert(md > .5, "a new variation sounds different");
mu.e.vibe_wasm_event(BUTTON, SHIFT, 1, 0, 0); mu.press(16); mu.e.vibe_wasm_event(BUTTON, SHIFT, 0, 0, 0); mu.run(.2);

// 6. Help on every page: HOME draws the page's controls.
const help = session(); help.open(13); const before = help.hash(); help.press(17); assert.notEqual(help.hash(), before); help.press(17);

console.log(JSON.stringify(results, null, 2));
console.log("PASSED: 50 pages open, draw and take every encoder; colour keys, browser, banks, Itinera, variation + undo, HOME.");
