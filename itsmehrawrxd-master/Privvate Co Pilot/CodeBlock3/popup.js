// popup.js - Enhanced popup interface for OhGees AI Assistant

document.addEventListener('DOMContentLoaded', function() {
    initializePopup();
});

async function initializePopup() {
    // Load current configuration
    await loadConfiguration();
    
    // Set up event listeners
    setupEventListeners();
    
    // Load current API keys
    await loadApiKeys();
}

function setupEventListeners() {
    // Save API key
    document.getElementById('saveKey').addEventListener('click', saveApiKey);
    
    // Test API key
    document.getElementById('testKey').addEventListener('click', testApiKey);
    
    // Clear all keys
    document.getElementById('clearKeys').addEventListener('click', clearAllKeys);
    
    // Open options page
    document.getElementById('openOptions').addEventListener('click', openOptions);
    
    // Toggle AI button
    document.getElementById('toggleButton').addEventListener('click', toggleAIButton);
    
    // Show help
    document.getElementById('help').addEventListener('click', showHelp);
    
    // Provider change
    document.getElementById('provider').addEventListener('change', onProviderChange);
}

async function loadConfiguration() {
    try {
        const result = await chrome.storage.local.get(['apiKeys', 'defaultProvider']);
        
        if (result.defaultProvider) {
            document.getElementById('provider').value = result.defaultProvider;
        }
        
        if (result.apiKeys) {
            // Load existing key for current provider
            const currentProvider = document.getElementById('provider').value;
            if (result.apiKeys[currentProvider]) {
                document.getElementById('apiKey').value = result.apiKeys[currentProvider];
            }
        }
    } catch (error) {
        showStatus('Error loading configuration: ' + error.message, 'error');
    }
}

async function loadApiKeys() {
    try {
        const result = await chrome.storage.local.get(['apiKeys']);
        const keyList = document.getElementById('keyList');
        
        if (!result.apiKeys || Object.keys(result.apiKeys).length === 0) {
            keyList.innerHTML = '<div class="key-item"><span class="key-name">No API keys configured</span><span class="key-status">-</span></div>';
            return;
        }
        
        keyList.innerHTML = '';
        
        for (const [provider, key] of Object.entries(result.apiKeys)) {
            const keyItem = document.createElement('div');
            keyItem.className = 'key-item';
            
            const keyName = document.createElement('span');
            keyName.className = 'key-name';
            keyName.textContent = getProviderDisplayName(provider);
            
            const keyStatus = document.createElement('span');
            keyStatus.className = 'key-status';
            keyStatus.textContent = key ? 'Valid' : 'Invalid';
            keyStatus.classList.add(key ? 'valid' : 'invalid');
            
            keyItem.appendChild(keyName);
            keyItem.appendChild(keyStatus);
            keyList.appendChild(keyItem);
        }
    } catch (error) {
        showStatus('Error loading API keys: ' + error.message, 'error');
    }
}

async function saveApiKey() {
    const provider = document.getElementById('provider').value;
    const apiKey = document.getElementById('apiKey').value.trim();
    
    if (!apiKey) {
        showStatus('Please enter an API key', 'error');
        return;
    }
    
    try {
        // Validate API key format
        if (!validateApiKey(provider, apiKey)) {
            showStatus('Invalid API key format for ' + getProviderDisplayName(provider), 'error');
            return;
        }
        
        // Save to storage
        const result = await chrome.storage.local.get(['apiKeys']);
        const apiKeys = result.apiKeys || {};
        apiKeys[provider] = apiKey;
        
        await chrome.storage.local.set({ 
            apiKeys: apiKeys,
            defaultProvider: provider
        });
        
        // Send to background script
        await chrome.runtime.sendMessage({
            action: 'setApiKey',
            provider: provider,
            key: apiKey
        });
        
        showStatus('API key saved successfully!', 'success');
        
        // Reload key list
        await loadApiKeys();
        
        // Clear input
        document.getElementById('apiKey').value = '';
        
    } catch (error) {
        showStatus('Error saving API key: ' + error.message, 'error');
    }
}

