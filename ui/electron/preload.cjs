const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  setMasterBoost: (boostPercent) => ipcRenderer.send('set-master-boost', boostPercent),
  onBoostStatus: (callback) => ipcRenderer.on('boost-status', (event, response) => callback(response))
});
