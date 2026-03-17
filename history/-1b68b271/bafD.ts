import * as vscode from 'vscode';

let outputChannel: vscode.OutputChannel;

export function activate(context: vscode.ExtensionContext) {
  outputChannel = vscode.window.createOutputChannel('Cursor Agent Exec');
  outputChannel.appendLine('Cursor Agent Exec extension activated');

  // Register commands for agent execution
  const executeCommand = vscode.commands.registerCommand('cursor-agent-exec.execute', async () => {
    const command = await vscode.window.showInputBox({
      prompt: 'Enter command to execute',
      placeHolder: 'e.g., echo "Hello World"'
    });

    if (command) {
      try {
        const terminal = vscode.window.createTerminal('Agent Exec');
        terminal.show();
        terminal.sendText(command);
        outputChannel.appendLine(`Executed: ${command}`);
      } catch (error) {
        outputChannel.appendLine(`Error executing command: ${error}`);
        vscode.window.showErrorMessage(`Failed to execute command: ${error}`);
      }
    }
  });

  const runAgentCommand = vscode.commands.registerCommand('cursor-agent-exec.runAgent', async () => {
    outputChannel.appendLine('Running agent...');
    // Agent execution logic would go here
    vscode.window.showInformationMessage('Agent execution capability activated');
  });

  context.subscriptions.push(executeCommand, runAgentCommand);
}

export function deactivate() {
  if (outputChannel) {
    outputChannel.dispose();
  }
}