/**
 * BigDaddyG IDE - Enhanced File Browser
 * Full system access including all drives, hidden files, and system directories
 * 
 * Features:
 * - Browse all drives (C:, D:, E:, etc.)
 * - Show hidden files and system files
 * - Quick navigation to common locations
 * - Search across drives
 * - File preview and metadata
 * - Drag-and-drop support
 */

class EnhancedFileBrowser {
  constructor(containerSelector) {
    this.container = document.querySelector(containerSelector);
    this.currentPath = '';
    this.showHidden = true;
    this.showSystem = true;
    this.drives = [];
    this.history = [];
    this.historyIndex = -1;
    
    this.init();
  }
  
  async init() {
    console.log('[FileBrowser] Initializing enhanced file browser...');
    
    // Discover all available drives
    await this.discoverDrives();
    
    // Render UI
    this.render();
    
    // Set up event listeners
    this.setupEventListeners();
    
    // Load initial directory (user's home)
    await this.navigateTo(await window.electronAPI.getUserHome());
    
    console.log('[FileBrowser] Enhanced file browser ready');
  }
  
  async discoverDrives() {
    try {
      // Scan for drives A: through Z:
      const driveLetters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('');
      const driveChecks = driveLetters.map(async (letter) => {
        const drivePath = `${letter}:\\`;
        const exists = await window.electronAPI.pathExists(drivePath);
        if (exists) {
          const info = await window.electronAPI.getDriveInfo(drivePath);
          return { letter, path: drivePath, ...info };
        }
        return null;
      });
      
      const results = await Promise.all(driveChecks);
      this.drives = results.filter(d => d !== null);
      
      console.log(`[FileBrowser] Found ${this.drives.length} drives:`, this.drives);
    } catch (error) {
      console.error('[FileBrowser] Error discovering drives:', error);
      this.drives = [];
    }
  }
  
  render() {
    this.container.innerHTML = `
      <div class="file-browser-enhanced">
        <!-- Toolbar -->
        <div class="fb-toolbar">
          <div class="fb-nav-buttons">
            <button id="fb-back" title="Back" disabled>◀</button>
            <button id="fb-forward" title="Forward" disabled>▶</button>
            <button id="fb-up" title="Up" disabled>⬆</button>
            <button id="fb-refresh" title="Refresh">🔄</button>
          </div>
          
          <div class="fb-address-bar">
            <input type="text" id="fb-path-input" placeholder="Enter path..." />
            <button id="fb-go">Go</button>
          </div>
          
          <div class="fb-options">
            <label>
              <input type="checkbox" id="fb-show-hidden" ${this.showHidden ? 'checked' : ''} />
              Hidden
            </label>
            <label>
              <input type="checkbox" id="fb-show-system" ${this.showSystem ? 'checked' : ''} />
              System
            </label>
            <button id="fb-search" title="Search">🔍</button>
          </div>
        </div>
        
        <!-- Quick Access -->
        <div class="fb-quick-access">
          <div class="fb-section-title">Quick Access</div>
          <div class="fb-quick-items">
            <button class="fb-quick-btn" data-path="home">🏠 Home</button>
            <button class="fb-quick-btn" data-path="desktop">🖥️ Desktop</button>
            <button class="fb-quick-btn" data-path="documents">📄 Documents</button>
            <button class="fb-quick-btn" data-path="downloads">📥 Downloads</button>
          </div>
        </div>
        
        <!-- Drives -->
        <div class="fb-drives">
          <div class="fb-section-title">Drives</div>
          <div class="fb-drives-list" id="fb-drives-list"></div>
        </div>
        
        <!-- File List -->
        <div class="fb-file-list-container">
          <div class="fb-breadcrumb" id="fb-breadcrumb"></div>
          <div class="fb-file-list" id="fb-file-list">
            <div class="fb-loading">Loading...</div>
          </div>
        </div>
        
        <!-- Status Bar -->
        <div class="fb-status">
          <span id="fb-status-text">Ready</span>
          <span id="fb-status-items">0 items</span>
        </div>
      </div>
    `;
    
    // Render drives
    this.renderDrives();
    
    // Add styles
    this.injectStyles();
  }
  
