/**
 * Configuration examples for OpenAI and Ollama clients with retry logic
 * Production-ready settings for agentic workflows
 */

module.exports = {
  // OpenAI Client Configuration
  openai: {
    // API Configuration
    apiKey: process.env.OPENAI_API_KEY,
    apiBase: process.env.OPENAI_API_BASE || 'https://api.openai.com/v1',
    model: process.env.OPENAI_MODEL || 'gpt-5.2-pro',

    // Retry Configuration (Production Settings)
    maxRetries: 5,                    // Maximum retry attempts
    initialDelay: 1000,               // 1 second initial delay
    maxDelay: 60000,                  // 1 minute maximum delay
    timeout: 60000,                   // 1 minute timeout per request

    // Retryable error patterns
    retryableStatusCodes: [429, 500, 502, 503, 504],
    retryableErrors: ['ECONNRESET', 'ETIMEDOUT', 'ENETUNREACH'],

    // Model settings
    temperature: 0.7,
    maxTokens: 4096
  },

  // Ollama Client Configuration
  ollama: {
    // Endpoint Configuration
    endpoint: process.env.OLLAMA_ENDPOINT || 'http://localhost:11434',
    agenticModel: process.env.OLLAMA_AGENTIC_MODEL || 'cheetah-stealth-agentic:latest',
    standardModel: process.env.OLLAMA_STANDARD_MODEL || 'bigdaddyg-fast:latest',

    // Retry Configuration (Local Model Settings)
    maxRetries: 3,                    // Fewer retries for local models
    initialDelay: 2000,               // 2 seconds initial delay (local can be slower)
    maxDelay: 30000,                  // 30 seconds maximum delay
    timeout: 120000,                  // 2 minutes timeout for large models

    // Retryable error patterns for local connections
    retryableErrors: ['ECONNRESET', 'ETIMEDOUT', 'ENETUNREACH'],

    // Model settings
    temperature: 0.7,
    maxTokens: 4096
  },

  // Agentic Workflow Settings
  agentic: {
    // Planning step configuration
    planningModel: 'gpt-5.2-pro',
    planningMaxTokens: 2048,

    // Code generation configuration
    codeModel: 'gpt-5.2-pro',
    codeMaxTokens: 4096,

    // Reflection/verification configuration
    reflectionModel: 'gpt-5.2-pro',
    reflectionMaxTokens: 2048,

    // Batch processing
    batchConcurrency: 3,
    batchDelay: 100
  }
};

/**
 * Usage Examples:
 * 
 * // OpenAI Client with retry logic
 * const OpenAIClient = require('./modules/openai-client.js');
 * const client = new OpenAIClient(config.openai);
 * 
 * // Ollama Client with retry logic
 * const OllamaClient = require('./modules/ollama-client.js');
 * const client = new OllamaClient(config.ollama);
 * 
 * // Environment Variables:
 * OPENAI_API_KEY=your-api-key
 * OPENAI_MODEL=gpt-5.2-pro
 * OLLAMA_ENDPOINT=http://localhost:11434
 * OLLAMA_AGENTIC_MODEL=cheetah-stealth-agentic:latest
 */