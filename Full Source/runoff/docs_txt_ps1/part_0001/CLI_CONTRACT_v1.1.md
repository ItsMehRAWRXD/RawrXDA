# RawrXD Unified CLI Contract — v1.1

> **Effective:** 2026-02-10
> **Binary:** `RawrXD_IDE_unified.exe`
> **Assembler:** MASM64 (ml64.exe)
> **ABI:** Microsoft x64 calling convention
> **Status:** FROZEN — breaking changes require version bump

---

## 1. Invocation

```
RawrXD_IDE_unified.exe [<mode>] [<mode-args>]
```

- **No arguments** → interactive GUI menu (console loop)
- **One mode switch** → execute mode, print results, exit
- Switch prefix: `-` or `/` (both accepted)
- `/c` and `-c` are aliases for `-compile`

---

## 2. Mode Table

| ID | Switch        | Aliases   | Description                                  | Requires Args | Artifacts            |
|----|---------------|-----------|----------------------------------------------|----------------|----------------------|
|  1 | `-compile`    | `-c`, `/c`| Inline trace-engine generation (no ml64)     | No             | `trace_map.json`     |
|  2 | `-encrypt`    |           | Camellia-256 encrypt/decrypt                 | No             | `encrypted.bin`      |
|  3 | `-inject`     |           | Process injection (requires target)          | `-pid=<N>` or `-pname=<name>` | — |
|  4 | `-uac`        |           | UAC bypass demonstration (fodhelper/eventvwr) | No            | —                    |
|  5 | `-persist`    |           | Registry persistence install/remove          | No             | Registry key         |
|  6 | `-sideload`   |           | DLL search-order hijack demonstration        | No             | —                    |
|  7 | `-avscan`     |           | Local PE/EICAR signature scanner             | No (self-scan) | —                    |
|  8 | `-entropy`    |           | Shannon entropy analysis                     | No (self-scan) | —                    |
|  9 | `-stubgen`    |           | Self-decrypting stub generator               | `<file.exe>`   | `stub_output.bin`    |
| 10 | `-trace`      |           | Source-to-binary trace mapping               | Optional: `-pid=<N>` | `trace_map.json` |
| 11 | `-agent`      |           | Autonomous agentic control loop              | No             | —                    |
| 12 | `-bbcov`      |           | Basic block coverage analysis                | No (self-scan) | `bbcov_report.json`  |
| 13 | `-covfuse`    |           | Static+dynamic coverage fusion               | No (self-scan) | `covfusion_report.json` |
| 14 | `-dyntrace`   |           | Runtime basic block tracing                  | `-pid=<N>`     | —                    |
| 15 | `-agenttrace` |           | Agent-driven coverage loop                   | `-pid=<N>`     | —                    |
| 16 | `-gapfuzz`    |           | Automated gap analysis + guided fuzzing      | No             | —                    |
| 17 | `-intelpt`    |           | Hardware-accelerated trace (queued)           | No             | —                    |
| 18 | `-diffcov`    |           | Differential coverage analysis               | No             | —                    |

### 2.1 Meta-Flags

| Flag        | Description                              | Combinable | Exit Behavior |
|-------------|------------------------------------------|------------|---------------|
| `--version` | Print version string and exit            | No         | Immediate exit |
| `--help`    | Print usage summary and exit             | No         | Immediate exit |
| `--json`    | Machine-readable JSON output to stdout   | Yes        | Modifies mode output |
| `--quiet`   | Suppress stdout, log-only                | Yes        | Modifies mode output |

---

## 3. Exit Code Semantics

| Code         | Meaning                          |
|--------------|----------------------------------|
| `0`          | Success — mode executed or usage printed |
| `0xC0000409` | FATAL — stack buffer overrun (GS violation) |

### Design Decision

All modes return **EXIT 0** including incomplete-argument cases (e.g., `-stubgen` without a file, `-inject` without `-pid`). This is intentional:

- The **program** executed successfully
- The **user intent** was incomplete, not erroneous
- This distinction enables automation: a non-zero exit means a genuine runtime fault, never a usage error

---

## 4. Output Format

### 4.1 Default (Human-Readable)

All output goes to `stdout` via `WriteConsoleA`. Format:

```
<Mode>: <status message>
<Mode>: <detail line 1>
<Mode>: <detail line 2>
Exiting...
```

### 4.2 Log File

Every invocation writes structured log entries to `rawrxd_ide.log`:

```
[YYYY-MM-DD HH:MM:SS] [LEVEL] <message>
```

Levels: `INFO`, `WARN`, `ERROR`, `DEBUG`

Latency is logged automatically for every CLI mode:

```
[2026-02-10 14:30:00] [INFO] Latency - -compile: 15 ms
```

### 4.3 Artifact Files

