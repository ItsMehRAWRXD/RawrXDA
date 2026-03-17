const { createConnection, ProposedFeatures, TextDocuments, CompletionItemKind } = require('vscode-languageserver/node');
const { TextDocument } = require('vscode-languageserver-textdocument');
const cp = require('child_process');
const path = require('path');

const connection = createConnection(ProposedFeatures.all);
const documents = new TextDocuments(TextDocument);

// Simple heuristic model call via PowerShell script
function callLocalModelPs(input) {
  return new Promise((resolve) => {
    // Resolve PowerShell executable: prefer Windows PowerShell then pwsh
    const pwshExe = process.env.ComSpec?.includes('powershell') ? process.env.ComSpec : 'pwsh.exe';
    const psScript = path.join(__dirname, 'simple-model.ps1');
    const child = cp.spawn(pwshExe, ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', psScript, '-InputObject', input], { stdio: ['ignore', 'pipe', 'pipe'] });

    let out = '';
    child.stdout.on('data', (d) => (out += d.toString()));
    child.stderr.on('data', () => {});
    child.on('exit', () => {
      try {
        resolve(out.trim());
      } catch {
        resolve('');
      }
    });
  });
}

connection.onInitialize((_params) => {
  return {
    capabilities: {
      textDocumentSync: 2,
      completionProvider: { resolveProvider: false, triggerCharacters: ['.', '>', ':'] },
      hoverProvider: false,
    },
  };
});

connection.onCompletion(async (params) => {
  const doc = documents.get(params.textDocument.uri);
  if (!doc) return [];
  const text = doc.getText();
  const offset = doc.offsetAt(params.position);
  const prefix = text.slice(Math.max(0, offset - 256), offset);

  const suggestion = await callLocalModelPs(prefix);
  if (!suggestion) return [];

  return [
    {
      label: suggestion.split('\n')[0].slice(0, 64) || 'suggestion',
      kind: CompletionItemKind.Text,
      insertText: suggestion,
    },
  ];
});

documents.listen(connection);
connection.listen();
