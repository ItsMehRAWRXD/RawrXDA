/**
 * AI Provider Manager - Minimal integration for Amazon Q, GitHub Copilot, and Ollama
 */

class AIProviderManager {
  constructor() {
    this.providers = new Map();
    this.activeProvider = null;
    this.ollamaModels = [];
    this.ollamaDiskModels = [];
    this.ollamaServerModels = [];
    this.orchestraModels = [];
    this.orchestraAvailable = false;
    this.modelCatalog = {};
    this.apiKeys = {};
  }

  async initialize() {
    await this.loadApiKeys();
    await this.registerProviders();
    await this.refreshModelCatalog();
    await this.discoverOllamaModels();
    console.log('[AI] Providers ready:', Array.from(this.providers.keys()));
  }

  async registerProviders() {
    // Amazon Q
    this.providers.set('amazonq', {
      name: 'Amazon Q',
      type: 'extension',
      endpoint: null,
      auth: () => this.getExtensionAuth('amazonq')
    });

    // GitHub Copilot
    this.providers.set('copilot', {
      name: 'GitHub Copilot',
      type: 'extension',
      endpoint: null,
      auth: () => this.getExtensionAuth('copilot')
    });

    // Ollama (local)
    this.providers.set('ollama', {
      name: 'Ollama',
      type: 'local',
      endpoint: 'http://localhost:11434',
      auth: null
    });

    // Orchestra (BigDaddyG)
    this.providers.set('orchestra', {
      name: 'Orchestra',
      type: 'local',
      endpoint: 'http://localhost:11441',
      auth: null
    });
  }

  async discoverOllamaModels() {
    try {
      const res = await fetch('http://localhost:11434/api/tags');
      const data = await res.json();
      this.ollamaModels = data.models?.map(m => m.name) || [];
      console.log('[AI] Ollama models:', this.ollamaModels);
    } catch {
      this.ollamaModels = [];
    }
  }

  async chat(message, options = {}) {
    const provider = options.provider || this.activeProvider || 'ollama';
    const model = options.model || 'llama3.2';

    if (provider === 'ollama') {
      return this.chatOllama(message, model);
    } else if (provider === 'orchestra') {
      return this.chatOrchestra(message, model);
    } else if (provider === 'amazonq' || provider === 'copilot') {
      return this.chatExtension(provider, message, options);
    }
  }

  async chatOllama(message, model) {
    const res = await fetch('http://localhost:11434/api/generate', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ model, prompt: message, stream: false })
    });
    const data = await res.json();
    return { response: data.response, provider: 'ollama', model };
  }

  async chatOrchestra(message, model) {
    const res = await fetch('http://localhost:11441/api/chat', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ message, model })
    });
    const data = await res.json();
    return { response: data.response, provider: 'orchestra', model };
  }

  async chatExtension(provider, message, options) {
    // Delegate to extension via VS Code API
    if (window.vscode?.extensions) {
      const ext = window.vscode.extensions.getExtension(
        provider === 'amazonq' ? 'amazonwebservices.amazon-q-vscode' : 'github.copilot'
      );
      if (ext?.isActive) {
        return ext.exports?.chat?.(message, options);
      }
    }
    throw new Error(`${provider} extension not available`);
  }

  getExtensionAuth(provider) {
    // Extensions handle their own auth
    return null;
  }

  getAvailableModels() {
    return {
      ollama: this.ollamaModels,
      builtin: ['BigDaddyG', 'Orchestra'],
      extensions: ['Amazon Q', 'GitHub Copilot']
    };
  }

  setActiveProvider(provider) {
    if (this.providers.has(provider)) {
      this.activeProvider = provider;
      console.log('[AI] Active provider:', provider);
    }
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = AIProviderManager;
} else {
  window.AIProviderManager = AIProviderManager;
}
