## Final KB/kb Audit (Standalone `kb` Identifiers)

This audit targets **standalone `kb` identifiers** (e.g., `kb`, `$kb`, `size_kb`) which are ambiguous in this repo because they can mean:

- **Keybindings** (keyboard shortcuts)
- **Knowledge Base** (agent/digester “KB”)
- **Kilobytes** (`kB`/`KB` units)

### What was changed

- **C++ (keybinding variables)**
  - `src/settings_manager_real.cpp` / `src/settings_manager_real.hpp`: renamed standalone `kb` parameter/local variables in keybinding serialization/deserialization to `keybinding` / `binding`.
  - `src/win32app/Win32IDE_ShortcutEditor.cpp`: renamed standalone `kb` locals to `binding` for clarity.

- **C++ (kilobyte parsing)**
  - `src/core/memory_pressure_handler.cpp`: replaced standalone `kb` with `memAvailableKiB` when parsing `/proc/meminfo` and kept the kernel’s literal unit label `kB` in the format string.

- **PowerShell (knowledge base parameter)**
  - `scripts/ide_chatbot.ps1`: renamed constructor parameter `$kb` to `$knowledgeBase`.

- **Tests**
  - `tests/gguf_inference_cli.cpp` and `RawrXD-ModelLoader/tests/gguf_inference_cli.cpp`: renamed loop index `kb` to `kBlock`.
  - `tests/integration/soak_test_cot.py`: renamed `size_kb` to `size_kib` (the code multiplies by 1024 bytes, i.e. KiB semantics).

### Remaining standalone `kb` occurrences (known/accepted)

- **Third-party vendored source**: `src/core/sqlite3.c`
  - Contains legacy Microsoft support links with `/kb/` in comments:
    - `http://support.microsoft.com/kb/47961`
  - Left unchanged to avoid editing the SQLite amalgamation in this pass.

### Conventions going forward

- **Keybindings**: prefer `binding`, `keybinding`, or `keyBinding` — avoid `kb`.
- **Knowledge base**: prefer `knowledgeBase` — avoid `$kb`.
- **1024-based sizes**: prefer `KiB` naming in variables when multiplying/dividing by 1024.
