// 0.31: the DAW's automation hangs on the parameter ids – they may only ever be added, never renamed, removed or moved
// in meaning. tests-data/param-ids.txt is the list as of 0.31 (id min max default steps); a new id may follow at the end.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";
const e = instantiate(new WebAssembly.Module(fs.readFileSync(process.argv[2]))).exports;
e.vibe_wasm_init(48000);
const cstr = p => { const b = new Uint8Array(e.memory.buffer, p); let n = 0; while (b[n]) n++; return Buffer.from(b.subarray(0, n)).toString(); };
const now = [];
for (let i = 0; i < e.vibe_wasm_param_count(); i++) {
  // vibe_param: id, name, min, max, default, unit, steps, choices – the contract is the id with its range, default and steps
  const p = e.vibe_wasm_param_info(i), u = new Uint32Array(e.memory.buffer, p, 8), f = new Float32Array(e.memory.buffer, p, 8);
  const num = x => +x.toFixed(4);
  now.push([cstr(u[0]), num(f[2]), num(f[3]), num(f[4]), u[6]].join(" "));
}
const file = new URL("./tests-data/param-ids.txt", import.meta.url);
if (process.argv[3] === "--write") { fs.writeFileSync(file, now.join("\n") + "\n"); console.log(`written: ${now.length} ids`); process.exit(0); }
const base = fs.readFileSync(file, "utf8").trim().split("\n");
assert.deepEqual(now.slice(0, base.length), base, "the parameter ids of earlier versions must stay, in their order");
assert.equal(new Set(now.map(l => l.split(" ")[0])).size, now.length, "every parameter id is unique");
console.log(JSON.stringify({ids: now.length, baseline: base.length, added: now.slice(base.length)}));
console.log(`PASSED: ${base.length} parameter ids as before (DAW automation and saved projects keep working).`);
