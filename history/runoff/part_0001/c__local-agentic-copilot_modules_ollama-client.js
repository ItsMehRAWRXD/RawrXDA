const EventEmitter = require('events');
const axios = require('axios');

class Ollama extends EventEmitter {
    constructor(options = {}) {
        super();
        this.endpoint = options.endpoint || 'http://localhost:11434';
        this.agenticModel = options.agenticModel || 'cheetah-stealth-agentic:latest';
        this.standardModel = options.standardModel || 'bigdaddyg-fast:latest';
        this.isAgenticMode = options.isAgenticMode !== undefined ? options.isAgenticMode : true;
        this.temperature = options.temperature || (this.isAgenticMode ? 0.9 : 0.7);
    }

    /**
     * Generate code from a natural language prompt
     * @param {string} prompt - The code generation prompt
     * @param {string} language - Target programming language
     * @returns {Promise<string>} Generated code
     */
    async generateCode(prompt, language = '') {
        const model = this.isAgenticMode ? this.agenticModel : this.standardModel;
        
        const fullPrompt = language 
            ? `Generate ${language} code for the following:\n\n${prompt}\n\nProvide only the code without explanations.`
            : `Generate code for the following:\n\n${prompt}\n\nProvide only the code without explanations.`;

        this.emit('info', `🚀 Generating code using ${this.isAgenticMode ? 'AGENTIC' : 'STANDARD'} mode...`);
        this.emit('info', `📦 Model: ${model}`);
        this.emit('info', `🌡️  Temperature: ${this.temperature}`);

        try {
            const response = await this._sendRequest(fullPrompt, model);
            this.emit('info', '✅ Code generation complete!');
            return response;
        } catch (error) {
            this.emit('error', `❌ Code generation failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Explain code
     * @param {string} code - Code to explain
     * @returns {Promise<string>} Code explanation
     */
    async explainCode(code) {
        const model = this.isAgenticMode ? this.agenticModel : this.standardModel;
        const prompt = `Explain this code in detail:\n\n${code}`;

        this.emit('info', `📖 Explaining code...`);

        try {
            const response = await this._sendRequest(prompt, model);
            this.emit('info', '✅ Explanation complete!');
            return response;
        } catch (error) {
            this.emit('error', `❌ Explanation failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Fix code issues
     * @param {string} code - Code to fix
     * @returns {Promise<string>} Fixed code
     */
    async fixCode(code) {
        const model = this.isAgenticMode ? this.agenticModel : this.standardModel;
        const prompt = `Fix any issues in this code and return the corrected version:\n\n${code}\n\nProvide only the fixed code without explanations.`;

        this.emit('info', `🔧 Fixing code...`);

        try {
            const response = await this._sendRequest(prompt, model);
            this.emit('info', '✅ Code fixed!');
            return response;
        } catch (error) {
            this.emit('error', `❌ Fix failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Get code completion
     * @param {string} context - Code context before cursor
     * @returns {Promise<string>} Code completion suggestion
     */
    async getCompletion(context) {
        const model = this.isAgenticMode ? this.agenticModel : this.standardModel;
        const prompt = `Complete the following code:\n\n${context}\n\nProvide only the completion without explanations.`;

        this.emit('info', `⚡ Getting completion...`);

        try {
            const response = await this._sendRequest(prompt, model);
            this.emit('info', '✅ Completion ready!');
            return response;
        } catch (error) {
            this.emit('error', `❌ Completion failed: ${error.message}`);
            throw error;
        }
    }

    /**
     * Toggle between agentic and standard mode
     */
    toggleAgenticMode() {
        this.isAgenticMode = !this.isAgenticMode;
        this.temperature = this.isAgenticMode ? 0.9 : 0.7;
        const mode = this.isAgenticMode ? '🚀 AGENTIC' : '⏸️ STANDARD';
        this.emit('info', `Mode switched to: ${mode}`);
        return this.isAgenticMode;
    }

    /**
     * Get current status
     */
    getStatus() {
        return {
            mode: this.isAgenticMode ? 'AGENTIC' : 'STANDARD',
            model: this.isAgenticMode ? this.agenticModel : this.standardModel,
            temperature: this.temperature,
            endpoint: this.endpoint
        };
    }

    /**
     * Check if Ollama is running
     * @returns {Promise<boolean>} True if Ollama is running
     */
    async checkHealth() {
        try {
            const response = await axios.get(`${this.endpoint}/api/tags`);
            this.emit('info', '✅ Ollama is running');
            return true;
        } catch (error) {
            this.emit('error', '❌ Ollama is not running');
            return false;
        }
    }

    /**
     * List available models
     * @returns {Promise<Array>} List of available models
     */
    async listModels() {
        try {
            const response = await axios.get(`${this.endpoint}/api/tags`);
            const models = response.data.models || [];
            this.emit('info', `📦 Found ${models.length} models`);
            return models;
        } catch (error) {
            this.emit('error', `❌ Failed to list models: ${error.message}`);
            throw error;
        }
    }

    /**
     * Send request to Ollama API
     * @private
     */
    async _sendRequest(prompt, model) {
        const requestData = {
            model: model,
            prompt: prompt,
            stream: false,
            options: {
                temperature: this.temperature,
                top_p: 0.9,
                top_k: 40
            }
        };

        try {
            const response = await axios.post(
                `${this.endpoint}/api/generate`,
                requestData,
                {
                    timeout: 180000, // 3 minute timeout for model loading
                    headers: {
                        'Content-Type': 'application/json'
                    }
                }
            );

            if (response.data && response.data.response) {
                return response.data.response.trim();
            } else {
                throw new Error('No response received from Ollama');
            }
        } catch (error) {
            if (error.code === 'ECONNREFUSED') {
                throw new Error('Cannot connect to Ollama. Make sure Ollama is running on localhost:11434');
            } else if (error.response) {
                throw new Error(`Ollama API error: ${error.response.status} - ${error.response.statusText}`);
            } else {
                throw new Error(`Request failed: ${error.message}`);
            }
        }
    }
}

module.exports = Ollama;
