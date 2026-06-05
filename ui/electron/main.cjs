const { app, BrowserWindow, ipcMain, Tray, Menu, nativeImage, globalShortcut } = require('electron');
const path = require('path');
const fs = require('fs');

const APO_CONFIG_PATH = 'C:\\Program Files\\EqualizerAPO\\config\\config.txt';

let mainWindow = null;
let tray = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 900,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      nodeIntegration: false,
      contextIsolation: true
    },
    autoHideMenuBar: true,
    titleBarStyle: 'hidden',
    titleBarOverlay: {
      color: '#1e1e1e',
      symbolColor: '#ffffff'
    }
  });

  if (app.isPackaged) {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'));
  } else {
    mainWindow.loadURL('http://localhost:5173');
  }

  // Prevent closing, hide instead for system tray
  mainWindow.on('close', (event) => {
    if (!app.isQuiting) {
      event.preventDefault();
      mainWindow.hide();
    }
    return false;
  });
}

function createTray() {
  // Simple transparent 1x1 icon for the tray
  const icon = nativeImage.createFromDataURL('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAAZdEVYdFNvZnR3YXJlAFBhaW50Lk5FVCB2My41LjbQg61aAAAADUlEQVQYV2P4//8/AwAI/AL+X2YQxgAAAABJRU5ErkJggg==');
  tray = new Tray(icon);
  tray.setToolTip('WinAudio Booster Pro');

  const contextMenu = Menu.buildFromTemplate([
    { label: 'Open Interface', click: () => { mainWindow.show(); } },
    { type: 'separator' },
    { label: 'Quit', click: () => {
        app.isQuiting = true;
        app.quit();
      }
    }
  ]);
  
  tray.setContextMenu(contextMenu);
  tray.on('click', () => {
    mainWindow.show();
  });
}

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
    // Configure launch on startup
    if (app.isPackaged) {
      app.setLoginItemSettings({
        openAtLogin: true,
        openAsHidden: true,
        path: process.execPath
      });
    }

    createWindow();
    createTray();

    // Register Global Shortcuts
    globalShortcut.register('CommandOrControl+Alt+Up', () => {
      if (mainWindow) mainWindow.webContents.send('hotkey-action', 'volume-up');
    });
    globalShortcut.register('CommandOrControl+Alt+Down', () => {
      if (mainWindow) mainWindow.webContents.send('hotkey-action', 'volume-down');
    });

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
      } else {
        mainWindow.show();
      }
    });
  });
}

// Function to safely update a specific config line prefix
function updateConfigLine(prefix, newContent) {
  try {
    if (!fs.existsSync(APO_CONFIG_PATH)) return false;
    let content = fs.readFileSync(APO_CONFIG_PATH, 'utf8');
    const lines = content.split('\n');
    let hasLine = false;
    const newLines = lines.map(line => {
      if (line.trim().startsWith(prefix)) {
        hasLine = true;
        return newContent;
      }
      return line;
    });
    if (!hasLine) newLines.push(newContent);
    fs.writeFileSync(APO_CONFIG_PATH, newLines.join('\n'), 'utf8');
    return true;
  } catch (err) {
    console.error('Failed to update config:', err);
    return false;
  }
}

// IPC Handlers
ipcMain.on('set-master-boost', (event, boostPercent) => {
  let dbGain = 0.0;
  if (boostPercent > 100) {
      const ratio = boostPercent / 100.0;
      dbGain = 20.0 * Math.log10(ratio);
  }
  const dbString = dbGain.toFixed(2);
  const success = updateConfigLine('Preamp:', `Preamp: ${dbString} dB`);
  event.reply('boost-status', { success, db: dbString });
});

ipcMain.on('set-eq-bands', (event, bandsArray) => {
  // Map 0-100 values to -15dB to +15dB
  // bands = ["115", "250", "450", "630", "1250", "2700", "5300", "7500", "13000"];
  const freqs = [115, 250, 450, 630, 1250, 2700, 5300, 7500, 13000];
  if (bandsArray && bandsArray.length === freqs.length) {
    let eqParts = [];
    for (let i = 0; i < freqs.length; i++) {
      const val = bandsArray[i]; // 0 to 100
      const db = ((val - 50) * 0.3).toFixed(1); // 0 -> -15.0, 50 -> 0.0, 100 -> +15.0
      eqParts.push(`${freqs[i]} ${db}`);
    }
    const eqString = `GraphicEQ: ${eqParts.join('; ')}`;
    updateConfigLine('GraphicEQ:', eqString);
  }
});
