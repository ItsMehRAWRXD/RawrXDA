/**
 * BigDaddyG IDE - Enhanced Terminal with Full PowerShell Access
 * Features:
 * - Native PowerShell integration
 * - Command generation from AI
 * - Multi-shell support (PowerShell, CMD, Git Bash, WSL)
 * - Command history and suggestions
 * - Persistent sessions
 */

// Note: In renderer process, use window.electron (from preload)
// const { ipcRenderer } = require('electron'); // NOT NEEDED - use window.electron instead

(function() {
'use strict';

class EnhancedTerminal {
    constructor() {
        this.activeShell = 'powershell'; // powershell, cmd, bash, wsl
        this.history = [];
        this.historyIndex = -1;
        this.currentDirectory = (window.env && window.env.cwd) ? window.env.cwd() : 'C:\\';
        this.sessionId = `terminal_${Date.now()}`;
        this.outputBuffer = [];
        
        console.log('[EnhancedTerminal] 🖥️ Initializing enhanced terminal...');
        this.init();
    }
    
    init() {
        // Wait for DOM
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.setup());
        } else {
            this.setup();
        }
        
        // Load saved history
        this.loadHistory();
    }
    
    setup() {
        // Create terminal UI
        this.createTerminalUI();
        
        // Register IPC handlers
        this.registerIPCHandlers();
        
        // Register keyboard shortcuts
        this.registerHotkeys();
        
        console.log('[EnhancedTerminal] ✅ Enhanced terminal ready');
        console.log(`[EnhancedTerminal] 🖥️ Shell: ${this.activeShell}`);
        console.log(`[EnhancedTerminal] 📁 CWD: ${this.currentDirectory}`);
    }
    
    createTerminalUI() {
        const terminal = document.createElement('div');
        terminal.id = 'enhanced-terminal';
        terminal.style.cssText = `
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
            height: 300px;
            background: #1e1e1e;
            border-top: 2px solid var(--cursor-jade-dark);
            display: none;
            flex-direction: column;
            z-index: 9999;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
        `;
        
        terminal.innerHTML = `
            <!-- Terminal Header -->
            <div style="display: flex; align-items: center; justify-content: space-between; padding: 8px 16px; background: #252525; border-bottom: 1px solid #333;">
                <div style="display: flex; align-items: center; gap: 12px;">
                    <span style="color: var(--cursor-jade-dark); font-weight: 600; font-size: 13px;">🖥️ Terminal</span>
                    
                    <!-- Shell Selector -->
                    <select id="shell-selector" style="
                        background: #1e1e1e;
                        border: 1px solid var(--cursor-jade-light);
                        color: #fff;
                        padding: 4px 8px;
                        border-radius: 4px;
                        font-size: 11px;
                        cursor: pointer;
                    ">
                        <option value="powershell">⚡ PowerShell</option>
                        <option value="cmd">📝 CMD</option>
                        <option value="bash">🐚 Git Bash</option>
                        <option value="wsl">🐧 WSL</option>
                    </select>
                    
                    <!-- Current Directory -->
                    <span id="terminal-cwd" style="
                        font-size: 11px;
                        color: #888;
                        font-family: 'Consolas', monospace;
                    ">${this.currentDirectory}</span>
                </div>
                
                <div style="display: flex; gap: 8px;">
                    <!-- AI Command Generator -->
                    <button id="ai-cmd-gen-btn" style="
                        background: linear-gradient(135deg, var(--cursor-jade-dark), var(--cursor-accent));
                        border: none;
                        color: white;
                        padding: 4px 12px;
                        border-radius: 4px;
                        cursor: pointer;
                        font-size: 11px;
                        font-weight: 600;
                    " title="Generate command from description (Ctrl+Shift+G)">
                        🤖 AI Generate
                    </button>
                    
                    <!-- Clear -->
                    <button id="terminal-clear-btn" style="
                        background: rgba(255, 152, 0, 0.1);
                        border: 1px solid var(--orange);
                        color: var(--orange);
                        padding: 4px 12px;
                        border-radius: 4px;
                        cursor: pointer;
                        font-size: 11px;
                    " title="Clear terminal (Ctrl+L)">
                        🗑️ Clear
                    </button>
                    
                    <!-- Minimize -->
                    <button id="terminal-minimize-btn" style="
                        background: none;
                        border: 1px solid #555;
                        color: #aaa;
                        padding: 4px 12px;
                        border-radius: 4px;
                        cursor: pointer;
                        font-size: 11px;
                    " title="Minimize terminal (Ctrl+\`)">
                        ➖
                    </button>
                </div>
            </div>
            
            <!-- Terminal Output -->
            <div id="terminal-output" style="
                flex: 1;
                overflow-y: auto;
                padding: 12px;
                color: #d4d4d4;
                font-size: 13px;
                line-height: 1.5;
                font-family: 'Consolas', monospace;
            ">
                <div style="color: var(--cursor-jade-dark);">
                    BigDaddyG IDE Terminal - Type 'help' for commands
                </div>
            </div>
            
            <!-- Terminal Input -->
            <div style="display: flex; align-items: center; padding: 8px 12px; background: #252525; border-top: 1px solid #333;">
                <span id="terminal-prompt" style="color: var(--cursor-jade-dark); margin-right: 8px; font-weight: 600;">PS></span>
                <input type="text" id="terminal-input" placeholder="Type command or describe what you want to do..." style="
                    flex: 1;
                    background: #1e1e1e;
                    border: 1px solid #444;
                    color: #fff;
                    padding: 6px 12px;
                    border-radius: 4px;
                    font-size: 13px;
                    font-family: 'Consolas', monospace;
                    outline: none;
                " />
                <button id="terminal-send-btn" style="
                    margin-left: 8px;
                    background: var(--cursor-jade-dark);
                    border: none;
                    color: white;
                    padding: 6px 16px;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                    font-weight: 600;
                ">
                    ↵ Run
                </button>
            </div>
        `;
        
        document.body.appendChild(terminal);
        
        // Attach event handlers
        this.attachEventHandlers();
        
        console.log('[EnhancedTerminal] ✅ Terminal UI created');
    }
    
    attachEventHandlers() {
        // Shell selector
        const shellSelector = document.getElementById('shell-selector');
        if (shellSelector) {
            shellSelector.value = this.activeShell;
            shellSelector.addEventListener('change', (e) => {
                this.switchShell(e.target.value);
            });
        }
        
        // Terminal input
        const input = document.getElementById('terminal-input');
        if (input) {
            input.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    this.executeCommand(input.value);
                    input.value = '';
                } else if (e.key === 'ArrowUp') {
                    e.preventDefault();
                    this.navigateHistory(-1);
                } else if (e.key === 'ArrowDown') {
                    e.preventDefault();
                    this.navigateHistory(1);
                } else if (e.key === 'Tab') {
                    e.preventDefault();
                    this.autoComplete(input.value);
                }
            });
        }
        
        // Send button
        const sendBtn = document.getElementById('terminal-send-btn');
        if (sendBtn) {
            sendBtn.onclick = () => {
                if (input) {
                    this.executeCommand(input.value);
                    input.value = '';
                }
            };
        }
        
        // AI Command Generator
        const aiGenBtn = document.getElementById('ai-cmd-gen-btn');
        if (aiGenBtn) {
            aiGenBtn.onclick = () => this.showAICommandGenerator();
        }
        
        // Clear button
        const clearBtn = document.getElementById('terminal-clear-btn');
        if (clearBtn) {
            clearBtn.onclick = () => this.clearTerminal();
        }
        
        // Minimize button
        const minimizeBtn = document.getElementById('terminal-minimize-btn');
        if (minimizeBtn) {
            minimizeBtn.onclick = () => this.toggle();
        }
    }
    
    registerHotkeys() {
        document.addEventListener('keydown', (e) => {
            // Ctrl+` - Toggle terminal
            if (e.ctrlKey && e.key === '`') {
                e.preventDefault();
                this.toggle();
            }
            
            // Ctrl+Shift+G - AI Command Generator
            if (e.ctrlKey && e.shiftKey && e.key === 'G') {
                e.preventDefault();
                this.showAICommandGenerator();
            }
            
            // Ctrl+L - Clear terminal
            if (e.ctrlKey && e.key === 'l' && this.isVisible()) {
                e.preventDefault();
                this.clearTerminal();
            }
        });
    }
    
    registerIPCHandlers() {
        // Listen for terminal output from main process
        if (window.electron && window.electron.on) {
            window.electron.on('terminal-output', (data) => {
                this.appendOutput(data.output, data.isError ? 'error' : 'output');
            });
            
            window.electron.on('terminal-exit', (data) => {
                this.appendOutput(`Process exited with code ${data.code}`, 'system');
            });
        }
    }
    
    async executeCommand(command) {
        if (!command.trim()) return;
        
        // Add to history
        this.history.push(command);
        this.historyIndex = this.history.length;
        this.saveHistory();
        
        // Show command in output
        this.appendOutput(`${this.getPrompt()} ${command}`, 'command');
        
        // Check for built-in commands
        if (this.handleBuiltInCommand(command)) {
            return;
        }
        
        // Send to Electron main process for execution
        try {
            if (window.electron && window.electron.executeCommand) {
                const result = await window.electron.executeCommand({
                    command: command,
                    shell: this.activeShell,
                    cwd: this.currentDirectory
                });
                
                if (result.output) {
                    this.appendOutput(result.output, 'output');
                }
                
                if (result.error) {
                    this.appendOutput(result.error, 'error');
                }
                
                // Update CWD if changed
                if (result.cwd) {
                    this.currentDirectory = result.cwd;
                    this.updateCWDDisplay();
                }
                
            } else {
                this.appendOutput('⚠️ Terminal backend not available. Commands cannot be executed.', 'error');
            }
        } catch (error) {
            this.appendOutput(`❌ Error: ${error.message}`, 'error');
        }
    }
    
    handleBuiltInCommand(command) {
        const cmd = command.trim().toLowerCase();
        
        if (cmd === 'help') {
            this.showHelp();
            return true;
        }
        
        if (cmd === 'clear' || cmd === 'cls') {
            this.clearTerminal();
            return true;
        }
        
        if (cmd === 'history') {
            this.showHistory();
            return true;
        }
        
        if (cmd.startsWith('cd ')) {
            const newDir = command.substring(3).trim();
            this.changeDirectory(newDir);
            return true;
        }
        
        if (cmd === 'pwd') {
            this.appendOutput(this.currentDirectory, 'output');
            return true;
        }
        
        return false;
    }
    
    showHelp() {
        const help = `
╔════════════════════════════════════════════════════════════════╗
║         BigDaddyG IDE Terminal - Command Reference           ║
╚════════════════════════════════════════════════════════════════╝

🔷 Built-in Commands:
  help                   Show this help message
  clear, cls             Clear terminal output
  history                Show command history
  cd <path>              Change directory
  pwd                    Print working directory

🔷 AI Features:
  Ctrl+Shift+G           Generate command from description
  @ai <description>      Ask AI to generate and execute command

🔷 Shell Commands:
  - PowerShell: All PowerShell cmdlets available
  - CMD: All Windows commands available
  - Bash: Git Bash commands (if installed)
  - WSL: Linux commands (if WSL installed)

🔷 Keyboard Shortcuts:
  Ctrl+\`                Toggle terminal
  Enter                  Execute command
  ↑/↓                    Navigate history
  Tab                    Auto-complete
  Ctrl+L                 Clear terminal
  Ctrl+C                 Cancel current command

🔷 Examples:
  ls                     List files (PowerShell/Bash)
  dir                    List files (CMD)
  npm install            Install Node packages
  git status             Check Git status
  python script.py       Run Python script
  @ai find large files   AI generates command for you

╚════════════════════════════════════════════════════════════════╝
        `.trim();
        
        this.appendOutput(help, 'help');
    }
    
    showHistory() {
        if (this.history.length === 0) {
            this.appendOutput('No command history', 'system');
            return;
        }
        
        this.appendOutput('\n📜 Command History:', 'system');
        this.history.forEach((cmd, i) => {
            this.appendOutput(`  ${i + 1}. ${cmd}`, 'output');
        });
    }
    
    async showAICommandGenerator() {
        const description = prompt('Describe what you want to do:\n\nExamples:\n- "Find all .js files modified in the last 7 days"\n- "Install npm packages and run dev server"\n- "Create a backup of the current directory"');
        
        if (!description) return;
        
        this.appendOutput(`🤖 Generating command for: "${description}"`, 'system');
        
        try {
            // Send to Orchestra AI for command generation
            const response = await fetch('http://localhost:11441/api/generate-command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    description: description,
                    shell: this.activeShell,
                    os: os.platform(),
                    cwd: this.currentDirectory
                })
            });
            
            if (!response.ok) {
                throw new Error(`Server returned ${response.status}`);
            }
            
            const data = await response.json();
            
            if (data.command) {
                this.appendOutput(`✅ Generated: ${data.command}`, 'success');
                
                if (data.explanation) {
                    this.appendOutput(`💡 ${data.explanation}`, 'info');
                }
                
                // Ask user to confirm execution
                const shouldExecute = confirm(`Execute this command?\n\n${data.command}\n\nPress OK to run, Cancel to copy to input.`);
                
                if (shouldExecute) {
                    await this.executeCommand(data.command);
                } else {
                    const input = document.getElementById('terminal-input');
                    if (input) {
                        input.value = data.command;
                        input.focus();
                    }
                }
            } else {
                this.appendOutput('❌ Could not generate command', 'error');
            }
            
        } catch (error) {
            this.appendOutput(`❌ Error generating command: ${error.message}`, 'error');
            this.appendOutput('💡 Tip: Make sure Orchestra server is running', 'info');
        }
    }
    
    switchShell(shell) {
        this.activeShell = shell;
        
        // Update prompt
        const prompt = document.getElementById('terminal-prompt');
        if (prompt) {
            switch (shell) {
                case 'powershell':
                    prompt.textContent = 'PS>';
                    break;
                case 'cmd':
                    prompt.textContent = 'C:\\>';
                    break;
                case 'bash':
                    prompt.textContent = '$';
                    break;
                case 'wsl':
                    prompt.textContent = 'λ';
                    break;
            }
        }
        
        this.appendOutput(`Switched to ${shell}`, 'system');
        console.log(`[EnhancedTerminal] 🔄 Switched to: ${shell}`);
    }
    
    getPrompt() {
        switch (this.activeShell) {
            case 'powershell': return 'PS>';
            case 'cmd': return 'C:\\>';
            case 'bash': return '$';
            case 'wsl': return 'λ';
            default: return '>';
        }
    }
    
    appendOutput(text, type = 'output') {
        const output = document.getElementById('terminal-output');
        if (!output) return;
        
        const line = document.createElement('div');
        line.style.marginBottom = '4px';
        
        // Style based on type
        switch (type) {
            case 'command':
                line.style.color = '#fff';
                line.style.fontWeight = '600';
                break;
            case 'error':
                line.style.color = '#f44747';
                break;
            case 'success':
                line.style.color = '#4ec9b0';
                break;
            case 'system':
                line.style.color = '#569cd6';
                break;
            case 'info':
                line.style.color = '#ce9178';
                break;
            case 'help':
                line.style.color = '#9cdcfe';
                line.style.whiteSpace = 'pre';
                break;
            default:
                line.style.color = '#d4d4d4';
        }
        
        line.textContent = text;
        output.appendChild(line);
        
        // Auto-scroll
        output.scrollTop = output.scrollHeight;
        
        // Store in buffer
        this.outputBuffer.push({ text, type, timestamp: Date.now() });
    }
    
    clearTerminal() {
        const output = document.getElementById('terminal-output');
        if (output) {
            output.innerHTML = '<div style="color: var(--cursor-jade-dark);">Terminal cleared</div>';
        }
        this.outputBuffer = [];
        console.log('[EnhancedTerminal] 🗑️ Terminal cleared');
    }
    
    changeDirectory(newDir) {
        // This will be handled by the actual shell execution
        this.executeCommand(`cd "${newDir}"`);
    }
    
    updateCWDDisplay() {
        const cwdDisplay = document.getElementById('terminal-cwd');
        if (cwdDisplay) {
            cwdDisplay.textContent = this.currentDirectory;
        }
    }
    
    navigateHistory(direction) {
        if (this.history.length === 0) return;
        
        this.historyIndex = Math.max(0, Math.min(this.history.length, this.historyIndex + direction));
        
        const input = document.getElementById('terminal-input');
        if (input && this.historyIndex < this.history.length) {
            input.value = this.history[this.historyIndex];
        }
    }
    
    autoComplete(partial) {
        // Simple auto-completion (can be enhanced)
        const commands = ['ls', 'dir', 'cd', 'pwd', 'npm', 'git', 'python', 'node', 'clear', 'help', 'history'];
        const matches = commands.filter(cmd => cmd.startsWith(partial.toLowerCase()));
        
        if (matches.length === 1) {
            const input = document.getElementById('terminal-input');
            if (input) {
                input.value = matches[0] + ' ';
            }
        } else if (matches.length > 1) {
            this.appendOutput(`\nSuggestions: ${matches.join(', ')}`, 'info');
        }
    }
    
    saveHistory() {
        try {
            localStorage.setItem('terminal-history', JSON.stringify(this.history.slice(-100))); // Keep last 100
        } catch (error) {
            console.error('[EnhancedTerminal] ❌ Error saving history:', error);
        }
    }
    
    loadHistory() {
        try {
            const saved = localStorage.getItem('terminal-history');
            if (saved) {
                this.history = JSON.parse(saved);
                this.historyIndex = this.history.length;
                console.log(`[EnhancedTerminal] 📜 Loaded ${this.history.length} history items`);
            }
        } catch (error) {
            console.error('[EnhancedTerminal] ❌ Error loading history:', error);
        }
    }
    
    toggle() {
        const terminal = document.getElementById('enhanced-terminal');
        if (!terminal) return;
        
        if (terminal.style.display === 'none') {
            terminal.style.display = 'flex';
            const input = document.getElementById('terminal-input');
            if (input) input.focus();
            console.log('[EnhancedTerminal] ✅ Terminal opened');
        } else {
            terminal.style.display = 'none';
            console.log('[EnhancedTerminal] ➖ Terminal minimized');
        }
    }
    
    isVisible() {
        const terminal = document.getElementById('enhanced-terminal');
        return terminal && terminal.style.display !== 'none';
    }
    
    // Public API
    open() {
        const terminal = document.getElementById('enhanced-terminal');
        if (terminal) {
            terminal.style.display = 'flex';
            const input = document.getElementById('terminal-input');
            if (input) input.focus();
        }
    }
    
    close() {
        const terminal = document.getElementById('enhanced-terminal');
        if (terminal) {
            terminal.style.display = 'none';
        }
    }
    
    run(command) {
        return this.executeCommand(command);
    }
}

// Initialize
window.enhancedTerminal = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.enhancedTerminal = new EnhancedTerminal();
    });
} else {
    window.enhancedTerminal = new EnhancedTerminal();
}

// Export
window.EnhancedTerminal = EnhancedTerminal;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = EnhancedTerminal;
}

console.log('[EnhancedTerminal] 📦 Enhanced Terminal module loaded');
console.log('[EnhancedTerminal] 💡 Press Ctrl+` to toggle terminal');

})(); // End IIFE

