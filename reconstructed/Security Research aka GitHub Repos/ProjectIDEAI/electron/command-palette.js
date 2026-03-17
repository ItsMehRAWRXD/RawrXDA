/**
 * Enhanced Command Palette - File Search & Terminal Integration
 * Ctrl+Shift+P functionality with comprehensive command access
 */

class CommandPalette {
    constructor() {
        this.isOpen = false;
        this.currentFilter = '';
        this.selectedIndex = 0;
        this.commands = [];
        this.files = [];
        this.init();
    }

    init() {
        this.loadCommands();
        this.scanFiles();
    }

    loadCommands() {
        this.commands = [
            // File Operations
            { name: 'File: New File', action: () => this.createNewFile(), category: 'file' },
            { name: 'File: Open File', action: () => this.openFile(), category: 'file' },
            { name: 'File: Save', action: () => this.saveFile(), category: 'file' },
            { name: 'File: Save As', action: () => this.saveFileAs(), category: 'file' },
            
            // Terminal Commands
            { name: 'Terminal: New Terminal', action: () => this.openTerminal(), category: 'terminal' },
            { name: 'Terminal: New PowerShell', action: () => this.openPowerShell(), category: 'terminal' },
            { name: 'Terminal: New Command Prompt', action: () => this.openCommandPrompt(), category: 'terminal' },
            { name: 'Terminal: Run Build', action: () => this.runBuild(), category: 'terminal' },
            { name: 'Terminal: Run npm install', action: () => this.runCommand('npm install'), category: 'terminal' },
            { name: 'Terminal: Run npm start', action: () => this.runCommand('npm start'), category: 'terminal' },
            { name: 'Terminal: Run git status', action: () => this.runCommand('git status'), category: 'terminal' },
            
            // View Commands
            { name: 'View: Toggle Sidebar', action: () => this.toggleSidebar(), category: 'view' },
            { name: 'View: Toggle Terminal', action: () => this.toggleTerminal(), category: 'view' },
            { name: 'View: Reload Window', action: () => location.reload(), category: 'view' },
            
            // AI Commands
            { name: 'AI: Open Chat', action: () => this.openAIChat(), category: 'ai' },
            { name: 'AI: Generate Code', action: () => this.generateCode(), category: 'ai' },
        ];
    }
    
    // Add command dynamically (called by other modules)
    addCommand(commandObj) {
        if (!commandObj || !commandObj.name || !commandObj.action) {
            console.warn('[CommandPalette] Invalid command object:', commandObj);
            return;
        }
        
        this.commands.push({
            name: commandObj.name,
            action: commandObj.action,
            category: commandObj.category || 'other'
        });
    }

    async scanFiles() {
        try {
            if (window.electron && window.electron.scanWorkspace) {
                this.files = await window.electron.scanWorkspace();
            }
        } catch (error) {
            console.warn('[CommandPalette] File scanning not available:', error);
        }
    }

    show() {
        if (this.isOpen) {
            this.hide();
            return;
        }

        this.isOpen = true;
        this.selectedIndex = 0;
        this.currentFilter = '';
        
        const overlay = document.createElement('div');
        overlay.id = 'command-palette-overlay';
        overlay.innerHTML = `
            <div class="command-palette">
                <div class="palette-header">
                    <input type="text" id="palette-input" placeholder="Search files, commands..." autocomplete="off">
                </div>
                <div class="palette-results" id="palette-results"></div>
            </div>
        `;

        this.addStyles();
        document.body.appendChild(overlay);
        
        const input = document.getElementById('palette-input');
        input.focus();
        
        this.setupEventListeners();
        this.updateResults();
    }

    hide() {
        const overlay = document.getElementById('command-palette-overlay');
        if (overlay) {
            overlay.remove();
        }
        this.isOpen = false;
    }

