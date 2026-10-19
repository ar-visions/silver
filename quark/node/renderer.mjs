// one BrowserWindow's page: it runs here, in a worker; trinity draws
// it through the addon into a canvas shared with the quark window
import { parentPort, workerData } from 'node:worker_threads';
// the module hooks (electron, the app's schemes) in this worker too
import './register.mjs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const gfx = require(workerData.addon);
const say = m => parentPort.postMessage(m);
const state = { id: workerData.id, w: workerData.w, h: workerData.h, scale: workerData.scale,
  canvas: 0, share: null, page: null, dirty: true, bg: [0.12, 0.12, 0.12, 1] };

gfx.gpuInit();
function makeCanvas() {
  if (state.canvas) gfx.canvasFree(state.canvas);
  state.canvas = gfx.canvasNew(state.w, state.h);
  const s = gfx.shareNew(state.canvas);
  if (!s) { say({ op: 'log', text: 'no shareable textures on this GPU' }); return; }
  state.share = s;
  say({ op: 'share', fd0: s.fd0, fd1: s.fd1, fmt: s.fmt, w: s.w, h: s.h });
  state.dirty = true;
}
makeCanvas();

// the page engine (dom/) takes over once loaded; until then a test
// picture proves the frame path
let Page = null;
async function load(m) {
  try {
    const mod = await import('./dom/page.mjs');
    Page = mod.Page;
    state.page = new Page({ gfx, say, width: state.w, height: state.h, scale: state.scale, bg: m.bg, zoom: m.zoom });
    await state.page.load(m.url, m.preload, m.args);
    state.dirty = true;
  } catch (e) {
    say({ op: 'log', text: `page load failed: ${e && e.stack || e}` });
  }
}
function paintTest() {
  const cv = state.canvas;
  gfx.canvasClear(cv, new Float32Array(state.bg));
  gfx.canvasFillRect(cv, 40, 40, 300, 160, new Float32Array([0.3, 0.42, 0.74, 1]), new Float32Array([12, 12, 12, 12]));
  gfx.canvasFillRect(cv, 380, 60, 120, 120, new Float32Array([1, 0.52, 1, 1]), null);
}
// a frame sees one pointer move: the newest (a mouse sends hundreds
// a second, and a page may lay out on each)
function flushMove() {
  const l = state.pendingMove; if (!l) return;
  state.pendingMove = null; state.page.event(l);
}
function frame() {
  if (!state.share) return;
  if (state.page) flushMove();
  let painted = false;
  if (state.page) painted = state.page.tick(state.canvas);
  else if (state.dirty) { paintTest(); painted = true; }
  if (!painted && !state.dirty) return;
  state.dirty = false;
  const n = gfx.sharePublish(state.share.id, state.canvas);
  if (!state.published) { state.published = true; say({ op: 'log', text: `first publish -> front ${n}` }); }
  if (n >= 0) say({ op: 'front', n, cw: state.w, ch: state.h });
}
setInterval(frame, 16);
// QUARK_DUMP=<selector>: the box tree under it, written every 5 s
if (process.env.QUARK_DUMP) setInterval(() => { if (state.page && state.page.loaded) { try { require('node:fs').writeFileSync('/tmp/claude-1000/qk/dump.' + state.id + '.txt', process.env.QUARK_DUMP.split('|').map(q => '== ' + q + '\n' + state.page.dumpBoxes(q)).join('\n')); } catch (e) { say({ op: 'log', text: 'dump: ' + e.message }); } } }, 5000);

parentPort.on('message', m => {
  switch (m.op) {
  case 'load': load(m); break;
  case 'size':
    if (m.w !== state.w || m.h !== state.h || m.scale !== state.scale) {
      state.w = m.w; state.h = m.h; state.scale = m.scale;
      makeCanvas();
      if (state.page) state.page.resize(m.w, m.h, m.scale);
      state.dirty = true;
    }
    break;
  case 'event': if (!state.page) break; if (/^mouse \S+ move /.test(m.line)) state.pendingMove = m.line; else { flushMove(); state.page.event(m.line); } break;
  case 'ipc': if (state.page) state.page.ipc(m.channel, m.args, m.ports || []); break;
  case 'invoke-reply': if (state.page) state.page.invokeReply(m.id, m.ok, m.value); break;
  case 'port': if (state.page) state.page.portMessage(m.port, m.data, m.ports || []); break;
  case 'eval': if (state.page) state.page.eval(m.id, m.code); else say({ op: 'eval', id: m.id, ok: false, value: 'no page' }); break;
  case 'fetch-reply': if (state.page) state.page.fetchReply(m.id, m.status, m.headers, m.body); break;
  case 'zoom': if (state.page) state.page.setZoom(m.factor); break;
  case 'focus': if (state.page) state.page.focus(m.on); break;
  case 'bg': if (state.page) state.page.setBackground(m.color); break;
  }
});
