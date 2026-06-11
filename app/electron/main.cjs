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
const statusBridge = require('./statusBridge.cjs');
const licensing = require('./licensing.cjs');

// =============================================================================
// Paths & state
// =============================================================================
const RES = process.resourcesPath || __dirname;
const HOST_EXE = app.isPackaged ? path.join(RES, 'engine', 'SonaraHost.exe')
                                : path.join(__dirname, '..', '..', 'build', 'Release', 'SonaraHost.exe');
const ENGINE_DLL = app.isPackaged ? path.join(RES, 'engine', 'BoosterAPO.dll')
                                  : path.join(__dirname, '..', '..', 'build', 'Release', 'BoosterAPO.dll');
const INSTALL_PS1 = app.isPackaged ? path.join(RES, 'engine', 'install-engine.ps1')
                                   : path.join(__dirname, '..', '..', 'engine', 'scripts', 'install-engine.ps1');
const UNINSTALL_PS1 = app.isPackaged ? path.join(RES, 'engine', 'uninstall-engine.ps1')
                                     : path.join(__dirname, '..', '..', 'engine', 'scripts', 'uninstall-engine.ps1');

let hostProcess = null;

function startHost() {
  if (hostProcess) return;
  try {
    const output = execFileSync('tasklist.exe', ['/nh', '/fi', 'imagename eq SonaraHost.exe'], { encoding: 'utf8', windowsHide: true });
    if (output.toLowerCase().includes('sonarahost.exe')) {
      console.log("[Host] SonaraHost.exe is already running.");
      return;
    }
  } catch (e) {}

  console.log("[Host] Starting SonaraHost.exe...");
  try {
    const { spawn } = require('child_process');
    hostProcess = spawn(HOST_EXE, [], {
      detached: true,
      stdio: 'pipe',
      windowsHide: true
    });
    hostProcess.stdout.on('data', (data) => console.log(`[Host] ${data.toString().trim()}`));
    hostProcess.stderr.on('data', (data) => console.error(`[Host Error] ${data.toString().trim()}`));
    hostProcess.unref();
    console.log("[Host] SonaraHost.exe spawned.");
  } catch (e) {
    console.error("[Host] Failed to spawn SonaraHost.exe:", e);
  }
}

function stopHost() {
  if (hostProcess) {
    try {
      hostProcess.kill('SIGINT');
    } catch (e) {}
    hostProcess = null;
  }
  try {
    // Try gentle taskkill first to trigger the C++ console control handler
    execFileSync('taskkill.exe', ['/im', 'SonaraHost.exe'], { stdio: 'ignore', windowsHide: true });
    // Brief sleep to allow device restoration to complete
    execFileSync('powershell.exe', ['-Command', 'Start-Sleep -Milliseconds 250'], { stdio: 'ignore', windowsHide: true });
  } catch (e) {}
  try {
    // Force kill fallback
    execFileSync('taskkill.exe', ['/f', '/im', 'SonaraHost.exe'], { stdio: 'ignore', windowsHide: true });
  } catch (e) {}

  // Restore the original audio device from Electron fallback!
  const ORIG_DEV_FILE = path.join(process.env.ProgramData || 'C:\\ProgramData', 'WinAudioBoosterPro', 'origdev.txt');
  if (fs.existsSync(ORIG_DEV_FILE)) {
    try {
      const origId = fs.readFileSync(ORIG_DEV_FILE, 'utf8').trim();
      if (origId) {
        console.log(`[Host] Restoring original default audio device to: ${origId}`);
        const psScript = `
          $c = @"
          using System;
          using System.Runtime.InteropServices;
          public enum ERole : uint { eConsole = 0, eMultimedia = 1, eCommunications = 2 }
          [Guid("870AF99C-171D-4F9E-AF0D-E63DF40C2BC9"), ComImport]
          internal class CPolicyConfigClient { }
          [Guid("F8679F50-850A-41CF-9C72-430F290290C8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
          internal interface IPolicyConfig {
              void GetMixFormat(); void GetDeviceFormat(); void ResetDefaultDRVM(); void GetProcessingPeriod(); void SetProcessingPeriod();
              void GetShareMode(); void SetShareMode(); void GetPropertyValue(); void SetPropertyValue();
              void SetDefaultEndpoint(string wszDeviceId, ERole role); void SetEndpointVisibility();
          }
          public class Switcher {
              public static void SetDefault(string id) {
                  IPolicyConfig c = (IPolicyConfig)new CPolicyConfigClient();
                  c.SetDefaultEndpoint(id, ERole.eConsole);
                  c.SetDefaultEndpoint(id, ERole.eMultimedia);
                  c.SetDefaultEndpoint(id, ERole.eCommunications);
              }
          }
          "@
          Add-Type -TypeDefinition $c -ErrorAction SilentlyContinue
          [Switcher]::SetDefault('${origId}')
        `;
        execFileSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', psScript], { stdio: 'ignore', windowsHide: true });
      }
      fs.unlinkSync(ORIG_DEV_FILE);
    } catch (err) {
      console.error("[Host] Failed to restore default audio device on exit:", err);
    }
  }
}

