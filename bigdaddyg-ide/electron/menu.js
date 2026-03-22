const { Menu, dialog } = require('electron');

const DEFAULT_ACCEL = {
  save: 'CmdOrCtrl+S',
  openProject: 'CmdOrCtrl+O',
  commandPalette: 'CmdOrCtrl+Shift+P',
  settings: 'CmdOrCtrl+,',
  chat: 'CmdOrCtrl+L',
  agent: 'CmdOrCtrl+Shift+A',
  modules: 'CmdOrCtrl+Shift+M',
  symbols: 'CmdOrCtrl+Shift+Y',
  models: 'CmdOrCtrl+Shift+G',
  sidebarToggle: 'CmdOrCtrl+B'
};

/**
 * @param {Electron.BrowserWindow | null} mainWindow
 * @param {(root: string) => void} setProjectRoot
 * @param {Partial<typeof DEFAULT_ACCEL>} [accelerators] merged over defaults (from renderer sync)
 */
function setApplicationMenu(mainWindow, setProjectRoot, accelerators = {}) {
  // Forwards menu choices to renderer (ide:action); does not run agent/IRC or touch disk by itself.
  const sendIde = (action) => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      mainWindow.webContents.send('ide:action', action);
    }
  };

  const acc = { ...DEFAULT_ACCEL, ...accelerators };

  const template = [
    {
      label: 'File',
      submenu: [
        {
          label: 'Open Project...',
          accelerator: acc.openProject || DEFAULT_ACCEL.openProject,
          click: async () => {
            if (!mainWindow || mainWindow.isDestroyed()) return;
            const result = await dialog.showOpenDialog(mainWindow, {
              properties: ['openDirectory'],
              title: 'Open Project Folder'
            });
            if (!result.canceled && result.filePaths && result.filePaths[0]) {
              const root = result.filePaths[0];
              if (typeof setProjectRoot === 'function') setProjectRoot(root);
              // Renderer must handle project:opened for UI; main IPC projectRoot used for fs:* — keep both in sync.
              mainWindow.webContents.send('project:opened', root);
            }
          }
        },
        {
          label: 'Save',
          accelerator: acc.save || DEFAULT_ACCEL.save,
          click: () => sendIde('save')
        },
        { type: 'separator' },
        { role: 'quit', label: 'Quit' }
      ]
    },
    {
      label: 'Edit',
      submenu: [
        { role: 'undo' },
        { role: 'redo' },
        { type: 'separator' },
        { role: 'cut' },
        { role: 'copy' },
        { role: 'paste' },
        { role: 'selectAll' }
      ]
    },
    {
      label: 'View',
      submenu: [
        {
          label: 'Command Palette',
          accelerator: acc.commandPalette,
          click: () => sendIde('command-palette')
        },
        {
          label: 'Settings',
          accelerator: acc.settings,
          click: () => sendIde('settings')
        },
        { type: 'separator' },
        {
          label: 'Toggle project sidebar',
          accelerator: acc.sidebarToggle,
          click: () => sendIde('sidebar-toggle')
        },
        { type: 'separator' },
        {
          label: 'AI Chat',
          accelerator: acc.chat,
          click: () => sendIde('chat')
        },
        {
          label: 'Agent Panel',
          accelerator: acc.agent,
          click: () => sendIde('agent')
        },
        {
          label: 'Modules',
          accelerator: acc.modules,
          click: () => sendIde('modules')
        },
        {
          label: 'Symbols & xrefs',
          accelerator: acc.symbols,
          click: () => sendIde('symbols')
        },
        {
          label: 'GGUF streamer',
          accelerator: acc.models,
          click: () => sendIde('models')
        },
        { type: 'separator' },
        { role: 'reload' },
        { role: 'forceReload' },
        { role: 'toggleDevTools', label: 'Toggle Developer Tools' },
        { type: 'separator' },
        { role: 'resetZoom' },
        { role: 'zoomIn' },
        { role: 'zoomOut' },
        { type: 'separator' },
        { role: 'togglefullscreen' }
      ]
    },
    {
      label: 'Help',
      submenu: [
        {
          label: 'About BigDaddyG IDE',
          click: () => {
            if (mainWindow && !mainWindow.isDestroyed()) {
              dialog.showMessageBox(mainWindow, {
                type: 'info',
                title: 'BigDaddyG IDE',
                message: 'BigDaddyG IDE',
                detail: 'RawrXD shell — WASM-local or Ollama-backed chat (Settings › AI). Version 2.0.0'
              });
            }
          }
        }
      ]
    }
  ];

  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}

module.exports = { setApplicationMenu, DEFAULT_ACCEL };
