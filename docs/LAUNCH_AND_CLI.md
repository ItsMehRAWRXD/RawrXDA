# RawrXD IDE — Where It Is and How to Run It

## Build location (this repo)

- **Project root:** `D:\rawrxd`
- **Build output:** `D:\rawrxd\build\bin\`
  - **Full IDE:** `RawrXD-Win32IDE.exe`
  - **Minimal GUI:** `rawrxd.exe`
- **Gold standalone:** `D:\rawrxd\build\gold\RawrXD_Gold.exe` (if built)

All build scripts now use `D:\rawrxd` as the project root.

---

## How to run the IDE

### Option 1: Launch script (recommended)

From repo root:

```powershell
cd D:\rawrxd
.\Launch-RawrXD.ps1
```

With CLI-style args (e.g. help/version):

```powershell
.\Launch-RawrXD.ps1 --help
.\Launch-RawrXD.ps1 --version
```

### Option 2: Run the exe directly

```powershell
# Full Win32 IDE
D:\rawrxd\build\bin\RawrXD-Win32IDE.exe

# Minimal UI (same binary supports --help / --version)
D:\rawrxd\build\bin\rawrxd.exe
D:\rawrxd\build\bin\rawrxd.exe --help
D:\rawrxd\build\bin\rawrxd.exe --version
```

### Option 3: Build then run

```powershell
cd D:\rawrxd
.\BUILD_ORCHESTRATOR.ps1 -Mode quick
.\Launch-RawrXD.ps1
```

---

## “CLI” usage

There is **no separate CLI-only executable**. The **minimal** GUI target `rawrxd.exe` is a Win32 GUI app that also handles a few CLI-style flags so you can use it from a terminal:

| Flag         | Effect                          |
|-------------|----------------------------------|
| `--help`    | Print usage and exit            |
| `--version` | Print version and build info    |

Example (from a console, so you see output):

```powershell
& "D:\rawrxd\build\bin\rawrxd.exe" --version
& "D:\rawrxd\build\bin\rawrxd.exe" --help
```

For scripted or headless use, the codebase also has **RawrEngine** and **RawrXD_Gold** targets (see CMakeLists.txt); those are different entry points (engine/standalone), not a general-purpose CLI client.

---

## If you don’t see the IDE after building

1. **Confirm the exe exists**
   - Check: `D:\rawrxd\build\bin\RawrXD-Win32IDE.exe` or `rawrxd.exe`
   - If `build\bin` is empty or missing, run a full IDE build (see below).

2. **Rebuild the IDE**
   - Quick: `.\BUILD_ORCHESTRATOR.ps1 -Mode quick`
   - IDE only: `.\BUILD_ORCHESTRATOR.ps1 -Mode ide`
   - Production: `.\BUILD_ORCHESTRATOR.ps1 -Mode production`

3. **Run via the script**
   - `.\Launch-RawrXD.ps1` — it will tell you if no exe was found and where it looked.

---

## Build scripts (all use `D:\rawrxd`)

| Script                   | Purpose                          |
|--------------------------|-----------------------------------|
| `BUILD_ORCHESTRATOR.ps1` | Main entry: quick / dev / production / ide / test |
| `BUILD_IDE_FAST.ps1`     | Fast IDE/compilers/desktop build |
| `BUILD_IDE_PRODUCTION.ps1`| Full production build + report   |
| `Launch-RawrXD.ps1`      | Run IDE from `build\bin`          |

Executables produced by these builds are under **`D:\rawrxd\build\bin`** (and optionally `build\gold` for RawrXD_Gold).