    addStyles() {
        if (document.getElementById('command-palette-styles')) return;
        
        const styles = document.createElement('style');
        styles.id = 'command-palette-styles';
        styles.textContent = `
            #command-palette-overlay {
                position: fixed;
                top: 0;
                left: 0;
                right: 0;
                bottom: 0;
                background: rgba(0, 0, 0, 0.8);
                backdrop-filter: blur(5px);
                z-index: 10000;
                display: flex;
                justify-content: center;
                padding-top: 80px;
            }

            .command-palette {
                width: 700px;
                max-width: 90%;
                height: fit-content;
                max-height: 600px;
                background: rgba(15, 15, 35, 0.98);
                border: 1px solid var(--cyan, #00d4ff);
                border-radius: 12px;
                box-shadow: 0 20px 60px rgba(0, 0, 0, 0.6);
                display: flex;
                flex-direction: column;
                overflow: hidden;
            }

            .palette-header {
                padding: 0;
                border-bottom: 1px solid rgba(0, 212, 255, 0.2);
            }

            #palette-input {
                width: 100%;
                padding: 18px 24px;
                background: transparent;
                border: none;
                color: #fff;
                font-size: 16px;
                outline: none;
                font-family: 'Segoe UI', system-ui, sans-serif;
            }

            #palette-input::placeholder {
                color: rgba(255, 255, 255, 0.5);
            }

            .palette-results {
                flex: 1;
                overflow-y: auto;
                max-height: 500px;
            }

            .result-item {
                padding: 12px 24px;
                cursor: pointer;
                display: flex;
                align-items: center;
                gap: 12px;
                transition: background 0.15s;
                border-left: 3px solid transparent;
            }

            .result-item:hover,
            .result-item.selected {
                background: rgba(0, 212, 255, 0.1);
                border-left-color: var(--cyan, #00d4ff);
            }

            .result-icon {
                width: 20px;
                height: 20px;
                display: flex;
                align-items: center;
                justify-content: center;
                font-size: 14px;
                flex-shrink: 0;
            }

            .result-content {
                flex: 1;
                min-width: 0;
            }

            .result-title {
                color: #fff;
                font-size: 14px;
                font-weight: 500;
                margin-bottom: 2px;
                white-space: nowrap;
                overflow: hidden;
                text-overflow: ellipsis;
            }

            .result-subtitle {
                color: rgba(255, 255, 255, 0.6);
                font-size: 12px;
                white-space: nowrap;
                overflow: hidden;
                text-overflow: ellipsis;
            }

            .result-shortcut {
                background: rgba(0, 0, 0, 0.4);
                padding: 4px 8px;
                border-radius: 4px;
                font-size: 11px;
                color: var(--cyan, #00d4ff);
                font-family: monospace;
                flex-shrink: 0;
            }

            .category-header {
                padding: 8px 24px;
                background: rgba(0, 0, 0, 0.3);
                color: var(--cyan, #00d4ff);
                font-size: 12px;
                font-weight: 600;
                text-transform: uppercase;
                letter-spacing: 0.5px;
                border-bottom: 1px solid rgba(0, 212, 255, 0.1);
            }
        `;
        document.head.appendChild(styles);
    }

    setupEventListeners() {
        const overlay = document.getElementById('command-palette-overlay');
        const input = document.getElementById('palette-input');

        overlay.addEventListener('click', (e) => {
            if (e.target === overlay) this.hide();
        });

        input.addEventListener('input', (e) => {
            this.currentFilter = e.target.value;
            this.selectedIndex = 0;
            this.updateResults();
        });

        input.addEventListener('keydown', (e) => {
            switch (e.key) {
                case 'Escape':
                    this.hide();
                    break;
                case 'ArrowDown':
                    e.preventDefault();
                    this.moveSelection(1);
                    break;
                case 'ArrowUp':
                    e.preventDefault();
                    this.moveSelection(-1);
                    break;
                case 'Enter':
                    e.preventDefault();
                    this.executeSelected();
                    break;
            }
        });
    }

    updateResults() {
        const results = document.getElementById('palette-results');
        const filter = this.currentFilter.toLowerCase();
        
        let allItems = [];
        
        // Add commands
        const matchingCommands = this.commands.filter(cmd => 
            cmd.name.toLowerCase().includes(filter)
        );
        
        // Add files
        const matchingFiles = this.files.filter(file => 
            file.name.toLowerCase().includes(filter) ||
            file.path.toLowerCase().includes(filter)
        ).slice(0, 20); // Limit file results

        // Group by category
        const grouped = {};
        
        if (matchingCommands.length > 0) {
            matchingCommands.forEach(cmd => {
                const category = cmd.category || 'other';
                if (!grouped[category]) grouped[category] = [];
                grouped[category].push({
                    type: 'command',
                    item: cmd,
                    title: cmd.name,
                    subtitle: this.getCategoryName(category),
                    icon: this.getCategoryIcon(category)
                });
            });
        }

        if (matchingFiles.length > 0) {
            if (!grouped.files) grouped.files = [];
            matchingFiles.forEach(file => {
                grouped.files.push({
                    type: 'file',
                    item: file,
                    title: file.name,
                    subtitle: file.path,
                    icon: this.getFileIcon(file.name)
                });
            });
        }

        // Render results
        results.innerHTML = '';
        allItems = [];

        Object.entries(grouped).forEach(([category, items]) => {
            if (items.length === 0) return;

            const header = document.createElement('div');
            header.className = 'category-header';
            header.textContent = this.getCategoryName(category);
            results.appendChild(header);

            items.forEach((item, index) => {
                const element = this.createResultElement(item, allItems.length);
                results.appendChild(element);
                allItems.push(item);
            });
        });

        this.allItems = allItems;
        this.updateSelection();
    }

