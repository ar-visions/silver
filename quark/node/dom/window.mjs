// the window: the globals a page's scripts see, put on this worker's
// global object so classic scripts run as they would in a browser
import * as E from './events.mjs';
import { Document, Element, Text, Comment, DocumentFragment, Node, DOMException, CSSStyleSheet, CSSRule, TreeWalker, NodeIterator, HTMLElement, HTMLUnknownElement, HTMLInputElement, HTMLTextAreaElement, HTMLCanvasElement, HTMLImageElement, HTMLAnchorElement, HTMLButtonElement, HTMLDivElement, HTMLSpanElement, HTMLStyleElement, HTMLLinkElement, HTMLScriptElement, HTMLSelectElement, HTMLOptionElement, HTMLTemplateElement, HTMLIFrameElement, HTMLVideoElement, HTMLAudioElement, HTMLFormElement, HTMLLabelElement, HTMLTableElement, HTMLDialogElement, SVGElement, HTMLBodyElement, HTMLHtmlElement, HTMLHeadElement, HTMLParagraphElement, HTMLUListElement, HTMLLIElement, HTMLPreElement, HTMLHeadingElement, HTMLBRElement, HTMLTableRowElement, HTMLTableCellElement, HTMLTableSectionElement, HTMLOListElement, HTMLHRElement, HTMLProgressElement, HTMLDetailsElement, HTMLSlotElement, HTMLMetaElement, HTMLTitleElement, HTMLFieldSetElement, HTMLLegendElement, HTMLOutputElement, HTMLDataListElement, HTMLOptGroupElement, HTMLBaseElement, HTMLObjectElement, HTMLEmbedElement, HTMLPictureElement, HTMLSourceElement, HTMLTimeElement, HTMLQuoteElement, HTMLModElement, HTMLDListElement, HTMLMenuElement, HTMLTableCaptionElement, HTMLTableColElement, HTMLMeterElement, HTMLAreaElement, HTMLMapElement, HTMLTrackElement, HTMLDataElement } from './elements.mjs';
import { DOMTokenList, CSSStyleDeclaration, NodeList as NodeListClass } from './node.mjs';
import { ImageData, Path2D, CanvasRenderingContext2D } from './canvas2d.mjs';
import { parseColor } from './css.mjs';

export class DOMRectReadOnly { constructor(x = 0, y = 0, w = 0, h = 0) { this.x = x; this.y = y; this.width = w; this.height = h; } get top() { return Math.min(this.y, this.y + this.height); } get left() { return Math.min(this.x, this.x + this.width); } get right() { return Math.max(this.x, this.x + this.width); } get bottom() { return Math.max(this.y, this.y + this.height); } toJSON() { return { x: this.x, y: this.y, width: this.width, height: this.height, top: this.top, left: this.left, right: this.right, bottom: this.bottom }; } static fromRect(r = {}) { return new DOMRectReadOnly(r.x, r.y, r.width, r.height); } }
export class DOMRect extends DOMRectReadOnly { static fromRect(r = {}) { return new DOMRect(r.x, r.y, r.width, r.height); } }
export class DOMPoint { constructor(x = 0, y = 0, z = 0, w = 1) { this.x = x; this.y = y; this.z = z; this.w = w; } matrixTransform(m) { return new DOMPoint(this.x * m.a + this.y * m.c + m.e, this.x * m.b + this.y * m.d + m.f); } static fromPoint(p) { return new DOMPoint(p.x, p.y, p.z, p.w); } }
export class DOMMatrix { constructor(init) { Object.assign(this, { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0, m11: 1, m12: 0, m21: 0, m22: 1, m41: 0, m42: 0, is2D: true, isIdentity: true }); if (Array.isArray(init)) { [this.a, this.b, this.c, this.d, this.e, this.f] = init; } else if (typeof init === 'string' && init !== 'none' && init) { const m = init.match(/matrix\(([^)]*)\)/); if (m) [this.a, this.b, this.c, this.d, this.e, this.f] = m[1].split(',').map(Number); const t = init.match(/translate\(([^,)]+)(?:,([^)]+))?\)/); if (t) { this.e = parseFloat(t[1]); this.f = parseFloat(t[2] || 0); } } this.m41 = this.e; this.m42 = this.f; this.isIdentity = this.a === 1 && this.b === 0 && this.c === 0 && this.d === 1 && this.e === 0 && this.f === 0; } translate(x, y) { const m = new DOMMatrix([this.a, this.b, this.c, this.d, this.e + x, this.f + (y || 0)]); return m; } scale(x, y = x) { return new DOMMatrix([this.a * x, this.b * x, this.c * y, this.d * y, this.e, this.f]); } multiply(o) { return new DOMMatrix([this.a * o.a + this.c * o.b, this.b * o.a + this.d * o.b, this.a * o.c + this.c * o.d, this.b * o.c + this.d * o.d, this.a * o.e + this.c * o.f + this.e, this.b * o.e + this.d * o.f + this.f]); } inverse() { const det = this.a * this.d - this.b * this.c || 1; return new DOMMatrix([this.d / det, -this.b / det, -this.c / det, this.a / det, (this.c * this.f - this.d * this.e) / det, (this.b * this.e - this.a * this.f) / det]); } transformPoint(p) { return new DOMPoint(p.x * this.a + p.y * this.c + this.e, p.x * this.b + p.y * this.d + this.f); } toString() { return `matrix(${this.a}, ${this.b}, ${this.c}, ${this.d}, ${this.e}, ${this.f})`; } toFloat32Array() { return new Float32Array([this.a, this.b, 0, 0, this.c, this.d, 0, 0, 0, 0, 1, 0, this.e, this.f, 0, 1]); } }
export class DOMMatrixReadOnly extends DOMMatrix {}

