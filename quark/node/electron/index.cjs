'use strict';
// Electron's main-process API on Node and trinity. The quark window
// (a trinity app) is on fd 3: lines out (windows, frames, titles),
// lines in (size, mouse, keys). Each BrowserWindow's page runs in a
// worker thread that draws through trinity (renderer.mjs)
const { EventEmitter } = require('node:events');
const net = require('node:net');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const child = require('node:child_process');
// utility children die with the main process
const utilityProcs = new Set();
process.on('exit', () => { for (const p of utilityProcs) { try { p.kill('SIGTERM'); } catch (e) { /* gone */ } } });
const { Worker } = require('node:worker_threads');
const { pathToFileURL } = require('node:url');

const ELECTRON_VERSION = '43.7.7';
let appDir = process.cwd(), pkg = {}, here = __dirname, addon = null;
const quark = { sock: null, buf: '', views: new Map(), scale: 1, width: 1280, height: 800 };

function log(s) { process.stderr.write(`quark: ${s}\n`); }
function post(line) { if (process.env.QUARK_TRACE) log(`-> ${line}`); if (quark.sock) quark.sock.write(line + '\n'); }

// ---------------------------------------------------------------- app
const paths = new Map();
function homeDirs() {
  const h = os.homedir();
  const xdg = (k, d) => process.env[k] || path.join(h, d);
  return {
    home: h, appData: xdg('XDG_CONFIG_HOME', '.config'), temp: os.tmpdir(),
    desktop: path.join(h, 'Desktop'), documents: path.join(h, 'Documents'),
    downloads: path.join(h, 'Downloads'), music: path.join(h, 'Music'),
    pictures: path.join(h, 'Pictures'), videos: path.join(h, 'Videos'),
    recent: path.join(h, '.local/share/recently-used'),
    exe: process.execPath, module: process.execPath,
  };
}
class CommandLine {
  constructor() { this.switches = new Map(); this.args = []; }
  appendSwitch(k, v) { this.switches.set(k, v === undefined ? '' : String(v)); }
  appendArgument(a) { this.args.push(a); }
  hasSwitch(k) { return this.switches.has(k) || process.argv.includes(`--${k}`); }
  getSwitchValue(k) {
    if (this.switches.has(k)) return this.switches.get(k);
    const a = process.argv.find(x => x.startsWith(`--${k}=`));
    return a ? a.slice(k.length + 3) : '';
  }
  removeSwitch(k) { this.switches.delete(k); }
}
class App extends EventEmitter {
  constructor() {
    super();
    this.commandLine = new CommandLine();
    this.ready = false;
    this.readyPromise = new Promise(r => { this._readyResolve = r; });
    this.accessibilitySupportEnabled = false;
    this.name = '';
    this.badgeCount = 0;
    this.dock = undefined;
  }
  get isPackaged() { return true; }
  whenReady() { return this.readyPromise; }
  isReady() { return this.ready; }
  getName() { return this.name || pkg.productName || pkg.name || 'quark'; }
  setName(n) { this.name = n; }
  getVersion() { return pkg.version || '0.0.0'; }
  getAppPath() { return appDir; }
  getPath(name) {
    if (paths.has(name)) return paths.get(name);
    const d = homeDirs();
    if (name in d) return d[name];
    if (name === 'userData') return path.join(d.appData, this.getName());
    if (name === 'sessionData') return this.getPath('userData');
    if (name === 'logs') return path.join(this.getPath('userData'), 'logs');
    if (name === 'crashDumps') return path.join(this.getPath('userData'), 'crashes');
    if (name === 'cache') return process.env.XDG_CACHE_HOME || path.join(d.home, '.cache');
    throw new Error(`Failed to get '${name}' path`);
  }
  setPath(name, p) { paths.set(name, p); }
  setAppLogsPath(p) { paths.set('logs', p || path.join(this.getPath('userData'), 'logs')); }
  getLocale() { return (process.env.LANG || 'en-US').split('.')[0].replace('_', '-'); }
  getLocaleCountryCode() { return 'US'; }
  getSystemLocale() { return this.getLocale(); }
  getPreferredSystemLanguages() { return [this.getLocale()]; }
  requestSingleInstanceLock() { return true; }
  hasSingleInstanceLock() { return true; }
  releaseSingleInstanceLock() {}
  setAppUserModelId() {}
  setAsDefaultProtocolClient() { return false; }
  removeAsDefaultProtocolClient() { return false; }
  isDefaultProtocolClient() { return false; }
  setDesktopName() {}
  enableSandbox() {}
  disableHardwareAcceleration() {}
  disableDomainBlockingFor3DAPIs() {}
  getGPUFeatureStatus() { return {}; }
  getGPUInfo() { return Promise.resolve({}); }
  getJumpListSettings() { return { minItems: 0, removedItems: [] }; }
  setJumpList() {}
  addRecentDocument() {}
  clearRecentDocuments() {}
  setBadgeCount(n) { this.badgeCount = n; return true; }
  getBadgeCount() { return this.badgeCount; }
  isAccessibilitySupportEnabled() { return this.accessibilitySupportEnabled; }
  setAccessibilitySupportEnabled(v) { this.accessibilitySupportEnabled = v; }
  configureHostResolver() {}
  resolveProxy() { return Promise.resolve('DIRECT'); }
  setProxy() { return Promise.resolve(); }
  getLoginItemSettings() { return { openAtLogin: false, openAsHidden: false, wasOpenedAtLogin: false, wasOpenedAsHidden: false, restoreState: false, executableWillLaunchAtLogin: false, launchItems: [] }; }
  setLoginItemSettings() {}
  isHidden() { return false; }
  hide() {} show() {}
  focus() { post('focus'); }
  runningUnderARM64Translation = false;
  isEmojiPanelSupported() { return false; }
  isUnityRunning() { return false; }
  getApplicationNameForProtocol() { return ''; }
  setActivationPolicy() {}
  relaunch(opts) {
    const args = (opts && opts.args) || process.argv.slice(1);
    child.spawn(process.execPath, args, { detached: true, stdio: 'ignore' }).unref();
  }
  quit() {
    if (this._quitting) return;
    this._quitting = true;
    let stop = false;
    const ev = { preventDefault: () => { stop = true; } };
    this.emit('before-quit', ev);
    if (stop) { this._quitting = false; return; }
    for (const w of BrowserWindow.getAllWindows()) w.close();
    this.emit('will-quit', ev);
    if (stop) { this._quitting = false; return; }
    this.emit('quit', ev, 0);
    post('quit');
    setTimeout(() => process.exit(0), 50);
  }
  exit(code = 0) { post('quit'); process.exit(code); }
  _becomeReady() {
    if (this.ready) return;
    this.emit('will-finish-launching');
    this.ready = true;
    const ev = {};
    this.emit('ready', ev, {});
    this._readyResolve(ev);
  }
}
const app = new App();

