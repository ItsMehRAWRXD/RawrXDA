// content.js
// This script runs in the context of every web page.

// Enhanced content script with better user experience
class AICoPilotContentScript {
  constructor() {
    this.isInitialized = false;
    this.selectedText = '';
    this.explanationOverlay = null;
    this.init();
  }

  init() {
    if (this.isInitialized) return;
    
    // Listen for messages from the background script
    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
      this.handleMessage(request, sender, sendResponse);
    });

    // Add text selection listener
    document.addEventListener('mouseup', () => {
      this.handleTextSelection();
    });

    // Add keyboard listener for text selection
    document.addEventListener('keyup', () => {
      this.handleTextSelection();
    });

    this.isInitialized = true;
    console.log('AI Co Pilot content script initialized');
  }

  handleMessage(request, sender, sendResponse) {
    switch (request.action) {
      case 'showAiExplanation':
        this.showExplanation(request.explanation);
        sendResponse({ success: true });
        break;
      
      case 'getSelectedText':
        sendResponse({ selectedText: this.selectedText });
        break;
      
      case 'highlightCode':
        this.highlightCode(request.selector, request.highlight);
        sendResponse({ success: true });
        break;
      
      default:
        sendResponse({ error: 'Unknown action' });
    }
  }

  handleTextSelection() {
    const selection = window.getSelection();
    const text = selection.toString().trim();
    
    if (text && text !== this.selectedText) {
      this.selectedText = text;
      
      // Show a subtle indicator that text is selected
      this.showSelectionIndicator(selection);
    }
  }

  showSelectionIndicator(selection) {
    // Remove existing indicator
    const existingIndicator = document.getElementById('ai-copilot-indicator');
    if (existingIndicator) {
      existingIndicator.remove();
    }

    // Only show for code-like content or longer text
    if (this.selectedText.length < 10 || !this.isCodeLike(this.selectedText)) {
      return;
    }

    const range = selection.getRangeAt(0);
    const rect = range.getBoundingClientRect();
    
    const indicator = document.createElement('div');
    indicator.id = 'ai-copilot-indicator';
    indicator.innerHTML = ' Right-click for AI explanation';
    indicator.style.cssText = `
      position: fixed;
      top: ${rect.top - 35}px;
      left: ${rect.left}px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      padding: 8px 12px;
      border-radius: 20px;
      font-size: 12px;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      z-index: 10000;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
      cursor: pointer;
      animation: fadeIn 0.3s ease;
      pointer-events: none;
    `;

    // Add CSS animation
    if (!document.getElementById('ai-copilot-styles')) {
      const style = document.createElement('style');
      style.id = 'ai-copilot-styles';
      style.textContent = `
        @keyframes fadeIn {
          from { opacity: 0; transform: translateY(10px); }
          to { opacity: 1; transform: translateY(0); }
        }
        @keyframes fadeOut {
          from { opacity: 1; transform: translateY(0); }
          to { opacity: 0; transform: translateY(-10px); }
        }
      `;
      document.head.appendChild(style);
    }

    document.body.appendChild(indicator);

    // Auto-remove after 3 seconds
    setTimeout(() => {
      if (indicator && indicator.parentNode) {
        indicator.style.animation = 'fadeOut 0.3s ease';
        setTimeout(() => {
          if (indicator && indicator.parentNode) {
            indicator.remove();
          }
        }, 300);
      }
    }, 3000);
  }

  isCodeLike(text) {
    // Simple heuristics to detect code-like content
    const codePatterns = [
      /[{}();]/,
      /function\s+\w+/,
      /class\s+\w+/,
      /import\s+.*from/,
      /const\s+\w+\s*=/,
      /let\s+\w+\s*=/,
      /var\s+\w+\s*=/,
      /def\s+\w+/,
      /public\s+class/,
      /private\s+\w+/,
      /#include/,
      /<\w+.*>/,
      /\/\*.*\*\//,
      /\/\/.*/
    ];

    return codePatterns.some(pattern => pattern.test(text));
  }

  showExplanation(explanation) {
    // Remove existing overlay
    this.removeExplanationOverlay();

    // Create overlay
    this.explanationOverlay = document.createElement('div');
    this.explanationOverlay.id = 'ai-copilot-explanation';
    this.explanationOverlay.innerHTML = `
      <div class="ai-copilot-modal">
        <div class="ai-copilot-header">
          <h3> AI Explanation</h3>
          <button class="ai-copilot-close">&times;</button>
        </div>
        <div class="ai-copilot-content">
          ${this.formatExplanation(explanation)}
        </div>
        <div class="ai-copilot-footer">
          <button class="ai-copilot-copy">Copy</button>
          <button class="ai-copilot-close-btn">Close</button>
        </div>
      </div>
    `;

    // Add styles
    this.explanationOverlay.style.cssText = `
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0, 0, 0, 0.7);
      z-index: 10001;
      display: flex;
      align-items: center;
      justify-content: center;
      animation: fadeIn 0.3s ease;
    `;

    // Add modal styles
    const modalStyles = `
      .ai-copilot-modal {
        background: white;
        border-radius: 12px;
        max-width: 600px;
        max-height: 80vh;
        width: 90%;
        box-shadow: 0 20px 40px rgba(0,0,0,0.3);
        display: flex;
        flex-direction: column;
        overflow: hidden;
      }
      .ai-copilot-header {
        background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        color: white;
        padding: 20px;
        display: flex;
        justify-content: space-between;
        align-items: center;
      }
      .ai-copilot-header h3 {
        margin: 0;
        font-size: 18px;
        font-weight: 600;
      }
      .ai-copilot-close {
        background: none;
        border: none;
        color: white;
        font-size: 24px;
        cursor: pointer;
        padding: 0;
        width: 30px;
        height: 30px;
        display: flex;
        align-items: center;
        justify-content: center;
        border-radius: 50%;
        transition: background 0.3s ease;
      }
      .ai-copilot-close:hover {
        background: rgba(255, 255, 255, 0.2);
      }
      .ai-copilot-content {
        padding: 20px;
        overflow-y: auto;
        flex: 1;
        line-height: 1.6;
        color: #333;
      }
      .ai-copilot-footer {
        padding: 15px 20px;
        background: #f8f9fa;
        display: flex;
        gap: 10px;
        justify-content: flex-end;
      }
      .ai-copilot-footer button {
        padding: 8px 16px;
        border: none;
        border-radius: 6px;
        cursor: pointer;
        font-size: 14px;
        transition: all 0.3s ease;
      }
      .ai-copilot-copy {
        background: #6c757d;
        color: white;
      }
      .ai-copilot-copy:hover {
        background: #5a6268;
      }
      .ai-copilot-close-btn {
        background: #007bff;
        color: white;
      }
      .ai-copilot-close-btn:hover {
        background: #0056b3;
      }
    `;

    // Add styles to head if not already present
    if (!document.getElementById('ai-copilot-modal-styles')) {
      const style = document.createElement('style');
      style.id = 'ai-copilot-modal-styles';
      style.textContent = modalStyles;
      document.head.appendChild(style);
    }

    document.body.appendChild(this.explanationOverlay);

    // Add event listeners
    this.explanationOverlay.querySelector('.ai-copilot-close').addEventListener('click', () => {
      this.removeExplanationOverlay();
    });

    this.explanationOverlay.querySelector('.ai-copilot-close-btn').addEventListener('click', () => {
      this.removeExplanationOverlay();
    });

    this.explanationOverlay.querySelector('.ai-copilot-copy').addEventListener('click', () => {
      navigator.clipboard.writeText(explanation).then(() => {
        const copyBtn = this.explanationOverlay.querySelector('.ai-copilot-copy');
        const originalText = copyBtn.textContent;
        copyBtn.textContent = 'Copied!';
        setTimeout(() => {
          copyBtn.textContent = originalText;
        }, 2000);
      });
    });

    // Close on overlay click
    this.explanationOverlay.addEventListener('click', (e) => {
      if (e.target === this.explanationOverlay) {
        this.removeExplanationOverlay();
      }
    });

    // Close on Escape key
    const escapeHandler = (e) => {
      if (e.key === 'Escape') {
        this.removeExplanationOverlay();
        document.removeEventListener('keydown', escapeHandler);
      }
    };
    document.addEventListener('keydown', escapeHandler);
  }

  formatExplanation(explanation) {
    // Format the explanation with better styling
    return explanation
      .replace(/\n/g, '<br>')
      .replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')
      .replace(/\*(.*?)\*/g, '<em>$1</em>')
      .replace(/`(.*?)`/g, '<code style="background: #f1f3f4; padding: 2px 4px; border-radius: 3px; font-family: monospace;">$1</code>');
  }

  removeExplanationOverlay() {
    if (this.explanationOverlay && this.explanationOverlay.parentNode) {
      this.explanationOverlay.style.animation = 'fadeOut 0.3s ease';
      setTimeout(() => {
        if (this.explanationOverlay && this.explanationOverlay.parentNode) {
          this.explanationOverlay.remove();
        }
      }, 300);
    }
  }

  highlightCode(selector, highlight) {
    const elements = document.querySelectorAll(selector);
    elements.forEach(element => {
      if (highlight) {
        element.style.backgroundColor = '#fff3cd';
        element.style.border = '2px solid #ffc107';
        element.style.borderRadius = '4px';
      } else {
        element.style.backgroundColor = '';
        element.style.border = '';
        element.style.borderRadius = '';
      }
    });
  }
}

// Initialize the content script
const aiCoPilot = new AICoPilotContentScript();
