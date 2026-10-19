// CanvasRenderingContext2D on a webgfx canvas; ImageData as pixels
import { parseColor } from './css.mjs';
const F = a => new Float32Array(a);
function rgba(c) { return F([c[0] / 255, c[1] / 255, c[2] / 255, c[3]]); }
export class ImageData {
  constructor(a, b, c) { if (typeof a === 'number') { this.width = a; this.height = b; this.data = new Uint8ClampedArray(a * b * 4); } else { this.data = a; this.width = b; this.height = c === undefined ? a.length / 4 / b : c; } this.colorSpace = 'srgb'; }
}
export class Path2D {
  constructor(src) { this.cmds = []; if (src instanceof Path2D) this.cmds = src.cmds.slice(); else if (typeof src === 'string') this.cmds.push(['svg', src]); }
  moveTo(x, y) { this.cmds.push(['M', x, y]); } lineTo(x, y) { this.cmds.push(['L', x, y]); } closePath() { this.cmds.push(['Z']); }
  bezierCurveTo(a, b, c, d, e, f) { this.cmds.push(['C', a, b, c, d, e, f]); } quadraticCurveTo(a, b, c, d) { this.cmds.push(['Q', a, b, c, d]); }
  rect(x, y, w, h) { this.cmds.push(['M', x, y], ['L', x + w, y], ['L', x + w, y + h], ['L', x, y + h], ['Z']); }
  roundRect(x, y, w, h, r) { const rr = Array.isArray(r) ? r[0] : (r || 0); this.cmds.push(['RR', x, y, w, h, Math.min(rr, w / 2, h / 2)]); }
  arc(x, y, r, a0, a1, ccw) { this.cmds.push(['A', x, y, r, a0, a1, !!ccw]); }
  ellipse(x, y, rx, ry, rot, a0, a1, ccw) { this.cmds.push(['E', x, y, rx, ry, rot, a0, a1, !!ccw]); }
  arcTo(x1, y1, x2, y2, r) { this.cmds.push(['L', x1, y1]); }
  addPath(p) { this.cmds.push(...p.cmds); }
}
export class CanvasRenderingContext2D {
  constructor(page, el) {
    this.page = page; this.gfx = page.gfx; this.canvas = el; this._id = 0; this.w = 0; this.h = 0;
    this._reset();
  }
  get id() {
    if (!this._id) { const w = Math.max(1, this.canvas.width), h = Math.max(1, this.canvas.height); this._id = this.gfx.canvasNew(w, h); this.w = w; this.h = h; const st = globalThis.__quarkCanvasStats; if (st) { st.made++; st.live++; st.sizes.push('c2d' + w + 'x' + h); } }
    return this._id;
  }
  _reset() {
    const w = Math.max(1, this.canvas.width), h = Math.max(1, this.canvas.height);
    const st = globalThis.__quarkCanvasStats;
    // a canvas is made at its first draw: a new element's 300x150
    // default is set to its real size before anything is drawn
    if (this._id && (w !== this.w || h !== this.h)) { this.gfx.canvasFree(this._id); this._id = 0; if (st) { st.freed++; st.live--; } }
    else if (this._id) this.gfx.canvasClear(this._id, F([0, 0, 0, 0]));
    this.stack = []; this.path = new Path2D();
    this.fillStyle = '#000000'; this.strokeStyle = '#000000'; this.lineWidth = 1; this.lineCap = 'butt'; this.lineJoin = 'miter'; this.miterLimit = 10;
    this.globalAlpha = 1; this.globalCompositeOperation = 'source-over'; this.font = '10px sans-serif'; this.textAlign = 'start'; this.textBaseline = 'alphabetic'; this.direction = 'ltr';
    this.shadowBlur = 0; this.shadowColor = 'rgba(0,0,0,0)'; this.shadowOffsetX = 0; this.shadowOffsetY = 0; this.imageSmoothingEnabled = true; this.imageSmoothingQuality = 'low'; this.filter = 'none'; this.letterSpacing = '0px'; this.fontKerning = 'auto';
    this.m = [1, 0, 0, 1, 0, 0]; this._dashes = [];
    this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null);
  }
  _color(style) { if (typeof style !== 'string') return [0, 0, 0, 1]; const c = parseColor(style) || [0, 0, 0, 1]; c[3] *= this.globalAlpha; return c; }
  save() { this.stack.push({ fillStyle: this.fillStyle, strokeStyle: this.strokeStyle, lineWidth: this.lineWidth, globalAlpha: this.globalAlpha, font: this.font, m: this.m.slice(), textAlign: this.textAlign, textBaseline: this.textBaseline, lineCap: this.lineCap }); this.gfx.canvasSave(this.id); }
  restore() { const s = this.stack.pop(); if (s) Object.assign(this, s); this.gfx.canvasRestore(this.id); this._applyTransform(); }
  _applyTransform() { this.gfx.canvasTransform(this.id, F(this.m)); }
  setTransform(a, b, c, d, e, f) { if (typeof a === 'object') { ({ a, b, c, d, e, f } = a); } this.m = [a, b, c, d, e, f]; this._applyTransform(); }
  resetTransform() { this.setTransform(1, 0, 0, 1, 0, 0); }
  getTransform() { const [a, b, c, d, e, f] = this.m; return { a, b, c, d, e, f, is2D: true, isIdentity: a === 1 && b === 0 && c === 0 && d === 1 && e === 0 && f === 0 }; }
  transform(a, b, c, d, e, f) { const m = this.m; this.m = [m[0] * a + m[2] * b, m[1] * a + m[3] * b, m[0] * c + m[2] * d, m[1] * c + m[3] * d, m[0] * e + m[2] * f + m[4], m[1] * e + m[3] * f + m[5]]; this._applyTransform(); }
  translate(x, y) { this.transform(1, 0, 0, 1, x, y); } scale(x, y) { this.transform(x, 0, 0, y, 0, 0); } rotate(r) { const c = Math.cos(r), s = Math.sin(r); this.transform(c, s, -s, c, 0, 0); }
  beginPath() { this.path = new Path2D(); } closePath() { this.path.closePath(); }
  moveTo(x, y) { this.path.moveTo(x, y); } lineTo(x, y) { this.path.lineTo(x, y); } bezierCurveTo(...a) { this.path.bezierCurveTo(...a); } quadraticCurveTo(...a) { this.path.quadraticCurveTo(...a); }
  rect(...a) { this.path.rect(...a); } roundRect(...a) { this.path.roundRect(...a); } arc(...a) { this.path.arc(...a); } ellipse(...a) { this.path.ellipse(...a); } arcTo(...a) { this.path.arcTo(...a); }
  _build(p) {
    const pid = this.gfx.pathNew();
    let cx = 0, cy = 0;
    for (const c of p.cmds) {
      switch (c[0]) {
      case 'M': this.gfx.pathMoveTo(pid, c[1], c[2]); cx = c[1]; cy = c[2]; break;
      case 'L': this.gfx.pathLineTo(pid, c[1], c[2]); cx = c[1]; cy = c[2]; break;
      case 'C': this.gfx.pathCubicTo(pid, c[1], c[2], c[3], c[4], c[5], c[6]); cx = c[5]; cy = c[6]; break;
      case 'Q': this.gfx.pathQuadTo(pid, c[1], c[2], c[3], c[4]); cx = c[3]; cy = c[4]; break;
      case 'Z': this.gfx.pathClose(pid); break;
      case 'RR': { const [, x, y, w, h, r] = c; this.gfx.pathMoveTo(pid, x + r, y); this.gfx.pathLineTo(pid, x + w - r, y); this.gfx.pathArcTo(pid, x + w, y + r, r, r, 0, false, true); this.gfx.pathLineTo(pid, x + w, y + h - r); this.gfx.pathArcTo(pid, x + w - r, y + h, r, r, 0, false, true); this.gfx.pathLineTo(pid, x + r, y + h); this.gfx.pathArcTo(pid, x, y + h - r, r, r, 0, false, true); this.gfx.pathLineTo(pid, x, y + r); this.gfx.pathArcTo(pid, x + r, y, r, r, 0, false, true); this.gfx.pathClose(pid); break; }
      case 'A': case 'E': { const [kind, x, y, rx0, ry0, rot0, a0v, a1v, ccwv] = c; const rx = rx0, ry = kind === 'A' ? rx0 : ry0; let a0 = kind === 'A' ? ry0 : a0v, a1 = kind === 'A' ? rot0 : a1v; const ccw = kind === 'A' ? a0v : ccwv;
        let sweep = a1 - a0; if (ccw) { if (sweep > 0) sweep -= 2 * Math.PI * Math.ceil(sweep / (2 * Math.PI)); if (sweep <= -2 * Math.PI) sweep = -2 * Math.PI; } else { if (sweep < 0) sweep += 2 * Math.PI * Math.ceil(-sweep / (2 * Math.PI)); if (sweep >= 2 * Math.PI) sweep = 2 * Math.PI; }
        const segs = Math.max(1, Math.ceil(Math.abs(sweep) / (Math.PI / 2)));
        let sx = x + rx * Math.cos(a0), sy = y + ry * Math.sin(a0);
        if (p.cmds.indexOf(c) === 0 || true) { if (cx === 0 && cy === 0 && p.cmds.indexOf(c) === 0) this.gfx.pathMoveTo(pid, sx, sy); else this.gfx.pathLineTo(pid, sx, sy); }
        for (let s = 1; s <= segs; s++) { const a = a0 + sweep * s / segs; const ex = x + rx * Math.cos(a), ey = y + ry * Math.sin(a); this.gfx.pathArcTo(pid, ex, ey, rx, ry, 0, false, sweep > 0); }
        cx = x + rx * Math.cos(a1); cy = y + ry * Math.sin(a1); break; }
      }
    }
    return pid;
  }
  fill(a, b) { const p = a instanceof Path2D ? a : this.path; const rule = (a instanceof Path2D ? b : a) === 'evenodd'; const pid = this._build(p); this.gfx.canvasFillPath(this.id, pid, rgba(this._color(this.fillStyle)), rule); this.gfx.pathFree(pid); this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null); }
  stroke(a) { const p = a instanceof Path2D ? a : this.path; const pid = this._build(p); this.gfx.canvasStrokePath(this.id, pid, rgba(this._color(this.strokeStyle)), this.lineWidth, this.lineCap === 'round' ? 1 : this.lineCap === 'square' ? 2 : 0); this.gfx.pathFree(pid); this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null); }
  clip(a, b) { const p = a instanceof Path2D ? a : this.path; const rule = (a instanceof Path2D ? b : a) === 'evenodd'; const pid = this._build(p); this.gfx.canvasClipPath(this.id, pid, rule); this.gfx.pathFree(pid); }
  fillRect(x, y, w, h) { if (w < 0) { x += w; w = -w; } if (h < 0) { y += h; h = -h; } this.gfx.canvasFillRect(this.id, x, y, w, h, rgba(this._color(this.fillStyle)), null); this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null); }
  strokeRect(x, y, w, h) { const p = new Path2D(); p.rect(x, y, w, h); this.stroke(p); }
  clearRect(x, y, w, h) { this.gfx.canvasSave(this.id); this.gfx.canvasSetComposite(this.id, 0); this.gfx.canvasFillRect(this.id, x, y, w, h, F([0, 0, 0, 1]), null); this.gfx.canvasRestore(this.id); this.gfx.canvasSetComposite(this.id, 2); this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null); }
  isPointInPath(a, b, c) { const p = a instanceof Path2D ? a : this.path; const x = a instanceof Path2D ? b : a, y = a instanceof Path2D ? c : b; const pid = this._build(p); const r = this.gfx.pathContains(pid, x, y, false); this.gfx.pathFree(pid); return r; }
  isPointInStroke() { return false; }
  setLineDash(d) { this._dashes = d; } getLineDash() { return this._dashes; }
  get lineDashOffset() { return 0; } set lineDashOffset(v) {}
  _fontSpec() { return this.page.fontFromShorthand(this.font); }
  measureText(t) {
    const spec = this._fontSpec();
    const run = this.page.text.shape(String(t), spec);
    const m = spec.face ? spec.face.metrics(spec.size) : { ascent: spec.size * 0.8, descent: spec.size * 0.2 };
    return { width: run.width, actualBoundingBoxLeft: 0, actualBoundingBoxRight: run.width, actualBoundingBoxAscent: m.ascent, actualBoundingBoxDescent: m.descent, fontBoundingBoxAscent: m.ascent, fontBoundingBoxDescent: m.descent, emHeightAscent: m.ascent, emHeightDescent: m.descent, alphabeticBaseline: 0, hangingBaseline: m.ascent * 0.8, ideographicBaseline: -m.descent };
  }
  fillText(t, x, y, maxW) { this._text(t, x, y, this.fillStyle); }
  strokeText(t, x, y) { this._text(t, x, y, this.strokeStyle); }
  _text(t, x, y, style) {
    const spec = this._fontSpec();
    const run = this.page.text.shape(String(t), spec);
    const m = spec.face ? spec.face.metrics(spec.size) : { ascent: spec.size * 0.8, descent: spec.size * 0.2 };
    if (this.textAlign === 'center') x -= run.width / 2; else if (this.textAlign === 'right' || this.textAlign === 'end') x -= run.width;
    if (this.textBaseline === 'top' || this.textBaseline === 'hanging') y += m.ascent; else if (this.textBaseline === 'middle') y += (m.ascent - m.descent) / 2; else if (this.textBaseline === 'bottom' || this.textBaseline === 'ideographic') y -= m.descent;
    this.page.text.draw(this.id, run, x, y, this._color(style));
    this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null);
  }
  drawImage(img, ...a) {
    const src = this.page.imageSource(img);
    if (!src) return;
    let sx = 0, sy = 0, sw = src.width, sh = src.height, dx, dy, dw, dh;
    if (a.length >= 8) { [sx, sy, sw, sh, dx, dy, dw, dh] = a; } else if (a.length >= 4) { [dx, dy, dw, dh] = a; } else { [dx, dy] = a; dw = sw; dh = sh; }
    const uv = F([sx / src.width, sy / src.height, (sx + sw) / src.width, (sy + sh) / src.height]);
    if (src.canvas) this.gfx.canvasDrawCanvas(this.id, src.canvas, F([dx, dy, dw, dh]), uv);
    else if (src.image) this.gfx.canvasDrawImage(this.id, src.image, F([dx, dy, dw, dh]), uv);
    this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null);
  }
  createImageData(w, h) { if (typeof w === 'object') return new ImageData(w.width, w.height); return new ImageData(w, h); }
  getImageData(x, y, w, h) {
    const buf = Buffer.alloc(this.w * this.h * 4);
    this.gfx.canvasRead(this.id, buf);
    const out = new ImageData(w, h);
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) { const sx = x + i, sy = y + j; if (sx < 0 || sy < 0 || sx >= this.w || sy >= this.h) continue; const s = (sy * this.w + sx) * 4, d = (j * w + i) * 4; out.data[d] = buf[s]; out.data[d + 1] = buf[s + 1]; out.data[d + 2] = buf[s + 2]; out.data[d + 3] = buf[s + 3]; }
    return out;
  }
  putImageData(data, x, y, dx = 0, dy = 0, dw = data.width, dh = data.height) {
    const w = data.width, h = data.height;
    const bytes = Buffer.from(data.data.buffer, data.data.byteOffset, data.data.byteLength);
    const img = this.gfx.imageNew(bytes, w, h);
    this.gfx.canvasSave(this.id);
    this.gfx.canvasTransform(this.id, F([1, 0, 0, 1, 0, 0]));
    this.gfx.canvasSetComposite(this.id, 1);
    this.gfx.canvasDrawImage(this.id, img, F([x + dx, y + dy, dw, dh]), F([dx / w, dy / h, (dx + dw) / w, (dy + dh) / h]));
    this.gfx.canvasSetComposite(this.id, 2);
    this.gfx.canvasRestore(this.id);
    this.gfx.imageFree(img);
    this.page.needPaint(this.canvas && this.canvas._box ? this.canvas._box.absRect() : null);
  }
  createLinearGradient() { return { addColorStop() {} }; } createRadialGradient() { return { addColorStop() {} }; } createConicGradient() { return { addColorStop() {} }; } createPattern() { return null; }
  getContextAttributes() { return { alpha: true, desynchronized: false, colorSpace: 'srgb', willReadFrequently: false }; }
  reset() { this._reset(); }
}
