// 0.27: a contact sheet of the device's pages (4 x 4 at 480 x 320) and the stage's three views, for the design pass.
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
const press = id => { e.vibe_wasm_event(3, id, 1, 0, 0); e.vibe_wasm_event(3, id, 0, 0, 0); };
const open = v => { press(19); e.vibe_wasm_event(4, 0, 0, -60, 0); e.vibe_wasm_event(4, 0, 0, v, 0); press(9); };
const run = s => { for (let k = 0; k < 48000 * s; k += 128) { e.vibe_wasm_audio(128, 0); if (k % 2048 === 0) e.vibe_wasm_draw(); } };
press(17); press(17);                                       // FIRST STEPS closed
e.vibe_wasm_event(8, 0, 1, 0, 0); e.vibe_wasm_event(1, 8, 1, .8, 0); e.vibe_wasm_event(1, 4, 1, .6, 0); run(3);
const pages = [0, 1, 2, 3, 5, 6, 7, 15, 21, 22, 23, 27, 30, 39, 45, -1];
const W = 480, H = 320, sheet = new Uint16Array(W * 4 * H * 4);
pages.forEach((v, i) => {
  if (v < 0) { open(0); press(19); } else open(v);
  run(.6);
  const f = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), W * H);
  const ox = (i % 4) * W, oy = Math.floor(i / 4) * H;
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) sheet[(oy + y) * W * 4 + ox + x] = f[y * W + x];
  if (v < 0) press(19);
});
png("Sheet-Device", sheet, W * 4, H * 4);
open(0); run(.3);
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
const click = (x, y) => { e.vibe_wasm_event(19, 0, 1, at(x, y), 0); e.vibe_wasm_event(19, 2, 0, at(x, y), 0); };
png("Stage-Garden", new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
click(728, 26); run(.5); png("Stage-Places", new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
click(790, 26); run(.3); press(19); run(.3); open(6); run(.3); png("Stage-Pages", new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
console.log("ok");
