// 0.27: the pictures of the guides, in the current look (Dokumentation/guide-img and Dokumentation/img).
import fs from "node:fs";
import zlib from "node:zlib";
import {instantiate} from "./test_host.mjs";
const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const [gdir, idir] = [process.argv[3], process.argv[4]];
const png = (path, px, w, h) => {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) { const v = px[y * w + x], o = y * (w * 3 + 1) + 1 + x * 3;
    raw[o] = (v >> 11 & 31) * 255 / 31; raw[o + 1] = (v >> 5 & 63) * 255 / 63; raw[o + 2] = (v & 31) * 255 / 31; }
  const chunk = (t, d) => { const c = Buffer.alloc(12 + d.length); c.writeUInt32BE(d.length); c.write(t, 4); d.copy(c, 8); c.writeUInt32BE(zlib.crc32(Buffer.concat([Buffer.from(t), d])) >>> 0, 8 + d.length); return c; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
};
const fresh = () => { const e = instantiate(module).exports; e.vibe_wasm_init(48000);
  const press = id => { e.vibe_wasm_event(3, id, 1, 0, 0); e.vibe_wasm_event(3, id, 0, 0, 0); };
  const open = v => { press(19); e.vibe_wasm_event(4, 0, 0, -60, 0); e.vibe_wasm_event(4, 0, 0, v, 0); press(9); };
  const run = s => { for (let k = 0; k < 48000 * s; k += 128) { e.vibe_wasm_audio(128, 0); if (k % 2048 === 0) e.vibe_wasm_draw(); } };
  const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
  const click = (x, y) => { e.vibe_wasm_event(19, 0, 1, at(x, y), 0); e.vibe_wasm_event(19, 2, 0, at(x, y), 0); };
  const disp = p => png(p, new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice(), 480, 320);
  const stage = p => png(p, new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice(), 1280, 720);
  press(17); press(17); e.vibe_wasm_event(8, 0, 1, 0, 0); run(3);
  return {e, press, open, run, click, disp, stage}; };
let s = fresh();
s.e.vibe_wasm_event(1, 4, 1, .7, 0); s.run(2); s.disp(`${gdir}/Display-LIVE.png`); s.stage(`${gdir}/Stage-ruhig.png`); s.e.vibe_wasm_event(1, 4, 0, 0, 0);
const pages = {BROWSER: 1, SPACE: 21, GRAIN: 22, ITINERA: 40, JOURNEY: 41};
for (const [n, v] of Object.entries(pages)) { s.open(v); s.run(.8); s.disp(`${gdir}/Display-${n}.png`); }
s.open(1); s.press(4); s.run(.5); s.disp(`${gdir}/Display-WORLDS.png`); s.press(4);
s.open(20); s.run(.5); s.disp(`${gdir}/Display-Momente.png`); s.disp(`${idir}/Display-Momente.png`); s.disp(`${idir}/Display-FX-Pad-Ebene.png`);
// the old page pictures by their 0.23 numbers (pages from RHYTHM on moved by 2)
for (const old of [2, 3, 6, 8, 12, 16, 17, 22, 26, 31, 35, 36]) { const v = old >= 25 ? old + 2 : old; s.open(v); s.run(.6); s.disp(`${idir}/p${String(old).padStart(2, "0")}.png`); }
s.open(0); s.press(19); s.run(.3); s.disp(`${idir}/p00-MENU.png`); s.press(19);
s.open(0); s.e.vibe_wasm_event(4, 4, 0, -3, 0); s.run(.2); s.disp(`${idir}/Display-Encoder-A.png`);
s.open(39); s.press(0); s.run(1.5); s.disp(`${idir}/Display-Radio.png`); s.open(0); s.run(.5); s.stage(`${gdir}/Stage-On-Air.png`); s.stage(`${idir}/Stage-On-Air.png`);
s = fresh();
s.e.vibe_wasm_event(1, 15, 1, 1, 0); s.e.vibe_wasm_event(1, 15, 0, 0, 0); s.run(.3); s.e.vibe_wasm_event(1, 4, 1, 1, 0); s.e.vibe_wasm_event(1, 8, 1, .8, 0); s.run(2.2);
s.e.vibe_wasm_event(1, 4, 0, 0, 0); s.e.vibe_wasm_event(1, 8, 0, 0, 0); s.run(18); s.stage(`${gdir}/Stage-Geste-Schleife.png`); s.stage(`${idir}/Stage-Geste-Schleife.png`);
// (Stage-Aufnahme needs a media host: the earlier picture stays)
console.log("ok");
