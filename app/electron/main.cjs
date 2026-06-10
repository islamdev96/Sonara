// main.cjs - Sonara (self-contained edition).
//
// Key change vs. the old build: there is NO dependency on Equalizer APO. The
// boost/EQ/effects are done by our OWN engine (BoosterAPO.dll). This process
// only (1) makes sure the engine is installed/attached, and (2) publishes live
// parameters to it through the shared params.bin bridge.
const { app, BrowserWindow, ipcMain, Tray, Menu, nativeImage, globalShortcut, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const { execFile, execFileSync } = require('child_process');
const bridge = require('./paramBridge.cjs');
const licensing = require('./licensing.cjs');

// =============================================================================
// Paths & state
// =============================================================================
const RES = process.resourcesPath || __dirname;
const ENGINE_DLL = app.isPackaged ? path.join(RES, 'engine', 'BoosterAPO.dll')
                                  : path.join(__dirname, '..', '..', 'build', 'Release', 'BoosterAPO.dll');
const INSTALL_PS1 = app.isPackaged ? path.join(RES, 'engine', 'install-engine.ps1')
                                   : path.join(__dirname, '..', '..', 'engine', 'scripts', 'install-engine.ps1');
const UNINSTALL_PS1 = app.isPackaged ? path.join(RES, 'engine', 'uninstall-engine.ps1')
                                     : path.join(__dirname, '..', '..', 'engine', 'scripts', 'uninstall-engine.ps1');
const ENGINE_SYS32 = path.join(process.env.WINDIR || 'C:\\Windows', 'System32', 'BoosterAPO.dll');

let mainWindow = null;
let tray = null;
let engineInstalled = false;

// Current DSP parameters (mirrors src/dsp/Parameters.h units).
const state = {
  enabled: true,
  preampDb: 0,
  outputGainDb: 0,
  eqGainsDb: [0,0,0,0,0,0,0,0,0,0],
  bass: 0, clarity: 0, ambience: 0, surround: 0, dynamic: 0,
  limiterOn: true, limiterCeilingDb: -0.3,
};

// =============================================================================
// Engine lifecycle (no third-party software involved)
// =============================================================================
function checkEngineInstalled() {
  engineInstalled = fs.existsSync(ENGINE_SYS32);
  return engineInstalled;
}

function runElevatedPS(scriptPath, args = []) {
  // Relaunch PowerShell elevated (UAC) to (un)install the engine.
  const psArgs = [
    '-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-Command',
    `Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','${scriptPath}'${args.length ? ",'" + args.join("','") + "'" : ''}`,
  ];
  return new Promise((resolve) => {
    execFile('powershell.exe', psArgs, { windowsHide: true, timeout: 120000 }, (err) => {
      checkEngineInstalled();
      resolve(!err && engineInstalled);
    });
  });
}

// =============================================================================
// boostPercent (UI 0..500) -> engine parameters
// =============================================================================
function boostPercentToPreampDb(percent) {
  const ratio = Math.max(1, percent) / 100.0; // 100% = 0 dB
  return 20.0 * Math.log10(ratio);
}

function publish() {
  // Map the friendly UI model onto the engine parameter struct.
  bridge.publish(state);
}

// =============================================================================
// Window & tray
// =============================================================================
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 980, height: 660, minWidth: 760, minHeight: 540,
    backgroundColor: '#0b0c10',
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      nodeIntegration: false, contextIsolation: true,
    },
    autoHideMenuBar: true,
    titleBarStyle: 'hidden',
    titleBarOverlay: { color: '#0b0c10', symbolColor: '#e6e6e6' },
  });
  if (app.isPackaged) mainWindow.loadFile(path.join(__dirname, '../dist/index.html'));
  else mainWindow.loadURL('http://localhost:5173');

  mainWindow.on('close', (e) => {
    if (!app.isQuiting) { e.preventDefault(); mainWindow.hide(); }
  });
}

function pushStatus() {
  if (!mainWindow) return;
  mainWindow.webContents.send('engine-status', { installed: engineInstalled });
  mainWindow.webContents.send('license-status', licensing.status());
}

