const axios = require('axios');
const path = require('path');

function loadConfig() {
  try {
    return require(path.join(__dirname, '../../config/providers.json'));
  } catch {
    return {
      bigdaddyg: { url: 'http://localhost:11434', model: 'bigdaddyg', enabled: true },
      experimental: {}
    };
  }
}

class OllamaProvider {
  constructor(modelName = 'bigdaddyg') {
    this.name = 'BigDaddyG (Ollama)';
    this.model = modelName;
    this.capabilities = ['code', 'plan', 'debug', 'test', 'explain'];
    this.baseUrl = 'http://localhost:11434';
  }

  async invoke(prompt, context = {}) {
    const response = await axios.post(`${this.baseUrl}/api/generate`, {
      model: this.model,
      prompt: this.formatPrompt(prompt, context),
      stream: false,
      options: {
        temperature: 0.1,
        top_k: 40,
        top_p: 0.9,
        num_ctx: 8192
      }
    }, { timeout: 60000 });

    return {
      content: response.data.response,
      model: this.model,
      tokens: response.data.eval_count
    };
  }

  formatPrompt(prompt, context) {
    const projectContext = context.projectContext ? JSON.stringify(context.projectContext, null, 2) : '{}';
    return `You are BigDaddyG, an expert AI coding assistant.
Project Context: ${projectContext}

User Request: ${prompt}

Provide a helpful, accurate response:`;
  }

  isConfigured() {
    return true;
  }
}

class CopilotProvider {
  constructor() {
    this.name = 'GitHub Copilot';
    this.capabilities = ['code', 'suggest'];
  }

  async invoke(prompt, context) {
    throw new Error('Copilot integration not yet implemented');
  }

  isConfigured() {
    return false;
  }
}

class AmazonQProvider {
  constructor() {
    this.name = 'Amazon Q';
    this.capabilities = ['code', 'plan', 'aws'];
  }

  async invoke(prompt, context) {
    throw new Error('Amazon Q integration not yet implemented');
  }

  isConfigured() {
    return false;
  }
}

class CursorProvider {
  constructor() {
    this.name = 'Cursor';
    this.capabilities = ['code', 'edit', 'debug'];
  }

  async invoke(prompt, context) {
    throw new Error('Cursor integration not yet implemented');
  }

  isConfigured() {
    return false;
  }
}

class AIProviderManager {
  constructor() {
    this.config = loadConfig();
    this.providers = {
      bigdaddyg: new OllamaProvider(this.config.bigdaddyg?.model || 'bigdaddyg'),
      copilot: new CopilotProvider(),
      amazonq: new AmazonQProvider(),
      cursor: new CursorProvider()
    };
    if (this.config.bigdaddyg?.url) {
      this.providers.bigdaddyg.baseUrl = this.config.bigdaddyg.url;
    }
    this.activeProvider = 'bigdaddyg';
  }

  async invoke(providerName, prompt, context = {}) {
    const provider = this.providers[providerName || this.activeProvider];
    if (!provider) {
      throw new Error(`Provider ${providerName || this.activeProvider} not found`);
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
      enabled: provider.isConfigured()
    }));
  }

  setActiveProvider(providerId) {
    if (this.providers[providerId]) {
      this.activeProvider = providerId;
      return true;
    }
    return false;
  }
}

module.exports = AIProviderManager;
