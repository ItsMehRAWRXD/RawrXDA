/**
 * BigDaddyG IDE - Renderer Process
 * Monaco Editor integration + AI Copilot features
 */

// ============================================================================
// MONACO EDITOR SETUP
// ============================================================================

let editor;
let openTabs = {};
let activeTab = 'welcome';
let tabCounter = 0; // For generating unique tab IDs
const MAX_TABS = 100; // Configurable limit (can be changed in settings)

// Monaco loader fallback support (local → CDN)
const MONACO_LOCAL_BASE = '../node_modules/monaco-editor/min/vs';
// Use jsDelivr to avoid MIME type mismatches seen with some unpkg mirrors
const MONACO_CDN_BASE = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs';
let monacoBasePath = MONACO_LOCAL_BASE;

function ensureMonacoLoader() {
    // If loader is already available, nothing to do
    if (typeof require !== 'undefined' && typeof require.config !== 'undefined') {
        return;
    }

    // Prevent duplicate injections
    if (document.querySelector('script[data-monaco-loader]')) {
        return;
    }

    const injectLoader = (base, markCdn = false) => {
        // Add CSS for the given base
        if (!document.querySelector('link[data-monaco-style="' + base + '"]')) {
            const css = document.createElement('link');
            css.rel = 'stylesheet';
            css.type = 'text/css';
            css.crossOrigin = 'anonymous';
            css.href = base.replace(/\/vs$/, '/editor/editor.main.css');
            css.setAttribute('data-monaco-style', base);
            document.head.appendChild(css);
        }

        const script = document.createElement('script');
        script.src = `${base}/loader.js`;
        script.crossOrigin = 'anonymous';
        script.setAttribute('data-monaco-loader', 'true');
        if (markCdn) {
            script.setAttribute('data-monaco-cdn', 'true');
        }
        script.onerror = () => {
            if (base === MONACO_LOCAL_BASE) {
                console.warn('[BigDaddyG] ⚠️ Local Monaco loader missing, switching to CDN');
                monacoBasePath = MONACO_CDN_BASE;
                injectLoader(MONACO_CDN_BASE, true);
            } else {
                console.error('[BigDaddyG] ❌ Monaco loader failed to load from CDN');
            }
        };
        document.head.appendChild(script);
    };

    // Try local first; on error we automatically fall back to CDN
    injectLoader(MONACO_LOCAL_BASE, false);
}

// Wait for Monaco loader to be available
let monacoInitAttempts = 0;
const MAX_MONACO_INIT_ATTEMPTS = 5;

function attemptMonacoInit() {
    // Ensure loader is injected (will fallback to CDN if local missing)
    ensureMonacoLoader();

    if (typeof require !== 'undefined' && typeof require.config !== 'undefined') {
        initializeMonacoEditor();
    } else {
        monacoInitAttempts++;
        if (monacoInitAttempts < MAX_MONACO_INIT_ATTEMPTS) {
            console.log(`[BigDaddyG] ⏳ Monaco loader not ready, retrying... (${monacoInitAttempts}/${MAX_MONACO_INIT_ATTEMPTS})`);
            setTimeout(attemptMonacoInit, 200);
        } else {
            console.error('[BigDaddyG] ❌ Monaco loader failed to initialize after multiple attempts');
        }
    }
}

if (typeof require === 'undefined' || typeof require.config === 'undefined') {
    console.log('[BigDaddyG] 🔄 Monaco loader not ready, scheduling initialization...');
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', attemptMonacoInit);
    } else {
        attemptMonacoInit();
    }
} else {
    initializeMonacoEditor();
}

