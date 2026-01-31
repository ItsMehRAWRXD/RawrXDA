import * as vscode from 'vscode';
import { LlamaBridge } from './llamaBridge';
import { AgentRunner } from './agentRunner';
import { InlineProvider } from './inlineProvider';

let llama: LlamaBridge;

export function activate(context: vscode.ExtensionContext) {
  const cfg = vscode.workspace.getConfiguration('rawr-assist');
  const ggufPath = cfg.get<string>('ggufPath')!;
  llama = new LlamaBridge(ggufPath, cfg.get<number>('contextLen')!);

  // 1.  inline completions
  const provider = new InlineProvider(llama);
  const sel = { scheme: '*', language: '*' };
  context.subscriptions.push(
    vscode.languages.registerInlineCompletionItemProvider(sel, provider)
  );

  // 2.  agentic command
  context.subscriptions.push(
    vscode.commands.registerCommand('rawr-assist.runAgent', async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) return;
      const selection = editor.document.getText(editor.selection);
      if (!selection) { vscode.window.showErrorMessage('No selection'); return; }
      await vscode.window.withProgress({ location: vscode.ProgressLocation.Notification, title: 'Rawr agent running…' }, async () => {
        const agent = new AgentRunner(llama);
        const result = await agent.run(selection);
        editor.edit(b => b.replace(editor.selection, result));
      });
    })
  );

  vscode.window.showInformationMessage('Rawr Assist – local Q2_K GGUF loaded');
}

export function deactivate() { llama?.dispose(); }