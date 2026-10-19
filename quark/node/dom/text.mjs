// text as glyph runs: shaping by code point with font fallback
export class TextShaper {
  constructor(fonts) { this.fonts = fonts; this.cache = new Map(); }
  // spec: { families, weight, italic, size, face (primary), letterSpacing }
  spec(cs) {
    // a computed style never changes: its spec is kept on it
    if (cs._spec) return cs._spec;
    const families = cs.fontFamilies, weight = cs.fontWeight, italic = cs.italic, size = cs.fontSize;
    const key = families.join(',') + '|' + weight + '|' + (italic ? 1 : 0) + '|' + size;
    let s = this.cache.get(key);
    if (!s) {
      const face = this.fonts.primary(families, weight, italic);
      s = { families, weight, italic, size, face, key };
      this.cache.set(key, s);
    }
    const ls = cs.get('letter-spacing');
    if (ls !== 'normal') { const n = parseFloat(ls) || 0; if (n) s = { ...s, letterSpacing: n }; }
    cs._spec = s;
    return s;
  }
  // a run: pieces per face, each with glyph ids and pen offsets
  shape(text, spec) {
    const pieces = [];
    let cur = null, x = 0;
    const ls = spec.letterSpacing || 0;
    for (const ch of text) {
      const cp = ch.codePointAt(0);
      if (cp === 0x200b || cp === 0xfeff || cp === 0x200c || cp === 0x200d) continue;
      let face = spec.face;
      let g = face ? face.glyph(cp) : 0;
      if (!g && cp !== 32) { face = this.fonts.faceFor(cp, spec.families, spec.weight, spec.italic, spec.face); g = face ? face.glyph(cp) : 0; }
      if (!face) continue;
      const adv = cp === 9 ? (face.advanceUnits(face.glyph(32)) * spec.size / face.upem) * 8 : (face.advanceUnits(g) * spec.size / face.upem);
      if (!cur || cur.face !== face) { cur = { face, ids: [], xs: [], start: x }; pieces.push(cur); }
      cur.ids.push(g); cur.xs.push(x);
      x += adv + ls;
    }
    return { pieces, width: x, text };
  }
  // the advance of one code point in the spec's primary face
  measure(text, spec) { return this.shape(text, spec).width; }
  draw(canvas, run, x, y, color, gfx) {
    gfx = gfx || this.fonts.gfx;
    const c = new Float32Array([color[0] / 255, color[1] / 255, color[2] / 255, color[3]]);
    for (const p of run.pieces) {
      const n = p.ids.length;
      if (!n) continue;
      const ids = new Uint32Array(p.ids), xs = new Float32Array(n), ys = new Float32Array(n);
      for (let i = 0; i < n; i++) { xs[i] = x + p.xs[i]; ys[i] = y; }
      gfx.canvasGlyphs(canvas, p.face.id, run.size || p.size || 0, ids, xs, ys, n, c);
    }
  }
}
