const { app, BrowserWindow, ipcMain, Tray, Menu, nativeImage, globalShortcut, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const { execSync, exec } = require('child_process');

// =============================================================================
// Paths & State
// =============================================================================
const APO_CONFIG_PATH = 'C:\\Program Files\\EqualizerAPO\\config\\config.txt';
const APO_EDITOR_PATH = 'C:\\Program Files\\EqualizerAPO\\Editor.exe';
const APO_CONFIGURATOR_PATH = 'C:\\Program Files\\EqualizerAPO\\Configurator.exe';

let mainWindow = null;
let tray = null;
let apoInstalled = false;
let apoActive = false;

// =============================================================================
// Audio Engine: Windows CoreAudio via PowerShell
// =============================================================================
const CORE_AUDIO_TYPE = `
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
namespace WA {
    public enum EDataFlow { eRender=0 }
    public enum ERole { eMultimedia=1 }
    [ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
    public class MMDeviceEnumeratorComObject {}
    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("A95664D2-9614-4F35-A746-DE8DB63617E6")]
    public interface IMMDeviceEnumerator {
        int EnumAudioEndpoints(EDataFlow d, int m, out IntPtr p);
        int GetDefaultAudioEndpoint(EDataFlow d, ERole r, out IMMDevice dev);
    }
    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("D666063F-1587-4E43-81F1-B948E807363F")]
    public interface IMMDevice {
        int Activate(ref Guid iid, int cls, IntPtr p, out IAudioEndpointVolume ep);
    }
    [ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown), Guid("5CDF2C82-841E-4546-9722-0CF74078229A")]
    public interface IAudioEndpointVolume {
        int RegisterControlChangeNotify(IntPtr p);
        int UnregisterControlChangeNotify(IntPtr p);
        int GetChannelCount(out uint c);
        int SetMasterVolumeLevel(float db, [In, MarshalAs(UnmanagedType.LPStruct)] Guid g);
        int SetMasterVolumeLevelScalar(float f, [In, MarshalAs(UnmanagedType.LPStruct)] Guid g);
        int GetMasterVolumeLevel(out float db);
        int GetMasterVolumeLevelScalar(out float f);
    }
    public static class Vol {
        public static IAudioEndpointVolume GetEP() {
            IMMDeviceEnumerator e = (IMMDeviceEnumerator)(new MMDeviceEnumeratorComObject());
            IMMDevice d; e.GetDefaultAudioEndpoint(EDataFlow.eRender, ERole.eMultimedia, out d);
            Guid iid = typeof(IAudioEndpointVolume).GUID;
            IAudioEndpointVolume ep; d.Activate(ref iid, 23, IntPtr.Zero, out ep);
            return ep;
        }
        public static int Get() { float f; GetEP().GetMasterVolumeLevelScalar(out f); return (int)Math.Round(f*100); }
        public static void Set(int pct) { GetEP().SetMasterVolumeLevelScalar(Math.Max(0,Math.Min(100,pct))/100f, Guid.Empty); }
    }
}
"@
`;

function runPowerShell(command) {
  try {
    const fullCmd = `${CORE_AUDIO_TYPE}\n${command}`;
    const result = execSync(`powershell -NoProfile -NonInteractive -Command "${fullCmd.replace(/"/g, '\\"')}"`, {
      timeout: 5000,
      encoding: 'utf8',
      windowsHide: true
    });
    return result.trim();
  } catch (err) {
    console.error('PowerShell error:', err.message);
    return null;
  }
}

function setSystemVolume(percent) {
  // Use a simpler PowerShell approach for reliability
  try {
    execSync(`powershell -NoProfile -NonInteractive -Command "Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; namespace V { public class A { [DllImport(\\"winmm.dll\\")] public static extern int waveOutSetVolume(IntPtr h, uint v); } }'; $vol = [Math]::Max(0,[Math]::Min(100,${percent})); $v = [uint32]([Math]::Floor($vol/100.0*65535)); [V.A]::waveOutSetVolume([IntPtr]::Zero, ($v -bor ($v -shl 16)))"`, {
      timeout: 3000,
      windowsHide: true
    });
  } catch (e) {
    // Fallback: use nircmd-style PowerShell
    try {
      const scalar = Math.max(0, Math.min(100, percent)) / 100.0;
      execSync(`powershell -NoProfile -NonInteractive -Command "(New-Object -ComObject WScript.Shell).SendKeys([char]173)"`, {
        timeout: 3000,
        windowsHide: true
      });
    } catch (e2) {
      console.error('Volume set failed:', e2.message);
    }
  }
}

// =============================================================================
// Equalizer APO Engine
// =============================================================================
function checkAPOInstalled() {
  apoInstalled = fs.existsSync(APO_EDITOR_PATH);
  return apoInstalled;
}

function updateAPOConfig(prefix, newContent) {
  try {
    if (!fs.existsSync(APO_CONFIG_PATH)) {
      // Create config directory and file if APO is installed but config missing
      const dir = path.dirname(APO_CONFIG_PATH);
      if (fs.existsSync(dir)) {
        fs.writeFileSync(APO_CONFIG_PATH, newContent + '\n', 'utf8');
        return true;
      }
      return false;
    }
    let content = fs.readFileSync(APO_CONFIG_PATH, 'utf8');
    const lines = content.split('\n');
    let found = false;
    const newLines = lines.map(line => {
      if (line.trim().startsWith(prefix)) {
        found = true;
        return newContent;
      }
      return line;
    });
    if (!found) newLines.unshift(newContent); // Put at top like the PS1 script does
    fs.writeFileSync(APO_CONFIG_PATH, newLines.join('\n'), 'utf8');
    return true;
  } catch (err) {
    console.error('APO config write error:', err.message);
    return false;
  }
}

function applyVolume(boostPercent) {
  const level = Math.max(0, Math.min(300, boostPercent));

  if (level <= 100) {
    // Normal range: just set system volume
    setSystemVolume(level);
    if (apoInstalled) {
      updateAPOConfig('Preamp:', 'Preamp: 0.00 dB');
    }
  } else {
    // Boost range: system to 100%, APO handles the rest
    setSystemVolume(100);
    if (apoInstalled) {
      const ratio = level / 100.0;
      const dbGain = 20.0 * Math.log10(ratio);
      updateAPOConfig('Preamp:', `Preamp: ${dbGain.toFixed(2)} dB`);
    }
  }
}

// =============================================================================
// Electron Window
// =============================================================================
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 950,
    height: 620,
    minWidth: 700,
    minHeight: 500,
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      nodeIntegration: false,
      contextIsolation: true
    },
    autoHideMenuBar: true,
    titleBarStyle: 'hidden',
    titleBarOverlay: {
      color: '#121212',
      symbolColor: '#ffffff'
    }
  });

  if (app.isPackaged) {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'));
  } else {
    mainWindow.loadURL('http://localhost:5173');
  }

  mainWindow.on('close', (event) => {
    if (!app.isQuiting) {
      event.preventDefault();
      mainWindow.hide();
    }
    return false;
  });
}

