// licensing.cjs - Commercial licensing / trial scaffold.
//
// Model: 14-day full-featured trial, then free tier capped at +100% boost.
// A paid license key unlocks unlimited boost + all effects + presets.
//
// Keys are signed offline with Ed25519: the app ships ONLY the public key, so
// keys cannot be forged without the private key. Optional online activation can
// be layered on later (endpoint left pluggable).
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const os = require('os');

const STORE_DIR = path.join(process.env.ProgramData || os.homedir(), 'WinAudioBoosterPro');
const LICENSE_FILE = path.join(STORE_DIR, 'license.json');
const TRIAL_FILE = path.join(STORE_DIR, '.trial');
const TRIAL_DAYS = 14;
// Launch phase: unlock everything for FREE to grow the user base.
// Flip to false later to enable the paid Pro tiers.
const LAUNCH_FREE = true;

// Replace with your real Ed25519 public key (base64, raw 32 bytes) at build.
const PUBLIC_KEY_B64 = process.env.WAB_LICENSE_PUBKEY || '';

function ensureDir() { try { fs.mkdirSync(STORE_DIR, { recursive: true }); } catch (_) {} }

function startOrLoadTrial() {
  ensureDir();
  try {
    if (fs.existsSync(TRIAL_FILE)) {
      return JSON.parse(fs.readFileSync(TRIAL_FILE, 'utf8'));
    }
  } catch (_) {}
  const rec = { start: Date.now(), id: crypto.randomUUID() };
  try { fs.writeFileSync(TRIAL_FILE, JSON.stringify(rec)); } catch (_) {}
  return rec;
}

function trialDaysLeft() {
  const rec = startOrLoadTrial();
  const elapsed = (Date.now() - rec.start) / 86400000;
  return Math.max(0, Math.ceil(TRIAL_DAYS - elapsed));
}

// A license key is: base64url(payloadJSON) + '.' + base64url(ed25519 signature).
function verifyKey(key) {
  if (!key || !PUBLIC_KEY_B64) return null;
  try {
    const [body, sig] = String(key).trim().split('.');
    if (!body || !sig) return null;
    const payload = Buffer.from(body, 'base64url');
    const signature = Buffer.from(sig, 'base64url');
    const pub = crypto.createPublicKey({
      key: Buffer.concat([
        Buffer.from('302a300506032b6570032100', 'hex'), // DER prefix for Ed25519
        Buffer.from(PUBLIC_KEY_B64, 'base64'),
      ]),
      format: 'der', type: 'spki',
    });
    if (!crypto.verify(null, payload, pub, signature)) return null;
    const data = JSON.parse(payload.toString('utf8'));
    if (data.exp && Date.now() > data.exp) return null; // expired subscription
    return data; // { email, plan, exp? }
  } catch (_) { return null; }
}

function loadLicense() {
  try {
    if (fs.existsSync(LICENSE_FILE)) {
      const { key } = JSON.parse(fs.readFileSync(LICENSE_FILE, 'utf8'));
      const data = verifyKey(key);
      if (data) return { valid: true, ...data, key };
    }
  } catch (_) {}
  return { valid: false };
}

function activate(key) {
  const data = verifyKey(key);
  if (!data) return { ok: false, error: 'Invalid or expired license key.' };
  ensureDir();
  fs.writeFileSync(LICENSE_FILE, JSON.stringify({ key }));
  return { ok: true, ...data };
}

function deactivate() { try { fs.unlinkSync(LICENSE_FILE); } catch (_) {} }

// The single source of truth the app uses to gate features.
function status() {
  if (LAUNCH_FREE) return { tier: 'free', maxBoostPercent: 500, launch: true };
  const lic = loadLicense();
  if (lic.valid) {
    return { tier: 'pro', maxBoostPercent: 500, plan: lic.plan || 'pro', email: lic.email || null };
  }
  const left = trialDaysLeft();
  if (left > 0) {
    return { tier: 'trial', maxBoostPercent: 500, trialDaysLeft: left };
  }
  return { tier: 'free', maxBoostPercent: 100, trialDaysLeft: 0 };
}

module.exports = { status, activate, deactivate, trialDaysLeft, verifyKey, LICENSE_FILE };
