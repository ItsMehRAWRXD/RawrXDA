# BigDaddyG Copilot (Local Ollama)

- Commands:
  - `BigDaddyG: Open AI Chat`
  - `BigDaddyG: Beacon Chat (Local Ollama)`
  - `BigDaddyG: Toggle Cursor Bypass`
- Keybinding:
  - `Ctrl+L` opens chat (overrides VS Code default when editor has focus)
 - Models:
  - Auto (resolves to first installed or fast default)
  - Dynamic list fetched from local Ollama (`/api/tags`), plus an input to type any model tag (no hardcoded set)
 - Modes:
   - Agent, Plan, Ask, Edit (selectable in chat UI)

## Build & Install

```pwsh
# From extension folder
cd "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\ide-extensions\bigdaddyg-copilot"

# Initialize if needed
npm init -y

# Dev deps
npm install --save-dev typescript @types/vscode

# Optional: install vsce for packaging
npm install -g vsce

# Compile
npm run compile

# Package (creates .vsix)
vsce package

# Install VSIX
code --install-extension .\bigdaddyg-copilot-1.0.0.vsix
```

## Notes
- Ensure Ollama is running locally at `http://127.0.0.1:11434`.
- Adjust model mappings in `src/extension.ts` `resolveModel()` to your local tags.
- If `Ctrl+L` doesn't trigger chat, open Keyboard Shortcuts and bind `bigdaddyg.openChat` to `Ctrl+L` (might require removing the default "Expand Line Selection").
