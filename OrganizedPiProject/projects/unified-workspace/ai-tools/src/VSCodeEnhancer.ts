import * as vscode from 'vscode';

export class VSCodeEnhancer {
  static enhanceSearchLimits(): void {
    const config = vscode.workspace.getConfiguration();
    
    // Maximize search capabilities
    config.update('search.maxResults', Number.MAX_SAFE_INTEGER, vscode.ConfigurationTarget.Global);
    config.update('search.searchOnType', true, vscode.ConfigurationTarget.Global);
    config.update('search.searchOnTypeDebouncePeriod', 0, vscode.ConfigurationTarget.Global);
    config.update('search.followSymlinks', true, vscode.ConfigurationTarget.Global);
    config.update('search.useIgnoreFiles', false, vscode.ConfigurationTarget.Global);
    config.update('search.useGlobalIgnoreFiles', false, vscode.ConfigurationTarget.Global);
    
    // Maximize file limits
    config.update('files.maxMemoryForLargeFilesMB', Number.MAX_SAFE_INTEGER, vscode.ConfigurationTarget.Global);
    config.update('files.watcherExclude', {}, vscode.ConfigurationTarget.Global);
    
    // Maximize editor capabilities
    config.update('editor.maxTokenizationLineLength', Number.MAX_SAFE_INTEGER, vscode.ConfigurationTarget.Global);
    config.update('editor.largeFileOptimizations', false, vscode.ConfigurationTarget.Global);
    
    // Maximize terminal capabilities
    config.update('terminal.integrated.scrollback', Number.MAX_SAFE_INTEGER, vscode.ConfigurationTarget.Global);
    
    // Maximize IntelliSense
    config.update('typescript.suggest.completeFunctionCalls', true, vscode.ConfigurationTarget.Global);
    config.update('typescript.suggest.includeCompletionsForImportStatements', true, vscode.ConfigurationTarget.Global);
    
    vscode.window.showInformationMessage('VS Code limits maximized!');
  }

  static enhanceCopilotLimits(): void {
    const config = vscode.workspace.getConfiguration();
    
    // Maximize GitHub Copilot
    config.update('github.copilot.enable', { '*': true }, vscode.ConfigurationTarget.Global);
    config.update('github.copilot.advanced', {
      'length': Number.MAX_SAFE_INTEGER,
      'temperature': 0.1,
      'top_p': 1,
      'stop': [],
      'indentationMode': { 'python': 'space', '*': 'auto' }
    }, vscode.ConfigurationTarget.Global);
    
    vscode.window.showInformationMessage('Copilot limits maximized!');
  }
}