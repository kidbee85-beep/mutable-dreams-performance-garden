// Stage (SDK 23): declared size, drawing without NaN/crash, clicks act like the device's controls.
// Writes PNG snapshots of the stage when an output folder is given.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";
import zlib from "node:zlib";

const [wasmPath, outDir] = process.argv.slice(2);
const module = new WebAssembly.Module(fs.readFileSync(wasmPath));
const rate = 48000, W = 1280, H = 720;          // 0.18: drawn at 1280 x 720; clicks below in 960 x 540 layout units
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);
const STAGE = 19, DOWN = 0, UP = 2, HIDDEN = 9;

function session() {
  const {exports: e} = instantiate(module);
  e.vibe_wasm_init(rate);
  const info = new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_info(), 2);
  assert.equal(info[0], W); assert.equal(info[1], H);
  const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
  const out = [];
  const s = {
    e,
    send: (type, id, down, value, midi = 0) => e.vibe_wasm_event(type, id, down, value, midi),
    click: (x, y, pointer = 0) => { s.send(STAGE, DOWN, 1, at(x, y), pointer); },
    release: (pointer = 0) => s.send(STAGE, UP, 0, 0, pointer),
    run: seconds => {
      for (let n = 0; n < rate * seconds; n += 128) {
        e.vibe_wasm_audio(128, 0);
        for (let i = 0; i < 128; i++) {
          assert(Number.isFinite(io[256 + i]) && Number.isFinite(io[512 + i]));
          out.push(io[256 + i]);
        }
      }
    },
    stage: () => {
      const ptr = e.vibe_wasm_stage_draw();
      assert(ptr, "stage draws");
      return new Uint16Array(e.memory.buffer, ptr, W * H).slice();
    },
    out,
  };
  return s;
}
const pad = i => [700 + (i % 4) * 60 + 27, 72 + Math.floor(i / 4) * 60 + 10];
const chip = i => [700 + (i % 3) * 80 + 37, 330 + Math.floor(i / 3) * 46 + 20];
const sum = (a, b, from, to) => { let d = 0; for (let i = from * rate; i < to * rate; i++) d += Math.abs(a[i] - b[i]); return d; };

function png(pixels, path) {
  const raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) {
    raw[y * (W * 3 + 1)] = 0;
    for (let x = 0; x < W; x++) {
      const p = pixels[y * W + x], o = y * (W * 3 + 1) + 1 + x * 3;
      raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31;
    }
  }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(zlib.crc32(td) >>> 0);
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr),
    chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}

// 1. Clicking PLAY on the stage starts the dream like the device's PLAY.
const dry = session();
dry.click(757, 458); dry.release(); dry.run(6);
const silent = session(); silent.run(1);
assert(Math.max(...dry.out.slice(rate * 0.5, rate * 1).map(Math.abs)) > 0.001, "stage PLAY plays");
assert(Math.max(...silent.out.map(Math.abs)) === 0, "silent before PLAY");
const idle = dry.stage();

// 2. A held pad on the stage = the same pad on the device; release on pointer up.
const viaStage = session(), viaPad = session();
viaStage.click(757, 458); viaStage.release();
viaPad.click(757, 458); viaPad.release();
viaStage.run(.5); viaPad.run(.5);
viaStage.click(...pad(8)); viaPad.send(1, 8, 1, 1 - .7 * 10 / 54);
viaStage.run(2); viaPad.run(2);
viaStage.release(); viaPad.send(1, 8, 0, 0);
viaStage.run(3.5); viaPad.run(3.5);
assert(sum(viaStage.out, dry.out, 0.5, 2.5) > .05, "stage BLOOM changes the sound");
assert(sum(viaStage.out, viaPad.out, 0, 6) < 1e-6, "stage pad == device pad");

// 3. Biome chip F4 changes the dream; hiding the stage releases what the pointer held.
const biome = session();
biome.click(757, 458); biome.release();
biome.click(...chip(3)); biome.release();
biome.run(6);
assert(sum(biome.out, dry.out, 3, 6) > .05, "stage Biome chip changes the Biome");
const hide = session(), held = session();
hide.click(757, 458); hide.release(); held.click(757, 458); held.release();
hide.click(...pad(4)); held.click(...pad(4));
hide.run(1); held.run(1);
hide.send(STAGE, HIDDEN, 0, 0); hide.run(3); held.run(3);
assert(sum(hide.out, held.out, 2.5, 4) > .05, "hidden stage releases FOREST");

// 3b. 0.11: the DENSITY slider on the landscape follows a drag (MACRO_X = 562, top 162, height 150).
const drag = session();
drag.click(570, 300); drag.s = 0;
drag.send(STAGE, 1, 1, at(570, 170), 0);                     // MOVE up to the top
drag.release(); drag.run(.3);
const density = drag.e.vibe_wasm_param_value(4);
assert(density > 90, `stage slider sets DENSITY (${density})`);
drag.click(570, 290); drag.release(); drag.run(.3);
assert(drag.e.vibe_wasm_param_value(4) < 15, "and down again");

// 3c. 0.13: ON AIR on the stage starts Mutable Dreams FM; the snapshot shows it.
const air = session(); air.click(860, 458); air.release(); air.run(4);
let airPeak = 0; for (const x of air.out.slice(rate * 2)) airPeak = Math.max(airPeak, Math.abs(x));
assert(airPeak > .001, "stage ON AIR starts the programme");
if (outDir) { fs.mkdirSync(outDir, {recursive: true}); png(air.stage(), `${outDir}/Stage-On-Air.png`); }

// 4. The performance scene for the snapshot: gesture recorded with FOREST + BLOOM, then looping, with STARDUST and ECHO.
const scene = session();
scene.click(757, 458); scene.release();
scene.run(1);
scene.click(...pad(15)); scene.release();
scene.click(...pad(4), 1); scene.click(...pad(8), 2);
scene.run(2.5);
const recording = scene.stage();
scene.release(1); scene.release(2);
scene.run(16);                       // four bars at the Biome's 56 BPM end after ~17 s, then it loops
scene.click(...pad(7), 3); scene.click(...pad(9), 4); scene.click(...pad(6), 5);
scene.run(2);
const looping = scene.stage();
const differs = (a, b) => { let n = 0; for (let i = 0; i < a.length; i++) n += a[i] !== b[i]; return n; };
assert(differs(idle, looping) > 20000, "stage shows the performance");
if (outDir) {
  fs.mkdirSync(outDir, {recursive: true});
  png(idle, `${outDir}/Stage-ruhig.png`);
  png(recording, `${outDir}/Stage-Aufnahme.png`);
  png(looping, `${outDir}/Stage-Geste-Schleife.png`);
}
console.log("PASSED: stage 1280x720, PLAY, pads (== device pads), Biome chips, release on hide, drawing reacts.");
