// 0.11 REC: a take is captured from the finished mix and written through the host's media as a valid WAV,
// early stop included, while the audio keeps running. Uses the SDK's own media host (pending answers included).
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";
import {MediaHost} from "./official/tools/media-host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const rate = 48000, BUTTON = 3, REC = 21, TRANSPORT = 8, MEDIA = 12;
const media = new MediaHost({fw: "mutable-dreams-garden", pending: 2, sources: {sd: {files: {}, write: true, label: "VIBE"}}});
const e = instantiate(module, new Map(), null, media).exports;
e.vibe_wasm_init(rate);
const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
const live = [];
const step = sec => {
  for (let n = 0; n < rate * sec; n += 128) {
    e.vibe_wasm_audio(128, 0);
    for (let i = 0; i < 128; i++) { assert(Number.isFinite(io[256 + i])); live.push(io[256 + i]); }
    for (const ev of media.events()) e.vibe_wasm_event(MEDIA, ev.id, ev.down, ev.value, 0);
    if (n % (128 * 12) === 0) e.vibe_wasm_draw();                 // about 30 frames a second
  }
};
e.vibe_wasm_event(TRANSPORT, 0, 1, 0, 0);
step(.5);
e.vibe_wasm_event(BUTTON, REC, 1, 0, 0); e.vibe_wasm_event(BUTTON, REC, 0, 0, 0);
e.vibe_wasm_event(1, 4, 1, .9, 0);                                 // FOREST in the take
step(4);
e.vibe_wasm_event(1, 4, 0, 0, 0);
e.vibe_wasm_event(BUTTON, REC, 1, 0, 0); e.vibe_wasm_event(BUTTON, REC, 0, 0, 0);   // stop early
step(2);
const written = media.count.written;
assert.equal(written.length, 1, "one take written");
const file = media.src.get("sd").files.get(written[0].path.replace(/^sd\//, ""));
assert(file, "the take is in VIBE FM/mutable-dreams-garden/");
assert.equal(file.toString("latin1", 0, 4), "RIFF"); assert.equal(file.toString("latin1", 8, 12), "WAVE");
const channels = file.readUInt16LE(22), sr = file.readUInt32LE(24), bits = file.readUInt16LE(34), data = file.readUInt32LE(40);
assert.equal(channels, 2); assert.equal(sr, 44100); assert.equal(bits, 16);
assert.equal(data + 44, file.length, "header matches the data");
assert.equal(file.readUInt32LE(4), file.length - 8);
const seconds = data / 4 / 44100;
assert(seconds > 3.8 && seconds < 4.3, `early stop keeps what was played (${seconds} s)`);
let peak = 0; for (let i = 44; i < file.length; i += 2) peak = Math.max(peak, Math.abs(file.readInt16LE(i)));
assert(peak > 200, "the take holds sound");
if (outDir) { fs.mkdirSync(outDir, {recursive: true}); fs.writeFileSync(`${outDir}/REC-Testtake.wav`, file); }
// A second take gets its own name.
e.vibe_wasm_event(BUTTON, REC, 1, 0, 0); e.vibe_wasm_event(BUTTON, REC, 0, 0, 0);
step(1.2);
e.vibe_wasm_event(BUTTON, REC, 1, 0, 0); e.vibe_wasm_event(BUTTON, REC, 0, 0, 0);
step(1.5);
assert.equal(media.count.written.length, 2);
assert.notEqual(media.count.written[0].path, media.count.written[1].path);
console.log(JSON.stringify({takes: media.count.written, seconds: +seconds.toFixed(2), peak, pendingAnswers: media.count.pending}, null, 2));
console.log("PASSED: REC writes a valid 44.1 kHz stereo WAV through the host, early stop, second take named apart.");
