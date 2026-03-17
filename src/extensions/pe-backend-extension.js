/**
 * PE Backend Extension — registers compile commands into the IDE.
 * Keybindings:
 *   F5              — Compile & Run (console)
 *   Ctrl+Shift+B    — Compile (console)
 *   Ctrl+Shift+G    — Compile (GUI)
 */

'use strict';

const { CompileService } = require('../services/compile-service');

function activate(ide) {
    const compiler = new CompileService(ide);

    // ── Commands ─────────────────────────────────────────────────────────
    ide.registerCommand('rawrxd.compileConsole', () => {
        const result = compiler.compile({ target: 'console' });
        if (result.success) {
            ide.showNotification(`Compiled: ${result.outputPath}`);
        } else {
            ide.showNotification(`Compile failed: ${result.error}`, 'error');
        }
        return result;
    });

    ide.registerCommand('rawrxd.compileGUI', () => {
        const result = compiler.compile({ target: 'gui' });
        if (result.success) {
            ide.showNotification(`Compiled: ${result.outputPath}`);
        } else {
            ide.showNotification(`Compile failed: ${result.error}`, 'error');
        }
        return result;
    });

    ide.registerCommand('rawrxd.compileAndRun', () => {
        return compiler.compileAndRun({ target: 'console' });
    });

    // ── Keybindings ──────────────────────────────────────────────────────
    ide.registerKeybinding('F5',           'rawrxd.compileAndRun');
    ide.registerKeybinding('Ctrl+Shift+B', 'rawrxd.compileConsole');
    ide.registerKeybinding('Ctrl+Shift+G', 'rawrxd.compileGUI');

    // ── Build Menu ───────────────────────────────────────────────────────
    ide.registerMenu('Build', [
        { label: 'Compile (Console)',  command: 'rawrxd.compileConsole',  shortcut: 'Ctrl+Shift+B' },
        { label: 'Compile (GUI)',      command: 'rawrxd.compileGUI',     shortcut: 'Ctrl+Shift+G' },
        { separator: true },
        { label: 'Compile && Run',     command: 'rawrxd.compileAndRun',  shortcut: 'F5' }
    ]);

    ide.log('[PE Backend] Extension activated \u2014 F5 to compile & run');
}

function deactivate() {
    // Cleanup if needed
}

module.exports = { activate, deactivate };
