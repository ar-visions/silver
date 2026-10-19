// the smallest Electron app: one window, a page, an ipc round trip
const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
ipcMain.handle('hello:add', (ev, a, b) => a + b);
ipcMain.on('hello:title', (ev, t) => { BrowserWindow.fromWebContents(ev.sender).setTitle(t); });
app.whenReady().then(() => {
  const win = new BrowserWindow({ width: 800, height: 600, webPreferences: { preload: path.join(__dirname, 'preload.js') } });
  win.loadFile('index.html');
});
app.on('window-all-closed', () => app.quit());
