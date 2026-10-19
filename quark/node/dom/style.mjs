// the cascade: rules matched to an element become its computed style
import { parseStylesheet, matchesComplex, matchCompound, parseColor, parseLength, splitSpaces, splitCommas, expandShorthand, colorToCss, PENDING } from './css.mjs';

export const INHERITED = new Set(['color', 'font-family', 'font-size', 'font-style', 'font-weight', 'font-variant', 'font-stretch', 'font-feature-settings', 'font-variation-settings', 'line-height', 'letter-spacing', 'word-spacing', 'text-align', 'text-indent', 'text-transform', 'white-space', 'word-break', 'overflow-wrap', 'word-wrap', 'visibility', 'cursor', 'direction', 'list-style-type', 'list-style-position', 'list-style-image', 'quotes', 'tab-size', 'text-rendering', '-webkit-font-smoothing', 'pointer-events', 'user-select', '-webkit-user-select', 'caret-color', 'text-shadow', 'hyphens', 'text-decoration-color', 'font-kerning', 'color-scheme', 'forced-color-adjust', 'accent-color', 'text-underline-position', 'text-size-adjust', '-webkit-text-size-adjust', 'image-rendering', 'orphans', 'widows', 'scrollbar-width', 'scrollbar-color', 'text-wrap', 'line-break', 'font-synthesis', 'font-optical-sizing', 'text-emphasis', 'hanging-punctuation', 'text-justify']);
// what the UA gives when nothing is set
export const INITIAL = {
  display: 'inline', position: 'static', color: 'rgb(0, 0, 0)', 'background-color': 'transparent', 'background-image': 'none', 'background-repeat': 'repeat', 'background-position': '0% 0%', 'background-size': 'auto', 'background-clip': 'border-box', 'background-origin': 'padding-box',
  'font-family': 'system-ui', 'font-size': '16px', 'font-weight': '400', 'font-style': 'normal', 'font-variant': 'normal', 'font-stretch': 'normal', 'line-height': 'normal', 'letter-spacing': 'normal', 'word-spacing': 'normal',
  'text-align': 'start', 'text-decoration-line': 'none', 'text-decoration-style': 'solid', 'text-decoration-color': 'currentcolor', 'text-decoration-thickness': 'auto', 'text-transform': 'none', 'white-space': 'normal', 'word-break': 'normal', 'overflow-wrap': 'normal', 'text-overflow': 'clip', 'text-indent': '0px', 'vertical-align': 'baseline', 'text-shadow': 'none', 'tab-size': '8',
  width: 'auto', height: 'auto', 'min-width': 'auto', 'min-height': 'auto', 'max-width': 'none', 'max-height': 'none', 'box-sizing': 'content-box',
  'margin-top': '0px', 'margin-right': '0px', 'margin-bottom': '0px', 'margin-left': '0px', 'padding-top': '0px', 'padding-right': '0px', 'padding-bottom': '0px', 'padding-left': '0px',
  'border-top-width': '0px', 'border-right-width': '0px', 'border-bottom-width': '0px', 'border-left-width': '0px', 'border-top-style': 'none', 'border-right-style': 'none', 'border-bottom-style': 'none', 'border-left-style': 'none',
  'border-top-color': 'currentcolor', 'border-right-color': 'currentcolor', 'border-bottom-color': 'currentcolor', 'border-left-color': 'currentcolor',
  'border-top-left-radius': '0px', 'border-top-right-radius': '0px', 'border-bottom-right-radius': '0px', 'border-bottom-left-radius': '0px',
  top: 'auto', right: 'auto', bottom: 'auto', left: 'auto', 'z-index': 'auto', float: 'none', clear: 'none',
  'overflow-x': 'visible', 'overflow-y': 'visible', visibility: 'visible', opacity: '1', cursor: 'auto', 'pointer-events': 'auto', 'user-select': 'auto',
  'flex-direction': 'row', 'flex-wrap': 'nowrap', 'justify-content': 'normal', 'align-items': 'normal', 'align-content': 'normal', 'align-self': 'auto', 'justify-self': 'auto', 'justify-items': 'legacy', 'flex-grow': '0', 'flex-shrink': '1', 'flex-basis': 'auto', order: '0', 'row-gap': 'normal', 'column-gap': 'normal',
  'grid-template-columns': 'none', 'grid-template-rows': 'none', 'grid-auto-flow': 'row', 'grid-auto-rows': 'auto', 'grid-auto-columns': 'auto', 'grid-row-start': 'auto', 'grid-row-end': 'auto', 'grid-column-start': 'auto', 'grid-column-end': 'auto', 'grid-template-areas': 'none',
  transform: 'none', 'transform-origin': '50% 50%', transition: 'none', 'transition-property': 'all', 'transition-duration': '0s', 'transition-timing-function': 'ease', 'transition-delay': '0s', 'animation-name': 'none', 'animation-duration': '0s', 'animation-iteration-count': '1', 'animation-timing-function': 'ease', 'animation-delay': '0s', 'animation-direction': 'normal', 'animation-fill-mode': 'none', 'animation-play-state': 'running',
  'box-shadow': 'none', outline: 'none', 'outline-width': '0px', 'outline-style': 'none', 'outline-color': 'currentcolor', 'outline-offset': '0px', filter: 'none', 'backdrop-filter': 'none', 'mix-blend-mode': 'normal', 'clip-path': 'none', 'mask-image': 'none', '-webkit-mask-image': 'none', '-webkit-mask-size': 'auto', '-webkit-mask-position': '0% 0%', '-webkit-mask-repeat': 'repeat',
  'list-style-type': 'disc', 'list-style-position': 'outside', content: 'normal', resize: 'none', direction: 'ltr', 'caret-color': 'auto', 'appearance': 'none', 'object-fit': 'fill', 'object-position': '50% 50%', 'aspect-ratio': 'auto', 'will-change': 'auto', 'contain': 'none', 'content-visibility': 'visible', 'scroll-behavior': 'auto', 'overscroll-behavior': 'auto', 'touch-action': 'auto', 'isolation': 'auto', 'table-layout': 'auto', 'border-collapse': 'separate', 'border-spacing': '0px', 'empty-cells': 'show', 'caption-side': 'top', 'writing-mode': 'horizontal-tb', 'text-wrap': 'wrap', 'scrollbar-width': 'auto', 'scrollbar-gutter': 'auto', 'inset-inline-start': 'auto', 'color-scheme': 'normal', 'font-feature-settings': 'normal', 'font-variation-settings': 'normal', 'font-kerning': 'auto', 'image-rendering': 'auto', '-webkit-font-smoothing': 'auto', 'text-rendering': 'auto', 'word-wrap': 'normal', 'hyphens': 'manual', 'quotes': 'auto', 'accent-color': 'auto', 'forced-color-adjust': 'auto', 'text-underline-position': 'auto', 'line-break': 'auto', 'font-synthesis': 'weight style', 'text-size-adjust': 'auto', 'orphans': '2', 'widows': '2', 'scrollbar-color': 'auto', 'speak': 'normal', 'text-emphasis': 'none', 'hanging-punctuation': 'none', 'text-justify': 'auto', 'font-optical-sizing': 'auto', 'break-inside': 'auto', 'page-break-inside': 'auto', 'columns': 'auto', 'column-count': 'auto', 'column-width': 'auto', 'transform-style': 'flat', 'perspective': 'none', 'backface-visibility': 'visible', 'translate': 'none', 'rotate': 'none', 'scale': 'none', 'rotate': 'none', 'offset-path': 'none', 'inset-block-start': 'auto', 'unicode-bidi': 'normal', 'zoom': '1',
};
// the UA sheet: what browsers give html elements before any author css
export const UA_SHEET = `
html, body, div, p, h1, h2, h3, h4, h5, h6, ul, ol, li, dl, dt, dd, form, fieldset, legend, header, footer, nav, section, article, aside, main, figure, figcaption, address, blockquote, pre, hr, table, caption, details, summary, dialog, menu, center, hgroup, search, template, noscript { display: block; }
head, script, style, title, meta, link, base, template, noscript, area, param, [hidden] { display: none !important; }
body { margin: 8px; }
h1 { font-size: 2em; margin: 0.67em 0; font-weight: bold; } h2 { font-size: 1.5em; margin: 0.83em 0; font-weight: bold; } h3 { font-size: 1.17em; margin: 1em 0; font-weight: bold; } h4 { margin: 1.33em 0; font-weight: bold; } h5 { font-size: 0.83em; margin: 1.67em 0; font-weight: bold; } h6 { font-size: 0.67em; margin: 2.33em 0; font-weight: bold; }
p, blockquote, figure, dl { margin: 1em 0; } blockquote, figure { margin-left: 40px; margin-right: 40px; } dd { margin-left: 40px; }
ul, ol { margin: 1em 0; padding-left: 40px; } ul { list-style-type: disc; } ol { list-style-type: decimal; } li { display: list-item; } ul ul, ol ul { list-style-type: circle; }
pre, code, kbd, samp, tt, xmp, plaintext, listing { font-family: monospace; } pre, xmp, plaintext, listing { white-space: pre; margin: 1em 0; }
b, strong { font-weight: bolder; } i, em, cite, var, dfn, address { font-style: italic; } u, ins { text-decoration: underline; } s, strike, del { text-decoration: line-through; }
small { font-size: smaller; } big { font-size: larger; } sub { vertical-align: sub; font-size: smaller; } sup { vertical-align: super; font-size: smaller; }
a:any-link { color: -webkit-link; text-decoration: underline; cursor: pointer; } mark { background-color: yellow; color: black; }
hr { border: 1px inset; margin: 0.5em auto; color: gray; } abbr[title] { text-decoration: underline dotted; }
table { display: table; border-spacing: 2px; border-collapse: separate; box-sizing: border-box; text-indent: 0; } thead { display: table-header-group; } tbody { display: table-row-group; } tfoot { display: table-footer-group; } tr { display: table-row; } td, th { display: table-cell; padding: 1px; } th { font-weight: bold; text-align: center; } caption { display: table-caption; text-align: center; } col { display: table-column; } colgroup { display: table-column-group; }
input, textarea, select, button { font: 13.3333px system-ui; color: fieldtext; letter-spacing: normal; word-spacing: normal; text-transform: none; text-indent: 0; text-shadow: none; display: inline-block; text-align: start; margin: 0; box-sizing: border-box; }
input, textarea, select { background-color: field; border: 2px inset; padding: 1px 2px; } textarea { padding: 2px; white-space: pre-wrap; resize: both; overflow: auto; } input { padding: 1px 2px; } input[type=checkbox], input[type=radio] { width: 13px; height: 13px; margin: 3px 3px 3px 4px; padding: 0; border: 0; }
button, input[type=button], input[type=submit], input[type=reset] { background-color: buttonface; border: 2px outset buttonborder; padding: 1px 6px; text-align: center; cursor: default; color: buttontext; }
button, input, select, textarea { line-height: normal; }
img, video, canvas, svg, iframe, embed, object, audio { display: inline-block; } img { overflow: clip; } iframe { border: 2px inset; } video { object-fit: contain; }
br { display: inline; } wbr { display: inline; } rt { font-size: 50%; }
:focus-visible { outline: auto 1px -webkit-focus-ring-color; }
details > summary:first-of-type { display: list-item; list-style: disclosure-closed inside; } details[open] > summary:first-of-type { list-style-type: disclosure-open; }
dialog { position: absolute; left: 0; right: 0; width: fit-content; height: fit-content; margin: auto; border: solid; padding: 1em; background: white; color: black; } dialog:not([open]) { display: none; }
label { cursor: default; } legend { padding: 0 2px; } fieldset { margin: 0 2px; padding: 0.35em 0.75em 0.625em; border: 2px groove; min-width: 0; }
option { display: block; padding: 0 2px 1px; } optgroup { font-weight: bolder; } select { overflow: hidden; } progress, meter { display: inline-block; width: 10em; height: 1em; }
`;
export { parseStylesheet };

