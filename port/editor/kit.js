/* Copied from Trimui-Brick/Visual-Kit/kit.js (0.1, by Kid Bunka, GPL-3.0-only) for the Dream Console (VIBE FM 0.19).
 * Added at the end: MDKit.sigil(ctx, id, cx, cy, r, colour, t) draws one Biome sign into any 2D context, and
 * kit.np.biome can be set from outside (the console follows the playing Biome). Keep in step with the Brick. */
/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams Visual Kit, by Kid Bunka: the Brick's worlds, drawn live in a browser (OBS browser source, a
 * radio studio page, a Grid app or plugin webview). The same rules as the Trimui Brick port (brick_garden.inc,
 * brick_views_draw.inc): every Biome its own sky, land, sign; fourteen kinds of plants; spores, fireflies, mist.
 *
 * MDKit.start(canvas, options) - options (or the page's URL query, see index.html):
 *   scene   garden | cave | meadow | stream | dream | atlas | title | cycle   (cycle: one after the other)
 *   biome   0..271: which Biome's place (default: from the now-playing data, else 0)
 *   cycle   seconds per scene in cycle mode (default 45)
 *   np      URL of a JSON file polled every 5 s: {"title","artist","show","biome","bpm","energy"}
 *   title, artist, show   what plays now, fixed (instead of np)
 *   lower   1: the lower third (sign, Biome, what plays); 0: none
 *   clear   1: no sky (transparent, for laying over video)
 *   bpm     tempo of the plants' pulse (default 76)
 */
(function () {
  'use strict';
  const TRACK = ['#aecfb6', '#dcc694', '#86c2b4', '#a5bed9', '#c8b1d8', '#cca091'];
  const SKY = [['#040610', '#462868'], ['#020c10', '#10605c'], ['#0a060e', '#824030'], ['#080412', '#782c60'],
               ['#040a16', '#3c6e96'], ['#030c08', '#286438'], ['#0c0806', '#785c28'], ['#020308', '#222e5a']];
  const SPECIES = ['tulip', 'fern', 'bells', 'dandelion', 'reeds', 'mushroom', 'thistle', 'grass', 'sunflower',
                   'lantern', 'lotus', 'spiral', 'crystal', 'orchid'];
  const imul = Math.imul;
  const hash = (n) => (imul((n | 0) >>> 0, 2654435761) >>> 0);

  function rgb(hex) { const v = parseInt(hex.slice(1), 16); return [v >> 16 & 255, v >> 8 & 255, v & 255]; }
  function mix(a, b, k) {
    const A = rgb(a), B = rgb(b); k = Math.max(0, Math.min(1, k));
    return '#' + A.map((x, i) => Math.round(x + (B[i] - x) * k).toString(16).padStart(2, '0')).join('');
  }
  function rgba(hex, a) { const c = rgb(hex); return `rgba(${c[0]},${c[1]},${c[2]},${a})`; }

  /* a Biome's place: the same hash and choices as gd_sky in brick_garden.inc */
  function place(biome) {
    const bh = hash(biome + 7);
    return { pal: (bh >>> 3) % 8, land: (bh >>> 9) % 6, extra: (bh >>> 15) % 5, bh, ph: (bh >>> 24) * 0.05 };
  }

  class Kit {
    constructor(canvas, o) {
      this.cv = canvas; this.g = canvas.getContext('2d'); this.o = o;
      this.np = { title: o.title || '', artist: o.artist || '', show: o.show || '', biome: o.biome, bpm: +o.bpm || 76, energy: 0.4 };
      this.scenes = ['garden', 'cave', 'meadow', 'stream', 'dream', 'atlas', 'title'];
      this.sceneIdx = 0; this.sceneAt = 0; this.fade = 1;
      this.spores = []; this.t0 = performance.now();
      this.resize(); window.addEventListener('resize', () => this.resize());
      if (o.np) this.poll();
      requestAnimationFrame((ts) => this.frame(ts));
    }
    resize() {
      const d = Math.min(window.devicePixelRatio || 1, this.o.maxDpr || 2);   /* (VIBE FM console: 1.25, a background) */
      this.W = this.cv.clientWidth || window.innerWidth || 1280; this.H = this.cv.clientHeight || window.innerHeight || 720;
      this.cv.width = Math.round(this.W * d); this.cv.height = Math.round(this.H * d);
      this.g.setTransform(d, 0, 0, d, 0, 0);
      this.u = Math.min(this.W / 1280, this.H / 720);          // one unit: the layout is thought at 1280 x 720
    }
    async poll() {
      try {
        const r = await fetch(this.o.np + (this.o.np.includes('?') ? '&' : '?') + 't=' + Date.now(), { cache: 'no-store' });
        if (r.ok) Object.assign(this.np, await r.json());
      } catch (e) { /* (no data yet: keep what we have) */ }
      setTimeout(() => this.poll(), 5000);
    }
    biome() { return this.np.biome != null && this.np.biome !== '' ? (+this.np.biome | 0) : (this.o.biome != null ? +this.o.biome | 0 : 0); }
    scene(t) {
      if (this.o.scene && this.o.scene !== 'cycle') return this.o.scene;
      const per = +(this.o.cycle || 45), i = Math.floor(t / per) % this.scenes.length, f = (t % per) / per;
      this.fade = f < 0.04 ? f / 0.04 : f > 0.96 ? (1 - f) / 0.04 : 1;
      return this.scenes[i];
    }
    frame(ts) {
      requestAnimationFrame((x) => this.frame(x));       // (first: a slip in one frame never stops the next)
      if (!this.cv.width || !this.cv.height || this.W !== (this.cv.clientWidth || window.innerWidth)) this.resize();
      if (!this.cv.width || !this.cv.height) return;
      try { this.draw(ts); } catch (e) { if (!this.warned) { this.warned = 1; console.warn('MDKit:', e); } }
    }
    draw(ts) {
      const t = (ts - this.t0) / 1000, g = this.g, W = this.W, H = this.H, sc = this.scene(t), b = this.biome();
      const bpm = +this.np.bpm || 76, beat = (t * bpm / 60) % 1, energy = Math.max(0.15, Math.min(1, +this.np.energy || 0.4));
      g.clearRect(0, 0, W, H);
      if (this.o.clear !== '1') this.sky(t, b, sc === 'cave' || sc === 'meadow' || sc === 'stream' ? 0.6 : 1);
      g.save(); g.globalAlpha = this.fade;
      if (sc === 'garden') this.garden(t, b, beat, energy);
      else if (sc === 'cave') this.cave(t, beat);
      else if (sc === 'meadow') this.meadow(t, b, energy);
      else if (sc === 'stream') this.stream(t, energy);
      else if (sc === 'dream') this.dream(t, b);
      else if (sc === 'atlas') this.atlas(t, b);
      else if (sc === 'title') this.title(t);
      g.restore();
      this.ambient(t, b, energy);
      if (this.o.lower !== '0') this.lower(t, b);
    }

    /* ---- the sky and the land (gd_sky): palette, stars, something of its own, land of its kind, veils ---- */
    sky(t, b, dim) {
      const g = this.g, W = this.W, H = this.H, P = place(b), top = SKY[P.pal][0], glow = SKY[P.pal][1], hz = H * 0.72;
      let gr = g.createLinearGradient(0, 0, 0, hz);
      gr.addColorStop(0, top); gr.addColorStop(1, mix(top, glow, 0.85 * dim));
      g.fillStyle = gr; g.fillRect(0, 0, W, hz);
      for (let i = 0; i < 160; i++) {                  // stars
        const h = hash(i + 1), x = (h % 10000) / 10000 * W, y = ((h >>> 10) % 10000) / 10000 * (hz - 80);
        const tw = 0.5 + 0.5 * Math.sin(t * (0.6 + ((h >>> 24) & 7) * 0.2) + i);
        g.fillStyle = `rgba(235,240,255,${(0.15 + 0.6 * tw) * (1 - y / hz)})`;
        g.beginPath(); g.arc(x, y, (0.6 + ((h >>> 20) & 3) * 0.35) * this.u, 0, 6.283); g.fill();
      }
      const ex = W * (0.15 + ((P.bh >>> 20) % 480) / 640), ey = H * (0.12 + ((P.bh >>> 12) % 120) / 720), u = this.u;
      if (P.extra === 1) for (let k = 0; k < 7; k++) {   // a nebula
        const c = mix(glow, '#c878dc', k / 7), r = (70 + k * 14) * u;
        const rg = g.createRadialGradient(ex + 80 * u * Math.sin(k * 2.1), ey + 40 * u * Math.cos(k * 1.7), 0, ex, ey, r * 1.6);
        rg.addColorStop(0, rgba(c, 0.10)); rg.addColorStop(1, rgba(c, 0)); g.fillStyle = rg; g.fillRect(0, 0, W, hz);
      } else if (P.extra === 2) {                      // a ringed planet
        g.fillStyle = mix(glow, '#e6c8a0', 0.5); g.beginPath(); g.arc(ex, ey, 18 * u, 0, 6.283); g.fill();
        g.strokeStyle = 'rgba(230,220,200,0.7)'; g.lineWidth = 2 * u; g.beginPath(); g.ellipse(ex, ey, 36 * u, 9 * u, -0.2, 0, 6.283); g.stroke();
      } else if (P.extra === 3) {                      // a second moon
        g.fillStyle = 'rgba(220,225,240,0.9)'; g.beginPath(); g.arc(ex, ey, 10 * u, 0, 6.283); g.fill();
        g.fillStyle = top; g.beginPath(); g.arc(ex + 4 * u, ey - 2 * u, 9 * u, 0, 6.283); g.fill();
      } else if (P.extra === 4) for (let k = 0; k < 5; k++) {   // aurora
        g.strokeStyle = rgba(mix('#5adcaa', glow, k / 5), 0.07 + 0.02 * (4 - k)); g.lineWidth = 14 * u; g.beginPath();
        for (let x = 0; x <= W; x += 12) { const y = ey - 40 * u + 26 * u * Math.sin(x / W * 5 + t * 0.3) + k * 10 * u; x ? g.lineTo(x, y) : g.moveTo(x, y); }
        g.stroke();
      }
      { // the moon crossing slowly
        const a = 3.1416 * (0.1 + 0.8 * ((t / 240) % 1)), mx = W / 2 - Math.cos(a) * W * 0.42, my = hz * 0.85 - Math.sin(a) * hz * 0.7;
        const rg = g.createRadialGradient(mx, my, 0, mx, my, 70 * u); rg.addColorStop(0, 'rgba(250,246,228,0.25)'); rg.addColorStop(1, 'rgba(250,246,228,0)');
        g.fillStyle = rg; g.beginPath(); g.arc(mx, my, 70 * u, 0, 6.283); g.fill();
        g.fillStyle = '#faf6e4'; g.beginPath(); g.arc(mx, my, 22 * u, 0, 6.283); g.fill();
        g.fillStyle = 'rgba(120,120,140,0.25)'; [[-7, -5, 5], [8, 4, 4], [2, -11, 3]].forEach(([dx, dy, r]) => { g.beginPath(); g.arc(mx + dx * u, my + dy * u, r * u, 0, 6.283); g.fill(); });
      }
      for (let layer = 0; layer < 3; layer++) {        // the land, far to near (the six kinds of gd_sky)
        g.fillStyle = mix(mix(top, glow, 0.30 - 0.09 * layer), '#060a12', 0.45 + 0.25 * layer);
        g.beginPath(); g.moveTo(0, hz);
        for (let x = 0; x <= W; x += 4) {
          const v = x / W, base = hz - (70 - layer * 34) * u * 1.2; let hy;
          switch (P.land) {
            case 1: hy = base - 40 * u * Math.abs(Math.sin(v * (3 + layer) + P.ph + layer)) + 12 * u * Math.abs(Math.sin(v * 13 + P.ph)) + 20 * u; break;
            case 2: hy = base + 14 * u * Math.sin(v * (1.4 + 0.5 * layer) + P.ph + layer * 2) + 10 * u; break;
            case 3: { const tri = Math.abs(((v * (34 + 10 * layer) + P.ph) % 1) - 0.5) * 2; hy = base + 16 * u * Math.sin(v * 2.5 + layer + P.ph) - (layer < 2 ? (1 - tri) * (26 - 6 * layer) * u : 0); break; }
            case 4: hy = base + 26 * u * Math.floor(2 * Math.sin(v * (2.5 + layer) + P.ph + layer)) * 0.5; break;
            case 5: { const sp = Math.sin(v * (7 + 3 * layer) + P.ph * 3); hy = base + 12 * u * Math.sin(v * 3 + layer) - (sp > 0.85 ? (sp - 0.85) * 520 * u : 0); break; }
            default: hy = base + 22 * u * Math.sin(v * (2 + layer) + layer * 1.7 + P.ph) + 9 * u * Math.sin(v * 9 + layer);
          }
          g.lineTo(x, hy);
        }
        g.lineTo(W, hz); g.closePath(); g.fill();
      }
      gr = g.createLinearGradient(0, hz, 0, H); gr.addColorStop(0, '#08101c'); gr.addColorStop(1, '#03060c');
      g.fillStyle = gr; g.fillRect(0, hz, W, H - hz);
      // veils drifting, and a vignette (gd_texture, here as gradients)
      for (let k = 0; k < 3; k++) {
        const vx = W * (0.2 + 0.3 * k) + Math.sin(t * 0.05 + k) * W * 0.1, vy = hz * (0.3 + 0.15 * k);
        const rg = g.createRadialGradient(vx, vy, 0, vx, vy, W * 0.35); rg.addColorStop(0, rgba(mix(glow, '#b4a0ff', 0.3), 0.08)); rg.addColorStop(1, rgba(glow, 0));
        g.fillStyle = rg; g.fillRect(0, 0, W, hz);
      }
      const vg = g.createRadialGradient(W / 2, H / 2, H * 0.35, W / 2, H / 2, H * 0.95); vg.addColorStop(0, 'rgba(0,0,0,0)'); vg.addColorStop(1, 'rgba(0,0,0,0.55)');
      g.fillStyle = vg; g.fillRect(0, 0, W, H);
    }

    /* ---- the plants: the fourteen kinds (gd_draw_species), drawn with paths ---- */
    stem(x, foot, h, bend, sway, w, col) {
      const g = this.g; g.strokeStyle = col; g.lineCap = 'round';
      let px = x, py = foot;
      for (let s = 1; s <= 14; s++) {
        const f = s / 14, nx = x + (bend + sway) * f * f, ny = foot - h * f;
        g.lineWidth = w * (1 - 0.5 * f); g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke(); px = nx; py = ny;
      }
      return [px, py];
    }
    leaf(x, y, side, len, rise, col, a) {
      const g = this.g; g.fillStyle = rgba(col, a); g.beginPath();
      g.moveTo(x, y); g.quadraticCurveTo(x + side * len * 0.5, y - rise - len * 0.25, x + side * len, y - rise);
      g.quadraticCurveTo(x + side * len * 0.5, y - rise * 0.2 + len * 0.1, x, y); g.fill();
    }
    disc(x, y, r, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.arc(x, y, Math.max(0.3, r), 0, 6.283); g.fill(); }
    plant(kind, x, foot, h, col, t, bloom, sway, k) {
      const g = this.g, u = this.u * k, D = (a, b, r, c) => this.disc(a, b, r, c);
      let tip;
      switch (kind) {
        case 'fern': {
          tip = this.stem(x, foot, h, -10 * u, sway, 5 * u, col);
          for (let s = 2; s <= 12; s += 2) { const f = s / 14; this.leaf(x + (-10 * u + sway) * f * f, foot - h * f, s & 2 ? 1 : -1, 26 * u * (1 - f * 0.6), 8 * u, col, 0.5); }
          let a = 0, r = 22 * u, cx = tip[0] + 16 * u, cy = tip[1], px = tip[0], py = tip[1]; g.strokeStyle = col;
          for (let q = 0; q < 40; q++) { const nx = cx - Math.cos(a) * r, ny = cy - Math.sin(a) * r; g.lineWidth = 3 * u * (1 - q / 50); g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke(); px = nx; py = ny; a += 0.22; r *= 0.97 - 0.04 * (1 - (0.35 + 0.4 * bloom)); }
          break;
        }
        case 'bells': {
          g.strokeStyle = col; let px = x, py = foot;
          for (let s = 1; s <= 14; s++) {
            const f = s / 14, nx = x + (46 * u + sway) * f, ny = foot - h * Math.sin(f * 2.2) / Math.sin(1.65) * 0.8;
            g.lineWidth = 3.6 * u * (1 - 0.5 * f); g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke();
            if (s === 6 || s === 10 || s === 14) {
              const sw = Math.sin(t * 6 + s) * 10 * u * bloom, bx = nx + sw, by = ny + 30 * u;
              g.lineWidth = 1.5 * u; g.beginPath(); g.moveTo(nx, ny); g.lineTo(bx, by - 10 * u); g.stroke();
              g.fillStyle = rgba(col, 0.85); g.beginPath(); g.moveTo(bx - 4 * u, by - 10 * u); g.quadraticCurveTo(bx - 14 * u, by + 4 * u, bx - 13 * u, by + 9 * u);
              g.lineTo(bx + 13 * u, by + 9 * u); g.quadraticCurveTo(bx + 14 * u, by + 4 * u, bx + 4 * u, by - 10 * u); g.fill();
              D(bx, by + 13 * u, 3 * u, '#fffcf4');
            }
            px = nx; py = ny;
          }
          break;
        }
        case 'dandelion': {
          tip = this.stem(x, foot, h, 4 * u, sway, 3.4 * u, col);
          const r = 34 * u; g.strokeStyle = rgba(col, 0.55); g.lineWidth = 1 * u;
          for (let q = 0; q < 22; q++) { const a = q * 0.2856 + t * 0.1, rr = r * (0.85 + 0.15 * Math.sin(q * 2.1)), ex = tip[0] + Math.cos(a) * rr, ey = tip[1] + Math.sin(a) * rr;
            g.beginPath(); g.moveTo(tip[0], tip[1]); g.lineTo(ex, ey); g.stroke(); D(ex, ey, (2 + 1.2 * bloom) * u, 'rgba(255,252,244,0.8)'); }
          D(tip[0], tip[1], 4 * u, col); break;
        }
        case 'reeds': {
          for (let q = 0; q < 5; q++) { const lean = (q - 2) * 9 * u, hh = h * (0.55 + 0.1 * ((q * 7) % 4)); g.strokeStyle = rgba(col, 0.65); g.lineWidth = 2.5 * u;
            g.beginPath(); g.moveTo(x + (q - 2) * 5 * u, foot); g.quadraticCurveTo(x + (q - 2) * 5 * u, foot - hh * 0.6, x + (q - 2) * 5 * u + lean + sway * 1.3, foot - hh); g.stroke(); }
          tip = this.stem(x, foot, h, 3 * u, sway, 2.4 * u, col);
          g.fillStyle = mix(col, '#785032', 0.5); g.beginPath(); g.ellipse(tip[0], tip[1] + 16 * u, 6 * u, 16 * u, 0, 0, 6.283); g.fill(); break;
        }
        case 'mushroom': {
          [[-34, 1.1], [8, 1.4], [40, 0.85]].forEach(([o, s], m) => { const ss = s * u * (0.8 + 0.4 * bloom), jump = bloom * 18 * s * u * (m === 1 ? 1 : 0.6), mx = x + o * u, top = foot - 40 * ss - jump;
            g.strokeStyle = 'rgba(230,222,205,0.9)'; g.lineWidth = 9 * ss; g.beginPath(); g.moveTo(mx, foot); g.lineTo(mx, top + 6 * ss); g.stroke();
            g.fillStyle = col; g.beginPath(); g.ellipse(mx, top, 24 * ss, 15 * ss, 0, 3.1416, 6.283); g.fill();
            for (let d = 0; d < 3; d++) D(mx + (d - 1) * 9 * ss, top - 8 * ss, 2.2 * ss, 'rgba(255,252,244,0.8)'); });
          break;
        }
        case 'thistle': {
          tip = this.stem(x, foot, h, -5 * u, sway, 4 * u, col); const r = 15 * u, sp = 1 + 0.5 * bloom; g.strokeStyle = col; g.lineWidth = 1.6 * u;
          for (let q = 0; q < 22; q++) { const a = q * 0.2856 - 1.57; if (Math.sin(a) > 0.5) continue; const L = r * (1.5 + 0.4 * Math.sin(q * 3.7)) * sp;
            g.beginPath(); g.moveTo(tip[0], tip[1]); g.lineTo(tip[0] + Math.cos(a) * L, tip[1] + Math.sin(a) * L); g.stroke(); }
          D(tip[0], tip[1] + 3 * u, r, mix(col, '#3c4658', 0.4)); break;
        }
        case 'grass': {
          for (let q = 0; q < 9; q++) { const lean = (q - 4) * 6 * u, hh = h * (0.5 + 0.5 * Math.abs(Math.sin(q * 1.7))), bx = x + (q - 4) * 3 * u, ex = bx + lean + sway * 1.6 + Math.sin(t * 3 + q) * 2 * u;
            g.strokeStyle = rgba(col, 0.75); g.lineWidth = 2 * u; g.beginPath(); g.moveTo(bx, foot); g.quadraticCurveTo(bx, foot - hh * 0.6, ex, foot - hh); g.stroke();
            if (q % 2 === 0) D(ex, foot - hh, (2 + 2.5 * bloom) * u, `rgba(255,252,244,${0.3 + 0.7 * bloom})`); }
          break;
        }
        case 'sunflower': {
          tip = this.stem(x, foot, h, 10 * u, sway, 6 * u, col); this.leaf(x, foot - 20 * u, -1, 38 * u, 14 * u, col, 0.5); this.leaf(x, foot - 40 * u, 1, 34 * u, 12 * u, col, 0.45);
          const r = 20 * u, nod = 6 * u * bloom; for (let q = 0; q < 14; q++) { const a = q * 0.4488 + t * 0.05; D(tip[0] + Math.cos(a) * r * 1.25, tip[1] + nod + Math.sin(a) * r * 1.25, r * 0.42, rgba(col, 0.8)); }
          D(tip[0], tip[1] + nod, r, mix(col, '#3c281e', 0.6)); break;
        }
        case 'lantern': {
          g.strokeStyle = col; let px = x, py = foot;
          for (let s = 1; s <= 12; s++) { const f = s / 12, nx = x + (30 * u + sway) * f * f, ny = foot - h * f; g.lineWidth = 3 * u * (1 - 0.5 * f); g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke();
            if (s === 5 || s === 8 || s === 12) { const lx = nx + Math.sin(t * 1.5 + s) * 3 * u, ly = ny + 18 * u, gl = 0.3 + 0.7 * bloom, rr = (8 + s * 0.4) * u;
              const rg = g.createRadialGradient(lx, ly, 0, lx, ly, rr * 2.5); rg.addColorStop(0, `rgba(255,200,120,${0.35 * gl})`); rg.addColorStop(1, 'rgba(255,200,120,0)'); g.fillStyle = rg; g.beginPath(); g.arc(lx, ly, rr * 2.5, 0, 6.283); g.fill();
              g.fillStyle = mix(col, '#ffbe6e', gl); g.beginPath(); g.ellipse(lx, ly, rr * 0.9, rr * 1.1, 0, 0, 6.283); g.fill(); D(lx, ly, rr * 0.35, `rgba(255,240,190,${gl})`); }
            px = nx; py = ny; }
          break;
        }
        case 'lotus': {
          const cy = foot - h * 0.35, open = 0.45 + 0.55 * bloom; g.strokeStyle = col; g.lineWidth = 3 * u; g.beginPath(); g.moveTo(x, foot); g.lineTo(x, cy); g.stroke();
          g.fillStyle = rgba(mix(col, '#3c785a', 0.6), 0.8); g.beginPath(); g.ellipse(x, foot - 2 * u, 30 * u, 7 * u, 0, 0, 6.283); g.fill();
          for (let ring = 0; ring < 2; ring++) for (let q = 0; q < 7; q++) { const a = -1.57 + (q - 3) * (0.28 + 0.22 * open) * (ring ? 0.7 : 1), L = (24 - ring * 6) * u;
            g.fillStyle = rgba(mix(col, '#fffcf4', ring * 0.3), 0.7); g.beginPath(); g.ellipse(x + Math.cos(a) * L * 0.5, cy + Math.sin(a) * L * 0.5, L * 0.5, 5 * u, a, 0, 6.283); g.fill(); }
          D(x, cy - 3 * u, 4 * u, '#fffcf4'); break;
        }
        case 'spiral': {
          g.strokeStyle = col; let px = x, py = foot;
          for (let s = 1; s <= 60; s++) { const f = s / 60, a = f * 12 + t * 0.4, rad = (10 + 6 * bloom) * u * (1 - 0.5 * f), nx = x + Math.sin(a) * rad + sway * f, ny = foot - h * f - Math.cos(a) * rad * 0.3;
            g.lineWidth = (2.6 * (1 - 0.6 * f) + 0.5) * u; g.globalAlpha *= 1; g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke(); px = nx; py = ny; }
          D(px, py, (3.5 + 3 * bloom) * u, '#fffcf4'); break;
        }
        case 'crystal': {
          [[-0.45, 0.55], [-0.18, 0.8], [0, 1], [0.2, 0.7], [0.42, 0.5]].forEach(([a, l], q) => { const L = h * l, w = (7 - Math.abs(q - 2)) * u, bx = x + (q - 2) * 5 * u, ex = bx + Math.sin(a) * L, ey = foot - Math.cos(a) * L;
            g.fillStyle = rgba(mix(col, '#fffcf4', 0.15), 0.75); g.beginPath(); g.moveTo(bx - w, foot); g.lineTo(ex, ey); g.lineTo(bx + w, foot); g.closePath(); g.fill();
            if (q === 2 && bloom > 0.05) { const gl = bloom * 14 * u; g.strokeStyle = `rgba(255,252,244,${bloom})`; g.lineWidth = 1.6 * u; g.beginPath(); g.moveTo(ex - gl, ey); g.lineTo(ex + gl, ey); g.moveTo(ex, ey - gl); g.lineTo(ex, ey + gl); g.stroke(); } });
          break;
        }
        case 'orchid': {
          this.leaf(x, foot - 4, -1, 36 * u, 4 * u, col, 0.5); this.leaf(x, foot - 4, 1, 32 * u, 4 * u, col, 0.5); g.strokeStyle = col; let px = x, py = foot;
          for (let s = 1; s <= 12; s++) { const f = s / 12, nx = x + (-36 * u + sway) * f * f, ny = foot - h * (f * 1.15 - 0.25 * f * f); g.lineWidth = 2.4 * u * (1 - 0.5 * f); g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke();
            if (s >= 6 && s % 2 === 0) { const o = (0.6 + 0.4 * bloom) * u * (0.8 + 0.04 * s); for (let q = 0; q < 5; q++) { const a = q * 1.2566 + Math.sin(t * 2 + s) * 0.1; D(nx + Math.cos(a) * 9 * o, ny + Math.sin(a) * 9 * o, 6 * o, rgba(col, 0.8)); } D(nx, ny + 6 * o, 5 * o, mix(col, '#fffcf4', 0.5)); }
            px = nx; py = ny; }
          break;
        }
        default: {                                     // tulip
          tip = this.stem(x, foot, h, 6 * u, sway, 5 * u, col); this.leaf(x, foot - 10 * u, -1, h * 0.3, h * 0.55, col, 0.45); this.leaf(x, foot - 24 * u, 1, h * 0.26, h * 0.45, col, 0.4);
          const open = 0.25 + 0.75 * bloom; for (let q = -1; q <= 1; q++) { const a = q * (0.35 + 0.5 * open); D(tip[0] + Math.sin(a) * 15 * u, tip[1] - (Math.cos(a) * 15 + 5) * u, (14 + 4 * bloom) * u, rgba(col, 0.6 + 0.4 * bloom)); }
          D(tip[0], tip[1] - 6 * u, (5 + 3 * bloom) * u, '#fffcf4');
        }
      }
    }

    /* ---- the garden: six plants of the Biome's kinds over the pond, blooming on the beat ---- */
    garden(t, b, beat, energy) {
      const g = this.g, W = this.W, H = this.H, hz = H * 0.72, u = this.u, P = place(b);
      for (let i = 0; i < 6; i++) {
        const kind = SPECIES[(hash(b * 6 + i + 3) >>> 7) % SPECIES.length], col = TRACK[i];
        const step = (hash(b + i) >>> 4) % 4, hit = ((beat * 4 + 4 - step) % 4) < 0.25 ? 1 - ((beat * 4 + 4 - step) % 4) / 0.25 : 0;
        const x = W * (0.12 + i * 0.152), h = (190 + 120 * energy + ((hash(i + b) >>> 9) % 60)) * u, sway = Math.sin(t * (0.6 + 0.15 * i) + i) * (8 + 20 * energy) * u;
        this.plant(kind, x, hz - 4 * u - (i * 5 % 3) * 6 * u, h, col, t, hit, sway, 1);
        if (hit > 0.95 && Math.random() < 0.6) this.spores.push({ x, y: hz - h, vx: (Math.random() - 0.5) * 40, vy: -40 - Math.random() * 60, life: 0, c: col });
      }
      // the pond: the garden mirrored, rippling
      g.save(); g.globalAlpha = 0.28; g.translate(0, hz * 2); g.scale(1, -1);
      g.drawImage(this.cv, 0, (hz - H * 0.32) * (this.cv.height / H), this.cv.width, H * 0.32 * (this.cv.height / H), 0, hz - H * 0.32, W, H * 0.32);
      g.restore();
      for (let k = 0; k < 4; k++) {                    // ripples
        const ph = (t * 0.4 + k * 0.25) % 1, rx = W * (0.2 + 0.2 * k), ry = hz + (24 + k * 10) * u;
        g.strokeStyle = `rgba(200,225,255,${0.3 * (1 - ph)})`; g.lineWidth = 1.3 * u; g.beginPath(); g.ellipse(rx, ry, (10 + 90 * ph) * u, (2 + 20 * ph) * u, 0, 0, 6.283); g.stroke();
      }
      void P;
    }

    /* ---- the echo cave (bk_draw_space): drops, rings of echoes, mist ---- */
    cave(t, beat) {
      const g = this.g, W = this.W, H = this.H, u = this.u, cx = W / 2, cy = H * 0.66, R = Math.min(W, H) * 0.62;
      g.fillStyle = '#2c3038'; g.fillRect(0, 0, W, H);
      for (let i = 0; i < 90; i++) { const h = hash(i + 50); this.disc((h % 1000) / 1000 * W, ((h >>> 10) % 1000) / 1000 * H * 0.5, (2 + (h >>> 28)) * u, 'rgba(90,96,110,0.25)'); }
      const rg = g.createRadialGradient(cx, cy + R * 0.3, R * 0.2, cx, cy + R * 0.3, R);
      rg.addColorStop(0, '#05070b'); rg.addColorStop(0.85, '#141821'); rg.addColorStop(1, 'rgba(20,24,33,0)');
      g.fillStyle = rg; g.beginPath(); g.arc(cx, cy + R * 0.3, R, 3.1416, 6.283); g.fill();
      for (let i = 0; i < 9; i++) {                    // stalactites
        const sx = cx - R * 0.8 + i * R * 0.2, top = cy + R * 0.3 - Math.sqrt(Math.max(R * R - (sx - cx) * (sx - cx), 0)), sl = (26 + (i * 37) % 30) * u;
        g.fillStyle = '#3e424e'; g.beginPath(); g.moveTo(sx - 9 * u, top - 4 * u); g.lineTo(sx, top + sl); g.lineTo(sx + 9 * u, top - 4 * u); g.fill();
        if (((t * 0.5 + i * 0.3) % 1) < 0.08) this.disc(sx, top + sl + 4 * u, 2.5 * u, '#fffcf4');
      }
      const dsec = 60 / (+this.np.bpm || 76) / 2, cyc = Math.max(2.4, dsec * 6), ph = t % cyc, py = H * 0.8;
      g.strokeStyle = 'rgba(200,230,255,0.25)'; g.lineWidth = 1 * u; g.beginPath(); g.moveTo(cx - R * 0.7, py); g.lineTo(cx + R * 0.7, py); g.stroke();
      for (let k = 0; k < 8; k++) { const age = ph - (k * dsec + 0.35), a = k ? 0.6 * Math.pow(0.6, k - 1) : 1; if (age < 0 || age > 1.6 || a < 0.02) continue;
        g.strokeStyle = k ? `rgba(140,200,230,${a * (1 - age / 1.6)})` : `rgba(255,252,244,${1 - age / 1.6})`; g.lineWidth = 2 * u;
        g.beginPath(); g.ellipse(cx, py, (14 + age * 160) * u, (3.5 + age * 40) * u, 0, 0, 6.283); g.stroke(); }
      if (ph < 0.35) this.disc(cx, H * 0.25 + ph / 0.35 * (py - H * 0.25), 5 * u, '#fffcf4');
      for (let i = 0; i < 5; i++) { const mx = W * (0.15 + 0.18 * i) + 20 * u * Math.sin(t * 0.3 + i), my = H * 0.62 + 10 * u * Math.sin(t * 0.4 + i * 2);
        const m = g.createRadialGradient(mx, my, 0, mx, my, 120 * u); m.addColorStop(0, 'rgba(140,200,230,0.07)'); m.addColorStop(1, 'rgba(140,200,230,0)'); g.fillStyle = m; g.fillRect(mx - 120 * u, my - 120 * u, 240 * u, 240 * u); }
      void beat;
    }

    /* ---- the spore meadow: a log of moss, a mushroom reading, spores leaving ---- */
    meadow(t, b, energy) {
      const g = this.g, W = this.W, H = this.H, u = this.u, ly = H * 0.78, P = place(b), moss = mix('#78c88c', SKY[P.pal][1], 0.3);
      g.fillStyle = '#2e241e'; g.beginPath(); g.roundRect ? g.roundRect(W * 0.08, ly, W * 0.84, 40 * u, 20 * u) : g.rect(W * 0.08, ly, W * 0.84, 40 * u); g.fill();
      g.strokeStyle = rgba(moss, 0.55); g.lineWidth = 3 * u;
      for (let x = W * 0.08; x < W * 0.92; x += 5 * u) { const a = Math.abs(0.25 + 0.2 * Math.sin(x * 0.011) * Math.sin(x * 0.0027 + 1.3) + 0.12 * Math.sin(x * 0.06)) * (0.5 + energy);
        g.beginPath(); g.moveTo(x, ly); g.lineTo(x, ly - a * 70 * u); g.stroke(); }
      const pos = 0.5 + 0.35 * Math.sin(t * 0.07), ww = (120 + 80 * Math.sin(t * 0.05)) * u, mx = W * 0.1 + pos * (W * 0.8 - ww) + ww / 2, ch = 140 * u;
      g.strokeStyle = 'rgba(230,220,200,0.9)'; g.lineWidth = 12 * u; g.beginPath(); g.moveTo(mx, ly); g.lineTo(mx, ly - ch); g.stroke();
      g.fillStyle = moss; g.beginPath(); g.ellipse(mx, ly - ch, ww / 2 + 20 * u, 46 * u, 0, 3.1416, 6.283); g.fill();
      if (Math.random() < 0.3 + 0.5 * energy) this.spores.push({ x: mx + (Math.random() - 0.5) * ww, y: ly - ch, vx: (Math.random() - 0.5) * 20, vy: -30 - Math.random() * 40, life: 0, c: '#fffcf4' });
      const gx = W * 0.08 + ((t * 0.08) % 1) * W * 0.84, gr = g.createRadialGradient(gx, ly + 20 * u, 0, gx, ly + 20 * u, 20 * u);
      gr.addColorStop(0, 'rgba(220,255,170,0.9)'); gr.addColorStop(1, 'rgba(220,255,170,0)'); g.fillStyle = gr; g.fillRect(gx - 20 * u, ly, 40 * u, 40 * u);
    }

    /* ---- the stream: water flowing, leaves riding ---- */
    stream(t, energy) {
      const g = this.g, W = this.W, H = this.H, u = this.u, speed = 40 + 80 * energy;
      for (let i = 0; i < 6; i++) { const by = H * (0.6 + i * 0.06); g.strokeStyle = `rgba(90,160,210,${0.25 + 0.08 * i})`; g.lineWidth = 9 * u; g.beginPath();
        for (let x = 0; x <= W; x += 10) { const y = by + 6 * u * Math.sin((x - t * speed * u * (1 + 0.2 * i)) * 0.012 / u + i); x ? g.lineTo(x, y) : g.moveTo(x, y); } g.stroke(); }
      for (let i = 0; i < 9; i++) { const lx = ((i * 157 + t * speed * 1.2) * u) % W, ly = H * (0.62 + (i % 4) * 0.06) + 4 * u * Math.sin(t * 2 + i);
        g.fillStyle = i % 2 ? '#c8964f' : '#dcaa5a'; g.beginPath(); g.ellipse(lx, ly, 10 * u, 5 * u, Math.sin(t + i) * 0.5, 0, 6.283); g.fill(); }
    }

    /* ---- the dream map: stations as Biome signs, a wisp travelling between them ---- */
    dream(t, b) {
      const g = this.g, W = this.W, H = this.H, u = this.u, n = 12, pts = [];
      for (let i = 0; i < n; i++) { const row = Math.floor(i * 4 / n), first = Math.floor((row * n + 3) / 4), last = Math.floor(((row + 1) * n + 3) / 4) - 1, k = i - first, cnt = last - first + 1;
        let fx = cnt > 1 ? k / (cnt - 1) : 0.5; if (row & 1) fx = 1 - fx; pts.push([W * (0.18 + fx * 0.64), H * (0.2 + row * 0.17) + 10 * u * Math.sin(i * 1.3 + t * 0.4)]); }
      const pos = (t * 0.25) % (n - 1), cur = Math.floor(pos);
      for (let i = 0; i + 1 < n; i++) { g.strokeStyle = i < cur ? rgba(TRACK[4], 0.9) : 'rgba(255,255,255,0.18)'; g.lineWidth = (i < cur ? 4 : 2) * u; g.beginPath(); g.moveTo(...pts[i]); g.lineTo(...pts[i + 1]); g.stroke(); }
      for (let i = 0; i < n; i++) { this.disc(pts[i][0], pts[i][1], 22 * u, 'rgba(10,14,24,0.8)'); this.sigil(b + i * 17, pts[i][0], pts[i][1], 17 * u, TRACK[(b + i) % 6], i === cur ? t : 0); }
      for (let k = 14; k >= 0; k--) { const q = pos - k * 0.035; if (q < 0) continue; const a = Math.min(Math.floor(q), n - 2), f = q - a, x = pts[a][0] + (pts[a + 1][0] - pts[a][0]) * f, y = pts[a][1] + (pts[a + 1][1] - pts[a][1]) * f;
        if (k === 0) { const gr = g.createRadialGradient(x, y, 0, x, y, 24 * u); gr.addColorStop(0, 'rgba(255,252,244,0.9)'); gr.addColorStop(1, 'rgba(200,177,216,0)'); g.fillStyle = gr; g.fillRect(x - 24 * u, y - 24 * u, 48 * u, 48 * u); }
        else this.disc(x, y, (4.5 - k * 0.28) * u, rgba(TRACK[4], 0.7 - k * 0.04)); }
    }

    /* ---- the atlas: a slow drift of Biome signs ---- */
    atlas(t, b) {
      const W = this.W, H = this.H, u = this.u;
      for (let i = 0; i < 36; i++) { const id = (b + i * 7) % 272, h = hash(id + 99), x = ((h % 1000) / 1000 * W + t * (6 + (h >>> 28)) * u) % (W + 80 * u) - 40 * u, y = H * 0.12 + ((h >>> 10) % 1000) / 1000 * H * 0.6;
        this.disc(x, y, 30 * u, 'rgba(10,14,24,0.55)'); this.sigil(id, x, y, (16 + (h >>> 29) * 4) * u, TRACK[id % 6], t * 0.5); }
    }

    /* ---- the title: moss letters, bioluminescent, mushrooms growing (brick_splash.inc) ---- */
    title(t) {
      const g = this.g, W = this.W, H = this.H, u = this.u;
      g.save(); g.textAlign = 'center'; g.font = `800 ${96 * u}px Nunito, sans-serif`;
      ['MUTABLE', 'DREAMS'].forEach((w, row) => {
        const y = H * (0.3 + row * 0.16);
        g.shadowColor = 'rgba(150,240,200,0.8)'; g.shadowBlur = (18 + 8 * Math.sin(t * 1.5)) * u; g.fillStyle = '#1e2e28'; g.fillText(w, W / 2, y);
        g.shadowBlur = 0; g.lineWidth = 2 * u; g.strokeStyle = 'rgba(150,240,200,0.75)'; g.strokeText(w, W / 2, y);
        const ww = g.measureText(w).width;
        for (let m = 0; m < 3; m++) { const mx = W / 2 - ww / 2 + ww * (0.2 + 0.3 * m) + row * 30 * u, my = y - 70 * u, gr = Math.min(1, Math.max(0, (t - 1 - m * 0.4 - row) / 2));
          g.strokeStyle = 'rgba(230,220,200,0.9)'; g.lineWidth = 3 * u; g.beginPath(); g.moveTo(mx, my); g.lineTo(mx, my - 16 * u * gr); g.stroke();
          g.fillStyle = '#d6a078'; g.beginPath(); g.ellipse(mx, my - 16 * u * gr, 10 * u * gr, 7 * u * gr, 0, 3.1416, 6.283); g.fill(); }
      });
      g.font = `700 ${30 * u}px Nunito, sans-serif`; g.fillStyle = '#fffcf4'; g.fillText('BY KID BUNKA', W / 2, H * 0.62);
      g.restore();
    }

    /* ---- a Biome's sign (bk_biome_sigil): bloom, spiral, crystal, mushrooms, frond, star-moss ---- */
    sigil(id, cx, cy, r, c, t) {
      const g = this.g, h = hash(id + 11), kind = (h >>> 5) % 6, parts = 3 + (h >>> 9) % 6, c2 = mix(c, TRACK[(h >>> 13) % 6], 0.5), rot = ((h >>> 17) % 628) / 100 + t * 0.15;
      if (kind === 0) { for (let i = 0; i < parts; i++) { const a = rot + i * 6.283 / parts; this.disc(cx + Math.cos(a) * r * 0.5, cy + Math.sin(a) * r * 0.5, r * 0.36, rgba(c, 0.8)); } this.disc(cx, cy, r * 0.25, c2); }
      else if (kind === 1) { g.strokeStyle = c; g.lineCap = 'round'; let px = cx, py = cy; for (let i = 1; i <= 30; i++) { const v = i / 30, a = rot + v * parts * 1.8, nx = cx + Math.cos(a) * v * r, ny = cy + Math.sin(a) * v * r; g.lineWidth = 2.2 * (1 - 0.5 * v) * this.u; g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke(); px = nx; py = ny; } this.disc(px, py, r * 0.15, c2); }
      else if (kind === 2) { for (let i = 0; i < parts; i++) { const a = -1.57 + (i - (parts - 1) / 2) * 0.35, L = r * (0.6 + 0.4 * ((h >>> (i + 3)) & 1)); g.fillStyle = rgba(i & 1 ? c2 : c, 0.82); g.beginPath(); g.moveTo(cx - r * 0.12, cy + r * 0.6); g.lineTo(cx + Math.cos(a) * L, cy + r * 0.6 + Math.sin(a) * L * 1.4); g.lineTo(cx + r * 0.12, cy + r * 0.6); g.fill(); } }
      else if (kind === 3) { for (let i = 0; i < (parts > 4 ? 3 : 2); i++) { const mx = cx + (i - 1) * r * 0.55, hh = r * (0.5 + 0.3 * ((h >>> (i * 2)) & 1)), cr = r * (0.42 - 0.08 * i); g.strokeStyle = 'rgba(230,220,200,0.9)'; g.lineWidth = 2 * this.u; g.beginPath(); g.moveTo(mx, cy + r * 0.7); g.lineTo(mx, cy + r * 0.7 - hh); g.stroke(); g.fillStyle = i & 1 ? c2 : c; g.beginPath(); g.ellipse(mx, cy + r * 0.7 - hh, cr, cr * 0.7, 0, 3.1416, 6.283); g.fill(); } }
      else if (kind === 4) { g.strokeStyle = c; let px = cx - r * 0.2, py = cy + r; for (let i = 1; i <= 8; i++) { const v = i / 8, nx = cx - r * 0.2 + Math.sin(v * 2 + rot * 0.2) * r * 0.5, ny = cy + r - v * r * 1.9, k = r * 0.35 * (1 - v * 0.6); g.lineWidth = 2 * this.u; g.beginPath(); g.moveTo(px, py); g.lineTo(nx, ny); g.stroke(); this.disc(nx - k, ny + 2, k * 0.45, rgba(c2, 0.8)); this.disc(nx + k, ny + 2, k * 0.45, rgba(c2, 0.8)); px = nx; py = ny; } }
      else { g.strokeStyle = rgba(c, 0.85); g.lineWidth = 1.6 * this.u; for (let i = 0; i < parts * 2; i++) { const a = rot + i * 3.1416 / parts, L = r * (i & 1 ? 0.55 : 0.95); g.beginPath(); g.moveTo(cx, cy); g.lineTo(cx + Math.cos(a) * L, cy + Math.sin(a) * L); g.stroke(); this.disc(cx + Math.cos(a) * L, cy + Math.sin(a) * L, i & 1 ? 1.8 : 2.6, c2); } this.disc(cx, cy, r * 0.2, c); }
    }

    /* ---- the air: spores and fireflies over everything, a breathing aura (bk_ambient) ---- */
    ambient(t, b, energy) {
      const W = this.W, H = this.H, u = this.u, a1 = TRACK[b % 6], a2 = TRACK[Math.floor(b / 6) % 6];
      for (let i = 0; i < 60; i++) { const h = hash(i + 1), sp = 0.012 + (h >>> 27) * 0.003 + energy * 0.03, life = (t * sp + (h & 1023) / 1023) % 1;
        const x = (h % 10000) / 10000 * W + 22 * u * Math.sin(t * 0.5 + i), y = H * (1 - life), f = Math.sin(life * 3.1416), c = i & 1 ? a1 : a2;
        this.disc(x, y, 4 * u, rgba(c, 0.09 * f)); this.disc(x, y, 1.5 * u, rgba(mix(c, '#fffcf4', 0.5), 0.7 * f)); }
      for (let i = this.spores.length - 1; i >= 0; i--) { const s = this.spores[i]; s.life += 0.012; s.x += s.vx * 0.016 * u; s.y += s.vy * 0.016 * u; s.vy += 0.3;
        this.disc(s.x, s.y, 2.4 * u, rgba(s.c, 1 - s.life)); if (s.life >= 1) this.spores.splice(i, 1); }
      if (this.spores.length > 200) this.spores.splice(0, this.spores.length - 200);
    }

    /* ---- the lower third: the Biome's sign, what plays now, Mutable Dreams by Kid Bunka ---- */
    lower(t, b) {
      const g = this.g, W = this.W, H = this.H, u = this.u, np = this.np, col = TRACK[b % 6];
      const x = 40 * u, y = H - 130 * u, w = Math.min(W - 80 * u, 760 * u), hh = 96 * u;
      const gr = g.createLinearGradient(x, 0, x + w, 0); gr.addColorStop(0, 'rgba(6,10,18,0.85)'); gr.addColorStop(1, 'rgba(6,10,18,0)');
      g.fillStyle = gr; g.beginPath(); g.roundRect ? g.roundRect(x, y, w, hh, 48 * u) : g.rect(x, y, w, hh); g.fill();
      g.strokeStyle = rgba(col, 0.5 + 0.2 * Math.sin(t * 1.5)); g.lineWidth = 2 * u; g.beginPath(); g.moveTo(x + 48 * u, y); g.lineTo(x + w * 0.7, y); g.stroke();
      this.disc(x + 48 * u, y + hh / 2, 34 * u, 'rgba(10,14,24,0.9)'); this.sigil(b, x + 48 * u, y + hh / 2, 24 * u, col, t);
      g.textBaseline = 'alphabetic'; g.textAlign = 'left';
      const line1 = np.title ? np.title + (np.artist ? '  ·  ' + np.artist : '') : (np.show || 'Mutable Dreams');
      g.font = `800 ${30 * u}px Nunito, sans-serif`; g.fillStyle = '#fffcf4'; g.shadowColor = rgba(col, 0.6); g.shadowBlur = 12 * u;
      g.fillText(line1, x + 100 * u, y + 44 * u); g.shadowBlur = 0;
      g.font = `600 ${18 * u}px "Barlow SC", "Barlow Semi Condensed", sans-serif`; g.fillStyle = rgba(col, 0.95);
      g.fillText(((np.show && np.title ? np.show + '  ·  ' : '') + 'MUTABLE DREAMS  ·  KID BUNKA').toUpperCase(), x + 100 * u, y + 72 * u);
    }
  }

  const sigilPainter = (g) => ({ g, u: 1, disc: Kit.prototype.disc });
  window.MDKit = { start: (canvas, o) => new Kit(canvas, o || {}), SPECIES, TRACK,
    sigil: (g, id, cx, cy, r, c, t) => Kit.prototype.sigil.call(sigilPainter(g), id, cx, cy, r, c, t || 0) };
})();
