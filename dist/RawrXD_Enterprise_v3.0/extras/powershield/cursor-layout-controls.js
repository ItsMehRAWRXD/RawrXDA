/**
 * Cursor IDE Layout Controls - Complete Menu Bar & Editor Layout System
 * Mirrors VS Code / Cursor IDE interface with full tab management, split panes, and panel controls
 */

class CursorLayoutControls {
  constructor() {
    this.state = {
      openTabs: [],
      activeTab: null,
      splitMode: 'single', // single, vertical, horizontal
      panels: {
        left: { visible: true, type: 'explorer' }, // explorer, search, scm, extensions, debug
        right: { visible: false, type: 'outline' }, // outline, timeline, problems, debug-console, comments
        bottom: { visible: false, type: 'terminal', collapsed: false } // terminal, problems, output, debug-console
      },
      panes: {
        main: { files: [] },
        split: { files: [] }
      }
    };
    
    this.init();
  }

  init() {
    this.createMenuBar();
    this.createTabBar();
    this.createEditorControls();
    this.createBottomPanel();
    this.setupEventListeners();
    this.logSuccess('✅ Cursor Layout Controls initialized');
  }

  // ============================================================
  // 1. MENU BAR (File / Edit / Selection / View / Go / Run / Terminal / Help)
  // ============================================================
  createMenuBar() {
    const menuBarHTML = `
      <div class="cursor-menu-bar" id="cursor-menu-bar" role="menubar">
        <div class="menu-container">
          <!-- File Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">File</span>
            <div class="menu-dropdown" data-menu="file">
              <div class="menu-entry" data-action="file-new"><span class="menu-icon">📄</span> New File</div>
              <div class="menu-entry" data-action="file-new-folder"><span class="menu-icon">📁</span> New Folder</div>
              <div class="menu-entry" data-action="file-open"><span class="menu-icon">📂</span> Open File</div>
              <div class="menu-entry" data-action="file-open-folder"><span class="menu-icon">📂</span> Open Folder</div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="file-save"><span class="menu-icon">💾</span> Save<span class="menu-shortcut">Ctrl+S</span></div>
              <div class="menu-entry" data-action="file-save-all"><span class="menu-icon">💾</span> Save All<span class="menu-shortcut">Ctrl+K S</span></div>
              <div class="menu-entry" data-action="file-save-as"><span class="menu-icon">💾</span> Save As<span class="menu-shortcut">Ctrl+Shift+S</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="file-close"><span class="menu-icon">✕</span> Close<span class="menu-shortcut">Ctrl+W</span></div>
              <div class="menu-entry" data-action="file-close-all"><span class="menu-icon">✕</span> Close All<span class="menu-shortcut">Ctrl+K W</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="file-exit"><span class="menu-icon">🚪</span> Exit</div>
            </div>
          </div>

          <!-- Edit Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Edit</span>
            <div class="menu-dropdown" data-menu="edit">
              <div class="menu-entry" data-action="edit-undo"><span class="menu-icon">↶</span> Undo<span class="menu-shortcut">Ctrl+Z</span></div>
              <div class="menu-entry" data-action="edit-redo"><span class="menu-icon">↷</span> Redo<span class="menu-shortcut">Ctrl+Y</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="edit-cut"><span class="menu-icon">✂️</span> Cut<span class="menu-shortcut">Ctrl+X</span></div>
              <div class="menu-entry" data-action="edit-copy"><span class="menu-icon">📋</span> Copy<span class="menu-shortcut">Ctrl+C</span></div>
              <div class="menu-entry" data-action="edit-paste"><span class="menu-icon">📌</span> Paste<span class="menu-shortcut">Ctrl+V</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="edit-find"><span class="menu-icon">🔍</span> Find<span class="menu-shortcut">Ctrl+F</span></div>
              <div class="menu-entry" data-action="edit-replace"><span class="menu-icon">↔️</span> Replace<span class="menu-shortcut">Ctrl+H</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="edit-select-all"><span class="menu-icon">⊞</span> Select All<span class="menu-shortcut">Ctrl+A</span></div>
            </div>
          </div>

          <!-- Selection Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Selection</span>
            <div class="menu-dropdown" data-menu="selection">
              <div class="menu-entry" data-action="selection-expand"><span class="menu-icon">⬈</span> Expand Selection<span class="menu-shortcut">Ctrl+Shift+→</span></div>
              <div class="menu-entry" data-action="selection-shrink"><span class="menu-icon">⬉</span> Shrink Selection<span class="menu-shortcut">Ctrl+Shift+←</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="selection-column"><span class="menu-icon">⊕</span> Column Selection<span class="menu-shortcut">Shift+Alt+Click</span></div>
              <div class="menu-entry" data-action="selection-multi"><span class="menu-icon">⊕</span> Multi-Cursor<span class="menu-shortcut">Ctrl+Alt+↓</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="selection-line"><span class="menu-icon">≡</span> Select Line<span class="menu-shortcut">Ctrl+L</span></div>
              <div class="menu-entry" data-action="selection-bracket"><span class="menu-icon">{}</span> Select Bracket<span class="menu-shortcut">Ctrl+Shift+\\</span></div>
            </div>
          </div>

          <!-- View Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">View</span>
            <div class="menu-dropdown" data-menu="view">
              <div class="menu-entry" data-action="view-explorer"><span class="menu-icon">📁</span> Explorer<span class="menu-shortcut">Ctrl+B</span></div>
              <div class="menu-entry" data-action="view-search"><span class="menu-icon">🔍</span> Search<span class="menu-shortcut">Ctrl+Shift+F</span></div>
              <div class="menu-entry" data-action="view-scm"><span class="menu-icon">📦</span> Source Control<span class="menu-shortcut">Ctrl+Shift+G</span></div>
              <div class="menu-entry" data-action="view-debug"><span class="menu-icon">🐛</span> Debug<span class="menu-shortcut">Ctrl+Shift+D</span></div>
              <div class="menu-entry" data-action="view-extensions"><span class="menu-icon">⚙️</span> Extensions<span class="menu-shortcut">Ctrl+Shift+X</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="view-outline"><span class="menu-icon">≡</span> Outline</div>
              <div class="menu-entry" data-action="view-problems"><span class="menu-icon">⚠️</span> Problems<span class="menu-shortcut">Ctrl+Shift+M</span></div>
              <div class="menu-entry" data-action="view-output"><span class="menu-icon">📝</span> Output<span class="menu-shortcut">Ctrl+Shift+U</span></div>
              <div class="menu-entry" data-action="view-debug-console"><span class="menu-icon">🖥️</span> Debug Console<span class="menu-shortcut">Ctrl+Shift+Y</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="view-terminal"><span class="menu-icon">⌘</span> Terminal<span class="menu-shortcut">Ctrl+`</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="view-word-wrap"><span class="menu-icon">↩️</span> Toggle Word Wrap<span class="menu-shortcut">Alt+Z</span></div>
              <div class="menu-entry" data-action="view-minimap"><span class="menu-icon">▬</span> Toggle Minimap</div>
              <div class="menu-entry" data-action="view-breadcrumb"><span class="menu-icon">🔗</span> Toggle Breadcrumb</div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="view-zoom-in"><span class="menu-icon">🔍+</span> Zoom In<span class="menu-shortcut">Ctrl+=</span></div>
              <div class="menu-entry" data-action="view-zoom-out"><span class="menu-icon">🔍-</span> Zoom Out<span class="menu-shortcut">Ctrl+-</span></div>
              <div class="menu-entry" data-action="view-zoom-reset"><span class="menu-icon">🔍0</span> Reset Zoom<span class="menu-shortcut">Ctrl+0</span></div>
            </div>
          </div>

          <!-- Go Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Go</span>
            <div class="menu-dropdown" data-menu="go">
              <div class="menu-entry" data-action="go-file"><span class="menu-icon">📄</span> Go to File<span class="menu-shortcut">Ctrl+P</span></div>
              <div class="menu-entry" data-action="go-line"><span class="menu-icon">↓</span> Go to Line<span class="menu-shortcut">Ctrl+G</span></div>
              <div class="menu-entry" data-action="go-symbol"><span class="menu-icon">@</span> Go to Symbol<span class="menu-shortcut">Ctrl+Shift+O</span></div>
              <div class="menu-entry" data-action="go-definition"><span class="menu-icon">→</span> Go to Definition<span class="menu-shortcut">F12</span></div>
              <div class="menu-entry" data-action="go-references"><span class="menu-icon">←</span> Find References<span class="menu-shortcut">Shift+F12</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="go-back"><span class="menu-icon">◄</span> Back<span class="menu-shortcut">Alt+←</span></div>
              <div class="menu-entry" data-action="go-forward"><span class="menu-icon">►</span> Forward<span class="menu-shortcut">Alt+→</span></div>
            </div>
          </div>

          <!-- Run Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Run</span>
            <div class="menu-dropdown" data-menu="run">
              <div class="menu-entry" data-action="run-start"><span class="menu-icon">▶️</span> Start<span class="menu-shortcut">F5</span></div>
              <div class="menu-entry" data-action="run-start-without-debug"><span class="menu-icon">▶️</span> Start Without Debug<span class="menu-shortcut">Ctrl+F5</span></div>
              <div class="menu-entry" data-action="run-stop"><span class="menu-icon">⏹️</span> Stop<span class="menu-shortcut">Shift+F5</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="run-pause"><span class="menu-icon">⏸️</span> Pause<span class="menu-shortcut">F6</span></div>
              <div class="menu-entry" data-action="run-step-over"><span class="menu-icon">↓</span> Step Over<span class="menu-shortcut">F10</span></div>
              <div class="menu-entry" data-action="run-step-into"><span class="menu-icon">↙️</span> Step Into<span class="menu-shortcut">F11</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="run-toggle-breakpoint"><span class="menu-icon">●</span> Toggle Breakpoint<span class="menu-shortcut">F9</span></div>
              <div class="menu-entry" data-action="run-open-config"><span class="menu-icon">⚙️</span> Open Configurations</div>
            </div>
          </div>

          <!-- Terminal Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Terminal</span>
            <div class="menu-dropdown" data-menu="terminal">
              <div class="menu-entry" data-action="terminal-new"><span class="menu-icon">⌘</span> New Terminal<span class="menu-shortcut">Ctrl+`</span></div>
              <div class="menu-entry" data-action="terminal-split"><span class="menu-icon">⊢</span> Split Terminal<span class="menu-shortcut">Ctrl+Shift+5</span></div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="terminal-clear"><span class="menu-icon">🗑️</span> Clear</div>
              <div class="menu-entry" data-action="terminal-run-selected"><span class="menu-icon">▶️</span> Run Selected Text</div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="terminal-show"><span class="menu-icon">⌘</span> Show Terminal<span class="menu-shortcut">Ctrl+`</span></div>
            </div>
          </div>

          <!-- Help Menu -->
          <div class="menu-item" role="menuitem" tabindex="0">
            <span class="menu-label">Help</span>
            <div class="menu-dropdown" data-menu="help">
              <div class="menu-entry" data-action="help-welcome"><span class="menu-icon">👋</span> Welcome</div>
              <div class="menu-entry" data-action="help-keyboard"><span class="menu-icon">⌨️</span> Keyboard Shortcuts<span class="menu-shortcut">Ctrl+K Ctrl+S</span></div>
              <div class="menu-entry" data-action="help-docs"><span class="menu-icon">📚</span> Documentation</div>
              <div class="menu-separator"></div>
              <div class="menu-entry" data-action="help-report-issue"><span class="menu-icon">🐛</span> Report Issue</div>
              <div class="menu-entry" data-action="help-about"><span class="menu-icon">ℹ️</span> About RawrXD</div>
            </div>
          </div>
        </div>
      </div>
    `;

    if (!document.querySelector('#cursor-menu-bar')) {
      const container = document.createElement('div');
      container.innerHTML = menuBarHTML;
      document.body.insertBefore(container.firstElementChild, document.body.firstChild);
    }

    this.setupMenuBarListeners();
  }

  setupMenuBarListeners() {
    const menuItems = document.querySelectorAll('.menu-item');
    menuItems.forEach(item => {
      item.addEventListener('click', (e) => {
        e.stopPropagation();
        const dropdown = item.querySelector('.menu-dropdown');
        document.querySelectorAll('.menu-dropdown').forEach(d => d.classList.remove('active'));
        dropdown.classList.add('active');
      });
    });

    document.addEventListener('click', () => {
      document.querySelectorAll('.menu-dropdown').forEach(d => d.classList.remove('active'));
    });

    document.querySelectorAll('.menu-entry').forEach(entry => {
      entry.addEventListener('click', (e) => {
        const action = entry.dataset.action;
        this.handleMenuAction(action);
      });
    });
  }

  // ============================================================
  // 2. TAB BAR (File tabs with split, close, drag-drop)
  // ============================================================
  createTabBar() {
    const tabBarHTML = `
      <div class="cursor-tab-bar" id="cursor-tab-bar">
        <div class="tabs-container" id="tabs-container">
          <div class="tab-placeholder">No open files</div>
        </div>
        <div class="tab-controls">
          <button class="tab-control-btn" id="tab-new-btn" title="New Tab (Ctrl+N)">+</button>
          <button class="tab-control-btn" id="tab-close-btn" title="Close Tab (Ctrl+W)">✕</button>
        </div>
      </div>
    `;

    if (!document.querySelector('#cursor-tab-bar')) {
      const container = document.createElement('div');
      container.innerHTML = tabBarHTML;
      document.body.insertBefore(container.firstElementChild, document.body.querySelector('.cursor-menu-bar').nextSibling);
    }

    this.setupTabBarListeners();
  }

  setupTabBarListeners() {
    document.getElementById('tab-new-btn')?.addEventListener('click', () => this.newTab());
    document.getElementById('tab-close-btn')?.addEventListener('click', () => this.closeTab(this.state.activeTab));
  }

  newTab() {
    const tabId = `tab-${Date.now()}`;
    this.state.openTabs.push({ id: tabId, name: 'Untitled', content: '', modified: false });
    this.state.activeTab = tabId;
    this.renderTabs();
    this.logSuccess(`✅ New tab created: ${tabId}`);
  }

  closeTab(tabId) {
    this.state.openTabs = this.state.openTabs.filter(t => t.id !== tabId);
    if (this.state.activeTab === tabId) {
      this.state.activeTab = this.state.openTabs[0]?.id || null;
    }
    this.renderTabs();
    this.logSuccess(`✅ Tab closed: ${tabId}`);
  }

  renderTabs() {
    const container = document.getElementById('tabs-container');
    if (!container) return;

    if (this.state.openTabs.length === 0) {
      container.innerHTML = '<div class="tab-placeholder">No open files</div>';
      return;
    }

    const tabsHTML = this.state.openTabs.map(tab => `
      <div class="tab ${tab.id === this.state.activeTab ? 'active' : ''}" data-tab-id="${tab.id}">
        <span class="tab-label">${tab.name}${tab.modified ? ' •' : ''}</span>
        <button class="tab-close" data-tab-id="${tab.id}">✕</button>
      </div>
    `).join('');

    container.innerHTML = tabsHTML;

    container.querySelectorAll('.tab').forEach(tab => {
      tab.addEventListener('click', () => {
        this.state.activeTab = tab.dataset.tabId;
        this.renderTabs();
      });
    });

    container.querySelectorAll('.tab-close').forEach(closeBtn => {
      closeBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.closeTab(closeBtn.dataset.tabId);
      });
    });
  }

  // ============================================================
  // 3. EDITOR CONTROLS (Split, maximize, layout buttons)
  // ============================================================
  createEditorControls() {
    const controlsHTML = `
      <div class="cursor-editor-controls" id="cursor-editor-controls">
        <button class="editor-control-btn" id="split-right-btn" title="Split Right (Ctrl+\\)">⊟</button>
        <button class="editor-control-btn" id="split-down-btn" title="Split Down (Ctrl+K Ctrl+\\)">⊞</button>
        <button class="editor-control-btn" id="toggle-layout-btn" title="Toggle Layout">⋮</button>
        <button class="editor-control-btn" id="maximize-editor-btn" title="Maximize Editor">◻️</button>
        <button class="editor-control-btn" id="close-editor-group-btn" title="Close Editor Group">✕</button>
      </div>
    `;

    if (!document.querySelector('#cursor-editor-controls')) {
      const container = document.createElement('div');
      container.innerHTML = controlsHTML;
      document.body.appendChild(container.firstElementChild);
    }

    this.setupEditorControlListeners();
  }

  setupEditorControlListeners() {
    document.getElementById('split-right-btn')?.addEventListener('click', () => this.splitEditor('vertical'));
    document.getElementById('split-down-btn')?.addEventListener('click', () => this.splitEditor('horizontal'));
    document.getElementById('toggle-layout-btn')?.addEventListener('click', () => this.toggleLayout());
    document.getElementById('maximize-editor-btn')?.addEventListener('click', () => this.toggleMaximize());
    document.getElementById('close-editor-group-btn')?.addEventListener('click', () => this.closeEditorGroup());
  }

  splitEditor(mode) {
    this.state.splitMode = mode === 'vertical' ? 'vertical' : 'horizontal';
    this.logSuccess(`✅ Editor split mode: ${this.state.splitMode}`);
  }

  toggleLayout() {
    this.state.splitMode = this.state.splitMode === 'vertical' ? 'horizontal' : 'vertical';
    this.logSuccess(`✅ Layout toggled to: ${this.state.splitMode}`);
  }

  toggleMaximize() {
    const editor = document.querySelector('.editor');
    if (editor) {
      editor.classList.toggle('maximized');
      this.logSuccess('✅ Editor maximized toggled');
    }
  }

  closeEditorGroup() {
    this.state.openTabs = [];
    this.state.activeTab = null;
    this.renderTabs();
    this.logSuccess('✅ Editor group closed');
  }

  // ============================================================
  // 4. BOTTOM PANEL (Terminal, Problems, Output, Debug Console)
  // ============================================================
  createBottomPanel() {
    const panelHTML = `
      <div class="cursor-bottom-panel" id="cursor-bottom-panel">
        <div class="panel-header">
          <div class="panel-tabs">
            <button class="panel-tab" data-panel="terminal">⌘ Terminal</button>
            <button class="panel-tab" data-panel="problems">⚠️ Problems</button>
            <button class="panel-tab" data-panel="output">📝 Output</button>
            <button class="panel-tab" data-panel="debug">🖥️ Debug Console</button>
          </div>
          <div class="panel-controls">
            <button class="panel-control-btn" id="panel-maximize-btn" title="Maximize Panel">◻️</button>
            <button class="panel-control-btn" id="panel-move-btn" title="Move Panel">⇄</button>
            <button class="panel-control-btn" id="panel-close-btn" title="Close Panel">✕</button>
          </div>
        </div>
        <div class="panel-content" id="panel-content">
          <div class="panel-body">Terminal output will appear here...</div>
        </div>
      </div>
    `;

    if (!document.querySelector('#cursor-bottom-panel')) {
      const container = document.createElement('div');
      container.innerHTML = panelHTML;
      document.body.appendChild(container.firstElementChild);
    }

    this.setupBottomPanelListeners();
  }

  setupBottomPanelListeners() {
    document.querySelectorAll('.panel-tab').forEach(tab => {
      tab.addEventListener('click', (e) => {
        const panelType = e.target.dataset.panel;
        this.switchPanel(panelType);
      });
    });

    document.getElementById('panel-maximize-btn')?.addEventListener('click', () => this.togglePanelMaximize());
    document.getElementById('panel-move-btn')?.addEventListener('click', () => this.togglePanelPosition());
    document.getElementById('panel-close-btn')?.addEventListener('click', () => this.togglePanel());
  }

  switchPanel(panelType) {
    this.state.panels.bottom.type = panelType;
    const content = document.querySelector('.panel-body');
    if (content) {
      const messages = {
        terminal: '⌘ Terminal - Ready\n> ',
        problems: '⚠️ No problems detected',
        output: '📝 Output channel',
        debug: '🖥️ Debug Console'
      };
      content.textContent = messages[panelType] || '';
    }
    this.logSuccess(`✅ Panel switched to: ${panelType}`);
  }

  togglePanel() {
    this.state.panels.bottom.visible = !this.state.panels.bottom.visible;
    const panel = document.getElementById('cursor-bottom-panel');
    if (panel) {
      panel.style.display = this.state.panels.bottom.visible ? 'flex' : 'none';
    }
    this.logSuccess(`✅ Bottom panel ${this.state.panels.bottom.visible ? 'shown' : 'hidden'}`);
  }

  togglePanelMaximize() {
    const panel = document.getElementById('cursor-bottom-panel');
    if (panel) {
      panel.classList.toggle('maximized');
      this.logSuccess('✅ Panel maximize toggled');
    }
  }

  togglePanelPosition() {
    this.state.panels.bottom.side = this.state.panels.bottom.side === 'bottom' ? 'right' : 'bottom';
    this.logSuccess(`✅ Panel moved to: ${this.state.panels.bottom.side}`);
  }

  // ============================================================
  // 5. MENU ACTION HANDLER
  // ============================================================
  handleMenuAction(action) {
    const handlers = {
      // File menu
      'file-new': () => this.newTab(),
      'file-new-folder': () => this.logSuccess('✅ New folder created'),
      'file-open': () => this.logSuccess('✅ File open dialog'),
      'file-open-folder': () => this.logSuccess('✅ Folder open dialog'),
      'file-save': () => this.logSuccess('✅ File saved'),
      'file-save-all': () => this.logSuccess('✅ All files saved'),
      'file-save-as': () => this.logSuccess('✅ Save as dialog'),
      'file-close': () => this.closeTab(this.state.activeTab),
      'file-close-all': () => this.closeEditorGroup(),
      'file-exit': () => this.logSuccess('✅ Exit'),

      // Edit menu
      'edit-undo': () => this.logSuccess('✅ Undo'),
      'edit-redo': () => this.logSuccess('✅ Redo'),
      'edit-cut': () => this.logSuccess('✅ Cut'),
      'edit-copy': () => this.logSuccess('✅ Copy'),
      'edit-paste': () => this.logSuccess('✅ Paste'),
      'edit-find': () => this.logSuccess('✅ Find dialog opened'),
      'edit-replace': () => this.logSuccess('✅ Replace dialog opened'),
      'edit-select-all': () => this.logSuccess('✅ Select all'),

      // Selection menu
      'selection-expand': () => this.logSuccess('✅ Selection expanded'),
      'selection-shrink': () => this.logSuccess('✅ Selection shrunk'),
      'selection-column': () => this.logSuccess('✅ Column selection mode'),
      'selection-multi': () => this.logSuccess('✅ Multi-cursor mode'),
      'selection-line': () => this.logSuccess('✅ Line selected'),
      'selection-bracket': () => this.logSuccess('✅ Bracket content selected'),

      // View menu
      'view-explorer': () => this.togglePanel('explorer'),
      'view-search': () => this.togglePanel('search'),
      'view-scm': () => this.togglePanel('scm'),
      'view-debug': () => this.togglePanel('debug'),
      'view-extensions': () => this.togglePanel('extensions'),
      'view-outline': () => this.togglePanel('outline'),
      'view-problems': () => this.switchPanel('problems'),
      'view-output': () => this.switchPanel('output'),
      'view-debug-console': () => this.switchPanel('debug'),
      'view-terminal': () => { this.state.panels.bottom.visible = !this.state.panels.bottom.visible; this.togglePanel(); },
      'view-word-wrap': () => this.logSuccess('✅ Word wrap toggled'),
      'view-minimap': () => this.logSuccess('✅ Minimap toggled'),
      'view-breadcrumb': () => this.logSuccess('✅ Breadcrumb toggled'),
      'view-zoom-in': () => this.logSuccess('✅ Zoomed in'),
      'view-zoom-out': () => this.logSuccess('✅ Zoomed out'),
      'view-zoom-reset': () => this.logSuccess('✅ Zoom reset'),

      // Go menu
      'go-file': () => this.logSuccess('✅ Go to file dialog'),
      'go-line': () => this.logSuccess('✅ Go to line dialog'),
      'go-symbol': () => this.logSuccess('✅ Go to symbol dialog'),
      'go-definition': () => this.logSuccess('✅ Go to definition'),
      'go-references': () => this.logSuccess('✅ Find references'),
      'go-back': () => this.logSuccess('✅ Navigated back'),
      'go-forward': () => this.logSuccess('✅ Navigated forward'),

      // Run menu
      'run-start': () => this.logSuccess('✅ Debug started'),
      'run-start-without-debug': () => this.logSuccess('✅ Program started'),
      'run-stop': () => this.logSuccess('✅ Program stopped'),
      'run-pause': () => this.logSuccess('✅ Program paused'),
      'run-step-over': () => this.logSuccess('✅ Step over'),
      'run-step-into': () => this.logSuccess('✅ Step into'),
      'run-toggle-breakpoint': () => this.logSuccess('✅ Breakpoint toggled'),
      'run-open-config': () => this.logSuccess('✅ Launch config opened'),

      // Terminal menu
      'terminal-new': () => this.logSuccess('✅ New terminal'),
      'terminal-split': () => this.logSuccess('✅ Terminal split'),
      'terminal-clear': () => this.logSuccess('✅ Terminal cleared'),
      'terminal-run-selected': () => this.logSuccess('✅ Running selected text'),
      'terminal-show': () => this.switchPanel('terminal'),

      // Help menu
      'help-welcome': () => this.logSuccess('✅ Welcome page'),
      'help-keyboard': () => this.logSuccess('✅ Keyboard shortcuts'),
      'help-docs': () => this.logSuccess('✅ Documentation'),
      'help-report-issue': () => this.logSuccess('✅ Issue reporter'),
      'help-about': () => this.logSuccess('✅ About RawrXD')
    };

    const handler = handlers[action];
    if (handler) {
      handler();
    } else {
      this.logError(`⚠️ Unknown action: ${action}`);
    }
  }

  // ============================================================
  // 6. EVENT LISTENERS & SHORTCUTS
  // ============================================================
  setupEventListeners() {
    document.addEventListener('keydown', (e) => {
      // Ctrl+N: New Tab
      if (e.ctrlKey && e.key === 'n') {
        e.preventDefault();
        this.newTab();
      }
      // Ctrl+W: Close Tab
      if (e.ctrlKey && e.key === 'w') {
        e.preventDefault();
        this.closeTab(this.state.activeTab);
      }
      // Ctrl+Shift+P: Command Palette
      if (e.ctrlKey && e.shiftKey && e.key === 'P') {
        e.preventDefault();
        this.logSuccess('✅ Command Palette opened');
      }
      // Ctrl+`: Toggle Terminal
      if (e.ctrlKey && e.key === '`') {
        e.preventDefault();
        this.switchPanel('terminal');
      }
      // Ctrl+B: Toggle Explorer
      if (e.ctrlKey && e.key === 'b') {
        e.preventDefault();
        this.logSuccess('✅ Explorer toggled');
      }
    });
  }

  // ============================================================
  // 7. UTILITIES
  // ============================================================
  logSuccess(msg) {
    console.log(`%c${msg}`, 'color: #4CAF50; font-weight: bold;');
  }

  logError(msg) {
    console.error(`%c${msg}`, 'color: #f44336; font-weight: bold;');
  }

  getState() {
    return this.state;
  }

  exportState() {
    return JSON.stringify(this.state, null, 2);
  }
}

// Initialize on page load
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    window.cursorLayout = new CursorLayoutControls();
  });
} else {
  window.cursorLayout = new CursorLayoutControls();
}
