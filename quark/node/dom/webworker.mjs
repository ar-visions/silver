// a page's Worker: a node worker with a web worker's surface
import { parentPort, workerData } from 'node:worker_threads';
// the hooks are per thread: the app's schemes resolve here too
import '../register.mjs';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import vm from 'node:vm';
const g = globalThis;
const listeners = new Map();
const target = { addEventListener(t, f) { let l = listeners.get(t); if (!l) { l = []; listeners.set(t, l); } l.push(f); }, removeEventListener(t, f) { const l = listeners.get(t); if (l) { const i = l.indexOf(f); if (i >= 0) l.splice(i, 1); } }, dispatchEvent(ev) { const l = listeners.get(ev.type) || []; for (const f of l.slice()) { try { typeof f === 'function' ? f(ev) : f.handleEvent(ev); } catch (e) { report(e); } } const h = g['on' + ev.type]; if (typeof h === 'function') { try { h(ev); } catch (e) { report(e); } } return true; } };
function report(e) { parentPort.postMessage({ __quark: 'error', text: String(e && e.stack || e) }); const ev = { type: 'error', message: String(e && e.message || e), error: e, filename: '', lineno: 0, colno: 0, preventDefault() {} }; target.dispatchEvent(ev); }
class MessageEventLike { constructor(data, ports) { this.type = 'message'; this.data = data; this.ports = ports || []; this.origin = ''; this.source = null; this.lastEventId = ''; } preventDefault() {} stopPropagation() {} }
Object.assign(g, { self: g, window: undefined, name: workerData.name || '', location: new URL(workerData.url), origin: new URL(workerData.url).origin, isSecureContext: true, crossOriginIsolated: false,
  addEventListener: target.addEventListener, removeEventListener: target.removeEventListener, dispatchEvent: target.dispatchEvent,
  postMessage: (data, transfer) => parentPort.postMessage({ data }, Array.isArray(transfer) ? transfer : (transfer && transfer.transfer) || []),
  close: () => process.exit(0), importScripts: (...urls) => { for (const u of urls) { const url = new URL(u, workerData.url); const code = readFileSync(fileURLToPath(toFile(url)), 'utf8'); vm.runInThisContext(code, { filename: url.href }); } },
  __navigator: { userAgent: workerData.userAgent, platform: 'Linux x86_64', language: 'en-US', languages: ['en-US', 'en'], hardwareConcurrency: 8, onLine: true, locks: { request: (n, o, cb) => (cb || o)({}) } },
  requestAnimationFrame: cb => setTimeout(() => cb(performance.now()), 16), cancelAnimationFrame: id => clearTimeout(id),
  requestIdleCallback: cb => setTimeout(() => cb({ didTimeout: false, timeRemaining: () => 10 }), 1), cancelIdleCallback: id => clearTimeout(id),
  WorkerGlobalScope: Object, DedicatedWorkerGlobalScope: Object, reportError: report,
  fetch: (u, o) => g.__nodeFetch(workerData.url ? new URL(String(u), workerData.url).href : u, o),
});
// node's own navigator is a getter: replace it, not assign
Object.defineProperty(g, 'navigator', { value: g.__navigator, configurable: true, writable: true }); delete g.__navigator;
g.__nodeFetch = globalThis.fetch;
g.fetch = async (u, o) => { const url = new URL(String(u instanceof Request ? u.url : u), workerData.url); if (url.protocol === 'file:' || workerData.roots.some(r => url.protocol === r + ':')) { const p = fileURLToPath(toFile(url)); const b = readFileSync(p); return new Response(b, { status: 200, headers: { 'content-type': mimeOf(p) } }); } return g.__nodeFetch(url.href, o); };
function toFile(url) { if (url.protocol === 'file:') return url; return new URL('file://' + url.pathname); }
function mimeOf(p) { return p.endsWith('.js') || p.endsWith('.mjs') ? 'text/javascript' : p.endsWith('.json') ? 'application/json' : p.endsWith('.wasm') ? 'application/wasm' : 'application/octet-stream'; }
parentPort.on('message', m => {
  if (m && m.__quark === 'terminate') process.exit(0);
  target.dispatchEvent(new MessageEventLike(m.data, m.ports));
});
process.on('uncaughtException', report);
process.on('unhandledRejection', e => { const ev = { type: 'unhandledrejection', reason: e, promise: null, preventDefault() {} }; target.dispatchEvent(ev); });
try {
  const url = new URL(workerData.url);
  const blob = url.protocol === 'blob:' || url.protocol === 'data:';
  if (workerData.type === 'module' && blob) await import('data:text/javascript;base64,' + Buffer.from(workerData.code).toString('base64'));
  else if (workerData.type === 'module') await import(url.protocol === 'file:' ? url.href : toFile(url).href);
  else if (blob) vm.runInThisContext(workerData.code, { filename: 'worker.js' });
  else { const code = readFileSync(fileURLToPath(toFile(url)), 'utf8'); vm.runInThisContext(code, { filename: url.href }); }
} catch (e) { report(e); }
