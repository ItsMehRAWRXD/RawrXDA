/**
 * Background Script Orchestrator for Chrome Extension
 * Orchestrates services rather than handling raw operations
 * Provides elegant service coordination and message routing
 */

import { ApiService } from './services/api.service.js';
import { NativeService } from './services/native.service.js';
import { StateService } from './services/state.service.js';

class ExtensionOrchestrator {
    constructor() {
        this.apiService = new ApiService('https://your-api-server.com');
        this.nativeService = new NativeService();
        this.stateService = new StateService();
        
        this.isInitialized = false;
        this.initialize();
    }

    /**
     * Initializes the extension orchestrator
     */
    async initialize() {
        try {
            console.log('Initializing Extension Orchestrator');
            
            // Check authentication status
            const isAuthenticated = await this.apiService.isAuthenticated();
            await this.stateService.updateConnectionStatus('api', isAuthenticated);
            
            // Check native service connection
            const nativeConnected = this.nativeService.isServiceConnected();
            await this.stateService.updateConnectionStatus('native', nativeConnected);
            
            // Set up event listeners
            this.setupEventListeners();
            
            this.isInitialized = true;
            console.log('Extension Orchestrator initialized successfully');
            
        } catch (error) {
            console.error('Failed to initialize Extension Orchestrator:', error);
        }
    }

