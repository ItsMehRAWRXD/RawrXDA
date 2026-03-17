/**
 * BigDaddyG IDE - Command Palette
 * Ctrl+Shift+P to access all features
 * VS Code-style fuzzy matching command search
 */

class CommandPalette {
  constructor() {
    this.commands = [];
    this.isOpen = false;
    this.selectedIndex = 0;
    this.filteredCommands = [];

    this.createUI();
    this.registerCommands();
    this.setupKeyboardShortcuts();

    console.log('[CommandPalette] ✅ Initialized with', this.commands.length, 'commands');
  }

  createUI() {
    // Modal overlay
    this.modal = document.createElement('div');
    this.modal.id = 'command-palette-modal';
    this.modal.style.cssText = `
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: rgba(0, 0, 0, 0.8);
      backdrop-filter: blur(5px);
      z-index: 99999;
      display: none;
      align-items: flex-start;
      justify-content: center;
      padding-top: 100px;
    `;

    // Command palette container
    const container = document.createElement('div');
    container.style.cssText = `
      background: #0a0a1e;
      border: 2px solid #00d4ff;
      border-radius: 12px;
      width: 600px;
      max-height: 500px;
      box-shadow: 0 10px 40px rgba(0, 212, 255, 0.3);
      overflow: hidden;
      display: flex;
      flex-direction: column;
    `;

    // Search input
    this.searchInput = document.createElement('input');
    this.searchInput.type = 'text';
    this.searchInput.placeholder = 'Type a command or search...';
    this.searchInput.style.cssText = `
      padding: 15px 20px;
      background: rgba(0, 0, 0, 0.3);
      border: none;
      border-bottom: 1px solid rgba(0, 212, 255, 0.3);
      color: #00d4ff;
      font-size: 16px;
      font-family: 'Segoe UI', sans-serif;
      outline: none;
    `;

    this.searchInput.addEventListener('input', () => this.filterCommands());
    this.searchInput.addEventListener('keydown', (e) => this.handleKeydown(e));

    // Results list
    this.resultsList = document.createElement('div');
    this.resultsList.style.cssText = `
      overflow-y: auto;
      max-height: 400px;
    `;

    container.appendChild(this.searchInput);
    container.appendChild(this.resultsList);
    this.modal.appendChild(container);
    document.body.appendChild(this.modal);

    // Close on overlay click
    this.modal.addEventListener('click', (e) => {
      if (e.target === this.modal) this.close();
    });
  }