    createResultElement(item, index) {
        const element = document.createElement('div');
        element.className = 'result-item';
        element.dataset.index = index;
        
        element.innerHTML = `
            <div class="result-icon">${item.icon}</div>
            <div class="result-content">
                <div class="result-title">${item.title}</div>
                <div class="result-subtitle">${item.subtitle}</div>
            </div>
        `;

        element.addEventListener('click', () => {
            this.selectedIndex = index;
            this.executeSelected();
        });

        return element;
    }

    moveSelection(direction) {
        if (!this.allItems || this.allItems.length === 0) return;
        
        this.selectedIndex = Math.max(0, Math.min(
            this.allItems.length - 1,
            this.selectedIndex + direction
        ));
        
        this.updateSelection();
    }

    updateSelection() {
        document.querySelectorAll('.result-item').forEach((item, index) => {
            item.classList.toggle('selected', index === this.selectedIndex);
        });

        // Scroll to selected item
        const selected = document.querySelector('.result-item.selected');
        if (selected) {
            selected.scrollIntoView({ block: 'nearest' });
        }
    }

    executeSelected() {
        if (!this.allItems || this.selectedIndex >= this.allItems.length) return;
        
        const selected = this.allItems[this.selectedIndex];
        this.hide();
        
        if (selected.type === 'command') {
            selected.item.action();
        } else if (selected.type === 'file') {
            this.openFileInEditor(selected.item);
        }
    }

    // Command implementations
    createNewFile() {
        if (window.createNewTab) {
            window.createNewTab('untitled', 'plaintext', '', null);
        }
    }

    openFile() {
        if (window.electron && window.electron.openFileDialog) {
            window.electron.openFileDialog();
        }
    }

    saveFile() {
        if (window.saveFile) {
            window.saveFile();
        }
    }

    saveFileAs() {
        if (window.saveFileAs) {
            window.saveFileAs();
        }
    }

    openTerminal() {
        this.createTerminal('bash');
    }

    openPowerShell() {
        this.createTerminal('powershell');
    }

    openCommandPrompt() {
        this.createTerminal('cmd');
    }

    createTerminal(shell = 'bash') {
        const terminalPanel = document.getElementById('terminal-panel') || this.createTerminalPanel();
        
        const terminalId = `terminal-${Date.now()}`;
        const terminalTab = document.createElement('div');
        terminalTab.className = 'terminal-tab active';
        terminalTab.innerHTML = `
            <span>${shell.toUpperCase()}</span>
            <button onclick="this.parentElement.remove()" style="margin-left: 8px; background: none; border: none; color: #fff; cursor: pointer;">×</button>
        `;

        const terminal = document.createElement('div');
        terminal.id = terminalId;
        terminal.className = 'terminal-instance';
        terminal.style.cssText = `
            width: 100%;
            height: 100%;
            background: #1e1e1e;
            color: #fff;
            font-family: 'Consolas', monospace;
            padding: 10px;
            overflow-y: auto;
        `;

        if (window.electron && window.electron.createTerminal) {
            window.electron.createTerminal(terminalId, shell);
        } else {
            this.createWebTerminal(terminal, shell);
        }

        terminalPanel.querySelector('.terminal-tabs').appendChild(terminalTab);
        terminalPanel.querySelector('.terminal-content').appendChild(terminal);
        terminalPanel.style.display = 'flex';
    }

    createTerminalPanel() {
        const panel = document.createElement('div');
        panel.id = 'terminal-panel';
        panel.style.cssText = `
            position: fixed;
            bottom: 0;
            left: 0;
            right: 0;
            height: 300px;
            background: #2d2d30;
            border-top: 1px solid var(--cyan);
            display: none;
            flex-direction: column;
            z-index: 1000;
        `;

        panel.innerHTML = `
            <div class="terminal-tabs" style="display: flex; background: #1e1e1e; padding: 5px; gap: 5px;"></div>
            <div class="terminal-content" style="flex: 1; position: relative;"></div>
        `;

        document.body.appendChild(panel);
        return panel;
    }

