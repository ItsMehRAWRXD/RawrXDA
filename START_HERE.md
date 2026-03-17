# RawrXD Agentic IDE — Canonical Entry Point

**Status:** canonical production lane  
**Validated:** 2026-03-13  
**Current output:** `build/monolithic/bin/RawrXD_Monolithic.exe`

---

## Use This Build Lane

The authoritative implementation is the monolithic x64 build under `src\asm\monolithic`.

This is the lane to use when the goal is a directly buildable, directly launchable RawrXD agentic IDE without hunting across prototype folders.

---

## Direct Entry Points From Repository Root

### Build
- `./Build-AgenticIDE.ps1`
- `Build-AgenticIDE.bat`

### Launch
- `./Launch-AgenticIDE.ps1`
- `Launch-AgenticIDE.bat`

---

## Typical Commands

```powershell
./Build-AgenticIDE.ps1 -Config debug
./Build-AgenticIDE.ps1 -Config release
./Build-AgenticIDE.ps1 -Config release -Run

./Launch-AgenticIDE.ps1
./Launch-AgenticIDE.ps1 -Rebuild
./Launch-AgenticIDE.ps1 -Rebuild -Config release
```

---

## Canonical Source Files

- `src\asm\monolithic\Build-Monolithic.ps1` — full monolithic assembler/linker pipeline
- `src\asm\monolithic\main.asm` — process entry and top-level wiring
- `src\asm\monolithic\ui.asm` — editor surface, input, paint, ghost text, PE save/open hooks
- `src\asm\monolithic\bridge.asm` — bridge boundary and message wiring
- `src\asm\monolithic\pe_writer.asm` — PE writer path

---

## Output Location

- `build\monolithic\bin\RawrXD_Monolithic.exe`

---

## What Changed

- Root-level build wrapper added
- Root-level launch wrapper added
- Batch wrappers added for direct Windows use
- Canonical path reduced to one stable build lane

---

## Working Rule

If the objective is the finished, usable IDE, stay on the monolithic lane first.

Treat the rest of the repository as support material, legacy work, component experiments, or alternate implementations unless a specific file is being targeted.
