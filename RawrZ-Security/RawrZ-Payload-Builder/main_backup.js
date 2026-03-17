import { app, BrowserWindow, dialog, ipcMain } from 'electron';
import path from 'node:path';
import fs from 'node:fs';
import crypto from 'node:crypto';
import zlib from 'node:zlib';

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    backgroundColor: '#f7f7f9',
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(import.meta.dirname, 'preload.js')
    }
  });
  mainWindow.loadFile('./src/index.html');
}

app.whenReady().then(createWindow);
app.on('window-all-closed', () => process.platform !== 'darwin' && app.quit());
app.on('activate', () => BrowserWindow.getAllWindows().length === 0 && createWindow());

// Dialog handlers
ipcMain.handle('dialog:openFile', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openFile']
  });
  if (canceled || !filePaths?.[0]) return null;
  return filePaths[0];
});

ipcMain.handle('dialog:openDirectory', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openDirectory', 'createDirectory']
  });
  if (canceled || !filePaths?.[0]) return null;
  return filePaths[0];
});

// Legacy handler support for old function calls
ipcMain.handle('select-directory', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openDirectory', 'createDirectory']
  });
  if (canceled || !filePaths?.[0]) return null;
  return filePaths[0];
});

ipcMain.handle('select-files', async () => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openFile']
  });
  if (canceled || !filePaths?.[0]) return null;
  return filePaths[0];
});

// File operations
ipcMain.handle('hash:file', async (_evt, filePath) => {
  if (!filePath || !fs.existsSync(filePath)) throw new Error('File not found');
  const hash = crypto.createHash('sha256');
  await new Promise((resolve, reject) => {
    const rs = fs.createReadStream(filePath);
    rs.on('error', reject);
    rs.on('data', (chunk) => hash.update(chunk));
    rs.on('end', resolve);
  });
  return hash.digest('hex');
});

ipcMain.handle('compress:file', async (_evt, { inputPath, outputDir }) => {
  if (!inputPath || !fs.existsSync(inputPath)) throw new Error('Input not found');
  const outDir = outputDir && fs.existsSync(outputDir) ? outputDir : path.dirname(inputPath);
  const outPath = path.join(outDir, path.basename(inputPath) + '.gz');
  await new Promise((resolve, reject) => {
    const rs = fs.createReadStream(inputPath);
    const ws = fs.createWriteStream(outPath);
    rs.pipe(zlib.createGzip()).pipe(ws).on('finish', resolve).on('error', reject);
  });
  return outPath;
});

ipcMain.handle('decompress:file', async (_evt, { inputPath, outputDir }) => {
  if (!inputPath || !fs.existsSync(inputPath)) throw new Error('Input not found');
  const outDir = outputDir && fs.existsSync(outputDir) ? outputDir : path.dirname(inputPath);
  const base = path.basename(inputPath).replace(/\.gz$/i, '') || path.basename(inputPath) + '.out';
  const outPath = path.join(outDir, base);
  await new Promise((resolve, reject) => {
    const rs = fs.createReadStream(inputPath);
    const ws = fs.createWriteStream(outPath);
    rs.pipe(zlib.createGunzip()).pipe(ws).on('finish', resolve).on('error', reject);
  });
  return outPath;
});

// Enhanced encryption with multiple methods
ipcMain.handle('encrypt:text', async (_evt, { text, hexKey, method = 'aes-256-gcm' }) => {
  if (typeof text !== 'string') throw new Error('Text required');
  
  const key = hexKey ? Buffer.from(hexKey, 'hex') : crypto.randomBytes(32);
  const iv = crypto.randomBytes(method.includes('gcm') ? 12 : 16);
  
  let cipher, encrypted, tag;
  
  switch (method) {
    case 'aes-256-gcm':
      cipher = crypto.createCipherGCM('aes-256-gcm', key, iv);
      encrypted = Buffer.concat([cipher.update(Buffer.from(text, 'utf8')), cipher.final()]);
      tag = cipher.getAuthTag();
      return {
        cipherTextHex: Buffer.concat([iv, tag, encrypted]).toString('hex'),
        keyHex: key.toString('hex'),
        method: method
      };
    case 'aes-256-cbc':
      cipher = crypto.createCipher('aes-256-cbc', key, iv);
      encrypted = Buffer.concat([cipher.update(Buffer.from(text, 'utf8')), cipher.final()]);
      return {
        cipherTextHex: Buffer.concat([iv, encrypted]).toString('hex'),
        keyHex: key.toString('hex'),
        method: method
      };
    case 'chacha20-poly1305':
      cipher = crypto.createCipher('chacha20-poly1305', key, iv);
      encrypted = Buffer.concat([cipher.update(Buffer.from(text, 'utf8')), cipher.final()]);
      tag = cipher.getAuthTag();
      return {
        cipherTextHex: Buffer.concat([iv, tag, encrypted]).toString('hex'),
        keyHex: key.toString('hex'),
        method: method
      };
    default:
      throw new Error(`Unsupported encryption method: ${method}`);
  }
});