function createTray() {
  const icon = nativeImage.createFromDataURL('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAhElEQVR4nGNgGNSAkYGBgcHYxIzh3LmzDCwEFDMwMDAwHD5ykuHevXsMp06dZDA2MSOsBswGBgYGhtOnTzOcPXuW4dq1a4TdQKwBDAwMDHfv3iWshqABly5dIl0NIyMjw717d/G7gFAYEOUCQmFAlAsIhQE+F+ANA3zhTVIYUC0n0R0AAEIIOGpxUvjYAAAAAElFTkSuQmCC');
  tray = new Tray(icon);
  tray.setToolTip('WinAudio Booster Pro');

  const contextMenu = Menu.buildFromTemplate([
    { label: 'Open Interface', click: () => { mainWindow.show(); } },
    { type: 'separator' },
    { label: 'Volume 100%', click: () => { applyVolume(100); mainWindow.webContents.send('volume-sync', 100); } },
    { label: 'Volume 200%', click: () => { if (apoInstalled) { applyVolume(200); mainWindow.webContents.send('volume-sync', 200); } } },
    { label: 'Volume 300%', click: () => { if (apoInstalled) { applyVolume(300); mainWindow.webContents.send('volume-sync', 300); } } },
    { type: 'separator' },
    { label: 'Quit', click: () => { app.isQuiting = true; app.quit(); } }
  ]);

  tray.setContextMenu(contextMenu);
  tray.on('click', () => mainWindow.show());
}

