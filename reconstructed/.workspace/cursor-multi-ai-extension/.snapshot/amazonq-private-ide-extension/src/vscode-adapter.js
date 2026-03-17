const vscode = require('vscode');

class VSCodeIDEAdapter {
  constructor() {
    this.window = {
      createOutputChannel: (name) => vscode.window.createOutputChannel(name),
      createStatusBarItem: (alignment = 'left', priority) => {
        const align = String(alignment).toLowerCase() === 'right' ? vscode.StatusBarAlignment.Right : vscode.StatusBarAlignment.Left;
        return vscode.window.createStatusBarItem(align, priority);
      },
      showInformationMessage: (...args) => vscode.window.showInformationMessage(...args),
      showErrorMessage: (...args) => vscode.window.showErrorMessage(...args),
      showWarningMessage: (...args) => vscode.window.showWarningMessage(...args),
      withProgress: (options, task) => {
        const locMap = {
          notification: vscode.ProgressLocation.Notification,
          window: vscode.ProgressLocation.Window,
          sourcecontrol: vscode.ProgressLocation.SourceControl
        };
        const location = locMap[String(options.location || 'notification').toLowerCase()] || vscode.ProgressLocation.Notification;
        return vscode.window.withProgress({ ...options, location }, task);
      },
      get activeTextEditor() { return vscode.window.activeTextEditor; },
      createWebviewPanel: (viewType, title, showOptions, webviewOptions) => {
        let column = showOptions;
        if (typeof showOptions === 'string' && showOptions.toLowerCase() === 'beside') {
          column = vscode.ViewColumn.Beside;
        }
        return vscode.window.createWebviewPanel(viewType, title, column, webviewOptions);
      }
    };

    this.workspace = {
      getConfiguration: (section) => vscode.workspace.getConfiguration(section)
    };

    this.commands = {
      registerCommand: (command, callback) => vscode.commands.registerCommand(command, callback)
    };
  }
}

module.exports = { VSCodeIDEAdapter };
