const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  setMasterBoost: (boostPercent) => ipcRenderer.send('set-master-boost', boostPercent),
  setEqBands: (bandsArray) => ipcRenderer.send('set-eq-bands', bandsArray),
  onBoostStatus: (callback) => ipcRenderer.on('boost-status', (event, response) => callback(response)),
  onHotkeyAction: (callback) => ipcRenderer.on('hotkey-action', (event, action) => callback(action))
});