const SYSTEM_COLORS = { field: '#ffffff', fieldtext: '#000000', buttonface: '#efefef', buttonborder: '#767676', buttontext: '#000000', canvas: '#ffffff', canvastext: '#000000', highlight: '#0075ff', highlighttext: '#ffffff', graytext: '#6d6d6d', linktext: '#0000ee', '-webkit-link': '#0000ee', '-webkit-focus-ring-color': '#005fcc', mark: '#ffff00', marktext: '#000000', accentcolor: '#0075ff', accentcolortext: '#ffffff', activetext: '#ff0000', visitedtext: '#551a8b', selecteditem: '#0075ff', selecteditemtext: '#ffffff', window: '#ffffff', windowtext: '#000000', menu: '#f0f0f0', menutext: '#000000', scrollbar: '#f0f0f0', threedface: '#f0f0f0', activeborder: '#b4b4b4', inactiveborder: '#f4f7fc', buttonhighlight: '#ffffff', buttonshadow: '#a0a0a0', '-webkit-activelink': '#ff0000' };

// an engine keeps its sheets and resolves each element once per pass
export class StyleEngine {
  constructor(env) {
    this.env = env; // { vw, vh, dpr, rem }
    this.sheets = [];     // { sheet, order, owner }
    this.ua = parseStylesheet(UA_SHEET);
    this.keyframes = new Map();
    this.fontFaces = [];
    this.generation = 0;
    this.index = null;
  }
  setSheets(list) {
    this.sheets = list;
    this.keyframes = new Map(this.ua.keyframes);
    this.fontFaces = [];
    for (const s of list) { for (const [k, v] of s.keyframes) this.keyframes.set(k, v); this.fontFaces.push(...s.fontFaces); }
    this.index = null;
    this.generation++;
  }
  get mutGen() { return this._mutGen || 0; }
  mutated() { this._mutGen = (this._mutGen || 0) + 1; }
  // a sheet's rules keyed by their rightmost compound, made once per
  // parsed sheet: the index merges them
  sheetIndex(sheet, origin) {
    if (sheet._index) return sheet._index;
    const si = { entries: [], has: { id: new Set(), cls: new Set(), tag: new Set(), any: false },
      anc: { id: new Set(), cls: new Set(), tag: new Set(), attr: new Set(), pseudo: new Set(), any: false },
      keys: { id: new Set(), cls: new Set(), tag: new Set(), any: false } };
    // a compound with :has makes its element depend on its subtree
    const noteHas = c => { if (!c.pseudos.some(p => p.name === 'has')) return; if (c.id) si.has.id.add(c.id); else if (c.classes.length) si.has.cls.add(c.classes[0]); else if (c.tag) si.has.tag.add(c.tag); else si.has.any = true; };
    // what a compound left of the last one reads: a change of it on
    // an element can change the styles under and after that element
    const noteAnc = c => {
      if (c.id) si.anc.id.add(c.id);
      for (const x of c.classes) si.anc.cls.add(x);
      if (c.tag) si.anc.tag.add(c.tag);
      for (const a of c.attrs) si.anc.attr.add(a.name);
      for (const p of c.pseudos) { if (p.list) { for (const l of p.list) for (const q of l.parts) noteAnc(q.compound); } else si.anc.pseudo.add(p.name); }
      if (!c.id && !c.classes.length && !c.tag && !c.attrs.length && !c.pseudos.length) si.anc.any = true;
    };
    for (const rule of sheet.rules) for (const sel of rule.selectors) {
      for (const p of sel.parts) noteHas(p.compound);
      for (let i = 0; i < sel.parts.length - 1; i++) noteAnc(sel.parts[i].compound);
      const last = sel.parts[sel.parts.length - 1].compound;
      const entry = { sel, rule, origin, kind: 'any', key: null };
      if (last.id) { entry.kind = 'id'; entry.key = last.id; si.keys.id.add(last.id); }
      else if (last.classes.length) { entry.kind = 'cls'; entry.key = last.classes[0]; si.keys.cls.add(last.classes[0]); }
      else if (last.tag) { entry.kind = 'tag'; entry.key = last.tag; si.keys.tag.add(last.tag); }
      else si.keys.any = true;
      si.entries.push(entry);
    }
    sheet._index = si;
    return si;
  }
  buildIndex() {
    const idx = { id: new Map(), cls: new Map(), tag: new Map(), any: [],
      has: { id: new Set(), cls: new Set(), tag: new Set(), any: false },
      anc: { id: new Set(), cls: new Set(), tag: new Set(), attr: new Set(), pseudo: new Set(), any: false } };
    const union = (a, b) => { for (const k of ['id', 'cls', 'tag', 'attr', 'pseudo']) if (b[k]) for (const v of b[k]) a[k].add(v); if (b.any) a.any = true; };
    const merge = si => {
      union(idx.has, si.has); union(idx.anc, si.anc);
      for (const e of si.entries) { if (e.kind === 'any') idx.any.push(e); else push(idx[e.kind], e.key, e); }
    };
    merge(this.sheetIndex(this.ua, 0));
    for (const s of this.sheets) merge(this.sheetIndex(s, 1));
    this.index = idx;
    this.candCache = new Map();
  }
  // the elements a set of sheets can style: their rightmost keys
  sheetKeys(sheets) {
    const out = { id: new Set(), cls: new Set(), tag: new Set(), any: false };
    for (const s of sheets) { const k = this.sheetIndex(s, 1).keys; for (const v of k.id) out.id.add(v); for (const v of k.cls) out.cls.add(v); for (const v of k.tag) out.tag.add(v); if (k.any) out.any = true; }
    return out;
  }
  keysTouch(el, keys) {
    if (keys.any || keys.tag.has(el.localName)) return true;
    const id = el._attrs.get('id'); if (id && keys.id.has(id)) return true;
    const cls = el._classes(); if (cls) for (const c of cls) if (keys.cls.has(c)) return true;
    return false;
  }
  // the rules that could match el; elements with the same tag, id
  // and classes share one list
  candidates(el) {
    if (!this.index) this.buildIndex();
    const idx = this.index;
    const id = el._attrs.get('id'), cls = el._classes();
    let key = el.localName; if (id) key += '#' + id; if (cls) for (const c of cls) key += '.' + c;
    let out = this.candCache.get(key);
    if (out) return out;
    out = [];
    if (id) { const l = idx.id.get(id); if (l) out.push(...l); }
    if (cls) for (const c of cls) { const l = idx.cls.get(c); if (l) out.push(...l); }
    const t = idx.tag.get(el.localName); if (t) out.push(...t);
    out.push(...idx.any);
    if (this.candCache.size < 65536) this.candCache.set(key, out);
    return out;
  }
  // could a rule that can match el read one of these keys (.class,
  // #id, [attr], :pseudo) on an element above or before it?
  rulesNameAbove(el, keys) {
    const cands = this.candidates(el);
    let set = cands.aboveKeys;
    if (!set) {
      set = cands.aboveKeys = new Set();
      const note = c => { if (c.id) set.add('#' + c.id); for (const x of c.classes) set.add('.' + x); for (const a of c.attrs) set.add('[' + a.name + ']'); for (const p of c.pseudos) { set.add(':' + p.name); if (p.list) for (const l of p.list) for (const q of l.parts) note(q.compound); } };
      for (const e of cands) { const parts = e.sel.parts; for (let i = 0; i < parts.length - 1; i++) note(parts[i].compound); const last = parts[parts.length - 1].compound; for (const p of last.pseudos) if (p.list) for (const l of p.list) for (let i = 0; i < l.parts.length - 1; i++) note(l.parts[i].compound); }
    }
    for (const k of keys) if (set.has(k)) return true;
    return false;
  }
  // the ancestors' keys; siblings share their parent's set while no
  // mutation has happened (page.mutGen)
  ancKeysOf(el) {
    const p = el.parentElement || (el.parentNode && el.parentNode.host) || null;
    if (!p) return EMPTY_KEYS;
    if (p._kidsAncGen === this.mutGen && p._kidsAnc) return p._kidsAnc;
    const set = new Set(this.ancKeysOf(p));
    set.add(p.localName);
    const id = p._attrs.get('id'); if (id) set.add('#' + id);
    const cls = p._classes(); if (cls) for (const c of cls) set.add('.' + c);
    p._kidsAnc = set; p._kidsAncGen = this.mutGen;
    return set;
  }
  // does a change of these keys on an element reach the styles under
  // or after it? keys: .class, #id, [attr], :pseudo; null is unknown
  reachesBelow(keys) {
    if (!this.index) this.buildIndex();
    const a = this.index.anc;
    // no key changed (an inline style): nothing below re-matches
    if (keys && !keys.length) return false;
    if (a.any || !keys) return true;
    for (const k of keys) {
      if (k[0] === '.') { if (a.cls.has(k.slice(1))) return true; }
      else if (k[0] === '#') { if (a.id.has(k.slice(1))) return true; }
      else if (k[0] === '[') { if (a.attr.has(k.slice(1, -1))) return true; }
      else if (k[0] === ':') { if (a.pseudo.has(k.slice(1))) return true; }
      else return true;
    }
    return false;
  }
  // could a :has rule on el read its descendants?
  hasSensitive(el) {
    if (!this.index) this.buildIndex();
    const h = this.index.has;
    if (h.any) return true;
    const id = el.getAttribute('id'); if (id && h.id.has(id)) return true;
    const cls = el._classes(); if (cls) for (const c of cls) if (h.cls.has(c)) return true;
    return h.tag.has(el.localName);
  }
  // declared values for el (and for its ::before/::after, by pseudoEl)
  declared(el, pseudoEl) {
    const hits = [];
    const cands = this.candidates(el);
    this.lastCands = cands.length;
    const anc = this.ancKeysOf(el);
    for (const c of cands) {
      if ((c.sel.pseudoEl || null) !== (pseudoEl || null)) continue;
      if (matchesComplex(el, c.sel, null, anc)) hits.push(c);
    }
    this.lastHits = hits.length;
    hits.sort((a, b) => a.origin - b.origin || a.sel.spec - b.sel.spec || a.rule.order - b.rule.order);
    const out = new Map(), important = new Map();
    for (const h of hits) for (const [k, v, imp] of h.rule.decls) { if (imp) important.set(k, v); else out.set(k, v); }
    if (!pseudoEl) {
      const inline = el._style || (el.getAttribute('style') ? el.style : null);
      if (inline) for (const k of inline._props.keys()) { const v = inline._props.get(k); if (inline._priority.get(k)) important.set(k, v); else out.set(k, v); }
      // presentational attributes
      presentational(el, out);
    }
    for (const [k, v] of important) out.set(k, v);
    return out;
  }
  // computed style: Map of longhand -> computed string, plus numbers
  compute(el, parent, pseudoEl, parentForPseudo) {
    const decl = this.declared(el, pseudoEl);
    const p = pseudoEl ? parentForPseudo : parent;
    const cs = new ComputedStyle(this, decl, p, el);
    return cs;
  }
}
const EMPTY_KEYS = new Set();
function push(m, k, v) { let l = m.get(k); if (!l) { l = []; m.set(k, l); } l.push(v); }
function presentational(el, out) {
  const set = (k, v) => { if (!out.has(k)) out.set(k, v); };
  const w = el.getAttribute('width'), h = el.getAttribute('height');
  if (el.localName === 'img' || el.localName === 'canvas' || el.localName === 'video' || el.localName === 'iframe' || el.localName === 'svg' || el.localName === 'table' || el.localName === 'td' || el.localName === 'th') {
    if (w !== null && w !== '') set('width', /^\d+$/.test(w) ? w + 'px' : w);
    if (h !== null && h !== '') set('height', /^\d+$/.test(h) ? h + 'px' : h);
  }
  const align = el.getAttribute('align'); if (align) set('text-align', align);
  const bg = el.getAttribute('bgcolor'); if (bg) set('background-color', bg);
  if (el.localName === 'font') { const c = el.getAttribute('color'); if (c) set('color', c); }
  if (el.localName === 'textarea') { const r = el.getAttribute('rows'), c = el.getAttribute('cols'); if (r) set('height', `${parseInt(r) * 1.2}em`); if (c) set('width', `${parseInt(c)}ch`); }
  if (el.localName === 'input') { const s = el.getAttribute('size'); if (s && !out.has('width')) set('width', `${parseInt(s) + 2}ch`); }
}

