# Include Folder Audit (D lazy init)

Findings
- `include/orchestration/TaskOrchestrator.h`: References `RawrXD::Backend` and `RawrXD::AgenticToolExecutor`; build errors suggest either missing headers or namespace/types not defined/forward-declared.
- Qt types `QMenuBar`, `QStatusBar`, `QWidget` used by src/ProductionAgenticIDE require proper includes in source; headers themselves are fine in Qt.
- Compression headers: `zlib.h` not found for backup manager; project downloads zlib via FetchContent but include paths may need propagation.

Actions
- Validate orchestration headers are present and included wherever types are used.
- Ensure `ZLIB::zlib` target usage exports include dirs to affected targets, or gate features via config when unavailable.
- Keep all environment-specific values externalized; do not hardcode drive letters.

D Path Convention
- Use `LAZY_INIT_IDE_ROOT=D:\lazy init ide` to anchor runtime paths.
- Prefer `QDir::currentPath()` by launching from `D:\lazy init ide`.
