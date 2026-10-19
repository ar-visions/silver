// layout: block flow, inline lines, flex, positioning, overflow
import { ELEMENT_NODE, TEXT_NODE } from './node.mjs';
import { parseLength, splitSpaces } from './css.mjs';
import { layoutGrid } from './grid.mjs';
const INLINE_KINDS = new Set(['inline', 'inline-block', 'inline-flex', 'inline-grid', 'inline-table']);
const REPLACED = new Set(['img', 'canvas', 'video', 'iframe', 'svg', 'input', 'select', 'textarea', 'progress', 'meter', 'embed', 'object']);

export class Box {
  constructor(el, cs, kind) {
    this.el = el; this.cs = cs; this.kind = kind;
    this.x = 0; this.y = 0; this.w = 0; this.h = 0;           // border box, page px
    this.mt = 0; this.mr = 0; this.mb = 0; this.ml = 0;
    this.pt = 0; this.pr = 0; this.pb = 0; this.pl = 0;
    this.bt = 0; this.br = 0; this.bb = 0; this.bl = 0;
    this.children = []; this.parent = null; this.lines = null;
    this.contentW = 0; this.contentH = 0;                      // scrollable extent
    this.offsetParent = null; this.scrollable = false; this.abs = null; this.text = null;
    this.rx = 0; this.ry = 0;                                  // relative/transform offset
    this.baseline = null; this.stacking = false; this.pseudo = null;
  }
  // content box
  cx() { return this.x + this.bl + this.pl; } cy() { return this.y + this.bt + this.pt; }
  cw() { return Math.max(0, this.w - this.bl - this.br - this.pl - this.pr); } ch() { return Math.max(0, this.h - this.bt - this.bb - this.pt - this.pb); }
  clientW() { return Math.max(0, this.w - this.bl - this.br); } clientH() { return Math.max(0, this.h - this.bt - this.bb); }
  scrollW() { return Math.max(this.clientW(), this.contentW + this.pl + this.pr); } scrollH() { return Math.max(this.clientH(), this.contentH + this.pt + this.pb); }
  // where it is on the screen: scrolls of the ancestors taken off
  absRect() {
    let x = this.x + this.rx, y = this.y + this.ry;
    for (let p = this.parent; p; p = p.parent) { if (p.scrollable && p.el && this.scrollsWith(p)) { x -= p.el._scrollX || 0; y -= p.el._scrollY || 0; } x += p.rx; y += p.ry; }
    return { x, y, w: this.w, h: this.h };
  }
  scrollsWith(p) { return !(this.fixed && this.fixedRoot === p); }
  offsetLeft() { const op = this.offsetParent; return this.x - (op ? op.x + op.bl : 0); }
  offsetTop() { const op = this.offsetParent; return this.y - (op ? op.y + op.bt : 0); }
}

