// PNG into rgba8 (zlib is node's); the formats a page's icons use
import { inflateSync } from 'node:zlib';
export function decodePNG(buf) {
  if (buf.length < 8 || buf[0] !== 0x89 || buf[1] !== 0x50) return null;
  let i = 8, w = 0, h = 0, depth = 8, type = 6, interlace = 0, palette = null, trns = null;
  const idat = [];
  while (i + 8 <= buf.length) {
    const len = buf.readUInt32BE(i); const kind = buf.toString('latin1', i + 4, i + 8); const data = buf.subarray(i + 8, i + 8 + len);
    if (kind === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); depth = data[8]; type = data[9]; interlace = data[12]; }
    else if (kind === 'PLTE') palette = data;
    else if (kind === 'tRNS') trns = data;
    else if (kind === 'IDAT') idat.push(data);
    else if (kind === 'IEND') break;
    i += 12 + len;
  }
  if (!w || !h || interlace) return null;
  const raw = inflateSync(Buffer.concat(idat));
  const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[type];
  const bpp = Math.max(1, (channels * depth) >> 3);
  const stride = Math.ceil(w * channels * depth / 8);
  const out = new Uint8Array(w * h * 4);
  let prev = new Uint8Array(stride), cur = new Uint8Array(stride), p = 0;
  for (let y = 0; y < h; y++) {
    const f = raw[p++];
    for (let x = 0; x < stride; x++) {
      const a = x >= bpp ? cur[x - bpp] : 0, b = prev[x], c = x >= bpp ? prev[x - bpp] : 0, v = raw[p++];
      let r;
      switch (f) { case 0: r = v; break; case 1: r = v + a; break; case 2: r = v + b; break; case 3: r = v + ((a + b) >> 1); break; case 4: { const q = a + b - c, pa = Math.abs(q - a), pb = Math.abs(q - b), pc = Math.abs(q - c); r = v + (pa <= pb && pa <= pc ? a : pb <= pc ? b : c); break; } default: r = v; }
      cur[x] = r & 255;
    }
    const row = y * w * 4;
    for (let x = 0; x < w; x++) {
      let r = 0, g = 0, b = 0, a = 255;
      if (depth === 8) {
        const o = x * channels;
        if (type === 6) { r = cur[o]; g = cur[o + 1]; b = cur[o + 2]; a = cur[o + 3]; }
        else if (type === 2) { r = cur[o]; g = cur[o + 1]; b = cur[o + 2]; if (trns && trns.length >= 6 && r === trns[1] && g === trns[3] && b === trns[5]) a = 0; }
        else if (type === 0) { r = g = b = cur[o]; if (trns && trns.length >= 2 && r === trns[1]) a = 0; }
        else if (type === 4) { r = g = b = cur[o]; a = cur[o + 1]; }
        else if (type === 3) { const k = cur[o]; r = palette[k * 3]; g = palette[k * 3 + 1]; b = palette[k * 3 + 2]; a = trns && k < trns.length ? trns[k] : 255; }
      } else if (depth === 16) {
        const o = x * channels * 2;
        if (type === 6) { r = cur[o]; g = cur[o + 2]; b = cur[o + 4]; a = cur[o + 6]; } else if (type === 2) { r = cur[o]; g = cur[o + 2]; b = cur[o + 4]; } else if (type === 0) { r = g = b = cur[o]; } else if (type === 4) { r = g = b = cur[o]; a = cur[o + 2]; }
      } else {
        const bit = x * depth, byte = cur[bit >> 3], shift = 8 - depth - (bit & 7), v = (byte >> shift) & ((1 << depth) - 1);
        if (type === 3) { r = palette[v * 3]; g = palette[v * 3 + 1]; b = palette[v * 3 + 2]; a = trns && v < trns.length ? trns[v] : 255; }
        else { r = g = b = v * 255 / ((1 << depth) - 1); }
      }
      out[row + x * 4] = r; out[row + x * 4 + 1] = g; out[row + x * 4 + 2] = b; out[row + x * 4 + 3] = a;
    }
    [prev, cur] = [cur, prev];
  }
  return { width: w, height: h, data: out };
}