// ---------------------------------------------------------- ports
// every MessagePortMain is a number; the hub routes a message to its
// peer wherever that lives: here, a renderer worker, a utility child
let nextPort = 1;
const ports = new Map(); // id -> { peer, where: 'main'|'worker'|'child', host, obj }
class MessagePortMain extends EventEmitter {
  constructor(id) { super(); this.id = id; this.started = false; this.queue = []; }
  postMessage(message, transfer) {
    const ids = (transfer || []).map(p => p.id);
    hubPost(this.id, message, ids);
  }
  start() {
    this.started = true;
    for (const m of this.queue) this.emit('message', m);
    this.queue = [];
  }
  close() { this.emit('close'); ports.delete(this.id); }
  _deliver(data, portIds) {
    const ev = { data, ports: portIds.map(id => portObject(id)) };
    if (this.started || this.listenerCount('message')) this.emit('message', ev);
    else this.queue.push(ev);
  }
}
function portObject(id) {
  let p = ports.get(id);
  if (!p) { p = { peer: 0, where: 'main', host: null, obj: null }; ports.set(id, p); }
  if (!p.obj) p.obj = new MessagePortMain(id);
  p.where = 'main'; p.host = null;
  return p.obj;
}
class MessageChannelMain {
  constructor() {
    const a = nextPort++, b = nextPort++;
    ports.set(a, { peer: b, where: 'main', host: null, obj: null });
    ports.set(b, { peer: a, where: 'main', host: null, obj: null });
    this.port1 = portObject(a);
    this.port2 = portObject(b);
  }
}
// a port leaves for a worker or a child: messages to it go there
function portMoveTo(id, where, host) {
  const p = ports.get(id);
  if (!p) return;
  p.where = where; p.host = host; p.obj = null;
}
function hubPost(fromId, message, transferIds) {
  const from = ports.get(fromId);
  if (!from) return;
  const to = ports.get(from.peer);
  if (!to) return;
  if (to.where === 'main') {
    const obj = portObject(from.peer);
    obj._deliver(message, transferIds);
  } else if (to.where === 'worker') {
    to.host.postMessage({ op: 'port', port: from.peer, data: message, ports: transferIds });
    for (const t of transferIds) portMoveTo(t, 'worker', to.host);
  } else if (to.where === 'child') {
    to.host.send({ quark: 'port', port: from.peer, data: message, ports: transferIds });
    for (const t of transferIds) portMoveTo(t, 'child', to.host);
  }
}
// a message came up from a worker or child on one of its ports
function hubFrom(portId, data, transferIds, where, host) {
  for (const t of transferIds) portMoveTo(t, where, host);
  hubPost(portId, data, transferIds);
}

// ----------------------------------------------------------- ipc
class IpcMainEvent {
  constructor(wc, frameUrl, portIds) {
    this.sender = wc;
    this.senderFrame = wc.mainFrame;
    this.frameId = 1;
    this.processId = wc.getOSProcessId();
    this.ports = portIds.map(id => portObject(id));
    this.returnValue = undefined;
  }
  reply(channel, ...args) { this.sender.send(channel, ...args); }
}
class IpcMain extends EventEmitter {
  constructor() { super(); this.handlers = new Map(); }
  handle(channel, fn) { this.handlers.set(channel, fn); }
  handleOnce(channel, fn) { this.handlers.set(channel, (...a) => { this.handlers.delete(channel); return fn(...a); }); }
  removeHandler(channel) { this.handlers.delete(channel); }
}
const ipcMain = new IpcMain();

