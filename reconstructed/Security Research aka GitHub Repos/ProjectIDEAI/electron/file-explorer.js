/**
 * BigDaddyG IDE - File Explorer
 * Browse ALL drives (C:, D:, USB, external), load entire projects
 * Cross-platform: Windows, macOS, Linux
 */

(function() {
'use strict';

class FileExplorer {
    constructor() {
        this.currentPath = null;
        this.drives = [];
        this.expandedFolders = new Set();
        this.init();
    }
    
    async init() {
        console.log('[Explorer] 📁 Initializing file explorer...');
        
        // Load all drives
        await this.loadDrives();
        
        // Watch for USB changes
        if (window.electron && window.electron.onDrivesChanged) {
            window.electron.onDrivesChanged((drives) => {
                console.log('[Explorer] 🔄 Drives changed:', drives);
                this.loadDrives();
            });
        }
        
        console.log('[Explorer] ✅ File explorer ready');
    }
    
    async loadDrives() {
        try {
            const result = await window.electron.listDrives();
            
            if (result.success) {
                this.drives = result.drives;
                console.log(`[Explorer] 💾 Loaded ${this.drives.length} drives`);
                this.renderDrives();
            } else {
                console.error('[Explorer] ❌ Failed to load drives:', result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading drives:', error);
        }
    }
    
    async loadDrivesToCenter() {
        try {
            const result = await window.electron.listDrives();
            
            if (result.success && result.drives) {
                this.drives = result.drives;
                console.log(`[Explorer] 💾 Loaded ${this.drives.length} drives to center`);
                this.renderDrivesToCenter();
            } else {
                console.error('[Explorer] ❌ Failed to load drives:', result.error);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading drives to center:', error);
        }
    }
    
    renderDrivesToCenter() {
        const container = document.getElementById('center-explorer-content');
        if (!container) {
            console.error('[Explorer] ❌ Center explorer container not found');
            return;
        }
        
        if (!this.drives || this.drives.length === 0) {
            container.innerHTML = '<div style="padding: 40px; text-align: center; color: var(--cursor-text-secondary); grid-column: 1 / -1;">No drives found. Click Refresh to try again.</div>';
            return;
        }
        
        container.innerHTML = '';
        
        this.drives.forEach(drive => {
            const card = document.createElement('div');
            card.style.cssText = `
                background: var(--cursor-bg-secondary);
                border: 1px solid var(--cursor-border);
                border-radius: 12px;
                padding: 24px;
                cursor: pointer;
                transition: all 0.2s;
                text-align: center;
            `;
            
            card.innerHTML = `
                <div style="font-size: 48px; margin-bottom: 12px;">${drive.icon}</div>
                <div style="font-weight: 600; font-size: 16px; color: var(--cursor-text); margin-bottom: 8px;">${drive.name}</div>
                <div style="font-size: 12px; color: var(--cursor-text-secondary); margin-bottom: 4px;">${drive.type}</div>
                <div style="font-size: 11px; color: var(--cursor-text-muted);">${drive.path}</div>
            `;
            
            card.onmouseover = () => {
                card.style.transform = 'translateY(-4px)';
                card.style.boxShadow = '0 8px 16px rgba(119, 221, 190, 0.2)';
                card.style.borderColor = 'var(--cursor-jade-light)';
            };
            
            card.onmouseout = () => {
                card.style.transform = 'translateY(0)';
                card.style.boxShadow = 'none';
                card.style.borderColor = 'var(--cursor-border)';
            };
            
            card.onclick = () => this.browseDriveToCenter(drive);
            
            container.appendChild(card);
        });
    }
    
    renderDrives() {
        // Wait for DOM if not ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.renderDrives());
            return;
        }
        
        const container = document.getElementById('drives-list');
        if (!container) {
            // Silently return - element may not exist in all layouts
            return;
        }
        
        container.innerHTML = '';
        
        if (this.drives.length === 0) {
            container.innerHTML = '<div style="padding: 20px; text-align: center; color: var(--cursor-text-muted);">No drives found</div>';
            return;
        }
        
        this.drives.forEach(drive => {
            const driveEl = document.createElement('div');
            driveEl.className = 'drive-item';
            driveEl.style.cssText = `
                padding: 12px;
                margin: 8px;
                background: var(--cursor-input-bg);
                border: 2px solid var(--cursor-jade-light);
                border-radius: 8px;
                cursor: pointer;
                transition: all 0.2s ease;
            `;
            
            const sizeGB = drive.size ? (drive.size / (1024 * 1024 * 1024)).toFixed(1) : '?';
            const freeGB = drive.free ? (drive.free / (1024 * 1024 * 1024)).toFixed(1) : '?';
            
            driveEl.innerHTML = `
                <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 6px;">
                    <span style="font-size: 24px;">${drive.icon}</span>
                    <div style="flex: 1;">
                        <div style="font-weight: 600; color: var(--cursor-text); font-size: 14px;">
                            ${drive.name} ${drive.label !== drive.name ? `(${drive.label})` : ''}
                        </div>
                        <div style="font-size: 11px; color: var(--cursor-jade-dark);">
                            ${drive.type}
                        </div>
                    </div>
                </div>
                ${drive.size ? `
                    <div style="font-size: 11px; color: var(--cursor-text-secondary);">
                        💽 ${sizeGB} GB total | 📊 ${freeGB} GB free
                    </div>
                ` : ''}
            `;
            
            driveEl.onclick = () => this.browseDrive(drive);
            
            driveEl.onmouseenter = () => {
                driveEl.style.background = 'var(--cursor-jade-light)';
                driveEl.style.borderColor = 'var(--cursor-accent)';
                driveEl.style.boxShadow = '0 4px 12px var(--cursor-jade-glow)';
                driveEl.style.transform = 'translateY(-2px)';
            };
            
            driveEl.onmouseleave = () => {
                driveEl.style.background = 'var(--cursor-input-bg)';
                driveEl.style.borderColor = 'var(--cursor-jade-light)';
                driveEl.style.boxShadow = 'none';
                driveEl.style.transform = 'translateY(0)';
            };
            
            container.appendChild(driveEl);
        });
    }
    
    async browseDrive(drive) {
        console.log('[Explorer] 📂 Browsing drive:', drive.path);
        this.currentPath = drive.path;
        await this.loadDirectory(drive.path);
    }
    
    async browseDriveToCenter(drive) {
        console.log('[Explorer] 📂 Browsing drive to center:', drive.path);
        this.currentPath = drive.path;
        await this.loadDirectoryToCenter(drive.path);
    }
    
    async loadDirectory(dirPath) {
        try {
            console.log('[Explorer] 📂 Loading directory:', dirPath);
            
            const result = await window.electron.readDir(dirPath);
            
            if (result.success) {
                this.renderDirectory(dirPath, result.files);
            } else {
                console.error('[Explorer] ❌ Failed to read directory:', result.error);
                alert(`Cannot access ${dirPath}: ${result.error}`);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading directory:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    async loadDirectoryToCenter(dirPath) {
        try {
            console.log('[Explorer] 📂 Loading directory to center:', dirPath);
            
            const result = await window.electron.readDir(dirPath);
            
            if (result.success) {
                this.renderDirectoryToCenter(dirPath, result.files);
            } else {
                console.error('[Explorer] ❌ Failed to read directory:', result.error);
                alert(`Cannot access ${dirPath}: ${result.error}`);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error loading directory:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    renderDirectory(dirPath, files) {
        const container = document.getElementById('file-browser');
        if (!container) {
            console.error('[Explorer] ❌ File browser container not found');
            return;
        }
        
        container.innerHTML = '';
        
        // Add breadcrumb navigation
        const breadcrumb = document.createElement('div');
        breadcrumb.style.cssText = `
            padding: 12px;
            background: var(--cursor-bg-secondary);
            border-bottom: 1px solid var(--cursor-border);
            font-size: 12px;
            color: var(--cursor-text-secondary);
            display: flex;
            align-items: center;
            gap: 6px;
        `;
        breadcrumb.innerHTML = `
            <button onclick="fileExplorer.showDrives()" style="background: none; border: 1px solid var(--cursor-jade-light); color: var(--cursor-jade-dark); padding: 4px 8px; border-radius: 4px; cursor: pointer; font-size: 11px;">
                🏠 Drives
            </button>
            <span>📁 ${dirPath}</span>
        `;
        container.appendChild(breadcrumb);
        
        // Add file list
        const fileList = document.createElement('div');
        fileList.style.cssText = 'flex: 1; overflow-y: auto; padding: 8px;';
        
        // Sort: folders first, then files
        const sorted = files.sort((a, b) => {
            if (a.isDirectory && !b.isDirectory) return -1;
            if (!a.isDirectory && b.isDirectory) return 1;
            return a.name.localeCompare(b.name);
        });
        
        sorted.forEach(file => {
            const fileEl = document.createElement('div');
            fileEl.className = 'file-item';
            fileEl.style.cssText = `
                padding: 8px 12px;
                margin: 2px 0;
                border-radius: 6px;
                cursor: pointer;
                font-size: 13px;
                display: flex;
                align-items: center;
                gap: 8px;
                transition: all 0.2s ease;
            `;
            
            const icon = file.isDirectory ? '📁' : '📄';
            fileEl.innerHTML = `
                <span>${icon}</span>
                <span style="flex: 1;">${file.name}</span>
            `;
            
            fileEl.onclick = () => {
                if (file.isDirectory) {
                    this.loadDirectory(file.path);
                } else {
                    this.openFile(file.path, file.name);
                }
            };
            
            fileEl.onmouseenter = () => {
                fileEl.style.background = 'var(--cursor-jade-light)';
                fileEl.style.transform = 'translateX(4px)';
            };
            
            fileEl.onmouseleave = () => {
                fileEl.style.background = 'transparent';
                fileEl.style.transform = 'translateX(0)';
            };
            
            fileList.appendChild(fileEl);
        });
        
        container.appendChild(fileList);
    }
    
    renderDirectoryToCenter(dirPath, files) {
        const container = document.getElementById('center-explorer-content');
        if (!container) {
            console.error('[Explorer] ❌ Center explorer container not found');
            return;
        }
        
        container.innerHTML = '';
        
        // Add breadcrumb navigation header
        const header = document.createElement('div');
        header.style.cssText = `
            grid-column: 1 / -1;
            padding: 16px;
            background: var(--cursor-bg-secondary);
            border-radius: 12px;
            margin-bottom: 16px;
            display: flex;
            align-items: center;
            gap: 12px;
        `;
        header.innerHTML = `
            <button onclick="fileExplorer.loadDrivesToCenter()" style="background: var(--cursor-accent); color: white; border: none; padding: 8px 16px; border-radius: 6px; cursor: pointer; font-size: 13px; font-weight: 600;">
                ← Back to Drives
            </button>
            <span style="font-size: 14px; color: var(--cursor-text); font-weight: 600;">📁 ${dirPath}</span>
        `;
        container.appendChild(header);
        
        // Sort: folders first, then files
        const sorted = files.sort((a, b) => {
            if (a.isDirectory && !b.isDirectory) return -1;
            if (!a.isDirectory && b.isDirectory) return 1;
            return a.name.localeCompare(b.name);
        });
        
        // Render files and folders as cards
        sorted.forEach(file => {
            const card = document.createElement('div');
            card.style.cssText = `
                background: var(--cursor-bg-secondary);
                border: 1px solid var(--cursor-border);
                border-radius: 12px;
                padding: 24px;
                cursor: pointer;
                transition: all 0.2s;
                text-align: center;
                display: flex;
                flex-direction: column;
                align-items: center;
                justify-content: center;
            `;
            
            const icon = file.isDirectory ? '📁' : (file.name.endsWith('.js') ? '📜' : file.name.endsWith('.json') ? '⚙️' : file.name.endsWith('.md') ? '📝' : file.name.endsWith('.html') ? '🌐' : file.name.endsWith('.css') ? '🎨' : '📄');
            
            card.innerHTML = `
                <div style="font-size: 48px; margin-bottom: 12px;">${icon}</div>
                <div style="font-weight: 600; font-size: 14px; color: var(--cursor-text); margin-bottom: 4px; word-break: break-word; max-width: 100%;">${file.name}</div>
                <div style="font-size: 11px; color: var(--cursor-text-secondary);">${file.isDirectory ? 'Folder' : 'File'}</div>
            `;
            
            card.onmouseover = () => {
                card.style.transform = 'translateY(-4px)';
                card.style.boxShadow = '0 8px 16px rgba(119, 221, 190, 0.2)';
                card.style.borderColor = 'var(--cursor-jade-light)';
            };
            
            card.onmouseout = () => {
                card.style.transform = 'translateY(0)';
                card.style.boxShadow = 'none';
                card.style.borderColor = 'var(--cursor-border)';
            };
            
            card.onclick = () => {
                if (file.isDirectory) {
                    this.loadDirectoryToCenter(file.path);
                } else {
                    this.openFile(file.path, file.name);
                }
            };
            
            container.appendChild(card);
        });
    }
    
    showDrives() {
        const fileBrowser = document.getElementById('file-browser');
        if (fileBrowser) {
            fileBrowser.innerHTML = '';
        }
        const drivesList = document.getElementById('drives-list');
        if (drivesList) {
            drivesList.style.display = 'block';
        }
    }
    
    async openFile(filePath, filename) {
        try {
            console.log('[Explorer] 📄 Opening file:', filePath);
            
            const result = await window.electron.readFile(filePath);
            
            if (result.success) {
                // Hide all center tab content panels
                document.querySelectorAll('.tab-content-panel').forEach(panel => {
                    panel.style.display = 'none';
                });
                
                // Show Monaco container
                const monaco = document.getElementById('monaco-container');
                if (monaco) {
                    monaco.style.display = 'flex';
                }
                
                // Create new tab in Monaco
                const language = detectLanguage(filename);
                if (typeof createNewTab === 'function') {
                    createNewTab(filename, language, result.content, filePath);
                    console.log('[Explorer] ✅ File opened in Monaco');
                } else {
                    console.error('[Explorer] ❌ createNewTab function not available');
                }
                
                // Close the Explorer center tab (optional - keeps Explorer tab but hides content)
                // Uncomment next 2 lines if you want to close the Explorer tab completely:
                // if (window.tabSystem) {
                //     window.tabSystem.closeTab(window.tabSystem.activeTab);
                // }
            } else {
                alert(`Cannot open ${filename}: ${result.error}`);
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error opening file:', error);
            alert(`Error opening file: ${error.message}`);
        }
    }
    
    async launchProgram(programPath) {
        try {
            console.log('[Explorer] 🚀 Launching program:', programPath);
            
            if (window.electron && window.electron.launchProgram) {
                const result = await window.electron.launchProgram(programPath);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Program launched successfully');
                    
                    // Show notification
                    if (window.showNotification) {
                        window.showNotification('✅ Program Launched', `${programPath} is now running`, 'success');
                    }
                } else {
                    console.error('[Explorer] ❌ Failed to launch program:', result.error);
                    alert(`Cannot launch ${programPath}: ${result.error}`);
                }
            } else {
                alert('Program launching not available. Please check electron bridge.');
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error launching program:', error);
            alert(`Error launching program: ${error.message}`);
        }
    }
    
    async openInSystemExplorer(dirPath) {
        try {
            console.log('[Explorer] 🪟 Opening in system explorer:', dirPath);
            
            if (window.electron && window.electron.openInExplorer) {
                const result = await window.electron.openInExplorer(dirPath);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Opened in system explorer');
                } else {
                    console.error('[Explorer] ❌ Failed to open in explorer:', result.error);
                }
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error opening in explorer:', error);
        }
    }
    
    async createNewFolder(parentPath, folderName) {
        try {
            if (!folderName) {
                folderName = prompt('Enter folder name:');
                if (!folderName) return;
            }
            
            const newPath = `${parentPath}\\${folderName}`;
            
            if (window.electron && window.electron.createDirectory) {
                const result = await window.electron.createDirectory(newPath);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Folder created:', newPath);
                    await this.loadDirectory(parentPath); // Refresh
                } else {
                    alert(`Cannot create folder: ${result.error}`);
                }
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error creating folder:', error);
            alert(`Error: ${error.message}`);
        }
    }
    
    async deleteItem(itemPath, isDirectory = false) {
        const itemName = itemPath.split('\\').pop();
        const confirm = window.confirm(`Are you sure you want to delete "${itemName}"?`);
        
        if (!confirm) return;
        
        try {
            if (window.electron && window.electron.deleteItem) {
                const result = await window.electron.deleteItem(itemPath, isDirectory);
                
                if (result.success) {
                    console.log('[Explorer] ✅ Item deleted:', itemPath);
                    
                    // Refresh current directory
                    if (this.currentPath) {
                        await this.loadDirectory(this.currentPath);
                    }
                } else {
                    alert(`Cannot delete: ${result.error}`);
                }
            }
        } catch (error) {
            console.error('[Explorer] ❌ Error deleting item:', error);
            alert(`Error: ${error.message}`);
        }
    }
}

// Helper function (if not defined elsewhere)
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

// Initialize file explorer
window.fileExplorer = null;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.fileExplorer = new FileExplorer();
    });
} else {
    window.fileExplorer = new FileExplorer();
}

// Export
window.FileExplorer = FileExplorer;

if (typeof module !== 'undefined' && module.exports) {
    module.exports = FileExplorer;
}

console.log('[Explorer] 📦 File explorer module loaded');

})(); // End IIFE

