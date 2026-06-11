// paramBridge.cjs - Writes the binary Parameters struct that the self-contained
// audio engine (BoosterAPO.dll) memory-maps. This is the bridge between the GUI
// and our own engine - it replaces the old Equalizer APO config.txt approach.
//
// Layout MUST match src/dsp/Parameters.h (#pragma pack(4)).
const fs = require('fs');
const path = require('path');
const os = require('os');

const MAGIC = 0x57414250; // 'WABP'
const VERSION = 2;
const NUM_EQ = 10;

// %ProgramData%\WinAudioBoosterPro\params.bin
const PARAMS_DIR = path.join(process.env.ProgramData || path.join(os.homedir(), 'AppData', 'Local'), 'WinAudioBoosterPro');
const PARAMS_FILE = path.join(PARAMS_DIR, 'params.bin');

let seq = 0;

function ensureDir() {
  try { fs.mkdirSync(PARAMS_DIR, { recursive: true }); } catch (_) {}
}

// Build the exact struct byte layout from Parameters.h.
function serialize(p) {
  // Compute total size: 3*u32 + i32 + 2*f32 + 10*f32 + 5*f32 + i32 + f32 + 8*u32
  const buf = Buffer.alloc(
    4 + 4 + 4 +            // magic, version, seq
    4 +                    // enabled
    4 + 4 +                // preampDb, outputGainDb
    NUM_EQ * 4 +           // eqGainsDb[10]
    5 * 4 +                // bass, clarity, ambience, surround, dynamic
    4 + 4 +                // limiterOn, limiterCeilingDb
    8 * 4                  // reserved[8]
  );
  let o = 0;
  buf.writeUInt32LE(MAGIC, o); o += 4;
  buf.writeUInt32LE(VERSION, o); o += 4;
  buf.writeUInt32LE(seq >>> 0, o); o += 4;
  buf.writeInt32LE(p.enabled ? 1 : 0, o); o += 4;
  buf.writeFloatLE(p.preampDb || 0, o); o += 4;
  buf.writeFloatLE(p.outputGainDb || 0, o); o += 4;
  for (let i = 0; i < NUM_EQ; i++) { buf.writeFloatLE((p.eqGainsDb && p.eqGainsDb[i]) || 0, o); o += 4; }
  buf.writeFloatLE(p.bass || 0, o); o += 4;
  buf.writeFloatLE(p.clarity || 0, o); o += 4;
  buf.writeFloatLE(p.ambience || 0, o); o += 4;
  buf.writeFloatLE(p.surround || 0, o); o += 4;
  buf.writeFloatLE(p.dynamic || 0, o); o += 4;
  buf.writeInt32LE(p.limiterOn === false ? 0 : 1, o); o += 4;
  buf.writeFloatLE(typeof p.limiterCeilingDb === 'number' ? p.limiterCeilingDb : -1.0, o); o += 4;
  // reserved already zeroed
  return buf;
}

// Publish parameters to the engine. Writes seq last to ensure the real-time
// seqlock reader in BoosterAPO never reads a torn parameter state.
function publish(p) {
  ensureDir();
  seq = (seq + 1) >>> 0;
  if (seq === 0) seq = 1; // 0 is reserved as our write-in-progress indicator
  
  const buf = serialize(p);
  try {
    let fd;
    try {
      fd = fs.openSync(PARAMS_FILE, 'r+');
    } catch (_) {
      // Create the file if it does not exist
      fd = fs.openSync(PARAMS_FILE, 'w');
    }

    // Step 1: Write structure with seq = 0 (write in progress)
    const writeBuf = Buffer.from(buf);
    writeBuf.writeUInt32LE(0, 8); // Offset of seq field is 8
    fs.writeSync(fd, writeBuf, 0, writeBuf.length, 0);

    // Step 2: Write actual new sequence number at offset 8 to finalize transaction
    const seqBuf = Buffer.alloc(4);
    seqBuf.writeUInt32LE(seq, 0);
    fs.writeSync(fd, seqBuf, 0, 4, 8);

    fs.closeSync(fd);
    return true;
  } catch (e) {
    console.error('paramBridge write failed:', e.message);
    return false;
  }
}

module.exports = { publish, PARAMS_FILE, PARAMS_DIR };
