/**
 * BigDaddyG IDE - API Key Settings Panel
 * Beautiful UI for managing API keys across all providers
 */

class APIKeySettingsPanel {
  constructor() {
    this.isOpen = false;
    this.activeTab = null;
    this.manager = window.apiKeyManager;
    this.createPanel();
  }

  createPanel() {
    // Create modal container
    this.modal = document.createElement('div');
    this.modal.id = 'api-key-settings-modal';
    this.modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            z-index: 10000;
            display: none;
            align-items: center;
            justify-content: center;
            backdrop-filter: blur(5px);
        `;

    // Create panel container
    this.panel = document.createElement('div');
    this.panel.style.cssText = `
            background: var(--void);
            border: 1px solid var(--cyan);
            border-radius: 12px;
            width: 90%;
            max-width: 1000px;
            height: 80vh;
            max-height: 700px;
            display: flex;
            flex-direction: column;
            box-shadow: 0 0 40px rgba(0, 212, 255, 0.3);
        `;

    this.panel.innerHTML = `
            <!-- Header -->
            <div style="
                padding: 20px;
                border-bottom: 1px solid rgba(0, 212, 255, 0.2);
                display: flex;
                justify-content: space-between;
                align-items: center;
            ">
                <h2 style="margin: 0; color: var(--cyan); display: flex; align-items: center; gap: 10px;">
                    🔐 API Key Manager
                    <span style="font-size: 12px; color: var(--orange);">Secure Storage</span>
                </h2>
                <button id="api-key-close-btn" style="
                    background: rgba(255, 71, 87, 0.2);
                    border: 1px solid var(--red);
                    color: var(--red);
                    padding: 8px 16px;
                    border-radius: 6px;
                    cursor: pointer;
                    font-weight: bold;
                ">✕ Close</button>
            </div>
            
            <!-- Content Area -->
            <div style="display: flex; flex: 1; overflow: hidden;">
                <!-- Provider List (Left) -->
                <div id="api-key-provider-list" style="
                    width: 280px;
                    border-right: 1px solid rgba(0, 212, 255, 0.2);
                    overflow-y: auto;
                    padding: 10px;
                    background: rgba(5, 5, 15, 0.5);
                "></div>
                
                <!-- Key Input Area (Right) -->
                <div id="api-key-content" style="
                    flex: 1;
                    padding: 30px;
                    overflow-y: auto;
                    display: flex;
                    flex-direction: column;
                "></div>
            </div>
        `;

    this.modal.appendChild(this.panel);
    document.body.appendChild(this.modal);

    // Render provider list
    this.renderProviderList();

    // Setup event handlers
    document.getElementById('api-key-close-btn').onclick = () => this.close();
    this.modal.onclick = (e) => {
      if (e.target === this.modal) this.close();
    };
  }

  renderProviderList() {
    const list = document.getElementById('api-key-provider-list');
    list.innerHTML = `
            <div style="padding: 10px 0; border-bottom: 1px solid rgba(0, 212, 255, 0.1); margin-bottom: 10px;">
                <input type="text" id="api-key-search" placeholder="🔍 Search providers..." style="
                    width: 100%;
                    padding: 8px;
                    background: rgba(0, 212, 255, 0.1);
                    border: 1px solid var(--cyan);
                    border-radius: 6px;
                    color: var(--cyan);
                    box-sizing: border-box;
                " />
            </div>
        `;

    const providers = this.manager.getAllProviders();
    const configured = new Set(this.manager.getConfiguredProviders());

    for (const [key, provider] of Object.entries(providers)) {
      const isConfigured = configured.has(key);
      const item = document.createElement('div');
      item.className = 'api-key-provider-item';
      item.onclick = () => this.selectProvider(key);
      item.style.cssText = `
                padding: 12px;
                margin-bottom: 8px;
                background: ${this.activeTab === key ? 'rgba(0, 212, 255, 0.15)' : 'transparent'};
                border: 1px solid ${this.activeTab === key ? 'var(--cyan)' : 'rgba(0, 212, 255, 0.1)'};
                border-radius: 8px;
                cursor: pointer;
                transition: all 0.2s;
                display: flex;
                justify-content: space-between;
                align-items: center;
            `;

      const label = document.createElement('span');
      label.style.cssText = 'flex: 1; color: var(--cyan);';
      label.innerHTML = `${provider.icon} ${provider.name}`;

      const badge = document.createElement('span');
      badge.style.cssText = `
                width: 12px;
                height: 12px;
                border-radius: 50%;
                background: ${isConfigured ? 'var(--green)' : 'var(--red)'};
                display: inline-block;
            `;

      item.appendChild(label);
      item.appendChild(badge);
      list.appendChild(item);
    }

    // Setup search
    document.getElementById('api-key-search').addEventListener('input', (e) => {
      const search = e.target.value.toLowerCase();
      document.querySelectorAll('.api-key-provider-item').forEach(item => {
        item.style.display = item.textContent.toLowerCase().includes(search) ? '' : 'none';
      });
    });
  }

  selectProvider(key) {
    this.activeTab = key;
    this.renderProviderList();
    this.renderProviderContent(key);
  }

  renderProviderContent(key) {
    const content = document.getElementById('api-key-content');
    const provider = this.manager.getProvider(key);
    const config = this.manager.getConfig(key);

    content.innerHTML = '';

    // Provider header
    const header = document.createElement('div');
    header.style.cssText = `
            padding-bottom: 20px;
            border-bottom: 1px solid rgba(0, 212, 255, 0.2);
            margin-bottom: 20px;
        `;

    header.innerHTML = `
            <h3 style="margin: 0 0 10px 0; color: var(--cyan); font-size: 20px;">
                ${provider.icon} ${provider.name}
            </h3>
            <p style="margin: 0 0 15px 0; color: #888; font-size: 14px;">
                ${provider.description}
            </p>
            <a href="${provider.website}" target="_blank" style="
                color: var(--purple);
                text-decoration: none;
                font-size: 12px;
                display: inline-block;
                padding: 4px 8px;
                border: 1px solid var(--purple);
                border-radius: 4px;
                transition: all 0.2s;
            " onmouseover="this.style.background='rgba(168, 85, 247, 0.2)'" 
               onmouseout="this.style.background='transparent'">
                📖 Get API Key →
            </a>
        `;

    content.appendChild(header);

    // Key input form
    const form = document.createElement('div');
    form.style.cssText = 'flex: 1;';

    // Main API key input
    const keyInput = document.createElement('div');
    keyInput.style.cssText = 'margin-bottom: 20px;';
    keyInput.innerHTML = `
            <label style="display: block; margin-bottom: 8px; color: var(--green); font-weight: bold;">
                API Key / Token
            </label>
            <input type="password" id="api-key-input" placeholder="Paste your API key here..." value="${config?.key || ''}" style="
                width: 100%;
                padding: 12px;
                background: rgba(0, 0, 0, 0.3);
                border: 1px solid var(--cyan);
                border-radius: 6px;
                color: var(--cyan);
                font-family: 'Courier New', monospace;
                font-size: 12px;
                box-sizing: border-box;
            " />
            <small style="color: #888; display: block; margin-top: 8px;">
                🔒 Keys are encrypted in local storage. Never shared or transmitted.
            </small>
        `;

    form.appendChild(keyInput);

    // Additional config fields (AWS, GitHub, etc.)
    if (provider.requiresConfig) {
      for (const field of provider.requiresConfig) {
        const fieldDiv = document.createElement('div');
        fieldDiv.style.cssText = 'margin-bottom: 20px;';

        const label = document.createElement('label');
        label.style.cssText = 'display: block; margin-bottom: 8px; color: var(--green); font-weight: bold;';
        label.textContent = this.formatFieldName(field);

        const input = document.createElement('input');
        input.id = `api-key-field-${field}`;
        input.type = field.includes('password') ? 'password' : 'text';
        input.placeholder = `Enter your ${this.formatFieldName(field).toLowerCase()}...`;
        input.value = config?.[field] || '';
        input.style.cssText = `
                    width: 100%;
                    padding: 12px;
                    background: rgba(0, 0, 0, 0.3);
                    border: 1px solid var(--cyan);
                    border-radius: 6px;
                    color: var(--cyan);
                    box-sizing: border-box;
                `;

        fieldDiv.appendChild(label);
        fieldDiv.appendChild(input);
        form.appendChild(fieldDiv);
      }
    }

    content.appendChild(form);

    // Action buttons
    const buttons = document.createElement('div');
    buttons.style.cssText = `
            display: flex;
            gap: 12px;
            padding-top: 20px;
            border-top: 1px solid rgba(0, 212, 255, 0.2);
        `;

    // Save button
    const saveBtn = document.createElement('button');
    saveBtn.textContent = `✅ ${config ? 'Update' : 'Save'} Key`;
    saveBtn.style.cssText = `
            flex: 1;
            padding: 12px;
            background: linear-gradient(135deg, var(--green), var(--cyan));
            color: var(--void);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-weight: bold;
            font-size: 14px;
            transition: all 0.2s;
        `;

    saveBtn.onmouseover = () => saveBtn.style.opacity = '0.8';
    saveBtn.onmouseout = () => saveBtn.style.opacity = '1';

    saveBtn.onclick = () => {
      const keyValue = document.getElementById('api-key-input').value;
      if (!keyValue.trim()) {
        alert('❌ Please enter an API key');
        return;
      }

      const additionalConfig = {};
      if (provider.requiresConfig) {
        for (const field of provider.requiresConfig) {
          const input = document.getElementById(`api-key-field-${field}`);
          if (input?.value) {
            additionalConfig[field] = input.value;
          }
        }
      }

      this.manager.setKey(key, keyValue, additionalConfig);
      alert(`✅ API key saved for ${provider.name}`);
      this.renderProviderList();
    };

    buttons.appendChild(saveBtn);

    // Test button
    const testBtn = document.createElement('button');
    testBtn.textContent = '🧪 Test Connection';
    testBtn.style.cssText = `
            flex: 1;
            padding: 12px;
            background: rgba(0, 212, 255, 0.1);
            color: var(--cyan);
            border: 1px solid var(--cyan);
            border-radius: 6px;
            cursor: pointer;
            font-weight: bold;
            font-size: 14px;
            transition: all 0.2s;
        `;

    testBtn.onclick = async () => {
      testBtn.disabled = true;
      testBtn.textContent = '⏳ Testing...';
      try {
        const result = await this.testConnection(key, keyValue, additionalConfig);
        if (result.success) {
          alert(`✅ Connection successful!\n${provider.name}\n\n${result.message}`);
        } else {
          alert(`❌ Connection failed\n${provider.name}\n\n${result.message}`);
        }
      } catch (error) {
        alert(`❌ Error testing connection:\n${error.message}`);
      } finally {
        testBtn.disabled = false;
        testBtn.textContent = '🧪 Test Connection';
      }
    };

    buttons.appendChild(testBtn);

    // Remove button (if configured)
    if (config) {
      const removeBtn = document.createElement('button');
      removeBtn.textContent = '🗑️ Remove';
      removeBtn.style.cssText = `
                padding: 12px;
                background: rgba(255, 71, 87, 0.1);
                color: var(--red);
                border: 1px solid var(--red);
                border-radius: 6px;
                cursor: pointer;
                font-weight: bold;
                font-size: 14px;
                transition: all 0.2s;
            `;

      removeBtn.onclick = () => {
        if (confirm(`Remove API key for ${provider.name}?`)) {
          this.manager.removeKey(key);
          alert(`🗑️ API key removed`);
          this.renderProviderList();
          this.selectProvider(Object.keys(this.manager.getAllProviders())[0]);
        }
      };

      buttons.appendChild(removeBtn);
    }

    content.appendChild(buttons);

    // Status info
    if (config) {
      const status = document.createElement('div');
      status.style.cssText = `
                margin-top: 20px;
                padding: 12px;
                background: rgba(0, 255, 136, 0.1);
                border: 1px solid var(--green);
                border-radius: 6px;
                color: var(--green);
                font-size: 12px;
            `;

      status.innerHTML = `
                ✅ <strong>Configured</strong><br/>
                Last updated: ${new Date(config.setAt).toLocaleString()}
            `;

      content.appendChild(status);
    }
  }

  async testConnection(providerKey, apiKey, additionalConfig = {}) {
    const endpoints = {
      openai: {
        url: 'https://api.openai.com/v1/models',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => data.data?.length > 0,
        message: (data) => `Found ${data.data?.length || 0} available models`
      },
      anthropic: {
        url: 'https://api.anthropic.com/v1/models',
        method: 'GET',
        headers: { 'x-api-key': apiKey, 'anthropic-version': '2023-06-01' },
        test: (data) => data.data?.length > 0,
        message: (data) => `Connected to Anthropic Claude API`
      },
      google: {
        url: `https://generativelanguage.googleapis.com/v1/models?key=${apiKey}`,
        method: 'GET',
        test: (data) => data.models?.length > 0,
        message: (data) => `Found ${data.models?.length || 0} Gemini models`
      },
      cohere: {
        url: 'https://api.cohere.ai/v1/models',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => Array.isArray(data),
        message: (data) => `Connected to Cohere API`
      },
      replicate: {
        url: 'https://api.replicate.com/v1/models',
        method: 'GET',
        headers: { 'Authorization': `Token ${apiKey}` },
        test: (data) => data.results?.length > 0,
        message: (data) => `Found ${data.results?.length || 0} Replicate models`
      },
      huggingface: {
        url: 'https://huggingface.co/api/models?search=text-generation&limit=1',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => Array.isArray(data) || data.error === undefined,
        message: (data) => `Connected to HuggingFace Hub API`
      },
      together: {
        url: 'https://api.together.xyz/models/list',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => data.models?.length > 0 || Array.isArray(data),
        message: (data) => `Connected to Together AI API`
      },
      aws: {
        url: null,
        test: (config) => config.region && config.accessKeyId && config.secretAccessKey,
        message: (config) => `AWS Bedrock configured for region: ${config.region}`
      },
      github: {
        url: 'https://api.github.com/user',
        method: 'GET',
        headers: { 'Authorization': `token ${apiKey}`, 'Accept': 'application/vnd.github.v3+json' },
        test: (data) => data.login !== undefined,
        message: (data) => `GitHub user: ${data.login}`
      },
      cursor: {
        url: 'https://api.cursor.sh/v1/user/profile',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => data.email !== undefined,
        message: (data) => `Cursor user: ${data.email}`
      },
      moonshot: {
        url: 'https://api.moonshot.cn/v1/models',
        method: 'GET',
        headers: { 'Authorization': `Bearer ${apiKey}` },
        test: (data) => data.data?.length > 0,
        message: (data) => `Connected to Moonshot (Kimi) API`
      },
      ollama: {
        url: 'http://localhost:11434/api/tags',
        method: 'GET',
        test: (data) => Array.isArray(data.models),
        message: (data) => `Found ${data.models?.length || 0} local Ollama models`
      }
    };

    const config = endpoints[providerKey];
    if (!config) {
      return { success: false, message: `No test configuration for ${providerKey}` };
    }

    // AWS is local, no HTTP test
    if (providerKey === 'aws') {
      return {
        success: config.test(additionalConfig),
        message: config.message(additionalConfig)
      };
    }

    // Ollama is local HTTP, no CORS issues
    if (providerKey === 'ollama') {
      try {
        const response = await fetch(config.url, { method: config.method });
        const data = await response.json();
        return {
          success: config.test(data),
          message: config.message(data)
        };
      } catch (error) {
        return {
          success: false,
          message: `Cannot connect to Ollama on localhost:11434\n${error.message}`
        };
      }
    }

    // Other APIs require server-side proxy to bypass CORS
    // This would call a backend endpoint
    try {
      const response = await fetch('http://localhost:3000/api/test-connection', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          provider: providerKey,
          apiKey: apiKey,
          config: additionalConfig,
          endpoint: config.url,
          headers: config.headers
        })
      });

      const data = await response.json();
      if (data.success) {
        return {
          success: true,
          message: config.message(data.response)
        };
      } else {
        return {
          success: false,
          message: `API Error: ${data.error || 'Unknown error'}`
        };
      }
    } catch (error) {
      return {
        success: false,
        message: `Test requires local server proxy.\nStart Micro-Model-Server for full functionality.\n\nError: ${error.message}`
      };
    }
  }

  formatFieldName(field) {
    return field.split(/(?=[A-Z])/).map(word =>
      word.charAt(0).toUpperCase() + word.slice(1)
    ).join(' ');
  }

  open() {
    this.isOpen = true;
    this.modal.style.display = 'flex';

    // Select first provider
    const providers = Object.keys(this.manager.getAllProviders());
    if (providers.length > 0) {
      this.selectProvider(providers[0]);
    }
  }

  close() {
    this.isOpen = false;
    this.modal.style.display = 'none';
  }

  toggle() {
    this.isOpen ? this.close() : this.open();
  }
}

// Initialize on page load
let apiKeySettingsPanel = null;
if (typeof window !== 'undefined') {
  window.addEventListener('DOMContentLoaded', () => {
    apiKeySettingsPanel = new APIKeySettingsPanel();
    window.apiKeySettingsPanel = apiKeySettingsPanel;
  });
}

console.log('[APIKeySettingsPanel] ✅ API Key Settings Panel initialized');