ipcMain.handle('decrypt:text', async (_evt, { cipherTextHex, hexKey, method = 'aes-256-gcm' }) => {
  if (!cipherTextHex || !hexKey) throw new Error('Cipher text and key required');
  
  const key = Buffer.from(hexKey, 'hex');
  const data = Buffer.from(cipherTextHex, 'hex');
  
  let iv, tag, payload, decipher;
  
  switch (method) {
    case 'aes-256-gcm':
      iv = data.subarray(0, 12);
      tag = data.subarray(12, 28);
      payload = data.subarray(28);
      decipher = crypto.createDecipherGCM('aes-256-gcm', key, iv);
      decipher.setAuthTag(tag);
      break;
    case 'aes-256-cbc':
      iv = data.subarray(0, 16);
      payload = data.subarray(16);
      decipher = crypto.createDecipher('aes-256-cbc', key, iv);
      break;
    case 'chacha20-poly1305':
      iv = data.subarray(0, 12);
      tag = data.subarray(12, 28);
      payload = data.subarray(28);
      decipher = crypto.createDecipher('chacha20-poly1305', key, iv);
      decipher.setAuthTag(tag);
      break;
    default:
      throw new Error(`Unsupported decryption method: ${method}`);
  }
  
  const plain = Buffer.concat([decipher.update(payload), decipher.final()]);
  return plain.toString('utf8');
});

// Enhanced file encryption with method selection and extension options
ipcMain.handle('encrypt:file', async (_evt, { inputPath, outputDir, outputName, hexKey, method = 'aes-256-gcm', extension = '.enc' }) => {
  if (!inputPath || !fs.existsSync(inputPath)) throw new Error('Input file not found');
  
  const data = await fs.promises.readFile(inputPath);
  const key = hexKey ? Buffer.from(hexKey, 'hex') : crypto.randomBytes(32);
  const iv = crypto.randomBytes(method.includes('gcm') ? 12 : 16);
  
  let cipher, encrypted, tag;
  
  switch (method) {
    case 'aes-256-gcm':
      cipher = crypto.createCipherGCM('aes-256-gcm', key, iv);
      encrypted = Buffer.concat([cipher.update(data), cipher.final()]);
      tag = cipher.getAuthTag();
      break;
    case 'aes-256-cbc':
      cipher = crypto.createCipher('aes-256-cbc', key, iv);
      encrypted = Buffer.concat([cipher.update(data), cipher.final()]);
      tag = Buffer.alloc(0); // No auth tag for CBC
      break;
    case 'chacha20-poly1305':
      cipher = crypto.createCipher('chacha20-poly1305', key, iv);
      encrypted = Buffer.concat([cipher.update(data), cipher.final()]);
      tag = cipher.getAuthTag();
      break;
    default:
      throw new Error(`Unsupported encryption method: ${method}`);
  }
  
  // RAWRZ1 format: Header(6) + Method(1) + IV + TAG + Encrypted
  const methodByte = Buffer.from([method === 'aes-256-gcm' ? 1 : method === 'aes-256-cbc' ? 2 : 3]);
  const header = Buffer.from('RAWRZ1');
  const final = Buffer.concat([header, methodByte, iv, tag, encrypted]);
  
  const outDir = outputDir && fs.existsSync(outputDir) ? outputDir : path.dirname(inputPath);
  const baseName = outputName?.trim() || path.basename(inputPath);
  const outPath = path.join(outDir, baseName + extension);
  
  await fs.promises.writeFile(outPath, final);
  return { 
    outPath, 
    keyHex: key.toString('hex'), 
    sizeIn: data.length, 
    sizeOut: final.length,
    method: method
  };
});

