const axios = require('axios');

class OpenAIClient {
    constructor() {
        // Retrieve the API key from the environment. In local development the key may be absent,
        // so we fall back to a dummy placeholder and emit a warning instead of throwing.
        this.apiKey = process.env.OPENAI_API_KEY || 'sk-PLACEHOLDER';
        if (this.apiKey === 'sk-PLACEHOLDER') {
            // eslint-disable-next-line no-console
            console.warn('⚠️  OPENAI_API_KEY not set – using placeholder. OpenAI calls will fail unless a valid key is provided.');
        }
        this.endpoint = 'https://api.openai.com/v1';
    }

    /**
     * List available models from OpenAI
     * @returns {Promise<Array<string>>}
     */
    async getAvailableModels() {
        try {
            const response = await axios.get(`${this.endpoint}/models`, {
                headers: {
                    Authorization: `Bearer ${this.apiKey}`,
                    'Content-Type': 'application/json'
                }
            });
            const models = response.data.data || [];
            return models.map(m => m.id);
        } catch (err) {
            console.error('Failed to fetch OpenAI models:', err.message);
            throw err;
        }
    }

    // Additional methods (e.g., chat completions) can be added here as needed.
}

module.exports = OpenAIClient;
