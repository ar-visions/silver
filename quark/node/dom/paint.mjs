// painting boxes onto a webgfx canvas in stacking order
import { parseLength, splitSpaces } from './css.mjs';
const F = a => new Float32Array(a);
const rgba = c => F([c[0] / 255, c[1] / 255, c[2] / 255, c[3]]);

export class Painter {
  constructor(page) { this.page = page; this.gfx = page.gfx; this.shaper = page.text; }
  // damage: rects (page coordinates) to paint again, null for all
  paint(canvas, root, w, h, bg, damage) {
    this.cv = canvas; this.w = w; this.h = h; this.scale = this.page.scale; this.alpha = 1;
    this.stamp = (this.stamp || 0) + 1;
    this.damage = null;
    if (damage) {
      if (!damage.length) return;
      let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
      for (const r of damage) { x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x + r.w); y1 = Math.max(y1, r.y + r.h); }
      x0 = Math.max(0, Math.floor(x0)); y0 = Math.max(0, Math.floor(y0)); x1 = Math.min(w / this.scale, Math.ceil(x1)); y1 = Math.min(h / this.scale, Math.ceil(y1));
      if (x1 <= x0 || y1 <= y0) return;
      this.damage = { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
      if (this.prof) { this.prof.damaged = (this.prof.damaged || 0) + 1; this.prof.damageArea = (this.prof.damageArea || 0) + (x1 - x0) * (y1 - y0) / (w * h / this.scale / this.scale); }
      this.gfx.canvasTransform(canvas, F([this.scale, 0, 0, this.scale, 0, 0]));
      this.gfx.canvasSave(canvas);
      this.gfx.canvasClipRounded(canvas, x0, y0, x1 - x0, y1 - y0, null);
      this.gfx.canvasFillRect(canvas, x0, y0, x1 - x0, y1 - y0, rgba(bg), null);
      this.paintStacking(root, 0, 0);
      this.gfx.canvasRestore(canvas);
      return;
    }
    this.gfx.canvasClear(canvas, rgba(bg));
    this.gfx.canvasTransform(canvas, F([this.scale, 0, 0, this.scale, 0, 0]));
    this.paintStacking(root, 0, 0);
  }
  // a fresh union of what a box paints (a layout change's extent)
  boundsFresh(box) { this.stamp = (this.stamp || 0) + 1; return this.bounds(box); }
  // does the box reach the damage rect? unknown bounds count as yes
  touches(box) {
    const d = this.damage; if (!d) return true;
    const b = this.bounds(box); if (b === null) return true; if (!b) return false;
    return b.x < d.x + d.w && b.x + b.w > d.x && b.y < d.y + d.h && b.y + b.h > d.y;
  }
  // a stacking context: negatives, flow, positioned by z
  paintStacking(box, ox, oy) {
    if (!this.touches(box)) return;
    // opacity: every descendant fades (no group layer yet)
    const prev = this.alpha; this.alpha *= box.cs.num('opacity', 1);
    if (this.alpha <= 0) { this.alpha = prev; return; }
    const cp = this.clipPath(box, ox, oy);
    this.paintBoxSelf(box, ox, oy);
    const ox2 = ox, oy2 = oy;
    const negs = [], flow = [], pos = [];
    this.collect(box, negs, flow, pos, true);
    negs.sort((a, b) => a.z - b.z);
    pos.sort((a, b) => a.z - b.z);
    this.withClip(box, ox, oy, () => {
      for (const b of negs) this.paintStacking(b, ox2, oy2);
      for (const b of flow) this.paintFlowBox(b, ox2, oy2);
      for (const b of pos) this.paintStacking(b, ox2, oy2);
    });
    if (cp) this.gfx.canvasRestore(this.cv);
    this.alpha = prev;
  }
  // children of a stacking context, sorted into the three groups;
  // non-stacking descendants contribute their own positioned kids
  collect(box, negs, flow, pos, top) {
    for (const k of box.children) {
      if (k.kind === 'text') { flow.push(k); continue; }
      if (k.el && k.cs.get('visibility') === 'collapse') continue;
      if (k.stacking) { if (k.z < 0) negs.push(k); else pos.push(k); continue; }
      if (k.positioned) { pos.push(k); continue; }
      flow.push(k);
    }
  }
  paintFlowBox(box, ox, oy) {
    if (!this.touches(box)) return;
    if (box.kind === 'text') { this.paintText(box, ox, oy); return; }
    const prev = this.alpha; this.alpha *= box.cs.num('opacity', 1);
    if (this.alpha <= 0) { this.alpha = prev; return; }
    const cp = this.clipPath(box, ox, oy);
    this.paintBoxSelf(box, ox, oy);
    const negs = [], flow = [], pos = [];
    this.collect(box, negs, flow, pos, false);
    this.withClip(box, ox, oy, () => {
      // positioned kids of a plain box join the nearest context in order
      for (const b of flow) this.paintFlowBox(b, ox, oy);
      for (const b of negs.concat(pos).sort((a, b) => a.z - b.z)) this.paintStacking(b, ox, oy);
    });
    if (cp) this.gfx.canvasRestore(this.cv);
    this.alpha = prev;
  }
  // clip-path: inset(t r b l [round radii]) clips the box whole, its
  // own painting and its children; the other shapes are not drawn
  clipPath(box, ox, oy) {
    const v = box.cs.get('clip-path');
    if (!v || v === 'none' || process.env.QUARK_NOCLIP) return false;
    const m = v.match(/^inset\((.*)\)$/);
    if (!m) return false;
    const [edges, round] = m[1].split(/\s+round\s+/);
    const ctx = box.cs.ctx();
    const e = splitSpaces(edges).map(t => parseLength(t, ctx, box.w) || 0);
    const t = e[0] || 0, r = e.length > 1 ? e[1] : t, b = e.length > 2 ? e[2] : t, l = e.length > 3 ? e[3] : r;
    const rr = round ? splitSpaces(round).map(x => parseLength(x, ctx, box.w) || 0) : [0];
    const radii = [rr[0], rr.length > 1 ? rr[1] : rr[0], rr.length > 2 ? rr[2] : rr[0], rr.length > 3 ? rr[3] : (rr.length > 1 ? rr[1] : rr[0])];
    const bx = this.rect(box, ox, oy);
    this.gfx.canvasSave(this.cv);
    this.gfx.canvasClipRounded(this.cv, bx.x + l, bx.y + t, Math.max(0, box.w - l - r), Math.max(0, box.h - t - b), F(radii));
    return true;
  }
  // what a box and its descendants may paint, in page coordinates:
  // null when unknown (a shadow, outline, transform or filter reaches
  // past the box by an amount the layout does not keep)
  bounds(box) {
    if (box.boundsStamp === this.stamp) return box.bounds;
    box.boundsStamp = this.stamp;
    let b = null;
    const add = (x, y, w, h) => { if (b === undefined) return; if (!b) b = { x, y, w, h }; else { const x2 = Math.max(b.x + b.w, x + w), y2 = Math.max(b.y + b.h, y + h); b.x = Math.min(b.x, x); b.y = Math.min(b.y, y); b.w = x2 - b.x; b.h = y2 - b.y; } };
    if (box.kind === 'text') {
      if (box.textFrags) { const sc = this.parentScroll(box); for (const f of box.textFrags) add(f.x - sc.x, f.top - sc.y, f.w, f.ascent + f.descent); }
      box.bounds = b; return b;
    }
    const cs = box.cs;
    if (cs.get('transform') !== 'none' || cs.get('filter') !== 'none') { box.bounds = null; return null; }
    const r = box.absRect(); add(r.x, r.y, r.w, r.h);
    // a shadow or outline reaches past the box by a known amount
    const reach = this.reachOf(cs);
    if (reach) add(r.x - reach, r.y - reach, r.w + 2 * reach, r.h + 2 * reach);
    if (box.kind === 'inline' && box.rects) for (const ir of box.rects) add(ir.x, ir.y, ir.w, ir.h);
    if (box.clips && box.kind !== 'inline') { box.bounds = b; return b; }
    for (const k of box.children) { const kb = this.bounds(k); if (kb === null && k.kind !== 'text') { b = undefined; break; } if (kb) add(kb.x, kb.y, kb.w, kb.h); }
    box.bounds = b === undefined ? null : b; return box.bounds;
  }
  // how far an outer shadow or an outline paints past the box
  reachOf(cs) {
    let reach = 0;
    const shadow = cs.get('box-shadow');
    if (shadow && shadow !== 'none') for (const sh of splitTopCommas(shadow)) {
      if (/\binset\b/.test(sh)) continue;
      const nums = []; for (const p of splitSpaces(sh)) if (/^-?[\d.]+/.test(p) || p.startsWith('calc')) nums.push(parseLength(p, cs.ctx(), null) || 0);
      reach = Math.max(reach, Math.max(Math.abs(nums[0] || 0), Math.abs(nums[1] || 0)) + (nums[2] || 0) + (nums[3] || 0));
    }
    if (cs.get('outline-style') !== 'none') reach = Math.max(reach, cs.num('outline-width') + Math.max(0, cs.num('outline-offset')));
    return reach;
  }
  // do the children stay inside the clip rect?
  clipHolds(box, r) {
    for (const k of box.children) { const kb = this.bounds(k); if (kb === null && k.kind !== 'text') return false; if (!kb) continue; if (kb.x < r.x - 0.01 || kb.y < r.y - 0.01 || kb.x + kb.w > r.x + r.w + 0.01 || kb.y + kb.h > r.y + r.h + 0.01) return false; }
    return true;
  }
  // overflow clips and scroll offsets apply to the children
  withClip(box, ox, oy, fn) {
    const sx = box.scrollable && box.el ? (box.el._scrollX || 0) : 0, sy = box.scrollable && box.el ? (box.el._scrollY || 0) : 0;
    let needClip = box.clips && box.kind !== 'inline' && !process.env.QUARK_NOCLIP;
    // nothing to cut: the content lies inside the clip rect
    if (needClip && !sx && !sy) {
      const r = this.rect(box, ox, oy);
      // a rounded box cuts its corners: the children must stay inside
      // the rect inset by the largest radius
      const ri = Math.max(...box.cs.radii(box.w, box.h));
      if (this.clipHolds(box, { x: r.x + box.bl + ri, y: r.y + box.bt + ri, w: Math.max(0, box.w - box.bl - box.br - 2 * ri), h: Math.max(0, box.h - box.bt - box.bb - 2 * ri) })) needClip = false;
    }
    if (process.env.QUARK_CLIPLOG) { const [qx, qy] = process.env.QUARK_CLIPLOG.split(',').map(Number); const r = this.rect(box, ox, oy); if (qx >= r.x && qy >= r.y && qx < r.x + r.w && qy < r.y + r.h && box.clips) this.page.log(`cliplog ${box.el ? box.el.localName + '.' + String(box.el.className).split(' ').slice(0, 2).join('.') : box.pseudo} rect ${r.x},${r.y} ${r.w}x${r.h} clips=${box.clips} needClip=${needClip} radii=${box.cs.radii(box.w, box.h).join('/')} scroll=${sx},${sy}`); }
    if (needClip || sx || sy) {
      const t0 = this.prof ? performance.now() : 0;
      this.gfx.canvasSave(this.cv);
      if (needClip) {
        const r = this.rect(box, ox, oy);
        const radii = box.cs.radii(box.w, box.h);
        const inner = [Math.max(0, radii[0] - Math.max(box.bt, box.bl)), Math.max(0, radii[1] - Math.max(box.bt, box.br)), Math.max(0, radii[2] - Math.max(box.bb, box.br)), Math.max(0, radii[3] - Math.max(box.bb, box.bl))];
        this.gfx.canvasClipRounded(this.cv, r.x + box.bl, r.y + box.bt, Math.max(0, box.w - box.bl - box.br), Math.max(0, box.h - box.bt - box.bb), F(inner));
      }
      if (sx || sy) this.gfx.canvasTransform(this.cv, F([this.scale, 0, 0, this.scale, -sx * this.scale, -sy * this.scale]));
      if (this.prof) { const d = performance.now() - t0; this.prof.clips++; this.prof.clipMs += d; if (d > 1) { this.prof.clipSlow++; if (process.env.QUARK_SLOWCLIP) { const chain = []; for (let p = box; p; p = p.parent) if (p.clips && p.kind !== 'inline' && p.el) { const pr = p.absRect(); chain.push(`${p.el.localName}.${String(p.el.className).split(' ').slice(0, 2).join('.')} ${pr.x.toFixed(0)},${pr.y.toFixed(0)} ${pr.w.toFixed(0)}x${pr.h.toFixed(0)} r${p.cs.radii(p.w, p.h).map(v => v.toFixed(0)).join('/')}`); } this.page.log('slow clip ' + d.toFixed(1) + ' ms: ' + chain.slice(0, 3).join(' < ')); } } }
      fn();
      const t1 = this.prof ? performance.now() : 0;
      if (sx || sy) this.gfx.canvasTransform(this.cv, F([this.scale, 0, 0, this.scale, 0, 0]));
      this.gfx.canvasRestore(this.cv);
      if (this.prof) { const d = performance.now() - t1; this.prof.clipMs += d; if (d > 1) this.prof.clipSlow++; }
    } else fn();
  }
  rect(box, ox, oy) { const r = box.absRect(); return { x: r.x, y: r.y, w: r.w, h: r.h }; }
  opacityOf(box) { return this.alpha; }
  paintBoxSelf(box, ox, oy) {
    if (box.kind === 'text' || box.anonymous) return;
    const cs = box.cs;
    if (cs.get('visibility') !== 'visible') return;
    const a = this.opacityOf(box);
    if (a <= 0) return;
    const r = this.rect(box, ox, oy);
    if (r.x > this.w / this.scale || r.y > this.h / this.scale || r.x + r.w < 0 || r.y + r.h < 0) { if (!box.children.some(c => c.positioned)) { /* still may paint kids out of bounds */ } }
    if (process.env.QUARK_CLIPLOG) { const [qx, qy] = process.env.QUARK_CLIPLOG.split(',').map(Number); if (qx >= r.x && qy >= r.y && qx < r.x + r.w && qy < r.y + r.h) { const bg = cs.colorOf('background-color'); if (bg[3] > 0 || box.bt || box.br || box.bb || box.bl) this.page.log(`paintlog ${box.el ? box.el.localName + '.' + String(box.el.className).split(' ').slice(0, 2).join('.') : box.pseudo} rect ${r.x},${r.y} ${r.w}x${r.h} bg=${bg.join(',')} border=${box.bt}/${box.br}/${box.bb}/${box.bl} radii=${cs.radii(r.w, r.h).join('/')}`); } }
    if (box.kind === 'inline' && box.rects) { for (const ir of box.rects) this.paintBackgroundAndBorder(box, ir.x, ir.y, ir.w, ir.h, a); }
    else this.paintBackgroundAndBorder(box, r.x, r.y, r.w, r.h, a);
    if (box.el) this.paintReplaced(box, r, a);
    // a caret in a focused field
    if (box.el && this.page.caretBox === box) this.paintCaret(box, r);
    const outline = cs.get('outline-style');
    if (outline !== 'none' && cs.num('outline-width') > 0) {
      const ow = cs.num('outline-width'), off = cs.num('outline-offset');
      const oc = cs.colorOf('outline-color'); oc[3] *= a;
      this.ring(r.x - off - ow, r.y - off - ow, r.w + 2 * (off + ow), r.h + 2 * (off + ow), ow, oc, cs.radii(r.w, r.h).map(x => x + off + ow));
    }
  }
  paintBackgroundAndBorder(box, x, y, w, h, alpha) {
    const cs = box.cs;
    const bg = cs.colorOf('background-color');
    const radii = cs.radii(w, h);
    const hasR = radii.some(v => v > 0);
    const shadow = cs.get('box-shadow');
    let shadows = 0;
    if (shadow && shadow !== 'none') shadows = this.setShadows(shadow, cs, alpha);
    const img = cs.get('background-image');
    if (bg[3] > 0 || shadows) {
      const c = bg.slice(); c[3] *= alpha;
      if (!bg[3] && shadows) c[3] = 0;
      this.gfx.canvasFillRect(this.cv, x, y, w, h, rgba(c), hasR ? F(radii) : null);
    }
    if (shadows) this.gfx.canvasClearShadows(this.cv);
    if (img && img !== 'none') this.paintBackgroundImage(box, img, x, y, w, h, alpha, radii);
    // borders
    const bt = box.bt, br = box.br, bb = box.bb, bl = box.bl;
    if (bt || br || bb || bl) {
      const ct = cs.colorOf('border-top-color'), cr = cs.colorOf('border-right-color'), cb = cs.colorOf('border-bottom-color'), cl = cs.colorOf('border-left-color');
      for (const c of [ct, cr, cb, cl]) c[3] *= alpha;
      // one width (a side may be 0), one colour and style on the drawn
      // sides: the first drawn side is the reference
      const t = Math.max(bt, br, bb, bl);
      // a side draws when it has width, a style and a colour
      const sides = [[bt, ct, cs.get('border-top-style')], [br, cr, cs.get('border-right-style')], [bb, cb, cs.get('border-bottom-style')], [bl, cl, cs.get('border-left-style')]];
      const drawn = sides.map(([v, c, st]) => v > 0 && st !== 'none' && c[3] > 0);
      const ref = sides.find((sd, i) => drawn[i]);
      const uniform = !!ref && sides.every(([v, c, st], i) => !drawn[i] || v === t && eq(c, ref[1]) && st === ref[2]);
      const same = sides.every(([v, c, st]) => v === t && eq(c, ref ? ref[1] : ct) && st === (ref ? ref[2] : 'none'));
      if (!ref) {
      } else if (uniform && !hasR) {
        // straight edges: a rect a side, no path
        const c = rgba(ref[1]); if (drawn[0]) this.gfx.canvasFillRect(this.cv, x, y, w, bt, c, null); if (drawn[2]) this.gfx.canvasFillRect(this.cv, x, y + h - bb, w, bb, c, null); if (drawn[3]) this.gfx.canvasFillRect(this.cv, x, y + bt, bl, Math.max(0, h - bt - bb), c, null); if (drawn[1]) this.gfx.canvasFillRect(this.cv, x + w - br, y + bt, br, Math.max(0, h - bt - bb), c, null);
      } else if (uniform) {
        // rounded: a stroke along the border's centre line; a side
        // not drawn is cut away by a clip (a path's 1 px band fills
        // nothing in trinity, a stroke's coverage is exact)
        this.strokeRing(x, y, w, h, t, drawn, ref[1], radii);
      } else if (same || hasR) {
        // one ring: outer rounded rect less the inner one, even-odd
        if (ref[1][3] > 0) this.ringSides(x, y, w, h, bt, br, bb, bl, ref[1], radii, [ct, cr, cb, cl]);
      } else {
        if (bt && ct[3] > 0 && cs.get('border-top-style') !== 'none') this.fillQuad([x, y, x + w, y, x + w - br, y + bt, x + bl, y + bt], ct);
        if (bb && cb[3] > 0 && cs.get('border-bottom-style') !== 'none') this.fillQuad([x + bl, y + h - bb, x + w - br, y + h - bb, x + w, y + h, x, y + h], cb);
        if (bl && cl[3] > 0 && cs.get('border-left-style') !== 'none') this.fillQuad([x, y, x + bl, y + bt, x + bl, y + h - bb, x, y + h], cl);
        if (br && cr[3] > 0 && cs.get('border-right-style') !== 'none') this.fillQuad([x + w - br, y + bt, x + w, y, x + w, y + h, x + w - br, y + h - bb], cr);
      }
    }
  }
  strokeRing(x, y, w, h, t, drawn, color, radii) {
    const [dt, dr, db, dl] = drawn;
    const cut = !(dt && dr && db && dl) && !process.env.QUARK_NOCLIP;
    if (cut) { this.gfx.canvasSave(this.cv); this.gfx.canvasClipRounded(this.cv, x + (dl ? 0 : t), y + (dt ? 0 : t), Math.max(0, w - (dl ? 0 : t) - (dr ? 0 : t)), Math.max(0, h - (dt ? 0 : t) - (db ? 0 : t)), null); }
    const pid = this.gfx.pathNew();
    rrectPath(this.gfx, pid, x + t / 2, y + t / 2, Math.max(0, w - t), Math.max(0, h - t), radii.map(r => Math.max(0, r - t / 2)));
    this.gfx.canvasStrokePath(this.cv, pid, rgba(color), t, 0);
    this.gfx.pathFree(pid);
    if (cut) this.gfx.canvasRestore(this.cv);
  }
  ringSides(x, y, w, h, bt, br, bb, bl, color, radii, colors) {
    const allSame = colors.every(c => eq(c, colors[0]));
    if (allSame) { this.ringPath(x, y, w, h, bt, br, bb, bl, color, radii); return; }
    // different colours with radii: a quad per side, clipped by the ring
    this.gfx.canvasSave(this.cv);
    const pid = this.ringPathId(x, y, w, h, bt, br, bb, bl, radii);
    this.gfx.canvasClipPath(this.cv, pid, true);
    this.gfx.pathFree(pid);
    this.fillQuad([x, y, x + w, y, x + w - br, y + bt, x + bl, y + bt], colors[0]);
    this.fillQuad([x + w - br, y + bt, x + w, y, x + w, y + h, x + w - br, y + h - bb], colors[1]);
    this.fillQuad([x + bl, y + h - bb, x + w - br, y + h - bb, x + w, y + h, x, y + h], colors[2]);
    this.fillQuad([x, y, x + bl, y + bt, x + bl, y + h - bb, x, y + h], colors[3]);
    this.gfx.canvasRestore(this.cv);
  }
  ringPathId(x, y, w, h, bt, br, bb, bl, radii) {
    const pid = this.gfx.pathNew();
    rrectPath(this.gfx, pid, x, y, w, h, radii);
    const inner = [Math.max(0, radii[0] - Math.max(bt, bl)), Math.max(0, radii[1] - Math.max(bt, br)), Math.max(0, radii[2] - Math.max(bb, br)), Math.max(0, radii[3] - Math.max(bb, bl))];
    rrectPath(this.gfx, pid, x + bl, y + bt, Math.max(0, w - bl - br), Math.max(0, h - bt - bb), inner);
    return pid;
  }
  ringPath(x, y, w, h, bt, br, bb, bl, color, radii) {
    const pid = this.ringPathId(x, y, w, h, bt, br, bb, bl, radii);
    this.gfx.canvasFillPath(this.cv, pid, rgba(color), true);
    this.gfx.pathFree(pid);
  }
  ring(x, y, w, h, t, color, radii) { this.ringPath(x, y, w, h, t, t, t, t, color, radii); }
  fillQuad(p, color) {
    const pid = this.gfx.pathNew();
    this.gfx.pathMoveTo(pid, p[0], p[1]); this.gfx.pathLineTo(pid, p[2], p[3]); this.gfx.pathLineTo(pid, p[4], p[5]); this.gfx.pathLineTo(pid, p[6], p[7]); this.gfx.pathClose(pid);
    this.gfx.canvasFillPath(this.cv, pid, rgba(color), false);
    this.gfx.pathFree(pid);
  }
  setShadows(spec, cs, alpha) {
    const list = splitTopCommas(spec);
    let n = 0;
    for (const s of list.slice(0, 4)) {
      let inset = false; const nums = []; let color = null;
      for (const p of splitSpaces(s)) {
        if (p === 'inset') inset = true;
        else if (/^-?[\d.]+/.test(p) || p.startsWith('calc')) nums.push(parseLength(p, cs.ctx(), null) || 0);
        else color = cs.parseColorValue(p, cs.color());
      }
      if (!color) color = cs.color();
      const c = color.slice(); c[3] *= alpha;
      this.gfx.canvasSetShadow(this.cv, n++, rgba(c), nums[0] || 0, nums[1] || 0, nums[2] || 0, nums[3] || 0, inset);
    }
    return n;
  }
  paintBackgroundImage(box, img, x, y, w, h, alpha, radii) {
    const layers = splitTopCommas(img);
    const sizes = splitTopCommas(box.cs.get('background-size')), positions = splitTopCommas(box.cs.get('background-position')), repeats = splitTopCommas(box.cs.get('background-repeat'));
    layers.forEach((layer, i) => {
      if (layer.startsWith('linear-gradient') || layer.startsWith('radial-gradient') || layer.startsWith('repeating-')) { this.paintGradient(box, layer, x, y, w, h, alpha, radii); return; }
      const m = layer.match(/^url\(\s*["']?([^"')]+)["']?\s*\)$/);
      if (!m) return;
      const src = this.page.imageFor(m[1], box.el || box.pseudoOf);
      if (!src || !src.image) return;
      const sz = sizes[i % sizes.length] || 'auto', pos = positions[i % positions.length] || '0% 0%', rep = repeats[i % repeats.length] || 'repeat';
      let dw = src.width, dh = src.height;
      const sp = splitSpaces(sz);
      if (sp[0] === 'contain' || sp[0] === 'cover') { const sc = sp[0] === 'contain' ? Math.min(w / dw, h / dh) : Math.max(w / dw, h / dh); dw *= sc; dh *= sc; }
      else { const a = sp[0] === 'auto' || !sp[0] ? null : parseLength(sp[0], box.cs.ctx(), w); const b = !sp[1] || sp[1] === 'auto' ? null : parseLength(sp[1], box.cs.ctx(), h); if (a != null && b != null) { dw = a; dh = b; } else if (a != null) { dh = dh * a / dw; dw = a; } else if (b != null) { dw = dw * b / dh; dh = b; } }
      const pp = splitSpaces(pos);
      const px = posValue(pp[0] || '0%', w - dw, box.cs), py = posValue(pp[1] || (['left', 'right', 'center'].includes(pp[0]) ? 'center' : '0%'), h - dh, box.cs);
      this.gfx.canvasSave(this.cv);
      this.gfx.canvasClipRounded(this.cv, x, y, w, h, F(radii));
      const tint = alpha < 1 ? null : null;
      if (rep === 'no-repeat' || dw <= 0 || dh <= 0) this.gfx.canvasDrawImage(this.cv, src.image, F([x + px, y + py, dw, dh]), F([0, 0, 1, 1]));
      else {
        const rx = rep === 'repeat-y' ? 1 : Math.ceil(w / dw) + 1, ry = rep === 'repeat-x' ? 1 : Math.ceil(h / dh) + 1;
        const sx = rep === 'repeat-y' ? x + px : x + ((px % dw) + dw) % dw - dw, sy = rep === 'repeat-x' ? y + py : y + ((py % dh) + dh) % dh - dh;
        for (let j = 0; j < ry + 1 && j < 64; j++) for (let k = 0; k < rx + 1 && k < 64; k++) this.gfx.canvasDrawImage(this.cv, src.image, F([sx + k * dw, sy + j * dh, dw, dh]), F([0, 0, 1, 1]));
      }
      this.gfx.canvasRestore(this.cv);
    });
  }
  // a gradient as its average colour for now; a real ramp comes later
  paintGradient(box, layer, x, y, w, h, alpha, radii) {
    const inner = layer.slice(layer.indexOf('(') + 1, layer.lastIndexOf(')'));
    const stops = splitTopCommas(inner).map(s => { const p = splitSpaces(s); const c = box.cs.parseColorValue(p[0], box.cs.color()); return c; }).filter(Boolean);
    if (!stops.length) return;
    const avg = [0, 0, 0, 0];
    for (const s of stops) { avg[0] += s[0] * s[3]; avg[1] += s[1] * s[3]; avg[2] += s[2] * s[3]; avg[3] += s[3]; }
    if (avg[3] <= 0) return;
    const c = [avg[0] / avg[3], avg[1] / avg[3], avg[2] / avg[3], avg[3] / stops.length * alpha];
    this.gfx.canvasFillRect(this.cv, x, y, w, h, rgba(c), radii.some(v => v > 0) ? F(radii) : null);
  }
  paintReplaced(box, r, alpha) {
    const el = box.el;
    const cx = r.x + box.bl + box.pl, cy = r.y + box.bt + box.pt, cw = box.cw(), ch = box.ch();
    if (el.localName === 'img' && el._image && el._image.id) {
      const fit = box.cs.get('object-fit');
      let dw = cw, dh = ch, dx = cx, dy = cy;
      if (fit === 'contain' || fit === 'scale-down' || fit === 'cover') { const iw = el._image.width, ih = el._image.height; const s = fit === 'cover' ? Math.max(cw / iw, ch / ih) : Math.min(cw / iw, ch / ih, fit === 'scale-down' ? 1 : Infinity); dw = iw * s; dh = ih * s; dx = cx + (cw - dw) / 2; dy = cy + (ch - dh) / 2; }
      this.gfx.canvasSave(this.cv); this.gfx.canvasClip(this.cv, Math.floor(cx * this.scale), Math.floor(cy * this.scale), Math.ceil(cw * this.scale), Math.ceil(ch * this.scale));
      this.gfx.canvasDrawImage(this.cv, el._image.id, F([dx, dy, dw, dh]), F([0, 0, 1, 1]));
      this.gfx.canvasRestore(this.cv);
    } else if (el.localName === 'canvas' && el._ctx && el._ctx.id) {
      this.gfx.canvasDrawCanvas(this.cv, el._ctx.id, F([cx, cy, cw, ch]), F([0, 0, 1, 1]));
    } else if (el.localName === 'input' && (el.type === 'checkbox' || el.type === 'radio')) {
      const c = box.cs.color(); c[3] *= alpha;
      const radio = el.type === 'radio';
      const s = Math.min(cw, ch);
      const radii = radio ? F([s / 2, s / 2, s / 2, s / 2]) : F([2, 2, 2, 2]);
      this.ring(cx, cy, s, s, 1, c, Array.from(radii));
      if (el.checked) {
        const accent = box.cs.get('accent-color');
        const ac = accent && accent !== 'auto' ? (box.cs.parseColorValue(accent, c) || c) : c;
        if (radio) this.gfx.canvasFillRect(this.cv, cx + s / 4, cy + s / 4, s / 2, s / 2, rgba(ac), F([s / 4, s / 4, s / 4, s / 4]));
        else { const pid = this.gfx.pathNew(); this.gfx.pathMoveTo(pid, cx + s * 0.22, cy + s * 0.52); this.gfx.pathLineTo(pid, cx + s * 0.42, cy + s * 0.72); this.gfx.pathLineTo(pid, cx + s * 0.8, cy + s * 0.3); this.gfx.canvasStrokePath(this.cv, pid, rgba(ac), Math.max(1.5, s / 7), 1); this.gfx.pathFree(pid); }
      }
    } else if (el.localName === 'select') {
      const c = box.cs.color(); c[3] *= alpha * 0.8;
      const pid = this.gfx.pathNew(); const ax = cx + cw - 10, ay = cy + ch / 2 - 2;
      this.gfx.pathMoveTo(pid, ax - 4, ay); this.gfx.pathLineTo(pid, ax, ay + 4); this.gfx.pathLineTo(pid, ax + 4, ay);
      this.gfx.canvasStrokePath(this.cv, pid, rgba(c), 1.5, 1); this.gfx.pathFree(pid);
    } else if (el.localName === 'progress' || el.localName === 'meter') {
      const c = box.cs.color(); const frac = Math.max(0, Math.min(1, el.value / (el.max || 1)));
      this.gfx.canvasFillRect(this.cv, cx, cy, cw, ch, rgba([c[0], c[1], c[2], 0.2 * alpha]), F([ch / 2, ch / 2, ch / 2, ch / 2]));
      this.gfx.canvasFillRect(this.cv, cx, cy, cw * frac, ch, rgba([c[0], c[1], c[2], alpha]), F([ch / 2, ch / 2, ch / 2, ch / 2]));
    }
  }
  paintCaret(box, r) {
    const el = box.el;
    if (!el || this.page.caretHidden) return;
    const f = box.children[0] && box.children[0].textFrags && box.children[0].textFrags[0];
    const cs = box.cs;
    const spec = this.shaper.spec(cs);
    const value = el.value || '';
    const pos = Math.min(el.selectionStart || 0, value.length);
    let cx = r.x + box.bl + box.pl, cy = r.y + box.bt + box.pt;
    let before = value.slice(0, pos);
    if (el.localName === 'textarea') { const lines = before.split('\n'); cy += (lines.length - 1) * cs.lineHeight; before = lines[lines.length - 1]; }
    const w = this.shaper.measure(before, spec) - (el._scrollX || 0);
    const col = cs.get('caret-color'); let c = col && col !== 'auto' ? cs.parseColorValue(col, cs.color()) : cs.color();
    if (!c) c = cs.color();
    // a selection first
    const s0 = Math.min(el.selectionStart || 0, el.selectionEnd || 0), s1 = Math.max(el.selectionStart || 0, el.selectionEnd || 0);
    if (s1 > s0 && el.localName !== 'textarea') { const x0 = this.shaper.measure(value.slice(0, s0), spec), x1 = this.shaper.measure(value.slice(0, s1), spec); this.gfx.canvasFillRect(this.cv, cx + x0 - (el._scrollX || 0), cy, x1 - x0, cs.lineHeight, rgba([38, 79, 120, 0.9]), null); }
    this.gfx.canvasFillRect(this.cv, cx + w, cy + (box.ch() - cs.lineHeight) / 2 * (el.localName === 'textarea' ? 0 : 1), Math.max(1, 1 / this.scale), cs.lineHeight, rgba(c), null);
  }
  paintText(box, ox, oy) {
    if (!box.textFrags || !box.textFrags.length) return;
    const cs = box.cs;
    if (cs.get('visibility') !== 'visible') return;
    let alpha = 1;
    for (let p = box.parent; p; p = p.parent) { if (p.stacking) break; }
    const color = cs.color().slice(); color[3] *= this.alpha * (box.placeholder ? 0.5 : 1);
    const sc = this.parentScroll(box);
    const deco = cs.get('text-decoration-line');
    const shadow = cs.get('text-shadow');
    for (const f of box.textFrags) {
      if (!f.run || !f.run.pieces.length) continue;
      const x = f.x - sc.x, y = f.y - sc.y;
      if (y - f.ascent > this.h / this.scale + 50 || y + f.descent < -50 || x > this.w / this.scale + 50 || x + f.w < -50) continue;
      if (shadow && shadow !== 'none') { const parts = splitSpaces(shadow); const nums = parts.filter(p => /^-?[\d.]+/.test(p)).map(p => parseFloat(p)); const col = parts.find(p => !/^-?[\d.]+/.test(p)); const scol = col ? cs.parseColorValue(col, color) : [0, 0, 0, 0.5]; if (scol) this.shaper.draw(this.cv, f.run, x + (nums[0] || 0), y + (nums[1] || 0), scol, this.gfx); }
      this.shaper.draw(this.cv, f.run, x, y, color, this.gfx);
      if (deco && deco !== 'none') {
        const dc = cs.colorOf('text-decoration-color');
        const th = Math.max(1, cs.fontSize / 14);
        if (deco.includes('underline')) this.gfx.canvasFillRect(this.cv, x, y + th * 1.5, f.w - (f.trailingSpace ? f.spaceW : 0), th, rgba(dc), null);
        if (deco.includes('line-through')) this.gfx.canvasFillRect(this.cv, x, y - cs.fontSize * 0.3, f.w, th, rgba(dc), null);
        if (deco.includes('overline')) this.gfx.canvasFillRect(this.cv, x, y - f.ascent, f.w, th, rgba(dc), null);
      }
    }
  }
  // text positions are page px before scroll; take the ancestors' scroll
  parentScroll(box) {
    let x = 0, y = 0;
    for (let p = box.parent; p; p = p.parent) { if (p.scrollable && p.el) { x += p.el._scrollX || 0; y += p.el._scrollY || 0; } x -= p.rx; y -= p.ry; }
    return { x, y };
  }
}
function eq(a, b) { return a[0] === b[0] && a[1] === b[1] && a[2] === b[2] && a[3] === b[3]; }
export function splitTopCommas(v) { const out = []; let cur = '', d = 0; for (const c of v) { if (c === '(') d++; if (c === ')') d--; if (c === ',' && d === 0) { out.push(cur.trim()); cur = ''; continue; } cur += c; } if (cur.trim()) out.push(cur.trim()); return out; }
function posValue(p, free, cs) { if (p === 'left' || p === 'top') return 0; if (p === 'right' || p === 'bottom') return free; if (p === 'center') return free / 2; if (p.endsWith('%')) return free * parseFloat(p) / 100; return parseLength(p, cs.ctx(), free) || 0; }
export function rrectPath(gfx, pid, x, y, w, h, r) {
  // radii that overlap scale down together (CSS)
  const f = Math.min(1, w / Math.max(1e-6, r[0] + r[1]), w / Math.max(1e-6, r[2] + r[3]), h / Math.max(1e-6, r[0] + r[3]), h / Math.max(1e-6, r[1] + r[2]));
  const [tl, tr, br, bl] = f < 1 ? r.map(v => v * f) : r;
  gfx.pathMoveTo(pid, x + tl, y);
  gfx.pathLineTo(pid, x + w - tr, y); if (tr) gfx.pathArcTo(pid, x + w, y + tr, tr, tr, 0, false, true);
  gfx.pathLineTo(pid, x + w, y + h - br); if (br) gfx.pathArcTo(pid, x + w - br, y + h, br, br, 0, false, true);
  gfx.pathLineTo(pid, x + bl, y + h); if (bl) gfx.pathArcTo(pid, x, y + h - bl, bl, bl, 0, false, true);
  gfx.pathLineTo(pid, x, y + tl); if (tl) gfx.pathArcTo(pid, x + tl, y, tl, tl, 0, false, true);
  gfx.pathClose(pid);
}
