const axios = require('axios');
const path = require('path');

function loadConfig() {
  try {
    return require(path.join(__dirname, '../../config/providers.json'));
  } catch {
    return {
      bigdaddyg: { url: 'http://localhost:11434', model: 'bigdaddyg', enabled: true }
    };
  }
}

class OllamaCompatibleProvider {
  constructor(id, displayName, config = {}, fallbackModel) {
    this.id = id;
    this.name = displayName;
    this.model = config.model || fallbackModel;
    this.baseUrl = config.url || 'http://localhost:11434';
    this.enabled = config.enabled !== false && Boolean(this.model);
    this.timeoutMs = Number(config.timeoutMs || 60000);
    this.capabilities = config.capabilities || ['code', 'plan', 'debug', 'test', 'explain'];
  }

  isConfigured() {
    return this.enabled;
  }

  setConfig(config = {}) {
    if (typeof config.enabled === 'boolean') {
      this.enabled = config.enabled;
    }
    if (typeof config.model === 'string' && config.model.trim()) {
      this.model = config.model.trim();
    }
    if (typeof config.url === 'string' && config.url.trim()) {
      this.baseUrl = config.url.trim().replace(/\/+$/, '');
    }
    if (typeof config.timeoutMs === 'number' && Number.isFinite(config.timeoutMs)) {
      this.timeoutMs = config.timeoutMs;
    }
  }

  formatPrompt(prompt, context = {}) {
    const projectContext = context.projectContext
      ? JSON.stringify(context.projectContext, null, 2)
      : '{}';
    return `You are ${this.name}, an expert coding assistant.
Project Context:
${projectContext}

User Request:
${prompt}

Return only the direct answer.`;
  }

  async invoke(prompt, context = {}) {
    if (!this.isConfigured()) {
      throw new Error(`${this.name} is disabled or missing model configuration`);
    }

    const response = await axios.post(
      `${this.baseUrl}/api/generate`,
      {
        model: this.model,
        prompt: this.formatPrompt(prompt, context),
        stream: false,
        options: {
          temperature: 0.1,
          top_k: 40,
          top_p: 0.9,
          num_ctx: 8192
        }
      },
      { timeout: this.timeoutMs }
    );

    return {
      content: response?.data?.response || '',
      model: this.model,
      provider: this.id,
      tokens: response?.data?.eval_count || 0
    };
  }
}

class AIProviderManager {
  constructor() {
    this.config = loadConfig();
    this.providers = {
      bigdaddyg: new OllamaCompatibleProvider(
        'bigdaddyg',
        'BigDaddyG (Ollama)',
        this.config.bigdaddyg,
        'bigdaddyg'
      ),
      copilot: new OllamaCompatibleProvider(
        'copilot',
        'GitHub Copilot (Compatible)',
        this.config.copilot,
        'codellama'
      ),
      amazonq: new OllamaCompatibleProvider(
        'amazonq',
        'Amazon Q (Compatible)',
        this.config.amazonq,
        'qwen2.5-coder'
      ),
      cursor: new OllamaCompatibleProvider(
        'cursor',
        'Cursor (Compatible)',
        this.config.cursor,
        'deepseek-coder'
      )
    };

    this.activeProvider = this.resolveInitialActiveProvider();
  }

  resolveInitialActiveProvider() {
    if (this.providers.bigdaddyg?.isConfigured()) {
      return 'bigdaddyg';
    }
    const firstEnabled = Object.entries(this.providers).find(([, p]) => p.isConfigured());
    return firstEnabled ? firstEnabled[0] : 'bigdaddyg';
  }

  getActiveProviderId() {
    return this.activeProvider;
  }

  async invoke(providerName, prompt, context = {}) {
    const providerId = providerName || this.activeProvider;
    const provider = this.providers[providerId];
    if (!provider) {
      throw new Error(`Provider ${providerId} not found`);
    }

    if (!provider.isConfigured()) {
      throw new Error(`Provider ${provider.name} is not configured or disabled`);
    }

    try {
      return await provider.invoke(prompt, {
        ...context,
        projectContext: context.projectContext || {}
      });
    } catch (error) {
      console.error('AI Provider error:', error);
      throw error;
    }
  }

  getAvailableProviders() {
    return Object.entries(this.providers).map(([id, provider]) => ({
      id,
      name: provider.name,
      capabilities: provider.capabilities,
      enabled: provider.isConfigured(),
      active: this.activeProvider === id
    }));
  }

  setActiveProvider(providerId) {
    const provider = this.providers[providerId];
    if (!provider || !provider.isConfigured()) {
      return false;
    }
    this.activeProvider = providerId;
    return true;
  }
}

module.exports = AIProviderManager;
