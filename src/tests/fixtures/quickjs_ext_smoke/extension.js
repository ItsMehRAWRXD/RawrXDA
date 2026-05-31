// Minimal JS entrypoint for QuickJS host smoke.
// Intentionally tiny: prove loading + activation hooks exist.

function activate(context) {
  try {
    if (typeof vscode !== "undefined" && vscode && vscode.window && vscode.window.showInformationMessage) {
      vscode.window.showInformationMessage("quickjs_ext_smoke activated");
    }
  } catch (_) {
    // ignore
  }
  return true;
}

function deactivate() {
  return true;
}

globalThis.activate = activate;
globalThis.deactivate = deactivate;

