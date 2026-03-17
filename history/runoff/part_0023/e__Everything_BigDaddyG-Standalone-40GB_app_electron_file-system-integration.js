/**
 * BigDaddyG IDE - Complete File System Integration
 * Connects file tree, explorer, and editor with actual file operations
 */

// Use window.require for Electron node integration
let fs, path, os;
if (typeof window !== 'undefined' && window.require) {
  fs = window.require('fs');
  path = window.require('path');
  os = window.require('os');
}

class FileSystemIntegration {
  constructor() {
    this.currentWorkspace = null;
    this.openFiles = new Map(); // Track open files
    this.fileWatchers = new Map(); // File change watchers
    this.recentFolders = this.loadRecentFolders();

    console.log('[FileSystem] 🗂️ File system integration initialized');
  }

  // ========== WORKSPACE MANAGEMENT ==========

  async openWorkspace(folderPath) {
    if (!fs || !path) {
      console.error('[FileSystem] Node.js modules not available');
      return { success: false, error: 'File system not available' };
    }

    try {
      const stats = await fs.promises.stat(folderPath);
      if (!stats.isDirectory()) {
        return { success: false, error: 'Not a directory' };
      }

      this.currentWorkspace = folderPath;
      this.addToRecentFolders(folderPath);

      console.log('[FileSystem] ✅ Opened workspace:', folderPath);

      // Populate file explorer
      this.populateFileExplorer(folderPath);

      return { success: true, path: folderPath };
    } catch (error) {
      console.error('[FileSystem] Error opening workspace:', error);
      return { success: false, error: error.message };
    }
  }

  async populateFileExplorer(folderPath) {
    const fileTree = document.getElementById('file-tree');
    if (!fileTree) {
      console.warn('[FileSystem] File tree element not found');
      return;
    }

    // Clear existing content
    fileTree.innerHTML = '';

    // Add root folder
    try {
      const files = await this.readDirectory(folderPath);

      files.forEach(file => {
        const fileItem = this.createFileItem(file);
        fileTree.appendChild(fileItem);
      });

      console.log(`[FileSystem] Populated file explorer with ${files.length} items`);
    } catch (error) {
      console.error('[FileSystem] Error populating explorer:', error);
      fileTree.innerHTML = `<div style="color: var(--red); padding: 10px;">Error loading folder: ${error.message}</div>`;
    }
  }

  async readDirectory(dirPath) {
    if (!fs) return [];

    const entries = await fs.promises.readdir(dirPath, { withFileTypes: true });
    const files = [];

    for (const entry of entries) {
      const fullPath = path.join(dirPath, entry.name);

      // Skip hidden files starting with .
      if (entry.name.startsWith('.')) continue;

      files.push({
        name: entry.name,
        path: fullPath,
        isDirectory: entry.isDirectory(),
        icon: this.getFileIcon(entry.name, entry.isDirectory())
      });
    }

    // Sort: directories first, then files
    return files.sort((a, b) => {
      if (a.isDirectory && !b.isDirectory) return -1;
      if (!a.isDirectory && b.isDirectory) return 1;
      return a.name.localeCompare(b.name);
    });
  }