// -------------------------------------------------------- windows
let nextWindow = 1;
const windows = new Map();
const webContentsById = new Map();
class WebFrameMain {
  constructor(wc) { this.wc = wc; this.parent = null; this.frameTreeNodeId = 1; this.routingId = 1; this.processId = process.pid; this.name = ''; }
  get url() { return this.wc.url; }
  get origin() { try { return new URL(this.wc.url).origin; } catch { return 'null'; } }
  get top() { return this; }
  get frames() { return []; }
  get framesInSubtree() { return [this]; }
  executeJavaScript(code) { return this.wc.executeJavaScript(code); }
  send(channel, ...args) { this.wc.send(channel, ...args); }
  postMessage(channel, message, transfer) { this.wc.postMessage(channel, message, transfer); }
  isDestroyed() { return this.wc.destroyed; }
  collectJavaScriptCallStack() { return Promise.resolve(''); }
}
class NavigationHistory {
  constructor(wc) { this.wc = wc; }
  canGoBack() { return false; } canGoForward() { return false; }
  goBack() {} goForward() {} getActiveIndex() { return 0; } length() { return 1; }
  getEntryAtIndex() { return { url: this.wc.url, title: this.wc.title }; }
}
class WebContents extends EventEmitter {
  constructor(win, prefs) {
    super();
    this.win = win; this.prefs = prefs || {}; this.id = win.id;
    this.url = ''; this.title = ''; this.destroyed = false; this.zoom = 1;
    this.mainFrame = new WebFrameMain(this);
    this.navigationHistory = new NavigationHistory(this);
    this.session = defaultSession;
    this.ipc = new IpcMain();
    this.openHandler = null;
    this.userAgent = 'quark';
    this.evals = new Map(); this.nextEval = 1;
    this.loading = false;
    webContentsById.set(this.id, this);
  }
  getType() { return 'window'; }
  getOSProcessId() { return process.pid; }
  getProcessId() { return process.pid; }
  getURL() { return this.url; }
  getTitle() { return this.title; }
  isDestroyed() { return this.destroyed; }
  isLoading() { return this.loading; } isLoadingMainFrame() { return this.loading; }
  isWaitingForResponse() { return false; }
  isCrashed() { return false; }
  isFocused() { return this.win.focused; }
  isOffscreen() { return false; }
  isDevToolsOpened() { return false; } isDevToolsFocused() { return false; }
  openDevTools() {} closeDevTools() {} toggleDevTools() {} inspectElement() {}
  get devToolsWebContents() { return null; }
  get debugger() { return { attach() {}, detach() {}, isAttached() { return false; }, sendCommand() { return Promise.resolve({}); }, on() {} }; }
  getZoomFactor() { return this.zoom; }
  setZoomFactor(z) { this.zoom = z; this.win._toWorker({ op: 'zoom', factor: z }); }
  getZoomLevel() { return Math.log(this.zoom) / Math.log(1.2); }
  setZoomLevel(l) { this.setZoomFactor(Math.pow(1.2, l)); }
  setVisualZoomLevelLimits() { return Promise.resolve(); }
  setUserAgent(ua) { this.userAgent = ua; } getUserAgent() { return this.userAgent; }
  setWindowOpenHandler(fn) { this.openHandler = fn; }
  setBackgroundThrottling() {}
  setAudioMuted() {} isAudioMuted() { return false; } isCurrentlyAudible() { return false; }
  focus() { this.win.focus(); }
  reload() { this.loadURL(this.url); } reloadIgnoringCache() { this.reload(); }
  loadURL(url, opts) {
    this.url = url; this.loading = true;
    this.emit('did-start-loading');
    this.emit('did-start-navigation', { url, isSameDocument: false, isMainFrame: true });
    this.win._load(url);
    return new Promise(r => this.once('did-finish-load', r));
  }
  loadFile(file) { return this.loadURL(pathToFileURL(path.resolve(appDir, file)).href); }
  send(channel, ...args) { this.win._toWorker({ op: 'ipc', channel, args: marshal(args) }); }
  postMessage(channel, message, transfer) {
    const ids = (transfer || []).map(p => p.id);
    for (const id of ids) portMoveTo(id, 'worker', this.win.worker);
    this.win._toWorker({ op: 'ipc', channel, args: marshal([message]), ports: ids });
  }
  executeJavaScript(code) {
    return new Promise((resolve, reject) => {
      const id = this.nextEval++;
      this.evals.set(id, { resolve, reject });
      this.win._toWorker({ op: 'eval', id, code });
    });
  }
  executeJavaScriptInIsolatedWorld(w, scripts) { return this.executeJavaScript(scripts.map(s => s.code).join(';')); }
  insertCSS() { return Promise.resolve(''); }
  capturePage() { return Promise.resolve(nativeImage.createEmpty()); }
  findInPage() { return 0; } stopFindInPage() {}
  copyImageAt() {} paste() {} copy() {} cut() {} selectAll() {} undo() {} redo() {}
  replaceMisspelling() {}
  setIgnoreMenuShortcuts() {}
  enableDeviceEmulation() {} disableDeviceEmulation() {}
  beginFrameSubscription() {} endFrameSubscription() {}
  getOrCreateDevToolsTargetId() { return ''; }
  print() {} printToPDF() { return Promise.resolve(Buffer.alloc(0)); }
  close() { this.win.close(); }
  forcefullyCrashRenderer() {}
  setImageAnimationPolicy() {}
  canGoBack() { return false; } canGoForward() { return false; } goBack() {} goForward() {}
  getPrintersAsync() { return Promise.resolve([]); }
  _destroy() { this.destroyed = true; this.emit('destroyed'); webContentsById.delete(this.id); }
}
WebContents.fromId = id => webContentsById.get(id) || null;
WebContents.fromFrame = f => f && f.wc || null;
WebContents.fromDevToolsTargetId = () => null;
WebContents.getAllWebContents = () => [...webContentsById.values()];
const webContents = { fromId: WebContents.fromId, fromFrame: WebContents.fromFrame, fromDevToolsTargetId: WebContents.fromDevToolsTargetId, getAllWebContents: WebContents.getAllWebContents, getFocusedWebContents: () => { const w = BrowserWindow.getFocusedWindow(); return w ? w.webContents : null; } };

// structured-clone safe args: Uint8Array is kept as it is by
// postMessage; anything else that is not clonable becomes JSON
function marshal(args) { return args; }

