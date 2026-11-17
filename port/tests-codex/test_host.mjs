// Minimal VIBE host for the local tests: the store (SDK 3/4) in a Map, logs and requests ignored.
// files: a Map name -> Uint8Array that survives between instances (a restart keeps the saved state).
export function instantiate(module, files = new Map(), log = null, media = null, device = null) {
  let inst = null;
  const mem = () => inst.exports.memory.buffer;
  const name = (p, n) => Buffer.from(mem(), p, n).toString();
  inst = new WebAssembly.Instance(module, {env: {
    vibe_host_log: (p, n) => { if (log) log.push(name(p, n)); },
    vibe_host_request: () => {},
    vibe_host_store: (op, np, nl, dp, len) => {
      if (op === 4) {
        const text = Buffer.from([...files.keys()].sort().map(k => k + "\n").join("")).subarray(0, len);
        new Uint8Array(mem(), dp, text.length).set(text);
        return text.length;
      }
      if (op === 14) {                                   // SDK 19/20: the device, e.g. {pads: 64, cols: 8, rows: 8}
        if (!device) return -1;
        const b = new Uint8Array(mem(), dp, len); b.fill(0);
        b[0] = 25; b[1] = device.pads; b[6] = device.cols; b[7] = device.rows;
        new DataView(mem()).setUint16(dp + 16, 480, true); new DataView(mem()).setUint16(dp + 18, 320, true);
        b.set(Buffer.from("TEST GRID"), 20);
        return 1;
      }
      if (op > 5) return media ? media.store(op, mem(), np, nl, dp, len) : -1;
      const key = name(np, nl), f = files.get(key);
      if (op === 0) { if (!f) return -1; const n = Math.min(len, f.length); new Uint8Array(mem(), dp, n).set(f.subarray(0, n)); return n; }
      if (op === 1) { files.set(key, new Uint8Array(new Uint8Array(mem(), dp, len))); return 0; }
      if (op === 2) return f ? f.length : -1;
      if (op === 3) return files.delete(key) ? 0 : -1;
      return -1;
    },
  }});
  return inst;
}