ipcMain.handle('decrypt:file', async (_evt, { inputPath, outputDir, outputName, hexKey }) => {
  if (!inputPath || !fs.existsSync(inputPath)) throw new Error('Input file not found');
  if (!hexKey) throw new Error('Decryption key required');
  
  const key = Buffer.from(hexKey, 'hex');
  const data = await fs.promises.readFile(inputPath);
  
  // Parse RAWRZ1 format
  if (data.subarray(0, 6).toString() !== 'RAWRZ1') {
    throw new Error('Invalid file format - RAWRZ1 header not found');
  }
  
  const methodByte = data[6];
  const method = methodByte === 1 ? 'aes-256-gcm' : methodByte === 2 ? 'aes-256-cbc' : 'chacha20-poly1305';
  
  let iv, tag, payload, decipher;
  let offset = 7;
  
  if (method.includes('gcm') || method.includes('poly1305')) {
    iv = data.subarray(offset, offset + 12);
    offset += 12;
    tag = data.subarray(offset, offset + 16);
    offset += 16;
    payload = data.subarray(offset);
    decipher = crypto.createDecipherGCM('aes-256-gcm', key, iv);
    decipher.setAuthTag(tag);
  } else {
    iv = data.subarray(offset, offset + 16);
    offset += 16;
    payload = data.subarray(offset);
    decipher = crypto.createDecipher('aes-256-cbc', key, iv);
  }
  
  const plain = Buffer.concat([decipher.update(payload), decipher.final()]);
  const outDir = outputDir && fs.existsSync(outputDir) ? outputDir : path.dirname(inputPath);
  const baseName = outputName?.trim() || path.basename(inputPath).replace(/\.(enc|encrypted)$/i, '') || (path.basename(inputPath) + '.dec');
  const outPath = path.join(outDir, baseName);
  await fs.promises.writeFile(outPath, plain);
  return { outPath, sizeOut: plain.length, method: method };
});

// Parse Jotti scan results with improved parsing
ipcMain.handle('parse:jotti', async (_evt, { resultText }) => {
  const lines = resultText.split('\n');
  const results = { scanners: [], detections: 0, total: 0 };
  
  for (const line of lines) {
    // Handle various Jotti result formats
    if (line.includes('Found nothing') || line.includes('Clean')) {
      const match = line.match(/^([^\t]+)\t.*(?:Found nothing|Clean)/i);
      if (match) {
        results.scanners.push({ name: match[1].trim(), result: 'Clean' });
        results.total++;
      }
    } else if (line.includes('Trojan') || line.includes('Virus') || line.includes('Malware') || line.includes('Infected')) {
      const match = line.match(/^([^\t]+)\t.*(Trojan|Virus|Malware|Infected)/i);
      if (match) {
        results.scanners.push({ name: match[1].trim(), result: match[2] });
        results.total++;
        results.detections++;
      }
    } else if (line.match(/(\d+)\/(\d+)\s+scanners? reported? (?:malware|threat)/i)) {
      const match = line.match(/(\d+)\/(\d+)\s+scanners? reported? (?:malware|threat)/i);
      if (match) {
        results.detections = parseInt(match[1]);
        results.total = parseInt(match[2]);
      }
    }
  }
  
  results.fud = results.detections === 0 && results.total > 0;
  return results;
});

// Engine execution (safe placeholders only - no harmful operations)
ipcMain.handle('execute:engine', async (_evt, { engineName, action = 'test' }) => {
  const engineMap = {
    'stub-generator': 'Generating safe C++/ASM stub templates',
    'beaconism': 'Beaconism compiler simulation (placeholder only)',
    'http-bot-generator': 'HTTP bot framework (educational demo)',
    'tcp-bot-generator': 'TCP bot system (safe implementation)',
    'irc-bot-generator': 'IRC bot builder (localhost testing)',
    'malware-analysis': 'Binary analysis tools (educational purposes)',
    'network-tools': 'Network scanner (localhost scanning only)',
    'stealth-engine': 'Steganography toolkit (safe encoding)',
    'polymorphic-engine': 'Code obfuscation (educational transformation)'
  };
  
  const message = engineMap[engineName] || 'Unknown engine';
  
  return {
    success: true,
    message: `${message} - ${action} completed safely`,
    timestamp: new Date().toISOString(),
    engine: engineName,
    action: action
  };
});

// Legacy and placeholder handlers for the existing UI
ipcMain.handle('generate-stub', async (_evt, payloadPath, options) => {
  console.log('Generating stub:', payloadPath, options);
  return { success: true, message: 'Stub generation completed (placeholder)', path: payloadPath + '_stub.exe' };
});