| File                 | Producer     | Format  |
|----------------------|-------------|---------|
| `trace_map.json`     | `-compile`, `-trace` | JSON |
| `bbcov_report.json`  | `-bbcov`    | JSON    |
| `covfusion_report.json` | `-covfuse` | JSON   |
| `encrypted.bin`      | `-encrypt`  | Binary  |
| `stub_output.bin`    | `-stubgen`  | Binary  |
| `rawrxd_ide.log`     | All modes   | Text    |

---

## 5. Self-Referential Modes

These modes operate on the **own executable** when no target is specified:

| Mode       | Self-Scan Behavior                    |
|------------|---------------------------------------|
| `-entropy` | Resolves own path via `GetModuleFileNameA`, computes Shannon entropy |
| `-avscan`  | Parses own PE headers, scans `.text` section for EICAR signatures |
| `-bbcov`   | Walks own PE `.text` section, enumerates basic blocks |
| `-covfuse` | Runs `-bbcov` internally, then correlates against `trace_map.json` |
| `-compile` | Generates `trace_map.json` inline (no external `ml64.exe`) |

---

## 6. Mode Argument Parsing

### 6.1 Global Arguments

Arguments are position-independent. The parser tokenizes all whitespace-delimited tokens, supports quoted paths, and identifies `-`/`/` prefixed switches.

### 6.2 Mode-Specific Arguments

| Argument        | Used By    | Format           | Example                      |
|-----------------|-----------|------------------|------------------------------|
| `-pid=<N>`      | `-inject`, `-trace` | Decimal integer | `-inject -pid=1234`     |
| `-pname=<name>` | `-inject`  | Process name     | `-inject -pname=notepad.exe` |
| `<file>`        | `-stubgen` | File path        | `-stubgen payload.exe`       |

### 6.3 Incomplete Arguments (Not Errors)

| Scenario                | Behavior                                    | Exit Code |
|-------------------------|---------------------------------------------|-----------|
| `-stubgen` (no file)    | Prints usage hint, exits                    | 0         |
| `-inject` (no pid/pname)| Prints usage hint, exits                    | 0         |
| `-trace` (no pid)       | Generates trace map only (no debug attach)  | 0         |

---

## 7. Security Scoping

| Mode         | Persistence | Side Effects                         |
|--------------|-------------|--------------------------------------|
| `-sideload`  | **None**    | Load → log → unload (system DLL)     |
| `-persist`   | Registry    | Writes `HKCU\...\Run\RawrXDService`  |
| `-uac`       | Registry    | ms-settings protocol hijack (cleans up) |
| `-inject`    | Memory      | Remote thread in target process       |

---

## 8. Threading Model

- Entry point: `_start_entry` → `MainDispatcher` → mode proc → `ExitProcess`
- Single-threaded execution (no worker threads in CLI mode)
- GUI mode: single-threaded console loop (`ShowGUIMenu` → dispatch → loop)
- All operations complete synchronously before exit

---

## 9. Platform Requirements

| Requirement        | Value                          |
|--------------------|---------------------------------|
| OS                 | Windows 10+ (x64)              |
| Subsystem          | Console (`/SUBSYSTEM:CONSOLE`) |
| APIs               | kernel32, user32, advapi32, shell32 |
| Privileges         | Standard user (most modes)     |
| Elevated           | `-uac` (triggers elevation), `-trace` with `-pid` (SeDebugPrivilege) |

---

## 10. Version History

| Version | Date       | Changes                                    |
|---------|------------|---------------------------------------------|
| 1.0     | 2026-02-10 | Initial contract freeze. 12 modes operational. All EXIT 0. Self-referential integrity confirmed. External dependency (`ml64.exe`) eliminated from `-compile`. |
| 1.1     | 2026-02-10 | Added modes 13-18 (`-covfuse`, `-dyntrace`, `-agenttrace`, `-gapfuzz`, `-intelpt`, `-diffcov`). Added meta-flags: `--version`, `--help`, `--json`, `--quiet`. All 20 entry points EXIT 0. `--quiet` gates `Print` (stdout suppression). `--json` sets global flag for per-mode adoption. |

---

## 11. Contract Guarantees

1. **Switch names are stable** — no renames without major version bump
2. **Exit code 0 = success or usage** — automation-safe
3. **Artifact filenames are stable** — `trace_map.json`, `bbcov_report.json`, `covfusion_report.json`
4. **Log format is stable** — `[timestamp] [LEVEL] message`
5. **Self-scan is the default** for `-entropy`, `-avscan`, `-bbcov`, `-covfuse`
6. **No external tool dependencies** — hermetic execution
7. **Meta-flags are additive** — `--json` and `--quiet` compose with any mode
8. **`--version` and `--help` are standalone** — override any mode if present

---

## 12. Future Extensions (Reserved, Not Yet Implemented)

| Flag        | Purpose                              | Status    |
|-------------|--------------------------------------|-----------|
| `--profile` | Performance profiling output         | Planned   |
| `--dry-run` | Parse args, validate, but don't execute | Planned |
| `--log=<path>` | Custom log file path              | Planned   |

These flags will be added in v1.2 without breaking existing behavior.
