// render a page without the window: node tests/render.mjs <file.html> <out.png> [w h]
import { createRequire } from 'node:module';
import { pathToFileURL } from 'node:url';
import { writeFileSync } from 'node:fs';
import { deflateSync } from 'node:zlib';
import path from 'node:path';
const require = createRequire(import.meta.url);
const gfx = require(process.env.QUARK_ADDON || '/src/silver/install/lib/quark/quark.node');
const [,, file, out, W = '800', H = '600'] = process.argv;
const w = Number(W), h = Number(H);
gfx.gpuInit();
const canvas = gfx.canvasNew(w, h);
const { Page } = await import('../node/dom/page.mjs');
const logs = [];
// a stand-in main process: invokes add their numbers, ipc sets the title
let page;
const say = m => {
  if (m.op === 'log' || m.op === 'console') logs.push(`${m.op}: ${m.text}`);
  else if (m.op === 'invoke') setTimeout(() => page.invokeReply(m.id, true, m.args.reduce((a, b) => a + b, 0)), 5);
  else if (m.op === 'ipc') logs.push(`ipc ${m.channel}: ${JSON.stringify(m.args)}`);
  else if (m.op === 'title') logs.push(`title: ${m.title}`);
};
page = new Page({ gfx, say, width: w, height: h, scale: 1, bg: '#ffffff', zoom: 1 });
const preload = process.env.QUARK_PRELOAD || null;
await page.load(pathToFileURL(path.resolve(file)).href, preload, []);
for (let i = 0; i < 5; i++) { page.tick(canvas); await new Promise(r => setTimeout(r, 30)); }
page.tick(canvas);
if (process.env.QUARK_HIT) { const [x, y] = process.env.QUARK_HIT.split(',').map(Number); const e = page.elementAt(x, y); process.stdout.write(`hit ${x},${y}: ${e && (e.localName + (e.id ? '#' + e.id : '') + (e.className ? '.' + e.className : ''))}\n`); }
if (process.env.QUARK_CLICK) { const [x, y] = process.env.QUARK_CLICK.split(',').map(Number); page.event(`mouse 1 move ${x} ${y} 0 0 0`); page.event(`mouse 1 down ${x} ${y} 0 1 0`); page.event(`mouse 1 up ${x} ${y} 0 0 0`); for (let i = 0; i < 5; i++) { page.tick(canvas); await new Promise(r => setTimeout(r, 30)); } const o = page.document.getElementById('out'); process.stdout.write(`out: ${o && o.textContent} title: ${page.document.title}\n`); }
if (process.env.QUARK_EVAL) { const vm = await import('node:vm'); process.stdout.write('eval: ' + JSON.stringify(vm.runInThisContext(process.env.QUARK_EVAL)) + '\n'); }
if (process.env.QUARK_DUMP) process.stdout.write(page.dumpBoxes(process.env.QUARK_DUMP === '1' ? '' : process.env.QUARK_DUMP) + '\n');
const px = Buffer.alloc(w * h * 4);
gfx.canvasRead(canvas, px);
writeFileSync(out, png(px, w, h));
const origOut = process.stdout; for (const l of logs) origOut.write(l + "\n");
process.stdout.write(`wrote ${out}\n`);
process.exit(0);
function png(rgba, w, h) {
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4); }
  const chunks = [Buffer.from([137, 80, 78, 71, 13, 10, 26, 10])];
  const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td) >>> 0); chunks.push(len, td, crc); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 6;
  chunk('IHDR', ihdr); chunk('IDAT', deflateSync(raw)); chunk('IEND', Buffer.alloc(0));
  return Buffer.concat(chunks);
}
function crc32(b) { let c, crc = 0xffffffff; for (let n = 0; n < b.length; n++) { c = (crc ^ b[n]) & 0xff; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crc = (crc >>> 8) ^ c; } return crc ^ 0xffffffff; }
