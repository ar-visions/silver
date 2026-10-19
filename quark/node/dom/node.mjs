// the DOM tree: nodes, elements, attributes, inline style, documents
import { EventTarget, Event, reportError } from './events.mjs';

// attribute names are matched case-insensitively; the lowering is cached
const lowerNames = new Map();
function lowerName(k) { if (typeof k !== 'string') k = String(k); let l = lowerNames.get(k); if (l === undefined) { l = k.toLowerCase(); if (lowerNames.size < 4096) lowerNames.set(k, l); } return l; }

export const ELEMENT_NODE = 1, TEXT_NODE = 3, COMMENT_NODE = 8, DOCUMENT_NODE = 9, DOCUMENT_TYPE_NODE = 10, DOCUMENT_FRAGMENT_NODE = 11;
const VOID = new Set(['area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'param', 'source', 'track', 'wbr']);
const SVG_NS = 'http://www.w3.org/2000/svg', HTML_NS = 'http://www.w3.org/1999/xhtml';

// childNodes and children: arrays with the collection methods
export class NodeList extends Array {
  item(i) { return this[i] === undefined ? null : this[i]; }
  namedItem(id) { return this.find(e => e.id === id || e.getAttribute && e.getAttribute('name') === id) || null; }
  static get [Symbol.species]() { return Array; }
}
export function collection(list) { const l = NodeList.from(list); return l; }
export class Node extends EventTarget {
  constructor(doc) {
    super();
    this._ownerDocument = doc; this._parentNode = null;
    this._childNodes = new NodeList(); this._nodeType = 0; this._nodeName = '';
  }
  // accessors on the prototype: DOMPurify looks them up there
  get ownerDocument() { return this._ownerDocument; } set ownerDocument(v) { this._ownerDocument = v; }
  get parentNode() { return this._parentNode; } set parentNode(v) { this._parentNode = v; }
  get childNodes() { return this._childNodes; } set childNodes(v) { this._childNodes = v; }
  get nodeType() { return this._nodeType; } set nodeType(v) { this._nodeType = v; }
  get nodeName() { return this._nodeName; } set nodeName(v) { this._nodeName = v; }
  _eventParent() { return this.parentNode || (this.nodeType === DOCUMENT_NODE ? this._window : (this.ownerDocument && this.ownerDocument._window && false)); }
  get parentElement() { const p = this.parentNode; return p && p.nodeType === ELEMENT_NODE ? p : null; }
  get firstChild() { return this.childNodes[0] || null; }
  get lastChild() { return this.childNodes[this.childNodes.length - 1] || null; }
  get nextSibling() { const p = this.parentNode; if (!p) return null; const i = p.childNodes.indexOf(this); return p.childNodes[i + 1] || null; }
  get previousSibling() { const p = this.parentNode; if (!p) return null; const i = p.childNodes.indexOf(this); return i > 0 ? p.childNodes[i - 1] : null; }
  get isConnected() { let n = this; while (n.parentNode) n = n.parentNode; return n.nodeType === DOCUMENT_NODE; }
  get baseURI() { return this.ownerDocument ? this.ownerDocument.URL : ''; }
  hasChildNodes() { return this.childNodes.length > 0; }
  getRootNode() { let n = this; while (n.parentNode) n = n.parentNode; return n; }
  contains(o) { for (let n = o; n; n = n.parentNode) if (n === this) return true; return false; }
  compareDocumentPosition(o) {
    if (o === this) return 0;
    if (this.contains(o)) return 20; if (o.contains(this)) return 10;
    const a = [], b = [];
    for (let n = this; n; n = n.parentNode) a.unshift(n);
    for (let n = o; n; n = n.parentNode) b.unshift(n);
    let i = 0; while (a[i] === b[i]) i++;
    if (!a[i - 1]) return 1;
    const sib = a[i - 1].childNodes;
    return sib.indexOf(a[i]) < sib.indexOf(b[i]) ? 4 : 2;
  }
  isSameNode(o) { return o === this; } isEqualNode(o) { return o === this || (o && o.outerHTML !== undefined && o.outerHTML === this.outerHTML); }
  get textContent() {
    if (this.nodeType === TEXT_NODE || this.nodeType === COMMENT_NODE) return this.data;
    if (this.nodeType === DOCUMENT_NODE) return null;
    let s = '';
    for (const c of this.childNodes) if (c.nodeType !== COMMENT_NODE) s += c.textContent;
    return s;
  }
  set textContent(v) {
    if (this.nodeType === TEXT_NODE || this.nodeType === COMMENT_NODE) { this.data = v == null ? '' : String(v); return; }
    this._removeAll();
    if (v != null && v !== '') this.appendChild(this.ownerDocument.createTextNode(String(v)));
  }
  get nodeValue() { return (this.nodeType === TEXT_NODE || this.nodeType === COMMENT_NODE) ? this.data : null; }
  set nodeValue(v) { if (this.nodeType === TEXT_NODE || this.nodeType === COMMENT_NODE) this.data = String(v); }
  _removeAll() { const kids = this.childNodes.slice(); for (const c of kids) this.removeChild(c); }
  _adopt(child) {
    if (child.nodeType === DOCUMENT_FRAGMENT_NODE) return;
    if (child.parentNode) child.parentNode.removeChild(child);
    child.parentNode = this;
    const doc = this.nodeType === DOCUMENT_NODE ? this : this.ownerDocument;
    if (doc && child.ownerDocument !== doc) setOwner(child, doc);
  }
  appendChild(child) { return this.insertBefore(child, null); }
  insertBefore(child, ref) {
    if (child === ref) return child;
    if (child.nodeType === DOCUMENT_FRAGMENT_NODE) {
      const kids = child.childNodes.slice();
      for (const k of kids) this.insertBefore(k, ref);
      return child;
    }
    if (child === this || child.contains(this)) throw new DOMException('HierarchyRequestError', 'HierarchyRequestError');
    const doc = this.ownerDocument || this;
    const wasConnected = child.isConnected;
    this._adopt(child);
    let i = ref ? this.childNodes.indexOf(ref) : -1;
    if (i < 0) this.childNodes.push(child); else this.childNodes.splice(i, 0, child);
    doc._mutated(this, 'childList', { added: [child], removed: [] });
    if (this.isConnected) connectedNode(child, true, !wasConnected);
    return child;
  }
  removeChild(child) {
    const i = this.childNodes.indexOf(child);
    if (i < 0) throw new DOMException('NotFoundError', 'NotFoundError');
    const doc = this.ownerDocument || this;
    const wasConnected = this.isConnected;
    this.childNodes.splice(i, 1);
    child.parentNode = null;
    if (wasConnected) connectedNode(child, false, true);
    doc._mutated(this, 'childList', { added: [], removed: [child] });
    return child;
  }
  replaceChild(n, old) { this.insertBefore(n, old); this.removeChild(old); return old; }
  replaceChildren(...nodes) { this._removeAll(); for (const n of nodes) this.appendChild(toNode(this.ownerDocument, n)); }
  append(...nodes) { for (const n of nodes) this.appendChild(toNode(this.ownerDocument, n)); }
  prepend(...nodes) { const first = this.firstChild; for (const n of nodes) this.insertBefore(toNode(this.ownerDocument, n), first); }
  before(...nodes) { if (!this.parentNode) return; for (const n of nodes) this.parentNode.insertBefore(toNode(this.ownerDocument, n), this); }
  after(...nodes) { if (!this.parentNode) return; const next = this.nextSibling; for (const n of nodes) this.parentNode.insertBefore(toNode(this.ownerDocument, n), next); }
  replaceWith(...nodes) { const p = this.parentNode; if (!p) return; const next = this.nextSibling; p.removeChild(this); for (const n of nodes) p.insertBefore(toNode(this.ownerDocument, n), next); }
  remove() { if (this.parentNode) this.parentNode.removeChild(this); }
  cloneNode(deep) {
    const c = this._cloneSelf();
    if (deep) for (const k of this.childNodes) c.appendChild(k.cloneNode(true));
    return c;
  }
  normalize() {
    for (let i = 0; i < this.childNodes.length; i++) {
      const c = this.childNodes[i];
      if (c.nodeType === TEXT_NODE) {
        while (this.childNodes[i + 1] && this.childNodes[i + 1].nodeType === TEXT_NODE) { c.data += this.childNodes[i + 1].data; this.removeChild(this.childNodes[i + 1]); }
        if (c.data === '') { this.removeChild(c); i--; }
      } else c.normalize();
    }
  }
  lookupNamespaceURI() { return null; } isDefaultNamespace() { return true; }
}
function setOwner(n, doc) { n.ownerDocument = doc; for (const c of n.childNodes) setOwner(c, doc); }
function toNode(doc, n) { return typeof n === 'string' || typeof n === 'number' ? doc.createTextNode(String(n)) : n; }
// a subtree joined or left the document: custom hooks, style dirt
function connectedNode(n, on, changed) {
  if (n.nodeType === ELEMENT_NODE) {
    if (on) n._connected(); else n._disconnected();
  }
  for (const c of n.childNodes) connectedNode(c, on, changed);
}

export class DOMException extends Error { constructor(message, name = 'Error') { super(message); this.name = name; this.code = { IndexSizeError: 1, HierarchyRequestError: 3, WrongDocumentError: 4, InvalidCharacterError: 5, NotFoundError: 8, NotSupportedError: 9, InvalidStateError: 11, SyntaxError: 12, InvalidAccessError: 15, SecurityError: 18, NetworkError: 19, AbortError: 20, NotAllowedError: 0 }[name] || 0; } }

export class CharacterData extends Node {
  constructor(doc, data) { super(doc); this._data = String(data); }
  get data() { return this._data; }
  set data(v) { this._data = String(v); const doc = this.ownerDocument; if (doc) doc._mutated(this, 'characterData'); }
  get length() { return this._data.length; }
  substringData(o, c) { return this._data.substr(o, c); }
  appendData(s) { this.data = this._data + s; } insertData(o, s) { this.data = this._data.slice(0, o) + s + this._data.slice(o); }
  deleteData(o, c) { this.data = this._data.slice(0, o) + this._data.slice(o + c); } replaceData(o, c, s) { this.data = this._data.slice(0, o) + s + this._data.slice(o + c); }
  get nextElementSibling() { let n = this.nextSibling; while (n && n.nodeType !== ELEMENT_NODE) n = n.nextSibling; return n; }
  get previousElementSibling() { let n = this.previousSibling; while (n && n.nodeType !== ELEMENT_NODE) n = n.previousSibling; return n; }
}
export class Text extends CharacterData {
  constructor(doc, data) { super(doc, data); this.nodeType = TEXT_NODE; this.nodeName = '#text'; }
  splitText(o) { const t = this.ownerDocument.createTextNode(this._data.slice(o)); this.data = this._data.slice(0, o); if (this.parentNode) this.parentNode.insertBefore(t, this.nextSibling); return t; }
  get wholeText() { return this._data; }
  _cloneSelf() { return new Text(this.ownerDocument, this._data); }
  get assignedSlot() { return null; }
}
export class Comment extends CharacterData {
  constructor(doc, data) { super(doc, data); this.nodeType = COMMENT_NODE; this.nodeName = '#comment'; }
  _cloneSelf() { return new Comment(this.ownerDocument, this._data); }
}
export class DocumentType extends Node { constructor(doc, name) { super(doc); this.nodeType = DOCUMENT_TYPE_NODE; this.nodeName = name; this.name = name; this.publicId = ''; this.systemId = ''; } _cloneSelf() { return new DocumentType(this.ownerDocument, this.name); } }

// class="" as a live list
export class DOMTokenList {
  constructor(el, attr) { this.el = el; this.attr = attr; }
  _list() { const v = this.el.getAttribute(this.attr); return v ? v.split(/\s+/).filter(Boolean) : []; }
  _set(list) { this.el.setAttribute(this.attr, list.join(' ')); }
  get length() { return this._list().length; }
  get value() { return this.el.getAttribute(this.attr) || ''; }
  set value(v) { this.el.setAttribute(this.attr, v); }
  item(i) { return this._list()[i] || null; }
  contains(t) { return this._list().includes(t); }
  add(...ts) { const l = this._list(); let changed = false; for (const t of ts) if (t && !l.includes(t)) { l.push(t); changed = true; } if (changed) this._set(l); }
  remove(...ts) { const l = this._list(); const n = l.filter(x => !ts.includes(x)); if (n.length !== l.length) this._set(n); }
  toggle(t, force) { const has = this.contains(t); if (force === true || (force === undefined && !has)) { this.add(t); return true; } this.remove(t); return false; }
  replace(a, b) { const l = this._list(); const i = l.indexOf(a); if (i < 0) return false; l[i] = b; this._set(l); return true; }
  forEach(fn, self) { this._list().forEach((v, i) => fn.call(self, v, i, this)); }
  entries() { return this._list().entries(); } keys() { return this._list().keys(); } values() { return this._list().values(); }
  [Symbol.iterator]() { return this._list()[Symbol.iterator](); }
  toString() { return this.value; }
  supports() { return true; }
}

// inline style and computed style: a bag of css properties
import { parseDeclarations, serializeDeclarations, camelToDash, dashToCamel, expandShorthand, PENDING } from './css.mjs';
export class CSSStyleDeclaration {
  constructor(el, onChange) {
    this._el = el; this._onChange = onChange; this._props = new Map(); this._priority = new Map();
    return new Proxy(this, styleProxy);
  }
  get cssText() { return serializeDeclarations(this._props, this._priority); }
  set cssText(v) { this._props.clear(); this._priority.clear(); for (const [k, val, imp] of parseDeclarations(String(v))) this._put(k, val, imp); this._changed(); }
  get length() { return this._props.size; }
  item(i) { return [...this._props.keys()][i] || ''; }
  getPropertyValue(k) { const v = this._props.get(k) || ''; return v[0] === PENDING ? '' : v; }
  getPropertyPriority(k) { return this._priority.get(k) || ''; }
  setProperty(k, v, p) {
    if (v === null || v === undefined || v === '') { this.removeProperty(k); return; }
    this._put(k, String(v), p === 'important'); this._changed();
  }
  removeProperty(k) { const old = this._props.get(k) || ''; if (this._props.has(k)) { this._props.delete(k); this._priority.delete(k); const longs = expandShorthand(k, ''); if (longs) for (const l of longs) { this._props.delete(l[0]); this._priority.delete(l[0]); } this._changed(); } return old; }
  _put(k, v, imp) {
    this._props.set(k, v);
    if (imp) this._priority.set(k, 'important'); else this._priority.delete(k);
    const longs = expandShorthand(k, v);
    if (longs) for (const [lk, lv] of longs) { this._props.set(lk, lv); if (imp) this._priority.set(lk, 'important'); }
  }
  _changed() { if (this._onChange) this._onChange(); }
  get parentRule() { return null; }
  [Symbol.iterator]() { return this._props.keys(); }
}
const styleProxy = {
  get(t, k) {
    if (typeof k === 'symbol' || k in t) return t[k];
    if (typeof k === 'string' && /^\d+$/.test(k)) return t.item(Number(k));
    const d = k.startsWith('--') ? k : camelToDash(k === 'cssFloat' ? 'float' : k);
    return t._props.get(d) || '';
  },
  set(t, k, v) {
    if (typeof k === 'symbol' || (k in t && !(k in Object.getPrototypeOf(t)) === false && typeof t[k] !== 'string') ) {
      if (k === 'cssText') { t.cssText = v; return true; }
      if (k in t) { t[k] = v; return true; }
    }
    const d = k.startsWith('--') ? k : camelToDash(k === 'cssFloat' ? 'float' : k);
    t.setProperty(d, v);
    return true;
  },
  has(t, k) { return k in t || typeof k === 'string'; },
  ownKeys(t) { return [...t._props.keys()].map(dashToCamel); },
  getOwnPropertyDescriptor(t, k) { return { enumerable: true, configurable: true, value: t._props.get(camelToDash(String(k))) || '' }; },
};

let nextElementId = 1;
export class Element extends Node {
  constructor(doc, tag, ns) {
    super(doc);
    this.nodeType = ELEMENT_NODE;
    this.namespaceURI = ns || HTML_NS;
    this.localName = ns === SVG_NS ? tag : tag.toLowerCase();
    this.tagName = ns === SVG_NS ? tag : this.localName.toUpperCase();
    this.nodeName = this.tagName;
    this.prefix = null;
    this._attrs = new Map();
    this._style = null; this._computed = null; this._box = null; this._uid = nextElementId++;
    this._classList = null; this._dataset = null; this._pseudo = new Set(); this._styleDirty = true; this._classSet = undefined;
  }
  _cloneSelf() { const c = this.ownerDocument._makeElement(this.localName, this.namespaceURI); for (const [k, v] of this._attrs) c.setAttribute(k, v); return c; }
  _connected() { this.ownerDocument._elementConnected(this); }
  _disconnected() { this.ownerDocument._elementDisconnected(this); }
  // attributes
  getAttribute(k) { const v = this._attrs.get(lowerName(k)); return v === undefined ? null : v; }
  // the class attribute as a Set, kept until the attribute changes
  _classes() { if (this._classSet === undefined) { const c = this._attrs.get('class'); this._classSet = c ? new Set(c.split(/\s+/).filter(Boolean)) : null; } return this._classSet; }
  getAttributeNS(ns, k) { return this.getAttribute(k); }
  hasAttribute(k) { return this._attrs.has(lowerName(k)); }
  hasAttributeNS(ns, k) { return this.hasAttribute(k); }
  hasAttributes() { return this._attrs.size > 0; }
  getAttributeNames() { return [...this._attrs.keys()]; }
  setAttribute(k, v) {
    k = lowerName(k); v = String(v);
    const old = this._attrs.get(k);
    if (old === v) return;
    this._attrs.set(k, v);
    this._attrChanged(k, v, old);
  }
  setAttributeNS(ns, k, v) { this.setAttribute(k.includes(':') ? k.split(':')[1] : k, v); }
  removeAttribute(k) { k = lowerName(k); if (!this._attrs.has(k)) return; const old = this._attrs.get(k); this._attrs.delete(k); this._attrChanged(k, null, old); }
  removeAttributeNS(ns, k) { this.removeAttribute(k); }
  toggleAttribute(k, force) { const has = this.hasAttribute(k); if (force === true || (force === undefined && !has)) { this.setAttribute(k, ''); return true; } this.removeAttribute(k); return false; }
  getAttributeNode(k) { const v = this.getAttribute(k); return v === null ? null : { name: k, value: v, ownerElement: this, specified: true }; }
  setAttributeNode(a) { this.setAttribute(a.name, a.value); } removeAttributeNode(a) { this.removeAttribute(a.name); }
  get attributes() {
    const list = [...this._attrs].map(([name, value]) => ({ name, value, localName: name, nodeName: name, nodeValue: value, ownerElement: this, specified: true }));
    list.item = i => list[i] || null; list.getNamedItem = n => list.find(a => a.name === n) || null;
    return list;
  }
  _attrChanged(k, v, old) {
    if (k === 'style' && !this._settingStyle) { this._style = null; }
    if (k === 'class') this._classSet = undefined;
    if (k === 'id') this.ownerDocument._idChanged(this, old, v);
    this.ownerDocument._mutated(this, 'attributes', { attributeName: k, oldValue: old === undefined ? null : old });
    this._attrHook(k, v);
  }
  _attrHook() {}
  get id() { return this.getAttribute('id') || ''; } set id(v) { this.setAttribute('id', v); }
  get className() { return this.getAttribute('class') || ''; } set className(v) { this.setAttribute('class', v); }
  get classList() { return this._classList || (this._classList = new DOMTokenList(this, 'class')); }
  get slot() { return this.getAttribute('slot') || ''; } set slot(v) { this.setAttribute('slot', v); }
  get part() { return new DOMTokenList(this, 'part'); }
  get style() {
    if (!this._style) {
      const s = this._style = new CSSStyleDeclaration(this, () => {
        this._settingStyle = true;
        const t = s.cssText;
        if (t) this.setAttribute('style', t); else this.removeAttribute('style');
        this._settingStyle = false;
        this.ownerDocument._styleChanged(this);
      });
      const a = this.getAttribute('style');
      if (a) for (const [k, v, imp] of parseDeclarations(a)) s._put(k, v, imp);
    }
    return this._style;
  }
  get dataset() {
    if (!this._dataset) {
      const el = this;
      this._dataset = new Proxy({}, {
        get(_, k) { if (typeof k !== 'string') return undefined; const v = el.getAttribute('data-' + camelToDash(k)); return v === null ? undefined : v; },
        set(_, k, v) { el.setAttribute('data-' + camelToDash(k), v); return true; },
        deleteProperty(_, k) { el.removeAttribute('data-' + camelToDash(k)); return true; },
        has(_, k) { return el.hasAttribute('data-' + camelToDash(k)); },
        ownKeys() { return [...el._attrs.keys()].filter(k => k.startsWith('data-')).map(k => dashToCamel(k.slice(5))); },
        getOwnPropertyDescriptor(_, k) { return { enumerable: true, configurable: true, value: el.getAttribute('data-' + camelToDash(k)) }; },
      });
    }
    return this._dataset;
  }
  // tree
  get children() { return collection(this.childNodes.filter(n => n.nodeType === ELEMENT_NODE)); }
  get childElementCount() { return this.children.length; }
  get firstElementChild() { return this.childNodes.find(n => n.nodeType === ELEMENT_NODE) || null; }
  get lastElementChild() { const c = this.children; return c[c.length - 1] || null; }
  get nextElementSibling() { let n = this.nextSibling; while (n && n.nodeType !== ELEMENT_NODE) n = n.nextSibling; return n; }
  get previousElementSibling() { let n = this.previousSibling; while (n && n.nodeType !== ELEMENT_NODE) n = n.previousSibling; return n; }
  get innerHTML() { return this.childNodes.map(serialize).join(''); }
  set innerHTML(v) { this._removeAll(); const frag = this.ownerDocument._parseFragment(String(v), this); while (frag.firstChild) this.appendChild(frag.firstChild); }
  get outerHTML() { return serialize(this); }
  set outerHTML(v) { if (!this.parentNode) return; const frag = this.ownerDocument._parseFragment(String(v), this.parentNode); this.replaceWith(frag); }
  insertAdjacentHTML(where, html) {
    const frag = this.ownerDocument._parseFragment(String(html), this.parentNode || this);
    this.insertAdjacentElement(where, frag);
  }
  insertAdjacentElement(where, el) {
    switch (where) {
    case 'beforebegin': this.before(el); break; case 'afterbegin': this.prepend(el); break;
    case 'beforeend': this.append(el); break; case 'afterend': this.after(el); break;
    }
    return el;
  }
  insertAdjacentText(where, t) { this.insertAdjacentElement(where, this.ownerDocument.createTextNode(t)); }
  get innerText() { return this.textContent; } set innerText(v) { this.textContent = v; }
  get outerText() { return this.textContent; }
  querySelector(s) { return this.ownerDocument._query(this, s, true); }
  querySelectorAll(s) { return this.ownerDocument._query(this, s, false); }
  matches(s) { return this.ownerDocument._matches(this, s); }
  webkitMatchesSelector(s) { return this.matches(s); }
  closest(s) { for (let n = this; n && n.nodeType === ELEMENT_NODE; n = n.parentNode) if (n.matches(s)) return n; return null; }
  getElementsByTagName(t) { t = t.toLowerCase(); return allDescendants(this, n => t === '*' || n.localName === t); }
  getElementsByTagNameNS(ns, t) { return this.getElementsByTagName(t); }
  getElementsByClassName(c) { const cs = c.split(/\s+/).filter(Boolean); return allDescendants(this, n => cs.every(x => n.classList.contains(x))); }
  // boxes: layout is brought up to date by the page before a read
  _layout() { return this.ownerDocument._page ? this.ownerDocument._page.ensureLayout() : null; }
  getBoundingClientRect() {
    this._layout();
    const b = this._box;
    if (!b) return rect(0, 0, 0, 0);
    const r = b.absRect();
    return rect(r.x, r.y, r.w, r.h);
  }
  getClientRects() { const r = this.getBoundingClientRect(); const list = r.width || r.height ? [r] : []; list.item = i => list[i] || null; return list; }
  get offsetParent() { this._layout(); const b = this._box; return b && b.offsetParent ? b.offsetParent.el : null; }
  get offsetLeft() { this._layout(); return this._box ? Math.round(this._box.offsetLeft()) : 0; }
  get offsetTop() { this._layout(); return this._box ? Math.round(this._box.offsetTop()) : 0; }
  get offsetWidth() { this._layout(); return this._box ? Math.round(this._box.w) : 0; }
  get offsetHeight() { this._layout(); return this._box ? Math.round(this._box.h) : 0; }
  get clientLeft() { this._layout(); return this._box ? Math.round(this._box.bl) : 0; }
  get clientTop() { this._layout(); return this._box ? Math.round(this._box.bt) : 0; }
  get clientWidth() { this._layout(); return this._box ? Math.round(this._box.clientW()) : 0; }
  get clientHeight() { this._layout(); return this._box ? Math.round(this._box.clientH()) : 0; }
  get scrollWidth() { this._layout(); return this._box ? Math.round(this._box.scrollW()) : 0; }
  get scrollHeight() { this._layout(); return this._box ? Math.round(this._box.scrollH()) : 0; }
  get scrollLeft() { return this._scrollX || 0; }
  set scrollLeft(v) { this._setScroll(v, this.scrollTop); }
  get scrollTop() { return this._scrollY || 0; }
  set scrollTop(v) { this._setScroll(this.scrollLeft, v); }
  _setScroll(x, y) {
    this._layout();
    const b = this._box;
    const mx = b ? Math.max(0, b.scrollW() - b.clientW()) : 0, my = b ? Math.max(0, b.scrollH() - b.clientH()) : 0;
    x = Math.max(0, Math.min(mx, Number(x) || 0)); y = Math.max(0, Math.min(my, Number(y) || 0));
    if (x === (this._scrollX || 0) && y === (this._scrollY || 0)) return;
    this._scrollX = x; this._scrollY = y;
    this.ownerDocument._page && this.ownerDocument._page.scrolled(this);
  }
  scrollTo(x, y) { if (typeof x === 'object') { y = x.top ?? this.scrollTop; x = x.left ?? this.scrollLeft; } this._setScroll(x, y); }
  scroll(x, y) { this.scrollTo(x, y); }
  scrollBy(x, y) { if (typeof x === 'object') { y = x.top || 0; x = x.left || 0; } this._setScroll(this.scrollLeft + x, this.scrollTop + y); }
  scrollIntoView() {}
  scrollIntoViewIfNeeded() {}
  // focus
  focus(opts) { this.ownerDocument._page && this.ownerDocument._page.focusElement(this, opts); }
  blur() { if (this.ownerDocument.activeElement === this) this.ownerDocument._page.focusElement(null); }
  get tabIndex() { const t = this.getAttribute('tabindex'); return t === null ? (FOCUSABLE.has(this.localName) ? 0 : -1) : parseInt(t) || 0; }
  set tabIndex(v) { this.setAttribute('tabindex', String(v)); }
  get isContentEditable() { const c = this.getAttribute('contenteditable'); return c !== null && c !== 'false'; }
  get contentEditable() { return this.getAttribute('contenteditable') || 'inherit'; } set contentEditable(v) { this.setAttribute('contenteditable', v); }
  get title() { return this.getAttribute('title') || ''; } set title(v) { this.setAttribute('title', v); }
  get lang() { return this.getAttribute('lang') || ''; } set lang(v) { this.setAttribute('lang', v); }
  get dir() { return this.getAttribute('dir') || ''; } set dir(v) { this.setAttribute('dir', v); }
  get hidden() { return this.hasAttribute('hidden'); } set hidden(v) { this.toggleAttribute('hidden', !!v); }
  get draggable() { return this.getAttribute('draggable') === 'true'; } set draggable(v) { this.setAttribute('draggable', v ? 'true' : 'false'); }
  get spellcheck() { return this.getAttribute('spellcheck') !== 'false'; } set spellcheck(v) { this.setAttribute('spellcheck', v ? 'true' : 'false'); }
  get inert() { return this.hasAttribute('inert'); } set inert(v) { this.toggleAttribute('inert', !!v); }
  get autofocus() { return this.hasAttribute('autofocus'); }
  get role() { return this.getAttribute('role'); } set role(v) { if (v == null) this.removeAttribute('role'); else this.setAttribute('role', v); }
  get ariaLabel() { return this.getAttribute('aria-label'); } set ariaLabel(v) { if (v == null) this.removeAttribute('aria-label'); else this.setAttribute('aria-label', v); }
  get nonce() { return this.getAttribute('nonce') || ''; } set nonce(v) { this.setAttribute('nonce', v); }
  get shadowRoot() { return this._shadow || null; }
  attachShadow(init) { this._shadow = new DocumentFragment(this.ownerDocument); this._shadow.host = this; this._shadow.mode = init.mode; this._shadow.adoptedStyleSheets = []; this._shadow.innerHTML = ''; this._shadow.getElementById = id => this._shadow.querySelector('#' + id); return this._shadow; }
  animate(keyframes, opts) { return makeAnimation(this, keyframes, opts); }
  getAnimations() { return []; }
  click() { this.dispatchEvent(new (this.ownerDocument._window.MouseEvent)('click', { bubbles: true, cancelable: true, composed: true })); }
  requestPointerLock() {} releasePointerCapture() {} setPointerCapture() {} hasPointerCapture() { return false; }
  requestFullscreen() { return Promise.resolve(); }
  checkVisibility() { const b = this.getBoundingClientRect(); return b.width > 0 || b.height > 0; }
  computedStyleMap() { return { get: k => ({ value: this.ownerDocument._window.getComputedStyle(this).getPropertyValue(k) }) }; }
  get assignedSlot() { return null; }
  toString() { return `[object ${this.constructor.name}]`; }
}
const FOCUSABLE = new Set(['input', 'textarea', 'select', 'button', 'a', 'iframe', 'summary']);
function rect(x, y, w, h) { return { x, y, width: w, height: h, top: y, left: x, right: x + w, bottom: y + h, toJSON() { return { ...this }; } }; }
function allDescendants(root, test) {
  const out = [];
  const walk = n => { for (const c of n.childNodes) { if (c.nodeType === ELEMENT_NODE) { if (test(c)) out.push(c); walk(c); } } };
  walk(root);
  out.item = i => out[i] || null; out.namedItem = id => out.find(e => e.id === id) || null;
  return out;
}
function makeAnimation(el, keyframes, opts) {
  // Web Animations: the last keyframe lands at once, the promise then
  const dur = typeof opts === 'number' ? opts : (opts && opts.duration) || 0;
  const fin = {};
  const anim = { playState: 'running', currentTime: 0, playbackRate: 1, effect: { getTiming: () => ({ duration: dur }), getComputedTiming: () => ({ duration: dur, progress: 1 }) }, oncancel: null, onfinish: null,
    cancel() { this.playState = 'idle'; if (this.oncancel) this.oncancel(); }, finish() { this.playState = 'finished'; }, pause() { this.playState = 'paused'; }, play() { this.playState = 'running'; }, reverse() {}, commitStyles() {}, persist() {}, updatePlaybackRate() {},
    addEventListener(t, f) { if (t === 'finish') this.onfinish = f; }, removeEventListener() {} };
  anim.finished = new Promise(r => setTimeout(() => { anim.playState = 'finished'; if (opts && opts.fill === 'forwards' && Array.isArray(keyframes) && keyframes.length) { const last = keyframes[keyframes.length - 1]; for (const k in last) if (k !== 'offset' && k !== 'easing') el.style[k] = last[k]; } if (anim.onfinish) anim.onfinish(fin); r(anim); }, dur));
  anim.ready = Promise.resolve(anim);
  return anim;
}

// serialization
const ESC = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' };
export function escapeText(s) { return s.replace(/[&<>]/g, c => ESC[c]); }
export function serialize(n) {
  switch (n.nodeType) {
  case TEXT_NODE: return n.parentNode && RAW.has(n.parentNode.localName) ? n.data : escapeText(n.data);
  case COMMENT_NODE: return `<!--${n.data}-->`;
  case DOCUMENT_TYPE_NODE: return `<!DOCTYPE ${n.name}>`;
  case DOCUMENT_FRAGMENT_NODE: case DOCUMENT_NODE: return n.childNodes.map(serialize).join('');
  case ELEMENT_NODE: {
    let s = '<' + n.localName;
    for (const [k, v] of n._attrs) s += ` ${k}="${v.replace(/[&"]/g, c => ESC[c])}"`;
    s += '>';
    if (VOID.has(n.localName)) return s;
    if (n.localName === 'template') return s + n.content.childNodes.map(serialize).join('') + '</template>';
    return s + n.childNodes.map(serialize).join('') + `</${n.localName}>`;
  }
  }
  return '';
}
const RAW = new Set(['script', 'style', 'xmp', 'iframe', 'noembed', 'noframes', 'plaintext']);

export class DocumentFragment extends Node {
  constructor(doc) { super(doc); this.nodeType = DOCUMENT_FRAGMENT_NODE; this.nodeName = '#document-fragment'; }
  _cloneSelf() { return new DocumentFragment(this.ownerDocument); }
  get children() { return collection(this.childNodes.filter(n => n.nodeType === ELEMENT_NODE)); }
  get firstElementChild() { return this.children[0] || null; } get lastElementChild() { const c = this.children; return c[c.length - 1] || null; }
  get childElementCount() { return this.children.length; }
  querySelector(s) { return this.ownerDocument._query(this, s, true); }
  querySelectorAll(s) { return this.ownerDocument._query(this, s, false); }
  getElementById(id) { return this.querySelector('#' + CSS_escape(id)); }
  get innerHTML() { return this.childNodes.map(serialize).join(''); }
  set innerHTML(v) { this._removeAll(); const frag = this.ownerDocument._parseFragment(String(v), this.host || this); while (frag.firstChild) this.appendChild(frag.firstChild); }
  get activeElement() { return this.ownerDocument.activeElement; }
  get styleSheets() { return []; }
  elementFromPoint(x, y) { return this.ownerDocument.elementFromPoint(x, y); }
}
export function CSS_escape(s) { return String(s).replace(/([^a-zA-Z0-9_-])/g, '\\$1'); }
export { VOID, SVG_NS, HTML_NS };
