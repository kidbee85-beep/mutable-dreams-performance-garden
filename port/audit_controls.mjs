// 0.31: every control does something. On each of the 50 pages: each of encoders 1-4 that shows a value changes it
// (one way or the other), and each F key with a caption changes what the page shows or says. Compared are the
// page's still parts: header, tabs, value cells and soft keys (the animated picture in between is left out).
// usage: node test_controls.mjs <wasm> [out.json]
import assert from "node:assert/strict";
import fs from "node:fs";
import crypto from "node:crypto";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const BUTTON = 3, ENCODER = 4, MENU = 19, ENTER = 9, HOME = 17, F1 = 0, PAGES = 50;
const e = instantiate(module, new Map()).exports;
e.vibe_wasm_init(48000);
const press = id => { e.vibe_wasm_event(BUTTON, id, 1, 0, 0); e.vibe_wasm_event(BUTTON, id, 0, 0, 0); };
const turn = (k, v) => e.vibe_wasm_event(ENCODER, k, 0, v, 0);
const run = s => { for (let k = 0; k < 48000 * s; k += 128) e.vibe_wasm_audio(128, 0); };
const open = v => { press(MENU); turn(0, -60); turn(0, v); press(ENTER); run(.05); };
// a picture of the rows; two pictures differ when more than 40 pixels changed (a twinkling star changes a few)
const look = (rows) => {
  const px = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320), out = [];
  for (const [a, b] of rows) out.push(px.slice(a * 480, b * 480));
  return out;
};
const differ = (x, y) => { let n = 0; for (let i = 0; i < x.length; i++) for (let j = 0; j < x[i].length; j++) if (x[i][j] !== y[i][j]) n++; return n > 40; };
const STILL = [[0, 98], [278, 320]];          // header, tabs, value cells, soft keys
press(HOME); press(HOME); run(.2);              // the tour away
const R = {pages: [], dead: []};
for (let p = 0; p < PAGES; p++) {
  open(p); run(.1);
  const name = (() => { look(STILL); return p; })();
  const row = {page: p, encoders: [], keys: []};
  for (let k = 0; k < 4; k++) {
    const before = look([[58, 98]]);
    turn(k, 3); run(.05); let after = look([[58, 98]]);
    let moved = differ(after, before);
    if (!moved) { turn(k, -6); run(.05); after = look([[58, 98]]); moved = differ(after, before); turn(k, 3); }
    else turn(k, -3);
    run(.05);
    row.encoders.push(moved ? 1 : 0);
  }
  for (let f = 0; f < 6; f++) {
    open(p); run(.1);
    const before = look(STILL); run(.15);
    if (differ(look(STILL), before)) { row.keys.push("~"); continue; }     // the still parts move by themselves: no verdict
    press(F1 + f); run(.15);
    const after = look(STILL);
    row.keys.push(differ(after, before) ? 1 : 0);
  }
  R.pages.push(row);
}
fs.writeFileSync(process.argv[3] || "/tmp/test_controls.json", JSON.stringify(R));
console.log(JSON.stringify(R.pages.map(r => `${r.page}:E${r.encoders.join("")} F${r.keys.join("")}`)));
