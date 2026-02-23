# 367 Scaffold Markers — Clarification for Automation / Copilot

**Use this when asked to "Begin massive 367 scaffold markers" or to apply/audit scaffold markers.** Answers the four clarification questions in one place.

---

## 1. Scope

- **What:** 367 numbered implementation-tracking markers (`SCAFFOLD_001` … `SCAFFOLD_367`) defined in the repo. Each marker corresponds to a specific area (Win32 IDE, Agent, Build, Integration, Docs, Audit, Misc).
- **Scope of work:** Use the markers to **reference** implementation points in code or docs (e.g. add `// SCAFFOLD_016` near the Agentic mode switcher), or to **audit/report** which markers are open vs done. Do **not** delete or renumber the 367 markers; they are the single source of truth.
- **Boundary:** Markers are listed in `SCAFFOLD_MARKERS.md` and `scaffold_markers_367.txt`; C/C++ code can include `include/scaffold_markers_367.h` and use `SCAFFOLD_NNN` (integer 1–367). Scripts that look for “placeholder” or “scaffold” **text** (e.g. `Convert-To-PureMASM.ps1`) are separate: they match phrases like `// TODO`, `scaffold`, `stub`, not the 367 marker IDs.

---

## 2. Targets

- **Primary:** `SCAFFOLD_MARKERS.md` (full registry: ID, Category, Description, File/Area, Status).
- **List form:** `scaffold_markers_367.txt` (one `SCAFFOLD_NNN` per line).
- **Code:** Any C/C++/ASM under `src/`, `Ship/`, `include/` where a marker is relevant; add a comment or `#if SCAFFOLD_NNN` using the header.
- **Docs:** `UNFINISHED_FEATURES.md`, audit reports, and task lists may reference `SCAFFOLD_NNN` by name.
- **Do not target:** Generated or third-party trees (e.g. `build/`, `node_modules/`, `third_party/`).

---

## 3. Marker format

- **Canonical name:** `SCAFFOLD_NNN` where NNN is zero-padded 001–367 (e.g. `SCAFFOLD_001`, `SCAFFOLD_016`, `SCAFFOLD_367`).
- **In code comments (C/C++/ASM):** Use one of:
  - `// SCAFFOLD_NNN` or `// SCAFFOLD_NNN: short description`
  - `; SCAFFOLD_NNN` (ASM)
  - `#include "scaffold_markers_367.h"` and then use the macro `SCAFFOLD_NNN` (value 1–367) for logging or `#if`.
- **Not used:** Generic tags like `// TODO: MASM-PORT-###` or free-form `// TODO:` for the **367** markers; those are separate. The 367 markers are **only** the identifiers `SCAFFOLD_001` … `SCAFFOLD_367` as defined in `SCAFFOLD_MARKERS.md` and `include/scaffold_markers_367.h`.

---

## 4. Script path

- **367 marker registry (read-only):**  
  - `SCAFFOLD_MARKERS.md` (root)  
  - `scaffold_markers_367.txt` (root)  
  - `include/scaffold_markers_367.h` (for code)

- **Placeholder/scaffold/stub *text* (patterns, not the 367 IDs):**  
  - `scripts/Convert-To-PureMASM.ps1` — scans for TODO/placeholder/scaffold/stub phrases; optional inventory: `reports/scaffold-marker-inventory.json`.  
  - `Convert-CppToPureMASM.ps1`, `Convert-ToPureMASM.ps1` (root) — similar pattern-based cleanup.  
  - `tools/enforce_no_scaffold.ps1` — enforces no scaffold/placeholder wording in source.

- **Enforce no scaffold in build:**  
  - CMake target `enforce_no_scaffold` runs `tools/enforce_no_scaffold.ps1`.

---

## Quick reference

| Question       | Answer                                                                 |
|----------------|------------------------------------------------------------------------|
| **Scope**      | 367 markers = implementation tracking; use to reference or audit only. |
| **Targets**    | SCAFFOLD_MARKERS.md, scaffold_markers_367.txt, src/Ship/include, docs. |
| **Marker fmt** | `SCAFFOLD_NNN` (001–367); in code: `// SCAFFOLD_NNN` or include header. |
| **Script path** | Registry: SCAFFOLD_MARKERS.md + include/scaffold_markers_367.h. Pattern scripts: scripts/Convert-To-PureMASM.ps1, tools/enforce_no_scaffold.ps1. |

When asked to **begin** or **apply** the 367 scaffold markers: (1) use the format above, (2) add or update only references/comments using `SCAFFOLD_NNN`, (3) leave the registry and header as the source of truth.
