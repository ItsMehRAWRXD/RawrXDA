// content.js (Enhanced for streaming with UI)
// This script runs in the context of every web page.

// Global variables for UI management
let aiExplanationOverlay = null;
let streamingTextElement = null;
let isStreaming = false;
let currentStream = '';

// Initialize the content script
(function() {
    'use strict';
    
    // Create floating AI assistant button
    createFloatingAIButton();
    
    // Listen for messages from the background script
    browser.runtime.onMessage.addListener(handleMessage);
    
    // Add keyboard shortcut (Ctrl+Shift+A)
    document.addEventListener('keydown', handleKeyboardShortcut);
})();

function createFloatingAIButton() {
    // Remove existing button if it exists
    const existingButton = document.getElementById('ohgees-ai-button');
    if (existingButton) {
        existingButton.remove();
    }
    
    // Create floating AI button
    const aiButton = document.createElement('div');
    aiButton.id = 'ohgees-ai-button';
    aiButton.innerHTML = '';
    aiButton.title = 'OhGees AI Assistant (Ctrl+Shift+A)';
    
    // Style the button
    Object.assign(aiButton.style, {
        position: 'fixed',
        top: '20px',
        right: '20px',
        width: '50px',
        height: '50px',
        backgroundColor: '#007acc',
        color: 'white',
        borderRadius: '50%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        fontSize: '24px',
        cursor: 'pointer',
        zIndex: '10000',
        boxShadow: '0 4px 12px rgba(0, 0, 0, 0.3)',
        transition: 'all 0.3s ease',
        border: 'none',
        userSelect: 'none'
    });
    
    // Add hover effects
    aiButton.addEventListener('mouseenter', () => {
        aiButton.style.transform = 'scale(1.1)';
        aiButton.style.backgroundColor = '#005a9e';
    });
    
    aiButton.addEventListener('mouseleave', () => {
        aiButton.style.transform = 'scale(1)';
        aiButton.style.backgroundColor = '#007acc';
    });
    
    // Add click handler
    aiButton.addEventListener('click', () => {
        const selectedText = getSelectedText();
        if (selectedText) {
            requestAIExplanation(selectedText);
        } else {
            showAIInputDialog();
        }
    });
    
    document.body.appendChild(aiButton);
}

function handleMessage(request, sender, sendResponse) {
    switch (request.action) {
        case 'showAiExplanation':
            showAIExplanation(request.explanation);
            break;
            
        case 'aiResponseChunk':
            handleStreamingResponse(request);
            break;
            
        case 'aiResponseComplete':
            handleStreamingComplete(request);
            break;
            
        case 'aiResponseError':
            handleAIError(request.error);
            break;
            
        case 'toggleAIButton':
            toggleAIButton(request.visible);
            break;
    }
}

function handleStreamingResponse(request) {
    if (request.done) {
        handleStreamingComplete(request);
        return;
    }
    
    if (request.chunk) {
        if (!isStreaming) {
            startStreamingUI();
        }
        
        currentStream += request.chunk;
        updateStreamingText(currentStream);
    }
}

function handleStreamingComplete(request) {
    isStreaming = false;
    
    if (streamingTextElement) {
        // Add completion indicator
        streamingTextElement.innerHTML += '<div style="color: #4CAF50; font-size: 12px; margin-top: 10px;"> Response complete</div>';
        
        // Auto-hide after 5 seconds
        setTimeout(() => {
            hideAIExplanation();
        }, 5000);
    }
}

function handleAIError(error) {
    isStreaming = false;
    showAIExplanation(` Error: ${error}`, true);
}

function startStreamingUI() {
    isStreaming = true;
    currentStream = '';
    
    // Create or update the overlay
    if (!aiExplanationOverlay) {
        createAIExplanationOverlay();
    }
    
    // Show streaming indicator
    if (streamingTextElement) {
        streamingTextElement.innerHTML = '<div style="color: #007acc;"> AI is thinking...</div>';
    }
}

function updateStreamingText(text) {
    if (streamingTextElement) {
        // Format the text with basic markdown support
        const formattedText = formatMarkdown(text);
        streamingTextElement.innerHTML = formattedText;
        
        // Auto-scroll to bottom
        streamingTextElement.scrollTop = streamingTextElement.scrollHeight;
    }
}