export class MutationObserver {
  constructor(cb) { this.cb = cb; this.records = []; this.targets = []; }
  observe(target, opts = {}) { this.targets.push({ target, opts }); target.ownerDocument && target.ownerDocument._page && target.ownerDocument._page.observers.add(this); if (target._page) target._page.observers.add(this); }
  disconnect() { this.targets = []; this.records = []; }
  takeRecords() { const r = this.records; this.records = []; return r; }
  _consider(node, kind, detail) {
    for (const { target, opts } of this.targets) {
      const inTree = node === target || (opts.subtree && target.contains(node));
      if (!inTree) continue;
      if (kind === 'childList' && !opts.childList) continue;
      if (kind === 'attributes' && !opts.attributes && !opts.attributeFilter) continue;
      if (kind === 'attributes' && opts.attributeFilter && !opts.attributeFilter.includes(detail.attributeName)) continue;
      if (kind === 'characterData' && !opts.characterData) continue;
      if (kind === 'value') continue;
      this.records.push({ type: kind, target: node, addedNodes: detail && detail.added || [], removedNodes: detail && detail.removed || [], attributeName: detail && detail.attributeName || null, oldValue: detail && detail.oldValue !== undefined ? detail.oldValue : null, previousSibling: null, nextSibling: null, attributeNamespace: null });
      return true;
    }
    return false;
  }
  _flush() { if (!this.records.length) return; const r = this.records; this.records = []; try { this.cb(r, this); } catch (e) { E.reportError(e); } }
}
export class ResizeObserver {
  constructor(cb) { this.cb = cb; this.targets = new Map(); }
  observe(el, opts) { this.targets.set(el, { last: null, box: opts && opts.box }); el.ownerDocument._page && el.ownerDocument._page.resizeObservers.add(this); }
  unobserve(el) { this.targets.delete(el); }
  disconnect() { this.targets.clear(); }
  // after a layout: entries whose size changed
  _check() {
    const entries = [];
    for (const [el, st] of this.targets) {
      const b = el._box;
      const w = b ? b.cw() : 0, h = b ? b.ch() : 0;
      const bw = b ? b.w : 0, bh = b ? b.h : 0;
      if (!st.last || st.last.w !== w || st.last.h !== h) {
        st.last = { w, h };
        entries.push({ target: el, contentRect: new DOMRect(b ? b.pl : 0, b ? b.pt : 0, w, h), contentBoxSize: [{ inlineSize: w, blockSize: h }], borderBoxSize: [{ inlineSize: bw, blockSize: bh }], devicePixelContentBoxSize: [{ inlineSize: w, blockSize: h }] });
      }
    }
    if (entries.length) { try { this.cb(entries, this); } catch (e) { E.reportError(e); } }
  }
}
export class IntersectionObserver {
  constructor(cb, opts) { this.cb = cb; this.opts = opts || {}; this.targets = new Map(); this.root = this.opts.root || null; this.rootMargin = '0px'; this.thresholds = [0]; }
  observe(el) { this.targets.set(el, undefined); el.ownerDocument._page && el.ownerDocument._page.intersectionObservers.add(this); }
  unobserve(el) { this.targets.delete(el); } disconnect() { this.targets.clear(); } takeRecords() { return []; }
  _check(page) {
    const entries = [];
    for (const [el, last] of this.targets) {
      const r = el.getBoundingClientRect();
      const vis = r.width > 0 && r.height > 0 && r.right > 0 && r.bottom > 0 && r.left < page.width / page.scale && r.top < page.height / page.scale && isShown(el);
      if (last !== vis) { this.targets.set(el, vis); entries.push({ target: el, isIntersecting: vis, intersectionRatio: vis ? 1 : 0, boundingClientRect: r, intersectionRect: vis ? r : new DOMRect(), rootBounds: new DOMRect(0, 0, page.width / page.scale, page.height / page.scale), time: performance.now() }); }
    }
    if (entries.length) { try { this.cb(entries, this); } catch (e) { E.reportError(e); } }
  }
}
function isShown(el) { for (let n = el; n; n = n.parentElement) { if (!n._box) return false; } return true; }
export class PerformanceObserver { observe() {} disconnect() {} takeRecords() { return []; } static supportedEntryTypes = []; }
export class ReportingObserver { observe() {} disconnect() {} takeRecords() { return []; } }