  registerCommands() {
    // File operations
    this.addCommand('File: New File', 'Ctrl+N', '📄', 'Create a new file', () => {
      console.log('[Command] New File');
      // Integrate with file system
    });

    this.addCommand('File: Open File', 'Ctrl+O', '📂', 'Open an existing file', () => {
      console.log('[Command] Open File');
    });

    this.addCommand('File: Save', 'Ctrl+S', '💾', 'Save current file', () => {
      console.log('[Command] Save');
    });

    // API Key Management
    this.addCommand('API Keys: Manage Keys', 'Ctrl+K', '🔐', 'Open API key settings panel', () => {
      window.apiKeySettingsPanel?.open();
    });

    this.addCommand('API Keys: Test Connection', '', '🧪', 'Test all configured API keys', () => {
      this.testAllAPIKeys();
    });

    this.addCommand('API Keys: View Configured', '', '📋', 'List all configured providers', () => {
      this.showConfiguredProviders();
    });

    // Model Management
    this.addCommand('Model: Switch Model', 'Ctrl+M', '🔄', 'Open model browser and switch models', () => {
      if (window.modelBrowser) {
        window.modelBrowser.open();
      } else {
        alert('Model browser not initialized');
      }
    });

    this.addCommand('Model: Hot Reload Model', 'Ctrl+Shift+R', '♻️', 'Hot reload current model', () => {
      console.log('[Command] Hot reload model');
    });

    this.addCommand('Model: Load Local Model', '', '📦', 'Load a local GGUF model', () => {
      console.log('[Command] Load local model');
    });

    // Swarm Engine
    this.addCommand('Swarm: Initialize Swarm', '', '🐝', 'Initialize 200 mini-agents', async () => {
      if (!window.swarmEngine) {
        window.swarmEngine = new SwarmEngine();
      }
      await window.swarmEngine.initSwarm(200);
      alert('✅ Swarm initialized: 200 agents ready');
    });

    this.addCommand('Swarm: Show Visualizer', '', '📊', 'Show swarm activity visualizer', () => {
      showSwarmEngine();
    });

    this.addCommand('Swarm: Execute Task', '', '⚡', 'Execute a task with the swarm', async () => {
      if (!window.swarmEngine) {
        alert('Initialize swarm first');
        return;
      }

      const code = prompt('Enter code to analyze:');
      if (code) {
        const result = await window.swarmEngine.executeTask({
          description: 'Analyze and optimize code',
          code: code
        }, 50);

        console.log('[Swarm] Result:', result);
        alert(`✅ Task completed by ${result.agentsUsed} agents in ${result.elapsed}ms`);
      }
    });

    this.addCommand('Swarm: Shutdown', '', '🛑', 'Shutdown all swarm agents', () => {
      window.swarmEngine?.shutdown();
      alert('🛑 Swarm shut down');
    });

    // System Optimization
    this.addCommand('System: Show Optimizer', '', '⚙️', 'Open system optimizer panel', () => {
      showSystemOptimizer();
    });

    this.addCommand('System: Benchmark Performance', '', '⏱️', 'Run system benchmark', () => {
      if (window.visualBenchmark) {
        window.visualBenchmark.runBenchmark();
      }
    });

    this.addCommand('System: Enable Turbo Mode', 'Ctrl+Shift+T', '🚀', 'Enable maximum performance', () => {
      console.log('[Command] Turbo mode enabled');
    });

    // AI & Chat
    this.addCommand('AI: Open Chat Panel', 'Ctrl+Shift+A', '💬', 'Open AI chat sidebar', () => {
      const sidebar = document.getElementById('right-sidebar');
      if (sidebar) {
        sidebar.classList.remove('collapsed');
      }
    });

    this.addCommand('AI: Clear Chat', '', '🗑️', 'Clear all chat messages', () => {
      clearChat();
    });

    this.addCommand('AI: Voice Input', '', '🎤', 'Start voice coding', () => {
      startVoiceInput();
    });

    // Theme & UI
    this.addCommand('Theme: Change Theme', 'Ctrl+T', '🎨', 'Open theme selector', () => {
      if (window.chameleonTheme) {
        window.chameleonTheme.cycleTheme();
      }
    });

    this.addCommand('Theme: Random Theme', '', '🎲', 'Apply random theme', () => {
      if (window.chameleonTheme) {
        window.chameleonTheme.randomizeTheme();
      }
    });

    this.addCommand('UI: Toggle Sidebar', 'Ctrl+B', '⬅️', 'Toggle left sidebar visibility', () => {
      const sidebar = document.getElementById('sidebar');
      if (sidebar) {
        sidebar.classList.toggle('collapsed');
      }
    });

    this.addCommand('UI: Toggle Terminal', 'Ctrl+`', '📟', 'Toggle bottom terminal panel', () => {
      const terminal = document.getElementById('bottom-panel');
      if (terminal) {
        terminal.classList.toggle('collapsed');
      }
    });

    // Advanced Features
    this.addCommand('Test: Run Agentic Test', '', '🧪', 'Run agentic comparison test', () => {
      runAgenticTest();
    });

    this.addCommand('View: Show Console', 'F12', '🖥️', 'Open developer console', () => {
      if (window.electron) {
        window.electron.ipcRenderer.send('toggle-devtools');
      }
    });

    this.addCommand('Help: Keyboard Shortcuts', 'Ctrl+/', '⌨️', 'Show all keyboard shortcuts', () => {
      this.showKeyboardShortcuts();
    });

    this.addCommand('Help: About', '', 'ℹ️', 'About BigDaddyG IDE', () => {
      alert('BigDaddyG IDE v1.0.0\\n\\nAdvanced AI-powered IDE with:\\n• 200 mini-agents\\n• Multi-provider API support\\n• Hot model swapping\\n• System optimization\\n\\nBuilt with Electron & Monaco Editor');
    });

    // Sort commands alphabetically
    this.commands.sort((a, b) => a.name.localeCompare(b.name));
    this.filteredCommands = [...this.commands];
  }

  addCommand(name, shortcut, icon, description, action) {
    this.commands.push({
      name,
      shortcut,
      icon,
      description,
      action,
      searchText: `${name} ${description}`.toLowerCase()
    });
  }

  setupKeyboardShortcuts() {
    document.addEventListener('keydown', (e) => {
      // Ctrl+Shift+P - Open command palette
      if (e.ctrlKey && e.shiftKey && e.key === 'P') {
        e.preventDefault();
        this.toggle();
      }

      // Escape - Close
      if (e.key === 'Escape' && this.isOpen) {
        e.preventDefault();
        this.close();
      }
    });
  }

