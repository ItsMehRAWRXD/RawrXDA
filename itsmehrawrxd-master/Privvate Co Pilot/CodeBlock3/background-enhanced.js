// background-enhanced.js
// Enhanced background script with streaming AI support

// Configuration
const AI_CONFIG = {
    providers: {
        openai: {
            endpoint: 'https://api.openai.com/v1/chat/completions',
            model: 'gpt-3.5-turbo',
            stream: true
        },
        anthropic: {
            endpoint: 'https://api.anthropic.com/v1/messages',
            model: 'claude-3-sonnet-20240229',
            stream: true
        },
        local: {
            endpoint: 'http://localhost:11434/api/generate',
            model: 'llama2',
            stream: true
        }
    },
    defaultProvider: 'openai',
    maxTokens: 2000,
    temperature: 0.7
};

// API Keys storage
let apiKeys = {};

// Initialize extension
chrome.runtime.onInstalled.addListener(() => {
    console.log('OhGees AI Assistant installed');
    
    // Set up context menu
    chrome.contextMenus.create({
        id: 'ai-explain',
        title: ' Explain with AI',
        contexts: ['selection']
    });
    
    chrome.contextMenus.create({
        id: 'ai-improve',
        title: ' Improve with AI',
        contexts: ['selection']
    });
    
    chrome.contextMenus.create({
        id: 'ai-summarize',
        title: ' Summarize with AI',
        contexts: ['selection']
    });
});

// Context menu handler
chrome.contextMenus.onClicked.addListener((info, tab) => {
    const selectedText = info.selectionText;
    if (!selectedText) return;
    
    let prompt = '';
    switch (info.menuItemId) {
        case 'ai-explain':
            prompt = `Please explain the following text in simple terms:\n\n${selectedText}`;
            break;
        case 'ai-improve':
            prompt = `Please improve and rewrite the following text to make it clearer and more professional:\n\n${selectedText}`;
            break;
        case 'ai-summarize':
            prompt = `Please provide a concise summary of the following text:\n\n${selectedText}`;
            break;
    }
    
    if (prompt) {
        sendMessageToTab(tab.id, {
            action: 'requestAIExplanation',
            text: prompt
        });
    }
});

// Message handler
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    switch (request.action) {
        case 'requestAIExplanation':
            handleAIRequest(request, sender.tab);
            break;
            
        case 'setApiKey':
            setApiKey(request.provider, request.key);
            sendResponse({ success: true });
            break;
            
        case 'getApiKeys':
            sendResponse({ keys: apiKeys });
            break;
            
        case 'clearApiKeys':
            apiKeys = {};
            sendResponse({ success: true });
            break;
            
        case 'getConfig':
            sendResponse({ config: AI_CONFIG });
            break;
    }
    
    return true; // Keep message channel open for async responses
});

async function handleAIRequest(request, tab) {
    try {
        const provider = request.provider || AI_CONFIG.defaultProvider;
        const apiKey = apiKeys[provider];
        
        if (!apiKey) {
            sendErrorToTab(tab.id, `No API key found for ${provider}. Please configure your API keys.`);
            return;
        }
        
        // Start streaming response
        await streamAIResponse(request.text, provider, apiKey, tab.id);
        
    } catch (error) {
        console.error('AI request error:', error);
        sendErrorToTab(tab.id, `AI request failed: ${error.message}`);
    }
}

