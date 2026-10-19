// the document and the html element kinds pages use
import { Node, Element, Text, Comment, DocumentType, DocumentFragment, DOMException, ELEMENT_NODE, TEXT_NODE, SVG_NS, HTML_NS, CSS_escape, collection } from './node.mjs';
import { parseHTML } from './html.mjs';
import { parseSelectorList, matchesComplex, parseStylesheet, parseDeclarations, serializeDeclarations } from './css.mjs';
import { Event, CustomEvent, InputEvent } from './events.mjs';

export class HTMLElement extends Element {
  get accessKey() { return this.getAttribute('accesskey') || ''; }
  get translate() { return true; }
  get offsetParentElement() { return this.offsetParent; }
  get form() { return this.closest('form'); }
  get labels() { return []; }
  get validity() { return { valid: true, valueMissing: false, typeMismatch: false, patternMismatch: false, tooLong: false, tooShort: false, rangeUnderflow: false, rangeOverflow: false, stepMismatch: false, badInput: false, customError: false }; }
  checkValidity() { return true; } reportValidity() { return true; } setCustomValidity() {}
  get validationMessage() { return ''; } get willValidate() { return false; }
  get popover() { return this.getAttribute('popover'); } showPopover() {} hidePopover() {} togglePopover() {}
  get enterKeyHint() { return ''; } get inputMode() { return this.getAttribute('inputmode') || ''; } set inputMode(v) { this.setAttribute('inputmode', v); }
  get autocapitalize() { return ''; }
}
export class HTMLUnknownElement extends HTMLElement {}
export class HTMLAnchorElement extends HTMLElement {
  get href() { const h = this.getAttribute('href'); if (h === null) return ''; try { return new URL(h, this.baseURI).href; } catch { return h; } }
  set href(v) { this.setAttribute('href', v); }
  get target() { return this.getAttribute('target') || ''; } set target(v) { this.setAttribute('target', v); }
  get download() { return this.getAttribute('download') || ''; } set download(v) { this.setAttribute('download', v); }
  get rel() { return this.getAttribute('rel') || ''; } set rel(v) { this.setAttribute('rel', v); }
  get protocol() { try { return new URL(this.href).protocol; } catch { return ''; } }
  get hostname() { try { return new URL(this.href).hostname; } catch { return ''; } }
  get pathname() { try { return new URL(this.href).pathname; } catch { return ''; } }
  get origin() { try { return new URL(this.href).origin; } catch { return ''; } }
  get hash() { try { return new URL(this.href).hash; } catch { return ''; } }
  get search() { try { return new URL(this.href).search; } catch { return ''; } }
  get text() { return this.textContent; } set text(v) { this.textContent = v; }
  toString() { return this.href; }
}
export class HTMLButtonElement extends HTMLElement {
  get type() { return this.getAttribute('type') || 'submit'; } set type(v) { this.setAttribute('type', v); }
  get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); }
  get value() { return this.getAttribute('value') || ''; } set value(v) { this.setAttribute('value', v); }
  get name() { return this.getAttribute('name') || ''; } set name(v) { this.setAttribute('name', v); }
}
export class HTMLLabelElement extends HTMLElement { get htmlFor() { return this.getAttribute('for') || ''; } set htmlFor(v) { this.setAttribute('for', v); } get control() { const f = this.getAttribute('for'); return f ? this.ownerDocument.getElementById(f) : this.querySelector('input,textarea,select,button'); } }
export class HTMLFormElement extends HTMLElement { submit() {} reset() {} requestSubmit() {} get elements() { return this.querySelectorAll('input,textarea,select,button'); } get action() { return this.getAttribute('action') || ''; } get method() { return this.getAttribute('method') || 'get'; } }
export class HTMLBRElement extends HTMLElement {}
export class HTMLSpanElement extends HTMLElement {} export class HTMLDivElement extends HTMLElement {} export class HTMLParagraphElement extends HTMLElement {} export class HTMLHeadingElement extends HTMLElement {} export class HTMLUListElement extends HTMLElement {} export class HTMLOListElement extends HTMLElement {} export class HTMLLIElement extends HTMLElement {} export class HTMLPreElement extends HTMLElement {} export class HTMLHRElement extends HTMLElement {} export class HTMLBodyElement extends HTMLElement {} export class HTMLHeadElement extends HTMLElement {} export class HTMLHtmlElement extends HTMLElement {} export class HTMLTitleElement extends HTMLElement { get text() { return this.textContent; } set text(v) { this.textContent = v; } } export class HTMLMetaElement extends HTMLElement { get content() { return this.getAttribute('content') || ''; } get name() { return this.getAttribute('name') || ''; } } export class HTMLTableElement extends HTMLElement { get rows() { return this.querySelectorAll('tr'); } get tBodies() { return this.querySelectorAll('tbody'); } createTBody() { const t = this.ownerDocument.createElement('tbody'); this.appendChild(t); return t; } insertRow(i) { const tb = this.querySelector('tbody') || this.createTBody(); return tb.insertRow(i); } } export class HTMLTableSectionElement extends HTMLElement { get rows() { return this.querySelectorAll('tr'); } insertRow(i = -1) { const r = this.ownerDocument.createElement('tr'); const rows = this.rows; if (i < 0 || i >= rows.length) this.appendChild(r); else this.insertBefore(r, rows[i]); return r; } deleteRow(i) { const r = this.rows[i]; if (r) r.remove(); } } export class HTMLTableRowElement extends HTMLElement { get cells() { return this.querySelectorAll('td,th'); } insertCell(i = -1) { const c = this.ownerDocument.createElement('td'); const cells = this.cells; if (i < 0 || i >= cells.length) this.appendChild(c); else this.insertBefore(c, cells[i]); return c; } get rowIndex() { const t = this.closest('table'); return t ? [...t.rows].indexOf(this) : -1; } } export class HTMLTableCellElement extends HTMLElement { get cellIndex() { return this.parentNode ? this.parentNode.cells.indexOf(this) : -1; } get colSpan() { return parseInt(this.getAttribute('colspan')) || 1; } set colSpan(v) { this.setAttribute('colspan', v); } get rowSpan() { return parseInt(this.getAttribute('rowspan')) || 1; } }
export class HTMLDialogElement extends HTMLElement { get open() { return this.hasAttribute('open'); } set open(v) { this.toggleAttribute('open', !!v); } show() { this.open = true; } showModal() { this.open = true; } close(v) { this.returnValue = v; this.open = false; this.dispatchEvent(new Event('close')); } }
export class HTMLDetailsElement extends HTMLElement { get open() { return this.hasAttribute('open'); } set open(v) { const was = this.open; this.toggleAttribute('open', !!v); if (was !== !!v) this.dispatchEvent(new Event('toggle')); } }
export class HTMLProgressElement extends HTMLElement { get value() { return parseFloat(this.getAttribute('value')) || 0; } set value(v) { this.setAttribute('value', v); } get max() { return parseFloat(this.getAttribute('max')) || 1; } set max(v) { this.setAttribute('max', v); } }
export class HTMLIFrameElement extends HTMLElement { get contentWindow() { return null; } get contentDocument() { return null; } get src() { return this.getAttribute('src') || ''; } set src(v) { this.setAttribute('src', v); } get srcdoc() { return this.getAttribute('srcdoc') || ''; } set srcdoc(v) { this.setAttribute('srcdoc', v); } get sandbox() { return this.classList; } get allow() { return ''; } }
export class HTMLTemplateElement extends HTMLElement {
  get content() { if (!this._content) this._content = this.ownerDocument.createDocumentFragment(); return this._content; }
  _cloneSelf() { const c = super._cloneSelf(); c._content = this.content.cloneNode(true); return c; }
  appendChild(n) { return this.content.appendChild(n); }
  insertBefore(n, r) { return this.content.insertBefore(n, r); }
  get innerHTML() { return this.content.innerHTML; } set innerHTML(v) { this.content.innerHTML = v; }
}
export class HTMLSlotElement extends HTMLElement { assignedNodes() { return []; } assignedElements() { return []; } }
export class HTMLStyleElement extends HTMLElement {
  _connected() { super._connected(); this.ownerDocument._sheetsChanged(); }
  _disconnected() { super._disconnected(); this.ownerDocument._sheetsChanged(); }
  get sheet() { if (!this._sheet) this._sheet = new CSSStyleSheet(this.ownerDocument, this); return this._sheet; }
  get media() { return this.getAttribute('media') || ''; } set media(v) { this.setAttribute('media', v); }
  get disabled() { return this._disabled || false; } set disabled(v) { this._disabled = !!v; this.ownerDocument._sheetsChanged(); }
}
export class HTMLLinkElement extends HTMLElement {
  get rel() { return this.getAttribute('rel') || ''; } set rel(v) { this.setAttribute('rel', v); }
  get href() { const h = this.getAttribute('href'); try { return h ? new URL(h, this.baseURI).href : ''; } catch { return h || ''; } } set href(v) { this.setAttribute('href', v); }
  get relList() { return this.classList; } get type() { return this.getAttribute('type') || ''; } get as() { return this.getAttribute('as') || ''; }
  get disabled() { return this._disabled || false; } set disabled(v) { this._disabled = !!v; this.ownerDocument._sheetsChanged(); }
  get sheet() { return this._sheet || null; }
  _connected() { super._connected(); this.ownerDocument._linkAdded(this); }
  _attrHook(k) { if (k === 'href' || k === 'rel') { if (this.isConnected) this.ownerDocument._linkAdded(this); } }
  _disconnected() { super._disconnected(); this.ownerDocument._sheetsChanged(); }
}
export class HTMLScriptElement extends HTMLElement {
  get src() { const s = this.getAttribute('src'); try { return s ? new URL(s, this.baseURI).href : ''; } catch { return s || ''; } } set src(v) { this.setAttribute('src', v); }
  get type() { return this.getAttribute('type') || ''; } set type(v) { this.setAttribute('type', v); }
  get text() { return this.textContent; } set text(v) { this.textContent = v; }
  get async() { return this.hasAttribute('async'); } set async(v) { this.toggleAttribute('async', !!v); }
  get defer() { return this.hasAttribute('defer'); } set defer(v) { this.toggleAttribute('defer', !!v); }
  get noModule() { return this.hasAttribute('nomodule'); } get crossOrigin() { return this.getAttribute('crossorigin'); } set crossOrigin(v) { this.setAttribute('crossorigin', v); }
  _connected() { super._connected(); if (!this._ran) this.ownerDocument._scriptAdded(this); }
}
export class HTMLImageElement extends HTMLElement {
  constructor(doc, tag, ns) { super(doc, tag || 'img', ns); this.naturalWidth = 0; this.naturalHeight = 0; this.complete = false; this._image = null; }
  get src() { const s = this.getAttribute('src'); try { return s ? new URL(s, this.baseURI).href : ''; } catch { return s || ''; } } set src(v) { this.setAttribute('src', v); }
  get srcset() { return this.getAttribute('srcset') || ''; } set srcset(v) { this.setAttribute('srcset', v); }
  get alt() { return this.getAttribute('alt') || ''; } set alt(v) { this.setAttribute('alt', v); }
  get width() { return this.offsetWidth || this.naturalWidth; } set width(v) { this.setAttribute('width', v); }
  get height() { return this.offsetHeight || this.naturalHeight; } set height(v) { this.setAttribute('height', v); }
  get currentSrc() { return this.src; } get loading() { return 'eager'; } set loading(v) {} get decoding() { return 'auto'; } set decoding(v) {} get crossOrigin() { return null; } set crossOrigin(v) {} get referrerPolicy() { return ''; } set referrerPolicy(v) {}
  decode() { return Promise.resolve(); }
  _attrHook(k, v) { if (k === 'src') this.ownerDocument._page && this.ownerDocument._page.loadImage(this); }
  _connected() { super._connected(); if (this.getAttribute('src') && !this._image) this.ownerDocument._page && this.ownerDocument._page.loadImage(this); }
}
export class HTMLCanvasElement extends HTMLElement {
  get width() { return parseInt(this.getAttribute('width')) || 300; } set width(v) { this.setAttribute('width', String(v)); this._resetContext(); }
  get height() { return parseInt(this.getAttribute('height')) || 150; } set height(v) { this.setAttribute('height', String(v)); this._resetContext(); }
  _resetContext() { if (this._ctx) this._ctx._reset(); }
  getContext(kind, opts) {
    if (kind !== '2d') return null;
    if (!this._ctx) { const page = this.ownerDocument._page; this._ctx = page ? page.makeCanvas2D(this) : null; }
    return this._ctx;
  }
  toDataURL() { return 'data:,'; } toBlob(cb) { cb && cb(null); } captureStream() { return null; } transferControlToOffscreen() { return this; }
  _attrHook(k) { if (k === 'width' || k === 'height') this._resetContext(); }
}
export class HTMLVideoElement extends HTMLElement { play() { return Promise.resolve(); } pause() {} load() {} canPlayType() { return ''; } get videoWidth() { return 0; } get videoHeight() { return 0; } get paused() { return true; } get duration() { return 0; } get currentTime() { return 0; } set currentTime(v) {} get readyState() { return 0; } get muted() { return true; } set muted(v) {} get volume() { return 1; } set volume(v) {} get src() { return this.getAttribute('src') || ''; } set src(v) { this.setAttribute('src', v); } get srcObject() { return null; } set srcObject(v) {} captureStream() { return null; } requestPictureInPicture() { return Promise.reject(new Error('no')); } get textTracks() { return []; } addTextTrack() { return { addCue() {}, removeCue() {}, cues: [] }; } }
export class HTMLAudioElement extends HTMLVideoElement {}
export class HTMLSourceElement extends HTMLElement {}
export class HTMLInputElement extends HTMLElement {
  constructor(doc, tag, ns) { super(doc, tag || 'input', ns); this._value = null; this._checked = null; this.selectionStart = 0; this.selectionEnd = 0; this.selectionDirection = 'none'; this.indeterminate = false; this._dirtyValue = false; }
  get type() { const t = (this.getAttribute('type') || 'text').toLowerCase(); return t; } set type(v) { this.setAttribute('type', v); }
  get value() {
    const t = this.type;
    if (t === 'checkbox' || t === 'radio') return this.getAttribute('value') ?? 'on';
    if (t === 'file') return this._files && this._files.length ? 'C:\\fakepath\\' + this._files[0].name : '';
    return this._value !== null ? this._value : (this.getAttribute('value') || '');
  }
  set value(v) { v = String(v == null ? '' : v); if (this.type === 'checkbox' || this.type === 'radio' || this.type === 'file') { this.setAttribute('value', v); return; } if (v === this._value) return; this._value = v; this._dirtyValue = true; this.selectionStart = this.selectionEnd = v.length; this.ownerDocument._mutated(this, 'value'); }
  get defaultValue() { return this.getAttribute('value') || ''; } set defaultValue(v) { this.setAttribute('value', v); }
  get checked() { return this._checked !== null ? this._checked : this.hasAttribute('checked'); }
  set checked(v) { v = !!v; if (v === this.checked) return; this._checked = v; if (v && this.type === 'radio' && this.name) { for (const r of this.ownerDocument.querySelectorAll(`input[type=radio][name="${this.name}"]`)) if (r !== this) r._checked = false; } this.ownerDocument._styleChanged(this); }
  get defaultChecked() { return this.hasAttribute('checked'); }
  get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); }
  get readOnly() { return this.hasAttribute('readonly'); } set readOnly(v) { this.toggleAttribute('readonly', !!v); }
  get required() { return this.hasAttribute('required'); } set required(v) { this.toggleAttribute('required', !!v); }
  get name() { return this.getAttribute('name') || ''; } set name(v) { this.setAttribute('name', v); }
  get placeholder() { return this.getAttribute('placeholder') || ''; } set placeholder(v) { this.setAttribute('placeholder', v); }
  get maxLength() { return parseInt(this.getAttribute('maxlength')) || -1; } set maxLength(v) { this.setAttribute('maxlength', v); }
  get size() { return parseInt(this.getAttribute('size')) || 20; } set size(v) { this.setAttribute('size', v); }
  get min() { return this.getAttribute('min') || ''; } set min(v) { this.setAttribute('min', v); } get max() { return this.getAttribute('max') || ''; } set max(v) { this.setAttribute('max', v); } get step() { return this.getAttribute('step') || ''; } set step(v) { this.setAttribute('step', v); }
  get valueAsNumber() { const n = parseFloat(this.value); return isNaN(n) ? NaN : n; } set valueAsNumber(v) { this.value = String(v); }
  get autocomplete() { return this.getAttribute('autocomplete') || ''; } set autocomplete(v) { this.setAttribute('autocomplete', v); }
  get files() { return this._files || []; } set files(v) { this._files = v; }
  get list() { return null; } get accept() { return this.getAttribute('accept') || ''; } set accept(v) { this.setAttribute('accept', v); } get multiple() { return this.hasAttribute('multiple'); } set multiple(v) { this.toggleAttribute('multiple', !!v); }
  get webkitdirectory() { return this.hasAttribute('webkitdirectory'); } set webkitdirectory(v) { this.toggleAttribute('webkitdirectory', !!v); }
  select() { this.selectionStart = 0; this.selectionEnd = this.value.length; this.focus(); }
  setSelectionRange(s, e, d) { this.selectionStart = Math.max(0, Math.min(s, this.value.length)); this.selectionEnd = Math.max(this.selectionStart, Math.min(e, this.value.length)); this.selectionDirection = d || 'none'; this.ownerDocument._page && this.ownerDocument._page.needPaint(); }
  setRangeText(t, s = this.selectionStart, e = this.selectionEnd) { const v = this.value; this.value = v.slice(0, s) + t + v.slice(e); this.setSelectionRange(s + t.length, s + t.length); }
  stepUp() {} stepDown() {} showPicker() {}
  get form() { return this.closest('form'); } get labels() { return this.ownerDocument.querySelectorAll(`label[for="${this.id}"]`); }
  _attrHook(k, v) { if (k === 'value' && !this._dirtyValue) this.ownerDocument._mutated(this, 'value'); }
}
export class HTMLTextAreaElement extends HTMLElement {
  constructor(doc, tag, ns) { super(doc, tag || 'textarea', ns); this._value = null; this.selectionStart = 0; this.selectionEnd = 0; this.selectionDirection = 'none'; }
  get type() { return 'textarea'; }
  get value() { return this._value !== null ? this._value : this.textContent; }
  set value(v) { v = String(v == null ? '' : v); if (v === this.value && this._value !== null) return; this._value = v; this.selectionStart = this.selectionEnd = v.length; this.ownerDocument._mutated(this, 'value'); }
  get defaultValue() { return this.textContent; } set defaultValue(v) { this.textContent = v; }
  get textLength() { return this.value.length; }
  get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); }
  get readOnly() { return this.hasAttribute('readonly'); } set readOnly(v) { this.toggleAttribute('readonly', !!v); }
  get required() { return this.hasAttribute('required'); } set required(v) { this.toggleAttribute('required', !!v); }
  get name() { return this.getAttribute('name') || ''; } set name(v) { this.setAttribute('name', v); }
  get placeholder() { return this.getAttribute('placeholder') || ''; } set placeholder(v) { this.setAttribute('placeholder', v); }
  get rows() { return parseInt(this.getAttribute('rows')) || 2; } set rows(v) { this.setAttribute('rows', v); } get cols() { return parseInt(this.getAttribute('cols')) || 20; } set cols(v) { this.setAttribute('cols', v); }
  get wrap() { return this.getAttribute('wrap') || ''; } set wrap(v) { this.setAttribute('wrap', v); }
  get maxLength() { return parseInt(this.getAttribute('maxlength')) || -1; } set maxLength(v) { this.setAttribute('maxlength', v); }
  get autocomplete() { return this.getAttribute('autocomplete') || ''; } set autocomplete(v) { this.setAttribute('autocomplete', v); }
  select() { this.selectionStart = 0; this.selectionEnd = this.value.length; this.focus(); }
  setSelectionRange(s, e, d) { this.selectionStart = Math.max(0, Math.min(s, this.value.length)); this.selectionEnd = Math.max(this.selectionStart, Math.min(e, this.value.length)); this.selectionDirection = d || 'none'; this.ownerDocument._page && this.ownerDocument._page.needPaint(); }
  setRangeText(t, s = this.selectionStart, e = this.selectionEnd) { const v = this.value; this.value = v.slice(0, s) + t + v.slice(e); this.setSelectionRange(s + t.length, s + t.length); }
  get form() { return this.closest('form'); } get labels() { return []; }
}
export class HTMLOptionElement extends HTMLElement {
  get value() { return this.hasAttribute('value') ? this.getAttribute('value') : this.textContent.trim(); } set value(v) { this.setAttribute('value', v); }
  get text() { return this.textContent.trim(); } set text(v) { this.textContent = v; }
  get label() { return this.getAttribute('label') || this.text; } set label(v) { this.setAttribute('label', v); }
  get selected() { return this._selected !== undefined ? this._selected : this.hasAttribute('selected'); } set selected(v) { this._selected = !!v; this.ownerDocument._mutated(this, 'value'); }
  get defaultSelected() { return this.hasAttribute('selected'); } set defaultSelected(v) { this.toggleAttribute('selected', !!v); }
  get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); }
  get index() { const s = this.closest('select'); return s ? [...s.options].indexOf(this) : 0; }
  get form() { return this.closest('form'); }
}
export class HTMLOptGroupElement extends HTMLElement { get label() { return this.getAttribute('label') || ''; } set label(v) { this.setAttribute('label', v); } get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); } }
export class HTMLSelectElement extends HTMLElement {
  get options() { const list = this.querySelectorAll('option'); list.length = list.length; return list; }
  get selectedIndex() { const o = this.options; for (let i = 0; i < o.length; i++) if (o[i].selected) return i; return o.length ? 0 : -1; }
  set selectedIndex(i) { const o = this.options; o.forEach((x, k) => { x._selected = k === i; }); this.ownerDocument._mutated(this, 'value'); }
  get value() { const o = this.options[this.selectedIndex]; return o ? o.value : ''; }
  set value(v) { const o = this.options; let found = false; o.forEach(x => { x._selected = (!found && x.value === String(v)); if (x._selected) found = true; }); this.ownerDocument._mutated(this, 'value'); }
  get selectedOptions() { return this.options.filter(o => o.selected); }
  get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); }
  get multiple() { return this.hasAttribute('multiple'); } get size() { return parseInt(this.getAttribute('size')) || 0; }
  get name() { return this.getAttribute('name') || ''; } set name(v) { this.setAttribute('name', v); }
  get length() { return this.options.length; } item(i) { return this.options[i] || null; } namedItem(n) { return this.querySelector(`option[name="${n}"],option#${n}`); }
  add(o, before) { this.insertBefore(o, typeof before === 'number' ? this.options[before] || null : before || null); }
  remove(i) { if (typeof i === 'number') { const o = this.options[i]; if (o) o.remove(); } else super.remove(); }
  get type() { return this.multiple ? 'select-multiple' : 'select-one'; }
  get form() { return this.closest('form'); } get labels() { return []; }
  showPicker() {}
}
export class HTMLOutputElement extends HTMLElement {}
export class HTMLFieldSetElement extends HTMLElement { get disabled() { return this.hasAttribute('disabled'); } set disabled(v) { this.toggleAttribute('disabled', !!v); } get elements() { return this.querySelectorAll('input,textarea,select,button'); } }
export class HTMLDataListElement extends HTMLElement { get options() { return this.querySelectorAll('option'); } }
export class HTMLLegendElement extends HTMLElement {}
export class HTMLBaseElement extends HTMLElement { get href() { return this.getAttribute('href') || ''; } }
export class HTMLObjectElement extends HTMLElement {} export class HTMLEmbedElement extends HTMLElement {} export class HTMLAreaElement extends HTMLElement {} export class HTMLMapElement extends HTMLElement {} export class HTMLPictureElement extends HTMLElement {} export class HTMLTimeElement extends HTMLElement {} export class HTMLDataElement extends HTMLElement {} export class HTMLMenuElement extends HTMLElement {} export class HTMLQuoteElement extends HTMLElement {} export class HTMLModElement extends HTMLElement {} export class HTMLDListElement extends HTMLElement {} export class HTMLTableCaptionElement extends HTMLElement {} export class HTMLTableColElement extends HTMLElement {} export class HTMLMeterElement extends HTMLProgressElement {} export class HTMLTrackElement extends HTMLElement {} export class HTMLFontElement extends HTMLElement {} export class HTMLMarqueeElement extends HTMLElement {} export class HTMLFrameSetElement extends HTMLElement {}
export class SVGElement extends Element {
  get ownerSVGElement() { return this.closest('svg'); } get viewportElement() { return this.ownerSVGElement; }
  get className() { return { baseVal: this.getAttribute('class') || '', animVal: this.getAttribute('class') || '' }; } set className(v) { this.setAttribute('class', typeof v === 'string' ? v : v.baseVal); }
  getBBox() { const r = this.getBoundingClientRect(); return { x: 0, y: 0, width: r.width, height: r.height }; }
  getScreenCTM() { return { a: 1, b: 0, c: 0, d: 1, e: 0, f: 0, inverse() { return this; } }; } getCTM() { return this.getScreenCTM(); }
  get viewBox() { const v = (this.getAttribute('viewBox') || '0 0 0 0').split(/[\s,]+/).map(Number); return { baseVal: { x: v[0], y: v[1], width: v[2], height: v[3] } }; }
  getTotalLength() { return 0; } getPointAtLength() { return { x: 0, y: 0 }; }
  createSVGPoint() { return { x: 0, y: 0, matrixTransform(m) { return { x: this.x * m.a + this.y * m.c + m.e, y: this.x * m.b + this.y * m.d + m.f }; } }; }
  get dataset() { return super.dataset; }
  focus() {} blur() {}
  get style() { return super.style; }
  get tabIndex() { return -1; }
  get width() { return { baseVal: { value: parseFloat(this.getAttribute('width')) || 0 } }; } get height() { return { baseVal: { value: parseFloat(this.getAttribute('height')) || 0 } }; }
}
const TAGS = { a: HTMLAnchorElement, button: HTMLButtonElement, label: HTMLLabelElement, form: HTMLFormElement, br: HTMLBRElement, span: HTMLSpanElement, div: HTMLDivElement, p: HTMLParagraphElement, h1: HTMLHeadingElement, h2: HTMLHeadingElement, h3: HTMLHeadingElement, h4: HTMLHeadingElement, h5: HTMLHeadingElement, h6: HTMLHeadingElement, ul: HTMLUListElement, ol: HTMLOListElement, li: HTMLLIElement, pre: HTMLPreElement, hr: HTMLHRElement, body: HTMLBodyElement, head: HTMLHeadElement, html: HTMLHtmlElement, title: HTMLTitleElement, meta: HTMLMetaElement, table: HTMLTableElement, thead: HTMLTableSectionElement, tbody: HTMLTableSectionElement, tfoot: HTMLTableSectionElement, tr: HTMLTableRowElement, td: HTMLTableCellElement, th: HTMLTableCellElement, dialog: HTMLDialogElement, details: HTMLDetailsElement, progress: HTMLProgressElement, meter: HTMLMeterElement, iframe: HTMLIFrameElement, template: HTMLTemplateElement, slot: HTMLSlotElement, style: HTMLStyleElement, link: HTMLLinkElement, script: HTMLScriptElement, img: HTMLImageElement, canvas: HTMLCanvasElement, video: HTMLVideoElement, audio: HTMLAudioElement, source: HTMLSourceElement, input: HTMLInputElement, textarea: HTMLTextAreaElement, option: HTMLOptionElement, optgroup: HTMLOptGroupElement, select: HTMLSelectElement, output: HTMLOutputElement, fieldset: HTMLFieldSetElement, datalist: HTMLDataListElement, legend: HTMLLegendElement, base: HTMLBaseElement, object: HTMLObjectElement, embed: HTMLEmbedElement, area: HTMLAreaElement, map: HTMLMapElement, picture: HTMLPictureElement, time: HTMLTimeElement, data: HTMLDataElement, menu: HTMLMenuElement, blockquote: HTMLQuoteElement, q: HTMLQuoteElement, ins: HTMLModElement, del: HTMLModElement, dl: HTMLDListElement, caption: HTMLTableCaptionElement, col: HTMLTableColElement, colgroup: HTMLTableColElement, track: HTMLTrackElement, font: HTMLFontElement, marquee: HTMLMarqueeElement, frameset: HTMLFrameSetElement };
const KNOWN = new Set(['abbr', 'address', 'article', 'aside', 'b', 'bdi', 'bdo', 'cite', 'code', 'dd', 'dfn', 'dt', 'em', 'figcaption', 'figure', 'footer', 'header', 'hgroup', 'i', 'kbd', 'main', 'mark', 'nav', 'noscript', 'rp', 'rt', 'ruby', 's', 'samp', 'section', 'small', 'strong', 'sub', 'summary', 'sup', 'u', 'var', 'wbr', 'center', 'search', 'nobr', 'tt', 'big', 'strike', 'acronym', 'plaintext', 'xmp', 'listing']);

