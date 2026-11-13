// Mutable Dreams · Dream Console – the plugin's own Editor page (SDK 16).
// Everything goes through window.vibe: parameters with vibe.set (the DAW records them), the rest as small
// commands with vibe.send; the firmware answers with a status packet (garden-editor.inc). The page keeps no
// state of its own: what it shows comes from onValues and onData.
(async () => {
  await vibe.ready;
  const $ = (id) => document.getElementById(id);
  const send = (...bytes) => vibe.send(new Uint8Array(bytes));
  const P = (id) => vibe.byId(id);
  const norm = (p, v) => (v - p.min) / (p.max - p.min);
  const plain = (p, f) => p.min + Math.min(1, Math.max(0, f)) * (p.max - p.min);

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
  menu.title = 'MENU: all 47 pages – click one on the display'; menu.onclick = () => send(16, 19); $('groups').append(menu);
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
  rows.forEach(([name, color, labels], r) => {
    const rn = document.createElement('div');
    rn.className = 'rowname'; rn.textContent = name; rn.style.color = `var(${color})`;
    $('pads').append(rn);
    labels.forEach((label, c) => {
      const id = r * 4 + c, el = document.createElement('div');
      el.className = 'pad'; el.style.setProperty('--c', `var(${color})`);
      el.innerHTML = `<div class="glow"></div><small>${id + 1}</small><b>${label}</b>`;
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
  const lib = {families: [], scales: [], biomes: [], family: -1, current: -1, favSlot: 0};
  const libDraw = () => {
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
      row.innerHTML = `<b></b><span class="fam"></span><span>${b.tempo} BPM</span><span class="key"></span><button class="star" title="Store as favourite F${lib.favSlot + 1}">☆ F${lib.favSlot + 1}</button>`;
      row.querySelector('b').textContent = b.name; row.querySelector('.fam').textContent = lib.families[b.family] || '';
      row.querySelector('.key').textContent = b.key;
      row.onclick = () => send(21, b.id & 255, b.id >> 8);
      row.querySelector('.star').onclick = (e) => { e.stopPropagation(); send(22, lib.favSlot, b.id & 255, b.id >> 8); };
      box.append(row);
    }
    box.scrollTop = keep;
  };
  const libFamilies = () => {
    const box = $('libfams'); box.textContent = '';
    ['ALL', ...lib.families].forEach((name, i) => {
      if (i > 0 && lib.biomes.length && !lib.biomes.some((x) => x && x.family === i - 1)) return;   // (BLANK has none)
      const b = document.createElement('button'); b.textContent = name; b.classList.toggle('on', i - 1 === lib.family);
      b.onclick = () => { lib.family = i - 1; libFamilies(); libDraw(); }; box.append(b);
    });
  };
  $('libq').oninput = libDraw; $('libsort').onchange = libDraw;
  $('libdice').onclick = () => { const l = lib.shown || []; if (l.length) { const b = l[Math.floor(Math.random() * l.length)]; send(21, b.id & 255, b.id >> 8); } };
  let libTimer = null;
  const libLater = () => { clearTimeout(libTimer); libTimer = setTimeout(libDraw, 60); };
  const libPacket = (b) => {
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
    if (start + count >= total) { libFamilies(); libDraw(); } else libLater();
  };
  setTimeout(() => send(20), 300);

  // ---- the firmware's status ------------------------------------------------------------------------------
  const text = (b, k) => [new TextDecoder().decode(b.subarray(k + 1, k + 1 + b[k])), k + 1 + b[k]];
  vibe.onData((b) => {
    if (b[0] === 101 || b[0] === 102) { libPacket(b); return; }
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
    favEls.forEach((el, i) => { let name; [name, k] = text(b, k); el.textContent = `F${i + 1} ${name}`; el.classList.toggle('on', i === fav); });
    const group = b[k], open = b[k + 1] ?? 0, current = b[k + 2] | (b[k + 3] << 8);
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