function initializeMonacoEditor() {
    if (typeof require === 'undefined' || typeof require.config === 'undefined') {
        console.error('[BigDaddyG] Monaco loader still not available');
        return;
    }

    // Point Monaco to the resolved base (local or CDN fallback)
    require.config({ paths: { 'vs': monacoBasePath } });

    require(['vs/editor/editor.main'], function () {
        console.log('[BigDaddyG] 🎨 Initializing Monaco Editor...');

        // Create Monaco Editor instance
        editor = monaco.editor.create(document.getElementById('monaco-container'), {
            value: getWelcomeMessage(),
            language: 'markdown',
            theme: 'vs-dark',

            // Editor options
            fontSize: 14,
            fontFamily: 'Consolas, "Courier New", monospace',
            lineNumbers: 'on',
            roundedSelection: true,
            scrollBeyondLastLine: false,
            minimap: {
                enabled: true
            },
            automaticLayout: true,

            // Advanced features
            suggestOnTriggerCharacters: true,
            acceptSuggestionOnEnter: 'on',
            tabCompletion: 'on',
            wordWrap: 'on',
            wrappingIndent: 'indent',

            // Copilot-like features
            quickSuggestions: true,
            quickSuggestionsDelay: 100,
            parameterHints: {
                enabled: true
            },

            // Bracket matching
            matchBrackets: 'always',
            bracketPairColorization: {
                enabled: true
            }
        });

        // Store initial tab
        openTabs['welcome'] = {
            id: 'welcome',
            filename: 'Welcome.md',
            language: 'markdown',
            content: editor.getValue(),
            icon: '📄'
        };

        // Set up context menu for AI copilot
        setupContextMenu();

        // Initialize ultra-fast autocomplete
        if (typeof AutocompleteEngine !== 'undefined') {
            window.autocompleteEngine = new AutocompleteEngine(editor, 'http://localhost:11441');
            console.log('[BigDaddyG] ⚡ Autocomplete engine initialized');
        }

        // Listen for content changes
        editor.onDidChangeModelContent(() => {
            if (openTabs[activeTab]) {
                openTabs[activeTab].content = editor.getValue();
            }
        });

        console.log('[BigDaddyG] ✅ Monaco Editor ready');
    });
}

// ============================================================================
// WELCOME MESSAGE
// ============================================================================

function getWelcomeMessage() {
    return `# 🌌 Welcome to BigDaddyG IDE Professional Edition

## Your AI-Powered Development Environment

**Features:**
- ✅ **Monaco Editor** - Same engine as VS Code
- ✅ **Syntax Highlighting** - 100+ languages
- ✅ **AI Copilot** - Right-click for AI suggestions
- ✅ **Multi-Tab Editing** - Work on multiple files
- ✅ **File System Integration** - Save and load real files
- ✅ **1M Context Window** - AI remembers everything
- ✅ **Trained on ASM/Security** - Specialized expertise

## Quick Start

### 1. Create a new file
Press \`Ctrl+N\` or use File → New File

### 2. Write some code
Try writing a function (JavaScript, Python, C++, etc.)

### 3. Get AI help
Select your code → Right-click → Choose AI action:
- 📖 **Explain** - Understand what the code does
- 🔧 **Fix** - Find and fix bugs
- ⚡ **Optimize** - Improve performance
- 🔄 **Refactor** - Better structure
- 🧪 **Generate Tests** - Create unit tests
- 📝 **Add Docs** - Write documentation

### 4. Accept suggestions
When AI generates code:
- ✅ **Apply** - Replace your selection
- ➕ **Insert** - Add below selection
- ❌ **Reject** - Dismiss suggestion

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| \`Ctrl+N\` | New File |
| \`Ctrl+O\` | Open File |
| \`Ctrl+S\` | Save File |
| \`Ctrl+K\` | Ask BigDaddyG |
| \`Ctrl+E\` | Explain Code |
| \`Ctrl+Shift+F\` | Fix Code |
| \`Ctrl+B\` | Toggle Sidebar |
| \`Ctrl+\`\` | Toggle Terminal |

## Example: Write Assembly Code

\`\`\`asm
; x86 assembly - Add two numbers
section .text
global _start

_start:
    mov eax, 5      ; Load 5 into EAX
    mov ebx, 3      ; Load 3 into EBX
    add eax, ebx    ; EAX = EAX + EBX
    
    ; Exit
    mov eax, 1      ; sys_exit
    xor ebx, ebx    ; return 0
    int 0x80
\`\`\`

**Select the code above and right-click → Explain** to see BigDaddyG in action!

## Chat with BigDaddyG

Use the AI panel on the right to:
- Ask programming questions
- Get code suggestions
- Debug issues
- Learn new concepts

**Try asking:**
- "Write a binary search in C++"
- "Explain how AES encryption works"
- "Create a polymorphic shellcode"
- "Optimize this bubble sort algorithm"

---

**🚀 Start coding and let BigDaddyG help you build amazing things!**
`;
}

