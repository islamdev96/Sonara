const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const fs = require('fs');

const APO_CONFIG_PATH = 'C:\\Program Files\\EqualizerAPO\\config\\config.txt';

function createWindow() {
  const win = new BrowserWindow({
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

  // In production, load the built index.html
  // In development, load the Vite dev server URL
  if (app.isPackaged) {
    win.loadFile(path.join(__dirname, '../dist/index.html'));
  } else {
    win.loadURL('http://localhost:5173');
  }
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

// IPC handler to boost audio using the APO config backend
ipcMain.on('set-master-boost', (event, boostPercent) => {
  try {
    let dbGain = 0.0;
    if (boostPercent > 100) {
        const ratio = boostPercent / 100.0;
        dbGain = 20.0 * Math.log10(ratio);
    }
    const dbString = dbGain.toFixed(2);

    if (fs.existsSync(APO_CONFIG_PATH)) {
      let content = fs.readFileSync(APO_CONFIG_PATH, 'utf8');
      const lines = content.split('\n');
      
      let hasPreamp = false;
      const newLines = lines.map(line => {
        if (/^\s*Preamp\s*:/.test(line)) {
          hasPreamp = true;
          return `Preamp: ${dbString} dB`;
        }
        return line;
      });

      if (!hasPreamp) {
        newLines.unshift(`Preamp: ${dbString} dB`);
      }

      fs.writeFileSync(APO_CONFIG_PATH, newLines.join('\n'), 'utf8');
      event.reply('boost-status', { success: true, db: dbString });
    } else {
      // If Equalizer APO is missing, fake it or throw error
      event.reply('boost-status', { success: false, error: 'Equalizer APO not installed.' });
    }
  } catch (err) {
    event.reply('boost-status', { success: false, error: err.message });
  }
});