async function testApiKey() {
    const provider = document.getElementById('provider').value;
    const apiKey = document.getElementById('apiKey').value.trim();
    
    if (!apiKey) {
        showStatus('Please enter an API key to test', 'error');
        return;
    }
    
    const testButton = document.getElementById('testKey');
    const originalText = testButton.textContent;
    
    try {
        testButton.innerHTML = '<span class="spinner"></span> Testing...';
        testButton.classList.add('loading');
        
        // Send test request to background script
        const response = await chrome.runtime.sendMessage({
            action: 'testApiKey',
            provider: provider,
            key: apiKey
        });
        
        if (response.success) {
            showStatus(' API key is valid and working!', 'success');
        } else {
            showStatus(' API key test failed: ' + response.error, 'error');
        }
        
    } catch (error) {
        showStatus(' Test failed: ' + error.message, 'error');
    } finally {
        testButton.textContent = originalText;
        testButton.classList.remove('loading');
    }
}

async function clearAllKeys() {
    if (!confirm('Are you sure you want to clear all API keys?')) {
        return;
    }
    
    try {
        await chrome.storage.local.remove(['apiKeys']);
        await chrome.runtime.sendMessage({ action: 'clearApiKeys' });
        
        showStatus('All API keys cleared', 'success');
        await loadApiKeys();
        
    } catch (error) {
        showStatus('Error clearing API keys: ' + error.message, 'error');
    }
}

function openOptions() {
    chrome.runtime.openOptionsPage();
}

async function toggleAIButton() {
    try {
        const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
        
        if (tab) {
            await chrome.tabs.sendMessage(tab.id, {
                action: 'toggleAIButton'
            });
            
            showStatus('AI button toggled', 'info');
        }
    } catch (error) {
        showStatus('Error toggling AI button: ' + error.message, 'error');
    }
}

function showHelp() {
    const helpText = `
OhGees AI Assistant - Help & Shortcuts

Keyboard Shortcuts:
• Ctrl+Shift+A - Toggle AI Assistant
• Ctrl+Shift+E - Explain selected text
• Escape - Close AI overlay

Features:
• Select text and right-click for AI options
• Click the floating AI button
• Use context menu for quick actions
• Stream AI responses in real-time

Supported Providers:
• OpenAI (GPT-3.5, GPT-4)
• Anthropic (Claude)
• Local (Ollama)

For more help, visit the options page.
    `;
    
    alert(helpText);
}

function onProviderChange() {
    const provider = document.getElementById('provider').value;
    
    // Update placeholder text
    const apiKeyInput = document.getElementById('apiKey');
    apiKeyInput.placeholder = `Enter your ${getProviderDisplayName(provider)} API key`;
    
    // Load existing key for this provider
    loadExistingKey(provider);
}

async function loadExistingKey(provider) {
    try {
        const result = await chrome.storage.local.get(['apiKeys']);
        if (result.apiKeys && result.apiKeys[provider]) {
            document.getElementById('apiKey').value = result.apiKeys[provider];
        } else {
            document.getElementById('apiKey').value = '';
        }
    } catch (error) {
        console.error('Error loading existing key:', error);
    }
}

function validateApiKey(provider, key) {
    const patterns = {
        openai: /^sk-[A-Za-z0-9]{48}$/,
        anthropic: /^sk-ant-[A-Za-z0-9-]{95}$/,
        local: /^.*$/ // Local providers don't need validation
    };
    
    return patterns[provider] ? patterns[provider].test(key) : true;
}

function getProviderDisplayName(provider) {
    const names = {
        openai: 'OpenAI',
        anthropic: 'Anthropic',
        local: 'Local (Ollama)'
    };
    
    return names[provider] || provider;
}

function showStatus(message, type = 'info') {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = `status ${type}`;
    status.classList.remove('hidden');
    
    // Auto-hide after 3 seconds
    setTimeout(() => {
        status.classList.add('hidden');
    }, 3000);
}

// Handle messages from background script
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    switch (request.action) {
        case 'apiKeyTestResult':
            if (request.success) {
                showStatus(' API key test successful!', 'success');
            } else {
                showStatus(' API key test failed: ' + request.error, 'error');
            }
            break;
            
        case 'configurationUpdated':
            loadConfiguration();
            loadApiKeys();
            break;
    }
});

// Handle storage changes
chrome.storage.onChanged.addListener((changes, namespace) => {
    if (namespace === 'local') {
        if (changes.apiKeys) {
            loadApiKeys();
        }
        if (changes.defaultProvider) {
            document.getElementById('provider').value = changes.defaultProvider.newValue;
        }
    }
});