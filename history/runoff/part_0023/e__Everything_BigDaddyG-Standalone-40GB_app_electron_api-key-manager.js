/**
 * BigDaddyG IDE - API Key Manager
 * Secure storage and management of API keys for major AI providers
 * Supports: OpenAI, Claude (Anthropic), AWS, GitHub Copilot, Cursor, Moonshot, Google Gemini, Cohere, etc.
 */

class APIKeyManager {
  constructor() {
    this.providers = {
      // OpenAI / ChatGPT
      openai: {
        name: 'OpenAI (ChatGPT)',
        icon: '🤖',
        website: 'https://platform.openai.com/api-keys',
        models: ['gpt-4-turbo', 'gpt-4', 'gpt-3.5-turbo'],
        description: 'Access to ChatGPT, GPT-4, and other OpenAI models',
        colors: { primary: '#00a67e', secondary: '#ffffff' }
      },

      // Anthropic Claude
      anthropic: {
        name: 'Anthropic (Claude)',
        icon: '🧠',
        website: 'https://console.anthropic.com/account/keys',
        models: ['claude-3-opus', 'claude-3-sonnet', 'claude-3-haiku'],
        description: 'Access to Claude 3 models with advanced reasoning',
        colors: { primary: '#1f1f1f', secondary: '#ffffff' }
      },

      // AWS Bedrock
      aws: {
        name: 'AWS (Bedrock)',
        icon: '☁️',
        website: 'https://console.aws.amazon.com/bedrock',
        models: ['claude-on-bedrock', 'titan', 'llama2', 'mistral'],
        description: 'Access to multiple models via AWS Bedrock',
        colors: { primary: '#ff9900', secondary: '#000000' },
        requiresConfig: ['region', 'accessKeyId', 'secretAccessKey']
      },

      // GitHub Copilot
      github: {
        name: 'GitHub Copilot',
        icon: '🐙',
        website: 'https://github.com/settings/copilot',
        models: ['copilot-gpt4', 'copilot-gpt3.5'],
        description: 'Code completion and generation with GitHub Copilot',
        colors: { primary: '#24292e', secondary: '#ffffff' },
        requiresConfig: ['githubToken']
      },

      // Cursor Pro
      cursor: {
        name: 'Cursor Pro',
        icon: '🎯',
        website: 'https://www.cursor.com/settings/account',
        models: ['cursor-gpt4', 'cursor-claude'],
        description: 'AI-powered code editing with Cursor integration',
        colors: { primary: '#5865f2', secondary: '#ffffff' },
        requiresConfig: ['cursorToken']
      },

      // Moonshot (Kimi)
      moonshot: {
        name: 'Moonshot (Kimi)',
        icon: '🌙',
        website: 'https://platform.moonshot.cn/console/api-keys',
        models: ['moonshot-v1-8k', 'moonshot-v1-32k', 'moonshot-v1-128k'],
        description: 'High-speed multilingual AI with long context window',
        colors: { primary: '#3b82f6', secondary: '#ffffff' }
      },

      // Google Gemini
      google: {
        name: 'Google Gemini',
        icon: '🔮',
        website: 'https://makersuite.google.com/app/apikey',
        models: ['gemini-pro', 'gemini-pro-vision'],
        description: 'Google\'s advanced multimodal AI models',
        colors: { primary: '#4285f4', secondary: '#ffffff' }
      },

      // Cohere
      cohere: {
        name: 'Cohere',
        icon: '🌐',
        website: 'https://dashboard.cohere.com/api-keys',
        models: ['command-r-plus', 'command-r', 'command-nightly'],
        description: 'Enterprise-grade NLP and language models',
        colors: { primary: '#ffa500', secondary: '#ffffff' }
      },

      // Together AI
      together: {
        name: 'Together AI',
        icon: '⚡',
        website: 'https://www.together.ai/settings/api-keys',
        models: ['meta-llama/llama-2-70b', 'mistral-7b', 'falcon-40b'],
        description: 'Open-source model serving and fine-tuning',
        colors: { primary: '#ff6b6b', secondary: '#ffffff' }
      },

      // HuggingFace
      huggingface: {
        name: 'HuggingFace',
        icon: '🤗',
        website: 'https://huggingface.co/settings/tokens',
        models: ['meta-llama/Llama-2-7b', 'mistral-7b', 'zephyr-7b'],
        description: 'Access to thousands of open-source models',
        colors: { primary: '#ffd21e', secondary: '#000000' }
      },

      // Replicate
      replicate: {
        name: 'Replicate',
        icon: '🎬',
        website: 'https://replicate.com/account/api-tokens',
        models: ['stable-diffusion', 'codellama', 'llama-2'],
        description: 'Run open-source models in the cloud',
        colors: { primary: '#620e8d', secondary: '#ffffff' }
      },

      // Ollama (Local)
      ollama: {
        name: 'Ollama (Local)',
        icon: '🦙',
        website: 'https://ollama.ai',
        models: ['llama2', 'mistral', 'neural-chat', 'starling'],
        description: 'Local model serving - no API key needed',
        colors: { primary: '#4169e1', secondary: '#ffffff' },
        isLocal: true
      }
    };

    this.keys = new Map();
    this.activeProvider = null;
    this.loadKeys();
  }