    /**
     * Sets up event listeners for the extension
     */
    setupEventListeners() {
        // Handle messages from popup and content scripts
        chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
            this.handleMessage(request, sender, sendResponse);
            return true; // Indicate async response
        });

        // Handle extension installation
        chrome.runtime.onInstalled.addListener(() => {
            this.handleInstallation();
        });

        // Handle context menu creation
        chrome.contextMenus.onClicked.addListener((info, tab) => {
            this.handleContextMenuClick(info, tab);
        });
    }

    /**
     * Handles incoming messages
     * @param {object} request - Message request
     * @param {object} sender - Message sender
     * @param {function} sendResponse - Response callback
     */
    async handleMessage(request, sender, sendResponse) {
        try {
            console.log('Handling message:', request.action);
            
            switch (request.action) {
                case 'aiChat':
                    await this.handleAiChat(request, sendResponse);
                    break;
                    
                case 'saveNative':
                    await this.handleSaveNative(request, sendResponse);
                    break;
                    
                case 'openNative':
                    await this.handleOpenNative(request, sendResponse);
                    break;
                    
                case 'authenticate':
                    await this.handleAuthenticate(request, sendResponse);
                    break;
                    
                case 'getConnectionStatus':
                    await this.handleGetConnectionStatus(request, sendResponse);
                    break;
                    
                default:
                    sendResponse({ error: 'Unknown action' });
            }
            
        } catch (error) {
            console.error('Error handling message:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles AI chat requests
     * @param {object} request - Chat request
     * @param {function} sendResponse - Response callback
     */
    async handleAiChat(request, sendResponse) {
        try {
            const response = await this.apiService.sendChatMessage(
                request.prompt,
                request.context,
                request.serviceType
            );
            
            // Save message to conversation history
            await this.stateService.addMessage({
                role: 'user',
                content: request.prompt,
                timestamp: new Date().toISOString()
            });
            
            await this.stateService.addMessage({
                role: 'assistant',
                content: response.message,
                timestamp: new Date().toISOString()
            });
            
            sendResponse({ success: true, response: response.message });
            
        } catch (error) {
            console.error('AI Chat Error:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles native file save requests
     * @param {object} request - Save request
     * @param {function} sendResponse - Response callback
     */
    async handleSaveNative(request, sendResponse) {
        try {
            const response = await this.nativeService.saveFile(
                request.filename,
                request.content
            );
            
            // Update file list in state
            await this.stateService.addFile({
                name: request.filename,
                size: request.content.length,
                timestamp: new Date().toISOString()
            });
            
            sendResponse({ success: true, response });
            
        } catch (error) {
            console.error('Native Save Error:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles native file open requests
     * @param {object} request - Open request
     * @param {function} sendResponse - Response callback
     */
    async handleOpenNative(request, sendResponse) {
        try {
            const response = await this.nativeService.openFile(request.filename);
            sendResponse({ success: true, response });
            
        } catch (error) {
            console.error('Native Open Error:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles authentication requests
     * @param {object} request - Authentication request
     * @param {function} sendResponse - Response callback
     */
    async handleAuthenticate(request, sendResponse) {
        try {
            const response = await this.apiService.authenticate(
                request.username,
                request.password
            );
            
            if (response.success) {
                await this.stateService.setUser({
                    username: request.username,
                    authenticated: true,
                    timestamp: new Date().toISOString()
                });
                
                await this.stateService.updateConnectionStatus('api', true);
            }
            
            sendResponse(response);
            
        } catch (error) {
            console.error('Authentication Error:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles connection status requests
     * @param {object} request - Connection status request
     * @param {function} sendResponse - Response callback
     */
    async handleGetConnectionStatus(request, sendResponse) {
        try {
            const status = this.stateService.getConnectionStatus();
            sendResponse({ success: true, status });
            
        } catch (error) {
            console.error('Get Connection Status Error:', error);
            sendResponse({ error: error.message });
        }
    }

    /**
     * Handles extension installation
     */
    handleInstallation() {
        console.log('Extension installed');
        
        // Create context menu items
        chrome.contextMenus.create({
            id: 'explainWithAi',
            title: ' Explain with AI',
            contexts: ['selection']
        });
        
        chrome.contextMenus.create({
            id: 'generateCode',
            title: ' Generate Code',
            contexts: ['selection']
        });
        
        chrome.contextMenus.create({
            id: 'saveToDesktop',
            title: ' Save to Desktop',
            contexts: ['selection']
        });
    }

    /**
     * Handles context menu clicks
     * @param {object} info - Context menu info
     * @param {object} tab - Active tab
     */
    async handleContextMenuClick(info, tab) {
        try {
            switch (info.menuItemId) {
                case 'explainWithAi':
                    await this.handleExplainWithAi(info, tab);
                    break;
                    
                case 'generateCode':
                    await this.handleGenerateCode(info, tab);
                    break;
                    
                case 'saveToDesktop':
                    await this.handleSaveToDesktop(info, tab);
                    break;
            }
            
        } catch (error) {
            console.error('Context Menu Error:', error);
        }
    }

    /**
     * Handles explain with AI context menu action
     * @param {object} info - Context menu info
     * @param {object} tab - Active tab
     */
    async handleExplainWithAi(info, tab) {
        try {
            const response = await this.apiService.sendChatMessage(
                `Explain this: "${info.selectionText}"`
            );
            
            // Send response to content script
            chrome.tabs.sendMessage(tab.id, {
                action: 'showAiExplanation',
                explanation: response.message
            });
            
        } catch (error) {
            console.error('Explain with AI Error:', error);
        }
    }

    /**
     * Handles generate code context menu action
     * @param {object} info - Context menu info
     * @param {object} tab - Active tab
     */
    async handleGenerateCode(info, tab) {
        try {
            const response = await this.apiService.sendChatMessage(
                `Generate code for: "${info.selectionText}"`
            );
            
            // Send response to content script
            chrome.tabs.sendMessage(tab.id, {
                action: 'showGeneratedCode',
                code: response.message
            });
            
        } catch (error) {
            console.error('Generate Code Error:', error);
        }
    }

    /**
     * Handles save to desktop context menu action
     * @param {object} info - Context menu info
     * @param {object} tab - Active tab
     */
    async handleSaveToDesktop(info, tab) {
        try {
            const filename = `selection_${Date.now()}.txt`;
            const response = await this.nativeService.saveFile(filename, info.selectionText);
            
            // Send response to content script
            chrome.tabs.sendMessage(tab.id, {
                action: 'showSaveResult',
                result: response
            });
            
        } catch (error) {
            console.error('Save to Desktop Error:', error);
        }
    }
}

// Initialize the extension orchestrator
const orchestrator = new ExtensionOrchestrator();