class BrowserWindow extends EventEmitter {
  constructor(opts = {}) {
    super();
    this.id = nextWindow++; if (process.env.QUARK_WIN_TRACE) console.error("QWIN " + this.id + " " + JSON.stringify(opts).slice(0, 200) + "\n" + new Error().stack.split("\n").slice(2, 8).join("\n"));
    this.opts = opts;
    this.bounds = { x: opts.x || 0, y: opts.y || 0, width: opts.width || 800, height: opts.height || 600 };
    this.minSize = [opts.minWidth || 0, opts.minHeight || 0];
    this.visible = false; this.destroyed = false; this.focused = false;
    this.maximized = false; this.minimized = false; this.fullscreen = !!opts.fullscreen;
    this.title = opts.title || app.getName();
    this.backgroundColor = opts.backgroundColor || '#1f1f1f';
    this.menuBarVisible = true;
    this.representedFilename = ''; this.documentEdited = false;
    this.webContents = new WebContents(this, opts.webPreferences);
    this.worker = null;
    this.shareFront = -1;
    windows.set(this.id, this);
    // the quark window follows the shown one's size, device px
    const dpr = quark.scale;
    this.cw = Math.round(this.bounds.width * dpr); this.ch = Math.round(this.bounds.height * dpr);
    post(`window ${this.id} ${this.cw} ${this.ch}`);
    this._startWorker();
    app.emit('browser-window-created', {}, this);
    app.emit('web-contents-created', {}, this.webContents);
    if (opts.show !== false) setImmediate(() => this.show());
  }
  _startWorker() {
    const w = new Worker(path.join(here, 'renderer.mjs'), {
      workerData: { id: this.id, w: this.cw, h: this.ch, scale: quark.scale, addon: process.env.QUARK_ADDON, here, appDir, launcher: process.execPath },
      stderr: false, stdout: false,
    });
    this.worker = w;
    w.on('message', m => this._fromWorker(m));
    w.on('error', e => log(`renderer ${this.id} error: ${e && e.stack || e}`));
    w.on('exit', c => { if (!this.destroyed) log(`renderer ${this.id} exited ${c}`); });
  }
  _toWorker(m) { if (this.worker) this.worker.postMessage(m); }
  _fromWorker(m) {
    const wc = this.webContents;
    switch (m.op) {
    case 'share':
      post(`share ${this.id} ${m.w} ${m.h} ${m.fmt}`);
      // one byte carries the fds; the empty line is skipped
      addon.sendFds(3, '\n', [m.fd0, m.fd1]);
      break;
    case 'front': post(`front ${this.id} ${m.n} ${m.cw} ${m.ch}`); break;
    case 'loaded':
      wc.loading = false;
      wc.emit('dom-ready');
      wc.emit('did-finish-load');
      wc.emit('did-navigate', {}, wc.url, 200, 'OK');
      if (!this.readySent) { this.readySent = true; this.emit('ready-to-show'); }
      break;
    case 'title':
      wc.title = m.title; this.title = m.title;
      wc.emit('page-title-updated', { preventDefault() {} }, m.title, false);
      post(`title ${this.id} ${m.title}`);
      break;
    case 'ipc': {
      const ev = new IpcMainEvent(wc, wc.url, m.ports || []);
      for (const id of m.ports || []) portMoveTo(id, 'main', null);
      wc.emit('ipc-message', ev, m.channel, ...m.args);
      wc.ipc.emit(m.channel, ev, ...m.args);
      ipcMain.emit(m.channel, ev, ...m.args);
      break;
    }
    case 'invoke': {
      const ev = new IpcMainEvent(wc, wc.url, []);
      const h = wc.ipc.handlers.get(m.channel) || ipcMain.handlers.get(m.channel);
      const done = (ok, value) => this._toWorker({ op: 'invoke-reply', id: m.id, ok, value: ok ? value : String(value && value.message || value) });
      if (!h) { done(false, `No handler registered for '${m.channel}'`); break; }
      Promise.resolve().then(() => h(ev, ...m.args)).then(v => done(true, v), e => done(false, e));
      break;
    }
    case 'port': hubFrom(m.port, m.data, m.ports || [], 'worker', this.worker); break;
    case 'eval': {
      const e = wc.evals.get(m.id);
      if (!e) break;
      wc.evals.delete(m.id);
      if (m.ok) e.resolve(m.value); else e.reject(new Error(m.value));
      break;
    }
    case 'fetch': protocolFetch(m, this); break;
    case 'open': {
      const r = wc.openHandler ? wc.openHandler({ url: m.url, frameName: '', features: '', disposition: 'new-window' }) : { action: 'allow' };
      if (!r || r.action !== 'deny') shell.openExternal(m.url);
      break;
    }
    case 'console': if (m.level === 'error' || m.level === 'warning' || process.env.QUARK_TRACE) log(`[page ${this.id} ${m.level}] ${String(m.text).slice(0, 600)}`); wc.emit('console-message', { preventDefault() {}, level: m.level, message: m.text, lineNumber: 0, sourceId: '' }, m.level, m.text, 0, ''); break;
    case 'log': log(`[${this.id}] ${m.text}`); break;
    case 'focus-window': this.focus(); break;
    case 'close-window': this.close(); break;
    case 'cursor': post(`cursor ${m.name}`); break;
    case 'clipboard': clipboard.writeText(m.text); break;
    }
  }
  _load(url) {
    let preload = this.opts.webPreferences && this.opts.webPreferences.preload;
    const args = (this.opts.webPreferences && this.opts.webPreferences.additionalArguments) || [];
    this._toWorker({ op: 'load', url, preload, args, bg: this.backgroundColor, zoom: this.opts.webPreferences && this.opts.webPreferences.zoomFactor || 1 });
  }
  _event(line) { this._toWorker({ op: 'event', line }); }
  loadURL(url, o) { return this.webContents.loadURL(url, o); }
  loadFile(f) { return this.webContents.loadFile(f); }
  show() {
    if (this.destroyed) return;
    const was = this.visible;
    this.visible = true;
    for (const w of windows.values()) if (w !== this && w.visible) { w.visible = false; post(`hide ${w.id}`); }
    post(`show ${this.id}`);
    if (!was) this.emit('show');
    this.focus();
  }
  showInactive() { this.show(); }
  hide() { this.visible = false; post(`hide ${this.id}`); this.emit('hide'); }
  isVisible() { return this.visible; }
  focus() {
    for (const w of windows.values()) if (w !== this && w.focused) { w.focused = false; w.emit('blur'); w.webContents.emit('blur'); app.emit('browser-window-blur', {}, w); }
    if (!this.focused) { this.focused = true; this.emit('focus'); this.webContents.emit('focus'); app.emit('browser-window-focus', {}, this); }
    this._toWorker({ op: 'focus', on: true });
  }
  blur() { if (this.focused) { this.focused = false; this.emit('blur'); this._toWorker({ op: 'focus', on: false }); } }
  isFocused() { return this.focused; }
  isDestroyed() { return this.destroyed; }
  close() {
    if (this.destroyed) return;
    let stop = false;
    this.emit('close', { preventDefault() { stop = true; } });
    if (stop) return;
    this.destroy();
  }
  destroy() {
    if (this.destroyed) return;
    this.destroyed = true;
    windows.delete(this.id);
    post(`close ${this.id}`);
    if (this.worker) { this.worker.terminate(); this.worker = null; }
    this.webContents._destroy();
    this.emit('closed');
    if (windows.size === 0) app.emit('window-all-closed');
  }
  getBounds() { return { ...this.bounds }; }
  setBounds(b) { Object.assign(this.bounds, b); this.emit('resize'); this.emit('move'); }
  getContentBounds() { return this.getBounds(); } setContentBounds(b) { this.setBounds(b); }
  getNormalBounds() { return this.getBounds(); }
  getSize() { return [this.bounds.width, this.bounds.height]; }
  setSize(w, h) { this.setBounds({ width: w, height: h }); }
  getContentSize() { return this.getSize(); } setContentSize(w, h) { this.setSize(w, h); }
  getPosition() { return [this.bounds.x, this.bounds.y]; }
  setPosition(x, y) { this.setBounds({ x, y }); }
  center() {}
  getMinimumSize() { return this.minSize; } setMinimumSize(w, h) { this.minSize = [w, h]; }
  getMaximumSize() { return [0, 0]; } setMaximumSize() {}
  isMaximized() { return this.maximized; } maximize() { this.maximized = true; this.emit('maximize'); } unmaximize() { this.maximized = false; this.emit('unmaximize'); }
  isMinimized() { return this.minimized; } minimize() { this.minimized = true; this.emit('minimize'); } restore() { this.minimized = false; this.emit('restore'); }
  isFullScreen() { return this.fullscreen; } setFullScreen(v) { this.fullscreen = v; this.emit(v ? 'enter-full-screen' : 'leave-full-screen'); }
  isSimpleFullScreen() { return false; } setSimpleFullScreen() {}
  isResizable() { return true; } setResizable() {}
  isMovable() { return true; } setMovable() {} setMinimizable() {} setMaximizable() {} setClosable() {} setFullScreenable() {}
  isAlwaysOnTop() { return false; } setAlwaysOnTop() {}
  isKiosk() { return false; } setKiosk() {}
  isModal() { return false; } isNormal() { return !this.maximized && !this.minimized && !this.fullscreen; }
  getTitle() { return this.title; }
  setTitle(t) { this.title = t; post(`title ${this.id} ${t}`); }
  setMenuBarVisibility(v) { this.menuBarVisible = v; } isMenuBarVisible() { return this.menuBarVisible; }
  setAutoHideMenuBar() {} isMenuBarAutoHide() { return false; }
  setMenu(m) { this.menu = m; } removeMenu() { this.menu = null; }
  setBackgroundColor(c) { this.backgroundColor = c; this._toWorker({ op: 'bg', color: c }); }
  getBackgroundColor() { return this.backgroundColor; }
  setRepresentedFilename(f) { this.representedFilename = f; } getRepresentedFilename() { return this.representedFilename; }
  setDocumentEdited(v) { this.documentEdited = v; } isDocumentEdited() { return this.documentEdited; }
  flashFrame() {} setSkipTaskbar() {} setHasShadow() {} hasShadow() { return true; }
  setOpacity() {} getOpacity() { return 1; }
  setTitleBarOverlay() {} setWindowButtonVisibility() {} setWindowButtonPosition() {}
  setSheetOffset() {} setTouchBar() {} setVibrancy() {} setBackgroundMaterial() {}
  setIcon() {} setOverlayIcon() {} setProgressBar() {} setThumbarButtons() {} setAppDetails() {}
  setContentProtection() {} setFocusable() {} setParentWindow() {} getParentWindow() { return null; } getChildWindows() { return []; }
  setEnabled() {} isEnabled() { return true; }
  moveTop() {} moveAbove() {} previewFile() {} closeFilePreview() {}
  addTabbedWindow() {} selectNextTab() {} mergeAllWindows() {} toggleTabBar() {}
  getNativeWindowHandle() { const b = Buffer.alloc(8); b.writeUInt32LE(this.id, 0); return b; }
  setAspectRatio() {} setShape() {}
  getMediaSourceId() { return `window:${this.id}:0`; }
  setAccentColor() {} getAccentColor() { return this.backgroundColor; }
  get contentView() { return { addChildView() {}, removeChildView() {}, children: [] }; }
  // resize from the quark window, device px
  _resized(pw, ph, scale) {
    quark.scale = scale;
    this.cw = pw; this.ch = ph;
    this.bounds.width = Math.round(pw / scale); this.bounds.height = Math.round(ph / scale);
    this._toWorker({ op: 'size', w: pw, h: ph, scale });
    this.emit('resize');
  }
}
BrowserWindow.getAllWindows = () => [...windows.values()];
BrowserWindow.getFocusedWindow = () => [...windows.values()].find(w => w.focused) || null;
BrowserWindow.fromId = id => windows.get(id) || null;
BrowserWindow.fromWebContents = wc => wc && windows.get(wc.id) || null;
BrowserWindow.fromBrowserView = () => null;
const BaseWindow = BrowserWindow;
class WebContentsView extends EventEmitter { constructor(o) { super(); this.webContents = { on() {}, loadURL() {}, isDestroyed: () => false, close() {} }; } setBounds() {} getBounds() { return { x: 0, y: 0, width: 0, height: 0 }; } setVisible() {} setBackgroundColor() {} }
class BrowserView extends WebContentsView {}

