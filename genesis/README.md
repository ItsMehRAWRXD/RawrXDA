# RawrXD Genesis — Self-Hosting MASM64 Bootstrap

**The hammer has evolved. It is now self-replicating.**

## Protocol

1. **Phase 1** — Use ML64 (one time only) to assemble `RawrXD_Genesis.asm` into `genesis.exe`.
2. **Phase 2** — `genesis.exe` (no args) reads self-compile path and writes minimal PE stub `RawrXD_Genesis.exe`.
3. **Phase 3** — `genesis.exe <file.asm>` assembles an external .asm file (when full parser/linker is implemented).
4. **Phase 4** — Genesis can link multiple .obj into RawrXD.exe with zero external tools.

## Capabilities (current / planned)

- **x86-64 assembler**: Parses MOV, PUSH, POP, CALL, RET, ADD, SUB, LEA, JMP (stubs in place).
- **PE64 linker**: Writes valid DOS/PE headers and section layout (minimal stub implemented).
- **Symbol resolution**: 2-pass assembly with forward references (stubbed).
- **Section management**: .text, .data layout (InitDefaultSections implemented).
- **Import table**: Can link against kernel32.dll (CreateFile, MapViewOfFile, VirtualAlloc, WriteFile, ExitProcess).

## Build

From repo root:

```powershell
.\GENESIS_COMPILER.ps1 -SelfCompile
```

Requires Visual Studio 2022 with C++ x64 tools (ML64 and LINK). Phase 1 produces `genesis\genesis.exe`; Phase 2 produces `genesis\RawrXD_Genesis.exe` (minimal MZ stub).

## One-liner

From repo root (builds genesis.exe and runs it once):

```powershell
.\GENESIS_ONE_LINER.ps1
```

Full bootstrap with self-compile:

```powershell
.\GENESIS_COMPILER.ps1 -SelfCompile
```

Remote (when hosted):

```powershell
irm "https://your-cdn/Genesis.ps1" | iex; .\GENESIS_COMPILER.ps1 -SelfCompile
```

**Requires:** Visual Studio 2022 (or Build Tools) with C++ x64 tools so that `ml64.exe` and `link.exe` are available under `C:\Program Files\Microsoft Visual Studio\2022\...\Hostx64\x64\`.

## Self-hosting goal

Once Genesis is fully implemented:

```powershell
.\genesis\genesis.exe --rebuild-universe   # Recompiles entire RawrXD from .asm sources
```

Visual Studio is then only required for the very first bootstrap of Genesis itself.

---

## Extension Loader (MASM64, in-process only)

`RawrXD_ExtensionLoader.asm` provides the **native extension host** for RawrXD: it loads DLLs from disk and/or registers **RWXD-format blobs** from memory. All code runs **inside RawrXD.exe**; there is no process injection and no modification of VS Code/Cursor.

- **ExtensionLoader_LoadModule** — Load a DLL by path, resolve exports by name, dispatch by extension ID + entry index.
- **ExtensionLoader_RegisterNativeBlob** — Register an in-process blob with magic `RWXD` (0x44585752). Blob layout: Magic, Version, Name(64), Publisher(64), EntryPointRVA(8), CommandCount(4), CommandTableRVA(8). Each command entry: CommandId(128 bytes), HandlerRVA(8). Handlers are called with one argument (context).
- **ExtensionLoader_ExecuteCommand** — Look up command ID across all registered native blobs and call the matching handler with the given context. Returns handler result or 0 if not found.
- **ExtensionLoader_UnloadAll** / **ExtensionLoader_UnloadNativeAll** — Free all DLLs and clear the native table.

Build: `.\scripts\Build-ExtensionLoader.ps1` (optionally `-LinkTest` for a test exe). Link `ExtensionLoader.obj` + `ExtensionStubs.obj` + `kernel32.lib` into RawrXD.
