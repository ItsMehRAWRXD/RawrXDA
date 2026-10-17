# Qt compatibility runtime: source audit, 2026-10-16

## Scope and truth-in-status

Inspected the GitHub default branch `backup/20260316-ide` (base SHA `49f5088d7578845f255ed68acca20058224b05b2`), not the local, uncommitted `F:\rawrxd` working tree. `master` is 3,373 commits behind the current default branch (GitHub comparison at audit time). Historical/reconstructed/archived copies are not assumed to be production sources. No Windows build, unit tests, CLI run, or IDE GUI smoke was executed by this remote-only audit.

Do not merge or market this as "Qt fully removed", "production ready", or "tests passed" until the gates below succeed. Qt names exposed by native shims are still an API-compatibility layer, **not** a complete Qt implementation.

## Direct findings

| Priority | Active source | Evidence | Disposition |
| --- | --- | --- | --- |
| P0 | `src/core/qt_elimination.h` | `QTimer::start()` previously detached worker with captured `this`; destructor called `stop()` but did not join | Replaced with joinable, cancellable worker in this branch |
| P0 | `include/qt_block.h` | `QTimer::start()/stop()/setInterval()` did nothing; `isActive()` always false | Replaced with worker scheduler and explicit `onTimeout` callback; unsupported string-based slot dispatch is deleted (compile-time rejection) |
| P0 | `include/qt_block.h` | `QObject::connect()` is variadic no-op | **Open**: replace call sites with explicit native callback registration; do not misrepresent UI signal functionality |
| P0 | `include/qt_block.h` | `QProcess::start()` no-op, `execute()` returns zero without running anything | **Open**: implement restricted, audited CreateProcessW pipe/process wrapper or port consumers to existing native terminal authority |
| P0 | `include/qt_block.h` | `QJsonDocument::fromJson()` returns empty; `toJson()` returns literal `{}` | **Open**: replace with a real bounded UTF-8 JSON parser/serializer; wire parse error propagation |
| P1 | `include/qt_block.h` | `QSettings` used in-memory map with empty `sync()`; groups ignored | Replaced with persisted, typed, grouped app-data store; does not claim Qt INI-format compatibility |
| P1 | `include/qt_block.h` | `QWidget::show()/hide()/setGeometry()` no-ops, `isVisible()` false | **Open**: use real Win32 HWND controls and event handling, not state-only fake widgets |
| P1 | `src/core/qt_elimination.h` | `QJsonDocument::object()` always returns empty object; `Q_ASSERT` expands to no-op | **Open**: real JSON implementation and assertion handling |
| P1 | `src/QtReplacements.hpp` | `QTimer::start(int)` only flips a bool | **Open**: consolidate to **one** native timer provider after include graph proves compatibility |
| P1 | `reconstructed/RawrXD/src/QtGUIStubs.hpp` and `Full Source/src/QtGUIStubs.hpp` | many widget calls are literal no-ops/constant return values | **Archived/copy lane, not automatically an active target**; do not bulk-edit before proving compilation ownership |
| P0 (integration) | `CMakeLists.txt` | `RAWRXD_PRODUCTION_STRIP_STUB_SOURCES` defaults OFF. `RAWRXD_BUILD_CLI=ON` is explicitly incompatible with strict strip mode | **Open**: choose true real providers for CLI and Win32IDE and make verified production profiles fail closed |
| P0 (dependency) | `CMakeLists.txt` | strict profile explicitly requires an external QuickJS provider or fails | **Open**: determine which target requires this and provide in-tree implementation or a native alternative |

## Patch surface

1. `src/core/qt_elimination.h`: joinable thread with cancellation and wake-on-stop (no detached worker retaining a destroyed `QTimer`).
2. `include/qt_block.h`: real callback-based timer; unsupported Qt MOC string-slot overload deleted rather than silently ignored. Timer callbacks execute on a worker, not the Win32 UI thread.
3. `include/qt_block.h`: `QVariant` numeric/boolean conversion and `QSettings` typed persistence to `%APPDATA%\<org>\<app>.settings` using hex-encoded records and atomic file replacement on Windows.
4. `tests/qt_block_runtime_smoke.cpp`: launchable C++20 assertions for persisted bool/int/double/string/binary, groups, timer firing, cancellation, and worker lifetime.

Limitations: `QSettings` is a **native binary-safe compat format**, not interoperable with existing Qt registry/INI storage; existing Qt preferences require a migration step. The timer cannot implement Qt thread-affinity or MOC dispatch; callers must marshal UI work to the HWND thread. Do not invoke destruction of a timer from its own callback. Settings concurrency across processes is not yet serialized.

## Verification commands (Windows x64, MSVC Developer Command Prompt)

From your checked-out audit branch:

```bat
git fetch origin
git switch audit/qt-runtime-native-20261016
cl /nologo /std:c++20 /EHsc /W4 tests\qt_block_runtime_smoke.cpp /Fe:qt_block_runtime_smoke.exe
qt_block_runtime_smoke.exe
```

If the self-test fails to compile because of **pre-existing errors elsewhere** in `qt_block.h`, preserve the exact compiler diagnostics; do not claim the patch passes. That failure becomes its own build-compatibility gate.

Next, on the actual Windows production checkout:

```bat
cmake -S . -B build_cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DRAWRXD_PRODUCTION_STRIP_STUB_SOURCES=ON -DRAWRXD_ENABLE_HEAVY_GATES=ON
cmake --build build_cmake --target RawrXD-Win32IDE --config Release
ctest --test-dir build_cmake -C Release --output-on-failure
```

These are **commands to run, not receipts**; strict configure may fail because of pre-existing required providers (including QuickJS).

## Remaining order of work

1. Locate the actual production compile/include graph for the Win32IDE, CLI, `src/core/qt_elimination.h`, `include/qt_block.h`, and `src/QtReplacements.hpp`. Make one owner per API and reject duplicate shadow shims.
2. Replace `QObject::connect` and `QProcess` no-ops; exercise a real agent terminal task end to end.
3. Replace Qt JSON stubs with validated parser/serializer and real error states.
4. Port remaining GUI consumers to actual Win32 controls, WM_COMMAND/WM_NOTIFY, layout, focus, redraw, and lifetime handling.
5. Re-run production-strip, CLI, IDE, snapshot/agent, offline, security, and crash/endurance gates. Capture exact commands, exit codes, revision SHA, and test receipts before merging.

## Repository caution

The GitHub connector cannot inspect or push uncommitted local source on `F:\rawrxd`. This branch contains **only the explicitly audited remote patches** listed here, not "everything" in the user's local checkout. PR should target the default branch `backup/20260316-ide`, **not** the older `master`.
