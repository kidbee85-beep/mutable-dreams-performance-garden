// One display image per page of the 2.1 instrument, reached the way a player would: MENU, encoder 1, ENTER.
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";

const [wasmPath, outDir] = process.argv.slice(2);
const module = new WebAssembly.Module(fs.readFileSync(wasmPath));
const W = 480, H = 320, BUTTON = 3, ENCODER = 4, MENU = 19, ENTER = 9;
function png(px, path) {
  const raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
    const p = px[y * W + x], o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31;
  }
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]);
    const c = Buffer.alloc(4); c.writeUInt32BE(zlib.crc32(td) >>> 0); return Buffer.concat([l, td, c]); };
  const h = Buffer.alloc(13); h.writeUInt32BE(W, 0); h.writeUInt32BE(H, 4); h[8] = 8; h[9] = 2;
  fs.writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", h),
    chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
fs.mkdirSync(outDir, {recursive: true});
const names = [];
for (let v = 0; v < 47; v++) {
  const e = instantiate(module).exports;
  e.vibe_wasm_init(48000);
  e.vibe_wasm_event(8, 0, 1, 0, 0);
  const run = s => { for (let n = 0; n < 48000 * s; n += 128) e.vibe_wasm_audio(128, 0); };
  run(1.5);
  e.vibe_wasm_event(BUTTON, MENU, 1, 0, 0); e.vibe_wasm_event(BUTTON, MENU, 0, 0, 0);
  if (v === 46) png(new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), W * H).slice(), `${outDir}/00-MENU.png`);
  e.vibe_wasm_event(ENCODER, 0, 0, v, 0);
  e.vibe_wasm_event(BUTTON, ENTER, 1, 0, 0); e.vibe_wasm_event(BUTTON, ENTER, 0, 0, 0);
  run(.4);
  const name = `${String(v + 1).padStart(2, "0")}`;
  png(new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), W * H).slice(), `${outDir}/${name}.png`);
  names.push(name);
}
console.log(names.length, "pages in", outDir);