function createTray() {
  const icon = nativeImage.createFromDataURL('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAhElEQVR4nGNgGNSAkYGBgcHYxIzh3LmzDCwEFDMwMDAwHD5ykuHevXsMp06dZDA2MSOsBswGBgYGhtOnTzOcPXuW4dq1a4TdQKwBDAwMDHfv3iWshqABly5dIl0NIyMjw717d/G7gFAYEOUCQmFAlAsIhQE+F+ANA3zhTVIYUC0n0R0AAEIIOGpxUvjYAAAAAElFTkSuQmCC');
  tray = new Tray(icon);
  tray.setToolTip('Sonara');
  tray.setContextMenu(Menu.buildFromTemplate([
    { label: 'Open', click: () => mainWindow.show() },
    { type: 'separator' },
    { label: 'Bypass (toggle)', click: () => { state.enabled = !state.enabled; publish(); } },
    { type: 'separator' },
    { label: 'Quit', click: () => { app.isQuiting = true; app.quit(); } },
  ]));
  tray.on('click', () => mainWindow.show());
}

// =============================================================================
// App ready (single instance)
// =============================================================================
if (!app.requestSingleInstanceLock()) { app.quit(); }
else {
  app.on('second-instance', () => { if (mainWindow) { mainWindow.show(); mainWindow.focus(); } });
  app.whenReady().then(() => {
    if (app.isPackaged) app.setLoginItemSettings({ openAtLogin: true, openAsHidden: true });
    checkEngineInstalled();
    publish();
    createWindow();
    createTray();
    mainWindow.webContents.on('did-finish-load', pushStatus);

    globalShortcut.register('CommandOrControl+Alt+Up',   () => mainWindow.webContents.send('hotkey', 'up'));
    globalShortcut.register('CommandOrControl+Alt+Down', () => mainWindow.webContents.send('hotkey', 'down'));
    globalShortcut.register('CommandOrControl+Alt+B',    () => { state.enabled = !state.enabled; publish(); });

    app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); else mainWindow.show(); });
  });
}
app.on('will-quit', () => globalShortcut.unregisterAll());

// =============================================================================
// IPC
// =============================================================================
ipcMain.handle('get-status', () => ({ installed: engineInstalled, license: licensing.status() }));

ipcMain.on('set-params', (_e, partial) => {
  // Clamp boost to the licensed maximum.
  const lic = licensing.status();
  if (typeof partial.boostPercent === 'number') {
    const capped = Math.min(partial.boostPercent, lic.maxBoostPercent);
    state.preampDb = boostPercentToPreampDb(capped);
  }
  if (Array.isArray(partial.eqGainsDb)) state.eqGainsDb = partial.eqGainsDb.slice(0, 10);
  for (const k of ['bass','clarity','ambience','surround','dynamic','outputGainDb','limiterCeilingDb']) {
    if (typeof partial[k] === 'number') state[k] = partial[k];
  }
  if (typeof partial.enabled === 'boolean') state.enabled = partial.enabled;
  if (typeof partial.limiterOn === 'boolean') state.limiterOn = partial.limiterOn;
  publish();
});

ipcMain.handle('install-engine', async () => {
  const res = dialog.showMessageBoxSync(mainWindow, {
    type: 'question', buttons: ['Install Engine', 'Cancel'], defaultId: 0,
    title: 'Install Sonara Engine',
    message: 'This installs the built-in audio engine (no third-party software). You will see a Windows admin prompt, and audio will briefly restart. Continue?',
  });
  if (res !== 0) return { installed: engineInstalled };
  await runElevatedPS(INSTALL_PS1, [ENGINE_DLL]);
  pushStatus();
  return { installed: engineInstalled };
});

ipcMain.handle('uninstall-engine', async () => {
  await runElevatedPS(UNINSTALL_PS1);
  pushStatus();
  return { installed: engineInstalled };
});

ipcMain.handle('activate-license', (_e, key) => {
  const r = licensing.activate(key);
  pushStatus();
  return r;
});
ipcMain.handle('deactivate-license', () => { licensing.deactivate(); pushStatus(); return licensing.status(); });
ipcMain.on('open-buy', () => shell.openExternal('https://sonara.app/buy'));
ipcMain.on('toggle-autostart', (_e, enable) => {
  if (app.isPackaged) app.setLoginItemSettings({ openAtLogin: !!enable, openAsHidden: !!enable });
});
