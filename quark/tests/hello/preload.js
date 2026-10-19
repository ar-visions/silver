const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('hello', {
  add: (a, b) => ipcRenderer.invoke('hello:add', a, b),
  title: t => ipcRenderer.send('hello:title', t),
});
