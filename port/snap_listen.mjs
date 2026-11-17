// 0.24: pictures of LISTEN, SING, CV GATE and the stage's PLACES with the moon pond and the singing flower.
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";
const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const rate = 48000, BUTTON = 3, ENCODER = 4, MENU = 19, ENTER = 9, F1 = 0, STAGE = 19;
const png = (name, px, w, h) => {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) { const v = px[y * w + x], o = y * (w * 3 + 1) + 1 + x * 3;
    raw[o] = (v >> 11 & 31) * 255 / 31; raw[o + 1] = (v >> 5 & 63) * 255 / 63; raw[o + 2] = (v & 31) * 255 / 31; }
  const chunk = (t, d) => { const c = Buffer.alloc(12 + d.length); c.writeUInt32BE(d.length); c.write(t, 4); d.copy(c, 8); c.writeUInt32BE(zlib.crc32(Buffer.concat([Buffer.from(t), d])) >>> 0, 8 + d.length); return c; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(`${outDir}/${name}.png`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
};
const e = instantiate(module).exports; e.vibe_wasm_init(rate);
const press = id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
const open = v => { press(MENU); e.vibe_wasm_event(ENCODER, 0, 0, -60, 0); e.vibe_wasm_event(ENCODER, 0, 0, v, 0); press(ENTER); };
let t = 0;
const voice = n => tt => { const f = 440 * 2 ** ((n - 69) / 12); let v = 0; for (let h = 1; h <= 5; h++) v += Math.sin(2 * Math.PI * f * h * tt) / h; return .3 * v; };
const run = (sec, src) => { for (let k = 0; k < rate * sec; k += 128) { const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 1024);
  if (src) for (let i = 0; i < 128; i++) { io[i] = io[768 + i] = src(t + i / rate); } e.vibe_wasm_audio(128, src ? 3 : 0); t += 128 / rate; if (k % 2048 === 0) e.vibe_wasm_draw(); } };
const display = name => png(name, new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(), 480, 320);
const stage = name => png(name, new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
press(17); press(17);                                     // (0.26) close FIRST STEPS (HOME ends it, HOME again closes the help)
e.vibe_wasm_event(8, 0, 1, 0, 0);
run(2);
open(25); press(F1); run(3, voice(57)); display("Display-Listen");
open(26); press(F1 + 3); run(1.2, voice(64)); display("Display-Sing");
run(.6, null); run(.6, null);
open(49); e.vibe_wasm_event(ENCODER, 0, 0, 1, 0); run(2); display("Display-CV");
open(0); run(1.5, voice(67)); display("Display-Live-Regen");
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
e.vibe_wasm_event(STAGE, 0, 1, at(728, 26), 0); e.vibe_wasm_event(STAGE, 2, 0, at(728, 26), 0);
run(1.5, voice(62)); stage("Stage-Orte-024");
e.vibe_wasm_event(STAGE, 0, 1, at(660, 26), 0); e.vibe_wasm_event(STAGE, 2, 0, at(660, 26), 0);
run(1.5, voice(69)); stage("Stage-Garten-Regen");
open(0); press(MENU); run(.1); display("Display-Menue-50");
e.vibe_wasm_event(STAGE, 0, 1, at(790, 26), 0); e.vibe_wasm_event(STAGE, 2, 0, at(790, 26), 0); run(.3); stage("Stage-Alle-Seiten");
console.log("ok");
