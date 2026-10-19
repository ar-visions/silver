// unpack an Electron asar archive: node asar-extract.mjs <file.asar> <dir>
import fs from 'node:fs';
import path from 'node:path';
const [,, file, dir] = process.argv;
const fd = fs.openSync(file, 'r');
const head = Buffer.alloc(16); fs.readSync(fd, head, 0, 16, 0);
const headerSize = head.readUInt32LE(12);
const json = Buffer.alloc(headerSize); fs.readSync(fd, json, 0, headerSize, 16);
const header = JSON.parse(json.toString('utf8'));
const base = 8 + head.readUInt32LE(4);
const unpacked = file + '.unpacked';
function walk(node, rel) {
  for (const [name, entry] of Object.entries(node.files)) {
    const p = path.join(rel, name), out = path.join(dir, p);
    if (entry.files) { fs.mkdirSync(out, { recursive: true }); walk(entry, p); continue; }
    if (entry.link) { try { fs.symlinkSync(entry.link, out); } catch {} continue; }
    fs.mkdirSync(path.dirname(out), { recursive: true });
    if (entry.unpacked) { const src = path.join(unpacked, p); if (fs.existsSync(src)) fs.copyFileSync(src, out); continue; }
    const size = entry.size, buf = Buffer.alloc(size);
    if (size) fs.readSync(fd, buf, 0, size, base + Number(entry.offset));
    fs.writeFileSync(out, buf, { mode: entry.executable ? 0o755 : 0o644 });
  }
}
walk(header, '');
process.stdout.write(`unpacked ${file} into ${dir}\n`);
