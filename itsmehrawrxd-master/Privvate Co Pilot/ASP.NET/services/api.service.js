/**
 * API Service for Chrome Extension
 * Manages all communication with the ASP.NET Core API
 * Provides elegant abstraction for API operations
 */

export class ApiService {
    constructor(baseUrl) {
        this.baseUrl = baseUrl;
        this.defaultHeaders = {
            'Content-Type': 'application/json'
        };
    }

    /**
     * Sets the JWT token for authentication
     * @param {string} token - JWT token
     */
    setAuthToken(token) {
        this.defaultHeaders['Authorization'] = `Bearer ${token}`;
    }

    /**
     * Removes the JWT token
     */
    clearAuthToken() {
        delete this.defaultHeaders['Authorization'];
    }

    /**
     * Makes a POST request to the API
     * @param {string} endpoint - API endpoint
     * @param {object} data - Request data
     * @returns {Promise<object>} Response data
     */
    async post(endpoint, data) {
        try {
            const response = await fetch(`${this.baseUrl}/${endpoint}`, {
                method: 'POST',
                headers: this.defaultHeaders,
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error('API POST Error:', error);
            throw error;
        }
    }

    /**
     * Makes a GET request to the API
     * @param {string} endpoint - API endpoint
     * @param {object} params - Query parameters
     * @returns {Promise<object>} Response data
     */
    async get(endpoint, params = {}) {
        try {
            const url = new URL(`${this.baseUrl}/${endpoint}`);
            Object.keys(params).forEach(key => 
                url.searchParams.append(key, params[key])
            );

            const response = await fetch(url, {
                method: 'GET',
                headers: this.defaultHeaders
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error('API GET Error:', error);
            throw error;
        }
    }

    /**
     * Makes a PUT request to the API
     * @param {string} endpoint - API endpoint
     * @param {object} data - Request data
     * @returns {Promise<object>} Response data
     */
    async put(endpoint, data) {
        try {
            const response = await fetch(`${this.baseUrl}/${endpoint}`, {
                method: 'PUT',
                headers: this.defaultHeaders,
                body: JSON.stringify(data)
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error('API PUT Error:', error);
            throw error;
        }
    }

    /**
     * Makes a DELETE request to the API
     * @param {string} endpoint - API endpoint
     * @returns {Promise<object>} Response data
     */
    async delete(endpoint) {
        try {
            const response = await fetch(`${this.baseUrl}/${endpoint}`, {
                method: 'DELETE',
                headers: this.defaultHeaders
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error('API DELETE Error:', error);
            throw error;
        }
    }

    /**
     * Sends a chat message to the AI service
     * @param {string} prompt - User prompt
     * @param {string} context - Optional context
     * @param {string} serviceType - Type of AI service
     * @returns {Promise<object>} AI response
     */
    async sendChatMessage(prompt, context = '', serviceType = 'Standard') {
        return await this.post('ai/chat', {
            prompt,
            context,
            serviceType
        });
    }

    /**
     * Gets conversation history
     * @param {number} limit - Maximum number of messages
     * @returns {Promise<Array>} Conversation history
     */
    async getConversationHistory(limit = 50) {
        return await this.get('ai/history', { limit });
    }

    /**
     * Clears conversation history
     * @returns {Promise<object>} Success status
     */
    async clearConversationHistory() {
        return await this.delete('ai/history');
    }

    /**
     * Sends a large context chat message
     * @param {string} prompt - User prompt
     * @param {string} filename - Filename for context
     * @param {string} context - Additional context
     * @returns {Promise<object>} AI response
     */
    async sendLargeContextMessage(prompt, filename, context = '') {
        return await this.post('ai/chat/large-context', {
            prompt,
            filename,
            context
        });
    }

    /**
     * Uploads a file to the server
     * @param {File} file - File to upload
     * @returns {Promise<object>} Upload result
     */
    async uploadFile(file) {
        const formData = new FormData();
        formData.append('file', file);

        try {
            const response = await fetch(`${this.baseUrl}/file/upload`, {
                method: 'POST',
                headers: {
                    'Authorization': this.defaultHeaders['Authorization']
                },
                body: formData
            });

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            console.error('File Upload Error:', error);
            throw error;
        }
    }

    /**
     * Generates a code stub
     * @param {string} sourceCode - Source code to compile
     * @param {string} className - Class name
     * @param {string} methodName - Method name
     * @returns {Promise<object>} Generated stub
     */
    async generateStub(sourceCode, className, methodName) {
        return await this.post('stub/generate', {
            sourceCode,
            className,
            methodName
        });
    }

    /**
     * Authenticates with the server
     * @param {string} username - Username
     * @param {string} password - Password
     * @returns {Promise<object>} Authentication result
     */
    async authenticate(username, password) {
        const result = await this.post('auth/login', {
            username,
            password
        });

        if (result.success && result.token) {
            this.setAuthToken(result.token);
            await this.saveToken(result.token);
        }

        return result;
    }

    /**
     * Registers a new user
     * @param {string} username - Username
     * @param {string} password - Password
     * @returns {Promise<object>} Registration result
     */
    async register(username, password) {
        return await this.post('auth/register', {
            username,
            password
        });
    }

    /**
     * Saves JWT token to storage
     * @param {string} token - JWT token
     */
    async saveToken(token) {
        await chrome.storage.local.set({ jwtToken: token });
    }

    /**
     * Loads JWT token from storage
     * @returns {Promise<string|null>} JWT token or null
     */
    async loadToken() {
        const result = await chrome.storage.local.get(['jwtToken']);
        return result.jwtToken || null;
    }

    /**
     * Removes JWT token from storage
     */
    async removeToken() {
        await chrome.storage.local.remove(['jwtToken']);
        this.clearAuthToken();
    }

    /**
     * Checks if user is authenticated
     * @returns {Promise<boolean>} Authentication status
     */
    async isAuthenticated() {
        const token = await this.loadToken();
        if (token) {
            this.setAuthToken(token);
            return true;
        }
        return false;
    }
}