// stylesheet objects pages script: insertRule and friends
export class CSSRule {
  constructor(sheet, text) { this.parentStyleSheet = sheet; this.cssText = text; const m = text.match(/^([^{]*)\{([\s\S]*)\}\s*$/); this.selectorText = m ? m[1].trim() : ''; this._body = m ? m[2] : ''; this.type = text.trim().startsWith('@') ? (text.startsWith('@media') ? 4 : text.startsWith('@keyframes') ? 7 : text.startsWith('@font-face') ? 5 : 0) : 1; this._style = null; this.parentRule = null; }
  get style() {
    if (!this._style) {
      const rule = this, sheet = this.parentStyleSheet;
      const props = new Map(), prio = new Map();
      for (const [k, v, imp] of parseDeclarations(this._body)) { props.set(k, v); if (imp) prio.set(k, 'important'); }
      this._style = new Proxy({}, {
        get(_, k) { if (k === 'cssText') return serializeDeclarations(props, prio); if (k === 'setProperty') return (n, v, p) => { props.set(n, String(v)); if (p) prio.set(n, p); rule._rewrite(props, prio); }; if (k === 'removeProperty') return n => { props.delete(n); rule._rewrite(props, prio); }; if (k === 'getPropertyValue') return n => props.get(n) || ''; if (k === 'length') return props.size; if (typeof k !== 'string') return undefined; return props.get(k.startsWith('--') ? k : k.replace(/[A-Z]/g, c => '-' + c.toLowerCase())) || ''; },
        set(_, k, v) { if (k === 'cssText') { props.clear(); prio.clear(); for (const [a, b, imp] of parseDeclarations(String(v))) { props.set(a, b); if (imp) prio.set(a, 'important'); } } else props.set(k.startsWith('--') ? k : k.replace(/[A-Z]/g, c => '-' + c.toLowerCase()), String(v)); rule._rewrite(props, prio); return true; },
      });
    }
    return this._style;
  }
  _rewrite(props, prio) { this._body = serializeDeclarations(props, prio); this.cssText = `${this.selectorText} { ${this._body} }`; this.parentStyleSheet._changed(); }
  get cssRules() { return []; } get media() { return { mediaText: this.selectorText.replace(/^@media\s*/, '') }; }
}
export class CSSStyleSheet {
  constructor(doc, owner) { this._doc = doc; this.ownerNode = owner || null; this._rules = []; this.disabled = false; this.media = { mediaText: '', appendMedium() {}, deleteMedium() {} }; this.href = null; this.title = ''; this.type = 'text/css'; this._parsed = null; this._fromOwner = !!owner; }
  get cssRules() { this._syncFromOwner(); return this._rules; } get rules() { return this.cssRules; }
  _syncFromOwner() { if (this.ownerNode && this._ownerText !== this.ownerNode.textContent) { this._ownerText = this.ownerNode.textContent; this._rules = splitRules(this._ownerText).map(t => new CSSRule(this, t)); } }
  insertRule(text, index = 0) { this._syncFromOwner(); this._rules.splice(index, 0, new CSSRule(this, text)); this._changed(); return index; }
  addRule(sel, body, index) { return this.insertRule(`${sel} { ${body} }`, index === undefined ? this._rules.length : index); }
  deleteRule(i) { this._syncFromOwner(); this._rules.splice(i, 1); this._changed(); } removeRule(i) { this.deleteRule(i); }
  replaceSync(text) { this._rules = splitRules(text).map(t => new CSSRule(this, t)); this._fromOwner = false; this._changed(); }
  replace(text) { this.replaceSync(text); return Promise.resolve(this); }
  get text() { this._syncFromOwner(); return this._rules.map(r => r.cssText).join('\n'); }
  _changed() { if (this.ownerNode) { this.ownerNode._settingSheet = true; const t = this._rules.map(r => r.cssText).join('\n'); this._ownerText = t; this.ownerNode.textContent = t; this.ownerNode._settingSheet = false; } this._doc && this._doc._sheetsChanged(); }
  get parentStyleSheet() { return null; } get ownerRule() { return null; }
}
export function splitRules(text) {
  const out = []; let depth = 0, start = 0, quote = 0;
  text = text.replace(/\/\*[\s\S]*?\*\//g, '');
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (quote) { if (c === quote) quote = 0; else if (c === '\\') i++; continue; }
    if (c === '"' || c === "'") quote = c;
    else if (c === '{') depth++;
    else if (c === '}') { depth--; if (depth === 0) { out.push(text.slice(start, i + 1).trim()); start = i + 1; } }
    else if (c === ';' && depth === 0) { const s = text.slice(start, i + 1).trim(); if (s) out.push(s); start = i + 1; }
  }
  return out.filter(Boolean);
}

export class Document extends Node {
  constructor(win, url) {
    super(null);
    this.ownerDocument = null; this.nodeType = 9; this.nodeName = '#document';
    this._window = win; this._page = null; this.URL = url || 'about:blank'; this.documentURI = this.URL;
    this._ids = new Map(); this.readyState = 'loading'; this.activeElement = null; this._doctype = null;
    this.cookie = ''; this.referrer = ''; this.characterSet = 'UTF-8'; this.charset = 'UTF-8'; this.inputEncoding = 'UTF-8'; this.compatMode = 'CSS1Compat'; this.contentType = 'text/html';
    this.designMode = 'off'; this.currentScript = null; this.adoptedStyleSheets = []; this._selectorCache = new Map();
    this.implementation = { createHTMLDocument: t => { const d = new Document(win, 'about:blank'); parseHTML(d, `<title>${t || ''}</title>`); return d; }, createDocument: () => new Document(win, 'about:blank'), hasFeature: () => true, createDocumentType: (n) => new DocumentType(this, n) };
    this.fonts = win ? win._fonts : null;
    this.timeline = { currentTime: 0 };
  }
  get defaultView() { return this._window; }
  get location() { return this._window.location; }
  get documentElement() { return this.childNodes.find(n => n.nodeType === ELEMENT_NODE) || null; }
  get head() { const h = this.documentElement; return h ? h.childNodes.find(n => n.localName === 'head') || null : null; }
  get body() { const h = this.documentElement; return h ? h.childNodes.find(n => n.localName === 'body' || n.localName === 'frameset') || null : null; }
  set body(b) { const old = this.body; if (old) old.replaceWith(b); else this.documentElement.appendChild(b); }
  get title() { const t = this.querySelector('title'); return t ? t.textContent.trim() : ''; }
  set title(v) { let t = this.querySelector('title'); if (!t) { t = this.createElement('title'); (this.head || this.documentElement).appendChild(t); } t.textContent = v; }
  get doctype() { return this._doctype; }
  _setDoctype(name) { this._doctype = new DocumentType(this, name); this.insertBefore(this._doctype, this.firstChild); }
  get scrollingElement() { return this.documentElement; }
  get hidden() { return false; } get visibilityState() { return 'visible'; }
  get fullscreenElement() { return null; } get pointerLockElement() { return null; } get pictureInPictureElement() { return null; } get fullscreenEnabled() { return false; }
  get styleSheets() { const l = [...this.querySelectorAll('style,link[rel~=stylesheet]')].map(e => e.sheet).filter(Boolean); l.item = i => l[i] || null; return l; }
  get forms() { return this.getElementsByTagName('form'); } get images() { return this.getElementsByTagName('img'); } get links() { return this.querySelectorAll('a[href],area[href]'); } get scripts() { return this.getElementsByTagName('script'); } get all() { return this.getElementsByTagName('*'); }
  get children() { return collection(this.childNodes.filter(n => n.nodeType === ELEMENT_NODE)); } get firstElementChild() { return this.documentElement; } get lastElementChild() { return this.documentElement; } get childElementCount() { return this.children.length; }
  hasFocus() { return this._window && this._window._focused !== false; }
  _makeElement(tag, ns) {
    if (ns === SVG_NS) return new SVGElement(this, tag, ns);
    const C = TAGS[tag] || (KNOWN.has(tag) || tag.includes('-') ? HTMLElement : HTMLUnknownElement);
    const el = new C(this, tag, HTML_NS);
    if (tag.includes('-') && this._window && this._window.customElements) this._window.customElements._upgrade(el);
    return el;
  }
  createElement(tag, opts) { const el = this._makeElement(String(tag).toLowerCase()); if (opts && opts.is) el.setAttribute('is', opts.is); return el; }
  createElementNS(ns, tag) { return this._makeElement(ns === SVG_NS ? tag : String(tag).toLowerCase(), ns === SVG_NS ? SVG_NS : HTML_NS); }
  createTextNode(t) { return new Text(this, t); }
  createComment(t) { return new Comment(this, t); }
  createDocumentFragment() { return new DocumentFragment(this); }
  createAttribute(n) { return { name: n, value: '', localName: n }; } createAttributeNS(ns, n) { return this.createAttribute(n); }
  createEvent(kind) { const W = this._window; const C = (W && W[kind]) || Event; const e = new C(''); e.initEvent = (t, b, c) => { e.type = t; e.bubbles = !!b; e.cancelable = !!c; }; e.initCustomEvent = (t, b, c, d) => { e.initEvent(t, b, c); e.detail = d; }; e.initMouseEvent = (t, b, c, v, d, sx, sy, cx, cy, ck, ak, sk, mk, btn) => { e.initEvent(t, b, c); Object.assign(e, { clientX: cx, clientY: cy, ctrlKey: ck, altKey: ak, shiftKey: sk, metaKey: mk, button: btn }); }; e.initKeyboardEvent = (t, b, c) => e.initEvent(t, b, c); e.initUIEvent = (t, b, c) => e.initEvent(t, b, c); return e; }
  createRange() { return this._window.Range ? new this._window.Range(this) : null; }
  createTreeWalker(root, what = 0xFFFFFFFF, filter) { return new TreeWalker(root, what, filter); }
  createNodeIterator(root, what = 0xFFFFFFFF, filter) { return new NodeIterator(root, what, filter); }
  getElementById(id) { const list = this._ids.get(String(id)); if (list) for (const e of list) if (e.isConnected) return e; return null; }
  getElementsByTagName(t) { return Element.prototype.getElementsByTagName.call(this, t); }
  getElementsByTagNameNS(ns, t) { return this.getElementsByTagName(t); }
  getElementsByClassName(c) { return Element.prototype.getElementsByClassName.call(this, c); }
  getElementsByName(n) { return this.querySelectorAll(`[name="${n}"]`); }
  querySelector(s) { return this._query(this, s, true); }
  querySelectorAll(s) { return this._query(this, s, false); }
  importNode(n, deep) { return n.cloneNode(deep); }
  adoptNode(n) { if (n.parentNode) n.parentNode.removeChild(n); return n; }
  elementFromPoint(x, y) { return this._page ? this._page.elementAt(x, y) : null; }
  elementsFromPoint(x, y) { const e = this.elementFromPoint(x, y); const out = []; for (let n = e; n; n = n.parentElement) out.push(n); return out; }
  caretRangeFromPoint() { return null; } caretPositionFromPoint() { return null; }
  getSelection() { return this._window.getSelection(); }
  execCommand(cmd, ui, value) { return this._page ? this._page.execCommand(cmd, value) : false; }
  queryCommandSupported(c) { return ['copy', 'cut', 'paste', 'insertText', 'selectAll', 'delete'].includes(c); } queryCommandEnabled(c) { return this.queryCommandSupported(c); } queryCommandState() { return false; } queryCommandValue() { return ''; }
  exitPointerLock() {} exitFullscreen() { return Promise.resolve(); } exitPictureInPicture() { return Promise.resolve(); }
  startViewTransition(cb) { const p = Promise.resolve().then(() => cb && cb()); return { ready: p, finished: p, updateCallbackDone: p, skipTransition() {} }; }
  getAnimations() { return []; }
  open() { return this; } close() {} write(s) { if (this.body) this.body.insertAdjacentHTML('beforeend', s); } writeln(s) { this.write(s + '\n'); }
  requestStorageAccess() { return Promise.resolve(); } hasStorageAccess() { return Promise.resolve(true); }
  get rootElement() { return this.documentElement; }
  // bookkeeping hooks
  _idChanged(el, old, now) {
    if (old) { const l = this._ids.get(old); if (l) { const i = l.indexOf(el); if (i >= 0) l.splice(i, 1); if (!l.length) this._ids.delete(old); } }
    if (now) { let l = this._ids.get(now); if (!l) { l = []; this._ids.set(now, l); } if (!l.includes(el)) l.push(el); }
  }
  _elementConnected(el) { const id = el.getAttribute('id'); if (id) this._idChanged(el, null, id); for (const c of el.childNodes) if (c.nodeType === ELEMENT_NODE && c.getAttribute('id')) this._idChanged(c, null, c.getAttribute('id')); if (el.localName.includes('-') && this._window.customElements) this._window.customElements._connected(el); }
  _elementDisconnected(el) { if (this.activeElement && el.contains(this.activeElement)) { this.activeElement = this.body; this._page && this._page.focusChanged(null); } if (el.localName.includes('-') && this._window.customElements) this._window.customElements._disconnected(el); }
  _mutated(node, kind, detail) { if (this._page) this._page.mutated(node, kind, detail); }
  _styleChanged(el) { if (this._page) this._page.styleChanged(el); }
  _sheetsChanged() { if (this._page) this._page.sheetsChanged(); }
  _linkAdded(link) { if (this._page) this._page.linkAdded(link); }
  _scriptAdded(s) { if (this._page) this._page.scriptAdded(s); }
  _parseFragment(html, ctx) { return parseHTML(this, html, ctx || this.body || this); }
  _selectors(s) {
    let sel = this._selectorCache.get(s);
    if (!sel) { sel = parseSelectorList(String(s), null); if (!sel.length && String(s).trim()) throw new DOMException(`'${s}' is not a valid selector`, 'SyntaxError'); this._selectorCache.set(s, sel); }
    return sel;
  }
  _matches(el, s) { const sels = this._selectors(s); return sels.some(x => matchesComplex(el, x, el)); }
  _query(root, s, first) {
    const sels = this._selectors(s);
    const out = [];
    const scope = root.nodeType === ELEMENT_NODE ? root : null;
    const walk = n => {
      for (const c of n.childNodes) {
        if (c.nodeType === ELEMENT_NODE) {
          for (const x of sels) if (matchesComplex(c, x, scope)) { out.push(c); if (first) return true; break; }
          if (walk(c)) return true;
        }
      }
      return false;
    };
    walk(root);
    if (first) return out[0] || null;
    out.item = i => out[i] || null;
    return out;
  }
}
const SHOW = { 1: 1, 3: 4, 8: 128 };
export class TreeWalker {
  constructor(root, what, filter) { this.root = root; this.whatToShow = what; this.filter = filter; this.currentNode = root; }
  _ok(n) { const bit = SHOW[n.nodeType] || 0; if (!(this.whatToShow & bit)) return false; if (!this.filter) return true; const r = typeof this.filter === 'function' ? this.filter(n) : this.filter.acceptNode(n); return r === 1 || r === undefined; }
  _reject(n) { if (!this.filter) return false; const r = typeof this.filter === 'function' ? this.filter(n) : this.filter.acceptNode(n); return r === 2; }
  nextNode() { let n = this.currentNode; for (;;) { n = nextInTree(n, this.root); if (!n) return null; if (this._ok(n)) { this.currentNode = n; return n; } } }
  previousNode() { let n = this.currentNode; for (;;) { n = prevInTree(n, this.root); if (!n) return null; if (this._ok(n)) { this.currentNode = n; return n; } } }
  firstChild() { for (const c of this.currentNode.childNodes) if (this._ok(c)) { this.currentNode = c; return c; } return null; }
  lastChild() { const k = this.currentNode.childNodes; for (let i = k.length - 1; i >= 0; i--) if (this._ok(k[i])) { this.currentNode = k[i]; return k[i]; } return null; }
  nextSibling() { for (let s = this.currentNode.nextSibling; s; s = s.nextSibling) if (this._ok(s)) { this.currentNode = s; return s; } return null; }
  previousSibling() { for (let s = this.currentNode.previousSibling; s; s = s.previousSibling) if (this._ok(s)) { this.currentNode = s; return s; } return null; }
  parentNode() { for (let p = this.currentNode.parentNode; p && p !== this.root.parentNode; p = p.parentNode) if (this._ok(p)) { this.currentNode = p; return p; } return null; }
}
export class NodeIterator extends TreeWalker { constructor(r, w, f) { super(r, w, f); this.referenceNode = r; this.pointerBeforeReferenceNode = true; this._first = true; } nextNode() { if (this._first) { this._first = false; if (this._ok(this.root)) return this.root; } return super.nextNode(); } detach() {} }
function nextInTree(n, root) { if (n.childNodes.length) return n.childNodes[0]; while (n && n !== root) { if (n.nextSibling) return n.nextSibling; n = n.parentNode; } return null; }
function prevInTree(n, root) { if (n === root) return null; if (n.previousSibling) { n = n.previousSibling; while (n.childNodes.length) n = n.childNodes[n.childNodes.length - 1]; return n; } return n.parentNode === root ? root : n.parentNode; }
export { Element, Text, Comment, DocumentFragment, Node, DOMException, parseHTML };