ipcMain.handle('protect-bot', async (_evt, filePath, password) => {
  return { success: true, message: 'Bot protection applied (placeholder)', protectedPath: filePath + '.protected' };
});

ipcMain.handle('obfuscate-bot', async (_evt, scriptPath) => {
  return { success: true, message: 'Bot obfuscation completed (placeholder)', obfuscatedPath: scriptPath + '.obf' };
});

// Enhanced Engine Registry System
const engineRegistry = {
  'stub-generator': {
    name: 'Stub Generator',
    icon: '🏗️',
    enabled: true,
    category: 'Crypters',
    features: {
      stubTypes: ['asm', 'c', 'csharp'],
      encryption: ['none', 'aes', 'hybrid'],
      protections: ['antiDebug', 'antiVM', 'antiSandbox']
    },
    menu: {
      payloadFile: { type: 'file', label: 'Payload File', required: true },
      stubType: { type: 'select', label: 'Stub Type', options: 'stubTypes' },
      encryption: { type: 'select', label: 'Encryption', options: 'encryption' },
      outputPath: { type: 'file', label: 'Output Path', mode: 'save' },
      protections: { type: 'checkboxes', label: 'Protections', options: 'protections' }
    }
  },
  'beaconism': {
    name: 'Beaconism Compiler',
    icon: '📡',
    enabled: false,
    category: 'Payload Generators',
    features: {
      architectures: ['x86', 'x64', 'arm64'],
      protocols: ['http', 'https', 'dns', 'tcp'],
      evasion: ['process-hollowing', 'dll-injection', 'manual-mapping'],
      encoding: ['base64', 'xor', 'rc4', 'chacha20']
    },
    menu: {
      c2Server: { type: 'text', label: 'C2 Server', placeholder: 'example.com' },
      protocol: { type: 'select', label: 'Protocol', options: 'protocols' },
      architecture: { type: 'select', label: 'Architecture', options: 'architectures' },
      evasionTech: { type: 'select', label: 'Evasion Technique', options: 'evasion' },
      encoding: { type: 'select', label: 'Encoding', options: 'encoding' },
      interval: { type: 'number', label: 'Callback Interval (ms)', default: 5000 }
    }
  },
  'http-bot-generator': {
    name: 'HTTP Bot Builder',
    icon: '🌐',
    enabled: false,
    category: 'Bot Builders',
    features: {
      methods: ['GET', 'POST', 'PUT', 'DELETE'],
      auth: ['none', 'basic', 'bearer', 'oauth2'],
      formats: ['json', 'xml', 'form'],
      persistence: ['registry', 'startup', 'service']
    },
    menu: {
      endpoint: { type: 'text', label: 'API Endpoint', required: true },
      method: { type: 'select', label: 'HTTP Method', options: 'methods' },
      auth: { type: 'select', label: 'Authentication', options: 'auth' },
      format: { type: 'select', label: 'Data Format', options: 'formats' },
      persistence: { type: 'checkboxes', label: 'Persistence Methods', options: 'persistence' }
    }
  },
  'irc-bot-generator': {
    name: 'IRC Bot Builder',
    icon: '💬',
    enabled: false,
    category: 'Bot Builders',
    features: {
      protocols: ['irc', 'ircs'],
      channels: ['single', 'multi'],
      commands: ['basic', 'advanced', 'custom'],
      security: ['ssl', 'sasl', 'nickserv']
    },
    menu: {
      server: { type: 'text', label: 'IRC Server', placeholder: 'irc.example.com' },
      port: { type: 'number', label: 'Port', default: 6667 },
      channel: { type: 'text', label: 'Channel', placeholder: '#botnet' },
      nickname: { type: 'text', label: 'Bot Nickname', placeholder: 'bot123' },
      ssl: { type: 'checkbox', label: 'Use SSL/TLS' },
      commands: { type: 'select', label: 'Command Set', options: 'commands' }
    }
  },
  'malware-analysis': {
    name: 'Binary Analysis Suite',
    icon: '🔍',
    enabled: false,
    category: 'Analysis Tools',
    features: {
      formats: ['pe', 'elf', 'macho'],
      analysis: ['static', 'dynamic', 'hybrid'],
      extractors: ['strings', 'imports', 'exports', 'resources'],
      disasm: ['x86', 'x64', 'arm']
    },
    menu: {
      targetFile: { type: 'file', label: 'Target Binary', required: true },
      analysisType: { type: 'select', label: 'Analysis Type', options: 'analysis' },
      extractors: { type: 'checkboxes', label: 'Data Extractors', options: 'extractors' },
      outputReport: { type: 'file', label: 'Report Output', mode: 'save' }
    }
  },
  'network-tools': {
    name: 'Network Scanner',
    icon: '🌍',
    enabled: false,
    category: 'Reconnaissance',
    features: {
      scanTypes: ['port', 'service', 'os', 'vuln'],
      protocols: ['tcp', 'udp', 'icmp'],
      ranges: ['single', 'range', 'cidr'],
      timing: ['stealth', 'normal', 'aggressive']
    },
    menu: {
      target: { type: 'text', label: 'Target', placeholder: '127.0.0.1' },
      scanType: { type: 'select', label: 'Scan Type', options: 'scanTypes' },
      protocol: { type: 'select', label: 'Protocol', options: 'protocols' },
      timing: { type: 'select', label: 'Timing', options: 'timing' },
      outputFile: { type: 'file', label: 'Results Output', mode: 'save' }
    }
  },
  'stealth-engine': {
    name: 'Steganography Toolkit',
    icon: '🎭',
    enabled: false,
    category: 'Concealment',
    features: {
      carriers: ['image', 'audio', 'video', 'text'],
      methods: ['lsb', 'dct', 'dwt', 'spread'],
      formats: ['png', 'jpg', 'wav', 'mp4'],
      encryption: ['none', 'aes', 'chacha20']
    },
    menu: {
      carrierFile: { type: 'file', label: 'Carrier File', required: true },
      payloadFile: { type: 'file', label: 'Hidden Payload', required: true },
      method: { type: 'select', label: 'Hiding Method', options: 'methods' },
      encryption: { type: 'select', label: 'Payload Encryption', options: 'encryption' },
      outputFile: { type: 'file', label: 'Output File', mode: 'save' }
    }
  },
  'polymorphic-engine': {
    name: 'Code Obfuscator',
    icon: '🔄',
    enabled: false,
    category: 'Evasion',
    features: {
      languages: ['c', 'cpp', 'csharp', 'python', 'javascript'],
      techniques: ['variable-rename', 'control-flow', 'string-encrypt', 'dead-code'],
      levels: ['light', 'medium', 'heavy', 'extreme'],
      antiAnalysis: ['anti-debug', 'anti-vm', 'anti-disasm']
    },
    menu: {
      sourceFile: { type: 'file', label: 'Source Code', required: true },
      language: { type: 'select', label: 'Language', options: 'languages' },
      level: { type: 'select', label: 'Obfuscation Level', options: 'levels' },
      techniques: { type: 'checkboxes', label: 'Techniques', options: 'techniques' },
      antiAnalysis: { type: 'checkboxes', label: 'Anti-Analysis', options: 'antiAnalysis' },
      outputFile: { type: 'file', label: 'Obfuscated Output', mode: 'save' }
    }
  }
};

