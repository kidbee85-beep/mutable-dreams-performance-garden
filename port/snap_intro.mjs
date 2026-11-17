// The intro at 1.6 s on the display and the stage, as PNG.
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";
const [wasmPath, outDir] = process.argv.slice(2);
const e = instantiate(new WebAssembly.Module(fs.readFileSync(wasmPath))).exports;
e.vibe_wasm_init(48000);
for (let n = 0; n < 48000 * 1.6; n += 128) e.vibe_wasm_audio(128, 0);
function png(px, W, H, path) {
  const raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) { const p = px[y * W + x], o = y * (W * 3 + 1) + 1 + x * 3;
    raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31; }
  const chunk = (t, d) => { const l = Buffer.alloc(4); l.writeUInt32BE(d.length); const td = Buffer.concat([Buffer.from(t), d]); const c = Buffer.alloc(4); c.writeUInt32BE(zlib.crc32(td) >>> 0); return Buffer.concat([l, td, c]); };
  const h = Buffer.alloc(13); h.writeUInt32BE(W, 0); h.writeUInt32BE(H, 4); h[8] = 8; h[9] = 2;
  fs.writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", h), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}
fs.mkdirSync(outDir, {recursive: true});
png(new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(), 480, 320, `${outDir}/Intro-Display.png`);
png(new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720, `${outDir}/Intro-Stage.png`);
console.log("intro snapshots in", outDir);
