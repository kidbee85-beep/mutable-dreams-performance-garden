// 0.16 All 47 pages from the stage and the Dream Console: the ALL PAGES chip, the mirrored display (clicks on pages
// and soft keys), colour groups, MENU, ENTER, DATA, GARDEN back to LIVE; the same through the editor commands 16-19.
import assert from "node:assert/strict";
import fs from "node:fs";
import {instantiate} from "./test_host.mjs";

const module = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const outDir = process.argv[3];
const rate = 48000, STAGE = 19, DOWN = 0, MOVE = 1, UP = 2, WHEEL = 3;
const e = instantiate(module).exports;
e.vibe_wasm_init(rate);
const run = sec => { for (let n = 0; n < rate * sec; n += 128) e.vibe_wasm_audio(128, 0); };
const at = (x, y) => Math.ceil(y * 4 / 3) * 4096 + Math.ceil(x * 4 / 3);   // layout units (960 x 540) -> stage pixels (1280 x 720)
const click = (x, y) => { e.vibe_wasm_event(STAGE, DOWN, 1, at(x, y), 0); e.vibe_wasm_event(STAGE, UP, 0, at(x, y), 0); run(.05); };
const send = (...b) => { new Uint8Array(e.memory.buffer, e.vibe_wasm_editor_inbox(), b.length).set(b); e.vibe_wasm_editor_data_in(b.length); run(.05); };
const take = () => { const box = e.vibe_wasm_editor_outbox(), dv = new DataView(e.memory.buffer), len = dv.getUint32(box, true);
  if (!len) return null; const b = new Uint8Array(e.memory.buffer, box + 4, len).slice(); dv.setUint32(box, 0, true); return b; };
const status = () => {
  let b = null; for (let i = 0; i < 12; i++) { e.vibe_wasm_draw(); b = take() || b; }
  const t = []; let k = 32;
  for (let i = 0; i < 10; i++) { t.push(Buffer.from(b.subarray(k + 1, k + 1 + b[k])).toString()); k += 1 + b[k]; }
  return {b, page: t[3], group: b[k], menu: !!(b[k + 1] & 1), help: !!(b[k + 1] & 2), onAir: b[2]};
};
const mirror = (dx, dy) => [16 + Math.ceil(dx * 1.5), 56 + Math.ceil(dy * 1.5)];   // display -> stage pixels
const steps = [];
run(.5);
let s = status();
assert.equal(s.page, "LIVE"); assert(!s.menu);

click(790, 26);                                          // PAGES
s = status(); assert(s.menu, "ALL PAGES opens the directory"); steps.push("chip -> MENU");
// the mirror is the display, 1.5 x
e.vibe_wasm_draw();
const dev = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice();
const st = new Uint16Array(e.memory.buffer, e.vibe_wasm_stage_draw(), 1280 * 720).slice();
let same = 0;
for (let i = 0; i < 400; i++) { const x = (i * 37) % 480, y = (i * 53) % 320, px = 21 + 2 * x, py = 75 + 2 * y; if (st[py * 1280 + px] === dev[y * 480 + x] && st[(py + 1) * 1280 + px + 1] === dev[y * 480 + x]) same++; }
assert(same >= 390, `stage mirrors the display (${same}/400)`);
if (outDir) fs.writeFileSync(`${outDir}/stage-pages.raw`, Buffer.from(st.buffer));

click(...mirror(240, 305)); click(...mirror(12 + 40, 60 + 3 * 20 + 7));   // (0.27) footer: bank 2/2, then RADIO (39 = bank 2, column 1, row 4)
s = status(); assert.equal(s.page, "RADIO", "a click on a page opens it"); steps.push("click page -> RADIO");
click(852 + 10, 78 + 3 * 36 + 10); click(...mirror(12 + 156 + 40, 60 + 0 * 20 + 7)); s = status(); assert.equal(s.page, "MIDI MAP", "the last page is reachable too");
click(852 + 10, 78 + 3 * 36 + 10); click(...mirror(12 + 40, 60 + 3 * 20 + 7)); s = status(); assert.equal(s.page, "RADIO");
click(...mirror(40, 300));                               // F1 on the display: ON AIR
s = status(); assert.equal(s.onAir, 1, "F1 on the mirrored display"); steps.push("F1 -> ON AIR");
click(...mirror(200, 300)); s = status(); assert.equal(s.onAir, 0, "F3 OFF AIR");

click(852 + 10, 78 + 3 * 36 + 10);                       // MENU chip
s = status(); assert(s.menu);
const before = s.page;
e.vibe_wasm_event(STAGE, WHEEL, 0, 2, 0); run(.05);      // DATA x2
click(860, 266 + 10);                                    // ENTER (right of SHIFT)
s = status(); assert(!s.menu && s.page !== before, `wheel + ENTER opens another page (${s.page})`); steps.push("wheel+ENTER -> " + s.page);

click(752 + 100 + 10, 78 + 2 * 36 + 10);                 // SEQ (K6)
s = status(); assert.equal(s.group, 5, "SEQ group"); steps.push("SEQ -> " + s.page);
click(852 + 10, 78 + 3 * 36 + 10);          // (MENU again closes nothing yet: it toggles)
s = status(); assert(s.menu); click(852 + 10, 78 + 3 * 36 + 10); s = status(); assert(!s.menu);
click(752 + 10, 228 + 10);                               // HOME
s = status(); assert(s.help, "HOME shows the controls"); click(752 + 10, 228 + 10); s = status(); assert(!s.help);
// encoder 1 drag: on SEQ pages the first value changes the picture
e.vibe_wasm_draw(); const a = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice();
e.vibe_wasm_event(STAGE, DOWN, 1, at(776, 340), 0);
for (let y = 336; y >= 260; y -= 4) { e.vibe_wasm_event(STAGE, MOVE, 1, at(776, y), 0); run(.02); }
e.vibe_wasm_event(STAGE, UP, 0, at(776, 260), 0); run(.2);
e.vibe_wasm_draw(); const b2 = new Uint16Array(e.memory.buffer, e.vibe_wasm_draw(), 480 * 320).slice();
let diff = 0; for (let i = 0; i < a.length; i++) if (a[i] !== b2[i]) diff++;
assert(diff > 50, "dragging encoder 1 changes the page"); steps.push("encoder 1 drag changes " + diff + " px");

click(660, 26);                                          // GARDEN
s = status(); assert.equal(s.page, "LIVE"); assert(!s.menu); steps.push("GARDEN -> LIVE");

// the same through the Dream Console
send(17, 6); s = status(); assert.equal(s.group, 6, "DREAM group from the page"); steps.push("console DREAM -> " + s.page);
send(16, 19); s = status(); assert(s.menu, "MENU from the page");
if (s.page !== "VOICE") send(19, 120, 152); send(19, (12 + 40) / 2 | 0, (60 + 2 * 20 + 7) / 2 | 0); s = status(); assert.equal(s.page, "VOICE", "display click from the page");
send(18, 6, 255); send(16, 19); send(18, 6, 1); send(16, 9); s = status(); steps.push("console DATA/ENTER -> " + s.page);
send(16, 16); s = status(); assert.equal(s.page, "LIVE", "RETURN from the page");
for (const bad of [[16, 99], [16, 6], [17, 9], [18, 40, 3], [19, 250, 250], [19]]) send(...bad);
run(.5);
console.log(JSON.stringify({passed: true, steps}, null, 1));