// ============================================================================
// TAB MANAGEMENT
// ============================================================================

// Open a file in editor (called from file tree clicks)
function openFile(fileId, language, content = '') {
    // If file already has a tab open, switch to it
    const existingTab = Object.values(openTabs).find(tab =>
        tab.id === fileId || tab.filename === fileId
    );

    if (existingTab) {
        switchTab(existingTab.id);
        return;
    }

    // Create new tab for the file
    const filename = fileId === 'welcome' ? 'Welcome.md' : fileId;
    const tabContent = content || (fileId === 'welcome' ? getWelcomeMessage() : '');
    createNewTab(filename, language, tabContent);
}

function createNewTab(filename, language, content = '') {
    // Check if we've reached the tab limit
    const currentTabCount = Object.keys(openTabs).length;
    if (currentTabCount >= MAX_TABS) {
        const proceed = confirm(
            `⚠️ You have ${currentTabCount} tabs open (limit: ${MAX_TABS}).\n\n` +
            `Opening more tabs may slow down the IDE.\n\n` +
            `Continue anyway? (You can close unused tabs with the × button)`
        );
        if (!proceed) {
            console.log('[BigDaddyG] ⚠️ Tab creation cancelled - limit reached');
            return null;
        }
    }

    const id = `file_${++tabCounter}_${Date.now()}`;
    const icon = getFileIcon(filename);

    openTabs[id] = {
        id: id,
        filename: filename,
        language: language,
        content: content,
        icon: icon,
        created: Date.now(),
        modified: Date.now()
    };

    renderTabs();
    switchTab(id);

    console.log(`[BigDaddyG] 📄 Created tab ${currentTabCount + 1}/${MAX_TABS}: ${filename}`);
    return id;
}

function switchTab(tabId) {
    if (!openTabs[tabId]) return;

    // Save current tab content
    if (openTabs[activeTab]) {
        openTabs[activeTab].content = editor.getValue();
    }

    // Switch to new tab
    activeTab = tabId;
    const tab = openTabs[tabId];

    // Update editor
    const model = monaco.editor.createModel(tab.content, tab.language);
    editor.setModel(model);

    // Update UI
    renderTabs();

    console.log(`[BigDaddyG] 📝 Switched to: ${tab.filename}`);
}

function closeTab(event, tabId) {
    event.stopPropagation();

    if (Object.keys(openTabs).length === 1) {
        console.log('[BigDaddyG] ⚠️ Cannot close last tab');
        return;
    }

    const tab = openTabs[tabId];
    console.log(`[BigDaddyG] 🗑️ Closing tab: ${tab?.filename || tabId}`);

    delete openTabs[tabId];

    // Switch to another tab if this was active
    if (activeTab === tabId) {
        const remainingTabs = Object.keys(openTabs);
        switchTab(remainingTabs[remainingTabs.length - 1]); // Switch to most recent
    }

    renderTabs();
}

// Navigation helpers
function nextTab() {
    const tabs = Object.keys(openTabs);
    const currentIndex = tabs.indexOf(activeTab);
    const nextIndex = (currentIndex + 1) % tabs.length;
    switchTab(tabs[nextIndex]);
}

function previousTab() {
    const tabs = Object.keys(openTabs);
    const currentIndex = tabs.indexOf(activeTab);
    const prevIndex = (currentIndex - 1 + tabs.length) % tabs.length;
    switchTab(tabs[prevIndex]);
}

function closeAllTabs() {
    if (!confirm(`Close all ${Object.keys(openTabs).length} tabs?`)) return;

    const welcomeTab = Object.keys(openTabs)[0];
    Object.keys(openTabs).forEach(id => {
        if (id !== welcomeTab) delete openTabs[id];
    });

    switchTab(welcomeTab);
    renderTabs();
    console.log('[BigDaddyG] 🗑️ Closed all tabs except welcome');
}

