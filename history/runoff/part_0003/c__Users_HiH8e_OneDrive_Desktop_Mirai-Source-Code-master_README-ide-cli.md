Agentic IDE CLI

Overview

This repository includes a small Node.js CLI to help agentically manage the single-file IDE in this workspace. It can:

- Serve the workspace via an HTTP static server
- Produce a file manifest
- Run headless UI checks (measure bounding boxes, detect overlaps, capture screenshot) using Puppeteer
- Handle simple git operations (init, add/commit, status) by invoking the system git
- Import and list todo items stored in `.ide-cli-todos.json`
- Set file permissions on the `models` directory (best-effort)

Quick start

1. Install dependencies (Node.js and npm required):

```powershell
cd "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"
npm install
```

2. Serve the IDE and run UI check:

```powershell
# Start a server in background (or in separate terminal)
node cli.js serve -p 8000

# In another terminal, run UI checks (will take a screenshot)
node cli.js ui-check --url http://localhost:8000/IDEre2.html
```

Notes and platform considerations

- Puppeteer will download a Chromium binary when you run npm install. If you want to use your system Chrome, use puppeteer-core and set the executablePath in the CLI.
- File permissions (chmod) on Windows are best-effort — Windows does not honor Unix permissions fully; consider managing permissions on a Linux machine if strict 755 is required.
- Serve the IDE over HTTP(S) — some storage APIs (OPFS) require secure contexts (https or localhost) and will not work on file://.

How to use

```text
Commands:
  serve [options]       Serve the workspace (static server)
  manifest              Create a JSON manifest of files
  ui-check              Run headless UI checks with Puppeteer
  git-init              Initialize a git repository
  git-add-commit        Add all and commit
  git-status            Show git status
  todo-import <file>    Import JSON todos
  todo-list             List stored todos
  set-model-perms       Set 755 on models directory (best-effort)
```

If something fails, copy the console output and open an issue with the log and environment details (OS, Node version, browser).