// -------------------------------------------------------- protocol
const schemeHandlers = new Map();
const privileged = new Set();
const protocol = {
  registerSchemesAsPrivileged(list) { for (const s of list) privileged.add(s.scheme); },
  handle(scheme, handler) { schemeHandlers.set(scheme, handler); },
  unhandle(scheme) { schemeHandlers.delete(scheme); },
  isProtocolHandled(scheme) { return schemeHandlers.has(scheme); },
  registerFileProtocol(scheme, handler) { schemeHandlers.set(scheme, async req => { const r = await new Promise(res => handler({ url: req.url, method: req.method, headers: {} }, res)); const p = typeof r === 'string' ? r : r.path; return new Response(fs.readFileSync(p)); }); return true; },
  registerStringProtocol(scheme, handler) { schemeHandlers.set(scheme, async req => { const r = await new Promise(res => handler({ url: req.url }, res)); return new Response(typeof r === 'string' ? r : r.data, { headers: { 'content-type': (r && r.mimeType) || 'text/plain' } }); }); return true; },
  registerBufferProtocol(scheme, handler) { return protocol.registerStringProtocol(scheme, handler); },
  unregisterProtocol(scheme) { schemeHandlers.delete(scheme); return true; },
  isProtocolRegistered(scheme) { return schemeHandlers.has(scheme); },
  interceptFileProtocol() { return true; }, uninterceptProtocol() { return true; },
};
// a page's request on a custom scheme: the app's handler answers
async function protocolFetch(m, win) {
  const reply = (status, headers, body) => win._toWorker({ op: 'fetch-reply', id: m.id, status, headers, body }, body ? [body.buffer] : []);
  try {
    const scheme = m.url.split(':')[0];
    const h = schemeHandlers.get(scheme);
    if (!h) { reply(0, {}, null); return; }
    const req = new Request(m.url, { method: m.method || 'GET', headers: m.headers || {} });
    const res = await h(req);
    if (!res) { reply(0, {}, null); return; }
    const headers = {};
    res.headers.forEach((v, k) => { headers[k] = v; });
    const body = new Uint8Array(await res.arrayBuffer());
    reply(res.status, headers, body);
  } catch (e) {
    log(`protocol ${m.url}: ${e && e.stack || e}`);
    reply(0, {}, null);
  }
}

