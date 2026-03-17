const path = require('path');
const vscode = require('vscode');
const { LanguageClient, TransportKind } = require('vscode-languageclient/node');

/** @type {LanguageClient | null} */
let client = null;

async function activate(context) {
  const output = vscode.window.createOutputChannel('CodeBuddy');
  output.appendLine('CodeBuddy activating...');

  const serverModule = context.asAbsolutePath(path.join('..', 'server', 'server.js'));

  const serverOptions = {
    run:   { module: serverModule, transport: TransportKind.stdio },
    debug: { module: serverModule, transport: TransportKind.stdio, options: { execArgv: ['--inspect=6009'] } },
  };

  const clientOptions = {
    documentSelector: [
      { scheme: 'file', language: 'javascript' },
      { scheme: 'file', language: 'typescript' },
      { scheme: 'file', language: 'powershell' },
      { scheme: 'untitled', language: 'javascript' },
      { scheme: 'untitled', language: 'typescript' },
      { scheme: 'untitled', language: 'powershell' },
    ],
    synchronize: {
      fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{js,ts,ps1,psm1}')
    }
  };

  client = new LanguageClient('codebuddy', 'CodeBuddy LSP', serverOptions, clientOptions);

  context.subscriptions.push(vscode.commands.registerCommand('codebuddy.start', async () => {
    if (client && client.isRunning()) {
      vscode.window.showInformationMessage('CodeBuddy is already running.');
      return;
    }
    await client.start();
    vscode.window.showInformationMessage('CodeBuddy started.');
  }));

  // Start automatically after activation
  await client.start();
  output.appendLine('CodeBuddy started.');
}

async function deactivate() {
  if (client) {
    await client.stop();
    client = null;
  }
}

module.exports = { activate, deactivate };
