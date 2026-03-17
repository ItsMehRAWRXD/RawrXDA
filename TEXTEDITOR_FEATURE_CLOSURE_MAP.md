# RawrXD Text Editor Feature Closure Map

## Requested Gaps

| Feature | Status | What already exists | What is still missing |
|---|---|---|---|
| Undo/Redo | Open | Menu IDs and editor command surface exist in GUI variants | A unified snapshot/history layer in the main shipped GUI target |
| Find/Replace | Partial | Integration-oriented editor wiring exists | Main GUI still needs common-dialog routing and buffer search/replace hookup |
| Syntax highlighting | Partial | Dedicated syntax module and display-integration module exist | Final renderer merge into the main GUI target |
| Multiple files | Open | None in the main text editor path | Tab control, document model, and per-buffer lifetime management |
| Code completion | Partial | Completion engine, popup UI, and integration module exist | Final binding into the main production GUI path |
| Toolbar buttons | Partial | Toolbar/button creation exists in GUI variants | Unified command routing across the shipped target |
| Status updates | Partial | Static/action status-bar text path exists | Caret position, dirty state, and richer live status updates |

## Existing Modules To Reuse

- `RawrXD_TextEditor_SyntaxHighlighter.asm`
- `RawrXD_TextEditor_DisplayIntegration.asm`
- `RawrXD_TextEditor_Completion.asm`
- `RawrXD_TextEditor_CompletionPopup.asm`
- `RawrXD_TextEditor_Integration.asm`
- `RawrXD_TextEditor_EditOps.asm`

## Recommended Merge Order

1. Merge syntax-highlighter and display-integration rendering hooks.
2. Merge completion popup and Ctrl+Space integration path.
3. Add a committed undo/redo history layer for the main GUI buffer.
4. Add find/replace dialog routing against the live buffer.
5. Add multi-document state and tab control last, after single-document commands are stable.

## Why This Matters

The repository already contains most of the specialized building blocks for syntax coloring, completion, popup UI, and editor integration. The main remaining work is not inventing those systems from scratch; it is consolidating them into one stable, single build target and wiring their command/message paths cleanly.