function closeOtherTabs() {
    if (!confirm(`Close all tabs except "${openTabs[activeTab]?.filename}"?`)) return;

    const keepTab = activeTab;
    Object.keys(openTabs).forEach(id => {
        if (id !== keepTab) delete openTabs[id];
    });

    renderTabs();
    console.log(`[BigDaddyG] 🗑️ Closed other tabs, kept: ${openTabs[keepTab].filename}`);
}

function renderTabs() {
    const tabBar = document.getElementById('tab-bar');
    tabBar.innerHTML = '';

    // Get sorted tabs (by creation time)
    const sortedTabs = Object.values(openTabs).sort((a, b) =>
        (a.created || 0) - (b.created || 0)
    );

    sortedTabs.forEach(tab => {
        const tabEl = document.createElement('div');
        tabEl.className = 'editor-tab' + (tab.id === activeTab ? ' active' : '');
        tabEl.setAttribute('data-file', tab.id);
        tabEl.onclick = () => switchTab(tab.id);

        // Show full filename on hover
        tabEl.title = `${tab.filename}\nCreated: ${new Date(tab.created).toLocaleString()}`;

        tabEl.innerHTML = `
            <span>${tab.icon}</span>
            <span style="flex: 1; overflow: hidden; text-overflow: ellipsis;">${tab.filename}</span>
            <span class="close-btn" onclick="closeTab(event, '${tab.id}')">×</span>
        `;

        tabBar.appendChild(tabEl);
    });

    // Update tab counter badge
    updateTabCountBadge();

    // Scroll active tab into view
    setTimeout(() => {
        const activeTabEl = tabBar.querySelector('.editor-tab.active');
        if (activeTabEl) {
            activeTabEl.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'center' });
        }
    }, 50);
}

function updateTabCountBadge() {
    // Remove old badge if exists
    let badge = document.querySelector('.tab-count-badge');
    if (badge) badge.remove();

    const tabCount = Object.keys(openTabs).length;

    // Only show badge if we have multiple tabs
    if (tabCount > 1) {
        badge = document.createElement('div');
        badge.className = 'tab-count-badge';
        badge.textContent = `${tabCount} / ${MAX_TABS} tabs`;

        // Color code based on usage
        if (tabCount >= MAX_TABS * 0.9) {
            badge.style.background = 'rgba(255, 71, 87, 0.2)';
            badge.style.borderColor = 'var(--red)';
            badge.style.color = 'var(--red)';
        } else if (tabCount >= MAX_TABS * 0.7) {
            badge.style.background = 'rgba(255, 107, 53, 0.2)';
            badge.style.borderColor = 'var(--orange)';
            badge.style.color = 'var(--orange)';
        }

        document.body.appendChild(badge);
    }
}

function getFileIcon(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const icons = {
        js: '📜', ts: '📘', jsx: '⚛️', tsx: '⚛️',
        py: '🐍', java: '☕', cpp: '⚙️', c: '🔧',
        rs: '🦀', go: '🐹', rb: '💎', php: '🐘',
        html: '🌐', css: '🎨', json: '📋', xml: '📄',
        md: '📝', txt: '📄', asm: '⚡', s: '⚡',
        sql: '🗄️', sh: '💻', bat: '💻', ps1: '💻'
    };
    return icons[ext] || '📄';
}

// ============================================================================
// AI COPILOT - CONTEXT MENU
// ============================================================================

function setupContextMenu() {
    editor.addAction({
        id: 'ai-explain',
        label: '📖 Explain Code',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 1,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('explain', text);
            }
        }
    });

    editor.addAction({
        id: 'ai-fix',
        label: '🔧 Fix Code',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 2,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('fix', text);
            }
        }
    });

    editor.addAction({
        id: 'ai-optimize',
        label: '⚡ Optimize Code',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 3,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('optimize', text);
            }
        }
    });

    editor.addAction({
        id: 'ai-refactor',
        label: '🔄 Refactor Code',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 4,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('refactor', text);
            }
        }
    });

    editor.addAction({
        id: 'ai-tests',
        label: '🧪 Generate Tests',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 5,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('tests', text);
            }
        }
    });

    editor.addAction({
        id: 'ai-docs',
        label: '📝 Add Documentation',
        contextMenuGroupId: 'bigdaddyg',
        contextMenuOrder: 6,
        run: function (ed) {
            const selection = ed.getSelection();
            const text = ed.getModel().getValueInRange(selection);
            if (text) {
                aiAction('docs', text);
            }
        }
    });

    console.log('[BigDaddyG] ✅ Context menu configured');
}

