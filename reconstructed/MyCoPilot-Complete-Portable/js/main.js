class MyCoPilotIDE {
    constructor() {
        this.editor = null;
        this.ws = null;
        this.backendAvailable = false;
        this.currentFile = null;
        this.currentProject = null;
        this.apiEndpoints = {
            base: 'http://localhost:8080',
            ws: 'ws://localhost:8081'
        };
        this.messageHistory = [];
        this.init();
    }

    async init() {
        await this.initEditor();
        await this.initWebSocket();
        await this.setupEventListeners();
        await this.initDriveExplorer();
    }

    initEditor() {
        const el = document.getElementById('editor');
        if (el && typeof CodeMirror !== 'undefined') {
            this.editor = CodeMirror(el, {
                theme: 'material-darker',
                lineNumbers: true,
                mode: 'javascript',
                autoCloseBrackets: true,
                matchBrackets: true
            });
        }
    }

    async checkService(endpoint) {
        try {
            const response = await fetch(`${this.apiEndpoints.base}${endpoint}`);
            return response.ok;
        } catch (e) {
            return false;
        }
    }

    initWebSocket() {
        try {
            this.ws = new WebSocket(`${this.apiEndpoints.ws}/copilot-bridge`);
            this.ws.onopen = () => {
                this.backendAvailable = true;
                this.addMessage('system', 'Backend connected');
            };
            this.ws.onclose = () => {
                this.backendAvailable = false;
                this.addMessage('system', 'Backend disconnected');
            };
        } catch (e) {
            this.backendAvailable = false;
        }
    }

    addMessage(sender, content) {
        this.messageHistory.push({ sender, content, timestamp: new Date().toISOString() });
        const chat = document.getElementById('ai-chat') || document.getElementById('ai-messages');
        if (chat) {
            const msg = document.createElement('div');
            msg.innerHTML = `<strong>${sender}:</strong> ${content}`;
            msg.style.cssText = 'margin:5px 0;padding:5px;background:#333;color:#fff;border-radius:3px;';
            chat.appendChild(msg);
            chat.scrollTop = chat.scrollHeight;
        }
    }

    async openFile(path) {
        if (!this.backendAvailable) {
            return this.addMessage('system', 'Backend service not available');
        }
        try {
            const res = await fetch(`${this.apiEndpoints.base}/api/file/read?path=${encodeURIComponent(path)}`);
            if (!res.ok) throw new Error('Failed to open file');
            const data = await res.json();
            if (this.editor) this.editor.setValue(data.content || '');
            this.currentFile = path;
            this.addMessage('system', `Opened: ${path}`);
        } catch (e) {
            this.addMessage('system', `Failed to open: ${e.message}`);
        }
    }

    async saveFile() {
        if (!this.currentFile) return this.saveFileAs();
        try {
            const content = this.editor ? this.editor.getValue() : '';
            const res = await fetch(`${this.apiEndpoints.base}/api/file/save`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ path: this.currentFile, content })
            });
            if (res.ok) this.addMessage('system', 'Saved');
            else throw new Error('Save failed');
        } catch (e) {
            this.addMessage('system', `Error: ${e.message}`);
        }
    }

    async compileCode() {
        if (!this.backendAvailable) {
            return this.addMessage('system', 'Backend service not available');
        }
        if (!this.currentFile) return this.addMessage('system', 'No file open');
        try {
            this.addMessage('system', 'Compiling...');
            const content = this.editor ? this.editor.getValue() : '';
            const res = await fetch(`${this.apiEndpoints.base}/api/compile`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ file: this.currentFile, content })
            });
            const result = await res.json();
            if (result.success) {
                this.addMessage('system', 'Compiled successfully');
                if (result.output) this.showOutput(result.output);
            } else {
                this.addMessage('system', `Compilation error: ${result.error}`);
            }
        } catch (e) {
            this.addMessage('system', `Error: ${e.message}`);
        }
    }

    async runCode() {
        if (!this.currentFile) return this.addMessage('system', 'No file open');
        try {
            this.addMessage('system', 'Running...');
            const content = this.editor ? this.editor.getValue() : '';
            const res = await fetch(`${this.apiEndpoints.base}/api/run`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ file: this.currentFile, content })
            });
            if (!res.ok) throw new Error('Run failed: ' + res.statusText);
            const result = await res.json();
            if (result.success) {
                this.addMessage('system', 'Executed successfully');
                if (result.output) this.showOutput(result.output);
            } else {
                this.addMessage('system', `Execution error: ${result.error}`);
            }
        } catch (e) {
            this.addMessage('system', `Error: ${e.message}`);
        }
    }

    showOutput(output) {
        let panel = document.getElementById('output-panel');
        if (!panel) {
            panel = document.createElement('div');
            panel.id = 'output-panel';
            panel.style.cssText = 'position:fixed;bottom:0;left:0;right:0;height:200px;background:#000;color:#0f0;font-family:monospace;z-index:999;border-top:2px solid #333;overflow-y:auto;padding:10px;';
            document.body.appendChild(panel);
        }
        panel.innerHTML = `<pre style="margin:0;">${output}</pre>`;
        panel.style.display = 'block';
    }

    async loadProject(path) {
        try {
            const res = await fetch(`${this.apiEndpoints.base}/api/project/load?path=${encodeURIComponent(path)}`);
            const project = await res.json();
            if (project.files) {
                this.currentProject = project;
                this.addMessage('system', `Loaded project: ${project.name}`);
                this.showFileTree(project.files);
            }
        } catch (e) {
            this.addMessage('system', 'Creating new project...');
            this.createBasicProject(path);
        }
    }

    createBasicProject(path) {
        this.currentProject = {
            name: path.split('/').pop() || 'New Project',
            path: path,
            files: []
        };
        this.addMessage('system', `Created project: ${this.currentProject.name}`);
    }

    showFileTree(files) {
        let tree = document.getElementById('file-tree');
        if (!tree) {
            tree = document.createElement('div');
            tree.id = 'file-tree';
            tree.style.cssText = 'position:fixed;left:0;top:0;width:250px;height:100vh;background:#1e1e1e;color:#fff;overflow-y:auto;padding:10px;border-right:1px solid #333;z-index:100;';
            document.body.appendChild(tree);
        }
        tree.innerHTML = `<h3>Files <button onclick="this.parentElement.remove()" style="float:right;background:#444;border:none;color:#fff;padding:2px 6px;">X</button></h3>
        <div onclick="ide.createNewFile()" style="cursor:pointer;padding:4px;background:#333;margin:2px 0;border-radius:2px;">+ New File</div>
        ${files.map(f => `<div onclick="ide.openFile('${f.path}')" style="cursor:pointer;padding:2px;margin:1px 0;" onmouseover="this.style.background='#333'" onmouseout="this.style.background='transparent'">${f.name}</div>`).join('')}`;
    }

    createNewFile() {
        const name = prompt('File name:');
        if (name) {
            this.currentFile = name;
            if (this.editor) this.editor.setValue('');
            this.addMessage('system', `New file: ${name}`);
        }
    }

    saveFileAs() {
        const path = prompt('Save as:');
        if (path) {
            this.currentFile = path;
            this.saveFile();
        }
    }

    setupEventListeners() {
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey && e.key === 's') {
                e.preventDefault();
                this.saveFile();
            }
            if (e.ctrlKey && e.key === 'o') {
                e.preventDefault();
                const path = prompt('Open file:');
                if (path) this.openFile(path);
            }
            if (e.ctrlKey && e.key === 'n') {
                e.preventDefault();
                this.createNewFile();
            }
            if (e.key === 'F5') {
                e.preventDefault();
                this.runCode();
            }
            if (e.key === 'F9') {
                e.preventDefault();
                this.compileCode();
            }
        });
    }
}

let ide;

window.addEventListener('DOMContentLoaded', () => {
    ide = new MyCoPilotIDE();
    window.ide = ide;
    console.log('IDE ready');
});
