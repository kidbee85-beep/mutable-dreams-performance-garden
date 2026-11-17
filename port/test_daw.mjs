// 0.14 DAW and controller features:
//  - SDK 26: with 8+ host outputs, the six engine tracks come as dry stems on OUT 3-8 (at any host rate)
//  - Program Change on channel 16 (+ bank select CC 0) picks a Biome by its place in the library
//  - SDK 24/25 Link: page, encoder names / values and a 240 x 240 picture for a controller with a screen
import assert from "node:assert/strict";
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const results = {};
function session(rate, outputs = 2) {
  const e = instantiate(module).exports;
  const ports = e.vibe_wasm_ports();
  new DataView(e.memory.buffer).setInt32(ports + 8, outputs, true);       // host_out
  e.vibe_wasm_init(rate);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const s = { e, rate, main: [], stems: Array.from({length: 6}, () => []),
    run: sec => { for (let n = 0; n < rate * sec; n += 128) { e.vibe_wasm_audio(128, 0);
      for (let i = 0; i < 128; i++) s.main.push(io[256 + i]);
      for (let k = 0; k < 6; k++) { const o = new Float32Array(e.memory.buffer, ports + 14864 + k * 256 * 4, 128); for (let i = 0; i < 128; i++) { assert(Number.isFinite(o[i]) && Math.abs(o[i]) <= 1); s.stems[k].push(o[i]); } } } } };
  return s;
}
const rms = a => Math.sqrt(a.reduce((x, v) => x + v * v, 0) / Math.max(1, a.length));

// 1. Stems at 48 and 44.1 kHz: active tracks carry sound, a two-output host gets none.
for (const rate of [48000, 44100]) {
  const s = session(rate, 8); s.e.vibe_wasm_event(8, 0, 1, 0, 0); s.run(6);
  const levels = s.stems.map(x => +rms(x).toFixed(5));
  results[`stems${rate}`] = levels;
  assert(levels.filter(v => v > 1e-4).length >= 3, `stems carry the tracks at ${rate} Hz`);
}
const two = session(48000, 2); two.e.vibe_wasm_event(8, 0, 1, 0, 0); two.run(2);
assert(two.stems.every(x => rms(x) === 0), "no stems on a two-output host");

// 2. Program Change 10 on channel 16 loads another Biome (the mix changes).
const dry = session(48000); dry.e.vibe_wasm_event(8, 0, 1, 0, 0); dry.run(8);
const pc = session(48000); pc.e.vibe_wasm_event(8, 0, 1, 0, 0); pc.e.vibe_wasm_event(5, 0, 0, 0, 0xcf | (10 << 8)); pc.run(8);
let d = 0; for (let i = 4 * 48000; i < 8 * 48000; i++) d += Math.abs(pc.main[i] - dry.main[i]);
results.programChange = +d.toFixed(2);
assert(d > .5, "Program Change on channel 16 changes the Biome");

// 3. Link: page and encoders, and a 240 x 240 picture.
const ln = session(48000); ln.run(.2); ln.e.vibe_wasm_draw();
const link = ln.e.vibe_wasm_link(), mem = new Uint8Array(ln.e.memory.buffer);
const str = (p, n) => { let s = ""; for (let i = 0; i < n && mem[p + i]; i++) s += String.fromCharCode(mem[p + i]); return s; };
const pageName = str(link + 4, 20), first = str(link + 24, 12), firstValue = str(link + 24 + 7 * 12, 12);
results.link = {page: pageName, encoder1: first, value1: firstValue};
assert.equal(pageName, "LIVE"); assert.equal(first, "DENSITY");
const scr = new Uint32Array(ln.e.memory.buffer, ln.e.vibe_wasm_link_screen(), 2);
assert(scr[0] > 0 && scr[1] > 0, "a link screen is offered");
const px = new Uint16Array(ln.e.memory.buffer, scr[1], 240 * 240).slice();
assert(px.some(p => p !== px[0]), "the link screen holds a picture");
if (outDir) {
  const W = 240, H = 240, raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const p = px[y * W + x], o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31; }
  const chunk = (t, dd) => { const l = Buffer.alloc(4); l.writeUInt32BE(dd.length); const td = Buffer.concat([Buffer.from(t), dd]); const c = Buffer.alloc(4); c.writeUInt32BE(zlib.crc32(td) >>> 0); return Buffer.concat([l, td, c]); };
  const h = Buffer.alloc(13); h.writeUInt32BE(W, 0); h.writeUInt32BE(H, 4); h[8] = 8; h[9] = 2;
  fs.mkdirSync(outDir, {recursive: true});
  fs.writeFileSync(`${outDir}/Link-Screen.png`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", h), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
console.log(JSON.stringify(results, null, 2));
console.log("PASSED: six stems on OUT 3-8 at 48 / 44.1 kHz, Program Change picks Biomes, Link page + screen.");