// a text range: start/end nodes with offsets; rects come from layout
export class Range {
  constructor(doc) { this.doc = doc; this.startContainer = doc; this.startOffset = 0; this.endContainer = doc; this.endOffset = 0; this.collapsed = true; }
  setStart(n, o) { this.startContainer = n; this.startOffset = o; this._update(); } setEnd(n, o) { this.endContainer = n; this.endOffset = o; this._update(); }
  setStartBefore(n) { this.setStart(n.parentNode, idx(n)); } setStartAfter(n) { this.setStart(n.parentNode, idx(n) + 1); } setEndBefore(n) { this.setEnd(n.parentNode, idx(n)); } setEndAfter(n) { this.setEnd(n.parentNode, idx(n) + 1); }
  selectNode(n) { this.setStart(n.parentNode, idx(n)); this.setEnd(n.parentNode, idx(n) + 1); } selectNodeContents(n) { this.setStart(n, 0); this.setEnd(n, n.childNodes.length || (n.data ? n.data.length : 0)); }
  collapse(toStart) { if (toStart) { this.endContainer = this.startContainer; this.endOffset = this.startOffset; } else { this.startContainer = this.endContainer; this.startOffset = this.endOffset; } this.collapsed = true; }
  _update() { this.collapsed = this.startContainer === this.endContainer && this.startOffset === this.endOffset; }
  get commonAncestorContainer() { let a = this.startContainer; while (a && !a.contains(this.endContainer)) a = a.parentNode; return a; }
  cloneRange() { const r = new Range(this.doc); r.setStart(this.startContainer, this.startOffset); r.setEnd(this.endContainer, this.endOffset); return r; }
  detach() {} toString() { if (this.startContainer === this.endContainer && this.startContainer.nodeType === 3) return this.startContainer.data.slice(this.startOffset, this.endOffset); return this.startContainer.textContent || ''; }
  getClientRects() { const r = this.getBoundingClientRect(); const l = [r]; l.item = i => l[i]; return l; }
  getBoundingClientRect() {
    const page = this.doc._page;
    if (page) page.ensureLayout();
    const n = this.startContainer;
    if (n.nodeType === 3 && n._box && n._box.textFrags) {
      // the span of characters within the text's fragments
      const frags = n._box.textFrags; let x0 = null, x1 = null, top = 0, bottom = 0, consumed = 0;
      const sc = page.layout.constructor === Object ? { x: 0, y: 0 } : page.painter.parentScroll(n._box);
      const s = this.startOffset, e = this.endContainer === n ? this.endOffset : n.data.length;
      for (const f of frags) {
        const len = f.text.length; const a = consumed, b = consumed + len;
        if (b >= s && a <= e) {
          const spec = page.text.spec(f.cs);
          const from = Math.max(0, s - a), to = Math.min(len, e - a);
          const px0 = f.x + page.text.measure(f.text.slice(0, from), spec), px1 = f.x + page.text.measure(f.text.slice(0, to), spec);
          if (x0 === null || px0 < x0) x0 = px0; if (x1 === null || px1 > x1) x1 = px1;
          top = f.top; bottom = f.top + f.ascent + f.descent;
        }
        consumed += len;
        if (!f.text.endsWith(' ') && f.trailingSpace) consumed += 0;
      }
      if (x0 !== null) return new DOMRect(x0 - sc.x, top - sc.y, x1 - x0, bottom - top);
    }
    const el = n.nodeType === 1 ? n : n.parentElement;
    if (el) { const r = el.getBoundingClientRect(); return new DOMRect(r.x, r.y, r.width, r.height); }
    return new DOMRect();
  }
  deleteContents() {} extractContents() { return this.doc.createDocumentFragment(); } cloneContents() { const f = this.doc.createDocumentFragment(); f.appendChild(this.doc.createTextNode(this.toString())); return f; } insertNode(n) { if (this.startContainer.nodeType === 3) this.startContainer.parentNode.insertBefore(n, this.startContainer.splitText(this.startOffset)); else this.startContainer.insertBefore(n, this.startContainer.childNodes[this.startOffset] || null); } surroundContents() {}
  compareBoundaryPoints() { return 0; } isPointInRange() { return false; } comparePoint() { return 0; } intersectsNode(n) { return this.commonAncestorContainer && this.commonAncestorContainer.contains(n); }
  createContextualFragment(html) { return this.doc._parseFragment(html, this.startContainer.nodeType === 1 ? this.startContainer : this.startContainer.parentNode); }
}
function idx(n) { return n.parentNode ? n.parentNode.childNodes.indexOf(n) : 0; }
export class StaticRange extends Range { constructor(init) { super(init.startContainer.ownerDocument || init.startContainer); this.setStart(init.startContainer, init.startOffset); this.setEnd(init.endContainer, init.endOffset); } }
export class Selection {
  constructor(doc) { this.doc = doc; this.ranges = []; }
  get rangeCount() { return this.ranges.length; } get type() { return this.ranges.length ? (this.ranges[0].collapsed ? 'Caret' : 'Range') : 'None'; }
  get anchorNode() { return this.ranges[0] ? this.ranges[0].startContainer : null; } get anchorOffset() { return this.ranges[0] ? this.ranges[0].startOffset : 0; }
  get focusNode() { return this.ranges[0] ? this.ranges[0].endContainer : null; } get focusOffset() { return this.ranges[0] ? this.ranges[0].endOffset : 0; }
  get isCollapsed() { return !this.ranges.length || this.ranges[0].collapsed; }
  getRangeAt(i) { if (!this.ranges[i]) throw new DOMException('IndexSizeError', 'IndexSizeError'); return this.ranges[i]; }
  addRange(r) { this.ranges = [r]; this._changed(); } removeAllRanges() { this.ranges = []; this._changed(); } removeRange() { this.ranges = []; this._changed(); } empty() { this.removeAllRanges(); }
  collapse(n, o) { const r = new Range(this.doc); r.setStart(n, o || 0); r.setEnd(n, o || 0); this.ranges = [r]; this._changed(); } collapseToStart() { if (this.ranges[0]) this.ranges[0].collapse(true); } collapseToEnd() { if (this.ranges[0]) this.ranges[0].collapse(false); }
  setBaseAndExtent(a, ao, f, fo) { const r = new Range(this.doc); r.setStart(a, ao); r.setEnd(f, fo); this.ranges = [r]; this._changed(); } extend(n, o) { if (this.ranges[0]) this.ranges[0].setEnd(n, o); }
  selectAllChildren(n) { const r = new Range(this.doc); r.selectNodeContents(n); this.ranges = [r]; this._changed(); }
  containsNode(n) { return !!this.ranges[0] && this.ranges[0].intersectsNode(n); } deleteFromDocument() {} modify() {}
  toString() { return this.ranges.map(r => r.toString()).join(''); }
  _changed() { this.doc.dispatchEvent(new E.Event('selectionchange')); }
}
export class FontFace {
  constructor(family, source, desc = {}) { this.family = family; this.source = source; this.weight = desc.weight || 'normal'; this.style = desc.style || 'normal'; this.status = 'unloaded'; this.unicodeRange = desc.unicodeRange || 'U+0-10FFFF'; this.display = desc.display || 'auto'; this.loaded = new Promise((res, rej) => { this._res = res; this._rej = rej; }); }
  load() { if (this._page) return this._page.loadFontFace(this); return this.loaded; }
}
export class FontFaceSet extends E.EventTarget {
  constructor(page) { super(); this.page = page; this.faces = new Set(); this.status = 'loaded'; this.ready = Promise.resolve(this); }
  add(f) { this.faces.add(f); f._page = this.page; f.load(); return this; } delete(f) { return this.faces.delete(f); } clear() { this.faces.clear(); } has(f) { return this.faces.has(f); }
  load() { return Promise.resolve([]); } check() { return true; } forEach(fn) { this.faces.forEach(fn); } get size() { return this.faces.size; } [Symbol.iterator]() { return this.faces[Symbol.iterator](); } values() { return this.faces.values(); } entries() { return this.faces.entries(); }
}
export class Storage {
  constructor(page, name) { this._page = page; this._name = name; this._map = page ? page.loadStorage(name) : new Map(); return new Proxy(this, { get(t, k) { if (k in t) return t[k]; return t._map.get(String(k)); }, set(t, k, v) { if (k in t && typeof t[k] !== 'undefined' && k[0] === '_') { t[k] = v; return true; } t.setItem(k, v); return true; }, deleteProperty(t, k) { t.removeItem(k); return true; }, has(t, k) { return k in t || t._map.has(String(k)); }, ownKeys(t) { return [...t._map.keys()]; }, getOwnPropertyDescriptor(t, k) { return t._map.has(k) ? { enumerable: true, configurable: true, value: t._map.get(k) } : undefined; } }); }
  get length() { return this._map.size; } key(i) { return [...this._map.keys()][i] ?? null; }
  getItem(k) { const v = this._map.get(String(k)); return v === undefined ? null : v; }
  setItem(k, v) { this._map.set(String(k), String(v)); this._page && this._page.saveStorage(this._name, this._map); }
  removeItem(k) { this._map.delete(String(k)); this._page && this._page.saveStorage(this._name, this._map); }
  clear() { this._map.clear(); this._page && this._page.saveStorage(this._name, this._map); }
}
export class MediaQueryList extends E.EventTarget { constructor(q, matches) { super(); this.media = q; this.matches = matches; this.onchange = null; } addListener(f) { this.addEventListener('change', f); } removeListener(f) { this.removeEventListener('change', f); } }
export class CustomElementRegistry {
  constructor() { this.defs = new Map(); this.waiting = new Map(); }
  define(name, C, opts) { this.defs.set(name, C); const w = this.waiting.get(name); if (w) { w.resolve(C); this.waiting.delete(name); } }
  get(name) { return this.defs.get(name); } getName(C) { for (const [k, v] of this.defs) if (v === C) return k; return null; }
  whenDefined(name) { if (this.defs.has(name)) return Promise.resolve(this.defs.get(name)); let d = this.waiting.get(name); if (!d) { d = {}; d.promise = new Promise(r => { d.resolve = r; }); this.waiting.set(name, d); } return d.promise; }
  upgrade() {} _upgrade(el) { const C = this.defs.get(el.localName); if (C) { Object.setPrototypeOf(el, C.prototype); if (C.prototype.constructor !== HTMLElement) { try { /* constructor side effects are skipped */ } catch {} } } }
  _connected(el) { if (typeof el.connectedCallback === 'function') { try { el.connectedCallback(); } catch (e) { E.reportError(e); } } }
  _disconnected(el) { if (typeof el.disconnectedCallback === 'function') { try { el.disconnectedCallback(); } catch (e) { E.reportError(e); } } }
}
export class DataTransfer { constructor() { this.items = []; this.files = []; this.types = []; this._data = new Map(); this.dropEffect = 'none'; this.effectAllowed = 'all'; } setData(t, v) { this._data.set(t, v); if (!this.types.includes(t)) this.types.push(t); } getData(t) { return this._data.get(t) || ''; } clearData() { this._data.clear(); this.types = []; } setDragImage() {} }
export class Location {
  constructor(page, href) { this._page = page; this._url = new URL(href); }
  get href() { return this._url.href; } set href(v) { this._page.navigate(v); }
  get protocol() { return this._url.protocol; } get host() { return this._url.host; } get hostname() { return this._url.hostname; } get port() { return this._url.port; } get pathname() { return this._url.pathname; } get search() { return this._url.search; } get hash() { return this._url.hash; } set hash(v) { this._url.hash = v; } get origin() { return this._url.origin; }
  assign(v) { this._page.navigate(v); } replace(v) { this._page.navigate(v); } reload() { this._page.navigate(this.href); } toString() { return this.href; }
}
export class History { constructor() { this.state = null; this.length = 1; this.scrollRestoration = 'auto'; } pushState(s) { this.state = s; this.length++; } replaceState(s) { this.state = s; } back() {} forward() {} go() {} }

