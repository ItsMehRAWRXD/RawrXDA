/**
 * BigDaddyG IDE - Enhanced File Explorer
 * Professional file browser with open editors tracking, context menus, and full drive access
 * Matches/exceeds VS Code, Visual Studio 2022, Cursor, and JetBrains quality
 */

(function() {
'use strict';

class EnhancedFileExplorer {
    constructor() {
        this.currentPath = null;
        this.drives = [];
        this.expandedFolders = new Set();
        this.openEditors = new Map(); // Track all open files
        this.watchedFiles = new Set();
        this.fileIcons = this.initFileIcons();
        this.init();
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    async init() {
        console.log('[Explorer] 📁 Initializing enhanced file explorer...');
        
        // Create explorer panel
        this.createExplorerPanel();
        
        // Load all drives
        await this.loadDrives();
        
        // Watch for USB changes
        if (window.electron && window.electron.onDrivesChanged) {
            window.electron.onDrivesChanged((drives) => {
                console.log('[Explorer] 🔄 Drives changed:', drives);
                this.loadDrives();
            });
        }
        
        // Listen for file open/close events
        this.setupEditorListeners();
        
        console.log('[Explorer] ✅ Enhanced file explorer ready');
    }
    
    createExplorerPanel() {
        const sidebar = document.getElementById('sidebar');
        if (!sidebar) return;
        
        // Clear existing content
        sidebar.innerHTML = '';
        
        // Create header
        const header = document.createElement('div');
        header.style.cssText = `
            padding: 12px 16px;
            background: rgba(0, 0, 0, 0.3);
            border-bottom: 1px solid rgba(0, 212, 255, 0.2);
            display: flex;
            justify-content: space-between;
            align-items: center;
        `;
        
        header.innerHTML = `
            <h3 style="margin: 0; font-size: 11px; text-transform: uppercase; color: var(--cyan); font-weight: 600; letter-spacing: 0.5px;">Explorer</h3>
            <div style="display: flex; gap: 8px;">
                <button onclick="window.enhancedFileExplorer.collapseAll()" style="background: none; border: 1px solid var(--cyan); color: var(--cyan); padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 11px;" title="Collapse All">▲</button>
                <button onclick="window.enhancedFileExplorer.refresh()" style="background: none; border: 1px solid var(--cyan); color: var(--cyan); padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 11px;" title="Refresh">🔄</button>
            </div>
        `;
        
        sidebar.appendChild(header);
        
        // Create sections container
        const container = document.createElement('div');
        container.style.cssText = `
            flex: 1;
            overflow-y: auto;
            overflow-x: hidden;
        `;
        
        // Open Editors section
        const openEditorsSection = document.createElement('div');
        openEditorsSection.id = 'open-editors-section';
        openEditorsSection.style.cssText = `
            border-bottom: 1px solid rgba(0, 212, 255, 0.1);
        `;
        
        const openEditorsHeader = document.createElement('div');
        openEditorsHeader.style.cssText = `
            padding: 8px 16px;
            background: rgba(0, 0, 0, 0.2);
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 11px;
            text-transform: uppercase;
            font-weight: 600;
            color: #888;
            user-select: none;
        `;
        
        openEditorsHeader.innerHTML = `
            <span>▼ Open Editors</span>
            <span id="open-editors-count" style="background: rgba(0, 212, 255, 0.2); padding: 2px 6px; border-radius: 10px; font-size: 10px;">0</span>
        `;
        
        openEditorsHeader.onclick = () => this.toggleSection('open-editors');
        
        const openEditorsList = document.createElement('div');
        openEditorsList.id = 'open-editors-list';
        openEditorsList.style.cssText = `
            display: block;
        `;
        
        openEditorsSection.appendChild(openEditorsHeader);
        openEditorsSection.appendChild(openEditorsList);
        
        // Workspace section
        const workspaceSection = document.createElement('div');
        workspaceSection.id = 'workspace-section';
        
        const workspaceHeader = document.createElement('div');
        workspaceHeader.style.cssText = `
            padding: 8px 16px;
            background: rgba(0, 0, 0, 0.2);
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 11px;
            text-transform: uppercase;
            font-weight: 600;
            color: #888;
            user-select: none;
        `;
        
        workspaceHeader.innerHTML = `
            <span>▼ File System</span>
            <button onclick="event.stopPropagation(); window.enhancedFileExplorer.addToWorkspace()" style="background: none; border: 1px solid var(--green); color: var(--green); padding: 2px 6px; border-radius: 4px; cursor: pointer; font-size: 10px;">+ Add Folder</button>
        `;
        
        workspaceHeader.onclick = () => this.toggleSection('workspace');
        
        const workspaceList = document.createElement('div');
        workspaceList.id = 'workspace-list';
        workspaceList.style.cssText = `
            display: block;
        `;
        
        workspaceSection.appendChild(workspaceHeader);
        workspaceSection.appendChild(workspaceList);
        
        container.appendChild(openEditorsSection);
        container.appendChild(workspaceSection);
        sidebar.appendChild(container);
    }
    
    // ========================================================================
    // FILE ICONS
    // ========================================================================
    
    initFileIcons() {
        return {
            // Programming languages
            'js': '📄', 'ts': '📘', 'jsx': '⚛️', 'tsx': '⚛️',
            'py': '🐍', 'java': '☕', 'c': '©️', 'cpp': '➕', 'cs': '#️⃣',
            'go': '🐹', 'rs': '🦀', 'rb': '💎', 'php': '🐘', 'swift': '🦅',
            
            // Web
            'html': '🌐', 'css': '🎨', 'scss': '🎨', 'sass': '🎨',
            'json': '📋', 'xml': '📰', 'yaml': '⚙️', 'yml': '⚙️',
            
            // Data & Config
            'md': '📝', 'txt': '📄', 'csv': '📊', 'sql': '🗄️',
            'env': '🔐', 'ini': '⚙️', 'conf': '⚙️', 'config': '⚙️',
            
            // Shell & Scripts
            'sh': '🐚', 'bash': '🐚', 'ps1': '💻', 'bat': '⚙️', 'cmd': '⚙️',
            
            // Images
            'png': '🖼️', 'jpg': '🖼️', 'jpeg': '🖼️', 'gif': '🖼️', 'svg': '🎨',
            'ico': '🖼️', 'webp': '🖼️',
            
            // Documents
            'pdf': '📕', 'doc': '📘', 'docx': '📘', 'xls': '📗', 'xlsx': '📗',
            
            // Archives
            'zip': '📦', 'rar': '📦', 'tar': '📦', 'gz': '📦', '7z': '📦',
            
            // Others
            'exe': '⚙️', 'dll': '🔧', 'so': '🔧', 'app': '📱',
            'log': '📜', 'git': '🔀', 'gitignore': '🚫',
            
            // Folders
            'folder': '📁', 'folder-open': '📂'
        };
    }
    
    getFileIcon(filename, isDirectory = false) {
        if (isDirectory) {
            return this.fileIcons['folder'];
        }
        
        const ext = filename.split('.').pop().toLowerCase();
        return this.fileIcons[ext] || '📄';
    }
    
    getFileLanguage(filename) {
        const ext = filename.split('.').pop().toLowerCase();
        const langMap = {
            'js': 'JavaScript', 'ts': 'TypeScript', 'jsx': 'React', 'tsx': 'React/TS',
            'py': 'Python', 'java': 'Java', 'cpp': 'C++', 'c': 'C', 'cs': 'C#',
            'go': 'Go', 'rs': 'Rust', 'rb': 'Ruby', 'php': 'PHP',
            'html': 'HTML', 'css': 'CSS', 'scss': 'SCSS',
            'json': 'JSON', 'xml': 'XML', 'yaml': 'YAML', 'yml': 'YAML',
            'md': 'Markdown', 'txt': 'Text',
            'sh': 'Shell', 'ps1': 'PowerShell', 'bat': 'Batch'
        };
        return langMap[ext] || 'Unknown';
    }
    
    // ========================================================================
    // OPEN EDITORS TRACKING
    // ========================================================================
    
    setupEditorListeners() {
        // Listen for file open events
        window.addEventListener('file-opened', (event) => {
            this.addOpenEditor(event.detail);
        });
        
        // Listen for file close events
        window.addEventListener('file-closed', (event) => {
            this.removeOpenEditor(event.detail.path);
        });
        
        // Listen for file save events
        window.addEventListener('file-saved', (event) => {
            this.updateOpenEditor(event.detail.path, { modified: false });
        });
        
        // Listen for file modify events
        window.addEventListener('file-modified', (event) => {
            this.updateOpenEditor(event.detail.path, { modified: true });
        });
    }
    
    addOpenEditor(fileInfo) {
        const { path, filename, language, content } = fileInfo;
        
        this.openEditors.set(path, {
            path,
            filename,
            language: language || this.getFileLanguage(filename),
            modified: false,
            openedAt: new Date(),
            content
        });
        
        this.renderOpenEditors();
        console.log('[Explorer] 📂 Added open editor:', filename);
    }
    
    removeOpenEditor(path) {
        this.openEditors.delete(path);
        this.renderOpenEditors();
        console.log('[Explorer] 📂 Removed open editor:', path);
    }
    
    updateOpenEditor(path, updates) {
        const editor = this.openEditors.get(path);
        if (editor) {
            Object.assign(editor, updates);
            this.renderOpenEditors();
        }
    }
    
    renderOpenEditors() {
        const list = document.getElementById('open-editors-list');
        const count = document.getElementById('open-editors-count');
        
        if (!list || !count) return;
        
        count.textContent = this.openEditors.size;
        
        if (this.openEditors.size === 0) {
            list.innerHTML = '<div style="padding: 12px 16px; color: #666; font-size: 11px; font-style: italic;">No editors open</div>';
            return;
        }
        
        list.innerHTML = '';
        
        this.openEditors.forEach((editor, path) => {
            const item = document.createElement('div');
            item.className = 'open-editor-item';
            item.style.cssText = `
                padding: 6px 16px 6px 24px;
                cursor: pointer;
                display: flex;
                align-items: center;
                gap: 8px;
                font-size: 12px;
                transition: background 0.2s;
                position: relative;
            `;
            
            item.innerHTML = `
                <span style="font-size: 14px;">${this.getFileIcon(editor.filename)}</span>
                <div style="flex: 1; min-width: 0;">
                    <div style="color: ${editor.modified ? 'var(--orange)' : '#fff'}; font-weight: ${editor.modified ? '600' : 'normal'}; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
                        ${editor.modified ? '● ' : ''}${editor.filename}
                    </div>
                    <div style="color: #666; font-size: 10px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${path}">
                        ${path}
                    </div>
                </div>
                <span style="font-size: 10px; color: #888; background: rgba(0, 212, 255, 0.1); padding: 2px 6px; border-radius: 4px;">${editor.language}</span>
            `;
            
            // Click to focus editor
            item.onclick = (e) => {
                e.stopPropagation();
                this.focusEditor(path);
            };
            
            // Right-click context menu
            item.oncontextmenu = (e) => {
                e.preventDefault();
                this.showEditorContextMenu(e, path, editor);
            };
            
            // Hover effects
            item.onmouseenter = () => {
                item.style.background = 'rgba(0, 212, 255, 0.1)';
            };
            
            item.onmouseleave = () => {
                item.style.background = 'transparent';
            };
            
            list.appendChild(item);
        });
    }
    
    focusEditor(path) {
        console.log('[Explorer] 🎯 Focusing editor:', path);
        
        // Trigger event to switch to this file's tab
        window.dispatchEvent(new CustomEvent('focus-editor', { 
            detail: { path } 
        }));
    }
    
    // ========================================================================
    // CONTEXT MENUS
    // ========================================================================
    
    showEditorContextMenu(event, path, editor) {
        const items = [
            {
                icon: '👁️',
                label: 'Reveal in File Explorer',
                action: () => this.revealInExplorer(path)
            },
            {
                icon: '💾',
                label: editor.modified ? 'Save' : 'Save (Up to date)',
                action: () => this.saveFile(path),
                disabled: !editor.modified
            },
            {
                icon: '💾',
                label: 'Save As...',
                action: () => this.saveFileAs(path)
            },
            'separator',
            {
                icon: '📋',
                label: 'Copy Path',
                action: () => this.copyPath(path)
            },
            {
                icon: '📋',
                label: 'Copy Relative Path',
                action: () => this.copyRelativePath(path)
            },
            'separator',
            {
                icon: '🔄',
                label: 'Rename',
                action: () => this.renameFile(path)
            },
            {
                icon: '🗑️',
                label: 'Delete',
                action: () => this.deleteFile(path),
                danger: true
            },
            'separator',
            {
                icon: '✖️',
                label: 'Close Editor',
                action: () => this.closeEditor(path)
            },
            {
                icon: '✖️',
                label: 'Close All Editors',
                action: () => this.closeAllEditors(),
                danger: true
            }
        ];
        
        if (window.showContextMenu) {
            window.showContextMenu(event.clientX, event.clientY, items);
        }
    }
    
    showFileContextMenu(event, itemPath, isDirectory, filename) {
        const items = [
            {
                icon: '📂',
                label: 'Open',
                action: () => {
                    if (isDirectory) {
                        this.expandFolder(itemPath);
                    } else {
                        this.openFile(itemPath, filename);
                    }
                }
            },
            {
                icon: '🪟',
                label: 'Open in System Explorer',
                action: () => this.openInSystemExplorer(itemPath)
            },
            'separator',
            {
                icon: '📄',
                label: 'New File',
                action: () => this.createNewFile(isDirectory ? itemPath : this.getParentPath(itemPath))
            },
            {
                icon: '📁',
                label: 'New Folder',
                action: () => this.createNewFolder(isDirectory ? itemPath : this.getParentPath(itemPath))
            },
            'separator',
            {
                icon: '📋',
                label: 'Copy',
                action: () => this.copyItem(itemPath)
            },
            {
                icon: '✂️',
                label: 'Cut',
                action: () => this.cutItem(itemPath)
            },
            {
                icon: '📋',
                label: 'Paste',
                action: () => this.pasteItem(itemPath),
                disabled: !this.clipboard
            },
            'separator',
            {
                icon: '🔄',
                label: 'Rename',
                action: () => this.renameItem(itemPath, filename)
            },
            {
                icon: '🗑️',
                label: 'Delete',
                action: () => this.deleteItem(itemPath, isDirectory),
                danger: true
            },
            'separator',
            {
                icon: '📋',
                label: 'Copy Path',
                action: () => this.copyPath(itemPath)
            }
        ];
        
        if (window.showContextMenu) {
            window.showContextMenu(event.clientX, event.clientY, items);
        }
    }
    
    // ========================================================================
    // FILE OPERATIONS
    // ========================================================================
    
    async saveFile(path) {
        console.log('[Explorer] 💾 Saving file:', path);
        
        window.dispatchEvent(new CustomEvent('save-file', { 
            detail: { path } 
        }));
    }
    
    async saveFileAs(path) {
        console.log('[Explorer] 💾 Save file as:', path);
        
        window.dispatchEvent(new CustomEvent('save-file-as', { 
            detail: { path } 
        }));
    }
    
    async copyPath(path) {
        try {
            await navigator.clipboard.writeText(path);
            if (window.showNotification) {
                window.showNotification('✅ Path Copied', path, 'success', 2000);
            }
        } catch (error) {
            console.error('[Explorer] Failed to copy path:', error);
        }
    }
    
    async copyRelativePath(path) {
        // Calculate relative path from current workspace
        let relativePath = path;
        
        if (this.currentPath) {
            // Remove current path prefix to get relative path
            if (path.startsWith(this.currentPath)) {
                relativePath = path.substring(this.currentPath.length);
                if (relativePath.startsWith('\\') || relativePath.startsWith('/')) {
                    relativePath = relativePath.substring(1);
                }
            }
        }
        
        await this.copyPath(relativePath);
    }
    
    async renameFile(path) {
        const currentName = path.split('\\').pop();
        const newName = prompt('Enter new name:', currentName);
        
        if (!newName || newName === currentName) return;
        
        const newPath = path.substring(0, path.lastIndexOf('\\')) + '\\' + newName;
        
        try {
            const result = await window.electron.moveItem(path, newPath);
            
            if (result.success) {
                console.log('[Explorer] ✅ File renamed');
                
                // Update open editor if exists
                if (this.openEditors.has(path)) {
                    const editor = this.openEditors.get(path);
                    this.openEditors.delete(path);
                    editor.path = newPath;
                    editor.filename = newName;
                    this.openEditors.set(newPath, editor);
                    this.renderOpenEditors();
                }
                
                this.refresh();
            } else {
                alert('Failed to rename: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Rename failed:', error);
            alert('Error: ' + error.message);
        }
    }
    
    async deleteFile(path) {
        if (!confirm(`Delete ${path}?`)) return;
        
        try {
            const result = await window.electron.deleteItem(path, false);
            
            if (result.success) {
                console.log('[Explorer] ✅ File deleted');
                
                // Close editor if open
                if (this.openEditors.has(path)) {
                    this.closeEditor(path);
                }
                
                this.refresh();
            } else {
                alert('Failed to delete: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Delete failed:', error);
            alert('Error: ' + error.message);
        }
    }
    
    closeEditor(path) {
        console.log('[Explorer] ✖️ Closing editor:', path);
        
        window.dispatchEvent(new CustomEvent('close-editor', { 
            detail: { path } 
        }));
        
        this.removeOpenEditor(path);
    }
    
    closeAllEditors() {
        if (!confirm('Close all editors?')) return;
        
        this.openEditors.forEach((editor, path) => {
            window.dispatchEvent(new CustomEvent('close-editor', { 
                detail: { path } 
            }));
        });
        
        this.openEditors.clear();
        this.renderOpenEditors();
    }
    
    async revealInExplorer(path) {
        try {
            await window.electron.openInExplorer(path);
        } catch (error) {
            console.error('[Explorer] Failed to reveal in explorer:', error);
        }
    }
    
    // ========================================================================
    // WORKSPACE MANAGEMENT
    // ========================================================================
    
    async loadDrives() {
        try {
            const result = await window.electron.listDrives();
            
            if (result.success) {
                this.drives = result.drives;
                console.log(`[Explorer] 💾 Loaded ${this.drives.length} drives`);
                this.renderWorkspace();
            } else {
                console.error('[Explorer] ❌ Failed to load drives:', result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading drives:', error);
        }
    }
    
    renderWorkspace() {
        const list = document.getElementById('workspace-list');
        if (!list) return;
        
        list.innerHTML = '';
        
        this.drives.forEach(drive => {
            const driveItem = this.createDriveItem(drive);
            list.appendChild(driveItem);
        });
    }
    
    createDriveItem(drive) {
        const item = document.createElement('div');
        item.className = 'drive-item';
        item.style.cssText = `
            padding: 8px 16px;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 10px;
            font-size: 12px;
            transition: background 0.2s;
            border-bottom: 1px solid rgba(0, 212, 255, 0.05);
        `;
        
        const icon = drive.icon || '💾';
        const name = drive.name || drive.path;
        const type = drive.type || 'Drive';
        
        item.innerHTML = `
            <span style="font-size: 18px;">${icon}</span>
            <div style="flex: 1; min-width: 0;">
                <div style="color: #fff; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
                    ${name}
                </div>
                <div style="color: #666; font-size: 10px;">
                    ${type} • ${drive.path}
                </div>
            </div>
            <span style="font-size: 16px; color: #888;">▶</span>
        `;
        
        item.onclick = () => this.browseDrive(drive);
        
        item.onmouseenter = () => {
            item.style.background = 'rgba(0, 212, 255, 0.1)';
        };
        
        item.onmouseleave = () => {
            item.style.background = 'transparent';
        };
        
        return item;
    }
    
    async browseDrive(drive) {
        console.log('[Explorer] 📂 Browsing drive:', drive.path);
        this.currentPath = drive.path;
        await this.loadDirectory(drive.path);
    }
    
    async loadDirectory(dirPath) {
        // Load directory contents and render tree
        console.log('[Explorer] 📂 Loading directory:', dirPath);
        
        try {
            const result = await window.electron.readDir(dirPath);
            
            if (result.success) {
                this.renderDirectoryTree(dirPath, result.files);
            } else {
                console.error('[Explorer] ❌ Failed to read directory:', result.error);
                if (window.showNotification) {
                    window.showNotification('❌ Error', `Cannot access ${dirPath}: ${result.error}`, 'error', 3000);
                }
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading directory:', error);
            if (window.showNotification) {
                window.showNotification('❌ Error', error.message, 'error', 3000);
            }
        }
    }
    
    renderDirectoryTree(dirPath, files) {
        const list = document.getElementById('workspace-list');
        if (!list) return;
        
        // Create a container for this directory
        const dirContainer = document.createElement('div');
        dirContainer.style.cssText = `
            border-left: 2px solid rgba(0, 212, 255, 0.2);
            margin-left: 16px;
        `;
        
        // Sort files: directories first, then files alphabetically
        files.sort((a, b) => {
            if (a.isDirectory && !b.isDirectory) return -1;
            if (!a.isDirectory && b.isDirectory) return 1;
            return a.name.localeCompare(b.name);
        });
        
        files.forEach(file => {
            const itemPath = `${dirPath}\\${file.name}`;
            const item = this.createFileTreeItem(file, itemPath);
            dirContainer.appendChild(item);
        });
        
        list.appendChild(dirContainer);
    }
    
    createFileTreeItem(file, fullPath) {
        const item = document.createElement('div');
        item.className = 'file-tree-item';
        item.style.cssText = `
            padding: 4px 8px 4px 24px;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 12px;
            transition: background 0.2s;
            position: relative;
        `;
        
        const isExpanded = this.expandedFolders.has(fullPath);
        const expandIcon = file.isDirectory ? (isExpanded ? '▼' : '▶') : '';
        
        item.innerHTML = `
            ${expandIcon ? `<span style="width: 12px; text-align: center; color: #888;">${expandIcon}</span>` : '<span style="width: 12px;"></span>'}
            <span style="font-size: 14px;">${this.getFileIcon(file.name, file.isDirectory)}</span>
            <span style="flex: 1; color: #fff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;">
                ${file.name}
            </span>
        `;
        
        // Click handler
        item.onclick = async (e) => {
            e.stopPropagation();
            
            if (file.isDirectory) {
                // Toggle expansion
                if (isExpanded) {
                    this.expandedFolders.delete(fullPath);
                } else {
                    this.expandedFolders.add(fullPath);
                    await this.loadDirectory(fullPath);
                }
                this.refresh();
            } else {
                // Open file
                await this.openFile(fullPath, file.name);
            }
        };
        
        // Context menu
        item.oncontextmenu = (e) => {
            e.preventDefault();
            e.stopPropagation();
            this.showFileContextMenu(e, fullPath, file.isDirectory, file.name);
        };
        
        // Hover effects
        item.onmouseenter = () => {
            item.style.background = 'rgba(0, 212, 255, 0.1)';
        };
        
        item.onmouseleave = () => {
            item.style.background = 'transparent';
        };
        
        return item;
    }
    
    async openFile(filePath, filename) {
        try {
            console.log('[Explorer] 📄 Opening file:', filePath);
            
            const result = await window.electron.readFile(filePath);
            
            if (result.success) {
                // Trigger file-opened event
                window.dispatchEvent(new CustomEvent('file-opened', {
                    detail: {
                        path: filePath,
                        filename: filename,
                        language: this.getFileLanguage(filename),
                        content: result.content
                    }
                }));
                
                console.log('[Explorer] ✅ File opened:', filename);
            } else {
                alert(`Cannot open ${filename}: ${result.error}`);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error opening file:', error);
            alert(`Error opening file: ${error.message}`);
        }
    }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    toggleSection(sectionName) {
        const list = document.getElementById(`${sectionName}-list`);
        if (list) {
            const isVisible = list.style.display !== 'none';
            list.style.display = isVisible ? 'none' : 'block';
        }
    }
    
    collapseAll() {
        this.expandedFolders.clear();
        this.renderWorkspace();
    }
    
    async refresh() {
        await this.loadDrives();
        this.renderOpenEditors();
    }
    
    async addToWorkspace() {
        // Open folder dialog and add to workspace
        console.log('[Explorer] Adding folder to workspace...');
        
        try {
            if (window.electron && window.electron.openFolderDialog) {
                const result = await window.electron.openFolderDialog();
                
                if (result && result.filePaths && result.filePaths.length > 0) {
                    const folderPath = result.filePaths[0];
                    
                    // Add to workspace (create workspace folder entry)
                    const folderName = folderPath.split('\\').pop() || folderPath;
                    
                    // Create a virtual drive entry for this workspace folder
                    this.drives.push({
                        path: folderPath,
                        name: folderName,
                        type: 'Workspace Folder',
                        icon: '📁',
                        isWorkspace: true
                    });
                    
                    this.renderWorkspace();
                    
                    if (window.showNotification) {
                        window.showNotification(
                            '✅ Folder Added',
                            `${folderName} added to workspace`,
                            'success',
                            3000
                        );
                    }
                    
                    console.log('[Explorer] ✅ Added to workspace:', folderPath);
                }
            } else {
                console.error('[Explorer] ❌ openFolderDialog not available');
                alert('Folder dialog not available. Please check electron bridge.');
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error adding to workspace:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    getParentPath(path) {
        return path.substring(0, path.lastIndexOf('\\'));
    }
    
    // ========================================================================
    // FILE/FOLDER CREATION AND CLIPBOARD OPERATIONS
    // ========================================================================
    
    async createNewFile(parentPath) {
        const filename = prompt('Enter file name:', 'newfile.txt');
        if (!filename) return;
        
        const filePath = `${parentPath}\\${filename}`;
        
        try {
            const result = await window.electron.writeFile(filePath, '');
            
            if (result.success) {
                console.log('[Explorer] ✅ File created:', filePath);
                this.refresh();
                
                // Open the new file
                await this.openFile(filePath, filename);
            } else {
                alert('Failed to create file: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Create file error:', error);
            alert('Error: ' + error.message);
        }
    }
    
    async createNewFolder(parentPath) {
        const foldername = prompt('Enter folder name:', 'New Folder');
        if (!foldername) return;
        
        const folderPath = `${parentPath}\\${foldername}`;
        
        try {
            const result = await window.electron.createDirectory(folderPath);
            
            if (result.success) {
                console.log('[Explorer] ✅ Folder created:', folderPath);
                this.refresh();
                
                if (window.showNotification) {
                    window.showNotification('✅ Created', `Folder "${foldername}" created`, 'success', 2000);
                }
            } else {
                alert('Failed to create folder: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Create folder error:', error);
            alert('Error: ' + error.message);
        }
    }
    
    copyItem(itemPath) {
        this.clipboard = {
            path: itemPath,
            operation: 'copy'
        };
        
        if (window.showNotification) {
            const itemName = itemPath.split('\\').pop();
            window.showNotification('📋 Copied', `"${itemName}" copied to clipboard`, 'info', 2000);
        }
        
        console.log('[Explorer] 📋 Copied to clipboard:', itemPath);
    }
    
    cutItem(itemPath) {
        this.clipboard = {
            path: itemPath,
            operation: 'cut'
        };
        
        if (window.showNotification) {
            const itemName = itemPath.split('\\').pop();
            window.showNotification('✂️ Cut', `"${itemName}" cut to clipboard`, 'info', 2000);
        }
        
        console.log('[Explorer] ✂️ Cut to clipboard:', itemPath);
    }
    
    async pasteItem(targetPath) {
        if (!this.clipboard) {
            console.warn('[Explorer] ⚠️ Nothing to paste');
            return;
        }
        
        const { path: sourcePath, operation } = this.clipboard;
        const itemName = sourcePath.split('\\').pop();
        const destPath = `${targetPath}\\${itemName}`;
        
        try {
            if (operation === 'copy') {
                // Copy operation
                const result = await window.electron.copyItem(sourcePath, destPath);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Pasted (copy):', destPath);
                    this.refresh();
                    
                    if (window.showNotification) {
                        window.showNotification('✅ Pasted', `"${itemName}" pasted`, 'success', 2000);
                    }
                } else {
                    alert('Failed to paste: ' + result.error);
                }
            } else if (operation === 'cut') {
                // Move operation
                const result = await window.electron.moveItem(sourcePath, destPath);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Pasted (move):', destPath);
                    this.clipboard = null; // Clear clipboard after cut operation
                    this.refresh();
                    
                    if (window.showNotification) {
                        window.showNotification('✅ Moved', `"${itemName}" moved`, 'success', 2000);
                    }
                } else {
                    alert('Failed to move: ' + result.error);
                }
            }
        } catch (error) {
            console.error('[Explorer] ❌ Paste error:', error);
            alert('Error: ' + error.message);
        }
    }
    
    async renameItem(itemPath, currentName) {
        const newName = prompt('Enter new name:', currentName);
        
        if (!newName || newName === currentName) return;
        
        const parentPath = this.getParentPath(itemPath);
        const newPath = `${parentPath}\\${newName}`;
        
        try {
            const result = await window.electron.moveItem(itemPath, newPath);
            
            if (result.success) {
                console.log('[Explorer] ✅ Renamed:', newPath);
                this.refresh();
                
                if (window.showNotification) {
                    window.showNotification('✅ Renamed', `"${currentName}" → "${newName}"`, 'success', 2000);
                }
            } else {
                alert('Failed to rename: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Rename error:', error);
            alert('Error: ' + error.message);
        }
    }
    
    async deleteItem(itemPath, isDirectory) {
        const itemName = itemPath.split('\\').pop();
        
        if (!confirm(`Delete "${itemName}"?${isDirectory ? ' This will delete all contents.' : ''}`)) {
            return;
        }
        
        try {
            const result = await window.electron.deleteItem(itemPath, isDirectory);
            
            if (result.success) {
                console.log('[Explorer] ✅ Deleted:', itemPath);
                this.refresh();
                
                if (window.showNotification) {
                    window.showNotification('✅ Deleted', `"${itemName}" deleted`, 'success', 2000);
                }
            } else {
                alert('Failed to delete: ' + result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Delete error:', error);
            alert('Error: ' + error.message);
        }
    }
    
    async openInSystemExplorer(itemPath) {
        try {
            await window.electron.openInExplorer(itemPath);
            console.log('[Explorer] 🪟 Opened in system explorer:', itemPath);
        } catch (error) {
            console.error('[Explorer] ❌ Failed to open in explorer:', error);
        }
    }
}

// ========================================================================
// GLOBAL EXPOSURE
// ========================================================================

window.enhancedFileExplorer = new EnhancedFileExplorer();

console.log('[Explorer] 🎨 Enhanced file explorer loaded');

})();