  handleKeydown(e) {
    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        this.selectedIndex = Math.min(this.selectedIndex + 1, this.filteredCommands.length - 1);
        this.renderResults();
        this.scrollToSelected();
        break;

      case 'ArrowUp':
        e.preventDefault();
        this.selectedIndex = Math.max(this.selectedIndex - 1, 0);
        this.renderResults();
        this.scrollToSelected();
        break;

      case 'Enter':
        e.preventDefault();
        this.executeSelected();
        break;

      case 'Escape':
        e.preventDefault();
        this.close();
        break;
    }
  }

  filterCommands() {
    const query = this.searchInput.value.toLowerCase().trim();

    if (!query) {
      this.filteredCommands = [...this.commands];
    } else {
      // Fuzzy matching
      this.filteredCommands = this.commands.filter(cmd => {
        return cmd.searchText.includes(query) || this.fuzzyMatch(query, cmd.name.toLowerCase());
      });
    }

    this.selectedIndex = 0;
    this.renderResults();
  }

  fuzzyMatch(query, text) {
    let queryIndex = 0;
    for (let i = 0; i < text.length && queryIndex < query.length; i++) {
      if (text[i] === query[queryIndex]) {
        queryIndex++;
      }
    }
    return queryIndex === query.length;
  }

  renderResults() {
    this.resultsList.innerHTML = '';

    if (this.filteredCommands.length === 0) {
      this.resultsList.innerHTML = `
        <div style="padding: 40px; text-align: center; color: #888;">
          <div style="font-size: 48px;">🔍</div>
          <div style="margin-top: 10px;">No commands found</div>
        </div>
      `;
      return;
    }

    this.filteredCommands.forEach((cmd, index) => {
      const item = document.createElement('div');
      item.style.cssText = `
        padding: 12px 20px;
        cursor: pointer;
        display: flex;
        align-items: center;
        gap: 15px;
        transition: background 0.2s;
        background: ${index === this.selectedIndex ? 'rgba(0, 212, 255, 0.2)' : 'transparent'};
        border-left: 3px solid ${index === this.selectedIndex ? '#00d4ff' : 'transparent'};
      `;

      item.innerHTML = `
        <span style="font-size: 20px;">${cmd.icon}</span>
        <div style="flex: 1;">
          <div style="color: #00d4ff; font-weight: bold;">${this.highlightMatch(cmd.name)}</div>
          <div style="color: #888; font-size: 12px; margin-top: 2px;">${cmd.description}</div>
        </div>
        ${cmd.shortcut ? `<div style="color: #888; font-size: 12px; background: rgba(0, 212, 255, 0.1); padding: 4px 8px; border-radius: 4px;">${cmd.shortcut}</div>` : ''}
      `;

      item.addEventListener('mouseenter', () => {
        this.selectedIndex = index;
        this.renderResults();
      });

      item.addEventListener('click', () => {
        this.executeSelected();
      });

      this.resultsList.appendChild(item);
    });
  }

  highlightMatch(text) {
    const query = this.searchInput.value.toLowerCase();
    if (!query) return text;

    const regex = new RegExp(`(${query})`, 'gi');
    return text.replace(regex, '<span style="background: rgba(0, 212, 255, 0.3); padding: 0 2px; border-radius: 2px;">$1</span>');
  }

  scrollToSelected() {
    const items = this.resultsList.children;
    if (items[this.selectedIndex]) {
      items[this.selectedIndex].scrollIntoView({
        behavior: 'smooth',
        block: 'nearest'
      });
    }
  }

  executeSelected() {
    if (this.filteredCommands[this.selectedIndex]) {
      const cmd = this.filteredCommands[this.selectedIndex];
      console.log('[CommandPalette] Executing:', cmd.name);

      try {
        cmd.action();
        this.close();
      } catch (error) {
        console.error('[CommandPalette] Error executing command:', error);
        alert(`❌ Error: ${error.message}`);
      }
    }
  }

  testAllAPIKeys() {
    const configured = window.apiKeyManager?.getConfiguredProviders() || [];
    if (configured.length === 0) {
      alert('No API keys configured. Open API key settings to add keys.');
      return;
    }

    alert(`Testing ${configured.length} configured providers:\\n\\n${configured.join('\\n')}\\n\\nOpen API Key Settings to test connections.`);
    window.apiKeySettingsPanel?.open();
  }

  showConfiguredProviders() {
    const configured = window.apiKeyManager?.getConfiguredProviders() || [];
    if (configured.length === 0) {
      alert('No API keys configured yet.\\n\\nUse "API Keys: Manage Keys" to add your keys.');
      return;
    }

    const list = configured.map(p => `✅ ${p}`).join('\\n');
    alert(`Configured Providers (${configured.length}):\\n\\n${list}`);
  }

  showKeyboardShortcuts() {
    const shortcuts = this.commands
      .filter(cmd => cmd.shortcut)
      .map(cmd => `${cmd.icon} ${cmd.name.padEnd(40)} ${cmd.shortcut}`)
      .join('\\n');

    alert(`Keyboard Shortcuts:\\n\\n${shortcuts}`);
  }

  open() {
    this.isOpen = true;
    this.modal.style.display = 'flex';
    this.searchInput.value = '';
    this.filteredCommands = [...this.commands];
    this.selectedIndex = 0;
    this.renderResults();

    // Focus search input
    setTimeout(() => this.searchInput.focus(), 100);
  }

  close() {
    this.isOpen = false;
    this.modal.style.display = 'none';
  }

  toggle() {
    this.isOpen ? this.close() : this.open();
  }
}

// Initialize command palette on page load
if (typeof window !== 'undefined') {
  window.addEventListener('DOMContentLoaded', () => {
    window.commandPalette = new CommandPalette();
    console.log('[CommandPalette] ✅ Ready - Press Ctrl+Shift+P to open');
  });
}
