// paramBridge.cjs - Writes the binary Parameters struct that the self-contained
// audio engine (SonaraAPO.dll) memory-maps. This is the bridge between the GUI
// and our own engine - it replaces the old Equalizer APO config.txt approach.
//
// Layout MUST match src/dsp/Parameters.h (#pragma pack(4)).
const fs = require('fs');
const path = require('path');
const os = require('os');

const MAGIC = 0x57414250; // 'WABP'
const VERSION = 3;
const NUM_EQ = 10;
const kEqFrequencies = [31.5, 63.0, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0];

// %ProgramData%\Sonara\params.bin
const PARAMS_DIR = path.join(process.env.ProgramData || path.join(os.homedir(), 'AppData', 'Local'), 'Sonara');
const PARAMS_FILE = path.join(PARAMS_DIR, 'params.bin');

let seq = 0;

function ensureDir() {
  try { fs.mkdirSync(PARAMS_DIR, { recursive: true }); } catch (_) {}
}

// Build the exact struct byte layout from Parameters.h.
function serialize(p) {
  // Compute total size: 3*u32 + i32 + 2*f32 + NUM_EQ*16 + 5*f32 + i32 + f32 + 8*u32 = 244 bytes
  const buf = Buffer.alloc(
    4 + 4 + 4 +            // magic, version, seq
    4 +                    // enabled
    4 + 4 +                // preampDb, outputGainDb
    NUM_EQ * 16 +          // eqBands[10] (each is 16 bytes: 3*f32 + i32)
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

  // Map fallback from old eqGainsDb if new eqBands is not present
  let eqBands = p.eqBands;
  if (!eqBands && p.eqGainsDb) {
    eqBands = p.eqGainsDb.map((g, i) => ({
      freq: kEqFrequencies[i],
      q: 1.4,
      gain: g,
      type: 0
    }));
  }

  for (let i = 0; i < NUM_EQ; i++) {
    const band = (eqBands && eqBands[i]) || { freq: kEqFrequencies[i], q: 1.4, gain: 0, type: 0 };
    buf.writeFloatLE(band.freq || kEqFrequencies[i], o); o += 4;
    buf.writeFloatLE(typeof band.q === 'number' ? band.q : 1.4, o); o += 4;
    buf.writeFloatLE(band.gain || 0, o); o += 4;
    buf.writeInt32LE(typeof band.type === 'number' ? band.type : 0, o); o += 4;
  }

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
// seqlock reader in SonaraAPO never reads a torn parameter state.
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
