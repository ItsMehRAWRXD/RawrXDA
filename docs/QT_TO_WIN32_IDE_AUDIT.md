# Qt → Win32 IDE / CLI — Full Audit (Not Stubbed, Not Scaffolded)

**Purpose:** Audit everything that is **not** stubbed or scaffolded and that **needs to be** present: i.e. Qt-era source that didn’t make it to the new Win32 IDE/CLI, or missing types/behavior that would break build or parity.  
**Date:** 2026-02-14.

---

## 1. Executive Summary

| Category | Status | Notes |
|----------|--------|--------|
| **Qt includes in build** | ✅ None | No `#include <Q` in src/ or Ship/ for Win32 IDE/CLI build. |
| **QtCompat::ThreadPool** | ✅ Fixed | Was referenced in Ship/RawrXD_Agent_Complete.hpp with no definition. C++20 impl added in Ship/ReverseEngineered_Internals.hpp (namespace RawrXD::QtCompat). |
| **Removed Qt paths** | ✅ N/A | src/settings.cpp, src/qtapp/ no longer in repo; Win32 equivalents in place. |
| **View / Git menu IDs** | ✅ Wired | View 2020–2031 and Git 3020–3024 are handled in Win32IDE_Commands.cpp (routeCommandUnified / handleViewCommand / handleGitCommand). |
| **TelemetryCollector** | ✅ Qt-free | src/agent/telemetry_collector.hpp|cpp — no QNetworkAccessManager; C++20/Win32. |
| **Legacy docs** | ⚠️ Stale | QT_DEPENDENCY_INVENTORY.md and TIER_2_TO_TIER_3_ROADMAP.h still reference removed paths (settings.cpp, qtapp, TelemetryCollector Qt); codebase is ahead of docs. |

**Conclusion:** The only **missing** Qt-origin artifact that wasn’t stubbed/scaffolded was **QtCompat::ThreadPool** (referenced but undefined). It is now implemented as a C++20 thread pool in Ship. All other Qt→Win32 gaps in the IDE/CLI are either already converted, documented as optional (ANSI→Unicode, layout restore), or refer to removed files.

---

## 2. Items Audited (Not Stub/Scaffold — Must Exist)

### 2.1 Ship — RawrXD_Agent_Complete.hpp

| Item | Was | Now |
|------|-----|-----|
| **QtCompat::ThreadPool** | Referenced (line 126); no definition anywhere in repo → **build break** if this TU were compiled. | **Defined** in Ship/ReverseEngineered_Internals.hpp: namespace RawrXD::QtCompat, class ThreadPool with Run(), WaitForDone(), ctor(size_t). Pure C++20 (std::thread, std::mutex, std::condition_variable, std::deque). |

### 2.2 Win32 IDE — View / Git

| Item | docs/QT_TO_WIN32_PARITY_AUDIT.md said | Actual code |
|------|----------------------------------------|-------------|
| View menu 2020–2027 | “Broken — handler expects 3001–3008” | Win32IDE_Commands.cpp has cases 2020–2031 and 3001/3007/3008/3009; Win32IDE.cpp menu uses 2020–2031. **Wired.** |
| Git menu 3020–3024 | “Broken — handler uses 8001–8005” | Win32IDE_Commands.cpp has cases 3020–3024 and 8001–8005; Win32IDE.cpp menu uses 3020–3024. **Wired.** |

### 2.3 Removed Qt Paths (No Action)

| Path | Status |
|------|--------|
| src/settings.cpp | Not in repo. Config via Registry/file/Win32. |
| src/qtapp/ (MainWindowSimple, etc.) | Not in repo. Replaced by src/win32app/Win32IDE*. |
| src/ui/monaco_settings_dialog.cpp (Qt version) | Current file is Win32 (no QDialog); inventory was stale. |

### 2.4 Telemetry / Logger

| Item | TIER_2 / inventory said | Actual |
|------|-------------------------|--------|
| Logger.cpp Qt removal | “DONE” / N/A | src/telemetry/logger.cpp exists; comment says “Structured JSON logging stub”; no Qt. |
| TelemetryCollector QNetworkAccessManager | “TODO: replace with WinHTTP” | telemetry_collector.hpp|cpp already Qt-free; callbacks, no Qt. |

### 2.5 Legacy headers (follow-up pass)

| Item | Status |
|------|--------|
| **include/mainwindow_qt_original.h** | Not in repo. Docs reference as legacy; Win32 IDE uses Win32IDE.cpp. |
| **include/agentic_iterative_reasoning.h** | Qt-free C++20 stub (agentic_agent_coordinator); no QObject/QString; in Win32/CLI build. |
| **Qt type names in src/Ship** | Only in comments (replaces Q..., no Qt). No Qt types in code. |

---

## 3. Optional / Deferred (Documented Elsewhere)

These are **not** required for “everything that isn’t stubbed/scaffolded” to be present; they are product/parity improvements.

| Item | Doc | Notes |
|------|-----|--------|
| ANSI → Unicode (Win32IDE.cpp) | QT_TO_WIN32_PARITY_AUDIT.md §4 | CreateWindowExA → W, MessageBoxA → W, etc. |
| Copy with formatting | Same | Plain text only; optional RTF/HTML to clipboard. |
| Layout restore / snapshots | Same | No restoreLayout equivalent; add if required. |
| Optional panels | WIN32_IDE_FEATURES_AUDIT | Network, CrashReporter, etc. — fix if included in default build. |

---

## 4. Stale References to Update (Optional)

| File | Suggestion |
|------|------------|
| **QT_DEPENDENCY_INVENTORY.md** | Mark Phase 0 items for settings.cpp, qtapp, monaco_settings_dialog as **removed / superseded** (paths no longer exist or are already Win32). |
| **TIER_2_TO_TIER_3_ROADMAP.h** | Update “TelemetryCollector Qt dependency removal” to **DONE** (telemetry_collector is Qt-free). |

---

## 5. Verification

- **No Qt symbol in Win32 IDE/CLI build:**  
  `grep -r "#include <Q" src/ Ship/ --include="*.cpp" --include="*.h" --include="*.hpp"` → no matches (except comments).
- **QtCompat::ThreadPool defined:**  
  `grep -n "QtCompat\|ThreadPool" Ship/ReverseEngineered_Internals.hpp` → namespace QtCompat and class ThreadPool present.
- **View/Git IDs:**  
  `grep -n "2020\|2027\|3020\|3024" src/win32app/Win32IDE_Commands.cpp src/win32app/Win32IDE.cpp` → menu and handler both use same IDs.

---

## 6. Cross-References

| Document | Content |
|----------|---------|
| **docs/QT_TO_WIN32_PARITY_AUDIT.md** | Qt-era vs Win32 status; View/Git were listed broken but are wired in code. |
| **docs/WIN32_IDE_FEATURES_AUDIT.md** | What’s working in Win32 GUI. |
| **UNFINISHED_FEATURES.md** | Stubs/scaffolds list; ALL STUBS table. |
| **QT_DEPENDENCY_INVENTORY.md** | Historical; many entries refer to removed files. |
