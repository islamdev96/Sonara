// statusBridge.cjs - Reads the status.bin file written by the APO to get
// real heartbeat + audio level data. This replaces the unreliable tasklist-based
// engine detection with proof-of-actual-processing.
//
// Layout MUST match engine/src/apo/StatusBlock.h (#pragma pack(4)).
const fs = require('fs');
const path = require('path');
const os = require('os');

const STATUS_MAGIC = 0x57414253; // 'WABS'
const HEARTBEAT_TIMEOUT_MS = 3000; // engine considered dead after 3s of no heartbeat

// %ProgramData%\WinAudioBoosterPro\status.bin
const STATUS_DIR = path.join(process.env.ProgramData || path.join(os.homedir(), 'AppData', 'Local'), 'WinAudioBoosterPro');
const STATUS_FILE = path.join(STATUS_DIR, 'status.bin');

// Parse the StatusBlock struct from a Buffer.
function parseStatus(buf) {
  if (!buf || buf.length < 56) return null;
  const magic = buf.readUInt32LE(0);
  if (magic !== STATUS_MAGIC) return null;

  const seq          = buf.readUInt32LE(4);
  if (seq === 0) return null; // write in progress

  // heartbeatMs is a uint64 at offset 8
  const heartbeatLo  = buf.readUInt32LE(8);
  const heartbeatHi  = buf.readUInt32LE(12);
  const heartbeatMs  = heartbeatHi * 0x100000000 + heartbeatLo;

  const rmsLeft      = buf.readFloatLE(16);
  const rmsRight     = buf.readFloatLE(20);
  const peakLeft     = buf.readFloatLE(24);
  const peakRight    = buf.readFloatLE(28);
  const sampleRate   = buf.readUInt32LE(32);
  const channels     = buf.readUInt32LE(36);

  return { seq, heartbeatMs, rmsLeft, rmsRight, peakLeft, peakRight, sampleRate, channels };
}

// Read and return the current status, or null if the file doesn't exist or is stale.
function readStatus() {
  try {
    const buf = fs.readFileSync(STATUS_FILE);
    const status = parseStatus(buf);
    if (!status) return null;

    // Check freshness: is the heartbeat recent?
    const now = Date.now(); // JS timestamp (ms since epoch)
    // GetTickCount64 is ms since boot — we can't compare directly to Date.now().
    // Instead, we read the file's mtime as a proxy for freshness.
    const stat = fs.statSync(STATUS_FILE);
    const fileAgeMs = now - stat.mtimeMs;
    const isAlive = fileAgeMs < HEARTBEAT_TIMEOUT_MS;

    return { ...status, isAlive, fileAgeMs };
  } catch (_) {
    return null;
  }
}

module.exports = { readStatus, STATUS_FILE, STATUS_DIR, HEARTBEAT_TIMEOUT_MS };
