// DOM events: EventTarget with capture, bubbling and the event classes
export class Event {
  constructor(type, init = {}) {
    this.type = type;
    this.bubbles = !!init.bubbles; this.cancelable = !!init.cancelable; this.composed = !!init.composed;
    this.defaultPrevented = false; this.target = null; this.currentTarget = null;
    this.eventPhase = 0; this.timeStamp = performance.now();
    this._stop = false; this._stopNow = false; this.isTrusted = false;
  }
  preventDefault() { if (this.cancelable) this.defaultPrevented = true; }
  stopPropagation() { this._stop = true; }
  stopImmediatePropagation() { this._stop = true; this._stopNow = true; }
  composedPath() { return this._path || []; }
  initEvent(type, bubbles, cancelable) { this.type = type; this.bubbles = !!bubbles; this.cancelable = !!cancelable; }
  get returnValue() { return !this.defaultPrevented; }
  set returnValue(v) { if (!v) this.preventDefault(); }
  get cancelBubble() { return this._stop; }
  set cancelBubble(v) { if (v) this._stop = true; }
  get srcElement() { return this.target; }
}
Event.NONE = 0; Event.CAPTURING_PHASE = 1; Event.AT_TARGET = 2; Event.BUBBLING_PHASE = 3;
export class CustomEvent extends Event { constructor(t, i = {}) { super(t, i); this.detail = i.detail === undefined ? null : i.detail; } initCustomEvent(t, b, c, d) { this.initEvent(t, b, c); this.detail = d; } }
export class UIEvent extends Event { constructor(t, i = {}) { super(t, i); this.view = i.view || null; this.detail = i.detail || 0; this.which = i.which || 0; } }
export class MouseEvent extends UIEvent {
  constructor(t, i = {}) {
    super(t, i);
    this.screenX = i.screenX || 0; this.screenY = i.screenY || 0;
    this.clientX = i.clientX || 0; this.clientY = i.clientY || 0;
    this.pageX = i.pageX ?? this.clientX; this.pageY = i.pageY ?? this.clientY;
    this.ctrlKey = !!i.ctrlKey; this.shiftKey = !!i.shiftKey; this.altKey = !!i.altKey; this.metaKey = !!i.metaKey;
    this.button = i.button || 0; this.buttons = i.buttons || 0;
    this.relatedTarget = i.relatedTarget || null;
    this.movementX = i.movementX || 0; this.movementY = i.movementY || 0;
  }
  get x() { return this.clientX; } get y() { return this.clientY; }
  get offsetX() { return this._offsetX ?? this.clientX; } get offsetY() { return this._offsetY ?? this.clientY; }
  get layerX() { return this.offsetX; } get layerY() { return this.offsetY; }
  getModifierState(k) { return k === 'Control' ? this.ctrlKey : k === 'Shift' ? this.shiftKey : k === 'Alt' ? this.altKey : k === 'Meta' ? this.metaKey : false; }
}
export class PointerEvent extends MouseEvent {
  constructor(t, i = {}) { super(t, i); this.pointerId = i.pointerId ?? 1; this.pointerType = i.pointerType || 'mouse'; this.isPrimary = i.isPrimary ?? true; this.width = i.width || 1; this.height = i.height || 1; this.pressure = i.pressure || 0; this.tiltX = 0; this.tiltY = 0; this.twist = 0; }
  getCoalescedEvents() { return [this]; } getPredictedEvents() { return []; }
}
export class WheelEvent extends MouseEvent { constructor(t, i = {}) { super(t, i); this.deltaX = i.deltaX || 0; this.deltaY = i.deltaY || 0; this.deltaZ = 0; this.deltaMode = i.deltaMode || 0; } }
export class DragEvent extends MouseEvent { constructor(t, i = {}) { super(t, i); this.dataTransfer = i.dataTransfer || null; } }
export class KeyboardEvent extends UIEvent {
  constructor(t, i = {}) {
    super(t, i);
    this.key = i.key || ''; this.code = i.code || ''; this.location = i.location || 0;
    this.ctrlKey = !!i.ctrlKey; this.shiftKey = !!i.shiftKey; this.altKey = !!i.altKey; this.metaKey = !!i.metaKey;
    this.repeat = !!i.repeat; this.isComposing = !!i.isComposing;
    this.keyCode = i.keyCode || 0; this.charCode = i.charCode || 0; this.which = i.which ?? this.keyCode;
  }
  getModifierState(k) { return k === 'Control' ? this.ctrlKey : k === 'Shift' ? this.shiftKey : k === 'Alt' ? this.altKey : k === 'Meta' ? this.metaKey : false; }
}
export class FocusEvent extends UIEvent { constructor(t, i = {}) { super(t, i); this.relatedTarget = i.relatedTarget || null; } }
export class InputEvent extends UIEvent { constructor(t, i = {}) { super(t, i); this.data = i.data ?? null; this.inputType = i.inputType || ''; this.isComposing = !!i.isComposing; this.dataTransfer = null; } getTargetRanges() { return []; } }
export class CompositionEvent extends UIEvent { constructor(t, i = {}) { super(t, i); this.data = i.data || ''; } }
export class ClipboardEvent extends Event { constructor(t, i = {}) { super(t, i); this.clipboardData = i.clipboardData || null; } }
export class MessageEvent extends Event { constructor(t, i = {}) { super(t, i); this.data = i.data; this.origin = i.origin || ''; this.lastEventId = ''; this.source = i.source || null; this.ports = i.ports || []; } }
export class ErrorEvent extends Event { constructor(t, i = {}) { super(t, i); this.message = i.message || ''; this.filename = i.filename || ''; this.lineno = i.lineno || 0; this.colno = i.colno || 0; this.error = i.error; } }
export class PromiseRejectionEvent extends Event { constructor(t, i = {}) { super(t, i); this.promise = i.promise; this.reason = i.reason; } }
export class ProgressEvent extends Event { constructor(t, i = {}) { super(t, i); this.lengthComputable = !!i.lengthComputable; this.loaded = i.loaded || 0; this.total = i.total || 0; } }
export class TransitionEvent extends Event { constructor(t, i = {}) { super(t, i); this.propertyName = i.propertyName || ''; this.elapsedTime = i.elapsedTime || 0; this.pseudoElement = ''; } }
export class AnimationEvent extends Event { constructor(t, i = {}) { super(t, i); this.animationName = i.animationName || ''; this.elapsedTime = i.elapsedTime || 0; this.pseudoElement = ''; } }
export class BeforeUnloadEvent extends Event { constructor() { super('beforeunload', { cancelable: true }); this.returnValue = ''; } }
export class HashChangeEvent extends Event { constructor(t, i = {}) { super(t, i); this.oldURL = i.oldURL || ''; this.newURL = i.newURL || ''; } }
export class PopStateEvent extends Event { constructor(t, i = {}) { super(t, i); this.state = i.state ?? null; } }
export class StorageEvent extends Event { constructor(t, i = {}) { super(t, i); Object.assign(this, { key: null, oldValue: null, newValue: null, url: '', storageArea: null }, i); } }

