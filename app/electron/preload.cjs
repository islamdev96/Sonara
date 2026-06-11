const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  // Live engine control
  setParams: (partial) => ipcRenderer.send('set-params', partial),
  getStatus: () => ipcRenderer.invoke('get-status'),

  // Self-contained engine lifecycle (no third-party software)
  installEngine: () => ipcRenderer.invoke('install-engine'),
  uninstallEngine: () => ipcRenderer.invoke('uninstall-engine'),

  // Commercial licensing
  activateLicense: (key) => ipcRenderer.invoke('activate-license', key),
  deactivateLicense: () => ipcRenderer.invoke('deactivate-license'),
  openBuy: () => ipcRenderer.send('open-buy'),

  toggleAutostart: (enable) => ipcRenderer.send('toggle-autostart', enable),

  // Events
  onEngineStatus: (cb) => ipcRenderer.on('engine-status', (_e, d) => cb(d)),
  onEngineLevels: (cb) => ipcRenderer.on('engine-levels', (_e, d) => cb(d)),
  onLicenseStatus: (cb) => ipcRenderer.on('license-status', (_e, d) => cb(d)),
  onHotkey: (cb) => ipcRenderer.on('hotkey', (_e, d) => cb(d)),
});