// --------------------------------------------------------- session
class WebRequest { onBeforeRequest() {} onBeforeSendHeaders() {} onSendHeaders() {} onHeadersReceived() {} onResponseStarted() {} onBeforeRedirect() {} onCompleted() {} onErrorOccurred() {} }
class Cookies extends EventEmitter { get() { return Promise.resolve([]); } set() { return Promise.resolve(); } remove() { return Promise.resolve(); } flushStore() { return Promise.resolve(); } }
class Session extends EventEmitter {
  constructor() { super(); this.protocol = protocol; this.webRequest = new WebRequest(); this.cookies = new Cookies(); this.storagePath = app.getPath('userData'); this.serviceWorkers = new EventEmitter(); this.serviceWorkers.getAllRunning = () => ({}); this.serviceWorkers.startWorkerForScope = () => Promise.reject(new Error('no service workers')); }
  setCodeCachePath() {} clearCodeCaches() { return Promise.resolve(); }
  setPermissionRequestHandler() {} setPermissionCheckHandler() {} setDevicePermissionHandler() {}
  setDisplayMediaRequestHandler() {} setBluetoothPairingHandler() {} setUSBProtectedClassesHandler() {}
  setSpellCheckerEnabled() {} isSpellCheckerEnabled() { return false; } setSpellCheckerLanguages() {} getSpellCheckerLanguages() { return []; }
  setProxy() { return Promise.resolve(); } resolveProxy() { return Promise.resolve('DIRECT'); } forceReloadProxyConfig() { return Promise.resolve(); }
  setUserAgent(ua) { this.ua = ua; } getUserAgent() { return this.ua || 'quark'; }
  clearStorageData() { return Promise.resolve(); } clearCache() { return Promise.resolve(); } clearAuthCache() { return Promise.resolve(); } clearHostResolverCache() { return Promise.resolve(); }
  getCacheSize() { return Promise.resolve(0); } flushStorageData() {}
  setCertificateVerifyProc() {} setSSLConfig() {} allowNTLMCredentialsForDomains() {}
  setPreloads(p) { this.preloads = p; } getPreloads() { return this.preloads || []; } registerPreloadScript() {} unregisterPreloadScript() {}
  loadExtension() { return Promise.reject(new Error('no extensions')); } removeExtension() {} getExtension() { return null; } getAllExtensions() { return []; }
  fetch(...a) { return globalThis.fetch(...a); }
  downloadURL() {} createInterruptedDownload() {}
  getStoragePath() { return this.storagePath; } isPersistent() { return true; }
  setDownloadPath() {} enableNetworkEmulation() {} disableNetworkEmulation() {} preconnect() {} closeAllConnections() { return Promise.resolve(); }
  getBlobData() { return Promise.resolve(Buffer.alloc(0)); }
  setCertificateVerifyProc() {}
}
const defaultSession = new Session();
const session = { defaultSession, fromPartition: () => defaultSession, fromPath: () => defaultSession };