function formatMarkdown(text) {
    // Basic markdown formatting
    return text
        .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
        .replace(/\*(.*?)\*/g, '<em>$1</em>')
        .replace(/`(.*?)`/g, '<code style="background: #f4f4f4; padding: 2px 4px; border-radius: 3px;">$1</code>')
        .replace(/\n/g, '<br>')
        .replace(/^### (.*$)/gm, '<h3 style="color: #007acc; margin: 10px 0 5px 0;">$1</h3>')
        .replace(/^## (.*$)/gm, '<h2 style="color: #007acc; margin: 15px 0 10px 0;">$1</h2>')
        .replace(/^# (.*$)/gm, '<h1 style="color: #007acc; margin: 20px 0 15px 0;">$1</h1>');
}

function createAIExplanationOverlay() {
    // Remove existing overlay
    if (aiExplanationOverlay) {
        aiExplanationOverlay.remove();
    }
    
    // Create overlay
    aiExplanationOverlay = document.createElement('div');
    aiExplanationOverlay.id = 'ohgees-ai-overlay';
    
    // Style the overlay
    Object.assign(aiExplanationOverlay.style, {
        position: 'fixed',
        top: '0',
        left: '0',
        width: '100%',
        height: '100%',
        backgroundColor: 'rgba(0, 0, 0, 0.5)',
        zIndex: '9999',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '20px',
        boxSizing: 'border-box'
    });
    
    // Create modal content
    const modal = document.createElement('div');
    Object.assign(modal.style, {
        backgroundColor: 'white',
        borderRadius: '12px',
        padding: '0',
        maxWidth: '600px',
        width: '100%',
        maxHeight: '80vh',
        overflow: 'hidden',
        boxShadow: '0 20px 40px rgba(0, 0, 0, 0.3)',
        display: 'flex',
        flexDirection: 'column'
    });
    
    // Create header
    const header = document.createElement('div');
    header.style.cssText = `
        background: linear-gradient(135deg, #007acc, #005a9e);
        color: white;
        padding: 15px 20px;
        display: flex;
        justify-content: space-between;
        align-items: center;
        font-weight: bold;
        font-size: 16px;
    `;
    header.innerHTML = `
        <span> OhGees AI Assistant</span>
        <button id="ohgees-close-btn" style="background: none; border: none; color: white; font-size: 20px; cursor: pointer; padding: 0; width: 30px; height: 30px; border-radius: 50%; display: flex; align-items: center; justify-content: center;">×</button>
    `;
    
    // Create content area
    const content = document.createElement('div');
    content.style.cssText = `
        padding: 20px;
        flex: 1;
        overflow-y: auto;
        line-height: 1.6;
        color: #333;
    `;
    
    streamingTextElement = content;
    
    // Create footer with actions
    const footer = document.createElement('div');
    footer.style.cssText = `
        padding: 15px 20px;
        border-top: 1px solid #eee;
        display: flex;
        gap: 10px;
        justify-content: flex-end;
    `;
    
    const copyButton = document.createElement('button');
    copyButton.textContent = ' Copy';
    copyButton.style.cssText = `
        background: #f0f0f0;
        border: 1px solid #ddd;
        padding: 8px 16px;
        border-radius: 6px;
        cursor: pointer;
        font-size: 14px;
    `;
    copyButton.addEventListener('click', copyToClipboard);
    
    const newQueryButton = document.createElement('button');
    newQueryButton.textContent = ' New Query';
    newQueryButton.style.cssText = `
        background: #007acc;
        color: white;
        border: none;
        padding: 8px 16px;
        border-radius: 6px;
        cursor: pointer;
        font-size: 14px;
    `;
    newQueryButton.addEventListener('click', () => {
        hideAIExplanation();
        showAIInputDialog();
    });
    
    footer.appendChild(copyButton);
    footer.appendChild(newQueryButton);
    
    // Assemble modal
    modal.appendChild(header);
    modal.appendChild(content);
    modal.appendChild(footer);
    aiExplanationOverlay.appendChild(modal);
    
    // Add event listeners
    const closeBtn = header.querySelector('#ohgees-close-btn');
    closeBtn.addEventListener('click', hideAIExplanation);
    
    aiExplanationOverlay.addEventListener('click', (e) => {
        if (e.target === aiExplanationOverlay) {
            hideAIExplanation();
        }
    });
    
    // Add to page
    document.body.appendChild(aiExplanationOverlay);
    
    // Focus for keyboard navigation
    closeBtn.focus();
}

function showAIExplanation(explanation, isError = false) {
    if (!aiExplanationOverlay) {
        createAIExplanationOverlay();
    }
    
    if (streamingTextElement) {
        const formattedText = isError ? explanation : formatMarkdown(explanation);
        streamingTextElement.innerHTML = formattedText;
    }
    
    // Show the overlay
    aiExplanationOverlay.style.display = 'flex';
}

function hideAIExplanation() {
    if (aiExplanationOverlay) {
        aiExplanationOverlay.style.display = 'none';
    }
    isStreaming = false;
    currentStream = '';
}

function showAIInputDialog() {
    const prompt = prompt('Enter your question or request for the AI assistant:');
    if (prompt && prompt.trim()) {
        requestAIExplanation(prompt.trim());
    }
}

function requestAIExplanation(text) {
    // Send message to background script
    browser.runtime.sendMessage({
        action: 'requestAIExplanation',
        text: text,
        url: window.location.href,
        title: document.title
    });
    
    // Show loading state
    startStreamingUI();
}

function getSelectedText() {
    const selection = window.getSelection();
    return selection.toString().trim();
}

function copyToClipboard() {
    if (streamingTextElement) {
        const text = streamingTextElement.innerText || streamingTextElement.textContent;
        navigator.clipboard.writeText(text).then(() => {
            // Show brief success indicator
            const copyBtn = document.querySelector('#ohgees-ai-overlay button');
            if (copyBtn) {
                const originalText = copyBtn.textContent;
                copyBtn.textContent = ' Copied!';
                setTimeout(() => {
                    copyBtn.textContent = originalText;
                }, 2000);
            }
        }).catch(err => {
            console.error('Failed to copy text: ', err);
        });
    }
}

function handleKeyboardShortcut(event) {
    // Ctrl+Shift+A
    if (event.ctrlKey && event.shiftKey && event.key === 'A') {
        event.preventDefault();
        
        const selectedText = getSelectedText();
        if (selectedText) {
            requestAIExplanation(selectedText);
        } else {
            showAIInputDialog();
        }
    }
    
    // Escape to close overlay
    if (event.key === 'Escape' && aiExplanationOverlay && aiExplanationOverlay.style.display === 'flex') {
        hideAIExplanation();
    }
}

function toggleAIButton(visible) {
    const button = document.getElementById('ohgees-ai-button');
    if (button) {
        button.style.display = visible ? 'flex' : 'none';
    }
}

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (aiExplanationOverlay) {
        aiExplanationOverlay.remove();
    }
    const button = document.getElementById('ohgees-ai-button');
    if (button) {
        button.remove();
    }
});

// Export functions for testing (if needed)
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        createFloatingAIButton,
        showAIExplanation,
        hideAIExplanation,
        requestAIExplanation,
        getSelectedText
    };
}
