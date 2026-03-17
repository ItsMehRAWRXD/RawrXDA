const {
  createConnection,
  TextDocuments,
  ProposedFeatures,
  CompletionItemKind,
  InitializeParams,
  TextDocumentSyncKind
} = require('vscode-languageserver/node');
const { TextDocument } = require('vscode-languageserver-textdocument');
const { spawn } = require('child_process');
const path = require('path');

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

connection.onInitialize((params) => {
  return {
    capabilities: {
      textDocumentSync: TextDocumentSyncKind.Incremental,
      completionProvider: {
        resolveProvider: false,
        triggerCharacters: ['.', '(', ' ', '\n']
      }
    }
  };
});

connection.onCompletion(async (params) => {
  try {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return [];

    const pos = params.position;
    const text = doc.getText();
    const lines = text.split(/\r?\n/);
    const currentLine = lines[pos.line] || '';
    
    // Call PowerShell heuristic model
    const psScriptPath = path.join(__dirname, '..', 'server', 'simple-model.ps1');
    
    return new Promise((resolve) => {
      const ps = spawn('pwsh', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', psScriptPath, currentLine], {
        cwd: path.dirname(psScriptPath)
      });

      let output = '';
      let errorOutput = '';

      ps.stdout.on('data', (data) => {
        output += data.toString();
      });

      ps.stderr.on('data', (data) => {
        errorOutput += data.toString();
      });

      ps.on('close', (code) => {
        if (code === 0 && output.trim()) {
          const suggestions = output.trim().split('\n').filter(s => s.length > 0);
          resolve(suggestions.slice(0, 5).map(suggestion => ({
            label: suggestion,
            kind: CompletionItemKind.Snippet,
            insertText: suggestion,
            detail: 'Local AI (PowerShell)'
          })));
        } else {
          resolve([]);
        }
      });

      setTimeout(() => {
        ps.kill();
        resolve([]);
      }, 2000);
    });
  } catch (err) {
    console.error('Completion error:', err);
    return [];
  }
});

documents.listen(connection);
connection.listen();