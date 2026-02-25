# BigDaddyG IDE

Full-featured IDE built on Electron and React. Core experience: file explorer, Monaco editor, integrated terminal, project management, and local DB. **AI support (Ollama/BigDaddyG, future Copilot/Amazon Q/Cursor) is a bonus**—the IDE is usable without it; AI enhances the experience when available.

**Environment: Windows-first.** Builds target NSIS on Windows. Linux/macOS packaging (AppImage, etc.) is scaffolded but secondary.

## Stack

- **Electron** – main process (file system, IPC, dialogs)
- **React** – UI (Create React App)
- **Tailwind CSS** – styling
- **Monaco Editor** – code editing
- **node-pty** – integrated terminal (required)
- **sqlite3** – local DB features (required)
- **Ollama** – local BigDaddyG model (optional; enables AI features)

## Prerequisites (Required for IDE)

- Node.js 18+
- npm or yarn
- **node-pty** – native build for integrated terminal (install via `npm install`; requires Windows build tools)
- **sqlite3** – native bindings for local DB (install via `npm install`; requires Windows build tools)

## Prerequisites (Required for AI Features)

- [Ollama](https://ollama.ai/) with the BigDaddyG model (for local AI when enabled)

## Setup

```bash
cd bigdaddyg-ide
npm install
```

## Scripts

| Command | Description |
|--------|-------------|
| `npm start` | Start dev: React on port 3000 + Electron (wait-on then launches Electron) |
| `npm run dev:react` | React dev server only |
| `npm run dev:electron` | Electron only (loads built or dev URL) |
| `npm run build` | Build React app to `build/` |
| `npm run build:electron` | Build React then package with electron-builder |
| `npm run pack` | Build without full installer (output in `dist/`) |
| `npm run dist` | Full distributable (NSIS on Windows, AppImage on Linux, etc.) |

## Configuration

- **`config/providers.json`** – AI provider URLs, models, and experimental flags (Ollama URL, Copilot/Amazon Q/Cursor placeholders). Used when AI features are enabled.
- **`config/performance.json`** – Editor and performance options (target FPS, memory, etc.).

Edit these files in the project root. For packaged builds, include `config/**/*` in `build.files` (already set in `package.json`) so config ships with the app.

## Usage

1. **Open a project**: Toolbar "Open project" or Sidebar "Open folder" – choose a directory. File access is restricted to this root.
2. **Edit files**: Click a file in the sidebar to open it in Monaco.
3. **Terminal**: Use the integrated terminal (powered by node-pty).
4. **AI (optional)**: When Ollama is running with BigDaddyG, use the toolbar dropdown to switch providers. "Toggle Agent" opens the agent panel for agentic tasks (plan + steps via BigDaddyG). Without AI, the IDE remains fully functional for editing and project management.

## Project layout

```
bigdaddyg-ide/
├── electron/          # Main process and preload
├── src/               # React app, components, contexts, agentic (renderer + shared)
├── config/            # providers.json, performance.json
├── public/            # index.html
└── build/             # Output of `npm run build` (used by Electron in production)
```

## Security

File read/write and directory listing are allowed only under the **project root** chosen via "Open project". The main process validates paths before any `fs` operation.

## License

MIT.
