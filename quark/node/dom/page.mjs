// a page: its document, scripts, styles, layout and painting, and the
// Electron renderer surface its preload sees
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import vm from 'node:vm';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { Worker as NodeWorker } from 'node:worker_threads';
import * as E from './events.mjs';
import { Document, parseHTML, CSSStyleSheet } from './elements.mjs';
import { ELEMENT_NODE, TEXT_NODE } from './node.mjs';
import { StyleEngine, ComputedStyle, UA_SHEET } from './style.mjs';
import { parseStylesheet, evalMedia, mediaEnv, splitSpaces, splitCommas, parseColor, camelToDash } from './css.mjs';
import { Fonts } from './fonts.mjs';
import { TextShaper } from './text.mjs';
import { Layout } from './layout.mjs';
import { Painter } from './paint.mjs';
import { decodePNG } from './png.mjs';
import { CanvasRenderingContext2D } from './canvas2d.mjs';
import { installGlobals, Location, History, Storage, Selection, FontFaceSet, MediaQueryList, CustomElementRegistry, DOMRect } from './window.mjs';
import { keyInfo } from './keys.mjs';
const here = path.dirname(fileURLToPath(import.meta.url));
const UA = 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.0.0 Electron/43.7.7 Safari/537.36 Quark/1.0';

