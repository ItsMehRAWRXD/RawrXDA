/**
 * Ollama Client for Local AI - NO RATE LIMITS
 * Drop-in replacement for OpenAI client using local models
 */

const axios = require('axios');
const { EventEmitter } = require('events');

class OllamaClient extends EventEmitter {
  constructor(config = {}) {
    super();
    this.endpoint = config.endpoint || process.env.OLLAMA_ENDPOINT || 'http://localhost:11434';
    this.agenticModel = config.agenticModel || process.env.OLLAMA_AGENTIC_MODEL || 'cheetah-stealth-agentic:latest';
    this.standardModel = config.standardModel || process.env.OLLAMA_STANDARD_MODEL || 'bigdaddyg-fast:latest';
    this.timeout = config.timeout || 120000;

    // Retry configuration for timeout scenarios
    this.maxRetries = config.maxRetries || 3;
    this.initialDelay = config.initialDelay || 2000; // 2 seconds for local models
    this.maxDelay = config.maxDelay || 30000; // 30 seconds max
    this.retryableErrors = config.retryableErrors || ['ECONNRESET', 'ETIMEDOUT', 'ENETUNREACH'];

    this.client = axios.create({
      baseURL: this.endpoint,
      timeout: this.timeout
    });
  }

  /**
   * Retry logic for local Ollama connections (mainly for timeouts)
   */
  async _retryWithBackoff(fn, attempt = 1) {
    try {
      return await fn();
    } catch (error) {
      const isRetryable = this._isRetryableError(error);

      if (!isRetryable || attempt > this.maxRetries) {
        this.emit('error', `Ollama request failed after ${attempt} attempts: ${error.message}`);
        throw error;
      }

      const delay = Math.min(
        this.initialDelay * Math.pow(2, attempt - 1),
        this.maxDelay
      );

      this.emit('warn', `Attempt ${attempt} failed (${error.message}). Retrying in ${delay}ms`);

      await new Promise(resolve => setTimeout(resolve, delay));

      return this._retryWithBackoff(fn, attempt + 1);
    }
  }

  /**
   * Check if error is retryable for Ollama
   */
  _isRetryableError(error) {
    // Rate limit errors are retryable
    if (error.response && error.response.status === 429) {
      return true;
    }

    // Server errors are retryable
    if (error.response && error.response.status >= 500) {
      return true;
    }

    // Network/connection errors are retryable
    if (error.code && this.retryableErrors.includes(error.code)) {
      return true;
    }

    // Timeout errors are retryable
    if (error.code === 'ECONNABORTED' || error.message.includes('timeout')) {
      return true;
    }

    return false;
  }

  /**
   * Chat completion with local Ollama model
   */
  async chat(messages, options = {}) {
    const startTime = Date.now();
    const requestId = this.generateRequestId();

    try {
      this.emit('debug', `[${requestId}] Chat request with ${messages.length} messages`);

      const model = options.model || this.agenticModel;

      // Convert OpenAI message format to Ollama format
      const prompt = this.formatPrompt(messages);

      const response = await this._retryWithBackoff(async () => {
        return await this.client.post('/api/generate', {
          model,
          prompt,
          stream: false,
          options: {
            temperature: options.temperature ?? 0.7,
            top_p: options.topP ?? 1,
            num_predict: options.maxTokens ?? 4096
          }
        });
      });

      const duration = Date.now() - startTime;
      const content = response.data.response;

      this.emit('info', `[${requestId}] Chat completed in ${duration}ms (${content.length} chars, local Ollama)`);
      this.emit('metric', { type: 'chat_latency', value: duration, model, provider: 'ollama' });

      return content;
    } catch (error) {
      this.emit('error', `[${requestId}] Chat failed: ${error.message}`);
      throw this.enrichError(error);
    }
  }

  /**
   * Convert OpenAI messages to Ollama prompt
   */
  formatPrompt(messages) {
    return messages.map(m => {
      if (m.role === 'system') return `<|system|>\n${m.content}\n`;
      if (m.role === 'user') return `<|user|>\n${m.content}\n`;
      if (m.role === 'assistant') return `<|assistant|>\n${m.content}\n`;
      return m.content;
    }).join('\n') + '<|assistant|>\n';
  }

  /**
   * Planning step (agentic mode)
   */
  async plan(goal, context = []) {
    const messages = [
      {
        role: 'system',
        content: `You are an expert AI agent planner. Analyze the goal and context, then produce a detailed step-by-step plan in JSON format.

Response format:
{
  "goal": "restated goal",
  "steps": [
    {"step": 1, "action": "description", "reasoning": "why"},
    ...
  ],
  "resources_needed": ["list"],
  "estimated_tokens": number,
  "confidence": 0.0-1.0
}`
      },
      {
        role: 'user',
        content: `Goal: ${goal}\n\nContext:\n${context.join('\n---\n')}`
      }
    ];

    const response = await this.chat(messages, { model: this.agenticModel, maxTokens: 2048 });

    try {
      return JSON.parse(response);
    } catch {
      this.emit('warn', 'Failed to parse plan JSON, returning raw text');
      return { goal, rawPlan: response };
    }
  }

  /**
   * Code generation step
   */
  async generateCode(spec, language = 'typescript', examples = []) {
    const messages = [
      {
        role: 'system',
        content: `You are an expert ${language} developer. Generate production-ready, well-structured code.

Requirements:
- Include comprehensive error handling
- Add structured logging at key points
- Follow best practices for ${language}
- Return ONLY code, no explanations
${examples.length > 0 ? `\nExamples:\n${examples.map(e => `\`\`\`${language}\n${e}\n\`\`\``).join('\n')}` : ''}`
      },
      {
        role: 'user',
        content: `Generate ${language} code for:\n${spec}`
      }
    ];

    return this.chat(messages, { model: this.agenticModel, maxTokens: 4096 });
  }

  /**
   * Reflection/verification step
   */
  async reflect(work, criteria) {
    const messages = [
      {
        role: 'system',
        content: `You are an expert code reviewer. Evaluate the work against criteria.

Response format:
{
  "passed": true/false,
  "score": 0.0-1.0,
  "issues": ["issue1", "issue2"],
  "improvements": ["suggestion1", "suggestion2"],
  "reasoning": "detailed explanation"
}`
      },
      {
        role: 'user',
        content: `Evaluate:\n${work}\n\nCriteria:\n${criteria}`
      }
    ];

    const response = await this.chat(messages, { model: this.agenticModel, maxTokens: 2048 });

    try {
      return JSON.parse(response);
    } catch {
      this.emit('warn', 'Failed to parse reflection JSON');
      return { rawReflection: response, score: 0.5 };
    }
  }

  /**
   * Get available Ollama models (no rate limits!)
   */
  async getAvailableModels() {
    try {
      this.emit('debug', 'Fetching available models from Ollama');
      const response = await this._retryWithBackoff(async () => {
        return await this.client.get('/api/tags');
      });

      const models = response.data.models.map(m => m.name);
      this.emit('info', `Retrieved ${models.length} local Ollama models`);
      return models;
    } catch (error) {
      this.emit('error', `Failed to fetch models: ${error.message}`);
      return [this.agenticModel, this.standardModel];
    }
  }

  generateRequestId() {
    return `ollama_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  enrichError(error) {
    const enriched = new Error(error.message);
    enriched.status = error.response?.status;
    enriched.retryable = this._isRetryableError(error); // Use the same retryable logic
    enriched.originalError = error;
    return enriched;
  }
}

module.exports = OllamaClient;