export function installGlobals(page) {
  const g = globalThis;
  const doc = page.document;
  const def = (k, v) => { try { Object.defineProperty(g, k, { value: v, writable: true, configurable: true, enumerable: false }); } catch {} };
  const classes = { Event: E.Event, CustomEvent: E.CustomEvent, UIEvent: E.UIEvent, MouseEvent: E.MouseEvent, PointerEvent: E.PointerEvent, WheelEvent: E.WheelEvent, KeyboardEvent: E.KeyboardEvent, FocusEvent: E.FocusEvent, InputEvent: E.InputEvent, CompositionEvent: E.CompositionEvent, ClipboardEvent: E.ClipboardEvent, DragEvent: E.DragEvent, MessageEvent: E.MessageEvent, ErrorEvent: E.ErrorEvent, PromiseRejectionEvent: E.PromiseRejectionEvent, ProgressEvent: E.ProgressEvent, TransitionEvent: E.TransitionEvent, AnimationEvent: E.AnimationEvent, HashChangeEvent: E.HashChangeEvent, PopStateEvent: E.PopStateEvent, StorageEvent: E.StorageEvent, BeforeUnloadEvent: E.BeforeUnloadEvent, EventTarget: E.EventTarget,
    Node, Element, Text, Comment, Document, DocumentFragment, HTMLDocument: Document, DOMException, CSSStyleSheet, CSSRule, CSSStyleRule: CSSRule, CSSMediaRule: CSSRule, CSSKeyframesRule: CSSRule, CSSStyleDeclaration, DOMTokenList, TreeWalker, NodeIterator, NodeFilter: { SHOW_ALL: 0xFFFFFFFF, SHOW_ELEMENT: 1, SHOW_TEXT: 4, SHOW_COMMENT: 128, FILTER_ACCEPT: 1, FILTER_REJECT: 2, FILTER_SKIP: 3 },
    HTMLElement, HTMLUnknownElement, HTMLInputElement, HTMLTextAreaElement, HTMLCanvasElement, HTMLImageElement, HTMLAnchorElement, HTMLButtonElement, HTMLDivElement, HTMLSpanElement, HTMLStyleElement, HTMLLinkElement, HTMLScriptElement, HTMLSelectElement, HTMLOptionElement, HTMLTemplateElement, HTMLIFrameElement, HTMLVideoElement, HTMLAudioElement, HTMLMediaElement: HTMLVideoElement, HTMLFormElement, HTMLLabelElement, HTMLTableElement, HTMLDialogElement, SVGElement, SVGSVGElement: SVGElement, SVGGraphicsElement: SVGElement, HTMLBodyElement, HTMLHtmlElement, HTMLHeadElement, HTMLParagraphElement, HTMLUListElement, HTMLLIElement, HTMLPreElement, HTMLHeadingElement, HTMLBRElement, HTMLTableRowElement, HTMLTableCellElement, HTMLTableSectionElement, HTMLOListElement, HTMLHRElement, HTMLProgressElement, HTMLDetailsElement, HTMLSlotElement, HTMLMetaElement, HTMLTitleElement, HTMLFieldSetElement, HTMLLegendElement, HTMLOutputElement, HTMLDataListElement, HTMLOptGroupElement, HTMLBaseElement, HTMLObjectElement, HTMLEmbedElement, HTMLPictureElement, HTMLSourceElement, HTMLTimeElement, HTMLQuoteElement, HTMLModElement, HTMLDListElement, HTMLMenuElement, HTMLTableCaptionElement, HTMLTableColElement, HTMLMeterElement, HTMLAreaElement, HTMLMapElement, HTMLTrackElement, HTMLDataElement, HTMLCollection: NodeListClass, NodeList: NodeListClass, NamedNodeMap: Array, Attr: Object, CharacterData: Text, ShadowRoot: DocumentFragment, DocumentType: Object, XMLDocument: Document,
    DOMRect, DOMRectReadOnly, DOMPoint, DOMPointReadOnly: DOMPoint, DOMMatrix, DOMMatrixReadOnly, DOMQuad: class { constructor(p1, p2, p3, p4) { this.p1 = p1; this.p2 = p2; this.p3 = p3; this.p4 = p4; } getBounds() { return new DOMRect(); } },
    MutationObserver, ResizeObserver, IntersectionObserver, PerformanceObserver, ReportingObserver, Range, StaticRange, Selection, FontFace, FontFaceSet, Storage, MediaQueryList, DataTransfer, DataTransferItem: Object, DataTransferItemList: Array, FileList: Array, Location, History,
    ImageData, Path2D, CanvasRenderingContext2D, CanvasGradient: Object, CanvasPattern: Object, OffscreenCanvas: class { constructor(w, h) { const c = doc.createElement('canvas'); c.width = w; c.height = h; return c; } }, Image: class extends HTMLImageElement { constructor(w, h) { super(doc, 'img'); if (w) this.setAttribute('width', w); if (h) this.setAttribute('height', h); } }, Option: class extends HTMLOptionElement { constructor(text, value, def, sel) { super(doc, 'option'); if (text) this.textContent = text; if (value !== undefined) this.setAttribute('value', value); if (def) this.setAttribute('selected', ''); if (sel) this._selected = true; } }, Audio: class extends HTMLAudioElement { constructor(src) { super(doc, 'audio'); if (src) this.setAttribute('src', src); } },
    XPathResult: Object, XPathEvaluator: Object, Screen: Object, Navigator: Object, Window: Object, MediaStream: Object, MediaSource: class { static isTypeSupported() { return false; } }, AudioContext: undefined, SpeechSynthesisUtterance: undefined, Notification: class { static get permission() { return 'denied'; } static requestPermission() { return Promise.resolve('denied'); } },
    ShadowRootInit: Object, ElementInternals: Object, Highlight: class { constructor() {} add() {} delete() {} clear() {} }, HighlightRegistry: Map, CSSStyleValue: Object, CSSUnitValue: class { constructor(v, u) { this.value = v; this.unit = u; } }, CSSKeywordValue: class { constructor(v) { this.value = v; } }, CSSNumericValue: Object,
    IdleDeadline: Object, ScrollTimeline: Object, Animation: Object, KeyframeEffect: Object, AnimationEffect: Object, DocumentTimeline: Object, GamepadEvent: E.Event, DeviceOrientationEvent: E.Event, TouchEvent: E.UIEvent, Touch: Object, TouchList: Array, SecurityPolicyViolationEvent: E.Event, SubmitEvent: E.Event, FormDataEvent: E.Event, ToggleEvent: E.Event, PageTransitionEvent: E.Event, ContentVisibilityAutoStateChangeEvent: E.Event, VisualViewport: Object, CSSImportRule: CSSRule, CSSSupportsRule: CSSRule, CSSFontFaceRule: CSSRule, CSSLayerBlockRule: CSSRule, CSSContainerRule: CSSRule, MediaList: Array, StyleSheet: CSSStyleSheet, StyleSheetList: Array, CSSRuleList: Array, SVGLength: Object, SVGMatrix: DOMMatrix, SVGPoint: DOMPoint, SVGRect: DOMRect, SVGAnimatedString: Object, Worklet: Object, PaintWorkletGlobalScope: Object, Clipboard: Object, ClipboardItem: class { constructor(items) { this.items = items; this.types = Object.keys(items); } getType(t) { return Promise.resolve(this.items[t]); } static supports() { return true; } }, PermissionStatus: Object, Permissions: Object, NavigationPreloadManager: Object, ServiceWorkerContainer: Object, ServiceWorker: Object, ServiceWorkerRegistration: Object, PushManager: Object, SharedWorker: class { constructor() { throw new DOMException('SharedWorker is not available', 'NotSupportedError'); } }, BroadcastChannel: g.BroadcastChannel, WebSocket: g.WebSocket, EventSource: class { constructor() { this.readyState = 2; } close() {} }, XMLSerializer: class { serializeToString(n) { return n.outerHTML || n.textContent || ''; } }, DOMParser: class { parseFromString(s, t) { const d = new Document(g, doc.URL); d._page = page; d._window = g; if (t && t.includes('xml')) { d.contentType = t; } const parsed = page.parseInto(d, String(s)); return parsed; } }, FormData: g.FormData, Headers: g.Headers, Request: g.Request, Response: g.Response, Blob: g.Blob, File: g.File, URL: g.URL, URLSearchParams: g.URLSearchParams, TextEncoder: g.TextEncoder, TextDecoder: g.TextDecoder, AbortController: g.AbortController, AbortSignal: g.AbortSignal, MessageChannel: g.MessageChannel, MessagePort: g.MessagePort, ReadableStream: g.ReadableStream, WritableStream: g.WritableStream, TransformStream: g.TransformStream, CompressionStream: g.CompressionStream, DecompressionStream: g.DecompressionStream, Performance: g.Performance, PerformanceEntry: g.PerformanceEntry, PerformanceMark: g.PerformanceMark, PerformanceMeasure: g.PerformanceMeasure, Crypto: g.Crypto, CryptoKey: g.CryptoKey, SubtleCrypto: g.SubtleCrypto, WebAssembly: g.WebAssembly, Intl: g.Intl, FileReader: class extends E.EventTarget { constructor() { super(); this.result = null; this.readyState = 0; this.error = null; this.onload = null; this.onerror = null; this.onloadend = null; } _done(r) { this.result = r; this.readyState = 2; const ev = new E.ProgressEvent('load'); ev.target = this; this.onload && this.onload(ev); this.dispatchEvent(ev); const e2 = new E.ProgressEvent('loadend'); e2.target = this; this.onloadend && this.onloadend(e2); this.dispatchEvent(e2); } readAsText(b) { b.text().then(t => this._done(t)); } readAsArrayBuffer(b) { b.arrayBuffer().then(a => this._done(a)); } readAsDataURL(b) { b.arrayBuffer().then(a => this._done(`data:${b.type || 'application/octet-stream'};base64,${Buffer.from(a).toString('base64')}`)); } abort() {} static EMPTY = 0; static LOADING = 1; static DONE = 2; },
    Worker: page.WorkerClass, Element2: undefined };
  for (const [k, v] of Object.entries(classes)) if (v !== undefined) def(k, v);
  def('window', g); def('self', g); def('top', g); def('parent', g); def('frames', g); def('document', doc); def('globalThis', g);
  def('location', page.location); def('history', page.history); def('navigator', page.navigator); def('screen', page.screen); def('localStorage', page.localStorage); def('sessionStorage', page.sessionStorage);
  def('customElements', page.customElements); def('visualViewport', page.visualViewport);
  def('getComputedStyle', (el, pseudo) => page.getComputedStyle(el, pseudo));
  def('matchMedia', q => page.matchMedia(q));
  def('requestAnimationFrame', cb => page.requestAnimationFrame(cb)); def('cancelAnimationFrame', id => page.cancelAnimationFrame(id));
  def('requestIdleCallback', (cb, o) => setTimeout(() => cb({ didTimeout: false, timeRemaining: () => 10 }), (o && o.timeout) ? Math.min(o.timeout, 50) : 16)); def('cancelIdleCallback', id => clearTimeout(id));
  def('getSelection', () => page.selection);
  def('alert', m => page.log(`alert: ${m}`)); def('confirm', () => true); def('prompt', () => null); def('print', () => {}); def('open', (url) => { page.openExternal(url); return null; }); def('close', () => page.closeWindow()); def('stop', () => {}); def('focus', () => page.focusWindow()); def('blur', () => {}); def('postMessage', (m, t, ports) => { const ev = new E.MessageEvent('message', { data: m, origin: page.location.origin, source: g, ports: ports || [] }); setTimeout(() => g.dispatchEvent(ev), 0); });
  def('scrollTo', () => {}); def('scroll', () => {}); def('scrollBy', () => {}); def('resizeTo', () => {}); def('moveTo', () => {}); def('getMatchedCSSRules', () => []);
  def('createImageBitmap', (img) => Promise.resolve(img)); def('reportError', e => E.reportError(e));
  def('queryLocalFonts', () => Promise.resolve([]));
  // the browser's performance: node's, without nodeTiming (pages pick
  // their node path on seeing it)
  const np = globalThis.performance;
  const perf = { timeOrigin: np.timeOrigin, now: () => np.now(), mark: (n, o) => np.mark(n, o), measure: (n, a, b) => np.measure(n, a, b), getEntries: () => np.getEntries(), getEntriesByName: (n, t) => np.getEntriesByName(n, t), getEntriesByType: t => np.getEntriesByType(t), clearMarks: n => np.clearMarks(n), clearMeasures: n => np.clearMeasures(n), clearResourceTimings() {}, setResourceTimingBufferSize() {}, toJSON: () => np.toJSON(), timing: { navigationStart: Math.round(np.timeOrigin), fetchStart: Math.round(np.timeOrigin), domainLookupStart: 0, responseEnd: 0, loadEventEnd: 0, domContentLoadedEventEnd: 0 }, navigation: { type: 0, redirectCount: 0 }, eventCounts: new Map(), memory: { usedJSHeapSize: 0, totalJSHeapSize: 0, jsHeapSizeLimit: 0 }, addEventListener() {}, removeEventListener() {}, dispatchEvent() { return true; }, onresourcetimingbufferfull: null };
  def('performance', perf);
  def('devicePixelRatio', page.scale);
  def('isSecureContext', true); def('crossOriginIsolated', false); def('origin', page.location.origin); def('name', ''); def('status', ''); def('closed', false); def('opener', null); def('frameElement', null); def('length', 0);
  def('CSS', { supports: (p, v) => true, escape: s => String(s).replace(/([^a-zA-Z0-9_-])/g, '\\$1'), px: v => new classes.CSSUnitValue(v, 'px'), number: v => new classes.CSSUnitValue(v, 'number'), registerProperty() {}, highlights: new Map(), paintWorklet: { addModule: () => Promise.resolve() } });
  def('styleMedia', { type: 'screen', matchMedium: q => page.matchMedia(q).matches });
  def('speechSynthesis', undefined); def('trustedTypes', undefined);
  def('onerror', null); def('onunhandledrejection', null); def('onresize', null); def('onload', null);
  def('innerWidth', page.width / page.scale); def('innerHeight', page.height / page.scale); def('outerWidth', page.width / page.scale); def('outerHeight', page.height / page.scale); def('screenX', 0); def('screenY', 0); def('screenLeft', 0); def('screenTop', 0); def('scrollX', 0); def('scrollY', 0); def('pageXOffset', 0); def('pageYOffset', 0);
  // the window is an EventTarget too
  const wt = new E.EventTarget();
  def('addEventListener', (...a) => wt.addEventListener(...a)); def('removeEventListener', (...a) => wt.removeEventListener(...a)); def('dispatchEvent', ev => { ev.target = ev.target || g; return wt.dispatchEvent(ev); });
  wt._invoke = function (ev, phase) { E.EventTarget.prototype._invoke.call(this, ev, phase); const h = g['on' + ev.type]; if (phase !== E.Event.CAPTURING_PHASE && typeof h === 'function' && h !== this['on' + ev.type]) { try { h.call(g, ev); } catch (e) { E.reportError(e); } } };
  page.windowTarget = wt;
  doc._eventParent = () => wt;
  return g;
}
