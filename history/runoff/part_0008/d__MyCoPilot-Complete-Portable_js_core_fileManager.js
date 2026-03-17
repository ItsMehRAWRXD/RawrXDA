class FileManager {
    constructor() {
        this.workspaceRoot = 'd:\\';
        this.fileTree = document.getElementById('file-tree');
        this.loadedFolders = new Map();
        this.pageSize = 100;
        this.cache = new Map();
        this.initDragDrop();
    }
    
    initDragDrop() {
        document.getElementById('editor').addEventListener('dragover', (e) => {
            e.preventDefault();
        });
        document.getElementById('editor').addEventListener('drop', async (e) => {
            e.preventDefault();
            const files = e.dataTransfer.files;
            if (files.length > 0) {
                const file = files[0];
                const reader = new FileReader();
                reader.onload = (e) => {
                    window.ide.editor.setValue(e.target.result);
                    window.ide.currentFile = file.name;
                };
                reader.readAsText(file);
            }
        });
    }

    async loadFiles(offset = 0) {
        try {
            const url = `http://localhost:8080/api/files?path=${encodeURIComponent(this.workspaceRoot)}&limit=${this.pageSize}&offset=${offset}`;
            const response = await fetch(url);
            const data = await response.json();
            
            if (offset === 0) {
                this.renderFileTree(data.files);
            } else {
                this.appendFileTree(data.files);
            }
            
            if (data.hasMore) {
                this.addLoadMoreButton(offset + this.pageSize);
            }
        } catch (error) {
            console.error('Failed to load files:', error);
            this.fileTree.innerHTML = '<div class="error">Failed to load files. Is server running?</div>';
        }
    }

    renderFileTree(files, container = this.fileTree, level = 0) {
        if (level === 0) container.innerHTML = '';
        if (!files || !Array.isArray(files)) return;
        files.forEach(file => {
            const item = document.createElement('div');
            item.className = 'file-item';
            item.style.paddingLeft = (level * 16) + 'px';
            item.style.display = 'flex';
            item.style.alignItems = 'center';
            item.style.transition = 'background 0.2s';
            item.onmouseover = () => item.style.background = '#333';
            item.onmouseout = () => item.style.background = 'transparent';
            if (file.isDirectory || file.type === 'directory') {
                item.innerHTML = `<span class='folder-icon' style='margin-right:6px;'>📁</span><span class='file-name'>${file.name}</span>`;
                item.onclick = () => this.toggleFolder(file, item);
                item.title = 'Open folder';
            } else {
                const filePath = file.path || `${this.workspaceRoot}${file.name}`;
                item.innerHTML = `<span class='file-icon' style='margin-right:6px;'>${this.getFileIcon(file.name)}</span><span class='file-name'>${file.name}</span>`;
                item.onclick = () => window.ide.openFile(filePath);
                item.title = filePath;
                item.oncontextmenu = (e) => {
                    e.preventDefault();
                    this.showContextMenu(e, { ...file, path: filePath });
                };
            }
            container.appendChild(item);
        });
    }

    getFileIcon(filename) {
        const ext = filename.split('.').pop().toLowerCase();
        const icons = {
            'js': '🟨',
            'ts': '🟦',
            'py': '🐍',
            'html': '🌐',
            'css': '🎨',
            'json': '🔢',
            'md': '📄',
            'java': '☕',
            'cpp': '💠',
            'c': '🔷',
            'go': '🐹',
            'rs': '🦀',
            'sh': '💻',
            'ps1': '🪟',
            'sql': '🗄️',
            'php': '🐘',
            'rb': '💎',
            'pl': '🦑',
            'yaml': '📄',
            'toml': '📄',
            'dockerfile': '🐳',
            'asm': '⚙️',
            's': '⚙️',
            'nasm': '⚙️',
            'tasm': '⚙️',
            'fasm': '⚙️',
            'yasm': '⚙️'
        };
        return icons[ext] || '📄';
    }

    async toggleFolder(folder, element) {
        const existing = element.nextElementSibling;
        if (existing && existing.classList.contains('folder-contents')) {
            existing.remove();
            return;
        }

        if (this.cache.has(folder.path)) {
            const container = document.createElement('div');
            container.className = 'folder-contents';
            this.renderFileTree(this.cache.get(folder.path), container, element.style.paddingLeft.replace('px', '') / 16 + 1);
            element.after(container);
            return;
        }

        try {
            const url = `http://localhost:8080/api/files?path=${encodeURIComponent(folder.path)}&limit=${this.pageSize}`;
            const response = await fetch(url);
            
            if (!response.ok) {
                console.warn(`Cannot access folder: ${folder.name}`);
                const container = document.createElement('div');
                container.className = 'folder-contents';
                container.style.paddingLeft = (parseInt(element.style.paddingLeft) + 16) + 'px';
                container.style.color = '#888';
                container.textContent = 'Access denied';
                element.after(container);
                return;
            }
            
            const data = await response.json();
            
            this.cache.set(folder.path, data.files);
            
            const container = document.createElement('div');
            container.className = 'folder-contents';
            this.renderFileTree(data.files, container, element.style.paddingLeft.replace('px', '') / 16 + 1);
            element.after(container);
        } catch (error) {
            console.error('Failed to load folder:', error);
        }
    }

    appendFileTree(files) {
        const loadMore = this.fileTree.querySelector('.load-more');
        if (loadMore) loadMore.remove();
        
        files.forEach(file => {
            const item = document.createElement('div');
            item.className = 'file-item';
            
            if (file.type === 'directory') {
                item.innerHTML = `<span class="folder-icon">📁</span><span class="file-name">${file.name}</span>`;
                item.onclick = () => this.toggleFolder(file, item);
            } else {
                item.innerHTML = `<span class="file-icon">${this.getFileIcon(file.name)}</span><span class="file-name">${file.name}</span>`;
                item.onclick = () => window.ide.openFile(file.path);
            }
            
            this.fileTree.appendChild(item);
        });
    }

    addLoadMoreButton(nextOffset) {
        const existing = this.fileTree.querySelector('.load-more');
        if (existing) existing.remove();
        
        const btn = document.createElement('button');
        btn.className = 'load-more';
        btn.textContent = 'Load More...';
        btn.onclick = () => this.loadFiles(nextOffset);
        this.fileTree.appendChild(btn);
    }
    
    showContextMenu(e, file) {
        const menu = document.createElement('div');
        menu.style.cssText = 'position:fixed;background:#2d2d2d;border:1px solid #444;padding:4px;z-index:10000;';
        menu.style.left = e.clientX + 'px';
        menu.style.top = e.clientY + 'px';
        menu.innerHTML = `
            <div style="padding:4px 8px;cursor:pointer;color:#fff;" onmouseover="this.style.background='#444'" onmouseout="this.style.background='transparent'">Delete</div>
            <div style="padding:4px 8px;cursor:pointer;color:#fff;" onmouseover="this.style.background='#444'" onmouseout="this.style.background='transparent'">Rename</div>
        `;
        menu.children[0].onclick = () => { this.deleteFile(file); menu.remove(); };
        menu.children[1].onclick = () => { this.renameFile(file); menu.remove(); };
        document.body.appendChild(menu);
        setTimeout(() => document.addEventListener('click', () => menu.remove(), { once: true }), 100);
    }
    
    async deleteFile(file) {
        if (!confirm(`Delete ${file.name}?`)) return;
        try {
            await fetch('http://localhost:8080/api/files', {
                method: 'DELETE',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ filePath: file.path })
            });
            this.loadFiles();
        } catch (error) {
            alert('Delete failed: ' + error.message);
        }
    }
    
    async renameFile(file) {
        const newName = prompt('New name:', file.name);
        if (!newName || newName === file.name) return;
        
        const dir = file.path.substring(0, file.path.lastIndexOf('\\'));
        const newPath = dir + '\\' + newName;
        
        try {
            await fetch('http://localhost:8080/api/files/rename', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ oldPath: file.path, newPath })
            });
            
            if (window.ide.currentFile === file.path) {
                window.ide.currentFile = newPath;
                const tab = window.ide.tabs.find(t => t.path === file.path);
                if (tab) {
                    tab.path = newPath;
                    tab.name = newName;
                    window.ide.renderTabs();
                }
            }
            
            this.loadFiles();
        } catch (error) {
            alert('Rename failed: ' + error.message);
        }
    }
}
