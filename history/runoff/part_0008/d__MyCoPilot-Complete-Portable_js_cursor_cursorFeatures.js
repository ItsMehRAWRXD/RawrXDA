class CursorFeatures {
    constructor(ide) {
        this.ide = ide;
        this.context = new Map();
        this.composer = null;
        this.settings = this.loadSettings();
    }

    loadSettings() {
        return JSON.parse(localStorage.getItem('cursor-settings') || JSON.stringify({
            model: 'gpt-4',
            contextMax: 32000,
            autoContextFiles: true,
            includeGitDiff: true,
            includeLinterErrors: true,
            temperature: 0.7,
            streaming: true,
            autocomplete: { enabled: true, delay: 300 }
        }));
    }

    async addContext(type, value) {
        this.context.set(type, value);
        if (type === 'file') {
            const content = await fetch(`http://localhost:8080/api/file/read?path=${encodeURIComponent(value)}`).then(r => r.text());
            this.context.set(`file:${value}`, content);
        }
    }

    openComposer() {
        if (!this.composer) {
            this.composer = document.createElement('div');
            this.composer.className = 'cursor-composer';
            this.composer.innerHTML = `
                <div class="composer-header">
                    <span>Composer (Ctrl+I)</span>
                    <button onclick="ide.cursorFeatures.closeComposer()">×</button>
                </div>
                <div class="composer-context"></div>
                <textarea placeholder="Describe changes across multiple files..."></textarea>
                <button onclick="ide.cursorFeatures.executeComposer()">Generate</button>
            `;
            document.body.appendChild(this.composer);
        }
        this.composer.style.display = 'block';
    }

    closeComposer() {
        if (this.composer) this.composer.style.display = 'none';
    }

    async executeComposer() {
        const prompt = this.composer.querySelector('textarea').value;
        const context = Array.from(this.context.entries());
        const response = await fetch('http://localhost:8080/api/ollama', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                model: 'codellama:7b',
                prompt: `Context: ${JSON.stringify(context)}\n\nTask: ${prompt}`,
                stream: false
            })
        });
        const result = await response.json();
        this.applyComposerChanges(result.response);
    }

    applyComposerChanges(changes) {
        console.log('Applying changes:', changes);
    }

    setupKeybindings() {
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.key === 'i') {
                e.preventDefault();
                this.openComposer();
            }
            if (e.ctrlKey && e.key === 'k') {
                e.preventDefault();
                this.ide.showInlineChat();
            }
            if (e.ctrlKey && e.key === 'l') {
                e.preventDefault();
                this.ide.toggleAIPanel();
            }
        });
    }

    addContextMenu() {
        this.ide.editor.container.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            const menu = document.createElement('div');
            menu.className = 'cursor-context-menu';
            menu.style.left = e.pageX + 'px';
            menu.style.top = e.pageY + 'px';
            menu.innerHTML = `
                <div onclick="ide.cursorFeatures.addToChat()">Add to Chat</div>
                <div onclick="ide.cursorFeatures.addToComposer()">Add to Composer</div>
                <div onclick="ide.cursorFeatures.editWithAI()">Edit with AI</div>
                <div onclick="ide.cursorFeatures.generateComment()">Generate Comment</div>
                <div onclick="ide.cursorFeatures.generateTests()">Generate Tests</div>
                <div onclick="ide.cursorFeatures.explainCode()">Explain Code</div>
                <div onclick="ide.cursorFeatures.fixBug()">Fix Bug</div>
            `;
            document.body.appendChild(menu);
            setTimeout(() => menu.remove(), 5000);
        });
    }

    addToChat() {
        const selection = this.ide.editor.getSelection();
        this.ide.sendAIMessage(`Explain this code:\n\`\`\`\n${selection}\n\`\`\``);
    }

    addToComposer() {
        const selection = this.ide.editor.getSelection();
        this.addContext('selection', selection);
        this.openComposer();
    }

    async editWithAI() {
        const selection = this.ide.editor.getSelection();
        const prompt = window.prompt('How should I edit this code?');
        if (prompt) {
            const response = await fetch('http://localhost:8080/api/ollama', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model: 'codellama:7b',
                    prompt: `Edit this code: ${selection}\n\nInstruction: ${prompt}`,
                    stream: false
                })
            });
            const result = await response.json();
            this.ide.editor.replaceSelection(result.response);
        }
    }

    async generateComment() {
        const selection = this.ide.editor.getSelection();
        const response = await fetch('http://localhost:8080/api/ollama', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                model: 'codellama:7b',
                prompt: `Generate a documentation comment for:\n${selection}`,
                stream: false
            })
        });
        const result = await response.json();
        this.ide.editor.insertAtCursor(`/**\n * ${result.response}\n */\n`);
    }

    async generateTests() {
        const selection = this.ide.editor.getSelection();
        const response = await fetch('http://localhost:8080/api/ollama', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                model: 'codellama:7b',
                prompt: `Generate unit tests for:\n${selection}`,
                stream: false
            })
        });
        const result = await response.json();
        this.ide.createFile('test.js', result.response);
    }

    explainCode() {
        const selection = this.ide.editor.getSelection();
        this.ide.sendAIMessage(`Explain this code:\n\`\`\`\n${selection}\n\`\`\``);
    }

    async fixBug() {
        const selection = this.ide.editor.getSelection();
        const response = await fetch('http://localhost:8080/api/ollama', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                model: 'codellama:7b',
                prompt: `Fix bugs in this code:\n${selection}`,
                stream: false
            })
        });
        const result = await response.json();
        this.ide.editor.replaceSelection(result.response);
    }
}
