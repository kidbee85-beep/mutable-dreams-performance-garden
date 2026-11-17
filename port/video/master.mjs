// The videos' master (from Codex's sound review, 16.11.2026): BS.1770 integrated loudness to -16 LUFS, a reconstructed
// true peak under -1 dBTP, smooth fades; no limiter, no clipping. Shared by make_demo.mjs (make_tour.mjs keeps its own copy).
export function master(raw, rate, seconds, soundFrom = 0) {
// Offline master: K-weighted BS.1770 stereo integrated loudness, 400 ms blocks / 100 ms hop,
  // absolute -70 LUFS and relative -10 LU gates. Never infer LUFS from unweighted RMS.
  function integratedLoudness(a) {
    const shelf = (f, g, q) => {
      const k = Math.tan(Math.PI * f / rate), vh = 10 ** (g / 20), vb = vh ** .4996667741545416;
      const d = 1 + k / q + k * k;
      return [(vh + vb * k / q + k * k) / d, 2 * (k * k - vh) / d,
        (vh - vb * k / q + k * k) / d, 2 * (k * k - 1) / d, (1 - k / q + k * k) / d];
    };
    const k = Math.tan(Math.PI * 38.13547087602444 / rate), q = .5003270373238773, d = 1 + k / q + k * k;
    const filters = [shelf(1681.974450955533, 3.999843853973347, .7071752369554196),
      [1, -2, 1, 2 * (k * k - 1) / d, (1 - k / q + k * k) / d]];
    const state = Array.from({length: 2}, () => filters.map(() => [0, 0]));
    const bins = [], hop = Math.round(.1 * rate); let power = 0, n = 0;
    for (let i = 0; i < a.length; i += 2) {
      for (let c = 0; c < 2; c++) {
        let y = a[i + c];
        for (let j = 0; j < filters.length; j++) {
          const [b0, b1, b2, a1, a2] = filters[j], z = state[c][j], v = b0 * y + z[0];
          z[0] = b1 * y - a1 * v + z[1]; z[1] = b2 * y - a2 * v; y = v;
        }
        power += y * y;
      }
      if (++n === hop) { bins.push(power / hop); n = 0; power = 0; }
    }
    const blocks = []; for (let i = 0; i + 3 < bins.length; i++) blocks.push((bins[i] + bins[i + 1] + bins[i + 2] + bins[i + 3]) / 4);
    const level = p => -.691 + 10 * Math.log10(p);
    const mean = ps => ps.reduce((s, p) => s + p, 0) / Math.max(1, ps.length);
    const absolute = blocks.filter(p => level(p) > -70), gate = level(mean(absolute)) - 10;
    return level(mean(absolute.filter(p => level(p) > gate)));
  }
  // Four-times polyphase reconstruction, 64-tap Blackman-windowed sinc; both channels.
  // Leave 0.25 dB margin under -1 dBTP, and verify the delivered PCM independently at 8x.
  function reconstructedPeak(a) {
    let peak = 0; for (const v of a) { if (!Number.isFinite(v)) throw new Error("Non-finite firmware audio"); peak = Math.max(peak, Math.abs(v)); }
    const taps = 64, frames = a.length / 2;
    for (let phase = 1; phase < 4; phase++) {
      const h = new Float64Array(taps); let sum = 0;
      for (let j = 0; j < taps; j++) {
        const x = j - taps / 2 + 1 - phase / 4;
        h[j] = Math.sin(Math.PI * x) / (Math.PI * x) * (.42 + .5 * Math.cos(2 * Math.PI * x / taps) + .08 * Math.cos(4 * Math.PI * x / taps));
        sum += h[j];
      }
      for (let j = 0; j < taps; j++) h[j] /= sum;
      for (let f = 0; f < frames; f++) {
        let l = 0, r = 0;
        for (let j = 0; j < taps; j++) {
          const k = f + j - taps / 2 + 1;
          if (k >= 0 && k < frames) { l += a[2 * k] * h[j]; r += a[2 * k + 1] * h[j]; }
        }
        peak = Math.max(peak, Math.abs(l), Math.abs(r));
      }
    }
    return peak;
  }
  
  for (let f = 0; f < raw.length / 2; f++) {
    const second = f / rate;
    const fade = Math.max(0, Math.min(1, (second - soundFrom) / 1, (seconds - second) / 3.5));
    const smooth = fade * fade * (3 - 2 * fade);
    raw[2 * f] *= smooth; raw[2 * f + 1] *= smooth;
  }
  const before = integratedLoudness(raw), peak = reconstructedPeak(raw);
  const gain = Number.isFinite(before) ? Math.min(10 ** ((-16 - before) / 20), 10 ** (-1.25 / 20) / Math.max(peak, 1e-12)) : 1;
  const pcm = new Int16Array(raw.length);
  for (let i = 0; i < raw.length; i++) {
    const sample = raw[i] * gain;
    if (!Number.isFinite(sample) || Math.abs(sample) >= 1) throw new Error("Invalid master: clipping");
    pcm[i] = Math.round(sample * 32767);
  }
  return {pcm, lufs: before + 20 * Math.log10(gain), dbtp: 20 * Math.log10(peak * gain), gainDb: 20 * Math.log10(gain)};
}