    createWebTerminal(container, shell) {
        const prompt = shell === 'powershell' ? 'PS> ' : shell === 'cmd' ? 'C:\\> ' : '$ ';
        
        container.innerHTML = `
            <div class="terminal-output"></div>
            <div class="terminal-input-line">
                <span class="terminal-prompt">${prompt}</span>
                <input type="text" class="terminal-input" style="background: transparent; border: none; color: #fff; outline: none; flex: 1;">
            </div>
        `;

        const input = container.querySelector('.terminal-input');
        const output = container.querySelector('.terminal-output');

        input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                const command = input.value.trim();
                if (command) {
                    this.executeTerminalCommand(command, output, shell);
                    input.value = '';
                }
            }
        });

        input.focus();
    }

    executeTerminalCommand(command, output, shell) {
        const commandLine = document.createElement('div');
        commandLine.innerHTML = `<span style="color: #00d4ff;">${shell === 'powershell' ? 'PS> ' : shell === 'cmd' ? 'C:\\> ' : '$ '}</span>${command}`;
        output.appendChild(commandLine);

        if (window.electron && window.electron.executeCommand) {
            window.electron.executeCommand(command, shell).then(result => {
                const resultDiv = document.createElement('div');
                resultDiv.textContent = result;
                output.appendChild(resultDiv);
                output.scrollTop = output.scrollHeight;
            });
        } else {
            // Web fallback for basic commands
            this.handleWebCommand(command, output);
        }
    }

    handleWebCommand(command, output) {
        const resultDiv = document.createElement('div');
        
        if (command === 'help') {
            resultDiv.innerHTML = `Available commands: help, clear, echo, date`;
        } else if (command === 'clear') {
            output.innerHTML = '';
            return;
        } else if (command.startsWith('echo ')) {
            resultDiv.textContent = command.substring(5);
        } else if (command === 'date') {
            resultDiv.textContent = new Date().toString();
        } else {
            resultDiv.innerHTML = `<span style="color: #ff6b6b;">Command not found: ${command}</span>`;
        }
        
        output.appendChild(resultDiv);
        output.scrollTop = output.scrollHeight;
    }

    runCommand(command) {
        this.openTerminal();
        setTimeout(() => {
            const input = document.querySelector('.terminal-input');
            if (input) {
                input.value = command;
                input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' }));
            }
        }, 100);
    }

    runBuild() {
        // Detect build system and run appropriate command
        if (this.files.some(f => f.name === 'package.json')) {
            this.runCommand('npm run build');
        } else if (this.files.some(f => f.name === 'Makefile')) {
            this.runCommand('make');
        } else {
            this.runCommand('build');
        }
    }

    toggleSidebar() {
        const sidebar = document.getElementById('sidebar');
        if (sidebar) {
            sidebar.style.display = sidebar.style.display === 'none' ? 'flex' : 'none';
        }
    }

    toggleTerminal() {
        const terminal = document.getElementById('terminal-panel');
        if (terminal) {
            terminal.style.display = terminal.style.display === 'none' ? 'flex' : 'none';
        }
    }

    openAIChat() {
        if (window.floatingChat) {
            window.floatingChat.toggle();
        }
    }

    generateCode() {
        // Open AI with code generation prompt
        this.openAIChat();
        setTimeout(() => {
            const input = document.querySelector('#floating-chat-input, #ai-input');
            if (input) {
                input.value = 'Generate code for: ';
                input.focus();
            }
        }, 100);
    }

    openFileInEditor(file) {
        if (window.electron && window.electron.openFile) {
            window.electron.openFile(file.path);
        } else if (window.openFile) {
            window.openFile(file.path);
        }
    }

    // Helper methods
    getCategoryName(category) {
        const names = {
            file: 'Files',
            terminal: 'Terminal',
            view: 'View',
            ai: 'AI Assistant',
            files: 'Files',
            other: 'Commands'
        };
        return names[category] || 'Commands';
    }

    getCategoryIcon(category) {
        const icons = {
            file: '📄',
            terminal: '⚡',
            view: '👁️',
            ai: '🤖',
            other: '⚙️'
        };
        return icons[category] || '⚙️';
    }

    getFileIcon(filename) {
        const ext = filename.split('.').pop()?.toLowerCase();
        const icons = {
            js: '🟨',
            ts: '🔷',
            html: '🌐',
            css: '🎨',
            json: '📋',
            md: '📝',
            py: '🐍',
            java: '☕',
            cpp: '⚡',
            c: '⚡',
            go: '🐹',
            rs: '🦀',
            php: '🐘',
            rb: '💎',
            sh: '🐚',
            bat: '⚙️',
            exe: '⚙️',
            dll: '📚'
        };
        return icons[ext] || '📄';
    }
}

// Initialize and integrate with hotkey manager
window.commandPalette = new CommandPalette();

// Update hotkey manager to use enhanced command palette
if (window.hotkeyManager) {
    // Replace the existing Ctrl+Shift+P handler
    window.hotkeyManager.register('Ctrl+Shift+P', () => {
        window.commandPalette.show();
    }, 'Enhanced Command Palette');
}

console.log('[CommandPalette] 🎯 Enhanced command palette loaded');