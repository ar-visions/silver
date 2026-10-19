// fonts through trinity (the addon): system matching, metrics,
// glyphs with fallback per code point, @font-face from bytes
import { readFileSync } from 'node:fs';
const GENERIC = { 'system-ui': 'sans-serif', '-apple-system': 'sans-serif', blinkmacsystemfont: 'sans-serif', 'segoe ui': 'sans-serif', 'segoe wpc': 'sans-serif', 'ui-monospace': 'monospace', 'ui-sans-serif': 'sans-serif', 'ui-serif': 'serif', 'ui-rounded': 'sans-serif', 'droid sans mono': 'monospace', menlo: 'monospace', consolas: 'monospace', 'courier new': 'monospace', 'helvetica neue': 'sans-serif', helvetica: 'sans-serif', arial: 'sans-serif', roboto: 'sans-serif', 'noto sans': 'sans-serif', 'dejavu sans mono': 'monospace', 'dejavu sans': 'sans-serif', ubuntu: 'sans-serif', cantarell: 'sans-serif', 'fira code': 'monospace', 'source code pro': 'monospace', 'sf mono': 'monospace', 'sfmono-regular': 'monospace', 'liberation mono': 'monospace', 'liberation sans': 'sans-serif', 'courier': 'monospace', 'monaco': 'monospace', 'inter': 'sans-serif', 'apple color emoji': 'emoji', 'segoe ui emoji': 'emoji', 'noto color emoji': 'emoji', 'segoe ui symbol': 'sans-serif', 'codicon': 'codicon', 'seti': 'seti' };
export class Fonts {
  constructor(gfx, log) {
    this.gfx = gfx; this.log = log;
    this.faces = new Map();      // key family|weight|italic -> Face
    this.files = new Map();      // file path|index -> id
    this.webFaces = [];          // @font-face: { family, weight, italic, id, ranges }
    this.pending = new Map();    // family -> promise
    this.fallbackCache = new Map(); // cp|weight|italic -> Face
  }
  // a loaded font file as an id
  openFile(file, index = 0) {
    const key = file + '|' + index;
    if (this.files.has(key)) return this.files.get(key);
    let id = 0;
    try { id = this.gfx.fontOpen(readFileSync(file), index); } catch (e) { this.log(`font ${file}: ${e.message}`); }
    this.files.set(key, id);
    return id;
  }
  face(id, family) { return new Face(this, id, family); }
  // @font-face given as bytes (fetched by the page)
  addWebFont(family, weight, italic, bytes, ranges) {
    const id = this.gfx.fontOpen(bytes, 0);
    if (!id) return false;
    this.webFaces.push({ family: family.toLowerCase(), weight: weight || 400, italic: !!italic, id, ranges: ranges || null, face: this.face(id, family) });
    this.faces.clear(); this.fallbackCache.clear();
    return true;
  }
  hasWebFont(family) { return this.webFaces.some(f => f.family === family.toLowerCase()); }
  // the face for one family of a font-family list; null when none
  resolveFamily(family, weight, italic) {
    const key = `${family.toLowerCase()}|${weight}|${italic ? 1 : 0}`;
    if (this.faces.has(key)) return this.faces.get(key);
    let face = null;
    const lower = family.toLowerCase();
    const web = this.webFaces.filter(f => f.family === lower);
    if (web.length) {
      let best = web[0], bestD = 1e9;
      for (const f of web) { const d = Math.abs(f.weight - weight) + (f.italic !== !!italic ? 1000 : 0); if (d < bestD) { bestD = d; best = f; } }
      face = best.face;
    } else {
      const generic = GENERIC[lower] || (['serif', 'sans-serif', 'monospace', 'cursive', 'fantasy', 'emoji', 'math', 'fangsong'].includes(lower) ? lower : null);
      const ask = generic || family;
      const m = this.gfx.fontMatch(ask, weight, 100, italic ? 100 : 0, 0, generic === 'emoji');
      // a named family only counts when fontconfig gave that family
      if (m && (generic || m.family.toLowerCase().includes(lower.split(' ')[0]) || lower.includes(m.family.toLowerCase().split(' ')[0]))) {
        const id = this.openFile(m.file, m.index);
        if (id) face = this.face(id, m.family);
      }
    }
    this.faces.set(key, face);
    return face;
  }
  // the first family that exists; the default sans when none
  primary(families, weight, italic) {
    for (const f of families) { const face = this.resolveFamily(f, weight, italic); if (face) return face; }
    return this.resolveFamily('sans-serif', weight, italic) || this.resolveFamily('serif', weight, italic);
  }
  // a face holding this code point: the list first, then the system
  faceFor(cp, families, weight, italic, primaryFace) {
    if (primaryFace && primaryFace.glyph(cp)) return primaryFace;
    for (const f of families) { const face = this.resolveFamily(f, weight, italic); if (face && face !== primaryFace && face.glyph(cp)) return face; }
    const key = `${cp}|${weight}|${italic ? 1 : 0}`;
    if (this.fallbackCache.has(key)) return this.fallbackCache.get(key) || primaryFace;
    let face = null;
    const emoji = cp >= 0x1f000 || (cp >= 0x2600 && cp <= 0x27bf);
    const m = this.gfx.fontMatch(null, weight, 100, italic ? 100 : 0, cp, emoji);
    if (m) { const id = this.openFile(m.file, m.index); if (id) { face = this.face(id, m.family); if (!face.glyph(cp)) face = null; } }
    this.fallbackCache.set(key, face);
    return face || primaryFace;
  }
}
export class Face {
  constructor(fonts, id, family) { this.fonts = fonts; this.gfx = fonts.gfx; this.id = id; this.family = family; this.upem = this.gfx.fontUnitsPerEm(id) || 1000; this.glyphs = new Map(); this.advances = new Map(); this.metricsCache = new Map(); }
  glyph(cp) { let g = this.glyphs.get(cp); if (g === undefined) { g = this.gfx.fontGlyph(this.id, cp) >>> 0; this.glyphs.set(cp, g); } return g; }
  // advance in units; scaled by px / upem
  advanceUnits(g) { let a = this.advances.get(g); if (a === undefined) { a = this.gfx.fontAdvance(this.id, g, this.upem); this.advances.set(g, a); } return a; }
  metrics(px) { let m = this.metricsCache.get(px); if (!m) { const r = this.gfx.fontMetrics(this.id, px); m = { ascent: r[0], descent: r[1], xHeight: r[2], zero: r[3] }; this.metricsCache.set(px, m); } return m; }
}