async function aiAction(action, code) {
    console.log(`[BigDaddyG] 🤖 AI Action: ${action}`);

    let prompt = '';
    switch (action) {
        case 'explain':
            prompt = `Explain this code in detail:\n\n${code}`;
            break;
        case 'fix':
            prompt = `Find and fix any bugs in this code:\n\n${code}`;
            break;
        case 'optimize':
            prompt = `Optimize this code for performance:\n\n${code}`;
            break;
        case 'refactor':
            prompt = `Refactor this code following best practices:\n\n${code}`;
            break;
        case 'tests':
            prompt = `Generate unit tests for this code:\n\n${code}`;
            break;
        case 'docs':
            prompt = `Add comprehensive documentation to this code:\n\n${code}`;
            break;
    }

    try {
        // Query BigDaddyG
        const response = await fetch('http://localhost:11441/api/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                message: prompt,
                model: 'BigDaddyG:Code'
            })
        });

        const data = await response.json();

        // Extract code blocks
        const codeBlocks = extractCodeBlocks(data.response);

        if (codeBlocks.length > 0 && action !== 'explain') {
            // Show inline suggestion
            showInlineSuggestion(codeBlocks[0].code, action);
        } else {
            // Show explanation in chat
            addAIMessage(`BigDaddyG (${action}): ` + data.response);
        }

    } catch (error) {
        console.error('[BigDaddyG] ❌ AI error:', error);
        addAIMessage(`Error: ${error.message}`, true);
    }
}

function extractCodeBlocks(text) {
    const regex = /```(\w+)?\n([\s\S]*?)```/g;
    const blocks = [];
    let match;

    while ((match = regex.exec(text)) !== null) {
        blocks.push({
            language: match[1] || 'text',
            code: match[2].trim()
        });
    }

    return blocks;
}

// ============================================================================
// INLINE SUGGESTIONS (Cursor-style)
// ============================================================================

function showInlineSuggestion(suggestedCode, action) {
    // Create suggestion overlay
    const overlay = document.createElement('div');
    overlay.id = 'inline-suggestion-overlay';
    overlay.style.cssText = `
        position: fixed;
        top: 50%;
        left: 50%;
        transform: translate(-50%, -50%);
        background: rgba(10, 10, 30, 0.98);
        backdrop-filter: blur(20px);
        border: 2px solid var(--green);
        border-radius: 15px;
        padding: 25px;
        max-width: 70%;
        max-height: 70vh;
        overflow: auto;
        z-index: 10000;
        box-shadow: 0 10px 50px rgba(0, 255, 0, 0.5);
    `;

    overlay.innerHTML = `
        <div style="margin-bottom: 20px; padding-bottom: 15px; border-bottom: 2px solid var(--green);">
            <h3 style="color: var(--green); margin: 0;">🤖 BigDaddyG Suggestion: ${action.toUpperCase()}</h3>
        </div>
        
        <div style="margin-bottom: 20px;">
            <div style="color: var(--cyan); font-size: 13px; margin-bottom: 10px; font-weight: bold;">📝 Suggested Code:</div>
            <pre style="background: rgba(0,0,0,0.5); padding: 15px; border-radius: 8px; overflow-x: auto; max-height: 400px; font-size: 13px; line-height: 1.5; color: #fff;"><code>${escapeHtml(suggestedCode)}</code></pre>
        </div>
        
        <div style="display: flex; gap: 10px;">
            <button onclick="applySuggestion()" style="flex: 1; background: var(--green); color: var(--void); border: none; padding: 12px; border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 13px;">✅ Apply (Replace Selection)</button>
            <button onclick="insertSuggestion()" style="flex: 1; background: var(--cyan); color: var(--void); border: none; padding: 12px; border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 13px;">➕ Insert (Add Below)</button>
            <button onclick="rejectSuggestion()" style="background: var(--orange); color: var(--void); border: none; padding: 12px 20px; border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 13px;">❌ Reject</button>
        </div>
    `;

    document.body.appendChild(overlay);
    window.currentSuggestion = suggestedCode;

    console.log('[BigDaddyG] 💡 Inline suggestion shown');
}

