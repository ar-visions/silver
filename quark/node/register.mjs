// module hooks for this thread: 'electron' is quark's, original-fs is
// fs, an app's own scheme over an absolute path (vscode-file) is that
// file. Synchronous hooks: they reach require(esm) as well as import
import Module from 'node:module';
import { existsSync } from 'node:fs';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';
const here = dirname(fileURLToPath(import.meta.url));
const electronFile = join(here, 'electron', 'index.cjs');
const electronURL = pathToFileURL(electronFile).href;
const ELECTRON = new Set(['electron', 'electron/main', 'electron/common', 'electron/renderer']);
if (process.env.QUARK_TRACE) process.stderr.write(`quark hooks: pid ${process.pid} argv ${process.argv.slice(1, 3).join(' ')}\n`);
Module.registerHooks({
  resolve(specifier, context, next) {
    if (ELECTRON.has(specifier)) return { url: electronURL, shortCircuit: true };
    if (specifier === 'original-fs' || specifier === 'node:original-fs') return { url: 'node:fs', shortCircuit: true };
    if (specifier === 'original-fs/promises' || specifier === 'node:original-fs/promises') return { url: 'node:fs/promises', shortCircuit: true };
    const m = /^([a-z][a-z0-9+.-]*):\/\/[^/]*(\/.*)$/i.exec(specifier);
    if (m && !['file', 'node', 'data', 'http', 'https', 'blob'].includes(m[1].toLowerCase())) {
      const p = decodeURIComponent(m[2].split('?')[0]);
      if (existsSync(p)) return { url: pathToFileURL(p).href, shortCircuit: true };
    }
    return next(specifier, context);
  },
});
// an app's own hooks (VS Code maps fs to node:original-fs for Electron)
// run before ours and short-circuit: their answers are mended here
const fix = r => { if (r && typeof r.url === 'string' && r.url.startsWith('node:original-fs')) r.url = r.url.replace('node:original-fs', 'node:fs'); return r; };
const origRegisterHooks = Module.registerHooks;
Module.registerHooks = function (hooks) {
  if (hooks && typeof hooks.resolve === 'function') {
    const h = hooks.resolve;
    hooks = { ...hooks, resolve(spec, ctx, next) { const r = h(spec, ctx, next); return r && typeof r.then === 'function' ? r.then(fix) : fix(r); } };
  }
  return origRegisterHooks.call(this, hooks);
};
// `import { registerHooks } from 'node:module'` binds the live export
Module.syncBuiltinESMExports();
// require('node:original-fs') is answered before any hook
const origLoad = Module._load;
Module._load = function (request, ...rest) {
  if (request === 'node:original-fs') request = 'fs';
  else if (request === 'node:original-fs/promises') request = 'fs/promises';
  else if (ELECTRON.has(request)) request = electronFile;
  return origLoad.call(this, request, ...rest);
};
