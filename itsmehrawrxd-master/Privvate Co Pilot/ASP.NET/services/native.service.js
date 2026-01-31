/**
 * Native Service for Chrome Extension
 * Manages communication with the native messaging host
 * Provides secure desktop integration capabilities
 */

export class NativeService {
    constructor() {
        this.port = null;
        this.isConnected = false;
        this.messageId = 0;
        this.pendingMessages = new Map();
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 3;
        
        this.connect();
    }

    /**
     * Connects to the native messaging host
     */
    connect() {
        try {
            this.port = chrome.runtime.connectNative('com.my_company.my_application');
            this.isConnected = true;
            this.reconnectAttempts = 0;
            
            this.port.onMessage.addListener((message) => {
                this.handleMessage(message);
            });
            
            this.port.onDisconnect.addListener(() => {
                this.handleDisconnect();
            });
            
            console.log('Connected to native messaging host');
        } catch (error) {
            console.error('Failed to connect to native host:', error);
            this.handleConnectionError();
        }
    }

    /**
     * Handles incoming messages from native host
     * @param {object} message - Message from native host
     */
    handleMessage(message) {
        const { id, response, error } = message;
        
        if (this.pendingMessages.has(id)) {
            const { resolve, reject } = this.pendingMessages.get(id);
            this.pendingMessages.delete(id);
            
            if (error) {
                reject(new Error(error));
            } else {
                resolve(response);
            }
        }
    }

    /**
     * Handles disconnection from native host
     */
    handleDisconnect() {
        this.isConnected = false;
        console.log('Disconnected from native messaging host');
        
        // Reject all pending messages
        for (const [id, { reject }] of this.pendingMessages) {
            reject(new Error('Native host disconnected'));
        }
        this.pendingMessages.clear();
        
        // Attempt to reconnect
        if (this.reconnectAttempts < this.maxReconnectAttempts) {
            this.reconnectAttempts++;
            console.log(`Attempting to reconnect (${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
            setTimeout(() => this.connect(), 1000 * this.reconnectAttempts);
        }
    }

    /**
     * Handles connection errors
     */
    handleConnectionError() {
        this.isConnected = false;
        console.error('Native host connection error');
        
        // Reject all pending messages
        for (const [id, { reject }] of this.pendingMessages) {
            reject(new Error('Native host connection failed'));
        }
        this.pendingMessages.clear();
    }

    /**
     * Sends a message to the native host
     * @param {string} action - Action to perform
     * @param {object} payload - Message payload
     * @returns {Promise<object>} Response from native host
     */
    async sendMessage(action, payload = {}) {
        if (!this.isConnected) {
            throw new Error('Not connected to native host');
        }

        return new Promise((resolve, reject) => {
            const id = ++this.messageId;
            const message = {
                id,
                action,
                payload
            };

            this.pendingMessages.set(id, { resolve, reject });
            
            try {
                this.port.postMessage(message);
                
                // Set timeout for message
                setTimeout(() => {
                    if (this.pendingMessages.has(id)) {
                        this.pendingMessages.delete(id);
                        reject(new Error('Message timeout'));
                    }
                }, 30000); // 30 second timeout
                
            } catch (error) {
                this.pendingMessages.delete(id);
                reject(error);
            }
        });
    }

    /**
     * Saves a file securely to the desktop
     * @param {string} filename - Name of the file
     * @param {string} content - File content
     * @returns {Promise<object>} Save result
     */
    async saveFile(filename, content) {
        return await this.sendMessage('saveFile', {
            filename,
            content
        });
    }

    /**
     * Opens a file from the desktop
     * @param {string} filename - Name of the file
     * @returns {Promise<object>} File content
     */
    async openFile(filename) {
        return await this.sendMessage('openFile', {
            filename
        });
    }

    /**
     * Gets system information
     * @returns {Promise<object>} System information
     */
    async getSystemInfo() {
        return await this.sendMessage('getSystemInfo', {});
    }

    /**
     * Lists running processes
     * @returns {Promise<object>} Process list
     */
    async listProcesses() {
        return await this.sendMessage('listProcesses', {});
    }

    /**
     * Executes a system command (whitelisted only)
     * @param {string} command - Command to execute
     * @returns {Promise<object>} Command result
     */
    async executeCommand(command) {
        return await this.sendMessage('executeCommand', {
            command
        });
    }

    /**
     * Gets list of files in safe directory
     * @returns {Promise<object>} File list
     */
    async getFileList() {
        return await this.sendMessage('getFileList', {});
    }

    /**
     * Deletes a file from safe directory
     * @param {string} filename - Name of the file
     * @returns {Promise<object>} Delete result
     */
    async deleteFile(filename) {
        return await this.sendMessage('deleteFile', {
            filename
        });
    }

    /**
     * Sends a hello message to test connection
     * @returns {Promise<object>} Hello response
     */
    async sayHello() {
        return await this.sendMessage('sayHello', {});
    }

    /**
     * Checks if the service is connected
     * @returns {boolean} Connection status
     */
    isServiceConnected() {
        return this.isConnected;
    }

    /**
     * Gets connection status information
     * @returns {object} Connection status
     */
    getConnectionStatus() {
        return {
            isConnected: this.isConnected,
            reconnectAttempts: this.reconnectAttempts,
            pendingMessages: this.pendingMessages.size
        };
    }

    /**
     * Manually reconnects to the native host
     */
    reconnect() {
        if (this.port) {
            this.port.disconnect();
        }
        this.reconnectAttempts = 0;
        this.connect();
    }

    /**
     * Disconnects from the native host
     */
    disconnect() {
        if (this.port) {
            this.port.disconnect();
        }
        this.isConnected = false;
        this.pendingMessages.clear();
    }
}
