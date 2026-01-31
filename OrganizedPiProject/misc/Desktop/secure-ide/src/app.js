// Secure IDE Application
class SecureIDE {
    constructor() {
        this.editor = null;
        this.terminal = null;
        this.websocket = null;
        this.currentFile = null;
        this.isConnected = false;
        this.aiEnabled = false;
        this.securityLevel = 'high';
        
        this.init();
    }

    async init() {
        console.log('Initializing Secure IDE...');
        
        // Initialize Monaco Editor
        await this.initMonacoEditor();
        
        // Initialize Terminal
        this.initTerminal();
        
        // Initialize WebSocket connection
        this.initWebSocket();
        
        // Setup event listeners
        this.setupEventListeners();
        
        // Load workspace
        this.loadWorkspace();
        
        console.log('Secure IDE initialized');
    }

    async initMonacoEditor() {
        return new Promise((resolve) => {
            require.config({ paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.45.0/min/vs' } });
            require(['vs/editor/editor.main'], () => {
                this.editor = monaco.editor.create(document.getElementById('monaco-editor'), {
                    value: '// Welcome to Secure IDE\n// Your secure coding environment\n\nfunction hello() {\n    console.log("Hello, Secure World!");\n}',
                    language: 'javascript',
                    theme: 'vs-dark',
                    automaticLayout: true,
                    minimap: { enabled: true },
                    wordWrap: 'on',
                    lineNumbers: 'on',
                    folding: true,
                    selectOnLineNumbers: true,
                    roundedSelection: false,
                    readOnly: false,
                    cursorStyle: 'line',
                    automaticLayout: true,
                    scrollBeyondLastLine: false,
                    fontSize: 14,
                    tabSize: 2,
                    insertSpaces: true,
                    detectIndentation: true,
                    renderWhitespace: 'selection',
                    renderControlCharacters: true,
                    fontLigatures: true,
                    suggest: {
                        showKeywords: true,
                        showSnippets: true,
                        showFunctions: true,
                        showConstructors: true,
                        showFields: true,
                        showVariables: true,
                        showClasses: true,
                        showStructs: true,
                        showInterfaces: true,
                        showModules: true,
                        showProperties: true,
                        showEvents: true,
                        showOperators: true,
                        showUnits: true,
                        showValues: true,
                        showConstants: true,
                        showEnums: true,
                        showEnumMembers: true,
                        showColors: true,
                        showFiles: true,
                        showReferences: true,
                        showFolders: true,
                        showTypeParameters: true,
                        showIssues: true,
                        showUsers: true,
                        showWords: true,
                    }
                });

                // Setup editor event listeners
                this.setupEditorEvents();
                resolve();
            });
        });
    }

    initTerminal() {
        const { Terminal } = window;
        const { FitAddon } = window;
        const { WebLinksAddon } = window;

        this.terminal = new Terminal({
            theme: {
                background: '#1e1e1e',
                foreground: '#d4d4d4',
                cursor: '#ffffff',
                selection: '#264f78',
                black: '#000000',
                red: '#cd3131',
                green: '#0dbc79',
                yellow: '#e5e510',
                blue: '#2472c8',
                magenta: '#bc3fbc',
                cyan: '#11a8cd',
                white: '#e5e5e5',
                brightBlack: '#666666',
                brightRed: '#f14c4c',
                brightGreen: '#23d18b',
                brightYellow: '#f5f543',
                brightBlue: '#3b8eea',
                brightMagenta: '#d670d6',
                brightCyan: '#29b8db',
                brightWhite: '#e5e5e5'
            },
            fontFamily: 'Consolas, "Courier New", monospace',
            fontSize: 14,
            cursorBlink: true,
            cursorStyle: 'block',
            scrollback: 1000,
            tabStopWidth: 4
        });

        this.fitAddon = new FitAddon.FitAddon();
        this.webLinksAddon = new WebLinksAddon.WebLinksAddon();

        this.terminal.loadAddon(this.fitAddon);
        this.terminal.loadAddon(this.webLinksAddon);
        this.terminal.open(document.getElementById('terminal'));
        this.fitAddon.fit();

        // Setup terminal event listeners
        this.setupTerminalEvents();
    }

    initWebSocket() {
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.hostname}:3001`;
        
        this.websocket = new WebSocket(wsUrl);
        
        this.websocket.onopen = () => {
            console.log('Connected to Secure IDE server');
            this.isConnected = true;
            this.updateConnectionStatus(true);
        };
        
        this.websocket.onmessage = (event) => {
            this.handleWebSocketMessage(JSON.parse(event.data));
        };
        
        this.websocket.onclose = () => {
            console.log('Disconnected from Secure IDE server');
            this.isConnected = false;
            this.updateConnectionStatus(false);
        };
        
        this.websocket.onerror = (error) => {
            console.error('WebSocket error:', error);
            this.updateConnectionStatus(false);
        };
    }

    setupEventListeners() {
        // Navigation buttons
        document.getElementById('fileMenuBtn').addEventListener('click', () => this.showFileMenu());
        document.getElementById('editMenuBtn').addEventListener('click', () => this.showEditMenu());
        document.getElementById('viewMenuBtn').addEventListener('click', () => this.showViewMenu());
        document.getElementById('aiMenuBtn').addEventListener('click', () => this.showAIMenu());
        document.getElementById('toolsMenuBtn').addEventListener('click', () => this.showToolsMenu());

        // Sidebar tabs
        document.querySelectorAll('.sidebar-tab').forEach(tab => {
            tab.addEventListener('click', () => this.switchSidebarTab(tab.dataset.tab));
        });

        // File operations
        document.getElementById('newFileBtn').addEventListener('click', () => this.createNewFile());
        document.getElementById('newFolderBtn').addEventListener('click', () => this.createNewFolder());
        document.getElementById('refreshBtn').addEventListener('click', () => this.refreshFileTree());

        // Search
        document.getElementById('searchBtn').addEventListener('click', () => this.performSearch());
        document.getElementById('searchInput').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.performSearch();
        });

        // AI Chat
        document.getElementById('sendBtn').addEventListener('click', () => this.sendAIMessage());
        document.getElementById('chatInput').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') this.sendAIMessage();
        });

        // Terminal
        document.getElementById('newTerminalBtn').addEventListener('click', () => this.createNewTerminal());
        document.getElementById('clearTerminalBtn').addEventListener('click', () => this.clearTerminal());
        document.getElementById('toggleTerminalBtn').addEventListener('click', () => this.toggleTerminal());

        // Settings
        document.getElementById('settingsBtn').addEventListener('click', () => this.showSettings());
        document.getElementById('closeSettingsBtn').addEventListener('click', () => this.hideSettings());
        document.getElementById('saveSettingsBtn').addEventListener('click', () => this.saveSettings());

        // Context menu
        document.addEventListener('contextmenu', (e) => this.showContextMenu(e));
        document.addEventListener('click', () => this.hideContextMenu());

        // Keyboard shortcuts
        document.addEventListener('keydown', (e) => this.handleKeyboardShortcuts(e));
    }

    setupEditorEvents() {
        this.editor.onDidChangeModelContent(() => {
            this.handleEditorContentChange();
        });

        this.editor.onDidChangeCursorPosition(() => {
            this.updateCursorPosition();
        });

        this.editor.onDidChangeModelSelection(() => {
            this.updateSelectionInfo();
        });

        this.editor.onDidFocusEditorText(() => {
            this.handleEditorFocus();
        });

        // AI suggestions
        this.editor.onDidChangeModelContent(() => {
            this.requestAISuggestions();
        });
    }

    setupTerminalEvents() {
        this.terminal.onData((data) => {
            // Handle terminal input
            this.sendTerminalInput(data);
        });

        this.terminal.onSelectionChange(() => {
            // Handle terminal selection
        });
    }

    // File Operations
    async createNewFile() {
        const fileName = prompt('Enter file name:');
        if (fileName) {
            this.sendWebSocketMessage({
                type: 'create_file',
                data: { fileName }
            });
        }
    }

    async createNewFolder() {
        const folderName = prompt('Enter folder name:');
        if (folderName) {
            this.sendWebSocketMessage({
                type: 'create_folder',
                data: { folderName }
            });
        }
    }

    async openFile(filePath) {
        this.sendWebSocketMessage({
            type: 'open_file',
            data: { filePath }
        });
    }

    async saveFile() {
        if (this.currentFile) {
            const content = this.editor.getValue();
            this.sendWebSocketMessage({
                type: 'save_file',
                data: { filePath: this.currentFile, content }
            });
        }
    }

    // AI Operations
    async requestAISuggestions() {
        if (!this.aiEnabled) return;

        const content = this.editor.getValue();
        const cursorPosition = this.editor.getPosition();
        
        this.sendWebSocketMessage({
            type: 'ai_request',
            data: {
                type: 'code_completion',
                content,
                cursorPosition,
                language: this.editor.getModel().getLanguageId()
            }
        });
    }

    async sendAIMessage() {
        const input = document.getElementById('chatInput');
        const message = input.value.trim();
        
        if (message) {
            this.addChatMessage('user', message);
            input.value = '';
            
            this.sendWebSocketMessage({
                type: 'ai_request',
                data: {
                    type: 'chat',
                    content: message
                }
            });
        }
    }

    addChatMessage(sender, content) {
        const messagesContainer = document.getElementById('chatMessages');
        const messageDiv = document.createElement('div');
        messageDiv.className = `message ${sender}`;
        
        const contentDiv = document.createElement('div');
        contentDiv.className = 'message-content';
        
        if (sender === 'assistant') {
            const icon = document.createElement('i');
            icon.className = 'fas fa-robot';
            contentDiv.appendChild(icon);
        }
        
        const text = document.createElement('p');
        text.textContent = content;
        contentDiv.appendChild(text);
        
        messageDiv.appendChild(contentDiv);
        messagesContainer.appendChild(messageDiv);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    // Terminal Operations
    createNewTerminal() {
        this.terminal.clear();
        this.terminal.write('$ ');
    }

    clearTerminal() {
        this.terminal.clear();
    }

    toggleTerminal() {
        const panel = document.getElementById('terminalPanel');
        const btn = document.getElementById('toggleTerminalBtn');
        const icon = btn.querySelector('i');
        
        if (panel.classList.contains('collapsed')) {
            panel.classList.remove('collapsed');
            icon.className = 'fas fa-chevron-down';
        } else {
            panel.classList.add('collapsed');
            icon.className = 'fas fa-chevron-up';
        }
    }

    sendTerminalInput(data) {
        this.sendWebSocketMessage({
            type: 'terminal_input',
            data: { input: data }
        });
    }

    // UI Operations
    switchSidebarTab(tabName) {
        // Hide all panels
        document.querySelectorAll('.sidebar-panel').forEach(panel => {
            panel.classList.remove('active');
        });
        
        // Remove active class from all tabs
        document.querySelectorAll('.sidebar-tab').forEach(tab => {
            tab.classList.remove('active');
        });
        
        // Show selected panel
        document.getElementById(`${tabName}Panel`).classList.add('active');
        
        // Add active class to selected tab
        document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');
    }

    showSettings() {
        document.getElementById('settingsModal').classList.add('show');
    }

    hideSettings() {
        document.getElementById('settingsModal').classList.remove('show');
    }

    saveSettings() {
        // Save settings logic
        this.hideSettings();
    }

    showContextMenu(event) {
        event.preventDefault();
        const menu = document.getElementById('fileContextMenu');
        menu.style.left = event.pageX + 'px';
        menu.style.top = event.pageY + 'px';
        menu.classList.add('show');
    }

    hideContextMenu() {
        document.getElementById('fileContextMenu').classList.remove('show');
    }

    // WebSocket Communication
    sendWebSocketMessage(message) {
        if (this.websocket && this.websocket.readyState === WebSocket.OPEN) {
            this.websocket.send(JSON.stringify(message));
        }
    }

    handleWebSocketMessage(message) {
        switch (message.type) {
            case 'file_content':
                this.handleFileContent(message.data);
                break;
            case 'ai_response':
                this.handleAIResponse(message.data);
                break;
            case 'terminal_output':
                this.handleTerminalOutput(message.data);
                break;
            case 'file_tree':
                this.updateFileTree(message.data);
                break;
            case 'search_results':
                this.updateSearchResults(message.data);
                break;
            case 'security_status':
                this.updateSecurityStatus(message.data);
                break;
            default:
                console.log('Unknown message type:', message.type);
        }
    }

    handleFileContent(data) {
        this.currentFile = data.filePath;
        this.editor.setValue(data.content);
        this.editor.setModel(monaco.editor.createModel(data.content, data.language));
        this.addEditorTab(data.filePath);
    }

    handleAIResponse(data) {
        if (data.type === 'chat') {
            this.addChatMessage('assistant', data.content);
        } else if (data.type === 'suggestions') {
            this.showAISuggestions(data.suggestions);
        }
    }

    handleTerminalOutput(data) {
        this.terminal.write(data.output);
    }

    // Utility Methods
    updateCursorPosition() {
        const position = this.editor.getPosition();
        document.getElementById('cursorPosition').textContent = `Ln ${position.lineNumber}, Col ${position.column}`;
    }

    updateSelectionInfo() {
        const selection = this.editor.getSelection();
        if (selection && !selection.isEmpty()) {
            const selectedText = this.editor.getModel().getValueInRange(selection);
            document.getElementById('selectionInfo').textContent = `${selectedText.length} chars selected`;
        } else {
            document.getElementById('selectionInfo').textContent = '';
        }
    }

    updateConnectionStatus(connected) {
        const status = document.getElementById('connectionStatus');
        const icon = status.querySelector('i');
        const text = status.querySelector('span');
        
        if (connected) {
            icon.className = 'fas fa-wifi';
            text.textContent = 'Connected';
            status.style.color = '#4caf50';
        } else {
            icon.className = 'fas fa-wifi-slash';
            text.textContent = 'Disconnected';
            status.style.color = '#f44336';
        }
    }

    updateSecurityStatus(data) {
        this.securityLevel = data.level;
        document.getElementById('securityLevel').textContent = `Security: ${data.level}`;
    }

    handleKeyboardShortcuts(e) {
        if (e.ctrlKey || e.metaKey) {
            switch (e.key) {
                case 's':
                    e.preventDefault();
                    this.saveFile();
                    break;
                case 'n':
                    e.preventDefault();
                    this.createNewFile();
                    break;
                case 'o':
                    e.preventDefault();
                    this.openFileDialog();
                    break;
                case 'f':
                    e.preventDefault();
                    this.focusSearch();
                    break;
                case '`':
                    e.preventDefault();
                    this.toggleTerminal();
                    break;
            }
        }
    }

    addEditorTab(filePath) {
        const tabsContainer = document.getElementById('editorTabs');
        const tab = document.createElement('button');
        tab.className = 'editor-tab active';
        const fileName = filePath.split('/').pop();
        const escapedFileName = fileName.replace(/[&<>"']/g, function(match) {
            return {
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                '"': '&quot;',
                "'": '&#39;'
            }[match];
        });
        tab.innerHTML = `
            <span>${escapedFileName}</span>
            <button class="close-btn">
                <i class="fas fa-times"></i>
            </button>
        `;
        
        tab.addEventListener('click', () => this.switchToTab(filePath));
        tab.querySelector('.close-btn').addEventListener('click', (e) => {
            e.stopPropagation();
            this.closeTab(filePath);
        });
        
        tabsContainer.appendChild(tab);
    }

    switchToTab(filePath) {
        // Switch to tab logic
    }

    closeTab(filePath) {
        // Close tab logic
    }

    loadWorkspace() {
        this.sendWebSocketMessage({
            type: 'load_workspace',
            data: {}
        });
    }

    refreshFileTree() {
        this.sendWebSocketMessage({
            type: 'refresh_file_tree',
            data: {}
        });
    }

    performSearch() {
        const query = document.getElementById('searchInput').value;
        if (query) {
            this.sendWebSocketMessage({
                type: 'search_files',
                data: { query }
            });
        }
    }

    updateFileTree(data) {
        const fileTree = document.getElementById('fileTree');
        fileTree.innerHTML = '';
        
        data.files.forEach(file => {
            const item = document.createElement('div');
            item.className = `file-item ${file.isDirectory ? 'folder' : 'file'}`;
            item.innerHTML = `
                <i class="fas fa-${file.isDirectory ? 'folder' : 'file'}"></i>
                <span>${file.name}</span>
            `;
            
            if (!file.isDirectory) {
                item.addEventListener('click', () => this.openFile(file.path));
            }
            
            fileTree.appendChild(item);
        });
    }

    updateSearchResults(data) {
        const resultsContainer = document.getElementById('searchResults');
        resultsContainer.innerHTML = '';
        
        data.results.forEach(result => {
            const item = document.createElement('div');
            item.className = 'search-result';
            item.innerHTML = `
                <div>${result.fileName}</div>
                <div style="font-size: 12px; color: #888;">${result.path}</div>
            `;
            
            item.addEventListener('click', () => this.openFile(result.path));
            resultsContainer.appendChild(item);
        });
    }

    showAISuggestions(suggestions) {
        // Show AI suggestions in editor
        console.log('AI Suggestions:', suggestions);
    }

    handleEditorContentChange() {
        // Handle content changes
    }

    handleEditorFocus() {
        // Handle editor focus
    }

    focusSearch() {
        document.getElementById('searchInput').focus();
    }

    openFileDialog() {
        // Open file dialog logic
    }
}

// Initialize the application when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    window.secureIDE = new SecureIDE();
});