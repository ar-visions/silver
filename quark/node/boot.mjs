// the main process: node boot.mjs <app folder> [the app's args]
import { readFileSync } from 'node:fs';
import { join, resolve, dirname } from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';
const here = dirname(fileURLToPath(import.meta.url));
const appDir = resolve(process.argv[2]);
const pkg = JSON.parse(readFileSync(join(appDir, 'package.json'), 'utf8'));
const launcher = process.env.QUARK_LAUNCHER || process.execPath;
// children fork the real node, not the launcher
process.env.QUARK_NODE = process.execPath;
// Electron's argv: the program, then the user's words
process.argv = [launcher, ...process.argv.slice(3)];
process.execPath = launcher;
process.versions.electron = '43.7.7';
process.versions.chrome = '0';
process.type = 'browser';
process.resourcesPath = dirname(appDir);
process.defaultApp = false;
process.noAsar = false;
process.on('uncaughtException', e => {
  process.stderr.write(`quark: main uncaught: ${e && e.stack || e}\n`);
});
const require = createRequire(import.meta.url);
const electron = require('./electron/index.cjs');
electron._boot({ appDir, pkg, here });
const main = pkg.main || 'index.js';
await import(pathToFileURL(join(appDir, main)).href);
electron._mainLoaded();