function applySuggestion() {
    if (!window.currentSuggestion) return;

    const selection = editor.getSelection();
    const id = { major: 1, minor: 1 };
    const op = {
        identifier: id,
        range: selection,
        text: window.currentSuggestion,
        forceMoveMarkers: true
    };

    editor.executeEdits('bigdaddyg-apply', [op]);
    rejectSuggestion();

    console.log('[BigDaddyG] ✅ Suggestion applied');
}

function insertSuggestion() {
    if (!window.currentSuggestion) return;

    const selection = editor.getSelection();
    const position = { lineNumber: selection.endLineNumber + 1, column: 1 };

    editor.executeEdits('bigdaddyg-insert', [{
        range: new monaco.Range(position.lineNumber, position.column, position.lineNumber, position.column),
        text: '\n' + window.currentSuggestion
    }]);

    rejectSuggestion();

    console.log('[BigDaddyG] ✅ Suggestion inserted');
}

function rejectSuggestion() {
    const overlay = document.getElementById('inline-suggestion-overlay');
    if (overlay) overlay.remove();
    window.currentSuggestion = null;

    console.log('[BigDaddyG] ❌ Suggestion rejected');
}

function escapeHtml(text) {
    return text.replace(/[&<>"']/g, match => {
        const escapeMap = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#39;'
        };
        return escapeMap[match];
    });
}

// ============================================================================
// KEYBOARD SHORTCUTS
// ============================================================================

document.addEventListener('keydown', (e) => {
    // Ctrl+Tab / Ctrl+Shift+Tab for tab navigation
    if (e.ctrlKey && e.key === 'Tab') {
        e.preventDefault();
        if (e.shiftKey) {
            previousTab();
        } else {
            nextTab();
        }
    }

    // Ctrl+W to close current tab
    if (e.ctrlKey && e.key === 'w') {
        e.preventDefault();
        const tabs = Object.keys(openTabs);
        if (tabs.length > 1) {
            closeTab({ stopPropagation: () => { } }, activeTab);
        }
    }

    // Ctrl+Shift+W to close all tabs
    if (e.ctrlKey && e.shiftKey && e.key === 'W') {
        e.preventDefault();
        closeAllTabs();
    }

    // Ctrl+1 through Ctrl+9 for direct tab access
    if (e.ctrlKey && e.key >= '1' && e.key <= '9') {
        e.preventDefault();
        const index = parseInt(e.key) - 1;
        const tabs = Object.keys(openTabs);
        if (tabs[index]) {
            switchTab(tabs[index]);
        }
    }

    // Alt+Left/Right for tab navigation (browser-style)
    if (e.altKey) {
        if (e.key === 'ArrowLeft') {
            e.preventDefault();
            previousTab();
        } else if (e.key === 'ArrowRight') {
            e.preventDefault();
            nextTab();
        }
    }
});

console.log('[BigDaddyG] ⌨️ Tab keyboard shortcuts enabled:');
console.log('  • Ctrl+Tab / Ctrl+Shift+Tab - Next/Previous tab');
console.log('  • Ctrl+W - Close tab');
console.log('  • Ctrl+Shift+W - Close all tabs');
console.log('  • Ctrl+1-9 - Jump to tab 1-9');
console.log('  • Alt+Left/Right - Navigate tabs');

// ============================================================================
// AI CHAT
// ============================================================================

async function sendToAI() {
    const input = document.getElementById('ai-input');
    const message = input.value.trim();

    if (!message) return;

    input.value = '';

    // Add user message
    addUserMessage(message);

    // Add thinking indicator
    const thinkingId = addAIMessage('Thinking...', false, true);

    try {
        const response = await fetch('http://localhost:11441/api/chat', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                message: message,
                model: 'BigDaddyG:Latest'
            })
        });

        const data = await response.json();

        // Remove thinking indicator
        document.getElementById(thinkingId).remove();

        // Add AI response
        addAIMessage(data.response);

    } catch (error) {
        document.getElementById(thinkingId).remove();
        addAIMessage(`Error: ${error.message}`, true);
    }
}

