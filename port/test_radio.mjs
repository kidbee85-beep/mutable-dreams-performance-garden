// 0.13 MUTABLE DREAMS FM: the station's programme runs by itself – new Biome and tempo each piece,
// SKIP jumps to the next, STOP ends the broadcast. Pads still play over it.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const rate = 48000, BUTTON = 3, ENCODER = 4, MENU = 19, ENTER = 9, F1 = 0, STOP = 22, RADIO_PAGE = 39;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
const press = id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
const tempos = [];
let peak = 0, frames = 0;
const run = sec => {
  for (let n = 0; n < rate * sec; n += 128) {
    e.vibe_wasm_audio(128, 0); frames += 128;
    for (let i = 0; i < 128; i++) { const x = io[256 + i]; assert(Number.isFinite(x) && Math.abs(x) < 1); peak = Math.max(peak, Math.abs(x)); }
    if (frames % (rate / 2) < 128) { const t = Math.round(e.vibe_wasm_param_value(1)); if (tempos.at(-1)?.tempo !== t) tempos.push({at: +(frames / rate).toFixed(1), tempo: t}); }
  }
};
// open the RADIO page from MENU, choose SHORT pieces, go on air
press(MENU); e.vibe_wasm_event(ENCODER, 0, 0, -60, 0); e.vibe_wasm_event(ENCODER, 0, 0, RADIO_PAGE, 0); press(ENTER);
e.vibe_wasm_event(ENCODER, 1, 0, -1, 0);
press(F1);
run(2);
assert(peak > .001, "on air: the programme sounds");
if (outDir) {
  const px = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice();
  fs.mkdirSync(outDir, {recursive: true}); fs.writeFileSync(`${outDir}/radio-display.raw`, Buffer.from(px.buffer));
}
run(118);
const changes = tempos.length;
assert(changes >= 3, `the programme moves on by itself (${JSON.stringify(tempos)})`);
// SKIP: the next piece at once
const before = tempos.length;
press(F1 + 1); run(2);
assert(tempos.length > before, "SKIP starts the next piece");
// STOP ends the broadcast: no new pieces afterwards
press(STOP); run(1);
const stopped = tempos.length;
run(60);
assert.equal(tempos.length, stopped, "off air after STOP");
// 0.14: ON AIR AT START is saved with the project; the next start goes on air by itself.
const SAVE = 18, files = new Map();
const a1 = instantiate(module, files).exports; a1.vibe_wasm_init(rate);
const press1 = id => { a1.vibe_wasm_event(BUTTON, id, 1, 0, 0); a1.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
press1(MENU); a1.vibe_wasm_event(ENCODER, 0, 0, -60, 0); a1.vibe_wasm_event(ENCODER, 0, 0, RADIO_PAGE, 0); press1(ENTER);
a1.vibe_wasm_event(ENCODER, 3, 0, 1, 0);                        // AT START: ON AIR
a1.vibe_wasm_event(SAVE, 0, 0, 0, 0);
const a2 = instantiate(module, files).exports; a2.vibe_wasm_init(rate);
const io2 = new Float32Array(a2.memory.buffer, a2.vibe_wasm_io(), 3 * 256);
let autoPeak = 0;
for (let n = 0; n < rate * 3; n += 128) { a2.vibe_wasm_audio(128, 0); for (let i = 0; i < 128; i++) autoPeak = Math.max(autoPeak, Math.abs(io2[256 + i])); }
assert(autoPeak > .001, "on air at start, without PLAY");
// The station chime sounds at a change of piece (on by default): compare with the chime switched off.
function changeover(chime) {
  const x = instantiate(module).exports; x.vibe_wasm_init(rate);
  const io3 = new Float32Array(x.memory.buffer, x.vibe_wasm_io(), 3 * 256), out = [];
  const p3 = id => { x.vibe_wasm_event(BUTTON, id, 1, 0, 0); x.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
  p3(MENU); x.vibe_wasm_event(ENCODER, 0, 0, -60, 0); x.vibe_wasm_event(ENCODER, 0, 0, RADIO_PAGE, 0); p3(ENTER);
  if (!chime) p3(F1 + 3);
  p3(F1); for (let n = 0; n < rate; n += 128) x.vibe_wasm_audio(128, 0);
  p3(F1 + 1);                                                   // SKIP: a change of piece now
  for (let n = 0; n < rate * 1.5; n += 128) { x.vibe_wasm_audio(128, 0); for (let i = 0; i < 128; i++) out.push(io3[256 + i]); }
  return out;
}
const withChime = changeover(true), without = changeover(false);
let chimeDiff = 0; for (let i = 0; i < withChime.length; i++) chimeDiff += Math.abs(withChime[i] - without[i]);
assert(chimeDiff > .5, "the station chime sounds at the change");
console.log(JSON.stringify({tempoChanges: tempos, peak: +peak.toFixed(4), autoPeak: +autoPeak.toFixed(4), chimeDiff: +chimeDiff.toFixed(2)}, null, 2));
console.log("PASSED: Mutable Dreams FM – on air from the RADIO page, pieces change by themselves, STOP ends it, on air at start, station chime.");
