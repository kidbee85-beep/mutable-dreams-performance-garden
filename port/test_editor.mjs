// 0.15 Dream Console: the page's commands (vibe.send) reach the firmware, the status packet (vibe.onData) comes back.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const rate = 48000;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const fb = () => e.vibe_wasm_draw();
const send = (...bytes) => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), bytes.length).set(bytes); e.vibe_wasm_editor_data_in(bytes.length); };
const audio = sec => { for (let n = 0; n < rate * sec; n += 128) e.vibe_wasm_audio(128, 0); };
const take = () => {
  const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer);
  const len = dv.getUint32(box, true); if (!len) return null;
  const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); return b;
};
const status = () => { let s = null; for (let i = 0; i < 12; i++) { fb(); s = take() || s; } return s; };
const texts = b => { const out = []; for (let k = 32; out.length < 10 && k < b.length; k += 1 + b[k]) out.push(Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString()); return out; };
const report = {};

audio(.5);
let s = status();
assert(s && s[0] === 100 && s.length > 40, "status packet arrives");
let t = texts(s);
assert.equal(t.length, 10, "biome, family, radio line, page, six favourites");
report.first = {biome: t[0], family: t[1], page: t[3], favourites: t.slice(4)};

send(1); audio(.5); s = status(); assert.equal(s[1], 1, "PLAY from the page");
send(13, 3); audio(1); s = status(); assert.equal(s[12], 3, "favourite F4"); assert.equal(texts(s)[0], t[7], "F4's Biome is loaded");
send(3); audio(1); s = status(); assert.equal(s[2], 1, "ON AIR"); assert(texts(s)[2].length > 0, "radio line");
report.on_air = texts(s)[2];
send(3); audio(.3); s = status(); assert.equal(s[2], 0, "OFF AIR");
send(10, 4, 120); audio(.3); s = status(); assert(s[16 + 4] > 100, "pad 5 lights while held from the page");
send(11, 4); audio(.6); s = status(); assert(s[16 + 4] < 255, "pad 5 lets go");
send(15, 5); audio(.2); s = status(); assert((s[13] >> 5) & 1, "SHIFT-click latches pad 6");
send(15, 5); audio(.2); s = status(); assert(!((s[13] >> 5) & 1), "and unlatches it");
send(12, 2, 1); audio(.2); s = status(); assert((s[15] >> 2) & 1, "moment 3 stored");
send(9); audio(.2); s = status(); assert(s[7] || s[8], "gesture record starts (or waits for the bar)");
send(9); audio(.2);
send(2); audio(.3); s = status(); assert.equal(s[1], 0, "STOP from the page"); assert.equal(s[2], 0, "STOP ends the broadcast");
for (const bad of [[0], [10], [10, 200, 9], [12, 99, 1], [13, 77], [15, 99], [255, 1, 2, 3]]) { send(...bad); audio(.05); }
const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
for (let n = 0; n < rate; n += 128) { e.vibe_wasm_audio(128, 0); for (let i = 0; i < 128; i++) assert(Number.isFinite(io[256 + i])); }
report.passed = true;
console.log(JSON.stringify(report, null, 1));