ipcMain.handle('get-engines', async () => {
  return {
    engines: Object.entries(engineRegistry).map(([id, engine]) => ({
      id,
      ...engine
    }))
  };
});

// Get specific engine configuration
ipcMain.handle('get-engine-config', async (_evt, engineId) => {
  const engine = engineRegistry[engineId];
  if (!engine) throw new Error(`Engine not found: ${engineId}`);
  return engine;
});

// Generate dynamic engine menu
ipcMain.handle('generate-engine-menu', async (_evt, engineId) => {
  const engine = engineRegistry[engineId];
  if (!engine) throw new Error(`Engine not found: ${engineId}`);
  
  const menu = {};
  Object.entries(engine.menu).forEach(([key, config]) => {
    menu[key] = {
      ...config,
      // Resolve options from features if it's a reference
      options: typeof config.options === 'string' 
        ? engine.features[config.options] || [] 
        : config.options || []
    };
  });
  
  return { engineId, menu, categories: Object.keys(engine.features) };
});

ipcMain.handle('fud-encrypt', async (_evt, filePath, outputPath) => {
  return { success: true, message: 'FUD encryption applied (placeholder)', encryptedPath: outputPath };
});

ipcMain.handle('get-stub-status', async () => {
  return { active: 5, burned: 0, total: 5 };
});

ipcMain.handle('burn-stub', async (_evt, stubId) => {
  return { success: true, message: `Stub ${stubId} burned (placeholder)` };
});