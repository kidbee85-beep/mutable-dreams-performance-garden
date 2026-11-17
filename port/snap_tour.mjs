// 0.26: pictures of FIRST STEPS – the card on the display and on the stage (step 2: hold BLOOM, the pad glows).
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";
const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const png = (name, px, w, h) => {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) { const v = px[y * w + x], o = y * (w * 3 + 1) + 1 + x * 3;
    raw[o] = (v >> 11 & 31) * 255 / 31; raw[o + 1] = (v >> 5 & 63) * 255 / 63; raw[o + 2] = (v & 31) * 255 / 31; }
  const chunk = (t, d) => { const c = Buffer.alloc(12 + d.length); c.writeUInt32BE(d.length); c.write(t, 4); d.copy(c, 8); c.writeUInt32BE(zlib.crc32(Buffer.concat([Buffer.from(t), d])) >>> 0, 8 + d.length); return c; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(`${outDir}/${name}.png`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
};
const e = instantiate(module).exports; e.vibe_wasm_init(48000);
const run = s => { for (let k = 0; k < 48000 * s; k += 128) { e.vibe_wasm_audio(128, 0); if (k % 2048 === 0) e.vibe_wasm_draw(); } };
run(3.5);
png("Display-Tour-1", new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(), 480, 320);
png("Stage-Tour-1", new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
e.vibe_wasm_event(8, 0, 1, 0, 0); run(2);
png("Display-Tour-2", new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(), 480, 320);
png("Stage-Tour-2", new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
console.log("ok");
