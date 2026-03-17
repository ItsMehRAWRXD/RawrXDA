// Dynamic Engine Manager - Auto-generates UI for all engines
class EngineManager {
  constructor() {
    this.engines = {};
    this.activeEngine = null;
  }

  // Initialize all engines and generate UI
  async initialize() {
    await this.loadEngineConfigs();
    this.generateEngineToggles();
    this.generateEngineMenus();
    this.setupEventListeners();
    this.log('🔧 Engine Manager initialized with auto-generated menus');
  }

  // Load engine configurations
  async loadEngineConfigs() {
    // Import engine configs (would be loaded from engine-config.js)
    this.engines = window.engineConfigs || {};
  }

  // Generate toggle switches for each engine
  generateEngineToggles() {
    const container = document.getElementById('engineToggles');
    if (!container) return;

    let togglesHTML = '<div class="engine-toggles-grid">';
    
    Object.entries(this.engines).forEach(([engineId, config]) => {
      togglesHTML += `
        <div class="engine-toggle-card">
          <div class="toggle-header">
            <span class="engine-icon">${config.icon}</span>
            <span class="engine-name">${config.name}</span>
            <span class="engine-category">${config.category}</span>
          </div>
          <label class="toggle-switch">
            <input type="checkbox" id="${engineId}-toggle" 
                   ${config.enabled ? 'checked' : ''} 
                   onchange="engineManager.toggleEngine('${engineId}')">
            <span class="slider"></span>
          </label>
        </div>
      `;
    });

    togglesHTML += '</div>';
    container.innerHTML = togglesHTML;
  }

  // Generate dynamic menus for each engine
  generateEngineMenus() {
    const container = document.getElementById('engineMenus');
    if (!container) return;

    let menusHTML = '';
    Object.keys(this.engines).forEach(engineId => {
      menusHTML += window.generateEngineMenu(engineId);
    });

    container.innerHTML = menusHTML;
  }

  // Toggle engine on/off
  toggleEngine(engineId) {
    const toggle = document.getElementById(`${engineId}-toggle`);
    const menu = document.getElementById(`${engineId}-menu`);
    
    this.engines[engineId].enabled = toggle.checked;
    
    if (toggle.checked) {
      this.log(`✅ ${this.engines[engineId].name} enabled`);
      if (menu) menu.style.display = 'block';
    } else {
      this.log(`❌ ${this.engines[engineId].name} disabled`);
      if (menu) menu.style.display = 'none';
    }

    this.updateEngineStats();
  }

  // Execute specific engine with its configuration
  async executeEngine(engineId) {
    if (!this.engines[engineId]?.enabled) {
      this.log(`❌ ${engineId} is disabled`);
      return;
    }

    const config = this.engines[engineId];
    const params = this.collectEngineParams(engineId);
    
    this.log(`🚀 Executing ${config.icon} ${config.name}...`);
    
    try {
      // Call the actual engine execution
      const result = await window.electronAPI.executeEngine(engineId, params);
      this.log(`✅ ${config.name} completed successfully`);
      
      // Display detailed results
      if (result.success && result.data) {
        if (result.data.botPath) {
          this.log(`📁 Generated: ${result.data.botPath}`);
          this.log(`📊 Format: ${result.data.format || 'exe'}`);
          this.log(`📏 Size: ${result.data.size} bytes`);
          
          if (result.data.encrypted) {
            this.log(`🔐 Encrypted with RAWRZ1`);
            this.log(`🔑 Key: ${result.data.key}`);
            this.log(`🧪 Ready for Jotti testing!`);
          }
        } else if (result.data.status) {
          this.log(`📋 Status: ${result.data.status}`);
        }
        
        // Show any additional data
        Object.entries(result.data).forEach(([key, value]) => {
          if (!['botPath', 'format', 'size', 'encrypted', 'key', 'status'].includes(key)) {
            this.log(`🔧 ${key}: ${value}`);
          }
        });
      } else {
        this.log(`📋 Result: ${JSON.stringify(result, null, 2)}`);
      }
      
      this.updateStats('generatedPayloads', 1);
    } catch (error) {
      this.log(`❌ ${config.name} failed: ${error.message}`);
    }
  }

  // Collect parameters from engine menu
  collectEngineParams(engineId) {
    const config = this.engines[engineId];
    const params = {};

    Object.entries(config.menu).forEach(([key, field]) => {
      const fieldId = `${engineId}-${key}`;
      
      switch (field.type) {
        case 'text':
        case 'number':
        case 'file':
          const input = document.getElementById(fieldId);
          if (input) params[key] = input.value;
          break;

        case 'select':
          const select = document.getElementById(fieldId);
          if (select) params[key] = select.value;
          break;

        case 'checkbox':
          const checkbox = document.getElementById(fieldId);
          if (checkbox) params[key] = checkbox.checked;
          break;

        case 'checkboxes':
          const checkboxes = document.querySelectorAll(`[id^="${fieldId}-"]:checked`);
          params[key] = Array.from(checkboxes).map(cb => cb.value);
          break;
      }
    });

    return params;
  }

  // Clear engine configuration
  clearEngineConfig(engineId) {
    const config = this.engines[engineId];
    
    Object.keys(config.menu).forEach(key => {
      const fieldId = `${engineId}-${key}`;
      const element = document.getElementById(fieldId);
      
      if (element) {
        if (element.type === 'checkbox') {
          element.checked = false;
        } else {
          element.value = '';
        }
      }
    });

    this.log(`🧹 ${config.name} configuration cleared`);
  }

  // Browse for files
  async browseFile(fieldId, mode = 'open') {
    try {
      let filePath;
      if (mode === 'save') {
        filePath = await window.electronAPI.saveFile();
      } else {
        filePath = await window.electronAPI.selectFile();
      }
      
      if (filePath) {
        document.getElementById(fieldId).value = filePath;
        this.log(`📁 Selected: ${filePath}`);
      }
    } catch (error) {
      this.log(`❌ File selection failed: ${error.message}`);
    }
  }

  // Update engine statistics
  updateEngineStats() {
    const enabled = Object.values(this.engines).filter(e => e.enabled).length;
    const total = Object.keys(this.engines).length;
    
    const statsElement = document.getElementById('engineStats');
    if (statsElement) {
      statsElement.textContent = `${enabled}/${total} engines active`;
    }
  }

  // Setup event listeners
  setupEventListeners() {
    // Global functions for onclick handlers
    window.engineManager = this;
    window.browseFile = (fieldId, mode) => this.browseFile(fieldId, mode);
    window.executeEngine = (engineId) => this.executeEngine(engineId);
    window.clearEngineConfig = (engineId) => this.clearEngineConfig(engineId);
  }

  // Logging function
  log(message) {
    const timestamp = new Date().toLocaleTimeString();
    const logElement = document.getElementById('output');
    if (logElement) {
      logElement.innerHTML += `<div>[${timestamp}] ${message}</div>`;
      logElement.scrollTop = logElement.scrollHeight;
    }
    console.log(`[${timestamp}] ${message}`);
  }

  // Update statistics
  updateStats(statId, increment = 1) {
    const element = document.getElementById(statId);
    if (element) {
      const current = parseInt(element.textContent) || 0;
      element.textContent = current + increment;
    }
  }
}

// Initialize engine manager when DOM is loaded
document.addEventListener('DOMContentLoaded', async () => {
  window.engineManager = new EngineManager();
  await window.engineManager.initialize();
});