  renderDrives() {
    const drivesList = document.getElementById('fb-drives-list');
    if (!drivesList) return;
    
    drivesList.innerHTML = this.drives.map(drive => `
      <div class="fb-drive-item" data-path="${drive.path}">
        <div class="fb-drive-icon">💾</div>
        <div class="fb-drive-info">
          <div class="fb-drive-name">${drive.letter}: ${drive.label || 'Local Disk'}</div>
          <div class="fb-drive-space">${this.formatBytes(drive.free)} free of ${this.formatBytes(drive.total)}</div>
          <div class="fb-drive-bar">
            <div class="fb-drive-bar-fill" style="width: ${drive.usedPercent}%"></div>
          </div>
        </div>
      </div>
    `).join('');
  }
  
  setupEventListeners() {
    // Navigation buttons
    document.getElementById('fb-back')?.addEventListener('click', () => this.goBack());
    document.getElementById('fb-forward')?.addEventListener('click', () => this.goForward());
    document.getElementById('fb-up')?.addEventListener('click', () => this.goUp());
    document.getElementById('fb-refresh')?.addEventListener('click', () => this.refresh());
    
    // Address bar
    document.getElementById('fb-go')?.addEventListener('click', () => {
      const path = document.getElementById('fb-path-input').value;
      this.navigateTo(path);
    });
    
    document.getElementById('fb-path-input')?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        const path = e.target.value;
        this.navigateTo(path);
      }
    });
    
    // Options
    document.getElementById('fb-show-hidden')?.addEventListener('change', (e) => {
      this.showHidden = e.target.checked;
      this.refresh();
    });
    
    document.getElementById('fb-show-system')?.addEventListener('change', (e) => {
      this.showSystem = e.target.checked;
      this.refresh();
    });
    
    document.getElementById('fb-search')?.addEventListener('click', () => this.showSearchDialog());
    
    // Quick access
    document.querySelectorAll('.fb-quick-btn').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        const pathType = e.target.dataset.path;
        const path = await window.electronAPI.getSpecialPath(pathType);
        this.navigateTo(path);
      });
    });
    
    // Drives
    document.getElementById('fb-drives-list')?.addEventListener('click', (e) => {
      const driveItem = e.target.closest('.fb-drive-item');
      if (driveItem) {
        this.navigateTo(driveItem.dataset.path);
      }
    });
  }
  
  async navigateTo(path) {
    if (!path) return;
    
    try {
      this.setStatus('Loading...', true);
      
      // Get directory contents
      const contents = await window.electronAPI.readDirectory(path, {
        showHidden: this.showHidden,
        showSystem: this.showSystem
      });
      
      // Update history
      if (path !== this.currentPath) {
        this.history = this.history.slice(0, this.historyIndex + 1);
        this.history.push(path);
        this.historyIndex = this.history.length - 1;
      }
      
      this.currentPath = path;
      
      // Update UI
      document.getElementById('fb-path-input').value = path;
      this.updateNavigationButtons();
      this.renderBreadcrumb();
      this.renderFileList(contents);
      
      this.setStatus(`${contents.length} items`, false);
      
    } catch (error) {
      console.error('[FileBrowser] Navigation error:', error);
      this.setStatus(`Error: ${error.message}`, false);
      this.showError(`Cannot access: ${path}\n${error.message}`);
    }
  }
  
  renderBreadcrumb() {
    const breadcrumb = document.getElementById('fb-breadcrumb');
    if (!breadcrumb) return;
    
    const parts = this.currentPath.split(/[\\\/]/).filter(p => p);
    let cumPath = '';
    
    breadcrumb.innerHTML = parts.map((part, i) => {
      cumPath += (i === 0) ? part : `\\${part}`;
      return `<span class="fb-breadcrumb-item" data-path="${cumPath}">${part}</span>`;
    }).join('<span class="fb-breadcrumb-sep">›</span>');
    
    // Add click handlers
    breadcrumb.querySelectorAll('.fb-breadcrumb-item').forEach(item => {
      item.addEventListener('click', (e) => {
        this.navigateTo(e.target.dataset.path);
      });
    });
  }
  
  renderFileList(items) {
    const fileList = document.getElementById('fb-file-list');
    if (!fileList) return;
    
    // Sort: directories first, then by name
    items.sort((a, b) => {
      if (a.isDirectory !== b.isDirectory) {
        return a.isDirectory ? -1 : 1;
      }
      return a.name.localeCompare(b.name);
    });
    
    fileList.innerHTML = items.map(item => {
      const icon = this.getFileIcon(item);
      const hidden = item.isHidden ? 'fb-hidden' : '';
      const system = item.isSystem ? 'fb-system' : '';
      
      return `
        <div class="fb-file-item ${hidden} ${system}" data-path="${item.path}">
          <div class="fb-file-icon">${icon}</div>
          <div class="fb-file-info">
            <div class="fb-file-name">${item.name}</div>
            <div class="fb-file-meta">
              ${item.isDirectory ? 'Folder' : this.formatBytes(item.size)} • ${this.formatDate(item.modified)}
            </div>
          </div>
        </div>
      `;
    }).join('');
    
    // Add click handlers
    fileList.querySelectorAll('.fb-file-item').forEach(item => {
      item.addEventListener('click', (e) => {
        this.handleItemClick(e.currentTarget.dataset.path);
      });
      
      item.addEventListener('dblclick', (e) => {
        this.handleItemDoubleClick(e.currentTarget.dataset.path);
      });
    });
    
    // Update status
    document.getElementById('fb-status-items').textContent = `${items.length} items`;
  }
  
  async handleItemClick(path) {
    // Select item
    document.querySelectorAll('.fb-file-item').forEach(item => {
      item.classList.remove('fb-selected');
    });
    document.querySelector(`[data-path="${path}"]`)?.classList.add('fb-selected');
  }
  
  async handleItemDoubleClick(path) {
    const info = await window.electronAPI.getFileInfo(path);
    
    if (info.isDirectory) {
      // Navigate into directory
      this.navigateTo(path);
    } else {
      // Open file in editor
      this.openFileInEditor(path);
    }
  }
  
  async openFileInEditor(path) {
    try {
      const content = await window.electronAPI.readFile(path);
      const filename = path.split(/[\\\/]/).pop();
      
      // Emit event to open in editor
      window.dispatchEvent(new CustomEvent('file-open', {
        detail: { path, filename, content }
      }));
      
      console.log('[FileBrowser] Opened file:', path);
    } catch (error) {
      console.error('[FileBrowser] Error opening file:', error);
      this.showError(`Cannot open file: ${path}\n${error.message}`);
    }
  }
  
  goBack() {
    if (this.historyIndex > 0) {
      this.historyIndex--;
      this.navigateTo(this.history[this.historyIndex]);
    }
  }
  
  goForward() {
    if (this.historyIndex < this.history.length - 1) {
      this.historyIndex++;
      this.navigateTo(this.history[this.historyIndex]);
    }
  }
  
  async goUp() {
    if (!this.currentPath) return;
    
    const parts = this.currentPath.split(/[\\\/]/);
    if (parts.length > 1) {
      parts.pop();
      const parentPath = parts.join('\\');
      await this.navigateTo(parentPath || parts[0] + '\\');
    }
  }
  
  refresh() {
    this.navigateTo(this.currentPath);
  }
  
  updateNavigationButtons() {
    document.getElementById('fb-back').disabled = this.historyIndex <= 0;
    document.getElementById('fb-forward').disabled = this.historyIndex >= this.history.length - 1;
    document.getElementById('fb-up').disabled = !this.currentPath || this.currentPath.split(/[\\\/]/).length <= 1;
  }
  
  showSearchDialog() {
    const query = prompt('Search for files and folders:');
    if (query) {
      this.performSearch(query);
    }
  }
  
  async performSearch(query) {
    this.setStatus('Searching...', true);
    
    try {
      const results = await window.electronAPI.searchFiles(this.currentPath, query, {
        showHidden: this.showHidden,
        showSystem: this.showSystem
      });
      
      this.renderFileList(results);
      this.setStatus(`Found ${results.length} items`, false);
    } catch (error) {
      console.error('[FileBrowser] Search error:', error);
      this.setStatus('Search failed', false);
    }
  }
  
  getFileIcon(item) {
    if (item.isDirectory) return '📁';
    
    const ext = item.name.split('.').pop().toLowerCase();
    const icons = {
      // Code
      js: '📜', ts: '📘', jsx: '⚛️', tsx: '⚛️',
      py: '🐍', java: '☕', cpp: '⚙️', c: '🔧', cs: '🔷',
      rs: '🦀', go: '🐹', rb: '💎', php: '🐘',
      
      // Web
      html: '🌐', css: '🎨', scss: '🎨', json: '📋',
      xml: '📄', yaml: '📋', yml: '📋',
      
      // Documents
      md: '📝', txt: '📄', pdf: '📕', doc: '📘', docx: '📘',
      xls: '📗', xlsx: '📗', ppt: '📙', pptx: '📙',
      
      // Media
      jpg: '🖼️', jpeg: '🖼️', png: '🖼️', gif: '🖼️', svg: '🖼️',
      mp4: '🎬', mov: '🎬', avi: '🎬', mkv: '🎬',
      mp3: '🎵', wav: '🎵', flac: '🎵',
      
      // Archives
      zip: '📦', rar: '📦', '7z': '📦', tar: '📦', gz: '📦',
      
      // Executables
      exe: '⚙️', dll: '🔧', msi: '📦', app: '📱',
      
      // Assembly
      asm: '⚡', s: '⚡',
      
      // Shell
      sh: '💻', bat: '💻', ps1: '💻', cmd: '💻'
    };
    
    return icons[ext] || '📄';
  }
  
  formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
  }
  
  formatDate(timestamp) {
    const date = new Date(timestamp);
    const now = new Date();
    const diff = now - date;
    
    if (diff < 86400000) { // Less than 24 hours
      return date.toLocaleTimeString();
    } else if (diff < 604800000) { // Less than 7 days
      return date.toLocaleDateString(undefined, { weekday: 'short', hour: '2-digit', minute: '2-digit' });
    } else {
      return date.toLocaleDateString();
    }
  }
  
  setStatus(text, isLoading) {
    const statusText = document.getElementById('fb-status-text');
    if (statusText) {
      statusText.textContent = text;
      statusText.style.fontStyle = isLoading ? 'italic' : 'normal';
    }
  }
  
  showError(message) {
    // Replace with better modal
    if (window.showNotification) {
      window.showNotification('❌ Error', message, 'error', 5000);
    } else {
      // Fallback to custom modal
      const modal = document.createElement('div');
      modal.style.cssText = `
        position: fixed;
        top: 0;
        left: 0;
        right: 0;
        bottom: 0;
        background: rgba(0, 0, 0, 0.8);
        z-index: 10000;
        display: flex;
        justify-content: center;
        align-items: center;
      `;
      
      modal.innerHTML = `
        <div style="background: rgba(10, 10, 30, 0.98); border: 1px solid var(--red); border-radius: 12px; padding: 30px; min-width: 400px; max-width: 600px;">
          <h3 style="margin: 0 0 16px 0; color: var(--red); font-size: 18px;">❌ Error</h3>
          <p style="margin: 0 0 24px 0; color: #ccc;">${message}</p>
          <button onclick="this.parentElement.parentElement.remove()" style="padding: 10px 20px; background: var(--red); color: #fff; border: none; border-radius: 6px; cursor: pointer; font-weight: bold;">Close</button>
        </div>
      `;
      
      modal.onclick = (e) => {
        if (e.target === modal) modal.remove();
      };
      
      document.body.appendChild(modal);
      setTimeout(() => modal.remove(), 5000);
    }
  }
  
  injectStyles() {
    if (document.getElementById('file-browser-styles')) return;
    
    const style = document.createElement('style');
    style.id = 'file-browser-styles';
    style.textContent = `
      .file-browser-enhanced {
        display: flex;
        flex-direction: column;
        height: 100%;
        background: var(--void);
        color: var(--green);
        font-family: 'Courier New', monospace;
      }
      
      .fb-toolbar {
        display: flex;
        gap: 10px;
        padding: 10px;
        background: rgba(0, 255, 0, 0.05);
        border-bottom: 1px solid var(--green);
      }
      
      .fb-nav-buttons {
        display: flex;
        gap: 5px;
      }
      
      .fb-nav-buttons button {
        padding: 5px 10px;
        background: rgba(0, 255, 0, 0.1);
        color: var(--green);
        border: 1px solid var(--green);
        border-radius: 4px;
        cursor: pointer;
      }
      
      .fb-nav-buttons button:disabled {
        opacity: 0.3;
        cursor: not-allowed;
      }
      
      .fb-address-bar {
        flex: 1;
        display: flex;
        gap: 5px;
      }
      
      .fb-address-bar input {
        flex: 1;
        padding: 5px 10px;
        background: rgba(0, 0, 0, 0.5);
        color: var(--green);
        border: 1px solid var(--green);
        border-radius: 4px;
        font-family: inherit;
      }
      
      .fb-address-bar button {
        padding: 5px 15px;
        background: var(--green);
        color: var(--void);
        border: none;
        border-radius: 4px;
        cursor: pointer;
        font-weight: bold;
      }
      
      .fb-options {
        display: flex;
        gap: 10px;
        align-items: center;
      }
      
      .fb-options label {
        display: flex;
        align-items: center;
        gap: 5px;
        font-size: 12px;
      }
      
      .fb-quick-access, .fb-drives {
        padding: 10px;
        border-bottom: 1px solid rgba(0, 255, 0, 0.2);
      }
      
      .fb-section-title {
        font-size: 11px;
        text-transform: uppercase;
        opacity: 0.7;
        margin-bottom: 8px;
      }
      
      .fb-quick-items {
        display: flex;
        gap: 5px;
        flex-wrap: wrap;
      }
      
      .fb-quick-btn {
        padding: 5px 10px;
        background: rgba(0, 255, 0, 0.1);
        color: var(--green);
        border: 1px solid var(--green);
        border-radius: 4px;
        cursor: pointer;
        font-size: 12px;
      }
      
      .fb-drives-list {
        display: flex;
        flex-direction: column;
        gap: 8px;
      }
      
      .fb-drive-item {
        display: flex;
        gap: 10px;
        padding: 8px;
        background: rgba(0, 255, 0, 0.05);
        border: 1px solid rgba(0, 255, 0, 0.2);
        border-radius: 4px;
        cursor: pointer;
        transition: all 0.2s;
      }
      
      .fb-drive-item:hover {
        background: rgba(0, 255, 0, 0.1);
        border-color: var(--green);
      }
      
      .fb-drive-icon {
        font-size: 24px;
      }
      
      .fb-drive-info {
        flex: 1;
      }
      
      .fb-drive-name {
        font-weight: bold;
        font-size: 13px;
      }
      
      .fb-drive-space {
        font-size: 11px;
        opacity: 0.7;
        margin-top: 2px;
      }
      
      .fb-drive-bar {
        height: 4px;
        background: rgba(0, 0, 0, 0.5);
        border-radius: 2px;
        margin-top: 4px;
        overflow: hidden;
      }
      
      .fb-drive-bar-fill {
        height: 100%;
        background: var(--green);
        transition: width 0.3s;
      }
      
      .fb-file-list-container {
        flex: 1;
        display: flex;
        flex-direction: column;
        overflow: hidden;
      }
      
      .fb-breadcrumb {
        padding: 10px;
        background: rgba(0, 0, 0, 0.3);
        border-bottom: 1px solid rgba(0, 255, 0, 0.2);
        display: flex;
        align-items: center;
        gap: 5px;
        font-size: 12px;
        overflow-x: auto;
      }
      
      .fb-breadcrumb-item {
        cursor: pointer;
        padding: 2px 5px;
        border-radius: 3px;
        transition: background 0.2s;
      }
      
      .fb-breadcrumb-item:hover {
        background: rgba(0, 255, 0, 0.1);
      }
      
      .fb-breadcrumb-sep {
        opacity: 0.5;
      }
      
      .fb-file-list {
        flex: 1;
        overflow-y: auto;
        padding: 10px;
      }
      
      .fb-file-item {
        display: flex;
        gap: 10px;
        padding: 8px;
        border-radius: 4px;
        cursor: pointer;
        transition: all 0.2s;
        border: 1px solid transparent;
      }
      
      .fb-file-item:hover {
        background: rgba(0, 255, 0, 0.1);
        border-color: rgba(0, 255, 0, 0.3);
      }
      
      .fb-file-item.fb-selected {
        background: rgba(0, 255, 0, 0.15);
        border-color: var(--green);
      }
      
      .fb-file-item.fb-hidden {
        opacity: 0.6;
      }
      
      .fb-file-item.fb-system {
        opacity: 0.5;
      }
      
      .fb-file-icon {
        font-size: 20px;
      }
      
      .fb-file-info {
        flex: 1;
      }
      
      .fb-file-name {
        font-size: 13px;
      }
      
      .fb-file-meta {
        font-size: 11px;
        opacity: 0.7;
        margin-top: 2px;
      }
      
      .fb-status {
        display: flex;
        justify-content: space-between;
        padding: 8px 10px;
        background: rgba(0, 0, 0, 0.3);
        border-top: 1px solid rgba(0, 255, 0, 0.2);
        font-size: 11px;
      }
      
      .fb-loading {
        text-align: center;
        padding: 20px;
        opacity: 0.7;
      }
    `;
    
    document.head.appendChild(style);
  }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
  module.exports = EnhancedFileBrowser;
}