function addUserMessage(message) {
    const container = document.getElementById('ai-messages');
    const msgEl = document.createElement('div');
    msgEl.className = 'ai-message user-message';
    msgEl.innerHTML = `<strong style="color: var(--orange);">You:</strong><br><br>${escapeHtml(message)}`;
    container.appendChild(msgEl);
    container.scrollTop = container.scrollHeight;
}

function addAIMessage(message, isError = false, isThinking = false) {
    const container = document.getElementById('ai-messages');
    const msgEl = document.createElement('div');
    const id = `ai-msg-${Date.now()}`;
    msgEl.id = id;
    msgEl.className = 'ai-message';

    if (isError) {
        msgEl.style.borderLeftColor = 'var(--red)';
        msgEl.innerHTML = `<strong style="color: var(--red);">Error:</strong><br><br>${escapeHtml(message)}`;
    } else if (isThinking) {
        msgEl.innerHTML = `<strong style="color: var(--cyan);">BigDaddyG:</strong><br><br><em style="opacity: 0.7;">${message}</em>`;
    } else {
        msgEl.innerHTML = `<strong style="color: var(--cyan);">BigDaddyG:</strong><br><br>${escapeHtml(message)}`;
    }

    container.appendChild(msgEl);
    container.scrollTop = container.scrollHeight;

    return id;
}

// ============================================================================
// MENU EVENTS
// ============================================================================

if (window.electron) {
    window.electron.onMenuEvent((event) => {
        console.log(`[BigDaddyG] 📋 Menu event: ${event}`);

        switch (event) {
            case 'new-file':
                const filename = prompt('Enter filename:');
                if (filename) {
                    const lang = detectLanguage(filename);
                    createNewTab(filename, lang);
                }
                break;

            case 'toggle-sidebar':
                document.getElementById('sidebar').classList.toggle('collapsed');
                break;

            case 'toggle-terminal':
                document.getElementById('bottom-panel').classList.toggle('collapsed');
                break;

            case 'ask-ai':
                document.getElementById('ai-input').focus();
                break;

            case 'ai-explain':
            case 'ai-fix':
            case 'ai-optimize':
                const selection = editor.getSelection();
                const text = editor.getModel().getValueInRange(selection);
                if (text) {
                    aiAction(event.replace('ai-', ''), text);
                }
                break;
        }
    });
}

function detectLanguage(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    const langMap = {
        js: 'javascript', ts: 'typescript', jsx: 'javascript', tsx: 'typescript',
        py: 'python', java: 'java', cpp: 'cpp', c: 'c',
        rs: 'rust', go: 'go', rb: 'ruby', php: 'php',
        html: 'html', css: 'css', json: 'json', xml: 'xml',
        md: 'markdown', txt: 'plaintext', asm: 'asm', s: 'asm',
        sql: 'sql', sh: 'shell', bat: 'bat', ps1: 'powershell'
    };
    return langMap[ext] || 'plaintext';
}

// Allow Enter key to send message
document.getElementById('ai-input').addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && e.ctrlKey) {
        sendToAI();
    }
});

console.log('[BigDaddyG] ✅ Renderer initialized');



// Wire up send button and enter key
if (typeof document !== 'undefined') {
    document.addEventListener('DOMContentLoaded', function () {
        const sendBtn = document.getElementById('ai-send-btn');
        const inputField = document.getElementById('ai-input');

        if (sendBtn) {
            sendBtn.addEventListener('click', sendToAI);
            console.log('[Chat] Send button wired');
        }

        if (inputField) {
            inputField.addEventListener('keypress', function (e) {
                if (e.key === 'Enter' && !e.shiftKey) {
                    e.preventDefault();
                    sendToAI();
                }
            });
            console.log('[Chat] Input field wired (Enter to send)');
        }
    });
}
