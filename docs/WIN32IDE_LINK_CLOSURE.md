# Win32IDE link closure (symbol ownership)

Short reference for **which translation unit (TU) owns** `extern "C"` symbols that the IDE and `universal_model_router` expect, so we do not define the same C name twice (LNK2005 / MSVC C2733 on mismatched ABI).

## Universal model loader + beacon (`universal_model_router.cpp`)

**Declared in:** `src/universal_model_router.cpp` (lines ~23–40): the router calls these with **wide paths** and **int** return codes.

| Symbol | Contract (summary) | Implementations |
|--------|--------------------|-----------------|
| `LoadModel(const wchar_t*)` | Success **0**, failure **-1** (after `ModelLoaderInit`) | ASM loader **or** `src/core/model_loader_fallbacks.cpp` |
| `ModelLoaderInit(void)` | Success **0** | Same |
| `UnloadModel(void)` | void | Same |
| `HotSwapModel(const wchar_t*, char)` | Success **1**, failure **0** (`UniversalModelRouter::hotSwapModel`) | Same |
| `GetTensor(const char*)` | Pointer or `nullptr` | Same |
| `GetCurrentModelPath(void)` | `const wchar_t*` — must stay valid for caller | Same; fallback uses **thread-local** copy after mutex |
| `GetModelLoadTimestamp(void)` | `unsigned long long` | Same |
| `BeaconRouterInit`, `BeaconSend`, `BeaconRecv`, `TryBeaconRecv`, `RegisterAgent` | Beacon pipe surface | Same fallback file (no-op / not-ready) |

**Canonical fallback:** `src/core/model_loader_fallbacks.cpp` — must stay **signature-identical** to the `extern "C"` block in `universal_model_router.cpp` (not legacy `bool LoadModel(const char*)`).

**Also in fallback (router-adjacent):** `ModelLoaderShutdown` — used where shutdown is linked.

## `RawrXD_Native_Log`

- **Definition (variadic):** `src/core/rawrxd_native_log_fallback.cpp` — `extern "C" void RawrXD_Native_Log(const char* fmt, ...)`.
- **Do not** add a second non-variadic `RawrXD_Native_Log(const char*, const char*)` in `rawr_engine_link_shims.cpp` (or anywhere else in the same executable): that causes **C2733** / wrong ABI vs the fallback.
- **Call sites** in shims should use the printf-style API, e.g. `RawrXD_Native_Log("[SO_STATS] %s", buf);`.
- `rawr_engine_link_shims.cpp` keeps a **single forward declaration** at the top and **no** duplicate definition.

## `Enterprise_DevUnlock`

- **Canonical C ABI:** `extern "C" int64_t Enterprise_DevUnlock()` in `src/core/enterprise_devunlock_bridge.cpp`.
- **Do not** stub the same symbol in `model_loader_fallbacks.cpp` or `rawr_engine_link_shims.cpp` with a different return type (would be **LNK2005** or ODR violations).

## CMake / when fallbacks link

With **`RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=ON`** (default dev lane), Win32IDE appends units such as:

- `src/core/model_loader_fallbacks.cpp`
- `src/core/rawr_engine_link_shims.cpp`
- `src/core/rawrxd_native_log_fallback.cpp`
- `src/core/enterprise_devunlock_bridge.cpp`

Production strict lanes may strip some of these; then every symbol above must be provided by real ASM or other TUs without duplicates.

## If `RawrXD-Win32IDE.exe` is missing after a “successful” link

- **Antivirus / Defender:** quarantine or blocking of new `.exe`.
- **File still locked:** IDE or another process holding `bin\RawrXD-Win32IDE.exe` (PRE_LINK `del` is best-effort).
- **Output location:** check `${CMAKE_BINARY_DIR}/bin/` and the active config (`Release`/`Debug`) for multi-config generators.

---

**Related:** `docs/TASK_BACKLOG_PRODUCTION_AGENTIC_RUNTIME_BATCH1.md`, `docs/CHANGELOG.md` (Win32IDE / fallback notes).
