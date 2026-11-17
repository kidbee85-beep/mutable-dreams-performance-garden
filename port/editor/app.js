// Mutable Dreams · Dream Console – the plugin's own Editor page (SDK 16).
// Everything goes through window.vibe: parameters with vibe.set (the DAW records them), the rest as small
// commands with vibe.send; the firmware answers with a status packet (garden-editor.inc). The page keeps no
// state of its own: what it shows comes from onValues and onData.
(async () => {
  await vibe.ready;
  const $ = (id) => document.getElementById(id);
  const send = (...bytes) => vibe.send(new Uint8Array(bytes));
  let listenOn = false;                                   // (0.24) the garden listens
  setTimeout(() => {
    const lin = document.getElementById('lin'); if (lin) lin.onclick = () => send(26, listenOn ? 0 : 1);
    const tb = document.getElementById('tour'); if (tb) tb.onclick = () => send(28);              // (0.26) FIRST STEPS
    const te = document.getElementById('coachend'); if (te) te.onclick = () => send(28);
    document.querySelectorAll('#sing button').forEach((x) => { x.onclick = () => send(27, Number(x.dataset.m)); });
  }, 0);
  const P = (id) => vibe.byId(id);
  const norm = (p, v) => (v - p.min) / (p.max - p.min);
  const plain = (p, f) => p.min + Math.min(1, Math.max(0, f)) * (p.max - p.min);

  // ---- 0.19: the night garden behind the page and the Biome signs (Visual Kit of the Brick, kit.js) --------------
  const MK = window.MDKit;
  const kit = MK ? MK.start($('garden'), {scene: 'garden', lower: '0', biome: 0, maxDpr: 1.25}) : null;
  const sigilOf = (canvas, id, t) => {                 // one Biome's sign into a small canvas (drawn at 2 x)
    if (!MK) return;
    const g = canvas.getContext('2d'), w = canvas.width, h = canvas.height;
    g.setTransform(1, 0, 0, 1, 0, 0); g.clearRect(0, 0, w, h);
    g.fillStyle = 'rgba(10,14,24,.75)'; g.beginPath(); g.arc(w / 2, h / 2, w * .48, 0, 6.283); g.fill();
    MK.sigil(g, id, w / 2, h / 2, w * .3, MK.TRACK[((id % 6) + 6) % 6], t);
  };
  let nowBiome = 0;
  (function spin(ts) { sigilOf($('sigil'), nowBiome, ts / 1000); sigilOf($('nowsigil'), nowBiome, ts / 1400); requestAnimationFrame(spin); })(0);

  // ---- transport, radio, REC, Biome ----------------------------------------------------------------------
  $('play').onclick = () => send(1);
  $('stop').onclick = () => send(2);
  $('air').onclick = () => send(3);
  $('rec').onclick = () => send(8);
  $('next').onclick = () => send(5);
  $('vary').onclick = () => send(6);
  $('undo').onclick = () => send(7);
  $('grec').onclick = () => send(9);
  $('gdub').onclick = () => send(14);

  // ---- the device display and its pages (0.16) -------------------------------------------------------------
  const display = $('display');
  vibe.onFrame((fb) => vibe.paint(display, fb));
  display.onclick = (e) => {                                  // F1-F6 at its bottom, a page in ALL PAGES
    const r = display.getBoundingClientRect();
    send(19, Math.floor((e.clientX - r.left) / r.width * 240), Math.floor((e.clientY - r.top) / r.height * 160));
  };
  display.onwheel = (e) => { e.preventDefault(); send(18, 6, e.deltaY > 0 ? 1 : 255); };   // DATA
  const groups = [['SOUND', '--mint'], ['ENV', '--gold'], ['LFO', '--tex'], ['HARMONY', '--space'], ['FX', '--space'], ['SEQ', '--gold'], ['DREAM', '--form']];
  const groupEls = groups.map(([name, color], g) => {
    const b = document.createElement('button'); b.className = 'small'; b.textContent = name; b.style.setProperty('--g', `var(${color})`);
    b.title = g < 6 ? `K${g + 1}: the ${name} pages – again: the next one` : 'MENU › DREAM: radio, Itinera, journey, banks, system, MIDI';
    b.onclick = () => send(17, g); $('groups').append(b); return b;
  });
  const menu = document.createElement('button'); menu.className = 'small'; menu.textContent = 'MENU'; menu.style.setProperty('--g', 'var(--ink)');
  menu.title = 'MENU: all 50 pages – click one on the display'; menu.onclick = () => send(16, 19); $('groups').append(menu);
  for (const b of document.querySelectorAll('[data-btn]')) b.onclick = () => send(16, Number(b.dataset.btn));
  $('shift').onclick = () => send(16, 6);          // VB_SHIFT: for the next control
  [['1', 0], ['2', 1], ['3', 2], ['4', 3], ['A', 4], ['B', 5], ['DATA', 6]].forEach(([name, id]) => {
    const el = document.createElement('div'); el.className = 'enc';
    el.innerHTML = `<span>${name === 'DATA' ? 'D' : name}</span><b>${name}</b>`;
    el.title = `Encoder ${name}: drag up and down, or turn the wheel`;
    let y0 = 0, turn = 0;
    const step = (n) => { turn += n * 15; el.querySelector('span').style.setProperty('--r', turn + 'deg'); send(18, id, n & 255); };
    el.onpointerdown = (e) => { el.setPointerCapture(e.pointerId); y0 = e.clientY; };
    el.onpointermove = (e) => { if (!e.buttons) return; const n = Math.trunc((y0 - e.clientY) / 8); if (n) { y0 -= n * 8; step(n); } };
    el.onwheel = (e) => { e.preventDefault(); step(e.deltaY > 0 ? -1 : 1); };
    $('encs').append(el);
  });

  // ---- the 16 pads ----------------------------------------------------------------------------------------
  const rows = [['TONE', '--gold', ['ROOT', 'FIFTH', 'THIRD', 'NINTH']], ['TEXTURE', '--tex', ['FOREST', 'AIR', 'WATER', 'STARDUST']],
                ['SPACE', '--space', ['BLOOM', 'ECHO', 'GRAIN', 'DUSK']], ['FORM', '--form', ['THIN', 'PULSE', 'NEXT', 'GESTURE']]];
  const padEls = [];
  /* (0.20) the pads' signs, the same as on the stage (garden-night.inc nd_pad_sign) */
  const padSign = (g, pad, cx, cy, r, col, t) => {
    const D = (x, y, rr, a = 1) => { g.globalAlpha = a; g.beginPath(); g.arc(x, y, Math.max(.4, rr), 0, 6.283); g.fill(); };
    const L = (pts, w, a = 1) => { g.globalAlpha = a; g.lineWidth = w; g.lineCap = 'round'; g.beginPath(); pts.forEach(([x, y], i) => i ? g.lineTo(x, y) : g.moveTo(x, y)); g.stroke(); };
    g.fillStyle = g.strokeStyle = col; const w = r * .14 + .8, S = Math.sin, C = Math.cos, R = (n, f) => Array.from({length: n + 1}, (_, i) => f(i / n));
    switch (pad) {
    case 0: D(cx - r * .15, cy + r * .45, r * .32); L([[cx + r * .14, cy + r * .42], [cx + r * .14, cy - r * .7]], w); break;
    case 1: D(cx - r * .35, cy + r * .45, r * .26); D(cx + r * .3, cy - r * .1, r * .26); L([[cx - r * .12, cy + r * .42], [cx - r * .12, cy - r * .6]], w, .6); L([[cx + r * .53, cy - r * .14], [cx + r * .53, cy - r * .9]], w, .6); break;
    case 2: for (let k = 0; k < 3; k++) D(cx, cy + r * (.55 - .5 * k), r * .22, k === 1 ? 1 : .6); break;
    case 3: for (let k = 0; k < 5; k++) { const a = k * 1.2566 + t * .2; D(cx + C(a) * r * .55, cy + S(a) * r * .55, r * .16, k & 1 ? .6 : 1); } D(cx, cy, r * .14); break;
    case 4: R(10, (u) => D(cx - r * .6 + u * r * 1.2, cy + r * .5 - u * r, S(u * 3.1416) * r * .32 + .6)); g.strokeStyle = 'rgba(20,34,30,.8)'; L([[cx - r * .7, cy + r * .6], [cx + r * .55, cy - r * .45]], w * .7); break;
    case 5: for (let k = 0; k < 3; k++) L(R(10, (u) => [cx - r * .7 + u * r * (1.3 - .2 * k), cy - r * .4 + k * r * .4 + S(u * 5 + t * 2 + k) * r * .08]), w, k === 1 ? 1 : .6); break;
    case 6: D(cx, cy + r * .25, r * .45); R(8, (u) => D(cx, cy + r * .25 - r * .45 - u * r * .5, r * .45 * (1 - u))); break;
    case 7: L([[cx, cy - r * .8], [cx, cy + r * .8]], w); L([[cx - r * .8, cy], [cx + r * .8, cy]], w); D(cx, cy, r * .2); for (let k = 0; k < 3; k++) D(cx + r * (.55 - .5 * k), cy - r * (.6 - .55 * k), r * .08 + .5, .6); break;
    case 8: for (let k = 0; k < 6; k++) { const a = k * 1.0472 + t * .3; D(cx + C(a) * r * .42, cy + S(a) * r * .42, r * .26, .6); } D(cx, cy, r * .22); break;
    case 9: for (let k = 0; k < 3; k++) { g.globalAlpha = .9 - .25 * k; g.lineWidth = w * .8; g.beginPath(); g.arc(cx, cy, r * (.25 + .3 * k), 0, 6.283); g.stroke(); } break;
    case 10: for (let k = 0; k < 9; k++) { const h = Math.imul(k + 1, 2654435761) >>> 0; D(cx + ((h % 100) / 100 - .5) * r * 1.4, cy + (((h >>> 8) % 100) / 100 - .5) * r * 1.4, r * (.08 + .06 * (k % 3)), k & 1 ? 1 : .6); } break;
    case 11: D(cx, cy, r * .6); g.fillStyle = '#16202f'; D(cx + r * .28, cy - r * .18, r * .52); break;
    case 12: for (let k = 0; k < 3; k++) L([[cx + (k - 1) * r * .45, cy + r * .7], [cx + (k - 1) * r * .45, cy - r * .6]], w, k === 1 ? 1 : .35); break;
    case 13: L(R(16, (u) => [cx - r * .8 + u * r * 1.6, cy - Math.abs(S(u * 6.283 * 2)) * r * .6 + r * .25]), w); break;
    case 14: L([[cx - r * .7, cy], [cx + r * .55, cy]], w); L([[cx + r * .15, cy - r * .4], [cx + r * .55, cy], [cx + r * .15, cy + r * .4]], w); break;
    default: L(R(30, (u) => [cx + C(u * 6.283) * r * .6, cy + S(u * 6.283) * r * .38]), w, .6); D(cx + C(t * 2) * r * .6, cy + S(t * 2) * r * .38, r * .14 + .5);
    }
    g.globalAlpha = 1;
  };
  rows.forEach(([name, color, labels], r) => {
    const rn = document.createElement('div');
    rn.className = 'rowname'; rn.textContent = name; rn.style.color = `var(${color})`;
    $('pads').append(rn);
    labels.forEach((label, c) => {
      const id = r * 4 + c, el = document.createElement('div');
      el.className = 'pad'; el.style.setProperty('--c', `var(${color})`);
      el.innerHTML = `<div class="glow"></div><small>${id + 1}</small><canvas class="sign" width="64" height="64"></canvas><b>${label}</b>`;
      { const c = el.querySelector('canvas'), g = c.getContext('2d'); g.scale(2, 2); padSign(g, id, 16, 16, 12, getComputedStyle(document.documentElement).getPropertyValue(color).trim() || '#fffcf4', 0); }
      el.oncontextmenu = (e) => e.preventDefault();
      el.onpointerdown = (e) => {
        if (e.shiftKey || e.button === 2) { send(15, id); return; }          // latch, as SHIFT + pad
        el.setPointerCapture(e.pointerId);
        const box = el.getBoundingClientRect();
        const vel = Math.round(127 * (1 - .7 * Math.min(1, Math.max(0, (e.clientY - box.top) / box.height))));
        el.dataset.held = '1'; send(10, id, vel);
      };
      const up = () => { if (el.dataset.held) { delete el.dataset.held; send(11, id); } };
      el.onpointerup = up; el.onpointercancel = up;
      $('pads').append(el); padEls.push(el);
    });
  });

  // ---- knobs: drag up and down, double-click for the default -----------------------------------------------
  for (const el of document.querySelectorAll('.knob')) {
    const p = P(el.dataset.id); let y0 = 0, v0 = 0;
    el.onpointerdown = (e) => { el.setPointerCapture(e.pointerId); y0 = e.clientY; v0 = vibe.values[p.i]; };
    el.onpointermove = (e) => { if (!e.buttons) return; vibe.set(p.i, v0 + (y0 - e.clientY) / 220 * (e.shiftKey ? .2 : 1) * (p.max - p.min)); };
    el.ondblclick = () => vibe.set(p.i, p.def);
    vibe.onValues((v) => { const f = norm(p, v[p.i]); el.style.setProperty('--a', f * 270 + 'deg'); el.querySelector('span').textContent = Math.round(f * 100); });
  }
  // ---- sliders and the wander switch ---------------------------------------------------------------------
  for (const s of document.querySelectorAll('.slide input')) {
    const p = P(s.dataset.id), out = s.parentElement.querySelector('em');
    s.oninput = () => vibe.set(p.i, plain(p, s.value / 1000));
    vibe.onValues((v) => { const f = norm(p, v[p.i]); if (document.activeElement !== s) s.value = f * 1000; out.textContent = Math.round(f * 100) + '%'; });
  }
  for (const c of document.querySelectorAll('.toggle input')) {
    const p = P(c.dataset.id);
    c.onchange = () => vibe.set(p.i, c.checked ? p.max : p.min);
    vibe.onValues((v) => { c.checked = v[p.i] > (p.min + p.max) / 2; });
  }
  // ---- menus for choices ----------------------------------------------------------------------------------
  for (const [sel, id] of [['gspeed', 'gesture_speed'], ['tmode', 'tone_mode']]) {
    const p = P(id), el = $(sel);
    p.choices.forEach((name, k) => el.add(new Option((id === 'tone_mode' ? 'Tone: ' : 'Playback: ') + name, k)));
    el.onchange = () => vibe.set(p.i, p.min + Number(el.value) / (p.choices.length - 1) * (p.max - p.min));
    vibe.onValues((v) => { el.value = Math.round(norm(p, v[p.i]) * (p.choices.length - 1)); });
  }
  // ---- four XY pads: textures and spaces as automation (the stronger of pad, gesture and automation wins) ----
  for (const xy of document.querySelectorAll('.xy')) {
    const px = P(xy.dataset.x), py = P(xy.dataset.y), dot = xy.querySelector('i');
    const at = (e) => { const r = xy.getBoundingClientRect();
      vibe.set(px.i, plain(px, (e.clientX - r.left) / r.width)); vibe.set(py.i, plain(py, 1 - (e.clientY - r.top) / r.height)); };
    xy.onpointerdown = (e) => { xy.setPointerCapture(e.pointerId); at(e); };
    xy.onpointermove = (e) => { if (e.buttons) at(e); };
    xy.ondblclick = () => { vibe.set(px.i, px.min); vibe.set(py.i, py.min); };
    vibe.onValues((v) => { dot.style.left = norm(px, v[px.i]) * 100 + '%'; dot.style.top = (1 - norm(py, v[py.i])) * 100 + '%'; });
  }
  // ---- favourites and moments -----------------------------------------------------------------------------
  const favEls = [...Array(6)].map((_, k) => { const b = document.createElement('button'); b.className = 'fav';
    b.onclick = () => send(13, k); $('favs').append(b); return b; });
  const momentEls = [...Array(6)].map((_, k) => { const b = document.createElement('button'); b.className = 'moment';
    b.textContent = 'M' + (k + 1); b.title = 'Click: glide there · Shift-click: store what sounds now';
    b.onclick = (e) => send(12, k, e.shiftKey ? 1 : 0); $('moments').append(b); return b; });

  // ---- the Biome library (0.17): the firmware sends it once on request (20), then 21 loads, 22 stores a favourite ----
  const NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const lib = {families: [], scales: [], biomes: [], family: -1, current: -1, favSlot: 0,
               mode: 'biomes', worlds: [], worldFamilies: [], worldFamily: -1, worldHere: -1, worldsAsked: false};
  const worldsDraw = () => {                           // (0.19) Kid Bunka's worlds: a click enters one
    const q = $('libq').value.trim().toLowerCase();
    let list = lib.worlds.filter((w) => w && (lib.worldFamily < 0 || w.family === lib.worldFamily) &&
      (!q || `${w.name} ${w.story} ${lib.worldFamilies[w.family]} ${w.tempo} ${(lib.biomes.find((b) => b && b.id === w.biome) || {}).name || ''}`.toLowerCase().includes(q)));
    if ($('libsort').value === 'name') list = [...list].sort((a, b) => a.name.localeCompare(b.name));
    if ($('libsort').value === 'tempo') list = [...list].sort((a, b) => a.tempo - b.tempo);
    lib.shown = list;
    $('libcount').textContent = `WORLDS · ${list.length} OF ${lib.worlds.filter(Boolean).length}`;
    const box = $('liblist'), keep = box.scrollTop; box.textContent = '';
    for (const w of list) {
      const row = document.createElement('div'); row.className = 'lib-row' + (w.index === lib.worldHere ? ' on' : '');
      row.innerHTML = `<canvas width="56" height="56"></canvas><b></b><span class="fam"></span><span></span><span class="key"></span><button class="star" title="Store its Biome as favourite F${lib.favSlot + 1}">☆ F${lib.favSlot + 1}</button><div class="story"></div>`;
      row.querySelector('b').textContent = w.name;
      row.querySelector('.fam').textContent = (lib.worldFamilies[w.family] || '').toUpperCase();
      row.children[3].textContent = w.tempo ? `${w.tempo} BPM` : '';
      const biome = lib.biomes.find((b) => b && b.id === w.biome);
      row.querySelector('.key').textContent = biome ? biome.name : '';
      row.querySelector('.story').textContent = w.story;
      sigilOf(row.querySelector('canvas'), w.biome, 0);
      row.onclick = () => send(24, w.index);
      row.querySelector('.star').onclick = (e) => { e.stopPropagation(); send(22, lib.favSlot, w.biome & 255, w.biome >> 8); };
      box.append(row);
    }
    box.scrollTop = keep;
  };
  const libDraw = () => {
    if (lib.mode === 'worlds') { worldsDraw(); return; }
    const q = $('libq').value.trim().toLowerCase();
    let list = lib.biomes.filter((b) => (lib.family < 0 || b.family === lib.family) &&
      (!q || `${b.name} ${lib.families[b.family]} ${b.key} ${b.tempo}`.toLowerCase().includes(q)));
    const order = $('libsort').value;
    if (order === 'name') list = [...list].sort((a, b) => a.name.localeCompare(b.name));
    if (order === 'tempo') list = [...list].sort((a, b) => a.tempo - b.tempo);
    lib.shown = list;
    $('libcount').textContent = `BIOME LIBRARY · ${list.length} OF ${lib.biomes.length}`;
    const box = $('liblist'), keep = box.scrollTop; box.textContent = '';
    for (const b of list) {
      const row = document.createElement('div'); row.className = 'lib-row' + (b.id === lib.current ? ' on' : '');
      row.innerHTML = `<canvas width="56" height="56"></canvas><b></b><span class="fam"></span><span>${b.tempo} BPM</span><span class="key"></span><button class="star" title="Store as favourite F${lib.favSlot + 1}">☆ F${lib.favSlot + 1}</button>`;
      row.querySelector('b').textContent = b.name; row.querySelector('.fam').textContent = lib.families[b.family] || '';
      row.querySelector('.key').textContent = b.key;
      sigilOf(row.querySelector('canvas'), b.id, 0);
      row.onclick = () => send(21, b.id & 255, b.id >> 8);
      row.querySelector('.star').onclick = (e) => { e.stopPropagation(); send(22, lib.favSlot, b.id & 255, b.id >> 8); };
      box.append(row);
    }
    box.scrollTop = keep;
  };
  const libFamilies = () => {
    const box = $('libfams'); box.textContent = '';
    if (lib.mode === 'worlds') {
      ['ALL', ...lib.worldFamilies].forEach((name, i) => {
        const b = document.createElement('button'); b.textContent = name.toUpperCase(); b.classList.toggle('on', i - 1 === lib.worldFamily);
        b.onclick = () => { lib.worldFamily = i - 1; libFamilies(); libDraw(); }; box.append(b);
      });
      return;
    }
    ['ALL', ...lib.families].forEach((name, i) => {
      if (i > 0 && lib.biomes.length && !lib.biomes.some((x) => x && x.family === i - 1)) return;   // (BLANK has none)
      const b = document.createElement('button'); b.textContent = name; b.classList.toggle('on', i - 1 === lib.family);
      b.onclick = () => { lib.family = i - 1; libFamilies(); libDraw(); }; box.append(b);
    });
  };
  $('libq').oninput = libDraw; $('libsort').onchange = libDraw;
  $('libdice').onclick = () => {
    const l = lib.shown || []; if (!l.length) return;
    const x = l[Math.floor(Math.random() * l.length)];
    if (lib.mode === 'worlds') send(24, x.index); else send(21, x.id & 255, x.id >> 8);
  };
  let libTimer = null;
  const libLater = () => { clearTimeout(libTimer); libTimer = setTimeout(libDraw, 60); };
  const setMode = (m) => {
    lib.mode = m;
    $('tab-biomes').classList.toggle('on', m === 'biomes'); $('tab-worlds').classList.toggle('on', m === 'worlds');
    $('libwander').hidden = m !== 'worlds';
    if (m === 'worlds' && !lib.worldsAsked) { lib.worldsAsked = true; send(23); }
    libFamilies(); libDraw();
  };
  $('libwander').onchange = () => send(25, Number($('libwander').value));
  $('tab-biomes').onclick = () => setMode('biomes');
  $('tab-worlds').onclick = () => setMode('worlds');
  const libPacket = (b) => {
    if (b[0] === 104) { let k = 2; lib.worldFamilies = []; for (let i = 0; i < b[1]; i++) { let t; [t, k] = text(b, k); lib.worldFamilies.push(t); } return; }
    if (b[0] === 103) {
      const total = b[1], start = b[2], count = b[3]; let k = 4;
      for (let i = 0; i < count; i++) {
        const family = b[k], biome = b[k + 1] | (b[k + 2] << 8), tempo = b[k + 3]; k += 4;
        let name, story; [name, k] = text(b, k); [story, k] = text(b, k);
        lib.worlds[start + i] = {index: start + i, family, biome, tempo, name, story};
      }
      if (start + count >= total && lib.mode === 'worlds') { libFamilies(); libDraw(); }
      return;
    }
    if (b[0] === 102) {
      let k = 1; const nf = b[k++]; lib.families = []; lib.scales = [];
      for (let i = 0; i < nf; i++) { let t; [t, k] = text(b, k); lib.families.push(t); }
      const ns = b[k++]; for (let i = 0; i < ns; i++) { let t; [t, k] = text(b, k); lib.scales.push(t); }
      lib.biomes = []; libFamilies(); return;
    }
    const total = b[1] | (b[2] << 8), start = b[3] | (b[4] << 8), count = b[5];
    let k = 6;
    for (let i = 0; i < count; i++) {
      const id = b[k] | (b[k + 1] << 8), tempo = b[k + 2], root = b[k + 3], scale = b[k + 4], family = b[k + 5]; k += 6;
      let name; [name, k] = text(b, k);
      lib.biomes[start + i] = {id, tempo, family, name, key: `${NOTES[root % 12]} ${lib.scales[scale] || ''}`.trim()};
    }
    if (start + count >= total) { libFamilies(); libDraw(); favEls.forEach((el) => { el.dataset.name = ''; }); } else libLater();
  };
  setTimeout(() => send(20), 300);

  // ---- the firmware's status ------------------------------------------------------------------------------
  const text = (b, k) => [new TextDecoder().decode(b.subarray(k + 1, k + 1 + b[k])), k + 1 + b[k]];
  vibe.onData((b) => {
    if (b[0] >= 101 && b[0] <= 104) { libPacket(b); return; }
    if (b[0] !== 100 || b.length < 33) return;
    const playing = b[1], onAir = b[2], inDream = b[3];
    const gmode = b[7], armed = b[8], rec = b[9], recSec = b[10], moment = b[11] - 1, fav = b[12];
    const latched = b[13] | (b[14] << 8), used = b[15];
    $('play').classList.toggle('on', !!playing);
    $('air').classList.toggle('on', !!onAir);
    $('rec').classList.toggle('on', rec !== 0);
    $('rec').querySelector('span').textContent = rec === 1 ? `REC ${Math.floor(recSec / 60)}:${String(recSec % 60).padStart(2, '0')}` : rec === 2 ? 'SAVING' : 'REC';
    padEls.forEach((el, i) => { el.querySelector('.glow').style.opacity = (b[16 + i] / 255 * .85).toFixed(2); el.classList.toggle('latched', !!(latched >> i & 1)); });
    let k = 32, biome, family, line, page;
    [biome, k] = text(b, k); [family, k] = text(b, k); [line, k] = text(b, k); [page, k] = text(b, k);
    $('biome').textContent = biome; $('family').textContent = inDream ? 'ITINERA' : family; $('page').textContent = 'PAGE · ' + page;
    $('radio').textContent = onAir ? 'ON AIR · MUTABLE DREAMS FM · ' + line : '';
    favEls.forEach((el, i) => {
      let name; [name, k] = text(b, k);
      if (el.dataset.name !== name) {                  // (a sign when the library knows the name)
        el.dataset.name = name; el.textContent = '';
        const known = lib.biomes.find((x) => x && x.name === name);
        if (known) { const c = document.createElement('canvas'); c.width = c.height = 36; sigilOf(c, known.id, 0); el.append(c); }
        el.append(`F${i + 1} ${name}`);
      }
      el.classList.toggle('on', i === fav);
    });
    const group = b[k], open = b[k + 1] ?? 0, current = b[k + 2] | (b[k + 3] << 8);
    nowBiome = current;
    const here = (b[k + 4] ?? 0) - 1, wander = b[k + 5] ?? 0;
    if (document.activeElement !== $('libwander')) $('libwander').value = String(wander);
    $('world').textContent = here >= 0 ? `WORLD · ${(lib.worlds[here] || {}).name || ''}${wander ? ' · WANDERS ON' : ''}` : '';
    if (here >= 0 && !lib.worldsAsked) { lib.worldsAsked = true; send(23); }
    if (here !== lib.worldHere) { lib.worldHere = here; if (lib.mode === 'worlds') libDraw(); }
    if (b.length > k + 6) {                             // (0.24) the garden listens
      const lon = b[k + 6], smode = b[k + 7], level = b[k + 8], heard = b[k + 9] - 1, answer = b[k + 10] - 1;
      const NN = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'], nn = (n) => NN[n % 12] + (Math.floor(n / 12) - 1);
      $('lin').classList.toggle('on', !!lon);
      $('lmeter').style.width = (level / 2.55).toFixed(0) + '%';
      $('lnote').textContent = answer >= 0 ? nn(answer) : heard >= 0 ? nn(heard) : '–';
      $('lnote').classList.toggle('answer', answer >= 0);
      document.querySelectorAll('#sing button').forEach((x) => x.classList.toggle('on', Number(x.dataset.m) === smode));
      listenOn = !!lon;
      const step = (b[k + 11] ?? 0) - 1;                // (0.26) FIRST STEPS
      const TOUR = [['WELCOME TO THE GARDEN', 'Press PLAY: a Biome wakes up – six tracks of music that never repeat.'],
        ['PLAY WITH YOUR HANDS', 'Hold pad 9 BLOOM: a room opens. Pads 5–12 are textures and spaces.'],
        ['SHAPE THE BIOME', 'Turn encoder 1 on LIVE (DENSITY) – or the DENSITY knob here. Encoders 1–4 are the four macros.'],
        ['ANOTHER WORLD', 'Tap pad 15 NEXT for the next Biome. F1–F6 on LIVE are your favourites.'],
        ['YOUR MOVES, LOOPED', 'Tap pad 16, play pads 5–12 a while, tap 16 again: your hands loop by themselves.'],
        ['THE WHOLE INSTRUMENT', 'Press MENU: all 50 pages. The coloured keys jump into groups.'],
        ['THE GARDEN LISTENS', 'LISTEN page (FX): F1 lets it hear IN 1. Sing on SING – it answers. (Moves on by itself.)'],
        ['YOU KNOW THE GARDEN', 'HOME explains every page. Hold an F key: it says what it does. Have fun – Kid Bunka']];
      $('coach').hidden = step < 0;
      $('tour').classList.toggle('on', step >= 0);
      if (step >= 0 && TOUR[step]) {
        $('coachtitle').textContent = `${step + 1} / 8 · ${TOUR[step][0]}`; $('coachtext').textContent = TOUR[step][1];
        const seeds = $('coachseeds'); if (seeds.children.length !== 8) { seeds.textContent = ''; for (let i = 0; i < 8; i++) seeds.append(document.createElement('i')); }
        [...seeds.children].forEach((el, i) => { el.className = i < step ? 'done' : i === step ? 'now' : ''; });
      }
      $('lhint').textContent = !lon ? 'INPUT lets the garden hear IN 1 / IN 2 (the LISTEN page has the sends).' :
        ['The input flows into the cloud, the echo and the room.', 'FOLLOW: the garden plays what you sing.',
         'HARMONY: a third or a fifth above you.', 'ANSWER: sing a phrase, stop – the garden replies.'][smode] || '';
    }
    if (kit) { kit.np.biome = current; kit.np.energy = Math.min(1, .25 + [...b.subarray(16, 32)].reduce((a, v) => a + v, 0) / 255 / 6); }
    if ((current !== lib.current || fav !== lib.favSlot) && lib.biomes.length) { lib.current = current; lib.favSlot = fav; libDraw(); }
    else { lib.current = current; lib.favSlot = fav; }
    groupEls.forEach((el, g) => el.classList.toggle('on', !(open & 1) && g === group));
    menu.classList.toggle('on', !!(open & 1));
    $('shift').classList.toggle('on', !!(open & 4));
    momentEls.forEach((el, i) => { el.classList.toggle('on', i === moment); el.classList.toggle('used', !!(used >> i & 1)); });
    $('gstate').textContent = armed ? 'Waiting for the next bar …' : gmode === 1 ? 'Capturing – play TEXTURE and SPACE (pads 5–12), tap again to loop' :
      gmode === 3 ? 'Layer – this pass adds to the loop' : gmode === 2 ? 'Looping – STOP ends it, LAYER adds a pass' : 'Tap CAPTURE, play TEXTURE and SPACE (pads 5–12): your moves loop.';
    $('grec').textContent = armed ? 'CANCEL' : gmode === 1 ? 'LOOP IT' : gmode === 3 ? 'END LAYER' : gmode === 2 ? 'STOP' : 'CAPTURE';
  });
})();
