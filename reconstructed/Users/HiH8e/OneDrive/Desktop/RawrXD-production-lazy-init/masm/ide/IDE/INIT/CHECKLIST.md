# IDE Initialization Checklist (Agent Chat + Terminals)

- Launch VS Code with admin rights when running system-impactful tasks (right-click VS Code icon → Run as administrator).
- Terminals: create three profiles in VS Code Terminal panel
  - PowerShell (default): set `pwsh.exe` as default; enable "Run as administrator" when elevated actions are needed.
  - Command Prompt: add profile for `cmd.exe`; test by running `ver`.
  - Bash (MSYS/MinGW or WSL if installed): add profile pointing to bash executable; test with `bash --version`.
- Context initialization on startup scripts:
  - Run `scripts/env/initialize.ps1` (create if missing) to preload PATH entries for `masm32`, `msys64`, `mingw64`, and project `src`/`build` dirs.
  - Run `agent_chat_init.ps1` (create if missing) to start agent chat backends and ensure sockets/ports are free.
- Admin actions: when right-clicking a script → "Run as administrator" to allow binding low ports, writing to ProgramData, or installing services.
- Verify tool availability: `ml.exe`, `link.exe`, `nasm`, `git`, `curl`, `python`, `node`, `7z` are on PATH in each profile.
- Logging: enable VS Code task output to Problems panel for builds; capture terminal logs under `logs/terminal/`.