  /**
   * Load API keys from secure storage
   */
  loadKeys() {
    try {
      const stored = localStorage.getItem('api-keys-encrypted');
      if (stored) {
        // In production, decrypt this
        const decrypted = this.decryptKeys(stored);
        this.keys = new Map(decrypted);
      }
    } catch (error) {
      console.warn('[APIKeyManager] Could not load stored keys:', error.message);
    }
  }

  /**
   * Save API keys to secure storage
   */
  saveKeys() {
    try {
      const data = Array.from(this.keys.entries());
      const encrypted = this.encryptKeys(data);
      localStorage.setItem('api-keys-encrypted', encrypted);
      console.log('[APIKeyManager] ✅ Keys saved securely');
    } catch (error) {
      console.error('[APIKeyManager] ❌ Error saving keys:', error.message);
    }
  }

  /**
   * Simple encryption (base64 + obfuscation)
   * In production, use proper encryption like TweetNaCl.js
   */
  encryptKeys(data) {
    const json = JSON.stringify(data);
    // Simple obfuscation - NOT production-grade!
    return btoa(json);
  }

  /**
   * Decrypt stored keys
   */
  decryptKeys(encrypted) {
    try {
      const json = atob(encrypted);
      return JSON.parse(json);
    } catch (error) {
      console.warn('[APIKeyManager] Decryption failed:', error.message);
      return [];
    }
  }

  /**
   * Set API key for a provider
   */
  setKey(provider, key, additionalConfig = {}) {
    if (!this.providers[provider]) {
      throw new Error(`Unknown provider: ${provider}`);
    }

    const config = {
      key: key,
      setAt: new Date().toISOString(),
      ...additionalConfig
    };

    this.keys.set(provider, config);
    this.saveKeys();

    console.log(`[APIKeyManager] ✅ Key set for ${provider}`);
    return true;
  }

  /**
   * Get API key for a provider
   */
  getKey(provider) {
    const config = this.keys.get(provider);
    return config ? config.key : null;
  }

  /**
   * Get full config for a provider
   */
  getConfig(provider) {
    return this.keys.get(provider) || null;
  }

  /**
   * Check if provider is configured
   */
  isConfigured(provider) {
    return this.keys.has(provider);
  }

  /**
   * Get all configured providers
   */
  getConfiguredProviders() {
    return Array.from(this.keys.keys());
  }

  /**
   * Remove API key for a provider
   */
  removeKey(provider) {
    this.keys.delete(provider);
    this.saveKeys();
    console.log(`[APIKeyManager] 🗑️ Key removed for ${provider}`);
    return true;
  }

  /**
   * Get provider metadata
   */
  getProvider(provider) {
    return this.providers[provider] || null;
  }

  /**
   * Get all providers
   */
  getAllProviders() {
    return this.providers;
  }
}

// Initialize on page load
let apiKeyManager = null;
if (typeof window !== 'undefined') {
  window.apiKeyManager = new APIKeyManager();
  apiKeyManager = window.apiKeyManager;
}

console.log('[APIKeyManager] ✅ API Key Manager initialized');
