class ExtensionManager {
    constructor(ide) {
        this.ide = ide;
        this.extensions = new Map();
        this.extensionsPath = 'D:\\MyCoPilot-Complete-Portable\\extensions';
        window.extensionAPI = this.createAPI();
    }

    createAPI() {
        return {
            ide: this.ide,
            createExtension: (manifest, code) => this.createExtension(manifest, code),
            downloadExtension: (url) => this.downloadExtension(url),
            installVSIX: (path) => this.installFromVSIX(path),
            getExtensions: () => this.getExtensions(),
            enableExtension: (id) => this.toggleExtension(id, true),
            disableExtension: (id) => this.toggleExtension(id, false),
            executeCommand: (cmd) => this.ide.executeCommand(cmd),
            registerCommand: (name, handler) => this.registerCommand(name, handler),
            modifyIDE: (changes) => this.applyIDEChanges(changes)
        };
    }

    async loadExtensions() {
        try {
            const response = await fetch(`http://localhost:8080/api/file/list?path=${encodeURIComponent(this.extensionsPath)}`);
            if (!response.ok) return;
            
            const files = await response.json();
            for (const file of files) {
                if (file.type === 'directory') {
                    await this.loadExtension(file.name);
                }
            }
        } catch (e) {
            console.log('No extensions folder found');
        }
    }

    async loadExtension(name) {
        try {
            const manifestPath = `${this.extensionsPath}\\${name}\\package.json`;
            const response = await fetch(`http://localhost:8080/api/file/read?path=${encodeURIComponent(manifestPath)}`);
            const manifestPayload = await response.json();
            const manifest = typeof manifestPayload === 'object' && manifestPayload.content
                ? JSON.parse(manifestPayload.content)
                : manifestPayload;
            
            const ext = {
                id: manifest.name,
                name: manifest.displayName || manifest.name,
                version: manifest.version,
                description: manifest.description,
                main: manifest.main,
                contributes: manifest.contributes || {},
                enabled: true,
                path: `${this.extensionsPath}\\${name}`
            };
            
            if (ext.main) {
                const mainPath = `${ext.path}\\${ext.main}`;
                const codeRes = await fetch(`http://localhost:8080/api/file/read?path=${encodeURIComponent(mainPath)}`);
                const codePayload = await codeRes.json();
                const code = codePayload && codePayload.content ? codePayload.content : '';
                ext.activate = new Function('ide', `${code}\n//# sourceURL=${name}/extension.js`);
            }
            
            this.extensions.set(ext.id, ext);
            if (ext.activate) ext.activate(this.ide);
            
            console.log(`Loaded extension: ${ext.name}`);
        } catch (e) {
            console.error(`Failed to load extension ${name}:`, e);
        }
    }

    async installFromVSIX(vsixPath) {
        const response = await fetch('http://localhost:8080/api/extension/install', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ vsixPath })
        });
        return await response.json();
    }

    getExtensions() {
        return Array.from(this.extensions.values());
    }

    toggleExtension(id, enabled) {
        const ext = this.extensions.get(id);
        if (ext) ext.enabled = enabled !== undefined ? enabled : !ext.enabled;
    }

    async createExtension(manifest, code) {
        const extPath = `${this.extensionsPath}\\${manifest.name}`;
        await fetch('http://localhost:8080/api/file/write', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                path: `${extPath}\\package.json`,
                content: JSON.stringify(manifest, null, 2)
            })
        });
        await fetch('http://localhost:8080/api/file/write', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                path: `${extPath}\\extension.js`,
                content: code
            })
        });
        await this.loadExtension(manifest.name);
        return { success: true, id: manifest.name };
    }

    async downloadExtension(url) {
        const response = await fetch('http://localhost:8080/api/extension/download', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ url })
        });
        const result = await response.json();
        if (result.success) await this.loadExtension(result.name);
        return result;
    }

    registerCommand(name, handler) {
        if (!this.ide.commands) this.ide.commands = new Map();
        this.ide.commands.set(name, handler);
    }

    applyIDEChanges(changes) {
        if (changes.theme) this.ide.setTheme(changes.theme);
        if (changes.keybindings) Object.assign(this.ide.keybindings, changes.keybindings);
        if (changes.settings) Object.assign(this.ide.settings, changes.settings);
        if (changes.ui) this.modifyUI(changes.ui);
    }

    modifyUI(uiChanges) {
        if (uiChanges.addPanel) {
            const panel = document.createElement('div');
            panel.innerHTML = uiChanges.addPanel.html;
            document.querySelector(uiChanges.addPanel.target).appendChild(panel);
        }
        if (uiChanges.addButton) {
            const btn = document.createElement('button');
            btn.textContent = uiChanges.addButton.label;
            btn.onclick = uiChanges.addButton.handler;
            document.querySelector(uiChanges.addButton.target).appendChild(btn);
        }
    }
}