let mainWindow = null;
let tray = null;
let engineInstalled = false;
let engineActive = false;

// Current DSP parameters (mirrors src/dsp/Parameters.h units).
const state = {
  enabled: true,
  preampDb: 0,
  outputGainDb: 0,
  eqGainsDb: [0,0,0,0,0,0,0,0,0,0],
  bass: 0, clarity: 0, ambience: 0, surround: 0, dynamic: 0,
  limiterOn: true, limiterCeilingDb: -1.0,
};

// =============================================================================
// Engine lifecycle (no third-party software involved)
// =============================================================================
function checkEngineInstalled() {
  engineInstalled = fs.existsSync(HOST_EXE);
  if (engineInstalled) {
    const status = statusBridge.readStatus();
    if (status && status.isAlive) {
      engineActive = true;
    } else {
      try {
        const output = execFileSync('tasklist.exe', ['/nh', '/fi', 'imagename eq SonaraHost.exe'], { encoding: 'utf8', windowsHide: true });
        engineActive = output.toLowerCase().includes('sonarahost.exe');
      } catch (e) {
        engineActive = false;
      }
    }
  } else {
    engineActive = false;
  }
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
// boostPercent (UI 0..500) -> engine preampDb.
// Design: this is a BOOST-ONLY control. Values below 100% clamp to 0 dB
// (unity gain) so the slider can never attenuate system volume — the user
// expects a "volume booster", not a volume fader.
function boostPercentToPreampDb(percent) {
  const p = Math.max(0, Math.min(500, percent));
  if (p <= 0) return -80.0;
  if (p <= 100) return 20.0 * Math.log10(p / 100.0);
  const over = (p - 100) / 400; // 0..1
  const maxDb = 12.0;
  return over * maxDb;
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
  mainWindow.webContents.send('engine-status', { installed: engineInstalled, active: engineActive });
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
    startHost();
    if (app.isPackaged) app.setLoginItemSettings({ openAtLogin: true, openAsHidden: true });
    checkEngineInstalled();
    publish();
    createWindow();
    createTray();
    mainWindow.webContents.on('did-finish-load', pushStatus);

    // Periodically poll engine status to detect when audiodg loads the APO
    setInterval(() => {
      const prevInstalled = engineInstalled;
      const prevActive = engineActive;
      checkEngineInstalled();
      if (engineInstalled !== prevInstalled || engineActive !== prevActive) {
        pushStatus();
      }
    }, 3000);

    // Fast polling: push audio levels to the renderer for the VU meter (~200ms).
    setInterval(() => {
      if (!mainWindow || !engineActive) return;
      const status = statusBridge.readStatus();
      if (status && status.isAlive) {
        mainWindow.webContents.send('engine-levels', {
          rmsLeft: status.rmsLeft,
          rmsRight: status.rmsRight,
          peakLeft: status.peakLeft,
          peakRight: status.peakRight,
          sampleRate: status.sampleRate,
          channels: status.channels,
        });
      }
    }, 200);

    globalShortcut.register('CommandOrControl+Alt+Up',   () => mainWindow.webContents.send('hotkey', 'up'));
    globalShortcut.register('CommandOrControl+Alt+Down', () => mainWindow.webContents.send('hotkey', 'down'));
    globalShortcut.register('CommandOrControl+Alt+B',    () => { state.enabled = !state.enabled; publish(); });

    app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); else mainWindow.show(); });
  });
}
app.on('will-quit', () => {
  stopHost();
  globalShortcut.unregisterAll();
});

// =============================================================================
// IPC
// =============================================================================
ipcMain.handle('get-status', () => ({ installed: engineInstalled, active: engineActive, license: licensing.status() }));

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
