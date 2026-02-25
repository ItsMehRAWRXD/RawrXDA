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
    this.apiKeyRecords = {};
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

    // OpenAI
    this.providers.set('openai', {
      name: 'OpenAI',
      type: 'cloud',
      endpoint: 'https://api.openai.com/v1/chat/completions',
      requiresKey: true,
      keyId: 'openai',
      defaultModel: 'gpt-4o-mini'
    });

    // Anthropic Claude
    this.providers.set('anthropic', {
      name: 'Anthropic Claude',
      type: 'cloud',
      endpoint: 'https://api.anthropic.com/v1/messages',
      requiresKey: true,
      keyId: 'anthropic',
      defaultModel: 'claude-3-sonnet-20240229'
    });

    // Google Gemini
    this.providers.set('gemini', {
      name: 'Google Gemini',
      type: 'cloud',
      endpoint: 'https://generativelanguage.googleapis.com/v1beta',
      requiresKey: true,
      keyId: 'gemini',
      defaultModel: 'gemini-1.5-flash'
    });

    // Groq
    this.providers.set('groq', {
      name: 'Groq',
      type: 'cloud',
      endpoint: 'https://api.groq.com/openai/v1/chat/completions',
      requiresKey: true,
      keyId: 'groq',
      defaultModel: 'mixtral-8x7b-32768'
    });

    // DeepSeek
    this.providers.set('deepseek', {
      name: 'DeepSeek',
      type: 'cloud',
      endpoint: 'https://api.deepseek.com/v1/chat/completions',
      requiresKey: true,
      keyId: 'deepseek',
      defaultModel: 'deepseek-chat'
    });
  }

  async loadApiKeys() {
    try {
      if (window.electron?.apikeys?.list) {
        const result = await window.electron.apikeys.list();
        if (result && result.success !== false) {
          const rawKeys = result.keys || {};
          this.apiKeys = Object.keys(rawKeys).reduce((acc, provider) => {
            acc[provider] = rawKeys[provider].key || rawKeys[provider];
            return acc;
          }, {});
          return;
        }
      }
    } catch (error) {
      console.warn('[AI] Failed to load API keys from main process, falling back to localStorage', error);
    }

    try {
      const stored = localStorage.getItem('aiProviderApiKeys');
      this.apiKeys = stored ? JSON.parse(stored) : {};
    } catch (error) {
      this.apiKeys = {};
    }
  }

  async saveApiKey(provider, key, metadata = {}) {
    this.apiKeys[provider] = key;
    try {
      if (window.electron?.apikeys?.set) {
        const result = await window.electron.apikeys.set(provider, key, metadata);
        if (result && result.success === false) {
          throw new Error(result.error || 'Failed to persist API key');
        }
      } else {
        localStorage.setItem('aiProviderApiKeys', JSON.stringify(this.apiKeys));
      }
    } catch (error) {
      console.error('[AI] Failed to save API key:', error);
      throw error;
    }
  }

  async deleteApiKey(provider) {
    delete this.apiKeys[provider];
    try {
      if (window.electron?.apikeys?.delete) {
        const result = await window.electron.apikeys.delete(provider);
        if (result && result.success === false) {
          throw new Error(result.error || 'Failed to remove API key');
        }
      } else {
        localStorage.setItem('aiProviderApiKeys', JSON.stringify(this.apiKeys));
      }
    } catch (error) {
      console.error('[AI] Failed to delete API key:', error);
      throw error;
    }
  }

  getApiKey(provider) {
    return this.apiKeys?.[provider] || '';
  }

  async refreshModelCatalog() {
    if (!window.electron?.models?.discover) {
      return;
    }
    try {
      const result = await window.electron.models.discover();
      if (result && result.success !== false) {
        const catalog = result.catalog || result.catalogue || {};
        this.modelCatalog = catalog;

        const diskSource = result.ollama?.disk ?? catalog.ollama ?? [];
        if (Array.isArray(diskSource)) {
          this.ollamaDiskModels = diskSource;
        } else if (Array.isArray(diskSource?.models)) {
          this.ollamaDiskModels = diskSource.models;
        } else {
          this.ollamaDiskModels = [];
        }

        const serverSource = result.ollama?.server ?? [];
        if (Array.isArray(serverSource)) {
          this.ollamaServerModels = serverSource
            .filter(model => model && typeof model === 'object')
            .map(model => ({
              name: model.name,
              size: model.size,
              digest: model.digest,
              modified: model.modified_at || model.modified
            }));
        } else {
          this.ollamaServerModels = [];
        }

        const orchestraModels = result.orchestra?.models;
        this.orchestraModels = Array.isArray(orchestraModels) ? orchestraModels : [];
        this.orchestraAvailable = result.orchestra?.available !== false;
      }
    } catch (error) {
      console.warn('[AI] Failed to refresh model catalog', error);
    }
  }

  async discoverOllamaModels() {
    try {
      const res = await fetch('http://localhost:11434/api/tags');
      if (!res.ok) {
        throw new Error(`HTTP ${res.status}`);
      }
      const data = await res.json();
      this.ollamaServerModels = (data.models || []).map(model => ({
        name: model.name,
        size: model.size,
        digest: model.digest,
        modified: model.modified_at
      }));
    } catch (error) {
      this.ollamaServerModels = [];
    }

    const diskNames = Array.isArray(this.ollamaDiskModels)
      ? this.ollamaDiskModels
          .filter(model => model && typeof model === 'object')
          .map(model => model.name || model.path || model.id)
          .filter(Boolean)
      : [];
    const serverNames = Array.isArray(this.ollamaServerModels)
      ? this.ollamaServerModels
          .filter(model => model && typeof model === 'object')
          .map(model => model.name)
          .filter(Boolean)
      : [];
    this.ollamaModels = Array.from(new Set([...serverNames, ...diskNames]));

    console.log('[AI] Ollama models:', this.ollamaModels);
  }

  async chat(message, options = {}) {
    const provider = options.provider || this.activeProvider || 'ollama';
    const model = options.model || this.getDefaultModel(provider);

    console.log(`[AI] Chat request: provider=${provider}, model=${model}`);

    switch (provider) {
      case 'ollama':
        return this.chatOllama(message, model, options);
      case 'orchestra':
        return this.chatOrchestra(message, model, options);
      case 'amazonq':
      case 'copilot':
        return this.chatExtension(provider, message, options);
      case 'openai':
        return this.chatOpenAI(message, model, options);
      case 'anthropic':
        return this.chatAnthropic(message, model, options);
      case 'gemini':
        return this.chatGemini(message, model, options);
      case 'groq':
        return this.chatGroq(message, model, options);
      case 'deepseek':
        return this.chatDeepSeek(message, model, options);
      default:
        throw new Error(`Unknown AI provider: ${provider}`);
    }
  }

  getDefaultModel(provider) {
    const defaults = {
      ollama: 'llama3.2',
      openai: 'gpt-4o-mini',
      anthropic: 'claude-3-sonnet-20240229',
      gemini: 'gemini-1.5-flash',
      groq: 'mixtral-8x7b-32768',
      deepseek: 'deepseek-chat'
    };
    return defaults[provider] || 'llama3.2';
  }

  async chatWithFallback(message, options = {}) {
    const providers = ['openai', 'anthropic', 'groq', 'ollama'];
    for (const provider of providers) {
      try {
        if (provider !== 'ollama' && !this.getApiKey(provider)) continue;
        return await this.chat(message, { ...options, provider });
      } catch (error) {
        console.warn(`[AI] ${provider} failed, trying next...`);
      }
    }
    throw new Error('All AI providers failed');
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

  async chatOpenAI(message, model, options = {}) {
    const apiKey = this.getApiKey('openai');
    if (!apiKey) {
      throw new Error('OpenAI API key not configured');
    }
    const payload = {
      model: model || this.providers.get('openai')?.defaultModel || 'gpt-4o-mini',
      messages: [{ role: 'user', content: message }],
      temperature: options.temperature ?? 0.7,
      max_tokens: options.maxTokens ?? 1024,
      stream: false
    };
    const res = await fetch('https://api.openai.com/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${apiKey}`
      },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error?.message || 'OpenAI request failed');
    }
    const content = data.choices?.[0]?.message?.content || '';
    return { response: content.trim(), provider: 'openai', model: payload.model, raw: data };
  }

  async chatAnthropic(message, model, options = {}) {
    const apiKey = this.getApiKey('anthropic');
    if (!apiKey) {
      throw new Error('Anthropic API key not configured');
    }
    const payload = {
      model: model || this.providers.get('anthropic')?.defaultModel || 'claude-3-sonnet-20240229',
      max_tokens: options.maxTokens ?? 1024,
      temperature: options.temperature ?? 0.7,
      messages: [{ role: 'user', content: message }]
    };
    const res = await fetch('https://api.anthropic.com/v1/messages', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'x-api-key': apiKey,
        'anthropic-version': '2023-06-01'
      },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error?.message || 'Anthropic request failed');
    }
    const content = data.content?.[0]?.text || '';
    return { response: content.trim(), provider: 'anthropic', model: payload.model, raw: data };
  }

  async chatGemini(message, model, options = {}) {
    const apiKey = this.getApiKey('gemini');
    if (!apiKey) {
      throw new Error('Gemini API key not configured');
    }
    const finalModel = model || this.providers.get('gemini')?.defaultModel || 'gemini-1.5-flash';
    const url = `https://generativelanguage.googleapis.com/v1beta/models/${finalModel}:generateContent?key=${apiKey}`;
    const payload = {
      contents: [
        {
          role: 'user',
          parts: [{ text: message }]
        }
      ],
      generationConfig: {
        temperature: options.temperature ?? 0.7,
        maxOutputTokens: options.maxTokens ?? 1024
      }
    };
    const res = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error?.message || 'Gemini request failed');
    }
    const text = data.candidates?.[0]?.content?.parts?.map(part => part.text || '').join('\n') || '';
    return { response: text.trim(), provider: 'gemini', model: finalModel, raw: data };
  }

  async chatGroq(message, model, options = {}) {
    const apiKey = this.getApiKey('groq');
    if (!apiKey) {
      throw new Error('Groq API key not configured');
    }
    const payload = {
      model: model || this.providers.get('groq')?.defaultModel || 'mixtral-8x7b-32768',
      messages: [{ role: 'user', content: message }],
      temperature: options.temperature ?? 0.7,
      max_tokens: options.maxTokens ?? 1024
    };
    const res = await fetch('https://api.groq.com/openai/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${apiKey}`
      },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error?.message || 'Groq request failed');
    }
    const content = data.choices?.[0]?.message?.content || '';
    return { response: content.trim(), provider: 'groq', model: payload.model, raw: data };
  }

  async chatDeepSeek(message, model, options = {}) {
    const apiKey = this.getApiKey('deepseek');
    if (!apiKey) {
      throw new Error('DeepSeek API key not configured');
    }
    const payload = {
      model: model || this.providers.get('deepseek')?.defaultModel || 'deepseek-chat',
      messages: [{ role: 'user', content: message }],
      temperature: options.temperature ?? 0.7,
      max_tokens: options.maxTokens ?? 1024
    };
    const res = await fetch('https://api.deepseek.com/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${apiKey}`
      },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) {
      throw new Error(data.error?.message || 'DeepSeek request failed');
    }
    const content = data.choices?.[0]?.message?.content || '';
    return { response: content.trim(), provider: 'deepseek', model: payload.model, raw: data };
  }

  async chatExtension(provider, message, options) {
    if (window.electron?.marketplace) {
      const extId = provider === 'amazonq' 
        ? 'amazonwebservices.amazon-q-vscode' 
        : 'github.copilot';
      
      try {
        const status = await window.electron.marketplace.status();
        const installed = status.installed?.find(ext => ext.id === extId);
        const state = status.states?.[extId];
        
        if (!installed) {
          throw new Error(`${provider} not installed. Install from marketplace (Ctrl+Shift+P)`);
        }
        if (state && !state.enabled) {
          throw new Error(`${provider} is disabled. Enable it from marketplace.`);
        }
      } catch (error) {
        console.warn(`[AI] Extension check failed:`, error);
      }
    }

    if (window.vscode?.extensions) {
      const extId = provider === 'amazonq' 
        ? 'amazonwebservices.amazon-q-vscode' 
        : 'github.copilot';
      const ext = window.vscode.extensions.getExtension(extId);
      if (ext?.isActive && ext.exports?.chat) {
        return ext.exports.chat(message, options);
      }
    }
    throw new Error(`${provider} extension not available or not activated`);
  }

  getExtensionAuth(provider) {
    // Extensions handle their own auth
    return null;
  }

  getAvailableModels() {
    return {
      ollama: {
        combined: this.ollamaModels,
        server: this.ollamaServerModels,
        disk: this.ollamaDiskModels
      },
      orchestra: {
        available: this.orchestraAvailable,
        models: this.orchestraModels
      },
      builtin: {
        bigdaddyg: this.modelCatalog?.bigdaddyg || null
      },
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
  
  if (!window.aiProviderManager) {
    window.aiProviderManager = new AIProviderManager();
    window.aiProviderManager.initialize().then(() => {
      console.log('[AI] Provider manager ready');
    }).catch(err => {
      console.error('[AI] Provider manager init failed:', err);
    });
  }
}