export class Layout {
  constructor(page) { this.page = page; this.shaper = page.text; }
  run(root, vw, vh) {
    this.vw = vw; this.vh = vh; this.absQueue = [];
    this.rootBox = this.build(root, null);
    if (!this.rootBox) return null;
    const b = this.rootBox;
    b.x = 0; b.y = 0;
    this.layoutBlock(b, vw, vh, true);
    b.h = Math.max(b.h, vh); b.w = vw;
    this.placeAbsolutes();
    return b;
  }
  // can old be laid out again on its own? a block-level box that its
  // parent laid out with inputs we kept; a flex or grid item is laid
  // by its container's algorithm, so the container is the one
  canRelayout(old) {
    if (!old.el || !old.parent || old.inCW === undefined) return false;
    if (old.kind !== 'block' && old.kind !== 'flex' && old.kind !== 'grid') return false;
    return old.positionedAbs || !old.viaFixed || !!old.inFlex;
  }
  // the subtree under old made and laid out again in place, with
  // the inputs of its last pass; the new box, or null when its size
  // or margins moved (the caller goes up one level)
  relayoutOne(old) {
    const parent = old.parent, i = parent.children.indexOf(old);
    if (i < 0) return null;
    const r = this.relayoutTry(old, parent, i);
    if (!r) this.rebind(old);
    return r;
  }
  // the old subtree is the tree again: its nodes point at it
  rebind(b) { if (b.el) b.el._box = b; if (b.textNode) b.textNode._box = b; for (const k of b.children) this.rebind(k); }
  relayoutTry(old, parent, i) {
    this.absQueue = [];
    const box = this.build(old.el, parent);
    if (!box) return null;
    parent.children[i] = box;
    box.x = old.x; box.y = old.y; box.staticX = old.staticX; box.staticY = old.staticY;
    box.flexItem = old.flexItem; box.flexRow = old.flexRow;
    if (old.positionedAbs) {
      this.absQueue.push(box);
      this.placeAbsolutes();
      if (box.x !== old.x || box.y !== old.y || box.w !== old.w || box.h !== old.h) { parent.children[i] = old; return null; }
      return box;
    }
    if (old.inFlex) {
      // a flex item: the same inputs to its container, the same
      // natural cross size, then the container's last call again
      const f = old.inFlex, row = old.flexRow;
      const g = this.flexInputs(box, row, f.w, f.h, f.main);
      box.inFlex = { w: f.w, h: f.h, main: f.main, base: g.base, min: g.min };
      if (g.base !== f.base || g.min !== f.min) { parent.children[i] = old; return null; }
      if (row) { box.fixedW = old.inFW; this.layoutFlexItem(box, old.inFW, f.w, f.h, true); box.inFlex.cross = box.h; }
      else { box.fixedH = old.inFH; this.layoutFlexItem(box, old.inFH, f.w, f.h, false); box.inFlex.cross = box.w; }
      if (box.inFlex.cross !== f.cross) { parent.children[i] = old; return null; }
      box.x = old.x; box.y = old.y;
      box.fixedW = old.inFW; box.fixedH = old.inFH;
      this.layoutBlockFixed(box, old.inCW, old.inCH, old.inFW, old.inFH);
    } else this.layoutBlock(box, old.inCW, old.inCH);
    if (box.w !== old.w || box.h !== old.h || box.ml !== old.ml || box.mr !== old.mr || box.mt !== old.mt || box.mb !== old.mb || box.baseline !== old.baseline) { parent.children[i] = old; return null; }
    this.placeAbsolutes();
    return box;
  }
  // ---- boxes from the DOM
  build(el, parent) {
    const cs = this.page.styleOf(el);
    const display = cs.get('display');
    if (display === 'none') return null;
    if (display === 'contents') { return null; }
    const kind = this.blockify(this.kindOf(display, el), parent);
    const box = new Box(el, cs, kind);
    box.parent = parent;
    el._box = box;
    const pos = cs.get('position');
    box.position = pos;
    box.positioned = pos !== 'static';
    box.replaced = REPLACED.has(el.localName) && !(el.localName === 'svg');
    const ox = cs.get('overflow-x'), oy = cs.get('overflow-y');
    box.clips = ox !== 'visible' || oy !== 'visible';
    box.scrollable = (ox === 'auto' || ox === 'scroll' || oy === 'auto' || oy === 'scroll' || ox === 'hidden' || oy === 'hidden');
    box.stacking = box.positioned && cs.get('z-index') !== 'auto' || cs.num('opacity', 1) < 1 || cs.get('transform') !== 'none' || pos === 'fixed' || cs.get('isolation') === 'isolate' || cs.get('filter') !== 'none' || cs.get('mix-blend-mode') !== 'normal';
    box.z = cs.get('z-index') === 'auto' ? 0 : parseInt(cs.get('z-index')) || 0;
    this.buildChildren(box, el);
    return box;
  }
  buildChildren(box, el) {
    const before = this.pseudoBox(el, 'before', box);
    if (before) box.children.push(before);
    for (const c of el.childNodes) {
      if (c.nodeType === TEXT_NODE) {
        if (el.localName === 'textarea' || el.localName === 'script' || el.localName === 'style' || el.localName === 'template') continue;
        const t = new Box(null, box.cs, 'text'); t.textNode = c; t.parent = box; t.text = c.data; c._box = t;
        box.children.push(t);
      } else if (c.nodeType === ELEMENT_NODE) {
        if (this.page.styleOf(c).get('display') === 'contents') { this.buildChildren(box, c); continue; }
        const cb = this.build(c, box);
        if (cb) box.children.push(cb);
      }
    }
    const after = this.pseudoBox(el, 'after', box);
    if (after) box.children.push(after);
    // a text input shows its value as text
    if (el.localName === 'input' && !['checkbox', 'radio', 'file', 'range', 'color', 'hidden', 'submit', 'button', 'reset', 'image'].includes(el.type)) {
      const v = el.value, ph = !v && el.placeholder;
      const t = new Box(null, box.cs, 'text'); t.parent = box; t.text = v || el.placeholder || ''; t.placeholder = ph; t.inputText = true;
      box.children = [t];
    } else if (el.localName === 'textarea') {
      const v = el.value, ph = !v && el.placeholder;
      const t = new Box(null, box.cs, 'text'); t.parent = box; t.text = v || el.placeholder || ''; t.placeholder = ph; t.inputText = true;
      box.children = [t];
    } else if (el.localName === 'select') {
      const o = el.options[el.selectedIndex];
      const t = new Box(null, box.cs, 'text'); t.parent = box; t.text = o ? o.text : ''; box.children = [t];
    }
  }
  pseudoBox(el, which, parent) {
    const cs = this.page.pseudoStyleOf(el, which);
    if (!cs) return null;
    const content = cs.get('content');
    if (content === 'none' || content === 'normal' || cs.get('display') === 'none') return null;
    const text = contentText(content, el);
    const kind = this.blockify(this.kindOf(cs.get('display'), null), parent);
    const box = new Box(null, cs, kind === 'text' ? 'inline' : kind);
    box.pseudo = which; box.parent = parent; box.pseudoOf = el;
    box.position = cs.get('position'); box.positioned = box.position !== 'static';
    box.z = cs.get('z-index') === 'auto' ? 0 : parseInt(cs.get('z-index')) || 0;
    box.stacking = box.positioned && cs.get('z-index') !== 'auto' || cs.num('opacity', 1) < 1;
    const ox = cs.get('overflow-x'), oy = cs.get('overflow-y');
    box.clips = ox !== 'visible' || oy !== 'visible'; box.scrollable = box.clips;
    if (text) { const t = new Box(null, cs, 'text'); t.parent = box; t.text = text; box.children.push(t); }
    if (which === 'before') el._before = box; else el._after = box;
    return box;
  }
  // a flex or grid item is block-level whatever its display
  blockify(kind, parent) {
    if (!parent || parent.kind !== 'flex' && parent.kind !== 'inline-flex' && parent.kind !== 'grid' && parent.kind !== 'inline-grid') return kind;
    if (kind === 'inline' || kind === 'inline-block') return 'block';
    if (kind === 'inline-flex') return 'flex';
    if (kind === 'inline-grid') return 'grid';
    return kind;
  }
  kindOf(display, el) {
    switch (display) {
    case 'block': case 'list-item': case 'flow-root': case 'table': case 'table-caption': case 'table-row-group': case 'table-header-group': case 'table-footer-group': return 'block';
    case 'grid': return 'grid';
    case 'inline-grid': return 'inline-grid';
    case 'flex': return 'flex';
    case 'inline-flex': return 'inline-flex';
    case 'inline-block': case 'inline-table': return 'inline-block';
    case 'table-row': return 'flex';
    case 'table-cell': case 'table-column': case 'table-column-group': return 'block';
    case 'inline': default: return el && REPLACED.has(el.localName) ? 'inline-block' : 'inline';
    }
  }
  // ---- sizes
  edges(box, cw) {
    const cs = box.cs;
    const L = (k, base) => { const v = cs.len(k, base); return v == null ? 0 : v; };
    box.mt = L('margin-top', cw); box.mb = L('margin-bottom', cw); box.ml = L('margin-left', cw); box.mr = L('margin-right', cw);
    box.autoML = cs.get('margin-left') === 'auto'; box.autoMR = cs.get('margin-right') === 'auto';
    box.pt = L('padding-top', cw); box.pb = L('padding-bottom', cw); box.pl = L('padding-left', cw); box.pr = L('padding-right', cw);
    box.bt = L('border-top-width'); box.bb = L('border-bottom-width'); box.bl = L('border-left-width'); box.br = L('border-right-width');
  }
  // a width property as border-box px, null when auto/none
  widthProp(box, k, base) {
    const cs = box.cs, v = cs.get(k);
    if (v === 'auto' || v === 'none' || v === '') return null;
    if (v === 'max-content' || v === 'min-content' || v.startsWith('fit-content')) return v;
    if (v.includes('%') && base == null) return null;
    let n = parseLength(v, cs.ctx(), base);
    if (n == null) return null;
    if (cs.get('box-sizing') !== 'border-box') n += box.pl + box.pr + box.bl + box.br;
    return n;
  }
  heightProp(box, k, base) {
    const cs = box.cs, v = cs.get(k);
    if (v === 'auto' || v === 'none' || v === '') return null;
    if (v.includes('%') && base == null) return null;
    if (v === 'max-content' || v === 'min-content' || v.startsWith('fit-content')) return null;
    let n = parseLength(v, cs.ctx(), base);
    if (n == null) return null;
    if (cs.get('box-sizing') !== 'border-box') n += box.pt + box.pb + box.bt + box.bb;
    return n;
  }
  clampW(box, w, base) {
    const mn = this.widthProp(box, 'min-width', base), mx = this.widthProp(box, 'max-width', base);
    if (typeof mx === 'number') w = Math.min(w, mx);
    if (typeof mn === 'number') w = Math.max(w, mn);
    return Math.max(0, w);
  }
  clampH(box, h, base) {
    const mn = this.heightProp(box, 'min-height', base), mx = this.heightProp(box, 'max-height', base);
    if (mx != null) h = Math.min(h, mx);
    if (mn != null) h = Math.max(h, mn);
    return Math.max(0, h);
  }
  // the size a replaced element wants
  replacedSize(box) {
    const el = box.el;
    if (!el) return null;
    switch (el.localName) {
    case 'img': return el._image ? { w: el._image.width, h: el._image.height } : { w: 0, h: 0 };
    case 'canvas': return { w: el.width, h: el.height };
    case 'video': case 'iframe': case 'embed': case 'object': return { w: 300, h: 150 };
    case 'input': { const t = el.type; if (t === 'checkbox' || t === 'radio') return { w: 13, h: 13 }; if (t === 'range') return { w: 129, h: 20 }; if (t === 'color') return { w: 50, h: 27 }; return null; }
    case 'select': return null;
    case 'progress': case 'meter': return { w: 160, h: 16 };
    }
    return null;
  }
  // ---- block layout. cw/ch: the containing block's content size
  layoutBlock(box, cw, ch, isRoot) {
    this.edges(box, cw);
    const cs = box.cs;
    const rep = this.replacedSize(box);
    let w = this.widthProp(box, 'width', cw);
    box.inCW = cw; box.inCH = ch; box.viaFixed = false; box.defW = typeof w === 'number';
    if (typeof w === 'string') w = this.shrinkToFit(box, cw, w);
    if (w == null) {
      if (box.kind === 'block' || box.kind === 'flex' || box.kind === 'grid' || isRoot) {
        if (box.positioned && (box.position === 'absolute' || box.position === 'fixed')) w = this.shrinkToFit(box, cw, 'fit-content');
        else if (rep && box.replaced) w = rep.w + box.pl + box.pr + box.bl + box.br;
        else if (box.flexItem) w = this.shrinkToFit(box, cw, 'fit-content');
        else w = Math.max(0, cw - box.ml - box.mr);
      } else {
        if (rep) w = rep.w + box.pl + box.pr + box.bl + box.br;
        else w = this.shrinkToFit(box, cw, 'fit-content');
      }
    }
    w = this.clampW(box, w, cw);
    box.w = w;
    if ((box.kind === 'block' || box.kind === 'flex' || box.kind === 'grid') && !box.flexItem && !box.positionedAbs) {
      const free = cw - w - box.ml - box.mr;
      if (free > 0 && box.autoML && box.autoMR) { box.ml += free / 2; box.mr += free / 2; }
      else if (free > 0 && box.autoML) box.ml += free;
    }
    let h = this.heightProp(box, 'height', ch);
    box.specifiedH = h; box.defH = h != null;
    const ar = cs.get('aspect-ratio');
    if (h == null && ar && ar !== 'auto') { const m = ar.match(/([\d.]+)\s*\/\s*([\d.]+)/); const r = m ? parseFloat(m[1]) / parseFloat(m[2]) : parseFloat(ar); if (r > 0) h = box.cw() / r + box.pt + box.pb + box.bt + box.bb; }
    if (h == null && rep && box.replaced) { h = (box.cw() && rep.w ? box.cw() * rep.h / rep.w : rep.h) + box.pt + box.pb + box.bt + box.bb; }
    const innerW = box.cw();
    const innerH = h == null ? null : Math.max(0, h - box.pt - box.pb - box.bt - box.bb);
    let contentH;
    if (box.replaced && rep) contentH = innerH == null ? rep.h : innerH;
    else if (box.kind === 'flex' || box.kind === 'inline-flex') contentH = this.layoutFlex(box, innerW, innerH);
    else if (box.kind === 'grid' || box.kind === 'inline-grid') contentH = layoutGrid(this, box, innerW, innerH);
    else contentH = this.layoutFlow(box, innerW, innerH);
    if (h == null) h = contentH + box.pt + box.pb + box.bt + box.bb;
    h = this.clampH(box, h, ch);
    box.h = h;
    if (box.el && box.el.localName === 'textarea' && box.specifiedH == null && box.cs.get('height') === 'auto') { const rows = box.el.rows || 2; box.h = Math.max(box.h, rows * box.cs.lineHeight + box.pt + box.pb + box.bt + box.bb); }
    this.relativeOffset(box, cw, ch);
    return box.h;
  }
  relativeOffset(box, cw, ch) {
    box.rx = 0; box.ry = 0;
    if (box.position === 'relative' || box.position === 'sticky') {
      const l = box.cs.len('left', cw), r = box.cs.len('right', cw), t = box.cs.len('top', ch), b = box.cs.len('bottom', ch);
      box.rx = l != null ? l : r != null ? -r : 0; box.ry = t != null ? t : b != null ? -b : 0;
    }
    const tr = box.cs.get('transform');
    if (tr && tr !== 'none') { const t = parseTranslate(tr, box); box.rx += t.x; box.ry += t.y; }
    const tl = box.cs.get('translate');
    if (tl && tl !== 'none') { const p = splitSpaces(tl); box.rx += parseLength(p[0], box.cs.ctx(), box.w) || 0; box.ry += parseLength(p[1] || '0', box.cs.ctx(), box.h) || 0; }
  }
  // the width content wants, within avail
  shrinkToFit(box, avail, mode) {
    const edges = box.pl + box.pr + box.bl + box.br;
    const max = this.intrinsicWidth(box, Infinity) + edges;
    if (mode === 'max-content') return max;
    const a = Math.max(0, avail - box.ml - box.mr);
    // the content fits as it is: its narrowest form is not needed
    if (mode !== 'min-content' && max <= a) return max;
    const min = this.intrinsicWidth(box, 0) + edges;
    if (mode === 'min-content') return min;
    return Math.min(max, Math.max(min, a));
  }
  // content width when laid out at a given width (Infinity: no wrap);
  // remembered on the box for the pass (a box lives one pass)
  intrinsicWidth(box, availW) {
    const rep = this.replacedSize(box);
    if (box.replaced && rep) return rep.w;
    const memo = box.iw || (box.iw = new Map());
    let w = memo.get(availW);
    if (w === undefined) { w = this.intrinsicWidthRun(box, availW); memo.set(availW, w); }
    return w;
  }
  intrinsicWidthRun(box, availW) {
    const saved = this.saveTree(box);
    let w;
    if (box.kind === 'flex' || box.kind === 'inline-flex') w = this.flexIntrinsic(box, availW);
    else if (box.kind === 'grid' || box.kind === 'inline-grid') { layoutGrid(this, box, availW === Infinity ? null : availW, null); w = box.contentW; }
    else { this.layoutFlow(box, availW === Infinity ? 1e7 : availW, null, true); w = box.contentW; }
    this.restoreTree(box, saved);
    return w;
  }
  saveTree(box) { return null; }
  restoreTree(box, s) {}
  // ---- flow: blocks stacked, inline content in lines
  layoutFlow(box, innerW, innerH, measuring) {
    const x0 = box.cx(), y0 = box.cy();
    let y = y0, maxW = 0, lastMarginBottom = 0, first = true, lastBase = null;
    const kids = box.children;
    box.floats = [];
    const isInline = k => k.kind === 'text' || k.kind === 'inline' || k.kind === 'inline-block' || k.kind === 'inline-flex' || k.kind === 'inline-grid';
    let i = 0;
    while (i < kids.length) {
      const k = kids[i];
      if (k.kind !== 'text' && (k.position === 'absolute' || k.position === 'fixed')) { k.staticX = this.floatLeft(box, x0, y); k.staticY = y; this.absQueue.push(k); i++; continue; }
      if (k.kind !== 'text' && !measuring && (k.cs.get('float') === 'left' || k.cs.get('float') === 'right')) { this.placeFloat(box, k, x0, y, innerW, innerH); maxW = Math.max(maxW, k.ml + k.w + k.mr); i++; continue; }
      if (k.kind !== 'text' && k.cs.get('clear') !== 'none') y = Math.max(y, this.clearY(box, k.cs.get('clear')));
      if (isInline(k)) {
        // a run of inline content makes lines
        let j = i; while (j < kids.length && (isInline(kids[j]) || (kids[j].position === 'absolute' || kids[j].position === 'fixed'))) j++;
        const run = kids.slice(i, j);
        for (const r of run) if (!isInline(r)) { r.staticX = this.floatLeft(box, x0, y); r.staticY = y; this.absQueue.push(r); }
        const lines = this.layoutInline(box, run.filter(isInline), x0, y, innerW, measuring);
        for (const ln of lines) { y += ln.h; maxW = Math.max(maxW, ln.w); if (ln.frags.some(f => f.kind === 'text' || f.kind === 'atomic')) lastBase = ln.baseline; }
        if (lines.length) lastMarginBottom = 0;
        first = false;
        i = j; continue;
      }
      // measuring: a block child counts its natural width, no layout
      if (measuring) {
        k.flexItem = false;
        this.edges(k, innerW >= 1e7 ? 0 : innerW);
        const wp = this.widthProp(k, 'width', innerW >= 1e7 ? null : innerW);
        let kw = typeof wp === 'number' ? wp : this.intrinsicWidth(k, innerW >= 1e7 ? Infinity : innerW) + k.pl + k.pr + k.bl + k.br;
        kw = this.clampW(k, kw, innerW >= 1e7 ? null : innerW);
        maxW = Math.max(maxW, kw + k.ml + k.mr);
        first = false;
        i++; continue;
      }
      // a block child: margins collapse with the one before
      k.flexItem = false;
      this.edges(k, innerW);
      const mt = k.mt;
      let gap = first ? mt : Math.max(mt, lastMarginBottom) - lastMarginBottom;
      if (first && !box.pt && !box.bt && box.parent && box.kind === 'block' && !box.clips && box.cs.get('display') !== 'inline-block' && !box.flexItem) { box.collapsedTop = Math.max(box.collapsedTop || 0, mt); gap = 0; }
      y += gap;
      k.x = x0 + k.ml; k.y = y;
      this.layoutBlock(k, innerW, innerH);
      // auto margins were resolved inside: the box follows them
      if (k.x !== x0 + k.ml) this.moveBox(k, x0 + k.ml - k.x, 0);
      y += k.h; lastMarginBottom = k.mb;
      maxW = Math.max(maxW, k.ml + k.w + k.mr);
      if (k.baseline != null && !k.clips) lastBase = k.y + k.baseline;
      first = false;
      i++;
    }
    if (!first) y += lastMarginBottom;
    // a formatting context root holds its floats
    if (box.floats.length && this.bfcRoot(box)) for (const f of box.floats) y = Math.max(y, f.y + f.h);
    box.contentW = maxW; box.contentH = y - y0;
    box.baseline = lastBase == null ? null : lastBase - box.y;
    if (box.el && box.el.localName === 'body' && box.parent && !box.parent.parent) { /* root */ }
    return box.contentH;
  }
  // a float: shrink-to-fit, at the current y, beside earlier floats
  placeFloat(box, k, x0, y, innerW, innerH) {
    const side = k.cs.get('float');
    k.flexItem = false;
    this.edges(k, innerW);
    let w = this.widthProp(k, 'width', innerW);
    if (typeof w !== 'number') w = this.shrinkToFit(k, innerW, 'fit-content');
    w = this.clampW(k, w, innerW);
    const fb = this.floatBand(box, y);
    const l = fb ? Math.max(x0, fb.l) : x0, r = fb ? Math.min(x0 + innerW, fb.r) : x0 + innerW;
    k.x = side === 'left' ? l + k.ml : r - k.mr - w; k.y = y + k.mt;
    this.layoutBlockFixed(k, innerW, innerH, w, null);
    box.floats.push({ side, x: k.x - k.ml, y: k.y - k.mt, w: k.ml + k.w + k.mr, h: k.mt + k.h + k.mb });
  }
  // the free x range at y beside the floats of this block and of
  // the blocks above it in the same formatting context
  floatBand(box, y) {
    let l = -Infinity, r = Infinity, any = false;
    for (let b = box; b; b = b.parent) {
      for (const f of b.floats || []) if (f.y <= y && y < f.y + f.h) { any = true; if (f.side === 'left') l = Math.max(l, f.x + f.w); else r = Math.min(r, f.x); }
      if (this.bfcRoot(b)) break;
    }
    return any ? { l, r } : null;
  }
  floatLeft(box, x0, y) { const fb = this.floatBand(box, y); return fb ? Math.max(x0, fb.l) : x0; }
  clearY(box, side) {
    let y = -Infinity;
    for (let b = box; b; b = b.parent) { for (const f of b.floats || []) if (side === 'both' || f.side === side) y = Math.max(y, f.y + f.h); if (this.bfcRoot(b)) break; }
    return y;
  }
  bfcRoot(b) { return !b.parent || b.clips || b.kind !== 'block' || b.flexItem || b.positionedAbs || b.cs.get('display') === 'flow-root' || b.cs.get('display') === 'inline-block' || b.cs.get('float') !== 'none'; }
  // ---- inline formatting
  layoutInline(box, items, x0, y0, availW, measuring) {
    const cs = box.cs;
    const ws = cs.get('white-space');
    const nowrap = ws === 'nowrap' || ws === 'pre';
    const align = cs.get('text-align');
    const lines = [];
    for (const k of items) this.resetInline(k, x0, y0);
    let line = this.newLine(x0, y0, availW, cs, box);
    const pushLine = () => { if (line.frags.length || line.forced) { this.finishLine(line, box, align, lines); } line = this.newLine(x0, y0 + lines.reduce((a, l) => a + l.h, 0), availW, cs, box); };
    const ctx = { box, lines, availW, nowrap, measuring, x0 };
    const self = this;
    function add(frag) {
      // a fragment that does not fit starts a new line
      if (!nowrap && line.frags.length && line.x + frag.w > line.x0 + line.availW + 0.01 && frag.breakBefore !== false) pushLine();
      frag.x = line.x; frag.line = line;
      line.frags.push(frag); line.x += frag.w;
      line.ascent = Math.max(line.ascent, frag.ascent); line.descent = Math.max(line.descent, frag.descent);
      line.h = Math.max(line.h, frag.lineH || 0);
    }
    const walk = (k, parentCs, depth) => {
      if (k.kind === 'text') { this.textFrags(k, k.cs, add, ws, () => { line.forced = true; pushLine(); }, line.h === 0 && !line.frags.length, () => line, box); return; }
      if (k.kind === 'inline') {
        this.edges(k, availW);
        k.lineStart = line; k.frags = [];
        // inline box edges become fragments of their own
        if (k.ml || k.pl || k.bl) add({ w: k.ml + k.pl + k.bl, ascent: 0, descent: 0, kind: 'edge', box: k, start: true, lineH: 0 });
        for (const c of k.children) {
          if (c.kind !== 'text' && (c.position === 'absolute' || c.position === 'fixed')) { c.staticX = line.x; c.staticY = line.y; this.absQueue.push(c); continue; }
          walk(c, k.cs, depth + 1);
        }
        if (k.mr || k.pr || k.br) add({ w: k.mr + k.pr + k.br, ascent: 0, descent: 0, kind: 'edge', box: k, start: false, lineH: 0 });
        k.inlineParentLine = true;
        return;
      }
      // atomic inline: inline-block, inline-flex, replaced
      k.flexItem = false;
      this.edges(k, availW);
      this.layoutBlock(k, availW, null);
      const va = k.cs.get('vertical-align');
      const total = k.mt + k.h + k.mb;
      let ascent, descent;
      const bl = k.baseline != null && k.kind !== 'inline-flex' && k.el && !k.replaced && !k.clips ? k.baseline : null;
      if (va === 'middle') { const xh = cs.fontSize * 0.5; ascent = total / 2 + xh / 2; descent = total - ascent; }
      else if (va === 'top' || va === 'bottom' || va === 'text-top' || va === 'text-bottom') { ascent = total; descent = 0; }
      else if (bl != null) { ascent = bl + k.mt; descent = total - ascent; }
      else { ascent = total; descent = 0; }
      add({ w: k.ml + k.w + k.mr, ascent, descent, kind: 'atomic', box: k, lineH: total, va });
    };
    for (const k of items) walk(k, cs, 0);
    if (line.frags.length || !lines.length && items.some(i => i.kind === 'text' && i.text === '')) this.finishLine(line, box, align, lines);
    else if (line.frags.length) this.finishLine(line, box, align, lines);
    // an empty block with only whitespace has no line
    return lines;
  }
  newLine(x0, y, availW, cs, box) {
    const fb = box ? this.floatBand(box, y) : null;
    if (fb) { const r = Math.min(x0 + availW, fb.r); x0 = Math.max(x0, fb.l); availW = Math.max(0, r - x0); }
    return { x: x0, y, x0, w: 0, h: 0, ascent: 0, descent: 0, frags: [], availW, strut: cs.lineHeight };
  }
  finishLine(line, box, align, lines) {
    const cs = box.cs;
    // the line's height: the strut and the tallest fragment
    const fontH = this.fontHeights(cs);
    let ascent = Math.max(line.ascent, fontH.ascent + (cs.lineHeight - fontH.ascent - fontH.descent) / 2);
    let descent = Math.max(line.descent, fontH.descent + (cs.lineHeight - fontH.ascent - fontH.descent) / 2);
    if (!line.frags.some(f => f.kind === 'text' || f.kind === 'atomic')) { ascent = line.ascent; descent = line.descent; }
    if (!line.frags.length && line.forced) { ascent = fontH.ascent + (cs.lineHeight - fontH.ascent - fontH.descent) / 2; descent = cs.lineHeight - ascent; }
    const h = Math.max(line.h, ascent + descent);
    line.h = h; line.baseline = line.y + ascent;
    line.w = line.x - line.x0;
    // trailing spaces do not count
    const last = line.frags[line.frags.length - 1];
    if (last && last.kind === 'text' && last.trailingSpace) { line.w -= last.spaceW; }
    let shift = 0;
    const free = line.availW - line.w;
    if (free > 0) { if (align === 'center') shift = free / 2; else if (align === 'right' || align === 'end') shift = free; }
    for (const f of line.frags) {
      f.x += shift;
      if (f.kind === 'atomic') {
        const b = f.box; const total = b.mt + b.h + b.mb;
        let top;
        if (f.va === 'top' || f.va === 'text-top') top = line.y; else if (f.va === 'bottom' || f.va === 'text-bottom') top = line.y + h - total; else top = line.baseline - f.ascent;
        this.moveBox(b, f.x + b.ml - b.x, top + b.mt - b.y);
      } else if (f.kind === 'text') {
        f.y = line.baseline; // the pen's baseline
        f.top = line.baseline - f.ascent;
        if (f.box) { f.box.fragsOut = f.box.fragsOut || []; f.box.fragsOut.push(f); }
      }
    }
    // inline boxes take the extent of their fragments on this line
    lines.push(line);
    for (const f of line.frags) { if (!f.box) continue; let b = f.kind === 'edge' ? f.box : f.box.parent; while (b && b !== box && b.kind === 'inline') { this.extendInline(b, f, line); b = b.parent; } }
  }
  extendInline(ib, f, line) {
    const ascent = this.fontHeights(ib.cs).ascent, descent = this.fontHeights(ib.cs).descent;
    const top = line.baseline - ascent - ib.pt - ib.bt, bottom = line.baseline + descent + ib.pb + ib.bb;
    // the box's own edge fragment: its rect leaves the margin out
    let fx = f.x, fw = f.w;
    if (f.kind === 'edge' && f.box === ib) { if (f.start) { fx += ib.ml; fw -= ib.ml; } else fw -= ib.mr; }
    if (!ib.rects) ib.rects = [];
    let r = ib.rects.find(r => r.line === line);
    if (!r) { r = { line, x: fx, y: top, w: fw, h: bottom - top }; if (!ib.rects.length) { ib.x = fx; ib.y = top; ib.w = 0; ib.h = 0; } ib.rects.push(r); }
    else { r.w = fx + fw - r.x; }
    ib.x = Math.min(ib.x, r.x); ib.y = Math.min(ib.y, top); ib.w = Math.max(ib.w, r.x + r.w - ib.x); ib.h = Math.max(ib.h, bottom - ib.y);
  }
  // an inline box starts each pass empty at the line's start
  resetInline(k, x, y) { if (k.kind !== 'inline') return; k.rects = []; k.x = x; k.y = y; k.w = 0; k.h = 0; for (const c of k.children) this.resetInline(c, x, y); }
  moveBox(b, dx, dy) {
    if (!dx && !dy) return;
    b.x += dx; b.y += dy;
    // an absolute child's static place moves with its flow
    if (b.staticX != null) { b.staticX += dx; b.staticY += dy; }
    if (b.floats) for (const f of b.floats) { f.x += dx; f.y += dy; }
    for (const c of b.children) this.moveBox(c, dx, dy);
    if (b.lines) for (const l of b.lines) { l.y += dy; l.baseline += dy; for (const f of l.frags) { f.x += dx; if (f.y != null) f.y += dy; if (f.top != null) f.top += dy; } }
    if (b.rects) for (const r of b.rects) { r.x += dx; r.y += dy; }
    if (b.fragsOut) for (const f of b.fragsOut) { /* moved with lines */ }
    if (b.textFrags) for (const f of b.textFrags) { f.x += dx; f.y += dy; f.top += dy; }
  }
  fontHeights(cs) {
    const spec = this.shaper.spec(cs);
    if (!spec.face) return { ascent: cs.fontSize * 0.8, descent: cs.fontSize * 0.2 };
    const m = spec.face.metrics(cs.fontSize);
    return { ascent: m.ascent, descent: m.descent };
  }
  // text node into word fragments
  textFrags(k, cs, add, ws, forceBreak, lineEmpty, currentLine, container) {
    let text = k.text;
    const spec = this.shaper.spec(cs);
    const fh = this.fontHeights(cs);
    const lh = cs.lineHeight;
    const half = (lh - fh.ascent - fh.descent) / 2;
    const ascent = fh.ascent + half, descent = fh.descent + half;
    const tt = cs.get('text-transform');
    if (tt === 'uppercase') text = text.toUpperCase(); else if (tt === 'lowercase') text = text.toLowerCase(); else if (tt === 'capitalize') text = text.replace(/(^|\s)(\S)/g, (m, a, b) => a + b.toUpperCase());
    const pre = ws === 'pre' || ws === 'pre-wrap' || ws === 'break-spaces';
    const preLines = ws === 'pre-line';
    const keepNewlines = pre || preLines;
    k.textFrags = [];
    if (!pre) text = text.replace(/[ \t\r\f]+/g, ' ');
    if (!keepNewlines) text = text.replace(/\n/g, ' ');
    const mkFrag = (s, isSpace) => {
      const run = this.shaper.shape(s, spec); run.size = spec.size;
      const f = { w: run.width, ascent, descent, kind: 'text', run, text: s, box: k, cs, lineH: lh, trailingSpace: false, spaceW: 0, color: cs.color(), isSpace };
      k.textFrags.push(f);
      return f;
    };
    const spaceW = this.shaper.shape(' ', spec).width;
    const parts = keepNewlines ? text.split('\n') : [text];
    const breakAll = cs.get('word-break') === 'break-all' || cs.get('overflow-wrap') === 'anywhere' || cs.get('overflow-wrap') === 'break-word' || cs.get('word-wrap') === 'break-word';
    parts.forEach((part, pi) => {
      if (pi > 0) forceBreak();
      if (pre) {
        if (part === '' && pi === parts.length - 1) return;
        const f = mkFrag(part, false); f.breakBefore = false; add(f);
        return;
      }
      // words and the spaces between them
      const tokens = part.split(/( )/).filter(t => t !== '');
      let pendingSpace = false;
      for (const tok of tokens) {
        if (tok === ' ') {
          const ln = currentLine();
          // a space at the start of a line is dropped
          if (!ln.frags.length || (ln.frags.length && ln.frags[ln.frags.length - 1].trailingSpace)) { continue; }
          const last = ln.frags[ln.frags.length - 1];
          if (last && last.kind === 'text' && last.box === k && !last.trailingSpace) { last.text += ' '; last.w += spaceW; last.trailingSpace = true; last.spaceW = spaceW; ln.x += spaceW; last.run = this.shaper.shape(last.text, spec); last.run.size = spec.size; }
          else { const f = mkFrag(' ', true); f.trailingSpace = true; f.spaceW = spaceW; f.breakBefore = false; add(f); }
          continue;
        }
        let f = mkFrag(tok, false);
        const ln = currentLine();
        const availHere = ln.availW - (ln.x - ln.x0);
        // a word wider than the line overflows, unless breaking is allowed
        if (f.w > ln.availW + 0.5 && breakAll && !(ws === 'nowrap')) {
          // a word longer than the line: cut it by character
          if (ln.frags.length && f.w > availHere) forceBreakSoft();
          let rest = tok;
          while (rest.length) {
            const lnow = currentLine();
            const room = lnow.availW - (lnow.x - lnow.x0);
            let n = rest.length;
            while (n > 1 && this.shaper.shape(rest.slice(0, n), spec).width > room) n--;
            const piece = mkFrag(rest.slice(0, n), false); piece.breakBefore = false;
            add(piece);
            rest = rest.slice(n);
            if (rest.length) forceBreakSoft();
          }
          continue;
        }
        add(f);
      }
    });
    function forceBreakSoft() { forceBreak(); }
  }
  // ---- flex
  layoutFlex(box, innerW, innerH) {
    const cs = box.cs;
    const dir = cs.get('flex-direction');
    const row = dir === 'row' || dir === 'row-reverse';
    const reverse = dir.endsWith('reverse');
    const wrap = cs.get('flex-wrap') !== 'nowrap';
    const items = box.children.filter(k => !(k.kind === 'text' && !k.text.trim()) && !(k.position === 'absolute' || k.position === 'fixed'));
    for (const k of box.children) if (k.kind !== 'text' && (k.position === 'absolute' || k.position === 'fixed')) { k.staticX = box.cx(); k.staticY = box.cy(); this.absQueue.push(k); }
    // text runs directly inside a flex box become anonymous blocks
    const wrapped = items.map(k => { if (k.kind === 'text' || k.kind === 'inline') { const anon = new Box(null, k.cs.parent || k.cs, 'block'); anon.anonymous = true; anon.parent = box; anon.children = [k]; anon.cs = this.page.anonymousStyle(box.cs); k.parent = anon; return anon; } return k; });
    const order = wrapped.map((k, i) => ({ k, i, o: parseInt(k.cs.get('order')) || 0 })).sort((a, b) => a.o - b.o || a.i - b.i).map(x => x.k);
    const mainSize = row ? innerW : innerH;
    const crossSize = row ? innerH : innerW;
    const gapMain = this.gapOf(cs, row ? 'column-gap' : 'row-gap', mainSize);
    const gapCross = this.gapOf(cs, row ? 'row-gap' : 'column-gap', crossSize);
    // base sizes
    const alignItems = cs.get('align-items') === 'normal' ? 'stretch' : cs.get('align-items');
    const infos = order.map(k => {
      k.flexItem = true; k.flexRow = row;
      // a column item not stretched fits its content
      let as = k.cs.get('align-self'); if (as === 'auto') as = alignItems; if (as === 'normal') as = 'stretch';
      k.flexItemColumnAuto = !row && (as !== 'stretch' || k.cs.get('margin-left') === 'auto' || k.cs.get('margin-right') === 'auto');
      const f = this.flexInputs(k, row, innerW, innerH, mainSize);
      k.inFlex = { w: innerW, h: innerH, main: mainSize, base: f.base, min: f.min };
      return { k, grow: f.grow, shrink: f.shrink, base: f.base, min: f.min, max: f.max, mainEdges: f.mainEdges, hyp: f.base };
    });
    // lines
    const lines = [];
    if (!wrap || mainSize == null) lines.push(infos);
    else {
      let cur = [], used = 0;
      for (const it of infos) { const need = it.base + it.mainEdges + (cur.length ? gapMain : 0); if (cur.length && used + need > mainSize + 0.01) { lines.push(cur); cur = []; used = 0; } cur.push(it); used += it.base + it.mainEdges + (cur.length > 1 ? gapMain : 0); }
      if (cur.length) lines.push(cur);
    }
    const lineCross = [];
    let crossPos = 0;
    const justify = cs.get('justify-content');
    const available = mainSize == null ? null : mainSize;
    for (const line of lines) {
      // resolve flexible lengths
      const gaps = gapMain * Math.max(0, line.length - 1);
      const sumBase = line.reduce((a, it) => a + it.base + it.mainEdges, 0) + gaps;
      let free = available == null ? 0 : available - sumBase;
      const grows = free > 0;
      for (const it of line) { it.frozen = false; it.main = it.base; }
      for (let iter = 0; iter < 8; iter++) {
        const active = line.filter(it => !it.frozen);
        if (!active.length) break;
        const used = line.reduce((a, it) => a + (it.frozen ? it.main : it.base) + it.mainEdges, 0) + gaps;
        free = available == null ? 0 : available - used;
        const totalFactor = active.reduce((a, it) => a + (grows ? it.grow : it.shrink * it.base), 0);
        let violated = false;
        for (const it of active) {
          let size = it.base;
          if (free > 0 && grows && totalFactor > 0) size = it.base + free * it.grow / totalFactor;
          else if (free < 0 && !grows && totalFactor > 0) size = it.base + free * (it.shrink * it.base) / totalFactor;
          const clamped = Math.max(it.min, Math.min(it.max, size));
          if (clamped !== size) { it.frozen = true; violated = true; }
          it.main = clamped;
        }
        if (!violated) { for (const it of active) it.frozen = true; break; }
        // frozen items keep their clamped size; the rest share again
        for (const it of active) if (!it.frozen) it.main = it.base;
        if (totalFactor === 0) break;
      }
      // cross sizes: lay each item out at its main size
      let lineCrossSize = 0;
      for (const it of line) {
        const k = it.k;
        if (row) { k.fixedW = it.main; this.layoutFlexItem(k, it.main, innerW, innerH, true); lineCrossSize = Math.max(lineCrossSize, k.mt + k.h + k.mb); k.inFlex.cross = k.h; }
        else { k.fixedH = it.main; this.layoutFlexItem(k, it.main, innerW, innerH, false); lineCrossSize = Math.max(lineCrossSize, k.ml + k.w + k.mr); k.inFlex.cross = k.w; }
      }
      if (!wrap && crossSize != null) lineCrossSize = crossSize;
      lineCross.push(lineCrossSize);
    }
    // align-content for wrapped lines
    const totalCross = lineCross.reduce((a, b) => a + b, 0) + gapCross * Math.max(0, lines.length - 1);
    let crossStart = 0, crossGapExtra = 0;
    if (crossSize != null && lines.length > 1) {
      const ac = cs.get('align-content');
      const freeC = crossSize - totalCross;
      if (ac === 'center') crossStart = freeC / 2; else if (ac === 'flex-end' || ac === 'end') crossStart = freeC; else if (ac === 'space-between' && lines.length > 1) crossGapExtra = freeC / (lines.length - 1); else if (ac === 'space-around') { crossGapExtra = freeC / lines.length; crossStart = crossGapExtra / 2; } else if (ac === 'stretch' || ac === 'normal') { for (let i = 0; i < lineCross.length; i++) lineCross[i] += freeC / lines.length; }
    }
    const x0 = box.cx(), y0 = box.cy();
    crossPos = crossStart;
    lines.forEach((line, li) => {
      const lcs = lineCross[li];
      const used = line.reduce((a, it) => a + it.main + it.mainEdges, 0) + gapMain * Math.max(0, line.length - 1);
      let mainPos = 0, extraGap = 0;
      const freeM = available == null ? 0 : available - used;
      // auto margins take the free space first
      const autos = line.reduce((a, it) => a + (row ? (it.k.autoML ? 1 : 0) + (it.k.autoMR ? 1 : 0) : (it.k.cs.get('margin-top') === 'auto' ? 1 : 0) + (it.k.cs.get('margin-bottom') === 'auto' ? 1 : 0)), 0);
      if (freeM > 0 && autos) { const each = freeM / autos; for (const it of line) { if (row) { if (it.k.autoML) it.k.ml += each; if (it.k.autoMR) it.k.mr += each; } else { if (it.k.cs.get('margin-top') === 'auto') it.k.mt += each; if (it.k.cs.get('margin-bottom') === 'auto') it.k.mb += each; } it.mainEdges = row ? it.k.ml + it.k.mr : it.k.mt + it.k.mb; } }
      else if (freeM > 0) {
        if (justify === 'center') mainPos = freeM / 2; else if (justify === 'flex-end' || justify === 'end' || justify === 'right') mainPos = freeM;
        else if (justify === 'space-between' && line.length > 1) extraGap = freeM / (line.length - 1);
        else if (justify === 'space-around') { extraGap = freeM / line.length; mainPos = extraGap / 2; }
        else if (justify === 'space-evenly') { extraGap = freeM / (line.length + 1); mainPos = extraGap; }
      }
      const seq = reverse ? line.slice().reverse() : line;
      if (reverse && freeM > 0 && (justify === 'normal' || justify === 'flex-start' || justify === 'start')) mainPos = freeM;
      for (const it of seq) {
        const k = it.k;
        let as = k.cs.get('align-self'); if (as === 'auto') as = alignItems; if (as === 'normal') as = 'stretch';
        const crossItem = row ? k.mt + k.h + k.mb : k.ml + k.w + k.mr;
        let crossOff = 0;
        const autoCrossStart = row ? k.cs.get('margin-top') === 'auto' : k.autoML, autoCrossEnd = row ? k.cs.get('margin-bottom') === 'auto' : k.autoMR;
        if (autoCrossStart && autoCrossEnd) crossOff = (lcs - crossItem) / 2; else if (autoCrossStart) crossOff = lcs - crossItem;
        else if (as === 'center') crossOff = (lcs - crossItem) / 2;
        else if (as === 'flex-end' || as === 'end' || as === 'self-end') crossOff = lcs - crossItem;
        else if (as === 'baseline') crossOff = 0;
        else if (as === 'stretch') {
          const hasSize = row ? (k.cs.get('height') !== 'auto') : (k.cs.get('width') !== 'auto');
          if (!hasSize) { if (row) { const nh = lcs - k.mt - k.mb; if (Math.abs(nh - k.h) > 0.01) { k.fixedH = this.clampH(k, nh, innerH); this.layoutFlexItem(k, it.main, innerW, innerH, true); } } else { const nw = lcs - k.ml - k.mr; if (Math.abs(nw - k.w) > 0.01) { k.fixedW = this.clampW(k, nw, innerW); this.layoutFlexItem(k, it.main, innerW, innerH, false); } } }
        }
        const mainOff = mainPos;
        if (row) this.moveBox(k, x0 + mainOff + k.ml - k.x, y0 + crossPos + crossOff + k.mt - k.y);
        else this.moveBox(k, x0 + crossPos + crossOff + k.ml - k.x, y0 + mainOff + k.mt - k.y);
        mainPos += it.main + it.mainEdges + gapMain + extraGap;
      }
      crossPos += lcs + gapCross + crossGapExtra;
    });
    const contentMain = Math.max(0, ...lines.map(line => line.reduce((a, it) => a + it.main + it.mainEdges, 0) + gapMain * Math.max(0, line.length - 1)));
    const contentCross = crossPos - crossGapExtra - (lines.length ? gapCross : 0);
    box.contentW = row ? contentMain : contentCross;
    box.contentH = row ? contentCross : contentMain;
    return row ? Math.max(0, totalCross + (crossStart > 0 ? 0 : 0)) : contentMain;
  }
  // a flex item's inputs to its container's algorithm: from its own
  // style, else from its content
  flexInputs(k, row, innerW, innerH, mainSize) {
    this.edges(k, innerW);
    const grow = parseFloat(k.cs.get('flex-grow')) || 0, shrink = k.cs.get('flex-shrink') === '' ? 1 : (parseFloat(k.cs.get('flex-shrink')) ?? 1);
    let basis = k.cs.get('flex-basis');
    const mainEdges = row ? k.ml + k.mr : k.mt + k.mb;
    let base;
    if (basis === 'content') basis = 'auto';
    if (basis === 'auto') { const sz = row ? this.widthProp(k, 'width', innerW) : this.heightProp(k, 'height', innerH); if (typeof sz === 'number') base = sz; else base = null; }
    else { base = row ? this.widthProp(k, 'flex-basis', mainSize) : this.heightProp(k, 'flex-basis', mainSize); if (typeof base === 'string') base = null; }
    if (base == null) {
      if (row) base = this.shrinkToFit(k, Infinity, 'max-content'); 
      else { this.layoutBlock(k, innerW, null); base = k.h; }
    }
    // min-width:auto on a flex item is its min-content size
    let minMain = row ? this.widthProp(k, 'min-width', innerW) : this.heightProp(k, 'min-height', innerH);
    if (minMain == null || typeof minMain === 'string') { const ov = row ? k.cs.get('overflow-x') : k.cs.get('overflow-y'); if (ov === 'visible' && !k.replaced && row) minMain = Math.min(base, this.shrinkToFit(k, Infinity, 'min-content')); else if (ov === 'visible' && !k.replaced && !row) minMain = 0; else minMain = 0; if (row && k.cs.get('width') !== 'auto' && typeof this.widthProp(k, 'width', innerW) === 'number') minMain = Math.min(minMain, this.widthProp(k, 'width', innerW)); }
    let maxMain = row ? this.widthProp(k, 'max-width', innerW) : this.heightProp(k, 'max-height', innerH);
    if (typeof maxMain !== 'number') maxMain = Infinity;
    base = Math.max(minMain, Math.min(maxMain, base));
    return { grow, shrink, base, min: minMain, max: maxMain, mainEdges };
  }
  layoutFlexItem(k, main, innerW, innerH, row) {
    // fixedW/fixedH override the item's own width/height
    k.x = 0; k.y = 0;
    const saveW = k.fixedW, saveH = k.fixedH;
    this.layoutBlockFixed(k, innerW, innerH, saveW, saveH);
  }
  layoutBlockFixed(box, cw, ch, fw, fh) {
    const cs = box.cs;
    box.inCW = cw; box.inCH = ch; box.viaFixed = true; box.inFW = fw; box.inFH = fh;
    this.edges(box, cw);
    let w = fw != null ? fw : this.widthProp(box, 'width', cw);
    const rep = this.replacedSize(box);
    if (typeof w === 'string') w = this.shrinkToFit(box, cw, w);
    if (w == null) {
      if (box.flexRow === false && !box.flexItemColumnAuto) w = Math.max(0, cw - box.ml - box.mr);
      else if (rep && box.replaced) w = rep.w + box.pl + box.pr + box.bl + box.br;
      else w = this.shrinkToFit(box, cw, 'fit-content');
    }
    if (fw == null) w = this.clampW(box, w, cw);
    box.w = w;
    let h = fh != null ? fh : this.heightProp(box, 'height', ch);
    box.specifiedH = h;
    const ar = cs.get('aspect-ratio');
    if (h == null && ar && ar !== 'auto') { const m = ar.match(/([\d.]+)\s*\/\s*([\d.]+)/); const r = m ? parseFloat(m[1]) / parseFloat(m[2]) : parseFloat(ar); if (r > 0) h = box.cw() / r + box.pt + box.pb + box.bt + box.bb; }
    if (h == null && rep && box.replaced) h = (box.cw() && rep.w ? box.cw() * rep.h / rep.w : rep.h) + box.pt + box.pb + box.bt + box.bb;
    const innerW = box.cw(), innerH = h == null ? null : Math.max(0, h - box.pt - box.pb - box.bt - box.bb);
    let contentH;
    if (box.replaced && rep) contentH = innerH == null ? rep.h : innerH;
    else if (box.kind === 'flex' || box.kind === 'inline-flex') contentH = this.layoutFlex(box, innerW, innerH);
    else if (box.kind === 'grid' || box.kind === 'inline-grid') contentH = layoutGrid(this, box, innerW, innerH);
    else contentH = this.layoutFlow(box, innerW, innerH);
    if (h == null) h = contentH + box.pt + box.pb + box.bt + box.bb;
    if (fh == null) h = this.clampH(box, h, ch);
    box.h = h;
    this.relativeOffset(box, cw, ch);
    box.fixedW = null; box.fixedH = null;
  }
  flexIntrinsic(box, availW) {
    const row = box.cs.get('flex-direction').startsWith('row');
    const gap = this.gapOf(box.cs, row ? 'column-gap' : 'row-gap', 0);
    let total = 0, max = 0, n = 0;
    for (const k of box.children) {
      if (k.kind === 'text' && !k.text.trim()) continue;
      if (k.position === 'absolute' || k.position === 'fixed') continue;
      this.edges(k, availW === Infinity ? 0 : availW);
      let w;
      if (k.kind === 'text' || k.kind === 'inline') { const spec = this.shaper.spec(k.cs); w = k.kind === 'text' ? this.textIntrinsic(k, spec, availW === Infinity) : this.shrinkToFit(k, availW, availW === Infinity ? 'max-content' : 'min-content'); }
      else { const wp = this.widthProp(k, 'width', availW === Infinity ? null : availW); w = typeof wp === 'number' ? wp : this.shrinkToFit(k, availW, availW === Infinity ? 'max-content' : 'min-content'); w += k.ml + k.mr; }
      if (row) { total += w + (n ? gap : 0); } else max = Math.max(max, w);
      n++;
    }
    return row ? total : max;
  }
  // a text run's widest form, or its narrowest, by its white-space
  textIntrinsic(k, spec, max) {
    const ws = k.cs.get('white-space');
    const m = t => this.shaper.measure(t, spec);
    if (ws === 'pre' || ws === 'nowrap' && max) { const t = ws === 'pre' ? k.text : k.text.replace(/\s+/g, ' ').trim(); return Math.max(...t.split('\n').map(m), 0); }
    if (ws === 'pre-wrap' || ws === 'break-spaces') return Math.max(...(max ? k.text.split('\n') : k.text.split(/[ \n]+/)).map(m), 0);
    if (ws === 'nowrap' || max) return m(k.text.replace(/\s+/g, ' ').trim());
    return Math.max(...k.text.split(/\s+/).map(m), 0);
  }
  gapOf(cs, k, base) { const v = cs.get(k); if (v === 'normal' || !v) return 0; return parseLength(v, cs.ctx(), base) || 0; }
  // ---- absolutes: after the flow, against their containing block
  placeAbsolutes() {
    const queue = this.absQueue; this.absQueue = [];
    for (const k of queue) {
      const cb = this.containingBlock(k);
      const cw = cb ? (cb.root ? this.vw : cb.clientW()) : this.vw, ch = cb ? (cb.root ? this.vh : cb.clientH()) : this.vh;
      const cx = cb && !cb.root ? cb.x + cb.bl : 0, cy = cb && !cb.root ? cb.y + cb.bt : 0;
      k.positionedAbs = true;
      this.edges(k, cw);
      const l = k.cs.len('left', cw), r = k.cs.len('right', cw), t = k.cs.len('top', ch), b = k.cs.len('bottom', ch);
      let w = this.widthProp(k, 'width', cw);
      if (typeof w === 'string') w = this.shrinkToFit(k, cw, w);
      if (w == null && l != null && r != null) w = Math.max(0, cw - l - r - k.ml - k.mr);
      if (w == null) { const rep = this.replacedSize(k); w = rep && k.replaced ? rep.w + k.pl + k.pr + k.bl + k.br : this.shrinkToFit(k, l != null ? cw - l : r != null ? cw - r : cw, 'fit-content'); }
      w = this.clampW(k, w, cw);
      let h = this.heightProp(k, 'height', ch);
      if (h == null && t != null && b != null) h = Math.max(0, ch - t - b - k.mt - k.mb);
      k.fixedW = w; k.fixedH = h;
      this.layoutBlockFixed(k, cw, ch, w, h);
      // place: insets, else the static position
      let x = l != null ? cx + l + k.ml : r != null ? cx + cw - r - k.w - k.mr : (k.staticX != null ? k.staticX + k.ml : cx);
      let y = t != null ? cy + t + k.mt : b != null ? cy + ch - b - k.h - k.mb : (k.staticY != null ? k.staticY + k.mt : cy);
      if (l != null && r != null && k.autoML && k.autoMR) x = cx + l + (cw - l - r - k.w) / 2;
      if (t != null && b != null && k.cs.get('margin-top') === 'auto' && k.cs.get('margin-bottom') === 'auto') y = cy + t + (ch - t - b - k.h) / 2;
      this.moveBox(k, x - k.x, y - k.y);
      if (k.position === 'fixed') { k.fixed = true; k.fixedRoot = this.rootBox; }
      if (this.absQueue.length) this.placeAbsolutes();
    }
  }
  containingBlock(k) {
    for (let p = k.parent; p; p = p.parent) {
      if (k.position === 'fixed') { if (p.cs && (p.cs.get('transform') !== 'none' || p.cs.get('filter') !== 'none')) return p; continue; }
      if (p.positioned || (p.cs && (p.cs.get('transform') !== 'none' || p.cs.get('filter') !== 'none'))) return p;
    }
    return this.rootBox ? Object.assign(this.rootBox, { root: true }) : null;
  }
}
// content: "..." attr(x) counters; the text it makes
export function contentText(v, el) {
  let out = '';
  for (const part of splitSpaces(v)) {
    if (part[0] === '"' || part[0] === "'") out += unquote(part);
    else if (part.startsWith('attr(')) out += el ? (el.getAttribute(part.slice(5, -1).trim()) || '') : '';
    else if (part === 'open-quote') out += '“'; else if (part === 'close-quote') out += '”';
    else if (part.startsWith('counter(')) out += '1';
  }
  return out;
}
function unquote(s) { s = s.slice(1, -1); return s.replace(/\\([0-9a-fA-F]{1,6})\s?/g, (m, h) => String.fromCodePoint(parseInt(h, 16))).replace(/\\(.)/g, '$1'); }
function parseTranslate(tr, box) {
  let x = 0, y = 0;
  const re = /(translate3d|translateX|translateY|translate|matrix)\(([^)]*)\)/g;
  let m;
  while ((m = re.exec(tr))) {
    const args = m[2].split(',').map(s => s.trim());
    const L = (s, base) => parseLength(s, box.cs.ctx(), base) || 0;
    if (m[1] === 'translate' || m[1] === 'translate3d') { x += L(args[0], box.w); y += L(args[1] || '0', box.h); }
    else if (m[1] === 'translateX') x += L(args[0], box.w); else if (m[1] === 'translateY') y += L(args[0], box.h);
    else if (m[1] === 'matrix') { x += parseFloat(args[4]) || 0; y += parseFloat(args[5]) || 0; }
  }
  return { x, y };
}
