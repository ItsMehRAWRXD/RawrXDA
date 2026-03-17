require('dotenv').config();
const express = require('express');
const fetch = require('node-fetch');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const app = express();
app.use(express.json());

const CLIENT_ID = process.env.CLIENT_ID || '';
const CLIENT_SECRET = process.env.CLIENT_SECRET || '';
const REDIRECT_URI = process.env.REDIRECT_URI || `http://localhost:3000/callback`;
const PORT = parseInt(process.env.PORT || '3000', 10);
const SCOPES = process.env.SCOPES || 'repo,read:user';
const ENCRYPTION_KEY = process.env.ENCRYPTION_KEY || '';

const TOKEN_FILE = path.join(__dirname, 'tokens.json');

function deriveKey(key) {
  // return 32-byte key
  return crypto.createHash('sha256').update(key).digest();
}

function encryptJSON(obj) {
  const iv = crypto.randomBytes(12);
  const key = ENCRYPTION_KEY ? deriveKey(ENCRYPTION_KEY) : null;
  const plaintext = Buffer.from(JSON.stringify(obj), 'utf8');
  if (!key) {
    return JSON.stringify({ raw: obj });
  }
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const encrypted = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  const tag = cipher.getAuthTag();
  return JSON.stringify({ iv: iv.toString('hex'), data: encrypted.toString('hex'), tag: tag.toString('hex') });
}

function decryptJSON(str) {
  try {
    const parsed = JSON.parse(str);
    if (parsed.raw) return parsed.raw;
    if (!ENCRYPTION_KEY) return null;
    const key = deriveKey(ENCRYPTION_KEY);
    const iv = Buffer.from(parsed.iv, 'hex');
    const data = Buffer.from(parsed.data, 'hex');
    const tag = Buffer.from(parsed.tag, 'hex');
    const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
    decipher.setAuthTag(tag);
    const out = Buffer.concat([decipher.update(data), decipher.final()]);
    return JSON.parse(out.toString('utf8'));
  } catch (e) {
    console.warn('Failed to decrypt token file:', e.message);
    return null;
  }
}

function saveTokens(obj) {
  const enc = encryptJSON(obj);
  fs.writeFileSync(TOKEN_FILE, enc, { encoding: 'utf8' });
}

function loadTokens() {
  if (!fs.existsSync(TOKEN_FILE)) return null;
  const content = fs.readFileSync(TOKEN_FILE, 'utf8');
  return decryptJSON(content);
}

app.get('/', (req, res) => {
  // Serve the static test client if present
  const staticIndex = path.join(__dirname, 'static', 'index.html');
  if (fs.existsSync(staticIndex)) {
    return res.sendFile(staticIndex);
  }
  res.send('Copilot Auth Scaffold - visit /authorize');
});

app.get('/authorize', (req, res) => {
  if (!CLIENT_ID) return res.status(500).send('CLIENT_ID not configured. Copy .env.example -> .env');
  const state = crypto.randomBytes(8).toString('hex');
  const url = `https://github.com/login/oauth/authorize?client_id=${encodeURIComponent(CLIENT_ID)}&redirect_uri=${encodeURIComponent(REDIRECT_URI)}&scope=${encodeURIComponent(SCOPES)}&state=${state}`;
  // Note: store state in memory for production use a DB or user session store
  req.app.locals.oauthState = state;
  res.redirect(url);
});

app.get('/callback', async (req, res) => {
  const { code, state } = req.query;
  if (!code) return res.status(400).send('Missing code');
  if (!state || state !== req.app.locals.oauthState) {
    console.warn('State mismatch or missing');
    // continue but warn
  }
  if (!CLIENT_SECRET) return res.status(500).send('CLIENT_SECRET not configured.');

  try {
    const tokenRes = await fetch('https://github.com/login/oauth/access_token', {
      method: 'POST',
      headers: { 'Accept': 'application/json', 'Content-Type': 'application/json' },
      body: JSON.stringify({ client_id: CLIENT_ID, client_secret: CLIENT_SECRET, code, redirect_uri: REDIRECT_URI })
    });
    const tokenJson = await tokenRes.json();
    if (tokenJson.error) {
      console.error('OAuth error', tokenJson);
      return res.status(500).send('OAuth token exchange failed');
    }
    // tokenJson contains access_token, scope, token_type
    saveTokens(tokenJson);
    // If a static success page exists, send it, otherwise a simple message
    const successPage = path.join(__dirname, 'static', 'success.html');
    if (fs.existsSync(successPage)) return res.sendFile(successPage);
    res.send('<h2>OAuth successful</h2><p>Tokens saved to server. You can close this window.</p>');
  } catch (e) {
    console.error('Callback error', e);
    res.status(500).send('Callback processing failed');
  }
});

// Unsafe but useful for local testing: return stored token (protect in prod)
app.get('/token', (req, res) => {
  const t = loadTokens();
  if (!t) return res.status(404).send('No token stored');
  res.json(t);
});

app.get('/health', (req, res) => {
  res.json({ status: 'ok', env: { clientId: !!CLIENT_ID, redirectUri: REDIRECT_URI } });
});

app.listen(PORT, () => {
  console.log(`Copilot Auth server listening on http://localhost:${PORT}`);
  if (!CLIENT_ID || !CLIENT_SECRET) {
    console.warn('CLIENT_ID or CLIENT_SECRET not set. Configure .env before using /authorize');
  }
});
