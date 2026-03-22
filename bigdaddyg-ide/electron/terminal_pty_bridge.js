/**
 * Integrated terminal — belongs in the main process (node-pty), not the renderer.
 * Wired from preload as electronAPI.terminal* / onTerminal*.
 */
'use strict';

const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

let nextSessionId = 1;
/** @type {Map<string, { pty: import('node-pty').IPty, sender: import('electron').WebContents }>} */
const sessions = new Map();

function homedir() {
  try {
    return os.homedir();
  } catch {
    return process.cwd();
  }
}

/**
 * @param {string | undefined} shell
 * @param {NodeJS.Platform} platform
 */
function resolveShellCommand(shell, platform) {
  const s = String(shell || 'powershell').toLowerCase();
  if (platform === 'win32') {
    if (s === 'cmd' || s === 'command') {
      return { file: process.env.ComSpec || 'cmd.exe', args: [] };
    }
    if (s === 'pwsh') {
      return { file: 'pwsh.exe', args: ['-NoLogo'] };
    }
    const ps =
      process.env.SystemRoot != null
        ? path.join(process.env.SystemRoot, 'System32', 'WindowsPowerShell', 'v1.0', 'powershell.exe')
        : 'powershell.exe';
    return { file: ps, args: ['-NoLogo'] };
  }
  if (s === 'zsh') {
    return { file: '/bin/zsh', args: ['-l'] };
  }
  return { file: '/bin/bash', args: ['-l'] };
}

/**
 * @param {import('electron').IpcMain} ipcMain
 * @param {() => string | null} getProjectRoot
 */
function registerTerminalPtyIpc(ipcMain, getProjectRoot) {
  ipcMain.handle('terminal:create', async (event, opts) => {
    let ptyMod;
    try {
      // eslint-disable-next-line global-require, import/no-extraneous-dependencies
      ptyMod = require('node-pty');
    } catch (e) {
      return {
        success: false,
        error: `node-pty unavailable: ${e.message}. From bigdaddyg-ide run: npm install && npm run rebuild:pty`
      };
    }

    const root = typeof getProjectRoot === 'function' ? getProjectRoot() : null;
    const cwd = root && String(root).trim() ? root : homedir();
    const shell = opts && opts.shell;
    const cols = Math.max(20, Math.min(200, Number(opts?.cols) || 80));
    const rows = Math.max(5, Math.min(100, Number(opts?.rows) || 24));
    const { file, args } = resolveShellCommand(shell, process.platform);

    let pty;
    try {
      pty = ptyMod.spawn(file, args, {
        name: 'xterm-256color',
        cols,
        rows,
        cwd,
        env: { ...process.env, TERM: 'xterm-256color' }
      });
    } catch (e) {
      return { success: false, error: e.message || String(e) };
    }

    const id = String(nextSessionId++);
    const sender = event.sender;
    sessions.set(id, { pty, sender });

    pty.onData((data) => {
      if (!sender.isDestroyed()) {
        sender.send('terminal:data', id, data);
      }
    });
    pty.onExit(({ exitCode }) => {
      sessions.delete(id);
      if (!sender.isDestroyed()) {
        sender.send('terminal:exit', id, exitCode == null ? 0 : exitCode);
      }
    });

    return {
      success: true,
      id,
      shell: shell || (process.platform === 'win32' ? 'powershell' : 'bash'),
      cwd
    };
  });

  ipcMain.handle('terminal:write', async (_, id, data) => {
    const s = sessions.get(String(id));
    if (!s) {
      return { success: false, error: 'Unknown terminal session' };
    }
    try {
      s.pty.write(typeof data === 'string' ? data : String(data));
      return { success: true };
    } catch (e) {
      return { success: false, error: e.message || String(e) };
    }
  });

  ipcMain.handle('terminal:resize', async (_, id, cols, rows) => {
    const s = sessions.get(String(id));
    if (!s) {
      return { success: false, error: 'Unknown terminal session' };
    }
    try {
      s.pty.resize(
        Math.max(20, Math.min(200, Number(cols) || 80)),
        Math.max(5, Math.min(100, Number(rows) || 24))
      );
      return { success: true };
    } catch (e) {
      return { success: false, error: e.message || String(e) };
    }
  });

  ipcMain.handle('terminal:kill', async (_, id) => {
    const s = sessions.get(String(id));
    if (!s) {
      return { success: true };
    }
    try {
      s.pty.kill();
    } catch {
      /* ignore */
    }
    sessions.delete(String(id));
    return { success: true };
  });

  /**
   * Separate elevated window (UAC) — not in-panel PTY.
   */
  ipcMain.handle('terminal:open-elevated', async (_, opts) => {
    const kind = opts && opts.shell;
    if (process.platform !== 'win32') {
      return { success: false, error: 'Elevated console is only supported on Windows.' };
    }
    try {
      if (kind === 'cmd') {
        spawn(
          'powershell.exe',
          ['-NoLogo', '-WindowStyle', 'Hidden', '-Command', 'Start-Process cmd -Verb runAs'],
          { detached: true, stdio: 'ignore' }
        ).unref();
      } else {
        spawn(
          'powershell.exe',
          ['-NoLogo', '-WindowStyle', 'Hidden', '-Command', 'Start-Process powershell -Verb runAs'],
          { detached: true, stdio: 'ignore' }
        ).unref();
      }
      return {
        success: true,
        message: 'If UAC prompts, approve to open an elevated console outside the IDE.'
      };
    } catch (e) {
      return { success: false, error: e.message || String(e) };
    }
  });
}

function disposeAllTerminalSessions() {
  for (const { pty } of sessions.values()) {
    try {
      pty.kill();
    } catch {
      /* ignore */
    }
  }
  sessions.clear();
}

module.exports = { registerTerminalPtyIpc, disposeAllTerminalSessions };