async function streamAIResponse(prompt, provider, apiKey, tabId) {
    const config = AI_CONFIG.providers[provider];
    if (!config) {
        throw new Error(`Unknown provider: ${provider}`);
    }
    
    try {
        const response = await fetch(config.endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${apiKey}`,
                ...(provider === 'anthropic' ? { 'anthropic-version': '2023-06-01' } : {})
            },
            body: JSON.stringify({
                model: config.model,
                messages: [
                    {
                        role: 'system',
                        content: 'You are OhGees AI Assistant, a helpful coding and general assistant. Provide clear, concise, and accurate responses.'
                    },
                    {
                        role: 'user',
                        content: prompt
                    }
                ],
                max_tokens: AI_CONFIG.maxTokens,
                temperature: AI_CONFIG.temperature,
                stream: config.stream
            })
        });
        
        if (!response.ok) {
            throw new Error(`API request failed: ${response.status} ${response.statusText}`);
        }
        
        if (config.stream) {
            await handleStreamingResponse(response, tabId);
        } else {
            const data = await response.json();
            const content = extractContent(data, provider);
            sendMessageToTab(tabId, {
                action: 'showAiExplanation',
                explanation: content
            });
        }
        
    } catch (error) {
        console.error('Streaming error:', error);
        sendErrorToTab(tabId, `Streaming failed: ${error.message}`);
    }
}

async function handleStreamingResponse(response, tabId) {
    const reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = '';
    
    try {
        while (true) {
            const { done, value } = await reader.read();
            
            if (done) {
                // Send completion signal
                sendMessageToTab(tabId, {
                    action: 'aiResponseComplete',
                    done: true
                });
                break;
            }
            
            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split('\n');
            buffer = lines.pop(); // Keep incomplete line in buffer
            
            for (const line of lines) {
                if (line.startsWith('data: ')) {
                    const data = line.slice(6);
                    
                    if (data === '[DONE]') {
                        sendMessageToTab(tabId, {
                            action: 'aiResponseComplete',
                            done: true
                        });
                        return;
                    }
                    
                    try {
                        const parsed = JSON.parse(data);
                        const chunk = extractStreamingContent(parsed);
                        
                        if (chunk) {
                            sendMessageToTab(tabId, {
                                action: 'aiResponseChunk',
                                chunk: chunk,
                                done: false
                            });
                        }
                    } catch (parseError) {
                        console.warn('Failed to parse streaming data:', parseError);
                    }
                }
            }
        }
    } finally {
        reader.releaseLock();
    }
}

function extractContent(data, provider) {
    switch (provider) {
        case 'openai':
            return data.choices?.[0]?.message?.content || 'No response generated';
        case 'anthropic':
            return data.content?.[0]?.text || 'No response generated';
        case 'local':
            return data.response || 'No response generated';
        default:
            return 'Unknown provider response format';
    }
}

function extractStreamingContent(data) {
    // Handle different streaming formats
    if (data.choices?.[0]?.delta?.content) {
        return data.choices[0].delta.content;
    }
    
    if (data.delta?.content) {
        return data.delta.content;
    }
    
    if (data.response) {
        return data.response;
    }
    
    return null;
}

function sendMessageToTab(tabId, message) {
    chrome.tabs.sendMessage(tabId, message).catch(error => {
        console.warn('Failed to send message to tab:', error);
    });
}

function sendErrorToTab(tabId, error) {
    sendMessageToTab(tabId, {
        action: 'aiResponseError',
        error: error
    });
}

function setApiKey(provider, key) {
    apiKeys[provider] = key;
    
    // Store in chrome storage
    chrome.storage.local.set({ apiKeys: apiKeys });
}

// Load API keys from storage
chrome.storage.local.get(['apiKeys'], (result) => {
    if (result.apiKeys) {
        apiKeys = result.apiKeys;
    }
});

// Handle tab updates to inject content script
chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
    if (changeInfo.status === 'complete' && tab.url) {
        // Inject content script if needed
        chrome.scripting.executeScript({
            target: { tabId: tabId },
            files: ['content.js']
        }).catch(error => {
            // Ignore errors for restricted pages
            console.warn('Failed to inject content script:', error);
        });
    }
});

// Handle extension icon click
chrome.action.onClicked.addListener((tab) => {
    sendMessageToTab(tab.id, {
        action: 'toggleAIButton'
    });
});

// Utility functions
function validateApiKey(provider, key) {
    const patterns = {
        openai: /^sk-[A-Za-z0-9]{48}$/,
        anthropic: /^sk-ant-[A-Za-z0-9-]{95}$/,
        local: /^.*$/ // Local providers don't need validation
    };
    
    return patterns[provider] ? patterns[provider].test(key) : true;
}

// Export for testing
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        handleAIRequest,
        streamAIResponse,
        extractContent,
        validateApiKey
    };
}
