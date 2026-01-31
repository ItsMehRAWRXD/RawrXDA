import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext) {
  const disposable = vscode.commands.registerCommand('hello.sayHello', () => {
    vscode.window.showInformationMessage('Hello from your extension!');
  });
  context.subscriptions.push(disposable);
}

export function deactivate() {}