// -------------------------------------------------- menus, dialogs
class MenuItem {
  constructor(o = {}) { Object.assign(this, { enabled: true, visible: true, checked: false, type: o.submenu ? 'submenu' : (o.type || 'normal') }, o); if (o.submenu && Array.isArray(o.submenu)) this.submenu = Menu.buildFromTemplate(o.submenu); this.id = o.id; }
}
class Menu {
  constructor() { this.items = []; }
  append(i) { this.items.push(i); } insert(at, i) { this.items.splice(at, 0, i); }
  getMenuItemById(id) { for (const i of this.items) { if (i.id === id) return i; const s = i.submenu && i.submenu.getMenuItemById(id); if (s) return s; } return null; }
  popup(o) { this.closePopup(); if (o && o.callback) setImmediate(o.callback); }
  closePopup() {}
  static buildFromTemplate(t) { const m = new Menu(); for (const e of t) m.append(e instanceof MenuItem ? e : new MenuItem(e)); return m; }
  static setApplicationMenu(m) { Menu.application = m; postMenu(m); }
  static getApplicationMenu() { return Menu.application || null; }
  static sendActionToFirstResponder() {}
}
// the system bar's menu: one line a top entry, titles only for now
function postMenu(m) {
  if (!m) { post('menu'); return; }
  post('menu ' + m.items.filter(i => i.visible !== false).map(i => (i.label || '').replace(/&/g, '').replace(/ /g, ' ')).join(' '));
}
const dialog = {
  showMessageBox(win, o) { if (!o) { o = win; } log(`messageBox: ${o.message || ''}`); return Promise.resolve({ response: o.defaultId || 0, checkboxChecked: false }); },
  showMessageBoxSync(win, o) { if (!o) o = win; log(`messageBox: ${o.message || ''}`); return o.defaultId || 0; },
  showErrorBox(t, c) { log(`error box: ${t}: ${c}`); },
  showOpenDialog(win, o) { return Promise.resolve({ canceled: true, filePaths: [] }); },
  showOpenDialogSync() { return []; },
  showSaveDialog() { return Promise.resolve({ canceled: true, filePath: '' }); },
  showSaveDialogSync() { return undefined; },
  showCertificateTrustDialog() { return Promise.resolve(); },
};
const shell = {
  openExternal(url) { child.spawn('xdg-open', [url], { detached: true, stdio: 'ignore' }).unref(); return Promise.resolve(); },
  openPath(p) { return shell.openExternal(p).then(() => ''); },
  showItemInFolder(p) { shell.openExternal(path.dirname(p)); },
  trashItem(p) { return fs.promises.rm(p, { recursive: true, force: true }); },
  beep() {}, writeShortcutLink() { return false; }, readShortcutLink() { return {}; },
};
const screen = Object.assign(new EventEmitter(), {
  display() { const s = quark.scale, w = Math.round(quark.width / s), h = Math.round(quark.height / s); return { id: 1, label: 'trinity', bounds: { x: 0, y: 0, width: w, height: h }, workArea: { x: 0, y: 0, width: w, height: h }, workAreaSize: { width: w, height: h }, size: { width: w, height: h }, scaleFactor: s, rotation: 0, internal: true, touchSupport: 'unknown', monochrome: false, accelerometerSupport: 'unknown', colorDepth: 24, colorSpace: 'srgb', depthPerComponent: 8, displayFrequency: 60, detected: true, maximumCursorSize: { width: 32, height: 32 }, nativeOrigin: { x: 0, y: 0 } }; },
  getPrimaryDisplay() { return screen.display(); },
  getAllDisplays() { return [screen.display()]; },
  getDisplayMatching() { return screen.display(); },
  getDisplayNearestPoint() { return screen.display(); },
  getCursorScreenPoint() { return { x: 0, y: 0 }; },
  screenToDipPoint(p) { return p; }, dipToScreenPoint(p) { return p; }, screenToDipRect(w, r) { return r; }, dipToScreenRect(w, r) { return r; },
});
const nativeTheme = Object.assign(new EventEmitter(), { shouldUseDarkColors: true, shouldUseHighContrastColors: false, shouldUseInvertedColorScheme: false, inForcedColorsMode: false, prefersReducedTransparency: false, themeSource: 'system' });
const powerMonitor = Object.assign(new EventEmitter(), { getSystemIdleState: () => 'active', getSystemIdleTime: () => 0, isOnBatteryPower: () => false, onBatteryPower: false, getCurrentThermalState: () => 'nominal' });
const systemPreferences = Object.assign(new EventEmitter(), { getMediaAccessStatus: () => 'granted', askForMediaAccess: () => Promise.resolve(true), getUserDefault: () => undefined, setUserDefault() {}, isTrustedAccessibilityClient: () => true, getAccentColor: () => '0078d4ff', getColor: () => '#000000', getAnimationSettings: () => ({ shouldRenderRichAnimation: true, scrollAnimationsEnabledBySystem: true, prefersReducedMotion: false }), accessibilityDisplayShouldReduceTransparency: false });
const clipboard = { text: '', html: '', findText: '', buffers: new Map(),
  readText() { return clipboard.text; }, writeText(t) { clipboard.text = String(t); post(`clipboard ${JSON.stringify(clipboard.text)}`); },
  readHTML() { return clipboard.html; }, writeHTML(h) { clipboard.html = h; },
  readRTF() { return ''; }, writeRTF() {}, readImage() { return nativeImage.createEmpty(); }, writeImage() {},
  readBookmark() { return { title: '', url: '' }; }, writeBookmark() {},
  readFindText() { return clipboard.findText; }, writeFindText(t) { clipboard.findText = t; },
  readBuffer(f) { return clipboard.buffers.get(f) || Buffer.alloc(0); }, writeBuffer(f, b) { clipboard.buffers.set(f, b); },
  has(f) { return f === 'text/plain' ? !!clipboard.text : clipboard.buffers.has(f); }, read(f) { return clipboard.readBuffer(f).toString(); },
  write(d) { if (d.text !== undefined) clipboard.writeText(d.text); if (d.html !== undefined) clipboard.html = d.html; },
  clear() { clipboard.text = ''; clipboard.html = ''; clipboard.buffers.clear(); }, availableFormats() { return clipboard.text ? ['text/plain'] : []; },
};
const safeStorage = Object.assign(new EventEmitter(), {
  isEncryptionAvailable: () => true,
  encryptString: s => Buffer.from('quark:' + s, 'utf8'),
  decryptString: b => { const s = b.toString('utf8'); return s.startsWith('quark:') ? s.slice(6) : s; },
  getSelectedStorageBackend: () => 'basic_text', setUsePlainTextEncryption() {},
});
const nativeImage = {
  createEmpty() { return nativeImage.createFromBuffer(Buffer.alloc(0)); },
  createFromPath(p) { let b = Buffer.alloc(0); try { b = fs.readFileSync(p); } catch {} return nativeImage.createFromBuffer(b); },
  createFromBuffer(b) { return { toPNG: () => b, toJPEG: () => b, toBitmap: () => b, toDataURL: () => 'data:image/png;base64,' + b.toString('base64'), getSize: () => ({ width: 0, height: 0 }), isEmpty: () => b.length === 0, resize() { return this; }, crop() { return this; }, setTemplateImage() {}, isTemplateImage: () => false, getAspectRatio: () => 1, getScaleFactors: () => [1], addRepresentation() {}, getBitmap: () => b }; },
  createFromDataURL(u) { return nativeImage.createFromBuffer(Buffer.from(u.split(',')[1] || '', 'base64')); },
  createFromNamedImage() { return nativeImage.createEmpty(); },
  createThumbnailFromPath() { return Promise.resolve(nativeImage.createEmpty()); },
};
// Electron's net.fetch reads file: urls; node's does not
const MIME = { '.html': 'text/html', '.htm': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.cjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.gif': 'image/gif', '.svg': 'image/svg+xml', '.webp': 'image/webp', '.ico': 'image/x-icon', '.ttf': 'font/ttf', '.otf': 'font/otf', '.woff': 'font/woff', '.woff2': 'font/woff2', '.wasm': 'application/wasm', '.txt': 'text/plain', '.md': 'text/markdown', '.xml': 'text/xml', '.map': 'application/json', '.mp4': 'video/mp4', '.webm': 'video/webm', '.mp3': 'audio/mpeg', '.wav': 'audio/wav', '.pdf': 'application/pdf' };
function mimeOf(p) { return MIME[path.extname(p).toLowerCase()] || 'application/octet-stream'; }
async function netFetch(input, init) {
  const url = typeof input === 'string' ? input : (input && input.url) || String(input);
  if (url.startsWith('file:')) {
    let p;
    try { p = require('node:url').fileURLToPath(url); } catch { return new Response(null, { status: 400 }); }
    try {
      const st = await fs.promises.stat(p);
      if (st.isDirectory()) return new Response(null, { status: 403 });
      return new Response(fs.createReadStream(p), { status: 200, headers: { 'content-type': mimeOf(p), 'content-length': String(st.size) } });
    } catch (e) { return new Response(null, { status: e.code === 'ENOENT' ? 404 : 500 }); }
  }
  const scheme = url.split(':')[0];
  if (schemeHandlers.has(scheme)) { const h = schemeHandlers.get(scheme); const r = await h(new Request(url, init)); return r || new Response(null, { status: 404 }); }
  return globalThis.fetch(input, init);
}
const netModule = {
  fetch: netFetch,
  isOnline: () => true, online: true,
  resolveHost: h => require('node:dns').promises.lookup(h, { all: true }).then(r => ({ endpoints: r.map(x => ({ address: x.address, family: x.family === 6 ? 'ipv6' : 'ipv4' })) })),
  request(o) {
    const https = require('node:https'), http = require('node:http');
    const opts = typeof o === 'string' ? new URL(o) : (o.url ? new URL(o.url) : o);
    const mod = (opts.protocol || 'https:') === 'http:' ? http : https;
    const req = mod.request(opts.href || opts, { method: o.method || 'GET', headers: o.headers });
    return req;
  },
};
const crashReporter = { start() {}, getLastCrashReport: () => null, getUploadedReports: () => [], getUploadToServer: () => false, setUploadToServer() {}, addExtraParameter() {}, removeExtraParameter() {}, getParameters: () => ({}) };
const contentTracing = { startRecording: () => Promise.resolve(), stopRecording: () => Promise.resolve(''), getCategories: () => Promise.resolve([]), getTraceBufferUsage: () => Promise.resolve({ value: 0, percentage: 0 }), enableHeapProfiling() {} };
const desktopCapturer = { getSources: () => Promise.resolve([]) };
const globalShortcut = { register: () => false, registerAll() {}, unregister() {}, unregisterAll() {}, isRegistered: () => false };
const powerSaveBlocker = { ids: 0, start() { return ++this.ids; }, stop() { return true; }, isStarted: () => true };
class Notification extends EventEmitter { constructor(o) { super(); Object.assign(this, o); } show() { log(`notification: ${this.title || ''}`); } close() {} static isSupported() { return false; } }
const autoUpdater = Object.assign(new EventEmitter(), { setFeedURL() {}, getFeedURL: () => '', checkForUpdates() {}, quitAndInstall() {} });
const webFrameMain = { fromId: () => null, fromFrameToken: () => null };
const inAppPurchase = Object.assign(new EventEmitter(), { canMakePayments: () => false });
const pushNotifications = new EventEmitter();
const parentPort = new EventEmitter();
const ShareMenu = class {}; const TouchBar = class { static TouchBarButton = class {}; static TouchBarLabel = class {}; static TouchBarSpacer = class {}; static TouchBarSegmentedControl = class {}; static TouchBarPopover = class {}; static TouchBarScrubber = class {}; static TouchBarColorPicker = class {}; static TouchBarGroup = class {}; static TouchBarSlider = class {}; static TouchBarOtherItemsProxy = class {} };
class Tray extends EventEmitter { setToolTip() {} setContextMenu() {} setImage() {} setTitle() {} destroy() {} isDestroyed() { return false; } }
const Debugger = class {};

// ----------------------------------------------------- utility processes
class UtilityProcess extends EventEmitter {
  constructor(modulePath, args, opts) {
    super();
    opts = opts || {};
    const stdio = opts.stdio === 'pipe' ? ['ignore', 'pipe', 'pipe', 'ipc'] : (Array.isArray(opts.stdio) ? [...opts.stdio, 'ipc'] : ['ignore', 'inherit', 'inherit', 'ipc']);
    const env = { ...(opts.env || process.env), QUARK_UTILITY: '1', QUARK_SERVICE: opts.serviceName || '' };
    const execArgv = ['--import', path.join(here, 'register.mjs'), '--import', path.join(here, 'electron', 'utility.mjs'), ...(opts.execArgv || [])];
    // the child is plain node, never the quark launcher
    if (process.env.QUARK_TRACE) log(`fork ${opts.serviceName}: ${process.env.QUARK_NODE || process.execPath} ${execArgv.join(' ')} ${modulePath} ${(args || []).join(' ')}`);
    this.proc = child.fork(modulePath, args || [], { cwd: opts.cwd, env, execArgv, stdio, serialization: 'advanced', execPath: process.env.QUARK_NODE || process.execPath });
    this.pid = this.proc.pid;
    utilityProcs.add(this.proc); this.proc.once('exit', () => utilityProcs.delete(this.proc));
    this.stdout = this.proc.stdout; this.stderr = this.proc.stderr;
    if (process.env.QUARK_TRACE && this.proc.stderr) this.proc.stderr.on('data', d => log(`[utility ${opts.serviceName}] ${String(d).trim().slice(0, 800)}`));
    if (process.env.QUARK_TRACE && this.proc.stdout) this.proc.stdout.on('data', d => log(`[utility ${opts.serviceName} out] ${String(d).trim().slice(0, 400)}`));
    this.proc.on('spawn', () => this.emit('spawn'));
    this.proc.on('exit', (code, sig) => { this.pid = undefined; this.emit('exit', code === null ? 1 : code); });
    this.proc.on('error', e => { log(`utility ${opts.serviceName}: ${e}`); if (this.listenerCount('error')) this.emit('error', { type: 'FatalError', location: '', report: String(e) }); });
    this.proc.on('message', m => {
      if (m && m.quark === 'port') hubFrom(m.port, m.data, m.ports || [], 'child', this.proc);
      else if (m && m.quark === 'message') this.emit('message', m.data);
    });
  }
  postMessage(message, transfer) {
    const ids = (transfer || []).map(p => p.id);
    for (const id of ids) portMoveTo(id, 'child', this.proc);
    this.proc.send({ quark: 'message', data: message, ports: ids });
  }
  kill() { if (this.proc.exitCode === null) { this.proc.kill(); return true; } return false; }
}
const utilityProcess = { fork: (m, a, o) => new UtilityProcess(m, a, o) };

// ------------------------------------------------------- the socket
function connectQuark() {
  let sock;
  try { sock = new net.Socket({ fd: 3, readable: true, writable: true }); }
  catch (e) { log(`no quark socket on fd 3: ${e}`); return; }
  quark.sock = sock;
  sock.on('data', d => {
    quark.buf += d.toString('utf8');
    let at;
    while ((at = quark.buf.indexOf('\n')) >= 0) {
      const line = quark.buf.slice(0, at); quark.buf = quark.buf.slice(at + 1);
      if (line) fromQuark(line);
    }
  });
  sock.on('error', e => log(`socket: ${e}`));
  sock.on('close', () => { log('quark window closed'); process.exit(0); });
}
function fromQuark(line) {
  const w = line.split(' ');
  const win = windows.get(Number(w[1]));
  switch (w[0]) {
  case 'size':
    if (win) win._resized(Number(w[2]), Number(w[3]), Number(w[4]) || 1);
    quark.width = Number(w[2]); quark.height = Number(w[3]); quark.scale = Number(w[4]) || 1;
    break;
  case 'mouse': case 'wheel': case 'key': case 'text':
    if (win) win._event(line);
    break;
  case 'focus':
    if (win) { if (w[2] === '0') win.blur(); else win.focus(); }
    break;
  case 'closing':
    if (win) win.close(); else app.quit();
    break;
  case 'menu':
    menuPick(w.slice(1).join(' '));
    break;
  default: log(`unknown line from quark: ${line}`);
  }
}
// a menu entry picked in the system bar: path of labels, '>' between
function menuPick(labels) {
  let items = Menu.application && Menu.application.items;
  let item = null;
  for (const l of labels.split('>')) {
    item = (items || []).find(i => (i.label || '').replace(/&/g, '') === l.replace(/ /g, ' '));
    if (!item) return;
    items = item.submenu && item.submenu.items;
  }
  if (item && item.click) item.click(item, BrowserWindow.getFocusedWindow(), {});
}

function _boot(o) {
  appDir = o.appDir; pkg = o.pkg; here = o.here;
  addon = require(process.env.QUARK_ADDON);
  connectQuark();
}
function _mainLoaded() {
  setImmediate(() => app._becomeReady());
}

module.exports = {
  app, BrowserWindow, BaseWindow, BrowserView, WebContentsView, webContents, WebContents, webFrameMain,
  ipcMain, MessageChannelMain, MessagePortMain, protocol, session, Session, Menu, MenuItem, dialog, shell,
  screen, nativeTheme, powerMonitor, systemPreferences, clipboard, safeStorage, nativeImage, net: netModule,
  crashReporter, contentTracing, desktopCapturer, globalShortcut, powerSaveBlocker, Notification, autoUpdater,
  utilityProcess, inAppPurchase, pushNotifications, parentPort, ShareMenu, TouchBar, Tray, Debugger,
  _boot, _mainLoaded,
};
