// Renders the 0.3 performance scene as a 48 kHz stereo WAV:
// Biome plays, FOREST + BLOOM are held and captured as a gesture, the hands leave,
// TONE pads answer, NEXT moves to the next Biome while the gesture keeps breathing,
// STARDUST is latched (0.6), ECHO is overdubbed into the gesture, PULSE joins on the beat,
// then GESTURE is tapped off and the dream settles.
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const [wasmPath, outPath = "demo.wav"] = process.argv.slice(2);
const rate = 48000, seconds = 80;
const {exports: e} = instantiate(new WebAssembly.Module(fs.readFileSync(wasmPath)));
e.vibe_wasm_init(rate);
const io = new Float32Array(e.memory.buffer, e.vibe_wasm_io(), 3 * 256);
const pad = (id, down, v = 0) => e.vibe_wasm_event(1, id, down, down ? v : 0, 0);
const cues = [
  [0.0, () => e.vibe_wasm_event(8, 0, 1, 0, 0)],
  [6.0, () => pad(15, 1, 1)], [6.05, () => pad(15, 0)],
  [6.3, () => pad(4, 1, .75)], [7.5, () => pad(8, 1, .9)],
  [13.0, () => pad(8, 0)], [15.5, () => pad(4, 0)],
  // 0.8: the ROOT pad as an arpeggio on the eighths, then back to single notes.
  [19.9, () => e.vibe_wasm_param_set(27, 2)],
  [20.0, () => pad(0, 1, .7)], [22.0, () => pad(0, 0)], [22.1, () => e.vibe_wasm_param_set(27, 0)],
  [23.0, () => pad(1, 1, .55)], [25.5, () => pad(1, 0)],
  [26.5, () => pad(3, 1, .8)], [29.0, () => pad(3, 0)],
  [31.0, () => pad(14, 1, 1)], [31.05, () => pad(14, 0)],
  // 0.6: SHIFT latches STARDUST, both hands free; SHIFT + 16 overdubs ECHO into the gesture.
  [36.0, () => e.vibe_wasm_event(3, 6, 1, 0, 0)], [36.05, () => pad(7, 1, .8)],
  [36.1, () => e.vibe_wasm_event(3, 6, 0, 0, 0)], [36.3, () => pad(7, 0)],
  [39.0, () => e.vibe_wasm_event(3, 6, 1, 0, 0)], [39.05, () => pad(15, 1, 1)], [39.1, () => pad(15, 0)],
  [39.15, () => e.vibe_wasm_event(3, 6, 0, 0, 0)],
  [39.3, () => pad(9, 1, .85)], [47.0, () => pad(9, 0)],
  [58.0, () => pad(7, 1, .8)], [58.1, () => pad(7, 0)],
  [44.0, () => pad(13, 1, .7)], [54.0, () => pad(13, 0)],
  [56.0, () => pad(6, 1, .6)], [62.0, () => pad(6, 0)],
  // 0.10: SHIFT + NEXT – the same Biome, a new variation.
  [52.0, () => e.vibe_wasm_event(3, 6, 1, 0, 0)], [52.05, () => pad(14, 1, 1)], [52.1, () => pad(14, 0)],
  [52.15, () => e.vibe_wasm_event(3, 6, 0, 0, 0)],
  [64.0, () => pad(15, 1, 1)], [64.05, () => pad(15, 0)],
];
cues.sort((a, b) => a[0] - b[0]);                    // cues fire in time order
const frames = rate * seconds, pcm = new Int16Array(frames * 2);
let position = 0, next = 0, peak = 0;
while (position < frames) {
  while (next < cues.length && cues[next][0] * rate <= position) cues[next++][1]();
  const n = Math.min(128, frames - position);
  e.vibe_wasm_audio(n, 0);
  for (let i = 0; i < n; i++) {
    const l = io[256 + i], r = io[512 + i];
    peak = Math.max(peak, Math.abs(l), Math.abs(r));
    pcm[2 * (position + i)] = Math.max(-1, Math.min(1, l)) * 32767;
    pcm[2 * (position + i) + 1] = Math.max(-1, Math.min(1, r)) * 32767;
  }
  position += n;
}
// Normalise the quiet engine to a -6 dBFS peak for listening; the gain is reported.
const gain = peak > 0 ? 0.5 / peak : 1;
for (let i = 0; i < pcm.length; i++) pcm[i] = Math.max(-32768, Math.min(32767, Math.round(pcm[i] * gain)));
const header = Buffer.alloc(44);
header.write("RIFF", 0); header.writeUInt32LE(36 + pcm.byteLength, 4); header.write("WAVEfmt ", 8);
header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(2, 22);
header.writeUInt32LE(rate, 24); header.writeUInt32LE(rate * 4, 28); header.writeUInt16LE(4, 32);
header.writeUInt16LE(16, 34); header.write("data", 36); header.writeUInt32LE(pcm.byteLength, 40);
fs.writeFileSync(outPath, Buffer.concat([header, Buffer.from(pcm.buffer)]));
console.log(JSON.stringify({outPath, seconds, rawPeak: +peak.toFixed(4), listeningGainDb: +(20 * Math.log10(gain)).toFixed(1)}));
