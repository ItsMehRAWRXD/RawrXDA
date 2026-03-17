import * as vscode from 'vscode';

export class InlineProvider implements vscode.InlineCompletionItemProvider {
  constructor(private llama: any) { }

  async provideInlineCompletionItems(
    document: vscode.TextDocument,
    position: vscode.Position,
    context: vscode.InlineCompletionContext,
    token: vscode.CancellationToken
  ): Promise<vscode.InlineCompletionItem[]> {
    const prompt = document.getText(new vscode.Range(new vscode.Position(0, 0), position));
    const suffix = document.getText(new vscode.Range(position, new vscode.Position(document.lineCount, 0)));
    const req = `${prompt}<FIM>${suffix}`;
    const resp = await this.llama.complete(req, 60);
    if (!resp) return [];
    return [{ insertText: resp }];
  }
}