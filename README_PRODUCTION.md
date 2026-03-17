# RawrXD Agentic IDE — Production Readme

## Canonical Target

The current production target is the monolithic Win32/x64 IDE built from `src\asm\monolithic` and emitted as:

- `build\monolithic\bin\RawrXD_Monolithic.exe`

This lane is now exposed directly from the repository root so the usable build is immediately accessible.

---

## Root-Level Entry Points

### Build wrappers
- `Build-AgenticIDE.ps1`
- `Build-AgenticIDE.bat`

### Launch wrappers
- `Launch-AgenticIDE.ps1`
- `Launch-AgenticIDE.bat`

These wrappers call the canonical builder at `src\asm\monolithic\Build-Monolithic.ps1`.

---

## Validated State

Validated on 2026-03-13:
- Monolithic build completed successfully
- Output generated at `build\monolithic\bin\RawrXD_Monolithic.exe`
- Build path reduced to one stable lane from repository root

---

## Build

### PowerShell
```powershell
./Build-AgenticIDE.ps1 -Config debug
./Build-AgenticIDE.ps1 -Config release
```

### Batch
```bat
Build-AgenticIDE.bat
```

### Build and launch
```powershell
./Build-AgenticIDE.ps1 -Config release -Run
```

---

## Launch

### Launch existing build
```powershell
./Launch-AgenticIDE.ps1
```

### Force rebuild first
```powershell
./Launch-AgenticIDE.ps1 -Rebuild
```

### Batch
```bat
Launch-AgenticIDE.bat
```

---

## Canonical Source Map

- `src\asm\monolithic\Build-Monolithic.ps1` — build and link pipeline
- `src\asm\monolithic\main.asm` — entrypoint and top-level lifecycle
- `src\asm\monolithic\ui.asm` — editor UI, input, painting, scrolling, ghost text flow
- `src\asm\monolithic\bridge.asm` — completion bridge integration
- `src\asm\monolithic\pe_writer.asm` — PE writer integration

---

## Repository Rule

When the goal is the working agentic IDE, use the monolithic lane first.

The rest of the repository contains alternate implementations, experiments, partial migrations, audits, generated artifacts, and historical prototypes. Those should not be the default production entry path.

---

## Outcome

The project now has a direct, root-accessible build and launch path for the current best working agentic IDE target instead of forcing navigation through nested prototype-heavy directories.
