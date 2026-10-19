// a utilityProcess child: process.parentPort over the ipc channel;
// ports are ids the main hub routes
import { EventEmitter } from 'node:events';
// the main process gone: this child goes with it
const ppid = process.ppid; setInterval(() => { if (process.ppid !== ppid) process.exit(0); }, 1000).unref();
const ports = new Map();
class ChildPort extends EventEmitter {
  constructor(id) { super(); this.id = id; this.queue = []; this.started = false; }
  postMessage(message, transfer) {
    process.send({ quark: 'port', port: this.id, data: message, ports: (transfer || []).map(p => p.id) });
  }
  start() { this.started = true; for (const m of this.queue) this.emit('message', m); this.queue = []; }
  close() { ports.delete(this.id); }
  _deliver(ev) { if (this.started || this.listenerCount('message')) this.emit('message', ev); else this.queue.push(ev); }
}
function portOf(id) { let p = ports.get(id); if (!p) { p = new ChildPort(id); ports.set(id, p); } return p; }
const parentPort = new EventEmitter();
parentPort.postMessage = (message, transfer) => process.send({ quark: 'message', data: message, ports: (transfer || []).map(p => p.id) });
parentPort.start = () => {};
process.on('message', m => {
  if (!m || !m.quark) return;
  const ev = { data: m.data, ports: (m.ports || []).map(portOf) };
  if (m.quark === 'message') parentPort.emit('message', ev);
  else if (m.quark === 'port') portOf(m.port)._deliver(ev);
});
process.parentPort = parentPort;
process.type = 'utility';
process.versions.electron = '43.7.7';
process.versions.chrome = '0';
if (process.env.QUARK_LAUNCHER) process.execPath = process.env.QUARK_LAUNCHER;
