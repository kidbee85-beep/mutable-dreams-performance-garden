// Display snapshots (480 x 320) of the 0.5 surface: FX pad layer, encoder A hint, gesture waiting for the bar.
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";

const [wasmPath, outDir] = process.argv.slice(2);
const module = new WebAssembly.Module(fs.readFileSync(wasmPath));
const W = 480, H = 320, BUTTON = 3, ENCODER = 4, PAD = 1, TRANSPORT = 8, PARAM = 10;
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
function shot(name, script) {
  const e = instantiate(module).exports;
  e.vibe_wasm_init(48000);
  e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
  const run = s => { for (let n = 0; n < 48000 * s; n += 128) e.vibe_wasm_audio(128, 0); };
  script(e, run);
  png(new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), W * H).slice(), `${outDir}/${name}.png`);
}
fs.mkdirSync(outDir, {recursive: true});
shot("Display-FX-Pad-Ebene", (e, run) => { e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_event(BUTTON, 14, 0, 0, 0);
  e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_event(BUTTON, 14, 0, 0, 0); run(.5); });
shot("Display-Encoder-A", (e, run) => { e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); run(.2); e.vibe_wasm_event(ENCODER, 4, 0, -15, 0);
  e.vibe_wasm_event(PAD, 4, 1, 1, 0); run(.5); });
shot("Display-Geste-wartet", (e, run) => { e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_param_set(13, 1); run(.3);
  e.vibe_wasm_event(PAD, 15, 1, 1, 0); e.vibe_wasm_event(PAD, 15, 0, 0, 0); run(.05); });
shot("Display-Latch-und-Overdub", (e, run) => {
  e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_event(PARAM, 0, 0, 0, 0);
  e.vibe_wasm_param_set(1, 120); run(.2);
  e.vibe_wasm_event(PAD, 15, 1, 1, 0); e.vibe_wasm_event(PAD, 15, 0, 0, 0);
  e.vibe_wasm_event(PAD, 4, 1, 1, 0); run(1); e.vibe_wasm_event(PAD, 4, 0, 0, 0);
  e.vibe_wasm_event(PAD, 15, 1, 1, 0); e.vibe_wasm_event(PAD, 15, 0, 0, 0); run(.3);
  e.vibe_wasm_event(BUTTON, 6, 1, 0, 0);
  e.vibe_wasm_event(PAD, 6, 1, .9, 0); e.vibe_wasm_event(PAD, 6, 0, 0, 0);
  e.vibe_wasm_event(PAD, 9, 1, .9, 0); e.vibe_wasm_event(PAD, 9, 0, 0, 0);
  e.vibe_wasm_event(PAD, 15, 1, 1, 0); e.vibe_wasm_event(PAD, 15, 0, 0, 0);
  run(.4);
});
shot("Display-Momente", (e, run) => {
  const k5 = () => { e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_event(BUTTON, 14, 0, 0, 0); };
  k5(); k5();
  e.vibe_wasm_event(PAD, 4, 1, 1, 0); e.vibe_wasm_event(PAD, 9, 1, .8, 0); run(1);
  for (const f of [0, 2]) { e.vibe_wasm_event(BUTTON, 6, 1, 0, 0); e.vibe_wasm_event(BUTTON, f, 1, 0, 0);
    e.vibe_wasm_event(BUTTON, f, 0, 0, 0); e.vibe_wasm_event(BUTTON, 6, 0, 0, 0); }
  e.vibe_wasm_event(PAD, 4, 0, 0, 0); e.vibe_wasm_event(PAD, 9, 0, 0, 0);
  e.vibe_wasm_event(BUTTON, 2, 1, 0, 0); e.vibe_wasm_event(BUTTON, 2, 0, 0, 0); run(1.5);
});
shot("Display-Ton-Modus-ARP", (e, run) => {
  const k5 = () => { e.vibe_wasm_event(BUTTON, 14, 1, 0, 0); e.vibe_wasm_event(BUTTON, 14, 0, 0, 0); };
  k5(); k5();
  e.vibe_wasm_event(BUTTON, 6, 1, 0, 0);
  e.vibe_wasm_event(ENCODER, 0, 0, 1, 0); e.vibe_wasm_event(ENCODER, 0, 0, 1, 0);
  e.vibe_wasm_event(ENCODER, 1, 0, 1, 0); e.vibe_wasm_event(ENCODER, 1, 0, 1, 0);
  e.vibe_wasm_event(PAD, 0, 1, .9, 0); run(1);
});
shot("Display-Radio", (e, run) => {
  e.vibe_wasm_event(BUTTON, 19, 1, 0, 0); e.vibe_wasm_event(BUTTON, 19, 0, 0, 0);
  e.vibe_wasm_event(ENCODER, 0, 0, -60, 0); e.vibe_wasm_event(ENCODER, 0, 0, 37, 0);
  e.vibe_wasm_event(BUTTON, 9, 1, 0, 0); e.vibe_wasm_event(BUTTON, 9, 0, 0, 0);
  e.vibe_wasm_event(BUTTON, 0, 1, 0, 0); e.vibe_wasm_event(BUTTON, 0, 0, 0, 0);
  run(6);
});
console.log("snapshots in", outDir);