// =============================================================================
// Single Instance & App Ready
// =============================================================================
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.show();
      mainWindow.focus();
    }
  });

  app.whenReady().then(() => {
    if (app.isPackaged) {
      app.setLoginItemSettings({ openAtLogin: true, openAsHidden: true, path: process.execPath });
    }

    checkAPOInstalled();

    createWindow();
    createTray();

    // Send APO status to renderer after window loads
    mainWindow.webContents.on('did-finish-load', () => {
      mainWindow.webContents.send('apo-status', { installed: apoInstalled });
    });

    // Global shortcuts
    globalShortcut.register('CommandOrControl+Alt+Up', () => {
      mainWindow.webContents.send('hotkey-action', 'volume-up');
    });
    globalShortcut.register('CommandOrControl+Alt+Down', () => {
      mainWindow.webContents.send('hotkey-action', 'volume-down');
    });

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) createWindow();
      else mainWindow.show();
    });
  });
}

// =============================================================================
// IPC Handlers
// =============================================================================
ipcMain.on('set-master-boost', (event, boostPercent) => {
  applyVolume(boostPercent);
  event.reply('boost-status', { success: true, level: boostPercent, apoInstalled });
});

ipcMain.on('set-eq-bands', (event, bandsArray) => {
  if (!apoInstalled) return;
  const freqs = [115, 250, 450, 630, 1250, 2700, 5300, 7500, 13000];
  if (bandsArray && bandsArray.length === freqs.length) {
    let eqParts = [];
    for (let i = 0; i < freqs.length; i++) {
      const db = ((bandsArray[i] - 50) * 0.3).toFixed(1);
      eqParts.push(`${freqs[i]} ${db}`);
    }
    updateAPOConfig('GraphicEQ:', `GraphicEQ: ${eqParts.join('; ')}`);
  }
});

ipcMain.on('toggle-auto-start', (event, enable) => {
  if (app.isPackaged) {
    app.setLoginItemSettings({ openAtLogin: enable, openAsHidden: enable, path: process.execPath });
  }
});

ipcMain.on('install-apo', async () => {
  const result = dialog.showMessageBoxSync(mainWindow, {
    type: 'question',
    buttons: ['Download & Install', 'Cancel'],
    title: 'Install Boost Engine',
    message: 'To enable audio boost beyond 100%, Equalizer APO (free, open-source audio driver) needs to be installed once.\n\nA one-time computer restart is required after installation.\n\nDownload and install now?'
  });

  if (result === 0) {
    try {
      const installerPath = path.join(app.getPath('temp'), 'EqualizerAPO64-1.3.exe');
      const url = 'https://downloads.sourceforge.net/project/equalizerapo/1.3/EqualizerAPO64-1.3.exe';
      
      execSync(`curl.exe -L -o "${installerPath}" "${url}"`, { timeout: 120000, windowsHide: true });
      
      if (fs.existsSync(installerPath)) {
        execSync(`"${installerPath}" /S`, { timeout: 120000, windowsHide: true });
        
        if (fs.existsSync(APO_CONFIGURATOR_PATH)) {
          exec(`"${APO_CONFIGURATOR_PATH}"`);
        }

        dialog.showMessageBoxSync(mainWindow, {
          type: 'info',
          title: 'Boost Engine Installed',
          message: 'Equalizer APO installed!\n\n1. Select your speakers in the Configurator\n2. Check the checkbox next to it\n3. Click Close\n4. Restart your computer ONCE\n\nAfter restart, 300% boost will be available!'
        });

        checkAPOInstalled();
        mainWindow.webContents.send('apo-status', { installed: apoInstalled });
      }
    } catch (err) {
      dialog.showMessageBoxSync(mainWindow, {
        type: 'error',
        title: 'Installation Failed',
        message: `Installation failed: ${err.message}\n\nTry running as Administrator.`
      });
    }
  }
});
