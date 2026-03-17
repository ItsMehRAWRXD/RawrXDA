# Rawr Assist – Local Q2_K GGUF Copilot + Agent

A VS Code extension that provides inline completions and agentic browser execution from any local Q2_K GGUF model (like BigDaddyG-NO-REFUSE-Q2_K.gguf).

## Features

- **Inline Completions**: Like GitHub Copilot, but from your local Q2_K model
- **Agentic Commands**: Select text and run browser-based agents that execute plans in real Chrome
- **No Cloud**: Everything runs locally, no API calls
- **Q2_K Optimized**: Designed for fast, low-RAM Q2_K models (~20 GB)

## Installation

1. Clone or download this repo
2. `npm install`
3. `npm run compile`
4. `npx vsce package` (installs vsce globally first if needed)
5. `code --install-extension rawr-assist-0.1.0.vsix`

## Configuration

In VS Code settings, set:
- `rawr-assist.ggufPath`: Path to your Q2_K GGUF (e.g., `C:\Franken\BigDaddyG-NO-REFUSE-Q2_K.gguf`)
- `rawr-assist.contextLen`: Context length (default 4096)

## Usage

- **Inline Completions**: Start typing in any file – suggestions appear automatically
- **Run Agent**: Select text describing a task → Ctrl+Shift+P → "Rawr: Run Agent on Selection" → Browser opens and executes the plan

## Requirements

- VS Code 1.75+
- Node.js 18+
- Chrome installed
- llama.dll in `./lib/` (build from llama.cpp)

## License

MIT