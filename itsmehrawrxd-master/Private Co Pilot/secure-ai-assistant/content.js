// Secure AI Desktop Helper - Content Script
// This script is injected into web pages to handle user interactions and file operations

class ContentScript {
    constructor() {
        this.init();
    }

    init() {
        this.setupMessageListeners();
        this.setupKeyboardShortcuts();
        console.log('Content script initialized');
    }

    setupMessageListeners() {
        chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
            this.handleMessage(request, sender, sendResponse);
            return true; // Keep message channel open for async responses
        });
    }

    async handleMessage(request, sender, sendResponse) {
        try {
            switch (request.action) {
                case 'performAction':
                    await this.performAction(request.script, sendResponse);
                    break;
                case 'selectFile':
                    await this.selectFile(sendResponse);
                    break;
                case 'captureContent':
                    await this.capturePageContent(sendResponse);
                    break;
                case 'getPageContent':
                    await this.getPageContent(sendResponse);
                    break;
                case 'displayResponse':
                    this.displayResponse(request.response);
                    sendResponse({ success: true });
                    break;
                default:
                    sendResponse({ success: false, error: 'Unknown action' });
            }
        } catch (error) {
            console.error('Content script error:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async performAction(script, sendResponse) {
        try {
            console.log('AI command to execute:', script);
            
            // Parse the AI response and execute safe actions
            const actions = this.parseAIResponse(script);
            
            for (const action of actions) {
                await this.executeAction(action);
            }
            
            sendResponse({ success: true, actionsExecuted: actions.length });
        } catch (error) {
            console.error('Error performing action:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    parseAIResponse(response) {
        const actions = [];
        const lowerResponse = response.toLowerCase();
        
        // Parse different types of commands
        if (lowerResponse.includes('click') && lowerResponse.includes('button')) {
            actions.push({ type: 'click', target: 'button' });
        }
        
        if (lowerResponse.includes('type') || lowerResponse.includes('enter text')) {
            const textMatch = response.match(/['"]([^'"]+)['"]/);
            if (textMatch) {
                actions.push({ type: 'type', text: textMatch[1] });
            }
        }
        
        if (lowerResponse.includes('scroll')) {
            if (lowerResponse.includes('up')) {
                actions.push({ type: 'scroll', direction: 'up' });
            } else if (lowerResponse.includes('down')) {
                actions.push({ type: 'scroll', direction: 'down' });
            }
        }
        
        if (lowerResponse.includes('fill form') || lowerResponse.includes('fill input')) {
            actions.push({ type: 'fillForm' });
        }
        
        if (lowerResponse.includes('highlight') || lowerResponse.includes('select')) {
            actions.push({ type: 'highlight' });
        }
        
        return actions;
    }

    async executeAction(action) {
        switch (action.type) {
            case 'click':
                await this.clickElement(action.target);
                break;
            case 'type':
                await this.typeText(action.text);
                break;
            case 'scroll':
                this.scrollPage(action.direction);
                break;
            case 'fillForm':
                await this.fillForm();
                break;
            case 'highlight':
                this.highlightText();
                break;
        }
    }

    async clickElement(selector) {
        const element = document.querySelector(selector) || 
                       document.querySelector('button') ||
                       document.querySelector('input[type="submit"]') ||
                       document.querySelector('a');
        
        if (element) {
            element.click();
            this.showNotification('Element clicked');
        } else {
            this.showNotification('No clickable element found');
        }
    }

    async typeText(text) {
        const inputField = document.querySelector('input[type="text"], input[type="email"], input[type="password"], textarea') ||
                          document.activeElement;
        
        if (inputField && (inputField.tagName === 'INPUT' || inputField.tagName === 'TEXTAREA')) {
            inputField.focus();
            inputField.value = text;
            inputField.dispatchEvent(new Event('input', { bubbles: true }));
            inputField.dispatchEvent(new Event('change', { bubbles: true }));
            this.showNotification(`Text entered: "${text}"`);
        } else {
            this.showNotification('No input field found');
        }
    }

    scrollPage(direction) {
        const scrollAmount = 300;
        if (direction === 'up') {
            window.scrollBy(0, -scrollAmount);
        } else {
            window.scrollBy(0, scrollAmount);
        }
        this.showNotification(`Scrolled ${direction}`);
    }

    async fillForm() {
        const inputs = document.querySelectorAll('input[type="text"], input[type="email"], textarea');
        let filledCount = 0;
        
        for (const input of inputs) {
            if (!input.value) {
                const placeholder = input.placeholder || input.name || 'field';
                input.value = `Sample ${placeholder}`;
                input.dispatchEvent(new Event('input', { bubbles: true }));
                filledCount++;
            }
        }
        
        this.showNotification(`Form filled: ${filledCount} fields`);
    }

    highlightText() {
        const selection = window.getSelection();
        if (selection.toString()) {
            this.showNotification('Text already selected');
            return;
        }
        
        // Find and highlight the first paragraph or heading
        const textElement = document.querySelector('p, h1, h2, h3, h4, h5, h6, div');
        if (textElement) {
            const range = document.createRange();
            range.selectNodeContents(textElement);
            selection.removeAllRanges();
            selection.addRange(range);
            this.showNotification('Text highlighted');
        } else {
            this.showNotification('No text to highlight');
        }
    }

    async selectFile(sendResponse) {
        try {
            // Create a file input element
            const fileInput = document.createElement('input');
            fileInput.type = 'file';
            fileInput.multiple = false;
            
            fileInput.addEventListener('change', async (event) => {
                const file = event.target.files[0];
                if (file) {
                    try {
                        const fileData = await this.readFileAsBase64(file);
                        
                        // Send file data to background script
                        chrome.runtime.sendMessage({
                            action: 'sendFile',
                            fileData: fileData,
                            fileName: file.name,
                            fileType: file.type,
                            fileSize: file.size
                        }, (response) => {
                            if (response && response.success) {
                                this.showNotification(`File "${file.name}" sent successfully`);
                            } else {
                                this.showNotification('File send failed');
                            }
                        });
                    } catch (error) {
                        console.error('Error reading file:', error);
                        this.showNotification('Error reading file');
                    }
                }
            });
            
            // Trigger file selection
            fileInput.click();
            sendResponse({ success: true });
        } catch (error) {
            console.error('Error selecting file:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    readFileAsBase64(file) {
        return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = reject;
            reader.readAsDataURL(file);
        });
    }

    async capturePageContent(sendResponse) {
        try {
            const content = this.extractPageContent();
            
            // Send content to background script
            chrome.runtime.sendMessage({
                action: 'captureContent',
                content: content,
                url: window.location.href,
                title: document.title
            }, (response) => {
                if (response && response.success) {
                    this.showNotification('Page content captured');
                } else {
                    this.showNotification('Content capture failed');
                }
            });
            
            sendResponse({ success: true, content: content });
        } catch (error) {
            console.error('Error capturing content:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    async getPageContent(sendResponse) {
        try {
            const content = this.extractPageContent();
            sendResponse({ success: true, content: content });
        } catch (error) {
            console.error('Error getting page content:', error);
            sendResponse({ success: false, error: error.message });
        }
    }

    extractPageContent() {
        // Remove script and style elements
        const elementsToRemove = document.querySelectorAll('script, style, nav, header, footer, aside');
        elementsToRemove.forEach(el => el.remove());
        
        // Extract main content
        const mainContent = document.querySelector('main, article, .content, .main, #content, #main') || document.body;
        
        // Get text content
        let content = mainContent.innerText || mainContent.textContent || '';
        
        // Clean up the content
        content = content
            .replace(/\s+/g, ' ')
            .replace(/\n\s*\n/g, '\n')
            .trim();
        
        // Limit content length
        if (content.length > 10000) {
            content = content.substring(0, 10000) + '...';
        }
        
        return {
            text: content,
            title: document.title,
            url: window.location.href,
            timestamp: new Date().toISOString(),
            wordCount: content.split(' ').length
        };
    }

    displayResponse(response) {
        // Create a temporary notification to show AI response
        this.showNotification(response, 5000);
        
        // Also log to console for debugging
        console.log('AI Response:', response);
    }

    showNotification(message, duration = 3000) {
        // Remove existing notification
        const existing = document.getElementById('ai-assistant-notification');
        if (existing) {
            existing.remove();
        }
        
        // Create notification element
        const notification = document.createElement('div');
        notification.id = 'ai-assistant-notification';
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
            z-index: 10000;
            max-width: 300px;
            font-family: 'Segoe UI', sans-serif;
            font-size: 14px;
            line-height: 1.4;
            word-wrap: break-word;
        `;
        
        notification.textContent = message;
        document.body.appendChild(notification);
        
        // Auto-remove after duration
        setTimeout(() => {
            if (notification.parentNode) {
                notification.remove();
            }
        }, duration);
        
        // Click to dismiss
        notification.addEventListener('click', () => {
            notification.remove();
        });
    }

    setupKeyboardShortcuts() {
        // Add keyboard shortcuts for quick access
        document.addEventListener('keydown', (event) => {
            // Ctrl+Shift+A to open AI assistant
            if (event.ctrlKey && event.shiftKey && event.key === 'A') {
                event.preventDefault();
                chrome.runtime.sendMessage({ action: 'openPopup' });
            }
            
            // Ctrl+Shift+C to capture content
            if (event.ctrlKey && event.shiftKey && event.key === 'C') {
                event.preventDefault();
                this.capturePageContent(() => {});
            }
        });
    }
}

// Initialize the content script
new ContentScript();

// Handle page navigation (for SPAs)
let lastUrl = location.href;
new MutationObserver(() => {
    const url = location.href;
    if (url !== lastUrl) {
        lastUrl = url;
        // Reinitialize on navigation
        setTimeout(() => {
            new ContentScript();
        }, 1000);
    }
}).observe(document, { subtree: true, childList: true });
