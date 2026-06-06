const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  setMasterBoost: (boostPercent) => ipcRenderer.send('set-master-boost', boostPercent),
  setEqBands: (bandsArray) => ipcRenderer.send('set-eq-bands', bandsArray),
  toggleAutoStart: (enable) => ipcRenderer.send('toggle-auto-start', enable),
  installAPO: () => ipcRenderer.send('install-apo'),
  onBoostStatus: (callback) => ipcRenderer.on('boost-status', (event, data) => callback(data)),
  onHotkeyAction: (callback) => ipcRenderer.on('hotkey-action', (event, action) => callback(action)),
  onAPOStatus: (callback) => ipcRenderer.on('apo-status', (event, data) => callback(data)),
  onVolumeSync: (callback) => ipcRenderer.on('volume-sync', (event, level) => callback(level))
});
