// 0.17 Biome library in the Dream Console: names (102) and all 271 Biomes (101) on request, load (21), store a
// favourite (22), the playing Biome in the status, and the status resumes after the library.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const run = sec => { for (let n = 0; n < rate * sec; n += 128) e.vibe_wasm_audio(128, 0); };
const send = (...b) => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), b.length).set(b); e.vibe_wasm_editor_data_in(b.length); };
const take = () => { const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), len = dv.getUint32(box, true);
  if (!len) return null; const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); return b; };
const text = (b, k) => [Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString(), k + 1 + b[k]];
const status = () => {
  for (let i = 0; i < 40; i++) { e.vibe_wasm_draw(); const b = take(); if (b && b[0] === 100) {
    let k = 32; const t = []; for (let j = 0; j < 10; j++) { let s; [s, k] = text(b, k); t.push(s); }
    return {biome: t[0], favs: t.slice(4), fav: b[12], id: b[k + 2] | (b[k + 3] << 8)}; } }
  throw new Error("no status");
};
run(.3);
send(20);
const families = [], scales = [], biomes = [];
let packets = 0, statusDuring = 0;
for (let i = 0; i < 40 && biomes.filter(Boolean).length < 271; i++) {
  e.vibe_wasm_draw();
  const b = take(); if (!b) continue;
  assert(b.length <= 4096, "a packet fits");
  if (b[0] === 100) { statusDuring++; continue; }
  packets++;
  if (b[0] === 102) {
    let k = 1; const nf = b[k++]; for (let j = 0; j < nf; j++) { let s; [s, k] = text(b, k); families.push(s); }
    const ns = b[k++]; for (let j = 0; j < ns; j++) { let s; [s, k] = text(b, k); scales.push(s); }
    continue;
  }
  assert.equal(b[0], 101);
  const total = b[1] | (b[2] << 8), start = b[3] | (b[4] << 8), count = b[5];
  assert.equal(total, 271);
  let k = 6;
  for (let j = 0; j < count; j++) {
    const id = b[k] | (b[k + 1] << 8), tempo = b[k + 2], root = b[k + 3], scale = b[k + 4], family = b[k + 5]; k += 6;
    let name; [name, k] = text(b, k);
    assert(id < 272 && tempo >= 30 && tempo <= 220 && root < 12 && scale < 13 && family < families.length && name.length > 0, `entry ${start + j} ${JSON.stringify({id,tempo,root,scale,family,name})}`);
    biomes[start + j] = {id, tempo, root, scale, family, name};
  }
  assert.equal(k, b.length, "packet fully used");
}
assert.equal(biomes.filter(Boolean).length, 271, "all 271 Biomes arrive");
assert.equal(new Set(biomes.map(b => b.id)).size, 271, "each Biome once");
assert.equal(scales.length, 13); assert(families.length >= 5);
assert.equal(statusDuring, 0, "the library goes first");
// load one
const pick = biomes[123];
send(21, pick.id & 255, pick.id >> 8); run(1.5);
let s = status();
assert.equal(s.id, pick.id, "21 loads the Biome"); assert.equal(s.biome, pick.name);
// store as F5
send(22, 4, pick.id & 255, pick.id >> 8); run(.1);
s = status(); assert.equal(s.favs[4], pick.name, "22 stores F5"); assert.equal(s.fav, 4);
// survives SAVE + restart
const files = new Map();
const a = instantiate(module, files).exports; a.vibe_wasm_init(rate);
new Uint8Array(a.memory.buffer, a.vibe_wasm_editor_inbox(), 4).set([22, 2, pick.id & 255, pick.id >> 8]); a.vibe_wasm_editor_data_in(4);
a.vibe_wasm_event(18, 0, 1, 0, 0); for (let i = 0; i < 4; i++) a.vibe_wasm_draw();
const b2 = instantiate(module, files).exports; b2.vibe_wasm_init(rate);
let saved = null;
for (let i = 0; i < 20 && !saved; i++) { b2.vibe_wasm_draw(); const box = b2.vibe_wasm_editor_outbox(), dv = new DataView(b2.memory.buffer), len = dv.getUint32(box, true);
  if (len) { const p = new Uint8Array(b2.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); if (p[0] === 100) saved = p; } }
let k = 32, names = []; for (let j = 0; j < 10; j++) { let t; [t, k] = text(saved, k); names.push(t); }
assert.equal(names[6], pick.name, "the stored favourite survives SAVE and a restart");
for (const bad of [[21, 255, 255], [22, 9, 0, 0], [22, 1, 255, 255], [21], [22, 1]]) send(...bad);
run(.3);
console.log(JSON.stringify({passed: true, packets, families, sample: biomes.slice(0, 3), picked: pick.name}, null, 1));