export class EventTarget {
  constructor() { this._listeners = null; }
  addEventListener(type, fn, opts) {
    if (!fn) return;
    const o = typeof opts === 'boolean' ? { capture: opts } : (opts || {});
    if (!this._listeners) this._listeners = new Map();
    let list = this._listeners.get(type);
    if (!list) { list = []; this._listeners.set(type, list); }
    if (list.some(l => l.fn === fn && !!l.capture === !!o.capture)) return;
    const entry = { fn, capture: !!o.capture, once: !!o.once, passive: !!o.passive };
    list.push(entry);
    if (o.signal) o.signal.addEventListener('abort', () => this.removeEventListener(type, fn, o), { once: true });
  }
  removeEventListener(type, fn, opts) {
    const cap = typeof opts === 'boolean' ? opts : !!(opts && opts.capture);
    const list = this._listeners && this._listeners.get(type);
    if (!list) return;
    const i = list.findIndex(l => l.fn === fn && l.capture === cap);
    if (i >= 0) list.splice(i, 1);
  }
  _invoke(ev, phase) {
    const list = this._listeners && this._listeners.get(ev.type);
    const handler = this['on' + ev.type];
    if (!list && typeof handler !== 'function') return;
    ev.currentTarget = this;
    ev.eventPhase = phase;
    const snapshot = list ? list.slice() : [];
    for (const l of snapshot) {
      if (phase === Event.CAPTURING_PHASE && !l.capture) continue;
      if (phase === Event.BUBBLING_PHASE && l.capture) continue;
      if (l.once) this.removeEventListener(ev.type, l.fn, l.capture);
      try {
        if (typeof l.fn === 'function') l.fn.call(this, ev); else if (l.fn && l.fn.handleEvent) l.fn.handleEvent(ev);
      } catch (e) { reportError(e); }
      if (ev._stopNow) return;
    }
    if (phase !== Event.CAPTURING_PHASE && typeof handler === 'function') {
      try { const r = handler.call(this, ev); if (r === false) ev.preventDefault(); } catch (e) { reportError(e); }
    }
  }
  dispatchEvent(ev) {
    ev.target = this;
    const path = [];
    for (let n = this; n; n = n._eventParent()) path.push(n);
    ev._path = path;
    ev._stop = false; ev._stopNow = false;
    for (let i = path.length - 1; i > 0 && !ev._stop; i--) path[i]._invoke(ev, Event.CAPTURING_PHASE);
    if (!ev._stop) this._invoke(ev, Event.AT_TARGET);
    if (ev.bubbles) for (let i = 1; i < path.length && !ev._stop; i++) path[i]._invoke(ev, Event.BUBBLING_PHASE);
    ev.currentTarget = null; ev.eventPhase = 0;
    return !ev.defaultPrevented;
  }
  _eventParent() { return null; }
}
export let reportError = e => { globalThis.__quarkReport && globalThis.__quarkReport(e); };
export function setReporter(f) { reportError = f; }
export class AbortSignalLike {}