export class Page {
  constructor(o) {
    this.gfx = o.gfx; this.say = o.say; this.width = o.width; this.height = o.height; this.scale = o.scale || 1; this.zoom = o.zoom || 1;
    this.bg = parseColor(o.bg || '#ffffff') || [255, 255, 255, 1];
    this.fonts = new Fonts(this.gfx, t => this.log(t));
    this.text = new TextShaper(this.fonts);
    this.engine = new StyleEngine({ vw: 0, vh: 0, dpr: this.scale, rem: 16 });
    this.layout = new Layout(this); this.painter = new Painter(this);
    this.styleGen = 1; this.layoutDirty = true; this.needsPaint = true; this.rootBox = null;
    this.layoutAll = true; this.layoutRoots = new Map(); // el -> { tree, own, self }
    if (process.env.QUARK_PROF) this.prof = { t0: performance.now(), ticks: 0, layouts: 0, partial: 0, partialMs: 0, partialList: [], styleMs: 0, cands: 0, sheetMarks: 0, deep: 0, hits: 0, layoutMs: 0, paints: 0, paintMs: 0, styles: 0, boxes: 0, clips: 0, clipMs: 0, clipSlow: 0 };
    this.rafs = []; this.rafId = 1; this.observers = new Set(); this.resizeObservers = new Set(); this.intersectionObservers = new Set();
    this.invokes = new Map(); this.nextInvoke = 1; this.fetches = new Map(); this.nextFetch = 1; this.ports = new Map();
    this.images = new Map(); this.sheets = new Map(); this.schemeRoots = new Set(); this.hover = null; this.active = null; this.focused = null; this.caretBox = null; this.caretHidden = false;
    this.ipcListeners = new Map(); this.loaded = false; this.pointer = { x: 0, y: 0 }; this.lastClick = 0; this.clickCount = 0;
    this.storageDir = path.join(process.env.XDG_CACHE_HOME || path.join(os.homedir(), '.cache'), 'quark', 'storage');
    this.updateEnv();
    E.setReporter(e => this.reportError(e));
  }
  log(t) { this.say({ op: 'log', text: t }); }
  updateEnv() {
    const cssW = this.width / this.scale / this.zoom, cssH = this.height / this.scale / this.zoom;
    this.cssW = cssW; this.cssH = cssH;
    this.engine.env.vw = cssW; this.engine.env.vh = cssH; this.engine.env.dpr = this.scale * this.zoom;
    mediaEnv.width = cssW; mediaEnv.height = cssH; mediaEnv.dpr = this.scale * this.zoom;
    this.painter.scale = this.scale * this.zoom;
  }
  // ---- loading
  async load(url, preload, args) {
    this.url = url; this.preloadPath = preload; this.args = args || [];
    this.location = new Location(this, url); this.history = new History();
    this.document = new Document(globalThis, url); this.document._page = this;
    this.navigator = this.makeNavigator();
    this.screen = { width: this.cssW, height: this.cssH, availWidth: this.cssW, availHeight: this.cssH, availLeft: 0, availTop: 0, colorDepth: 24, pixelDepth: 24, orientation: { type: 'landscape-primary', angle: 0, addEventListener() {}, removeEventListener() {} }, isExtended: false };
    this.localStorage = new Storage(this, 'local'); this.sessionStorage = new Storage(null, 'session');
    this.selection = new Selection(this.document);
    this.customElements = new CustomElementRegistry();
    this.visualViewport = Object.assign(new E.EventTarget(), { width: this.cssW, height: this.cssH, scale: 1, offsetLeft: 0, offsetTop: 0, pageLeft: 0, pageTop: 0, onresize: null, onscroll: null });
    this.fontFaceSet = new FontFaceSet(this); this.document.fonts = this.fontFaceSet;
    this.mediaLists = [];
    this.WorkerClass = this.makeWorkerClass();
    installGlobals(this);
    this.installConsole();
    process.on('unhandledRejection', e => { const ev = new E.PromiseRejectionEvent('unhandledrejection', { reason: e, cancelable: true }); globalThis.dispatchEvent(ev); if (!ev.defaultPrevented) this.log(`unhandled rejection: ${e && e.stack || e}`); });
    process.on('uncaughtException', e => this.reportError(e));
    if (preload) await this.runPreload(preload);
    let html = '';
    try { html = await this.fetchText(url); } catch (e) { this.log(`load ${url}: ${e.message}`); html = `<html><body><p>${e.message}</p></body></html>`; }
    this.parsing = true;
    parseHTML(this.document, html);
    this.parsing = false;
    this.document.readyState = 'interactive';
    await this.runPendingScripts();
    await this.waitForSheets();
    this.document.dispatchEvent(new E.Event('DOMContentLoaded', { bubbles: true }));
    this.document.readyState = 'complete';
    this.document.dispatchEvent(new E.Event('readystatechange'));
    globalThis.dispatchEvent(new E.Event('load'));
    this.loaded = true;
    this.say({ op: 'loaded' });
    this.say({ op: 'title', title: this.document.title });
    this.titleWatch = this.document.title;
    this.needPaint();
  }
  parseInto(doc, s) { const was = this.parsing; this.parsing = true; parseHTML(doc, s); this.parsing = was; return doc; }
  // the preload runs first, with Electron's renderer modules
  async runPreload(file) {
    let src;
    try { src = fs.readFileSync(file, 'utf8'); } catch (e) { this.log(`preload ${file}: ${e.message}`); return; }
    const electron = this.rendererElectron();
    const proc = this.sandboxProcess();
    const req = name => { if (name === 'electron' || name === 'electron/renderer') return electron; throw new Error(`Cannot find module '${name}'`); };
    const mod = { exports: {} };
    try {
      const fn = vm.runInThisContext(`(function (require, process, module, exports, __filename, __dirname) {${src}\n})`, { filename: file });
      fn(req, proc, mod, mod.exports, file, path.dirname(file));
    } catch (e) { this.reportError(e); }
  }
  sandboxProcess() {
    const page = this;
    return { platform: 'linux', arch: process.arch, argv: ['renderer', ...this.args], env: { ...process.env }, versions: { ...process.versions, electron: '43.7.7', chrome: '136.0.0.0' }, type: 'renderer', pid: process.pid, sandboxed: true, contextIsolated: true, execPath: process.execPath, resourcesPath: process.resourcesPath, mas: false, windowsStore: false, isMainFrame: true, defaultApp: false, noAsar: true,
      cwd: () => process.cwd(), uptime: () => process.uptime(), hrtime: process.hrtime, nextTick: process.nextTick, memoryUsage: () => process.memoryUsage(), getProcessMemoryInfo: () => Promise.resolve({ residentSet: 0, private: 0, shared: 0 }), getSystemMemoryInfo: () => ({ total: os.totalmem() / 1024, free: os.freemem() / 1024 }), getCPUUsage: () => ({ percentCPUUsage: 0, idleWakeupsPerSecond: 0 }), getSystemVersion: () => os.release(), getCreationTime: () => Date.now(), crash() {}, hang() {},
      on: (t, f) => { if (t === 'loaded') setTimeout(f, 0); return this; }, once() { return this; }, off() { return this; }, removeListener() { return this; }, emit() { return false; }, listenerCount: () => 0 };
  }
  rendererElectron() {
    const page = this;
    const listeners = this.ipcListeners;
    const on = (ch, fn, once) => { let l = listeners.get(ch); if (!l) { l = []; listeners.set(ch, l); } l.push({ fn, once }); return ipcRenderer; };
    const off = (ch, fn) => { const l = listeners.get(ch); if (l) { const i = l.findIndex(x => x.fn === fn); if (i >= 0) l.splice(i, 1); } return ipcRenderer; };
    const ipcRenderer = {
      on: (c, f) => on(c, f, false), once: (c, f) => on(c, f, true), off, removeListener: off, removeAllListeners: c => { if (c) listeners.delete(c); else listeners.clear(); return ipcRenderer; }, addListener: (c, f) => on(c, f, false), listenerCount: c => (listeners.get(c) || []).length, eventNames: () => [...listeners.keys()], setMaxListeners() { return ipcRenderer; }, getMaxListeners: () => 0, prependListener: (c, f) => on(c, f, false),
      send: (channel, ...args) => page.say({ op: 'ipc', channel, args: page.outbound(args), ports: [] }),
      sendToHost: (channel, ...args) => page.say({ op: 'ipc', channel, args: page.outbound(args), ports: [] }),
      invoke: (channel, ...args) => new Promise((resolve, reject) => { const id = page.nextInvoke++; page.invokes.set(id, { resolve, reject }); page.say({ op: 'invoke', id, channel, args: page.outbound(args) }); }),
      sendSync: channel => { throw new Error(`sendSync is not available in quark (${channel})`); },
      postMessage: (channel, message, transfer) => { const ids = (transfer || []).map(p => page.portId(p)); page.say({ op: 'ipc', channel, args: page.outbound([message]), ports: ids }); },
    };
    const webFrame = { setZoomLevel: l => page.setZoomLevel(l), getZoomLevel: () => Math.log(page.zoom) / Math.log(1.2), setZoomFactor: f => page.setZoom(f), getZoomFactor: () => page.zoom, setVisualZoomLevelLimits() {}, setLayoutZoomLevelLimits() {}, insertCSS: css => { const s = page.document.createElement('style'); s.textContent = css; page.document.head.appendChild(s); return 'k' + s._uid; }, removeInsertedCSS() {}, executeJavaScript: code => Promise.resolve(vm.runInThisContext(code)), executeJavaScriptInIsolatedWorld: (w, s) => Promise.resolve(s.map(x => vm.runInThisContext(x.code)).pop()), setSpellCheckProvider() {}, getResourceUsage: () => ({ images: {}, scripts: {}, cssStyleSheets: {}, xslStyleSheets: {}, fonts: {}, other: {} }), clearCache() {}, routingId: 1, top: null, parent: null, opener: null, frameToken: '1', firstChild: null, nextSibling: null, isWordMisspelled: () => false, getWordSuggestions: () => [], setIsolatedWorldInfo() {}, findFrameByName: () => null };
    const contextBridge = { exposeInMainWorld: (name, obj) => { globalThis[name] = obj; }, exposeInIsolatedWorld: (w, name, obj) => { globalThis[name] = obj; }, executeInMainWorld: ({ func, args }) => func(...(args || [])) };
    const webUtils = { getPathForFile: f => (f && f.path) || '' };
    return { ipcRenderer, webFrame, contextBridge, webUtils, crashReporter: { start() {}, addExtraParameter() {}, removeExtraParameter() {}, getParameters: () => ({}) }, nativeImage: { createEmpty: () => ({ isEmpty: () => true }) }, clipboard: { readText: () => '', writeText: t => page.say({ op: 'clipboard', text: t }) } };
  }
  // args to the main side: Uint8Arrays go as they are; the rest as cloneable
  outbound(args) { return args.map(a => a === undefined ? null : a); }
  portId(p) { if (p && p._quarkId) return p._quarkId; const id = -(this.ports.size + 1); return id; }
  // ---- fetching: file, data, http and the app's own schemes
  async fetchBytes(url) {
    const u = new URL(url, this.url || 'file:///');
    if (u.protocol === 'file:') return fs.promises.readFile(fileURLToPath(u));
    if (u.protocol === 'data:') { const r = await fetch(u.href); return Buffer.from(await r.arrayBuffer()); }
    if (u.protocol === 'http:' || u.protocol === 'https:' || u.protocol === 'blob:') { const r = await fetch(u.href); if (!r.ok) throw new Error(`${r.status} ${u.href}`); return Buffer.from(await r.arrayBuffer()); }
    // a scheme the app handles in its main process
    return new Promise((resolve, reject) => { const id = this.nextFetch++; this.fetches.set(id, { resolve, reject }); this.say({ op: 'fetch', id, url: u.href, method: 'GET', headers: {} }); });
  }
  fetchReply(id, status, headers, body) {
    const f = this.fetches.get(id); if (!f) return; this.fetches.delete(id);
    if (!status) f.reject(new Error(`no answer for request`)); else if (status >= 400) f.reject(Object.assign(new Error(`${status}`), { status, headers, body })); else f.resolve({ body: Buffer.from(body.buffer, body.byteOffset, body.byteLength), headers, status });
  }
  async fetchText(url) { const r = await this.fetchBytes(url); return (r.body || r).toString('utf8'); }
  async fetchBuffer(url) { const r = await this.fetchBytes(url); return r.body || r; }
  // a module url node can import: the app's scheme maps onto the disk
  moduleURL(url) {
    const u = new URL(url, this.url);
    if (u.protocol === 'file:' || u.protocol === 'data:' || u.protocol === 'node:') return u.href;
    if (fs.existsSync(u.pathname)) return pathToFileURL(u.pathname).href;
    return u.href;
  }
  // ---- scripts
  // the parser's scripts run in order after the parse; later ones at once
  scriptAdded(s) { if (this.parsing) return; this.runScript(s); }
  async runPendingScripts() {
    // scripts in document order, each after the last
    const list = [...this.document.querySelectorAll('script')].filter(s => !s._ran);
    for (const s of list) await this.runScript(s);
  }
  async runScript(s) {
    if (s._ran) return; s._ran = true;
    const type = (s.getAttribute('type') || '').trim().toLowerCase();
    if (type && type !== 'text/javascript' && type !== 'module' && type !== 'application/javascript' && type !== 'text/jsx' && !type.includes('javascript')) return;
    if (s.hasAttribute('nomodule')) return;
    const src = s.getAttribute('src');
    try {
      this.document.currentScript = s;
      if (type === 'module') {
        if (src) { const url = this.moduleURL(new URL(src, this.url).href); await import(url); }
        else { const text = s.textContent; const url = 'data:text/javascript;base64,' + Buffer.from(text).toString('base64'); await import(url); }
      } else {
        let code = s.textContent, name = this.url + '#inline';
        if (src) { const abs = new URL(src, this.url).href; code = await this.fetchText(abs); name = abs; }
        vm.runInThisContext(code, { filename: name });
      }
      s.dispatchEvent(new E.Event('load'));
    } catch (e) { this.reportError(e); s.dispatchEvent(new E.Event('error')); }
    finally { this.document.currentScript = null; }
  }
  reportError(e) {
    const msg = e && e.stack || String(e);
    this.say({ op: 'console', level: 'error', text: msg });
    const ev = new E.ErrorEvent('error', { message: String(e && e.message || e), error: e, cancelable: true });
    try { globalThis.dispatchEvent(ev); } catch {}
  }
  installConsole() {
    const page = this, orig = console;
    const fmt = a => a.map(x => typeof x === 'string' ? x : (x instanceof Error ? x.stack : safeStr(x))).join(' ');
    const make = level => (...a) => { page.say({ op: 'console', level, text: fmt(a) }); };
    globalThis.console = { log: make('log'), info: make('info'), warn: make('warning'), error: make('error'), debug: make('debug'), trace: make('log'), dir: make('log'), table: make('log'), group: make('log'), groupCollapsed: make('log'), groupEnd() {}, time() {}, timeEnd() {}, timeLog() {}, count() {}, countReset() {}, assert(c, ...a) { if (!c) make('error')('Assertion failed:', ...a); }, clear() {}, profile() {}, profileEnd() {}, timeStamp() {} };
  }
  // ---- styles
  sheetsChanged() { this.sheetsDirty = true; this.layoutDirty = true; this.needPaint(); }
  linkAdded(link) {
    if (!/\bstylesheet\b/i.test(link.getAttribute('rel') || '')) return;
    const href = link.getAttribute('href'); if (!href) return;
    const url = new URL(href, this.url).href;
    if (link._loadedHref === url) return;
    link._loadedHref = url;
    const p = this.fetchText(url).then(text => { link._cssText = text; link._sheet = new CSSStyleSheet(this.document, null); link._sheet.href = url; link._sheet.replaceSync(text); link._sheet._doc = null; this.sheetsChanged(); link.dispatchEvent(new E.Event('load')); }, e => { this.log(`stylesheet ${url}: ${e.message}`); link.dispatchEvent(new E.Event('error')); });
    (this.sheetLoads = this.sheetLoads || []).push(p);
  }
  async waitForSheets() { while (this.sheetLoads && this.sheetLoads.length) { const l = this.sheetLoads; this.sheetLoads = []; await Promise.all(l); } }
  collectSheets() {
    const list = [];
    let order = 0;
    // url()s in a sheet are relative to the sheet: made absolute first
    const add = (text, owner, media, base) => {
      if (!text) return;
      const key = base ? base + '\u0000' + text : text;
      let s = this.sheets.get(key);
      if (!s) { s = parseStylesheet(base ? absolutizeUrls(text, base) : text, media ? { matches: evalMedia(media) } : null); this.sheets.set(key, s); }
      list.push(s);
      for (const ff of s.fontFaces) this.loadFontFaceDecl(ff, owner);
      for (const im of s.imports) this.importSheet(im, list, base);
    };
    for (const el of this.document.querySelectorAll('style,link')) {
      if (el._disabled) continue;
      const media = el.getAttribute('media');
      if (media && !evalMedia(media)) continue;
      if (el.localName === 'style') add(el.textContent, el, null, null);
      else if (el._cssText) add(el._cssText, el, null, el._loadedHref);
    }
    for (const sheet of this.document.adoptedStyleSheets || []) add(sheet.text, null, null, null);
    const before = this.engine.sheets;
    this.engine.setSheets(list);
    this.sheetsDirty = false;
    this.sheetsDiff(before, list);
  }
  // only the elements the sheets that came or went can style are
  // restyled; everything when a sheet has an unkeyed rule or the
  // marks run past a limit
  sheetsDiff(before, list) {
    if (!before || !this.rootBox) { this.invalidateStyle(); return; }
    const changed = [];
    for (const s of list) if (!before.includes(s)) changed.push(s);
    for (const s of before) if (!list.includes(s)) changed.push(s);
    if (!changed.length) return;
    const keys = this.engine.sheetKeys(changed);
    if (keys.any) { this.invalidateStyle(); return; }
    let n = 0;
    const walk = el => { if (this.engine.keysTouch(el, keys)) { el._styleDirty = true; el._pseudoStyles = null; this.dirtyLayout(el, 'own'); n++; } for (let c = el.firstElementChild; c; c = c.nextElementSibling) walk(c); };
    walk(this.document.documentElement);
    if (n > 400) { this.invalidateStyle(); return; }
    if (this.prof) this.prof.sheetMarks += n;
  }
  importSheet(im, list, base) {
    const url = new URL(im.url, base || this.url).href;
    if (this.importedCss && this.importedCss.has(url)) { const s = this.sheets.get(this.importedCss.get(url)); if (s) list.push(s); return; }
    this.importedCss = this.importedCss || new Map();
    this.importedCss.set(url, '');
    this.fetchText(url).then(t => { this.importedCss.set(url, t); this.sheetsChanged(); }).catch(e => this.log(`@import ${url}: ${e.message}`));
  }
  loadFontFaceDecl(d, owner) {
    const family = (d.get('font-family') || '').replace(/^["']|["']$/g, '');
    const src = d.get('src') || '';
    if (!family || !src) return;
    const key = family + '|' + src;
    this.fontLoads = this.fontLoads || new Set();
    if (this.fontLoads.has(key)) return;
    this.fontLoads.add(key);
    const urls = [...src.matchAll(/url\(\s*["']?([^"')]+)["']?\s*\)/g)].map(m => m[1]);
    const u = urls.find(x => /\.(ttf|otf|woff2?)(\?|#|$)/i.test(x)) || urls[0];
    if (!u) return;
    const weight = parseWeight(d.get('font-weight') || '400'), italic = /italic|oblique/.test(d.get('font-style') || '');
    this.fetchBuffer(new URL(u, this.url).href).then(bytes => {
      if (/\.woff2?(\?|#|$)/i.test(u)) { this.log(`font ${family}: woff is not read yet (${u})`); return; }
      if (this.fonts.addWebFont(family, weight, italic, bytes)) { this.text.cache.clear(); this.invalidateStyle(); this.needPaint(); }
    }).catch(e => this.log(`font ${family}: ${e.message}`));
  }
  loadFontFace(ff) {
    const src = typeof ff.source === 'string' ? ff.source : null;
    const run = async () => {
      let bytes;
      if (src) { const m = src.match(/url\(\s*["']?([^"')]+)["']?\s*\)/); if (!m) throw new Error('no url'); bytes = await this.fetchBuffer(new URL(m[1], this.url).href); }
      else bytes = Buffer.from(ff.source instanceof ArrayBuffer ? new Uint8Array(ff.source) : ff.source);
      if (!this.fonts.addWebFont(ff.family.replace(/^["']|["']$/g, ''), parseWeight(ff.weight), /italic/.test(ff.style), bytes)) throw new Error('font not read');
      ff.status = 'loaded'; this.text.cache.clear(); this.invalidateStyle(); this.needPaint(); ff._res(ff);
      return ff;
    };
    ff.status = 'loading';
    return run().catch(e => { ff.status = 'error'; ff._rej(e); throw e; });
  }
  invalidateStyle() { this.styleGen++; this.dirtyLayoutAll(); this.needPaint(); }
  // layout marks: an element whose box tree under it must be made
  // again (self: its own box may stay), or everything
  dirtyLayout(el, kind) { this.layoutDirty = true; if (el && el.nodeType !== 1) el = el.parentElement; if (!el) { this.layoutAll = true; return; } const r = this.layoutRoots.get(el) || { tree: false, own: false, self: false }; r[kind] = true; this.layoutRoots.set(el, r); }
  dirtyLayoutAll() { this.layoutDirty = true; this.layoutAll = true; }
  styleOf(el) {
    if (el._computed && !el._styleDirty && !el._styleKeys && el._styleGen === this.styleGen) return el._computed;
    if (this.sheetsDirty || !this.engine.index) this.collectSheets();
    const parent = el.parentElement ? this.styleOf(el.parentElement) : (el.parentNode && el.parentNode.host ? this.styleOf(el.parentNode.host) : null);
    // marked only by keys changed above it: no rule of its own names
    // one, and its parent stands, so it stands too
    if (el._styleKeys && !el._styleDirty) {
      const keys = el._styleKeys; el._styleKeys = null;
      if (el._computed && el._styleGen === this.styleGen && el._computed.parent === parent && !this.engine.rulesNameAbove(el, keys)) return el._computed;
    }
    el._styleKeys = null;
    const old = el._styleGen === this.styleGen ? el._computed : null;
    const t0 = this.prof ? performance.now() : 0;
    const decl = this.engine.declared(el, null);
    if (this.prof) { this.prof.styles++; this.prof.styleMs += performance.now() - t0; this.prof.cands += this.engine.lastCands; this.prof.hits += this.engine.lastHits; }
    el._styleDirty = false; el._styleGen = this.styleGen;
    // the same declarations under the same parent: the subtree stands
    if (old && old.parent === parent && sameDecl(old.decl, decl)) return old;
    const cs = new ComputedStyle(this.engine, decl, parent, el);
    el._computed = cs; el._pseudoStyles = null; el._styleChanged = true;
    for (let c = el.firstElementChild; c; c = c.nextElementSibling) c._styleDirty = true;
    return cs;
  }
  pseudoStyleOf(el, which) {
    if (el._pseudoStyles && el._pseudoStyles[which] !== undefined && el._styleGen === this.styleGen) return el._pseudoStyles[which];
    const own = this.styleOf(el);
    const cs = this.engine.compute(el, null, which, own);
    const content = cs.raw('content');
    const r = content && content !== 'none' && content !== 'normal' ? cs : null;
    el._pseudoStyles = el._pseudoStyles || {}; el._pseudoStyles[which] = r;
    return r;
  }
  anonymousStyle(parentCs) { return new ComputedStyle(this.engine, new Map([['display', 'block']]), parentCs, null); }
  getComputedStyle(el, pseudo) {
    const page = this;
    const cs = pseudo ? (this.pseudoStyleOf(el, pseudo.replace(/^:+/, '')) || this.styleOf(el)) : this.styleOf(el);
    const resolved = k => {
      // layout-dependent values come from the box
      if (el._box && (k === 'width' || k === 'height')) { page.ensureLayout(); const b = el._box; if (b) return (k === 'width' ? b.cw() : b.ch()) + 'px'; }
      return cs.get(k);
    };
    return new Proxy({}, { get(_, k) { if (k === 'getPropertyValue') return n => resolved(String(n)); if (k === 'getPropertyPriority') return () => ''; if (k === 'length') return 0; if (k === 'item') return () => ''; if (k === 'cssText') return ''; if (k === 'setProperty' || k === 'removeProperty') return () => {}; if (typeof k !== 'string') return undefined; return resolved(k.startsWith('--') ? k : camelToDash(k === 'cssFloat' ? 'float' : k)); } });
  }
  matchMedia(q) { const m = new MediaQueryList(q, evalMedia(q)); this.mediaLists.push(m); return m; }
  setZoom(f) { this.zoom = Math.max(0.25, Math.min(5, f)); this.resize(this.width, this.height, this.scale, true); }
  setZoomLevel(l) { this.setZoom(Math.pow(1.2, l)); }
  setBackground(c) { this.bg = parseColor(c) || this.bg; this.needPaint(); }
  // ---- mutations
  mutated(node, kind, detail) {
    this.engine.mutated();
    if (process.env.QUARK_TRACE_DIRTY) { this.dirtyCounts = this.dirtyCounts || {}; const k = kind + (detail && detail.attributeName ? ':' + detail.attributeName : '') + ' ' + (node.localName || node.nodeName) + (node.className && typeof node.className === 'string' ? '.' + node.className.split(' ')[0] : ''); this.dirtyCounts[k] = (this.dirtyCounts[k] || 0) + 1; if (!this.dirtyTimer) this.dirtyTimer = setTimeout(() => { this.dirtyTimer = null; const top = Object.entries(this.dirtyCounts).sort((a, b) => b[1] - a[1]).slice(0, 8).map(([k, v]) => `${v} ${k}`).join(' | '); this.dirtyCounts = {}; this.log(`dirty/s: ${top}`); }, 1000); }
    // a style element's text is a stylesheet
    const styleEl = node.localName === 'style' ? node : (node.parentNode && node.parentNode.localName === 'style' ? node.parentNode : null);
    if (styleEl && kind !== 'attributes' && styleEl.isConnected) this.sheetsChanged();
    if (kind === 'characterData') { this.dirtyLayout(node.parentNode, 'self'); if (node._box && node._box.text !== undefined) node._box.text = node.data; }
    else if (kind === 'childList') this.dirtyChildren(node, detail);
    else if (kind === 'attributes') this.dirtyStyle(node, true, attrKeys(node, detail));
    else this.dirtyStyle(node, true, [':checked', ':placeholder-shown']);
    for (const o of this.observers) if (o._consider(node, kind, detail)) this.scheduleObservers();
    this.needPaint();
    if (this.document && this.titleWatch !== undefined && node.localName === 'title' || (node.parentNode && node.parentNode.localName === 'title')) { const t = this.document.title; if (t !== this.titleWatch) { this.titleWatch = t; this.say({ op: 'title', title: t }); } }
  }
  styleChanged(el) { this.dirtyStyle(el, false, []); this.needPaint(); }
  // el and its subtree restyle; its later siblings too (+ ~ :nth);
  // ancestors a :has rule could read; the rest keep their styles
  dirtyStyle(el, siblings, keys) {
    this.layoutDirty = true;
    if (!el || el.nodeType !== 1) { this.layoutAll = true; return; }
    let n = 1, sib = 0; el._styleDirty = true; el._pseudoStyles = null;
    if (this.engine.reachesBelow(keys)) {
      this.dirtyLayout(el, 'tree');
      n = dirtyTree(el, keys);
      if (siblings) for (let s = el.nextElementSibling; s; s = s.nextElementSibling) { sib += dirtyTree(s, keys); this.dirtyLayout(s, 'tree'); }
      n += sib;
    } else this.dirtyLayout(el, 'own');
    let has = 0;
    for (let a = el.parentElement; a; a = a.parentElement) if (this.engine.hasSensitive(a)) { a._styleDirty = true; a._pseudoStyles = null; has++; this.dirtyLayout(a, 'own'); }
    n += has;
    this.traceDirty('style', el, n);
  }
  traceDirty(kind, el, n) {
    if (!process.env.QUARK_TRACE_DIRTY) return;
    this.dirtyMarks = this.dirtyMarks || {};
    const k = `${kind} ${el.localName}${el.className && typeof el.className === 'string' ? '.' + el.className.split(' ')[0] : ''}`;
    const e = this.dirtyMarks[k] || (this.dirtyMarks[k] = { calls: 0, marks: 0 }); e.calls++; e.marks += n;
    if (!this.marksTimer) this.marksTimer = setTimeout(() => { this.marksTimer = null; const top = Object.entries(this.dirtyMarks).sort((a, b) => b[1].marks - a[1].marks).slice(0, 8).map(([k, v]) => `${v.marks}/${v.calls} ${k}`).join(' | '); this.dirtyMarks = {}; this.log(`marks/s: ${top}`); }, 1000);
  }
  // children came or went under p: the new ones whole, every child
  // for its place (:nth-child, + ~), p and above for :has
  dirtyChildren(p, detail) {
    this.layoutDirty = true;
    if (!p || p.nodeType !== 1) { this.layoutAll = true; return; }
    this.dirtyLayout(p, 'self');
    let n = 0;
    if (detail && detail.added) for (const k of detail.added) if (k.nodeType === 1) n += dirtyTree(k);
    for (let c = p.firstElementChild; c; c = c.nextElementSibling) { c._styleDirty = true; c._pseudoStyles = null; n++; }
    for (let a = p; a; a = a.parentElement) if (this.engine.hasSensitive(a)) { a._styleDirty = true; a._pseudoStyles = null; n++; this.dirtyLayout(a, 'own'); }
    this.traceDirty('children', p, n);
  }
  // the chains (element to root) whose pseudo state changed: the
  // topmost changed element of each is where the restyle starts
  dirtyChains(a, b, keys) {
    let top = null;
    for (const n of a) if (!b.includes(n)) top = n;
    for (const n of b) if (!a.includes(n)) { if (!top || n.contains(top)) top = n; }
    if (!top) return;
    const seen = new Set([...a, ...b]);
    // every element whose state flipped restyles; the topmost's keys
    // decide whether the trees under them do
    for (const n of seen) if (a.includes(n) !== b.includes(n)) { n._styleDirty = true; n._pseudoStyles = null; this.dirtyLayout(n, 'own'); }
    this.dirtyStyle(top, true, keys);
  }
  scrolled(el) { this.needPaint(); const ev = new E.Event('scroll', { bubbles: false }); setTimeout(() => el.dispatchEvent(ev), 0); }
  scheduleObservers() { if (this.obsScheduled) return; this.obsScheduled = true; queueMicrotask(() => { this.obsScheduled = false; for (const o of this.observers) o._flush(); }); }
  needPaint() { this.needsPaint = true; }
  // ---- images
  loadImage(img) {
    const src = img.getAttribute('src'); if (!src) return;
    const url = new URL(src, this.url).href;
    this.imageFor(url, img, true).then(r => { if (!r) { img.dispatchEvent(new E.Event('error')); return; } img._image = r; img.naturalWidth = r.width; img.naturalHeight = r.height; img.complete = true; this.dirtyLayout(img, 'tree'); this.needPaint(); img.dispatchEvent(new E.Event('load')); });
  }
  imageFor(url, el, wait) {
    let u; try { u = new URL(url, this.url).href; } catch { return wait ? Promise.resolve(null) : null; }
    let rec = this.images.get(u);
    if (!rec) {
      rec = { id: 0, width: 0, height: 0, promise: null };
      this.images.set(u, rec);
      rec.promise = this.fetchBuffer(u).then(bytes => {
        const png = decodePNG(bytes);
        if (!png) { if (/\.svg/i.test(u) || bytes.subarray(0, 5).toString() === '<?xml' || bytes.subarray(0, 4).toString() === '<svg') { this.log(`image ${u}: svg is not drawn yet`); } else this.log(`image ${u}: not a png`); return null; }
        rec.id = this.gfx.imageNew(Buffer.from(png.data.buffer), png.width, png.height); rec.width = png.width; rec.height = png.height; rec.image = rec.id;
        this.dirtyLayoutAll(); this.needPaint();
        return rec;
      }).catch(e => { this.log(`image ${u}: ${e.message}`); return null; });
    }
    if (wait) return rec.promise.then(() => rec.id ? rec : null);
    return rec.id ? rec : null;
  }
  imageSource(img) {
    if (!img) return null;
    if (img.localName === 'canvas' && img._ctx) return { canvas: img._ctx.id, width: img.width, height: img.height };
    if (img.localName === 'img' && img._image) return { image: img._image.id, width: img._image.width, height: img._image.height };
    if (img.data && img.width) { const id = this.gfx.imageNew(Buffer.from(img.data.buffer, img.data.byteOffset, img.data.byteLength), img.width, img.height); return { image: id, width: img.width, height: img.height }; }
    return null;
  }
  makeCanvas2D(el) { return new CanvasRenderingContext2D(this, el); }
  fontFromShorthand(f) {
    const m = String(f).match(/(\d+(?:\.\d+)?)(px|pt|em|rem)?\s+(.*)$/);
    const size = m ? parseFloat(m[1]) * (m[2] === 'pt' ? 4 / 3 : m[2] === 'em' || m[2] === 'rem' ? 16 : 1) : 10;
    const fams = m ? splitCommas(m[3]).map(x => x.replace(/^["']|["']$/g, '')) : ['sans-serif'];
    const weight = /bold|[6-9]00/.test(f) ? 700 : 400, italic = /italic/.test(f);
    const face = this.fonts.primary(fams, weight, italic);
    return { families: fams, weight, italic, size, face };
  }
  // ---- frames
  requestAnimationFrame(cb) { const id = this.rafId++; this.rafs.push({ id, cb }); return id; }
  cancelAnimationFrame(id) { const i = this.rafs.findIndex(r => r.id === id); if (i >= 0) this.rafs.splice(i, 1); }
  // once a frame: animation callbacks, then layout and paint if needed
  tick(canvas) {
    if (!this.document) return false;
    if (this.rafs.length) { const list = this.rafs; this.rafs = []; const t = performance.now(); for (const r of list) { try { r.cb(t); } catch (e) { this.reportError(e); } } }
    if (this.prof) { this.prof.ticks++; const now = performance.now(); if (now - this.prof.t0 > 1000) { const p = this.prof; const pl = p.partialList.sort((a, b) => a - b); this.log(`prof/s: ticks ${p.ticks} layouts ${p.layouts} (${p.layoutMs.toFixed(0)} ms, ${p.partial} partial ${p.partialMs.toFixed(0)} ms: median ${pl.length ? pl[pl.length >> 1].toFixed(2) : 0} max ${pl.length ? pl[pl.length - 1].toFixed(1) : 0}) paints ${p.paints} (${p.paintMs.toFixed(0)} ms, clips ${p.clips} ${p.clipMs.toFixed(0)} ms, ${p.clipSlow} slow) styles ${p.styles} (${p.styleMs.toFixed(1)} ms, ${p.styles ? (p.cands / p.styles).toFixed(0) : 0} rules each, ${p.styles ? (p.hits / p.styles).toFixed(0) : 0} hit) sheet marks ${p.sheetMarks} boxes ${p.boxes}`); this.prof = { t0: now, ticks: 0, layouts: 0, partial: 0, partialMs: 0, partialList: [], styleMs: 0, cands: 0, sheetMarks: 0, deep: 0, hits: 0, layoutMs: 0, paints: 0, paintMs: 0, styles: 0, boxes: p.boxes, clips: 0, clipMs: 0, clipSlow: 0 }; } }
    if (!this.needsPaint && !this.layoutDirty) return false;
    this.ensureLayout();
    this.paint(canvas);
    for (const o of this.intersectionObservers) o._check(this);
    return true;
  }
  ensureLayout() {
    if (!this.layoutDirty || !this.document || !this.document.documentElement) return this.rootBox;
    if (this.inLayout) return this.rootBox;
    this.inLayout = true;
    try {
      if (this.sheetsDirty) this.collectSheets();
      this.layoutDirty = false;
      const t0 = performance.now();
      const partial = !this.layoutAll && this.relayoutRoots();
      this.layoutAll = false; this.layoutRoots.clear();
      if (!partial) this.rootBox = this.layout.run(this.document.documentElement, this.cssW, this.cssH);
      const dt = performance.now() - t0;
      if (this.prof && partial) { this.prof.partialMs += dt; this.prof.partialList.push(dt); }
      if (this.prof) { this.prof.layouts++; this.prof.layoutMs += dt; let n = 0; const walk = b => { n++; if (b.children) for (const k of b.children) walk(k); }; walk(this.rootBox); this.prof.boxes = n; }
      if (dt > 100) this.log(`layout ${dt.toFixed(0)} ms`);
      this.caretBox = this.focused && (this.focused.localName === 'input' || this.focused.localName === 'textarea') ? this.focused._box : null;
      this.needsPaint = true;
    } finally { this.inLayout = false; }
    for (const o of this.resizeObservers) o._check();
    if (this.layoutDirty && !this.relayoutGuard) { this.relayoutGuard = true; const b = this.ensureLayout(); this.relayoutGuard = false; return b; }
    return this.rootBox;
  }
  // lays out again only around the dirty elements: each root's
  // nearest block box is tried in place, then its parents while a
  // size moved; false means the whole page must go
  relayoutRoots() {
    if (!this.rootBox || !this.layoutRoots.size) return false;
    const roots = [];
    for (const [el, r] of this.layoutRoots) {
      if (!el.isConnected) continue;
      let self = false;
      if (r.tree) self = false;
      else {
        // own: a root only when its style really changed
        let changed = false;
        if (r.own) { const before = el._computed; if (el._styleDirty) { this.styleOf(el); changed = el._computed !== before; } else changed = !!el._styleChanged; }
        el._styleChanged = false;
        if (changed) self = false; else if (r.self) self = true; else continue;
      }
      roots.push({ el, self, depth: depthOf(el) });
    }
    roots.sort((a, b) => a.depth - b.depth);
    const done = [];
    for (const { el, self } of roots) {
      if (done.some(d => d.contains(el))) continue;
      let a = self ? el : el.parentElement, tries = 0, ok = false;
      for (; a && tries < 6; a = a.parentElement) {
        const b = a._box;
        if (!b || b.el !== a || !this.boxAttached(b) || !this.layout.canRelayout(b)) continue;
        tries++;
        const nb = this.layout.relayoutOne(b);
        if (nb) { done.push(a); ok = true; this.tracePartial(nb, tries); break; }
      }
      if (!ok) { this.traceFull(el, self); return false; }
    }
    if (this.prof) this.prof.partial++;
    return true;
  }
  tracePartial(b, tries) {
    if (!process.env.QUARK_TRACE_DIRTY) return;
    let n = 0; const walk = x => { n++; for (const k of x.children) walk(k); }; walk(b);
    const path = []; for (let a = b.el; a && path.length < 3; a = a.parentElement) path.push(a.localName + (a.className && typeof a.className === 'string' ? '.' + a.className.split(' ').slice(0, 2).join('.') : ''));
    const k = path.join(' < ');
    this.partCauses = this.partCauses || {}; const e = this.partCauses[k] || (this.partCauses[k] = { n: 0, boxes: 0, tries: 0 }); e.n++; e.boxes += n; e.tries += tries;
    if (!this.partTimer) this.partTimer = setTimeout(() => { this.partTimer = null; const top = Object.entries(this.partCauses).sort((a, b) => b[1].boxes - a[1].boxes).slice(0, 5).map(([k, v]) => `${v.n}x${Math.round(v.boxes / v.n)} (${(v.tries / v.n).toFixed(1)} up) ${k}`).join(' | '); this.partCauses = {}; this.log(`partial/s: ${top}`); }, 1000);
  }
  traceFull(el, self) {
    if (!process.env.QUARK_TRACE_DIRTY) return;
    const path = []; for (let a = el; a && path.length < 6; a = a.parentElement) path.push(a.localName + (a.className && typeof a.className === 'string' ? '.' + a.className.split(' ').slice(0, 2).join('.') : ''));
    const k = (self ? 'in ' : 'at ') + path.join(' < ');
    this.fullCauses = this.fullCauses || {}; this.fullCauses[k] = (this.fullCauses[k] || 0) + 1;
    if (!this.fullTimer) this.fullTimer = setTimeout(() => { this.fullTimer = null; const top = Object.entries(this.fullCauses).sort((a, b) => b[1] - a[1]).slice(0, 5).map(([k, v]) => `${v} ${k}`).join(' | '); this.fullCauses = {}; this.log(`full/s: ${top}`); }, 1000);
  }
  boxAttached(b) { for (let p = b; p; p = p.parent) if (p === this.rootBox) return true; return false; }
  paint(canvas) {
    if (!this.rootBox) return;
    this.needsPaint = false;
    const t0 = performance.now();
    // the root's background, else the body's, covers the whole canvas
    let bg = this.bg;
    const html = this.document.documentElement, body = this.document.body;
    const hb = html ? this.styleOf(html).colorOf('background-color') : null;
    const bb = body ? this.styleOf(body).colorOf('background-color') : null;
    if (hb && hb[3] > 0) bg = hb; else if (bb && bb[3] > 0) bg = bb;
    this.painter.prof = this.prof; this.painter.paint(canvas, this.rootBox, this.width, this.height, bg);
    const dt = performance.now() - t0;
    if (this.prof) { this.prof.paints++; this.prof.paintMs += dt; }
    if (dt > 100) this.log(`paint ${dt.toFixed(0)} ms`);
  }
  resize(w, h, scale, keepZoom) {
    this.width = w; this.height = h; this.scale = scale;
    this.updateEnv();
    if (this.screen) { this.screen.width = this.cssW; this.screen.height = this.cssH; this.screen.availWidth = this.cssW; this.screen.availHeight = this.cssH; }
    if (this.visualViewport) { this.visualViewport.width = this.cssW; this.visualViewport.height = this.cssH; }
    const g = globalThis;
    try { Object.defineProperty(g, 'innerWidth', { value: this.cssW, configurable: true, writable: true }); Object.defineProperty(g, 'innerHeight', { value: this.cssH, configurable: true, writable: true }); Object.defineProperty(g, 'outerWidth', { value: this.cssW, configurable: true, writable: true }); Object.defineProperty(g, 'outerHeight', { value: this.cssH, configurable: true, writable: true }); Object.defineProperty(g, 'devicePixelRatio', { value: this.scale * this.zoom, configurable: true, writable: true }); } catch {}
    for (const m of this.mediaLists) { const now = evalMedia(m.media); if (now !== m.matches) { m.matches = now; const ev = new E.Event('change'); ev.matches = now; ev.media = m.media; m.dispatchEvent(ev); if (m.onchange) m.onchange(ev); } }
    this.invalidateStyle();
    if (this.document) { globalThis.dispatchEvent(new E.UIEvent('resize')); this.visualViewport && this.visualViewport.dispatchEvent(new E.Event('resize')); }
  }
  // the box tree under a selector, for the log
  dumpBoxes(selector) {
    this.ensureLayout();
    let el = selector ? this.document.querySelector(selector) : this.document.documentElement;
    // text:<words> : the box three above the first text holding them
    if (selector && selector.startsWith('text:')) { const want = selector.slice(5); const w = this.document.createTreeWalker(this.document.body, 4); let n; el = null; while ((n = w.nextNode())) if (n.data.includes(want)) { el = n.parentElement; for (let k = 0; k < 3 && el.parentElement; k++) el = el.parentElement; break; } }
    if (!el || !el._box) return `no box for ${selector}`;
    const out = [];
    // the ancestors first: where the width came from
    const chain = []; for (let n = el.parentElement; n; n = n.parentElement) chain.unshift(n);
    for (const n of chain) { const b = n._box; if (!b) continue; const r = b.absRect(); out.push(`^ ${b.kind} ${n.localName}${n.className && typeof n.className === 'string' ? '.' + n.className.split(' ').slice(0, 3).join('.') : ''} ${r.x.toFixed(0)},${r.y.toFixed(0)} ${r.w.toFixed(0)}x${r.h.toFixed(0)} w:${b.cs.get('width')} d:${b.cs.get('display')} p:${b.cs.get('position')}${b.flexItem ? ' flexItem' : ''}`); }
    const dump = (b, d) => { if (b.kind === 'text' && !b.text.trim()) return; const r = b.absRect(); out.push(`${'  '.repeat(d)}${b.kind} ${b.el ? b.el.localName + (b.el.className && typeof b.el.className === 'string' ? '.' + b.el.className.split(' ').slice(0, 3).join('.') : '') : (b.text !== undefined ? JSON.stringify(String(b.text || '').slice(0, 24)) : b.pseudo || '')} ${r.x.toFixed(0)},${r.y.toFixed(0)} ${r.w.toFixed(0)}x${r.h.toFixed(0)}${b.kind === 'text' && b.textFrags ? ' frags ' + b.textFrags.length : ''}${b.el && b.el.getAttribute && b.el.getAttribute('style') ? ' s:' + b.el.getAttribute('style').slice(0, 60) : ''}${b.scrollable && b.el && (b.el._scrollX || b.el._scrollY) ? ' sc:' + (b.el._scrollX || 0) + ',' + (b.el._scrollY || 0) : ''} y:${b.y.toFixed(0)}`); if (process.env.QUARK_DUMP_CS && b.el) out.push(`${'  '.repeat(d)}  dom:${JSON.stringify(String(b.el.textContent || '').slice(0, 24))} cs: ` + ['display', 'width', 'height', 'min-width', 'font-size', 'font-family', 'line-height', 'white-space', 'flex', 'align-items', 'justify-content'].map(p => p + '=' + b.cs.get(p)).join(' ') + ` m=${['top', 'right', 'bottom', 'left'].map(s => b.cs.get('margin-' + s)).join('/')} p=${['top', 'right', 'bottom', 'left'].map(s => b.cs.get('padding-' + s)).join('/')} b=${['top', 'right', 'bottom', 'left'].map(s => b.cs.get('border-' + s + '-width') + ' ' + b.cs.get('border-' + s + '-style') + ' ' + b.cs.get('border-' + s + '-color')).join('/')} r=${b.cs.get('border-top-left-radius')} pos=${b.cs.get('position')} top=${b.cs.get('top')} bottom=${b.cs.get('bottom')} clip-path=${b.cs.get('clip-path')} prev=${b.el.previousElementSibling ? b.el.previousElementSibling.localName + '.' + String(b.el.previousElementSibling.className).split(' ').slice(0, 4).join('.') : '-'}` + (process.env.QUARK_DUMP_VAR ? ' ' + process.env.QUARK_DUMP_VAR + '=' + b.cs.vars.get(process.env.QUARK_DUMP_VAR) : '')); if (out.length > 600) return; for (const c of b.children) dump(c, d + 1); };
    dump(el._box, 0);
    return out.join('\n');
  }
  // ---- hit testing
  elementAt(x, y) {
    this.ensureLayout();
    if (!this.rootBox) return null;
    const hit = this.hitBox(this.rootBox, x, y);
    return hit ? hit : this.document.documentElement;
  }
  // deepest box under the point, in paint order (last painted first)
  hitBox(box, x, y) {
    if (box.kind === 'text') return null;
    const r = box.absRect();
    const clipped = box.clips && box.kind !== 'inline';
    if (clipped && (x < r.x || y < r.y || x > r.x + r.w || y > r.y + r.h)) return null;
    const cs = box.cs;
    const pe = cs.get('pointer-events') === 'none';
    const vis = cs.get('visibility') === 'visible';
    // children on top first: positioned by z, then flow backwards
    const kids = box.children.filter(k => k.kind !== 'text');
    const pos = kids.filter(k => k.positioned || k.stacking).sort((a, b) => b.z - a.z);
    const flow = kids.filter(k => !(k.positioned || k.stacking));
    for (const k of pos) { if (k.z < 0) continue; const h = this.hitBox(k, x, y); if (h) return h; }
    for (let i = flow.length - 1; i >= 0; i--) { const h = this.hitBox(flow[i], x, y); if (h) return h; }
    for (const k of pos) { if (k.z >= 0) continue; const h = this.hitBox(k, x, y); if (h) return h; }
    if (pe || !vis) return null;
    const el = box.el || box.pseudoOf;
    if (!el) return null;
    if (box.kind === 'inline' && box.rects) { for (const ir of box.rects) if (x >= ir.x && y >= ir.y && x <= ir.x + ir.w && y <= ir.y + ir.h) return el; return null; }
    if (x >= r.x && y >= r.y && x <= r.x + r.w && y <= r.y + r.h) return el;
    // a box's text may spill outside it (nowrap): the fragments count
    for (const k of box.children) if (k.kind === 'text' && k.textFrags) { const sc = this.painter.parentScroll(k); for (const f of k.textFrags) if (x >= f.x - sc.x && x <= f.x - sc.x + f.w && y >= f.top - sc.y && y <= f.top - sc.y + f.ascent + f.descent) return el; }
    return null;
  }
  // ---- input from the window
  event(line) {
    const w = line.split(' ');
    const kind = w[0];
    const k = this.scale * this.zoom;
    if (kind === 'mouse') { const x = Number(w[3]) / k, y = Number(w[4]) / k; this.mouse(w[2], x, y, Number(w[5]), Number(w[6]), Number(w[7])); }
    else if (kind === 'wheel') { const x = Number(w[2]) / k, y = Number(w[3]) / k; this.wheel(x, y, Number(w[4]), Number(w[5]), Number(w[6])); }
    else if (kind === 'key') this.key(w[2] === 'down', Number(w[3]), Number(w[4]));
    else if (kind === 'text') this.typed(line.slice(line.indexOf(' ', 5) + 1));
  }
  modsInit(mods, buttons) { return { ctrlKey: !!(mods & 2), shiftKey: !!(mods & 1), altKey: !!(mods & 4), metaKey: !!(mods & 8), buttons }; }
  mouse(kind, x, y, button, held, mods) {
    this.pointer = { x, y };
    const target = this.elementAt(x, y) || this.document.documentElement;
    const buttons = ((held & 1) ? 1 : 0) | ((held & 2) ? 2 : 0) | ((held & 4) ? 4 : 0);
    const common = { bubbles: true, cancelable: true, composed: true, clientX: x, clientY: y, screenX: x, screenY: y, button, buttons, view: globalThis, ...this.modsInit(mods, buttons) };
    const domButton = button === 1 ? 2 : button === 2 ? 1 : 0;
    common.button = domButton;
    if (kind === 'move') {
      this.updateHover(target, common);
      const mv = new E.PointerEvent('pointermove', common); target.dispatchEvent(mv);
      target.dispatchEvent(new E.MouseEvent('mousemove', common));
      if (this.dragTarget && this.dragging) { this.dragTarget.dispatchEvent(new E.DragEvent('drag', common)); target.dispatchEvent(new E.DragEvent('dragover', { ...common, dataTransfer: this.dataTransfer })); }
      this.updateCursor(target);
      return;
    }
    if (kind === 'down') {
      this.updateHover(target, common);
      this.pressTarget = target; this.pressButton = domButton;
      const now = Date.now();
      if (domButton === 0) { this.clickCount = (now - this.lastClick < 400 && this.lastClickTarget === target) ? this.clickCount + 1 : 1; this.lastClick = now; this.lastClickTarget = target; }
      const detail = domButton === 0 ? this.clickCount : 1;
      const pd = new E.PointerEvent('pointerdown', { ...common, detail }); target.dispatchEvent(pd);
      const md = new E.MouseEvent('mousedown', { ...common, detail }); target.dispatchEvent(md);
      this.setActive(target, true);
      if (!md.defaultPrevented && !pd.defaultPrevented) {
        // focus moves to the nearest focusable
        let f = target; while (f && f.nodeType === ELEMENT_NODE && !isFocusable(f)) f = f.parentElement;
        if (f && f.nodeType === ELEMENT_NODE && isFocusable(f)) this.focusElement(f, { fromMouse: true }); else this.focusElement(null, { fromMouse: true });
        if (f && (f.localName === 'input' || f.localName === 'textarea')) this.placeCaret(f, x, y);
      }
      if (domButton === 2) { target.dispatchEvent(new E.PointerEvent('contextmenu', common)); }
      return;
    }
    if (kind === 'up') {
      target.dispatchEvent(new E.PointerEvent('pointerup', common));
      target.dispatchEvent(new E.MouseEvent('mouseup', common));
      this.setActive(null, false);
      if (this.pressTarget && domButton === this.pressButton) {
        const common2 = this.pressTarget === target ? target : commonAncestor(this.pressTarget, target);
        if (common2 && domButton === 0) {
          const c = new E.PointerEvent('click', { ...common, detail: this.clickCount }); common2.dispatchEvent(c);
          if (!c.defaultPrevented) this.defaultClick(common2);
          if (this.clickCount === 2) common2.dispatchEvent(new E.MouseEvent('dblclick', { ...common, detail: 2 }));
        } else if (common2 && domButton === 1) common2.dispatchEvent(new E.MouseEvent('auxclick', common));
      }
      this.pressTarget = null;
    }
  }
  defaultClick(el) {
    for (let n = el; n; n = n.parentElement) {
      if (n.localName === 'label') { const c = n.control; if (c && c !== el) { c.focus(); if (c.localName === 'input' && (c.type === 'checkbox' || c.type === 'radio')) this.toggleCheck(c); } return; }
      if (n.localName === 'input' && (n.type === 'checkbox' || n.type === 'radio')) { this.toggleCheck(n); return; }
      if (n.localName === 'a' && n.getAttribute('href')) { const h = n.href; if (h && !h.startsWith('#') && !h.startsWith('javascript:')) this.say({ op: 'open', url: h }); return; }
      if (n.localName === 'summary' && n.parentElement && n.parentElement.localName === 'details') { n.parentElement.open = !n.parentElement.open; return; }
    }
  }
  toggleCheck(c) { if (c.type === 'checkbox') c.checked = !c.checked; else c.checked = true; c.dispatchEvent(new E.InputEvent('input', { bubbles: true })); c.dispatchEvent(new E.Event('change', { bubbles: true })); }
  updateHover(target, init) {
    if (target === this.hover) return;
    const old = this.hover; this.hover = target;
    const oldChain = chainOf(old), newChain = chainOf(target);
    for (const n of oldChain) if (!newChain.includes(n)) n._pseudo.delete('hover');
    for (const n of newChain) if (!oldChain.includes(n)) n._pseudo.add('hover');
    if (old) { old.dispatchEvent(new E.PointerEvent('pointerout', { ...init, relatedTarget: target })); old.dispatchEvent(new E.MouseEvent('mouseout', { ...init, relatedTarget: target })); for (const n of oldChain) if (!newChain.includes(n)) { n.dispatchEvent(new E.PointerEvent('pointerleave', { ...init, bubbles: false, relatedTarget: target })); n.dispatchEvent(new E.MouseEvent('mouseleave', { ...init, bubbles: false, relatedTarget: target })); } }
    if (target) { target.dispatchEvent(new E.PointerEvent('pointerover', { ...init, relatedTarget: old })); target.dispatchEvent(new E.MouseEvent('mouseover', { ...init, relatedTarget: old })); for (const n of newChain) if (!oldChain.includes(n)) { n.dispatchEvent(new E.PointerEvent('pointerenter', { ...init, bubbles: false, relatedTarget: old })); n.dispatchEvent(new E.MouseEvent('mouseenter', { ...init, bubbles: false, relatedTarget: old })); } }
    this.hoverStyleDirty(oldChain, newChain);
  }
  // hover changes restyle only when a rule could care
  hoverStyleDirty(a, b) { this.dirtyChains(a, b, [':hover']); this.needPaint(); }
  setActive(target, on) {
    const old = chainOf(this.active); this.active = on ? target : null;
    for (const n of old) n._pseudo.delete('active');
    if (on) for (const n of chainOf(target)) n._pseudo.add('active');
    this.dirtyChains(old, on ? chainOf(target) : [], [':active']); this.needPaint();
  }
  updateCursor(target) {
    let c = 'default';
    for (let n = target; n; n = n.parentElement) { const v = this.styleOf(n).get('cursor'); if (v && v !== 'auto') { c = v; break; } if (n.localName === 'a' || n.localName === 'button') { c = 'pointer'; break; } if (n.localName === 'input' || n.localName === 'textarea') { c = 'text'; break; } }
    if (c !== this.cursor) { this.cursor = c; this.say({ op: 'cursor', name: c }); }
  }
  wheel(x, y, dx, dy, mods) {
    const target = this.elementAt(x, y) || this.document.documentElement;
    // a GLFW notch is about 100 px of page
    const ev = new E.WheelEvent('wheel', { bubbles: true, cancelable: true, composed: true, clientX: x, clientY: y, deltaX: -dx * 100, deltaY: -dy * 100, deltaMode: 0, view: globalThis, ...this.modsInit(mods, 0) });
    target.dispatchEvent(ev);
    if (ev.defaultPrevented) return;
    // scroll the nearest box that can
    for (let n = target; n; n = n.parentElement) {
      const b = n._box; if (!b || !b.scrollable) continue;
      const cs = this.styleOf(n);
      const canY = b.scrollH() > b.clientH() + 0.5 && cs.get('overflow-y') !== 'hidden', canX = b.scrollW() > b.clientW() + 0.5 && cs.get('overflow-x') !== 'hidden';
      if ((canY && dy) || (canX && dx)) { n._setScroll(n.scrollLeft + (canX ? -dx * 100 : 0), n.scrollTop + (canY ? -dy * 100 : 0)); return; }
    }
  }
  key(down, code, mods) {
    const info = keyInfo(code, mods);
    const target = this.focused || this.document.body || this.document.documentElement;
    const init = { bubbles: true, cancelable: true, composed: true, key: info.key, code: info.code, keyCode: info.keyCode, which: info.keyCode, location: info.location || 0, view: globalThis, repeat: down && this.lastKeyDown === code, ...this.modsInit(mods, 0) };
    if (down) this.lastKeyDown = code; else if (this.lastKeyDown === code) this.lastKeyDown = null;
    const ev = new E.KeyboardEvent(down ? 'keydown' : 'keyup', init);
    target.dispatchEvent(ev);
    if (down && !ev.defaultPrevented) this.defaultKey(target, info, mods);
  }
  // editing keys in fields, tab focus, enter on buttons
  defaultKey(target, info, mods) {
    const f = this.focused;
    const editable = f && (f.localName === 'input' || f.localName === 'textarea');
    const ctrl = !!(mods & 2), shift = !!(mods & 1);
    if (editable && !f.readOnly && !f.disabled) {
      const v = f.value; let s = f.selectionStart ?? v.length, e = f.selectionEnd ?? v.length;
      const set = (nv, pos) => { f.value = nv; f.setSelectionRange(pos, pos); f.dispatchEvent(new E.InputEvent('input', { bubbles: true, inputType: 'deleteContentBackward' })); };
      if (info.key === 'Backspace') { if (s !== e) set(v.slice(0, Math.min(s, e)) + v.slice(Math.max(s, e)), Math.min(s, e)); else if (s > 0) { const n = ctrl ? wordStart(v, s) : s - 1; set(v.slice(0, n) + v.slice(s), n); } return; }
      if (info.key === 'Delete') { if (s !== e) set(v.slice(0, Math.min(s, e)) + v.slice(Math.max(s, e)), Math.min(s, e)); else if (s < v.length) set(v.slice(0, s) + v.slice(s + 1), s); return; }
      if (info.key === 'ArrowLeft') { const n = ctrl ? wordStart(v, s) : Math.max(0, (shift ? e : Math.min(s, e)) - (s === e ? 1 : 0)); if (shift) f.setSelectionRange(s, Math.max(0, e - 1)); else f.setSelectionRange(s === e ? Math.max(0, s - 1) : Math.min(s, e), s === e ? Math.max(0, s - 1) : Math.min(s, e)); this.needPaint(); return; }
      if (info.key === 'ArrowRight') { if (shift) f.setSelectionRange(s, Math.min(v.length, e + 1)); else { const p = s === e ? Math.min(v.length, s + 1) : Math.max(s, e); f.setSelectionRange(p, p); } this.needPaint(); return; }
      if (info.key === 'Home') { f.setSelectionRange(shift ? 0 : 0, shift ? e : 0); this.needPaint(); return; }
      if (info.key === 'End') { f.setSelectionRange(shift ? s : v.length, v.length); this.needPaint(); return; }
      if (ctrl && info.key === 'a') { f.setSelectionRange(0, v.length); this.needPaint(); return; }
      if (ctrl && (info.key === 'c' || info.key === 'x')) { const sel = v.slice(Math.min(s, e), Math.max(s, e)); if (sel) this.say({ op: 'clipboard', text: sel }); if (info.key === 'x' && sel) set(v.slice(0, Math.min(s, e)) + v.slice(Math.max(s, e)), Math.min(s, e)); return; }
      if (info.key === 'Enter' && f.localName === 'textarea') { this.insertText('\n'); return; }
      if (info.key === 'Enter' && f.localName === 'input') { f.dispatchEvent(new E.Event('change', { bubbles: true })); const form = f.closest('form'); if (form) form.dispatchEvent(new E.Event('submit', { bubbles: true, cancelable: true })); return; }
    }
    if (info.key === 'Tab' && !ctrl) { this.focusNext(shift ? -1 : 1); return; }
    if ((info.key === 'Enter' || info.key === ' ') && f && (f.localName === 'button' || (f.localName === 'input' && (f.type === 'button' || f.type === 'submit' || f.type === 'checkbox')))) { if (info.key === ' ' && f.type === 'checkbox') this.toggleCheck(f); else f.click(); }
  }
  typed(text) {
    if (!text) return;
    const f = this.focused;
    const target = f || this.document.body;
    for (const ch of text) { const kp = new E.KeyboardEvent('keypress', { bubbles: true, cancelable: true, key: ch, charCode: ch.codePointAt(0), keyCode: ch.codePointAt(0), which: ch.codePointAt(0), view: globalThis }); target.dispatchEvent(kp); if (kp.defaultPrevented) return; }
    if (f && (f.localName === 'input' || f.localName === 'textarea') && !f.readOnly && !f.disabled) this.insertText(text);
    else if (f && f.isContentEditable) { const bi = new E.InputEvent('beforeinput', { bubbles: true, cancelable: true, data: text, inputType: 'insertText' }); f.dispatchEvent(bi); if (!bi.defaultPrevented) { f.appendChild(this.document.createTextNode(text)); f.dispatchEvent(new E.InputEvent('input', { bubbles: true, data: text, inputType: 'insertText' })); } }
  }
  insertText(text) {
    const f = this.focused; if (!f) return;
    const bi = new E.InputEvent('beforeinput', { bubbles: true, cancelable: true, data: text, inputType: 'insertText' });
    f.dispatchEvent(bi); if (bi.defaultPrevented) return;
    const v = f.value, s = Math.min(f.selectionStart ?? v.length, f.selectionEnd ?? v.length), e = Math.max(f.selectionStart ?? v.length, f.selectionEnd ?? v.length);
    if (f.maxLength > 0 && v.length - (e - s) + text.length > f.maxLength) text = text.slice(0, Math.max(0, f.maxLength - (v.length - (e - s))));
    f.value = v.slice(0, s) + text + v.slice(e);
    f.setSelectionRange(s + text.length, s + text.length);
    f.dispatchEvent(new E.InputEvent('input', { bubbles: true, data: text, inputType: 'insertText' }));
  }
  placeCaret(f, x, y) {
    const b = f._box; if (!b) return;
    const r = b.absRect(); const cs = this.styleOf(f); const spec = this.text.spec(cs);
    const v = f.value; const lx = x - (r.x + b.bl + b.pl) + (f._scrollX || 0);
    let best = v.length, bd = Infinity;
    let line = v;
    if (f.localName === 'textarea') { const lines = v.split('\n'); const li = Math.max(0, Math.min(lines.length - 1, Math.floor((y - (r.y + b.bt + b.pt)) / cs.lineHeight))); let before = 0; for (let i = 0; i < li; i++) before += lines[i].length + 1; line = lines[li]; for (let i = 0; i <= line.length; i++) { const d = Math.abs(this.text.measure(line.slice(0, i), spec) - lx); if (d < bd) { bd = d; best = before + i; } } }
    else for (let i = 0; i <= v.length; i++) { const d = Math.abs(this.text.measure(v.slice(0, i), spec) - lx); if (d < bd) { bd = d; best = i; } }
    f.setSelectionRange(best, best);
    this.needPaint();
  }
  // ---- focus
  focusElement(el, opts) {
    if (el && !el.isConnected) return;
    const old = this.focused;
    if (el === old) return;
    const fromMouse = opts && opts.fromMouse;
    if (old) { old._pseudo.delete('focus'); old._pseudo.delete('focus-visible'); for (const n of chainOf(old)) n._pseudo.delete('focus-within'); old.dispatchEvent(new E.FocusEvent('blur', { relatedTarget: el })); old.dispatchEvent(new E.FocusEvent('focusout', { bubbles: true, relatedTarget: el })); }
    this.focused = el; this.document.activeElement = el || this.document.body;
    if (el) { el._pseudo.add('focus'); if (!fromMouse || el.localName === 'input' || el.localName === 'textarea') el._pseudo.add('focus-visible'); for (const n of chainOf(el)) n._pseudo.add('focus-within'); el.dispatchEvent(new E.FocusEvent('focus', { relatedTarget: old })); el.dispatchEvent(new E.FocusEvent('focusin', { bubbles: true, relatedTarget: old })); }
    this.dirtyChains(chainOf(old), chainOf(el), [':focus', ':focus-visible', ':focus-within']); this.needPaint();
  }
  focusChanged() { this.focused = null; }
  focusNext(dir) {
    const all = [...this.document.querySelectorAll('*')].filter(e => isFocusable(e) && e.tabIndex >= 0 && e._box && !e.disabled);
    if (!all.length) return;
    let i = all.indexOf(this.focused);
    i = (i + dir + all.length) % all.length;
    this.focusElement(all[i]);
  }
  focus(on) { this._focused = on; globalThis.dispatchEvent(new E.FocusEvent(on ? 'focus' : 'blur')); this.document.dispatchEvent(new E.Event(on ? 'focus' : 'blur')); if (this.document) this.document.dispatchEvent(new E.Event('visibilitychange')); this.caretHidden = !on; this.needPaint(); }
  focusWindow() { this.say({ op: 'focus-window' }); } closeWindow() { this.say({ op: 'close-window' }); }
  openExternal(url) { this.say({ op: 'open', url }); }
  navigate(url) { this.log(`navigate to ${url} is not done yet`); }
  execCommand(cmd, value) {
    const f = this.focused;
    if (cmd === 'copy' || cmd === 'cut') { let t = ''; if (f && (f.localName === 'input' || f.localName === 'textarea')) t = f.value.slice(Math.min(f.selectionStart, f.selectionEnd), Math.max(f.selectionStart, f.selectionEnd)); else t = this.selection.toString(); const ev = new E.ClipboardEvent(cmd, { bubbles: true, cancelable: true, clipboardData: makeDataTransfer() }); (f || this.document.body).dispatchEvent(ev); if (ev.defaultPrevented) { const d = ev.clipboardData.getData('text/plain'); if (d) t = d; } if (t) this.say({ op: 'clipboard', text: t }); return true; }
    if (cmd === 'paste') { const ev = new E.ClipboardEvent('paste', { bubbles: true, cancelable: true, clipboardData: makeDataTransfer() }); (f || this.document.body).dispatchEvent(ev); return true; }
    if (cmd === 'insertText') { if (f && (f.localName === 'input' || f.localName === 'textarea')) this.insertText(String(value)); return true; }
    if (cmd === 'selectAll') { if (f && f.select) f.select(); return true; }
    if (cmd === 'delete') { if (f && f.value !== undefined) { const s = Math.min(f.selectionStart, f.selectionEnd), e = Math.max(f.selectionStart, f.selectionEnd); f.value = f.value.slice(0, s) + f.value.slice(e); f.setSelectionRange(s, s); } return true; }
    return false;
  }
  // ---- ipc from the main process
  ipc(channel, args, portIds) {
    const l = this.ipcListeners.get(channel); if (!l) return;
    const ports = portIds.map(id => this.rendererPort(id));
    const ev = { sender: { send: (c, ...a) => this.say({ op: 'ipc', channel: c, args: a, ports: [] }) }, senderId: 0, ports, channel, returnValue: undefined, preventDefault() {}, reply: (c, ...a) => this.say({ op: 'ipc', channel: c, args: a, ports: [] }) };
    for (const x of l.slice()) { if (x.once) { const i = l.indexOf(x); if (i >= 0) l.splice(i, 1); } try { x.fn(ev, ...args); } catch (e) { this.reportError(e); } }
  }
  invokeReply(id, ok, value) { const p = this.invokes.get(id); if (!p) return; this.invokes.delete(id); if (ok) p.resolve(value); else p.reject(new Error(String(value))); }
  // ports: Electron ports arriving in the renderer behave as MessagePorts
  rendererPort(id) {
    let p = this.ports.get(id);
    if (p) return p;
    const page = this;
    const t = new E.EventTarget();
    p = Object.assign(t, { _quarkId: id, onmessage: null, onmessageerror: null, _started: false, _queue: [],
      postMessage(data, transfer) { const ids = (Array.isArray(transfer) ? transfer : (transfer && transfer.transfer) || []).map(x => page.portId(x)); page.say({ op: 'port', port: id, data, ports: ids }); },
      start() { this._started = true; for (const ev of this._queue) this.dispatchEvent(ev); this._queue = []; },
      close() { page.ports.delete(id); },
      _deliver(data, ports) { const ev = new E.MessageEvent('message', { data, ports }); if (!this._started && !this.onmessage && !this._listeners) { this._queue.push(ev); return; } this.dispatchEvent(ev); },
    });
    const origAdd = p.addEventListener.bind(p);
    p.addEventListener = (type, fn, o) => { origAdd(type, fn, o); if (type === 'message' && !p._started) p.start(); };
    Object.defineProperty(p, 'onmessage', { get() { return p._onmessage || null; }, set(f) { p._onmessage = f; if (f) p.start(); } });
    p._invoke = function (ev, phase) { E.EventTarget.prototype._invoke.call(this, ev, phase); if (phase !== E.Event.CAPTURING_PHASE && typeof this._onmessage === 'function' && ev.type === 'message') { try { this._onmessage(ev); } catch (e) { page.reportError(e); } } };
    this.ports.set(id, p);
    return p;
  }
  portMessage(portId, data, portIds) { const p = this.rendererPort(portId); p._deliver(data, portIds.map(id => this.rendererPort(id))); }
  eval(id, code) { try { const v = vm.runInThisContext(code); Promise.resolve(v).then(r => this.say({ op: 'eval', id, ok: true, value: r === undefined ? null : JSON.parse(JSON.stringify(r)) }), e => this.say({ op: 'eval', id, ok: false, value: String(e) })); } catch (e) { this.say({ op: 'eval', id, ok: false, value: String(e && e.message || e) }); } }
  // ---- storage files
  storageFile(name) { const origin = (this.location && this.location.origin || 'null').replace(/[^a-z0-9]/gi, '_'); return path.join(this.storageDir, `${origin}-${name}.json`); }
  loadStorage(name) { try { return new Map(Object.entries(JSON.parse(fs.readFileSync(this.storageFile(name), 'utf8')))); } catch { return new Map(); } }
  saveStorage(name, map) { if (name !== 'local') return; try { fs.mkdirSync(this.storageDir, { recursive: true }); fs.writeFileSync(this.storageFile(name), JSON.stringify(Object.fromEntries(map))); } catch (e) { this.log(`storage: ${e.message}`); } }
  makeNavigator() {
    const page = this;
    return { userAgent: UA, platform: 'Linux x86_64', language: 'en-US', languages: ['en-US', 'en'], onLine: true, hardwareConcurrency: os.cpus().length, maxTouchPoints: 0, vendor: 'Google Inc.', appVersion: UA.slice(8), appName: 'Netscape', appCodeName: 'Mozilla', product: 'Gecko', productSub: '20030107', cookieEnabled: true, doNotTrack: null, pdfViewerEnabled: false, webdriver: false, deviceMemory: 8,
      clipboard: { writeText: t => { page.say({ op: 'clipboard', text: String(t) }); return Promise.resolve(); }, readText: () => Promise.resolve(''), write: () => Promise.resolve(), read: () => Promise.resolve([]) },
      keyboard: { getLayoutMap: () => Promise.resolve(new Map()), lock: () => Promise.resolve(), unlock() {}, addEventListener() {}, removeEventListener() {} },
      userActivation: { isActive: true, hasBeenActive: true }, permissions: { query: () => Promise.resolve({ state: 'granted', onchange: null, addEventListener() {}, removeEventListener() {} }) },
      mediaDevices: { enumerateDevices: () => Promise.resolve([]), getUserMedia: () => Promise.reject(new Error('no media')), getDisplayMedia: () => Promise.reject(new Error('no media')), addEventListener() {}, removeEventListener() {} },
      storage: { estimate: () => Promise.resolve({ usage: 0, quota: 1e9 }), persist: () => Promise.resolve(true), persisted: () => Promise.resolve(true) },
      locks: { request: (name, opts, cb) => Promise.resolve((cb || opts)({ name, mode: 'exclusive' })), query: () => Promise.resolve({ held: [], pending: [] }) },
      connection: { effectiveType: '4g', downlink: 10, rtt: 50, saveData: false, addEventListener() {}, removeEventListener() {} },
      scheduling: { isInputPending: () => false }, userAgentData: { brands: [{ brand: 'Chromium', version: '136' }], mobile: false, platform: 'Linux', getHighEntropyValues: () => Promise.resolve({ platform: 'Linux', platformVersion: '6.0', architecture: 'x86', bitness: '64', model: '', uaFullVersion: '136.0.0.0' }) },
      sendBeacon: () => true, getGamepads: () => [], vibrate: () => false, share: () => Promise.reject(new Error('no share')), canShare: () => false, requestMIDIAccess: () => Promise.reject(new Error('no midi')), getBattery: () => Promise.resolve({ charging: true, level: 1, addEventListener() {} }),
      serviceWorker: undefined, credentials: undefined, geolocation: undefined, wakeLock: { request: () => Promise.resolve({ release: () => Promise.resolve(), released: false, addEventListener() {} }) }, gpu: undefined, bluetooth: undefined, usb: undefined, hid: undefined, serial: undefined, xr: undefined, virtualKeyboard: undefined, windowControlsOverlay: { visible: false, getTitlebarAreaRect: () => new DOMRect(), addEventListener() {}, removeEventListener() {} }, ink: undefined, login: undefined, presentation: undefined, mediaCapabilities: { decodingInfo: () => Promise.resolve({ supported: false, smooth: false, powerEfficient: false }) }, mediaSession: { setActionHandler() {}, metadata: null, playbackState: 'none' } };
  }
  makeWorkerClass() {
    const page = this;
    return class Worker extends E.EventTarget {
      constructor(url, opts = {}) {
        super();
        this.onmessage = null; this.onerror = null; this.onmessageerror = null;
        const href = new URL(String(url), page.url).href;
        let code = null;
        if (href.startsWith('blob:')) { const b = page.blobs && page.blobs.get(href); code = b ? b.text : ''; }
        else if (href.startsWith('data:')) code = Buffer.from(href.split(',')[1] || '', href.includes(';base64') ? 'base64' : 'utf8').toString('utf8');
        if (code !== null && code.includes('import ') && !code.includes('importScripts')) { /* a module blob: write it out */ const tmp = path.join(os.tmpdir(), `quark-worker-${process.pid}-${Date.now()}-${Math.random().toString(36).slice(2)}.mjs`); fs.writeFileSync(tmp, code); this.url = pathToFileURL(tmp).href; code = null; opts = { ...opts, type: 'module' }; }
        this.worker = new NodeWorker(path.join(here, 'webworker.mjs'), { workerData: { url: this.url || page.moduleURL(href), code, type: opts.type || 'classic', name: opts.name || '', userAgent: UA, roots: [...page.schemeRoots] }, stderr: false, stdout: false });
        this.worker.on('message', m => { if (m && m.__quark === 'error') { page.log(`worker: ${m.text}`); const ev = new E.ErrorEvent('error', { message: m.text }); this.dispatchEvent(ev); if (this.onerror) this.onerror(ev); return; } const ev = new E.MessageEvent('message', { data: m.data, ports: m.ports || [] }); this.dispatchEvent(ev); if (this.onmessage) this.onmessage(ev); });
        this.worker.on('error', e => { page.log(`worker ${href}: ${e && e.stack || e}`); const ev = new E.ErrorEvent('error', { message: String(e && e.message || e), error: e }); this.dispatchEvent(ev); if (this.onerror) this.onerror(ev); });
      }
      postMessage(data, transfer) { try { this.worker.postMessage({ data }, Array.isArray(transfer) ? transfer : (transfer && transfer.transfer) || []); } catch (e) { page.log(`worker post: ${e.message}`); } }
      terminate() { this.worker.terminate(); }
    };
  }
}
export function absolutizeUrls(text, base) {
  return text.replace(/url\(\s*(["']?)([^"')]+)\1\s*\)/g, (m, q, u) => { if (/^(data:|blob:|#|[a-z][a-z0-9+.-]*:)/i.test(u)) return m; try { return `url(${q}${new URL(u, base).href}${q})`; } catch { return m; } });
}
function parseWeight(w) { w = String(w).trim().split(/\s+/)[0]; if (w === 'bold') return 700; if (w === 'normal') return 400; return parseInt(w) || 400; }
function safeStr(x) { try { return typeof x === 'object' && x !== null ? JSON.stringify(x, (k, v) => typeof v === 'bigint' ? String(v) : v) : String(x); } catch { return String(x); } }
function chainOf(el) { const out = []; for (let n = el; n && n.nodeType === ELEMENT_NODE; n = n.parentElement) out.push(n); return out; }
function commonAncestor(a, b) { for (let n = a; n; n = n.parentNode) if (n.contains(b)) return n; return null; }
function isFocusable(e) { if (e.nodeType !== ELEMENT_NODE) return false; const t = e.getAttribute('tabindex'); if (t !== null && !e.hasAttribute('disabled')) return true; return ['input', 'textarea', 'select', 'button', 'a', 'summary'].includes(e.localName) && !e.hasAttribute('disabled') || e.isContentEditable; }
function wordStart(v, s) { let i = s; while (i > 0 && /\s/.test(v[i - 1])) i--; while (i > 0 && !/\s/.test(v[i - 1])) i--; return i; }
function makeDataTransfer() { const d = new Map(); return { setData: (t, v) => d.set(t, v), getData: t => d.get(t) || '', clearData: () => d.clear(), types: [], files: [], items: [], dropEffect: 'none', effectAllowed: 'all' }; }

// el restyles; under it, with keys, only what a rule naming one of
// the keys above it could reach (styleOf checks); else everything
function dirtyTree(el, keys) {
  el._styleDirty = true; el._pseudoStyles = null;
  let n = 1;
  for (let c = el.firstElementChild; c; c = c.nextElementSibling) n += keys ? dirtyUnder(c, keys) : dirtyTree(c);
  return n;
}
function dirtyUnder(el, keys) {
  if (!el._styleDirty) { if (el._styleKeys) for (const k of keys) el._styleKeys.push(k); else el._styleKeys = keys.slice(); }
  el._pseudoStyles = null;
  let n = 1;
  for (let c = el.firstElementChild; c; c = c.nextElementSibling) n += dirtyUnder(c, keys);
  return n;
}
function sameDecl(a, b) {
  if (a.size !== b.size) return false;
  for (const [k, v] of a) if (b.get(k) !== v) return false;
  return true;
}
// the selector keys an attribute change touches
function attrKeys(el, detail) {
  const name = detail.attributeName, now = el.getAttribute(name), was = detail.oldValue;
  if (name === 'style') return [];
  if (name === 'id') return ['#' + (was || ''), '#' + (now || '')];
  if (name === 'class') {
    const a = was ? was.split(/\s+/) : [], b = now ? now.split(/\s+/) : [];
    const out = [];
    for (const c of a) if (c && !b.includes(c)) out.push('.' + c);
    for (const c of b) if (c && !a.includes(c)) out.push('.' + c);
    return out;
  }
  return ['[' + name + ']'];
}
function depthOf(el) { let n = 0; for (let a = el.parentElement; a; a = a.parentElement) n++; return n; }
