// 0.19 Kid Bunka's 48 worlds: the device (BROWSER F5 / F1, encoders, SHIFT + F) and the Dream Console (23 / 24, 103 / 104).
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const press = id => { e.vibe_wasm_event(3, id, 1, 0, 0); e.vibe_wasm_event(3, id, 0, 0, 0); };
const run = s => { for (let n = 0; n < rate * s; n += 128) e.vibe_wasm_audio(128, 0); };
const send = (...b) => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), b.length).set(b); e.vibe_wasm_editor_data_in(b.length); };
const take = () => { const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), len = dv.getUint32(box, true);
  if (!len) return null; const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); return b; };
const text = (b, k) => [Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString(), k + 1 + b[k]];
const status = () => { for (let i = 0; i < 40; i++) { e.vibe_wasm_draw(); const b = take(); if (b && b[0] === 100) {
  let k = 32, t = []; for (let j = 0; j < 10; j++) { let s; [s, k] = text(b, k); t.push(s); }
  return {biome: b[k + 2] | (b[k + 3] << 8), world: b[k + 4] - 1, favs: t.slice(4), name: t[0]}; } } throw new Error("no status"); };
const R = {};
// the console's list
send(23);
const fams = [], worlds = [];
for (let i = 0; i < 20 && worlds.filter(Boolean).length < 48; i++) {
  e.vibe_wasm_draw(); const b = take(); if (!b || b[0] === 100) continue;
  assert(b.length <= 4096);
  if (b[0] === 104) { let k = 2; for (let j = 0; j < b[1]; j++) { let t; [t, k] = text(b, k); fams.push(t); } continue; }
  assert.equal(b[0], 103);
  let k = 4;
  for (let j = 0; j < b[3]; j++) {
    const family = b[k], biome = b[k + 1] | (b[k + 2] << 8), tempo = b[k + 3]; k += 4;
    let name, story; [name, k] = text(b, k); [story, k] = text(b, k);
    assert(family < fams.length && biome < 272 && name && story, `world ${b[2] + j}`);
    worlds[b[2] + j] = {family, biome, tempo, name, story};
  }
  assert.equal(k, b.length);
}
assert.equal(worlds.filter(Boolean).length, 48, "48 worlds arrive"); assert.equal(fams.length, 8, "in 8 families");
R.families = fams;
// enter one from the console
const w = 13; send(24, w); run(1.5);
let s = status();
assert.equal(s.biome, worlds[w].biome, "24 enters the world's Biome"); assert.equal(s.world, w, "the status names the world");
assert.equal(Math.round(e.vibe_wasm_param_value(1)), worlds[w].tempo, "and its tempo");
R.entered = {name: worlds[w].name, biome: s.name, tempo: worlds[w].tempo};
// the device: BROWSER, F5 worlds, encoder 2 four on, F1 enters
press(19); e.vibe_wasm_event(4, 0, 0, -60, 0); e.vibe_wasm_event(4, 0, 0, 1, 0); press(9);
press(4); e.vibe_wasm_event(4, 0, 0, 1, 0); e.vibe_wasm_event(4, 1, 0, 2, 0); run(.1);   // family: ALL -> the first, two on
press(0); run(1.5); s = status();
assert(s.world >= 0, "F1 enters a world on the device");
assert.equal(worlds[s.world].family, 0, "encoder 1 chose the first family (ALL -> 0)");
R.deviceWorld = worlds[s.world].name;
// SHIFT + F3 in the worlds stores the world's Biome as favourite 3
e.vibe_wasm_event(3, 6, 1, 0, 0); press(2); e.vibe_wasm_event(3, 6, 0, 0, 0); run(.1);
s = status(); const stored = s.favs[2];
R.favourite = stored;
assert(stored.length > 0);
// back to the Biomes: F5 again, F1 loads a Biome as before
press(4); press(0); run(1.5); s = status(); assert.equal(s.world, -1, "a plain Biome is no world");
// (0.21) wandering worlds: 5 minutes, then the next world of the family; kept by SAVE and a restart
{
  const files = new Map();
  const w2 = instantiate(module, files).exports; w2.vibe_wasm_init(rate);
  const send2 = (...b) => { new Uint8Array(w2.memory.buffer, w2.vibe_wasm_editor_inbox(), b.length).set(b); w2.vibe_wasm_editor_data_in(b.length); };
  const take2 = () => { const box = w2.vibe_wasm_editor_outbox(), dv = new DataView(w2.memory.buffer), len = dv.getUint32(box, true);
    if (!len) return null; const b = new Uint8Array(w2.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); return b; };
  const st2 = (x) => { for (let i = 0; i < 40; i++) { x.vibe_wasm_draw(); const box = x.vibe_wasm_editor_outbox(), dv = new DataView(x.memory.buffer), len = dv.getUint32(box, true);
    if (!len) continue; const b = new Uint8Array(x.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); if (b[0] !== 100) continue;
    let k = 32; for (let j = 0; j < 10; j++) k += 1 + b[k]; return {world: b[k + 4] - 1, wander: b[k + 5]}; } throw new Error("no status"); };
  void take2;
  w2.vibe_wasm_event(8, 0, 1, 0, 0);
  send2(25, 5); send2(24, 6);                                       // Bell Orchard at Noon (Bells And Resonance)
  for (let n = 0; n < rate * 2; n += 128) w2.vibe_wasm_audio(128, 0);
  let x = st2(w2); assert.equal(x.world, 6); assert.equal(x.wander, 5);
  for (let n = 0; n < rate * 302; n += 128) w2.vibe_wasm_audio(128, 0);
  x = st2(w2);
  assert(x.world !== 6 && worlds[x.world].family === worlds[6].family, `after 5 minutes the next world of the family (${x.world})`);
  R.wanderedTo = worlds[x.world].name;
  w2.vibe_wasm_event(18, 0, 1, 0, 0); w2.vibe_wasm_draw();
  const w3 = instantiate(module, files).exports; w3.vibe_wasm_init(rate);
  const y = st2(w3); assert.equal(y.wander, 5, "the wandering setting survives SAVE and a restart");
  R.restored = y;
}
for (const bad of [[24, 200], [24], [23, 9], [25, 7]]) send(...bad);
run(.3);
console.log(JSON.stringify({passed: true, ...R}, null, 1));