const KEYWORD_SIZES = { 'xx-small': 9, 'x-small': 10, small: 13, medium: 16, large: 18, 'x-large': 24, 'xx-large': 32, 'xxx-large': 48 };
const WEIGHTS = { normal: 400, bold: 700 };
export class ComputedStyle {
  constructor(engine, decl, parent, el) {
    this.engine = engine; this.decl = decl; this.parent = parent; this.el = el;
    this.values = new Map(); this.vars = parent ? parent.vars : new Map();
    this.n = {};
    // custom properties cascade and inherit
    let own = null;
    for (const [k, v] of decl) if (k.startsWith('--')) { if (!own) own = new Map(parent ? parent.vars : []); own.set(k, v); }
    if (own) this.vars = own;
    this.resolveFont();
  }
  rem() { return this.engine.env.rem || 16; }
  ctx() { return { em: this.n.fontSize, rem: this.rem(), vw: this.engine.env.vw, vh: this.engine.env.vh, lh: this.n.lineHeight, ch: this.n.fontSize * 0.55 }; }
  raw(k) {
    let v = this.decl.get(k);
    if (v !== undefined && v[0] === PENDING) v = this.pendingLonghand(k, v);
    else if (v !== undefined && v.includes('var(')) { v = this.substituteVars(v); if (v === null) v = undefined; }
    if (v === 'inherit' || (v === undefined && INHERITED.has(k)) || v === 'unset' && INHERITED.has(k) || v === 'revert' && INHERITED.has(k)) return this.parent ? this.parent.get(k) : INITIAL[k];
    if (v === undefined || v === 'initial' || v === 'unset' || v === 'revert' || v === 'revert-layer') return INITIAL[k] !== undefined ? INITIAL[k] : '';
    return v;
  }
  // a longhand from a shorthand holding var(): substitute, then split
  pendingLonghand(k, v) {
    const j = v.indexOf(PENDING, 1);
    const sub = this.substituteVars(v.slice(j + 1));
    if (sub === null) return undefined;
    const longs = expandShorthand(v.slice(1, j), sub.trim());
    const hit = longs && longs.find(l => l[0] === k);
    return hit && hit[1] !== '' ? hit[1] : undefined;
  }
  substituteVars(v) {
    let guard = 0;
    while (v.includes('var(') && guard++ < 20) {
      const i = v.indexOf('var(');
      let d = 0, j = i + 3;
      for (; j < v.length; j++) { if (v[j] === '(') d++; else if (v[j] === ')') { d--; if (d === 0) break; } }
      const inner = v.slice(i + 4, j);
      const comma = inner.indexOf(',');
      const name = (comma < 0 ? inner : inner.slice(0, comma)).trim();
      const fallback = comma < 0 ? '' : inner.slice(comma + 1).trim();
      let val = this.vars.get(name);
      if (val === undefined || val === '') val = fallback;
      // no value and no fallback: the declaration is invalid
      if (val === '') return null;
      v = v.slice(0, i) + val + v.slice(j + 1);
    }
    return v;
  }
  resolveFont() {
    const parentSize = this.parent ? this.parent.n.fontSize : this.rem();
    let fs = this.raw('font-size');
    let size;
    if (KEYWORD_SIZES[fs] !== undefined) size = KEYWORD_SIZES[fs];
    else if (fs === 'smaller') size = parentSize / 1.2;
    else if (fs === 'larger') size = parentSize * 1.2;
    else { size = parseLength(fs, { em: parentSize, rem: this.rem(), vw: this.engine.env.vw, vh: this.engine.env.vh }, parentSize); if (size == null) size = parentSize; }
    this.n.fontSize = size;
    const fw = this.raw('font-weight');
    const pw = this.parent ? this.parent.n.fontWeight : 400;
    this.n.fontWeight = fw === 'bolder' ? Math.min(900, pw + 300) : fw === 'lighter' ? Math.max(100, pw - 300) : (WEIGHTS[fw] ?? (parseInt(fw) || 400));
    const lh = this.raw('line-height');
    if (lh === 'normal') this.n.lineHeight = Math.round(size * 1.2);
    else if (/^[\d.]+$/.test(lh)) this.n.lineHeight = parseFloat(lh) * size;
    else { const l = parseLength(lh, { em: size, rem: this.rem(), vw: this.engine.env.vw, vh: this.engine.env.vh }, size); this.n.lineHeight = l == null ? size * 1.2 : l; }
    this.n.lineHeightRaw = lh;
  }
  // the computed value as text
  get(k) {
    if (this.values.has(k)) return this.values.get(k);
    let v = this.computeOne(k);
    this.values.set(k, v);
    return v;
  }
  computeOne(k) {
    const v = this.raw(k);
    switch (k) {
    case 'font-size': return this.n.fontSize + 'px';
    case 'font-weight': return String(this.n.fontWeight);
    case 'line-height': return this.n.lineHeightRaw === 'normal' ? 'normal' : this.n.lineHeight + 'px';
    case 'color': case 'background-color': case 'border-top-color': case 'border-right-color': case 'border-bottom-color': case 'border-left-color': case 'outline-color': case 'text-decoration-color': case 'caret-color': {
      const cur = k === 'color' ? (this.parent ? this.parent.color() : [0, 0, 0, 1]) : this.color();
      const c = this.parseColorValue(v, cur);
      if (!c) return k === 'color' ? colorToCss(cur) : (k === 'caret-color' ? 'auto' : 'rgba(0, 0, 0, 0)');
      return colorToCss(c);
    }
    case 'display': {
      // blockification: positioned, floated or flex items
      let d = v;
      if (d === 'inline' && this.el && this.el.localName === 'li' && false) d = 'list-item';
      const pos = this.raw('position');
      if ((pos === 'absolute' || pos === 'fixed' || this.raw('float') !== 'none') && (d === 'inline' || d === 'inline-block' || d === 'inline-flex' || d === 'inline-grid' || d === 'inline-table')) d = d === 'inline-flex' ? 'flex' : d === 'inline-grid' ? 'grid' : 'block';
      if (d === 'inline-flex' && false) d = 'flex';
      return d;
    }
    case 'margin-top': case 'margin-right': case 'margin-bottom': case 'margin-left': case 'top': case 'right': case 'bottom': case 'left': case 'width': case 'height': case 'min-width': case 'min-height': case 'max-width': case 'max-height': case 'flex-basis':
      return v === 'auto' || v === 'none' || v.endsWith('%') || v.startsWith('calc') || v.startsWith('min(') || v.startsWith('max(') || v.startsWith('clamp(') || v.startsWith('fit-content') || v === 'max-content' || v === 'min-content' || v === 'content' ? v : this.px(v, k);
    case 'padding-top': case 'padding-right': case 'padding-bottom': case 'padding-left': case 'row-gap': case 'column-gap': case 'text-indent': case 'outline-offset':
      return v.endsWith('%') || v === 'normal' ? v : this.px(v, k);
    case 'border-top-width': case 'border-right-width': case 'border-bottom-width': case 'border-left-width': case 'outline-width': {
      const side = k.replace('-width', '-style');
      const style = k.startsWith('outline') ? this.raw('outline-style') : this.raw(side);
      if (style === 'none' || style === 'hidden') return '0px';
      if (v === 'thin') return '1px'; if (v === 'medium') return '3px'; if (v === 'thick') return '5px';
      return this.px(v, k);
    }
    case 'letter-spacing': case 'word-spacing': return v === 'normal' ? v : this.px(v, k);
    case 'border-top-left-radius': case 'border-top-right-radius': case 'border-bottom-right-radius': case 'border-bottom-left-radius': return v;
    case 'opacity': return String(Math.max(0, Math.min(1, parseFloat(v.endsWith('%') ? parseFloat(v) / 100 : v) || 0)));
    case 'z-index': return v === 'auto' ? v : String(parseInt(v) || 0);
    case 'white-space': { if (v === 'pre' || v === 'nowrap' || v === 'pre-wrap' || v === 'pre-line' || v === 'break-spaces') return v; return 'normal'; }
    case 'overflow-x': case 'overflow-y': {
      // one visible, one not: visible becomes auto
      const o = k === 'overflow-x' ? this.raw('overflow-y') : this.raw('overflow-x');
      if (v === 'visible' && o !== 'visible' && o !== 'clip') return 'auto';
      return v === 'overlay' ? 'auto' : v;
    }
    case 'font-family': return v;
    }
    return v;
  }
  px(v, k) { const n = parseLength(v, this.ctx(), null); return (n == null ? 0 : n) + 'px'; }
  parseColorValue(v, current) {
    const lower = (v || '').toLowerCase().trim();
    if (SYSTEM_COLORS[lower]) return parseColor(SYSTEM_COLORS[lower]);
    if (lower === 'currentcolor') return current;
    return parseColor(v, current);
  }
  // numbers for layout and paint
  color() { if (this._color) return this._color; const c = this.parseColorValue(this.raw('color'), this.parent ? this.parent.color() : [0, 0, 0, 1]); return this._color = c || (this.parent ? this.parent.color() : [0, 0, 0, 1]); }
  colorOf(k) { const c = this.parseColorValue(this.raw(k), this.color()); return c || [0, 0, 0, 0]; }
  length(k, pctBase) {
    const v = this.get(k);
    if (v === 'auto' || v === 'none' || v === 'normal' || v === '') return null;
    // parsed once per value; a percent once per base
    const key = v.includes('%') ? k + '|' + pctBase : k;
    const lens = this._lens || (this._lens = new Map());
    let n = lens.get(key);
    if (n === undefined) { n = parseLength(v, this.ctx(), pctBase); lens.set(key, n); }
    return n;
  }
  // a length that may be a percent of the base; null for auto
  len(k, pctBase) { return this.length(k, pctBase); }
  num(k, d = 0) { const v = parseFloat(this.get(k)); return isNaN(v) ? d : v; }
  is(k, ...vals) { return vals.includes(this.get(k)); }
  get fontSize() { return this.n.fontSize; }
  get lineHeight() { return this.n.lineHeight; }
  get fontWeight() { return this.n.fontWeight; }
  get fontFamilies() { if (this._fams) return this._fams; return this._fams = splitCommas(this.get('font-family')).map(f => f.replace(/^["']|["']$/g, '').trim()).filter(Boolean); }
  get italic() { const s = this.get('font-style'); return s === 'italic' || s === 'oblique'; }
  radii(w, h) {
    if (this._radii && this._radiiW === w && this._radiiH === h) return this._radii;
    const r = ['border-top-left-radius', 'border-top-right-radius', 'border-bottom-right-radius', 'border-bottom-left-radius'].map(k => {
      const v = this.get(k); const parts = splitSpaces(v);
      const rx = parseLength(parts[0] || '0', this.ctx(), w) || 0, ry = parseLength(parts[1] || parts[0] || '0', this.ctx(), h) || 0;
      return Math.max(0, Math.min(rx, ry));
    });
    // radii that overlap scale down together
    const f = Math.min(1, w / Math.max(1e-6, r[0] + r[1]), w / Math.max(1e-6, r[2] + r[3]), h / Math.max(1e-6, r[0] + r[3]), h / Math.max(1e-6, r[1] + r[2]));
    this._radii = r.map(x => x * f); this._radiiW = w; this._radiiH = h;
    return this._radii;
  }
  getPropertyValue(k) { return this.get(k); }
}