  createFileItem(file) {
    const item = document.createElement('div');
    item.className = 'file-item';
    item.dataset.path = file.path;
    item.dataset.isDirectory = file.isDirectory;

    item.innerHTML = `
            <span class="file-icon">${file.icon}</span>
            <span class="file-name">${file.name}</span>
        `;

    // Click handler
    item.addEventListener('click', () => {
      if (file.isDirectory) {
        this.toggleDirectory(item, file.path);
      } else {
        this.openFileInEditor(file.path);
      }
    });

    // Context menu
    item.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      this.showFileContextMenu(file, e.clientX, e.clientY);
    });

    return item;
  }

  async toggleDirectory(item, dirPath) {
    const isExpanded = item.classList.contains('expanded');

    if (isExpanded) {
      // Collapse
      item.classList.remove('expanded');
      const childItems = item.querySelectorAll('.file-item-child');
      childItems.forEach(child => child.remove());
    } else {
      // Expand
      item.classList.add('expanded');

      try {
        const files = await this.readDirectory(dirPath);

        files.forEach(file => {
          const childItem = this.createFileItem(file);
          childItem.classList.add('file-item-child');
          childItem.style.marginLeft = '20px';
          item.after(childItem);
        });
      } catch (error) {
        console.error('[FileSystem] Error expanding directory:', error);
      }
    }
  }

  // ========== FILE OPERATIONS ==========

  async openFileInEditor(filePath) {
    if (!fs || !path) {
      console.error('[FileSystem] Cannot open file: File system not available');
      return;
    }

    try {
      const content = await fs.promises.readFile(filePath, 'utf8');
      const fileName = path.basename(filePath);
      const ext = path.extname(filePath).slice(1);

      // Detect language
      const language = this.detectLanguage(ext);

      // Create tab and open in editor
      if (typeof openFile === 'function') {
        openFile(filePath, language, content);
      } else if (typeof createNewTab === 'function') {
        createNewTab(fileName, language, content);
      }

      // Track open file
      this.openFiles.set(filePath, {
        content: content,
        modified: false,
        lastSaved: Date.now()
      });

      console.log(`[FileSystem] ✅ Opened file: ${fileName}`);

    } catch (error) {
      console.error('[FileSystem] Error opening file:', error);
      alert(`Error opening file: ${error.message}`);
    }
  }

  async saveFile(filePath, content) {
    if (!fs) {
      console.error('[FileSystem] Cannot save file: File system not available');
      return { success: false, error: 'File system not available' };
    }

    try {
      await fs.promises.writeFile(filePath, content, 'utf8');

      // Update tracking
      if (this.openFiles.has(filePath)) {
        this.openFiles.get(filePath).modified = false;
        this.openFiles.get(filePath).lastSaved = Date.now();
      }

      console.log('[FileSystem] ✅ Saved file:', filePath);
      return { success: true };

    } catch (error) {
      console.error('[FileSystem] Error saving file:', error);
      return { success: false, error: error.message };
    }
  }

  async createNewFile(dirPath, fileName) {
    if (!fs || !path) return { success: false, error: 'File system not available' };

    const filePath = path.join(dirPath, fileName);

    try {
      await fs.promises.writeFile(filePath, '', 'utf8');

      console.log('[FileSystem] ✅ Created file:', filePath);

      // Refresh explorer
      this.populateFileExplorer(this.currentWorkspace || dirPath);

      // Open in editor
      this.openFileInEditor(filePath);

      return { success: true, path: filePath };

    } catch (error) {
      console.error('[FileSystem] Error creating file:', error);
      return { success: false, error: error.message };
    }
  }

  async createNewFolder(dirPath, folderName) {
    if (!fs || !path) return { success: false, error: 'File system not available' };

    const folderPath = path.join(dirPath, folderName);

    try {
      await fs.promises.mkdir(folderPath, { recursive: true });

      console.log('[FileSystem] ✅ Created folder:', folderPath);

      // Refresh explorer
      this.populateFileExplorer(this.currentWorkspace || dirPath);

      return { success: true, path: folderPath };

    } catch (error) {
      console.error('[FileSystem] Error creating folder:', error);
      return { success: false, error: error.message };
    }
  }

  // ========== CONTEXT MENU ==========

  showFileContextMenu(file, x, y) {
    // Remove existing menu
    const existingMenu = document.getElementById('file-context-menu');
    if (existingMenu) existingMenu.remove();

    const menu = document.createElement('div');
    menu.id = 'file-context-menu';
    menu.style.cssText = `
            position: fixed;
            left: ${x}px;
            top: ${y}px;
            background: rgba(10, 10, 30, 0.98);
            border: 1px solid var(--cyan);
            border-radius: 8px;
            padding: 8px 0;
            min-width: 180px;
            z-index: 10000;
            box-shadow: 0 4px 20px rgba(0, 212, 255, 0.3);
        `;

    const actions = file.isDirectory ? [
      { label: '📄 New File', action: () => this.promptNewFile(file.path) },
      { label: '📁 New Folder', action: () => this.promptNewFolder(file.path) },
      { label: '🔄 Refresh', action: () => this.populateFileExplorer(file.path) },
      { label: '📋 Copy Path', action: () => navigator.clipboard.writeText(file.path) },
      { label: '🗑️ Delete', action: () => this.deleteItem(file.path) }
    ] : [
      { label: '📂 Open', action: () => this.openFileInEditor(file.path) },
      { label: '✏️ Rename', action: () => this.promptRename(file.path) },
      { label: '📋 Copy Path', action: () => navigator.clipboard.writeText(file.path) },
      { label: '🗑️ Delete', action: () => this.deleteItem(file.path) }
    ];

    actions.forEach(action => {
      const item = document.createElement('div');
      item.textContent = action.label;
      item.style.cssText = `
                padding: 8px 16px;
                cursor: pointer;
                color: #fff;
                font-size: 13px;
                transition: background 0.2s;
            `;
      item.onmouseover = () => item.style.background = 'rgba(0, 212, 255, 0.2)';
      item.onmouseout = () => item.style.background = 'transparent';
      item.onclick = () => {
        action.action();
        menu.remove();
      };
      menu.appendChild(item);
    });

    document.body.appendChild(menu);

    // Close on click outside
    setTimeout(() => {
      document.addEventListener('click', () => menu.remove(), { once: true });
    }, 100);
  }

  // ========== UTILITIES ==========

  getFileIcon(fileName, isDirectory) {
    if (isDirectory) return '📁';

    const ext = fileName.split('.').pop().toLowerCase();
    const icons = {
      js: '📜', ts: '📘', jsx: '⚛️', tsx: '⚛️',
      py: '🐍', java: '☕', cpp: '⚙️', c: '🔧', h: '📋',
      rs: '🦀', go: '🐹', rb: '💎', php: '🐘',
      html: '🌐', css: '🎨', json: '📊', xml: '📄',
      md: '📝', txt: '📄', asm: '⚡', s: '⚡',
      sql: '🗄️', sh: '💻', bat: '💻', ps1: '💻',
      jpg: '🖼️', png: '🖼️', gif: '🖼️', svg: '🎨',
      pdf: '📕', zip: '📦', rar: '📦', tar: '📦'
    };
    return icons[ext] || '📄';
  }

  detectLanguage(ext) {
    const languages = {
      js: 'javascript', ts: 'typescript', jsx: 'javascript', tsx: 'typescript',
      py: 'python', java: 'java', cpp: 'cpp', c: 'c', h: 'cpp',
      rs: 'rust', go: 'go', rb: 'ruby', php: 'php',
      html: 'html', css: 'css', json: 'json', xml: 'xml',
      md: 'markdown', txt: 'plaintext', asm: 'asm', s: 'asm',
      sql: 'sql', sh: 'shell', bat: 'bat', ps1: 'powershell'
    };
    return languages[ext] || 'plaintext';
  }

  promptNewFile(dirPath) {
    const fileName = prompt('Enter file name:');
    if (fileName) {
      this.createNewFile(dirPath, fileName);
    }
  }

  promptNewFolder(dirPath) {
    const folderName = prompt('Enter folder name:');
    if (folderName) {
      this.createNewFolder(dirPath, folderName);
    }
  }

  async promptRename(oldPath) {
    if (!fs || !path) return;

    const oldName = path.basename(oldPath);
    const newName = prompt('Rename to:', oldName);

    if (newName && newName !== oldName) {
      const newPath = path.join(path.dirname(oldPath), newName);

      try {
        await fs.promises.rename(oldPath, newPath);
        console.log('[FileSystem] ✅ Renamed:', newPath);
        this.populateFileExplorer(this.currentWorkspace);
      } catch (error) {
        console.error('[FileSystem] Error renaming:', error);
        alert(`Error renaming: ${error.message}`);
      }
    }
  }

  async deleteItem(itemPath) {
    if (!fs) return;

    if (!confirm(`Delete ${path.basename(itemPath)}?`)) return;

    try {
      const stats = await fs.promises.stat(itemPath);
      if (stats.isDirectory()) {
        await fs.promises.rmdir(itemPath, { recursive: true });
      } else {
        await fs.promises.unlink(itemPath);
      }

      console.log('[FileSystem] ✅ Deleted:', itemPath);
      this.populateFileExplorer(this.currentWorkspace);

    } catch (error) {
      console.error('[FileSystem] Error deleting:', error);
      alert(`Error deleting: ${error.message}`);
    }
  }

  // ========== RECENT FOLDERS ==========

  loadRecentFolders() {
    try {
      const stored = localStorage.getItem('bigdaddyg_recent_folders');
      return stored ? JSON.parse(stored) : [];
    } catch (error) {
      return [];
    }
  }

  addToRecentFolders(folderPath) {
    this.recentFolders = this.recentFolders.filter(p => p !== folderPath);
    this.recentFolders.unshift(folderPath);
    this.recentFolders = this.recentFolders.slice(0, 10); // Keep last 10

    try {
      localStorage.setItem('bigdaddyg_recent_folders', JSON.stringify(this.recentFolders));
    } catch (error) {
      console.error('[FileSystem] Error saving recent folders:', error);
    }
  }
}

// Initialize and export
if (typeof window !== 'undefined') {
  window.fileSystemIntegration = new FileSystemIntegration();
  console.log('[FileSystem] ✅ File system integration ready');
}
