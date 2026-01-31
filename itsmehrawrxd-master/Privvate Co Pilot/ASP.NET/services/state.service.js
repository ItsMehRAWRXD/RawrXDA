/**
 * State Service for Chrome Extension
 * Manages application state and data persistence
 * Provides centralized state management for the extension
 */

export class StateService {
    constructor() {
        this.state = {
            user: null,
            settings: {},
            conversationHistory: [],
            files: [],
            systemInfo: null,
            connectionStatus: {
                api: false,
                native: false
            }
        };
        
        this.listeners = new Map();
        this.loadState();
    }

    /**
     * Loads state from storage
     */
    async loadState() {
        try {
            const result = await chrome.storage.local.get([
                'user',
                'settings',
                'conversationHistory',
                'files',
                'systemInfo'
            ]);
            
            this.state = {
                ...this.state,
                ...result
            };
            
            this.notifyListeners('stateLoaded', this.state);
        } catch (error) {
            console.error('Failed to load state:', error);
        }
    }

    /**
     * Saves state to storage
     */
    async saveState() {
        try {
            await chrome.storage.local.set({
                user: this.state.user,
                settings: this.state.settings,
                conversationHistory: this.state.conversationHistory,
                files: this.state.files,
                systemInfo: this.state.systemInfo
            });
            
            this.notifyListeners('stateSaved', this.state);
        } catch (error) {
            console.error('Failed to save state:', error);
        }
    }

    /**
     * Gets the current state
     * @returns {object} Current state
     */
    getState() {
        return { ...this.state };
    }

    /**
     * Updates a specific part of the state
     * @param {string} key - State key to update
     * @param {any} value - New value
     */
    async updateState(key, value) {
        this.state[key] = value;
        await this.saveState();
        this.notifyListeners('stateUpdated', { key, value, state: this.state });
    }

    /**
     * Sets user information
     * @param {object} user - User data
     */
    async setUser(user) {
        await this.updateState('user', user);
    }

    /**
     * Gets user information
     * @returns {object|null} User data
     */
    getUser() {
        return this.state.user;
    }

    /**
     * Clears user information
     */
    async clearUser() {
        await this.updateState('user', null);
    }

    /**
     * Updates settings
     * @param {object} settings - Settings to update
     */
    async updateSettings(settings) {
        const newSettings = { ...this.state.settings, ...settings };
        await this.updateState('settings', newSettings);
    }

    /**
     * Gets a specific setting
     * @param {string} key - Setting key
     * @param {any} defaultValue - Default value if setting not found
     * @returns {any} Setting value
     */
    getSetting(key, defaultValue = null) {
        return this.state.settings[key] ?? defaultValue;
    }

    /**
     * Sets a specific setting
     * @param {string} key - Setting key
     * @param {any} value - Setting value
     */
    async setSetting(key, value) {
        await this.updateSettings({ [key]: value });
    }

    /**
     * Adds a message to conversation history
     * @param {object} message - Message to add
     */
    async addMessage(message) {
        const history = [...this.state.conversationHistory, message];
        await this.updateState('conversationHistory', history);
    }

    /**
     * Gets conversation history
     * @param {number} limit - Maximum number of messages to return
     * @returns {Array} Conversation history
     */
    getConversationHistory(limit = null) {
        const history = this.state.conversationHistory;
        return limit ? history.slice(-limit) : history;
    }

    /**
     * Clears conversation history
     */
    async clearConversationHistory() {
        await this.updateState('conversationHistory', []);
    }

    /**
     * Updates file list
     * @param {Array} files - File list
     */
    async updateFiles(files) {
        await this.updateState('files', files);
    }

    /**
     * Gets file list
     * @returns {Array} File list
     */
    getFiles() {
        return this.state.files;
    }

    /**
     * Adds a file to the list
     * @param {object} file - File to add
     */
    async addFile(file) {
        const files = [...this.state.files, file];
        await this.updateState('files', files);
    }

    /**
     * Removes a file from the list
     * @param {string} filename - Name of file to remove
     */
    async removeFile(filename) {
        const files = this.state.files.filter(file => file.name !== filename);
        await this.updateState('files', files);
    }

    /**
     * Updates system information
     * @param {object} systemInfo - System information
     */
    async updateSystemInfo(systemInfo) {
        await this.updateState('systemInfo', systemInfo);
    }

    /**
     * Gets system information
     * @returns {object|null} System information
     */
    getSystemInfo() {
        return this.state.systemInfo;
    }

    /**
     * Updates connection status
     * @param {string} service - Service name (api, native)
     * @param {boolean} status - Connection status
     */
    async updateConnectionStatus(service, status) {
        const connectionStatus = { ...this.state.connectionStatus };
        connectionStatus[service] = status;
        await this.updateState('connectionStatus', connectionStatus);
    }

    /**
     * Gets connection status
     * @returns {object} Connection status
     */
    getConnectionStatus() {
        return this.state.connectionStatus;
    }

    /**
     * Subscribes to state changes
     * @param {string} event - Event name
     * @param {function} callback - Callback function
     * @returns {function} Unsubscribe function
     */
    subscribe(event, callback) {
        if (!this.listeners.has(event)) {
            this.listeners.set(event, new Set());
        }
        
        this.listeners.get(event).add(callback);
        
        // Return unsubscribe function
        return () => {
            const eventListeners = this.listeners.get(event);
            if (eventListeners) {
                eventListeners.delete(callback);
                if (eventListeners.size === 0) {
                    this.listeners.delete(event);
                }
            }
        };
    }

    /**
     * Notifies listeners of state changes
     * @param {string} event - Event name
     * @param {any} data - Event data
     */
    notifyListeners(event, data) {
        const eventListeners = this.listeners.get(event);
        if (eventListeners) {
            eventListeners.forEach(callback => {
                try {
                    callback(data);
                } catch (error) {
                    console.error('Error in state listener:', error);
                }
            });
        }
    }

    /**
     * Resets state to initial values
     */
    async resetState() {
        this.state = {
            user: null,
            settings: {},
            conversationHistory: [],
            files: [],
            systemInfo: null,
            connectionStatus: {
                api: false,
                native: false
            }
        };
        
        await this.saveState();
        this.notifyListeners('stateReset', this.state);
    }

    /**
     * Exports state as JSON
     * @returns {string} JSON representation of state
     */
    exportState() {
        return JSON.stringify(this.state, null, 2);
    }

    /**
     * Imports state from JSON
     * @param {string} jsonState - JSON representation of state
     */
    async importState(jsonState) {
        try {
            const importedState = JSON.parse(jsonState);
            this.state = { ...this.state, ...importedState };
            await this.saveState();
            this.notifyListeners('stateImported', this.state);
        } catch (error) {
            console.error('Failed to import state:', error);
            throw new Error('Invalid state format');
        }
    }
}
