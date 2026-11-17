// 0.23 the stage's PLACES view: GARDEN | PLACES | PAGES; the four places drawn; a click on a place opens its page.
import assert from "node:assert/strict";
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const rate = 48000, STAGE = 19;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
const click = (x, y) => { e.vibe_wasm_event(STAGE, 0, 1, at(x, y), 0); e.vibe_wasm_event(STAGE, 2, 0, at(x, y), 0); };
const run = s => { for (let n = 0; n < rate * s; n += 128) e.vibe_wasm_audio(128, 0); };
const pageName = () => { for (let i = 0; i < 30; i++) { e.vibe_wasm_draw(); const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), len = dv.getUint32(box, true);
  if (!len) continue; const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); if (b[0] !== 100) continue;
  let k = 32; for (let j = 0; j < 3; j++) k += 1 + b[k]; return Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString(); } };
e.vibe_wasm_event(8, 0, 1, 0, 0);
e.vibe_wasm_event(1, 9, 1, .9, 0); run(4);                 // ECHO held: the cave rings
const garden = new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice();
click(728, 26); run(.3);                                   // PLACES
const places = new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice();
let diff = 0; for (let i = 0; i < garden.length; i += 7) if (garden[i] !== places[i]) diff++;
assert(diff > 30000, `PLACES draws its own picture (${diff})`);
assert.equal(pageName(), "LIVE", "PLACES keeps the device on LIVE");
if (outDir) {
  const raw = Buffer.alloc((1280 * 3 + 1) * 720);
  for (let y = 0; y < 720; y++) for (let x = 0; x < 1280; x++) { const v = places[y * 1280 + x], o = y * (1280 * 3 + 1) + 1 + x * 3;
    raw[o] = (v >> 11 & 31) * 255 / 31; raw[o + 1] = (v >> 5 & 63) * 255 / 63; raw[o + 2] = (v & 31) * 255 / 31; }
  const chunk = (t, d) => { const c = Buffer.alloc(12 + d.length); c.writeUInt32BE(d.length); c.write(t, 4); d.copy(c, 8); c.writeUInt32BE(zlib.crc32 ? zlib.crc32(Buffer.concat([Buffer.from(t), d])) >>> 0 : 0, 8 + d.length); return c; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(1280); ihdr.writeUInt32BE(720, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(`${outDir}/Stage-Orte.png`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
click(24 + 322 + 8 + 100, 72 + 100); run(.2);              // the spore meadow -> GRAIN
assert.equal(pageName(), "GRAIN", "a click on the meadow opens GRAIN");
click(660, 26); run(.2);                                   // GARDEN
assert.equal(pageName(), "LIVE");
click(728, 26); click(100, 72 + 230 + 8 + 100); run(.2);   // PLACES, the stream -> TAPE
assert.equal(pageName(), "TAPE");
console.log(JSON.stringify({passed: true, placesDiffer: diff}));
