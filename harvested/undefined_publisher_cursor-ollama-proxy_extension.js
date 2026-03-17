const vscode = require('vscode');
const express = require('express');
const { createProxyMiddleware } = require('http-proxy-middleware');
const fs = require('fs');
const path = require('path');
const os = require('os');

let proxyServer = null;

function activate(context) {
    const startCommand = vscode.commands.registerCommand('cursor-ollama.start', startProxy);
    const stopCommand = vscode.commands.registerCommand('cursor-ollama.stop', stopProxy);
    
    context.subscriptions.push(startCommand, stopCommand);
    
    // Auto-start proxy
    startProxy();
}

async function startProxy() {
    if (proxyServer) {
        vscode.window.showInformationMessage('Ollama proxy already running');
        return;
    }

    try {
        const app = express();
        
        const proxy = createProxyMiddleware({
            target: 'http://localhost:11434',
            changeOrigin: true,
            pathRewrite: {
                '^/v1/chat/completions': '/api/chat',
                '^/v1/models': '/api/tags'
            }
        });

        app.use('/', proxy);
        
        proxyServer = app.listen(8081, () => {
            updateCursorSettings();
            vscode.window.showInformationMessage('Ollama proxy started on port 8081');
        });
        
    } catch (error) {
        vscode.window.showErrorMessage(`Failed to start proxy: ${error.message}`);
    }
}

function stopProxy() {
    if (proxyServer) {
        proxyServer.close();
        proxyServer = null;
        vscode.window.showInformationMessage('Ollama proxy stopped');
    }
}

function updateCursorSettings() {
    const settingsPath = path.join(os.homedir(), 'AppData', 'Roaming', 'Cursor', 'User', 'settings.json');
    
    try {
        let settings = {};
        if (fs.existsSync(settingsPath)) {
            settings = JSON.parse(fs.readFileSync(settingsPath, 'utf8'));
        }
        
        settings['cursor.chat.openaiBaseUrl'] = 'http://localhost:8081/v1';
        
        fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 4));
    } catch (error) {
        console.error('Failed to update settings:', error);
    }
}

function deactivate() {
    stopProxy();
}

module.exports = { activate, deactivate };