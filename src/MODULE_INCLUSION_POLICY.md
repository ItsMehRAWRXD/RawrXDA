// ============================================================================
// MODULE_INCLUSION_POLICY.md — Canonical Include Hierarchy & Rules
// ============================================================================
// FIX #7: Unified module inclusion consistency across RawrXD
// 
// PURPOSE:
//   - Establish canonical include order and patterns
//   - Eliminate circular dependencies
//   - Create single source of truth for dependency relationships
//   - Enable automated consistency checking
// ============================================================================

## LAYER HIERARCHY (Bottom→Top)

### LAYER 0: Platform Abstractions (No internal dependencies)
- `<windows.h>` — Win32 API (system)
- `<cstdint>`, `<cstddef>`, `<cstring>` — C++ stdlib (system)
- `<vector>`, `<string>`, `<map>`, `<mutex>` — STL containers (system)

### LAYER 1: Core Utilities (Depend only on Layer 0)
Location: `src/utils/`
- `ErrorReporter.h` — Error reporting utilities
- `logger_interface.h` — Logging interface
Classes: Must NOT include Layer 2+ headers

### LAYER 2: Collections & Data Structures (Depend on Layer 0-1)
Location: `src/core/`, `src/collections/`
- `buffer.h` — Memory buffer abstractions
- `diagnostic_registry.cpp` — Diagnostic data structures
Classes: Must NOT include Layer 3+ headers (Win32IDE, Agentic)

### LAYER 3: Subsystems (Depend on Layer 0-2)
Location: `src/core/`, `src/agentic/`
- `ToolRegistry.h` — Tool registry (Agentic subsystem)
- `WindowManager.h` — Window lifecycle (Win32 subsystem)
- `TemplatizedEmbeddingEngine.hpp` — ML engine
Classes: May import from each other (same layer) if no cycles

### LAYER 4: Application Integration (Top level)
Location: `src/win32app/`
- `Win32IDE.h` — Main IDE class
- `Win32IDE_Commands.cpp` — Command handlers
Classes: May depend on all layers below

## CANONICAL #include ORDERING

For each translation unit (`.cpp` file), use this order:

```cpp
// 1. Include guard or pragma (for .h files)
#pragma once

// 2. System headers (stdlib, Win32)
#include <windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <mutex>

// 3. Third-party library headers (nlohmann/json, etc.)
#include <nlohmann/json.hpp>

// 4. Own public header (for .cpp files only)
#include "MyClass.h"

// 5. Dependent module headers (Layer 0→N)
// Group by layer, within layer group alphabetically
#include "logger_interface.h"
#include "diagnostic_registry.h"
#include "ToolRegistry.h"
#include "WindowManager.h"

// 6. Internal implementation headers (not public)
#include "internal_helpers.hpp"
```

## FORBIDDEN PATTERNS (Circular Dependencies)

❌ **CIRCULAR:**
```cpp
// Window.h
#include "EditorPane.h"

// EditorPane.h
#include "Window.h"  // CIRCULAR!
```

✅ **FIX (forward declaration):**
```cpp  
// Window.h
class EditorPane;  // Forward declare

class Window {
    EditorPane* pane;  // Pointer OK without full definition
};

// Window.cpp
#include "EditorPane.h"  // Full definition in .cpp
```

❌ **INCONSISTENT:**
```cpp
// file1.cpp
#include "MyHeader.h"
#include <vector>
#include "OtherHeader.h"  // MIXED ORDERING

// file2.cpp
#include <vector>
#include "MyHeader.h"     // Different order
#include "OtherHeader.h"
```

✅ **CONSISTENT (canonical order applied everywhere):**
```cpp
// All .cpp files follow same order
#include <system-headers>
#include <third-party>
#include "local-headers"
```

## DEPENDENCY MATRIX

```
Win32IDE_Commands.cpp
  ├─ Win32IDE.h [Layer 4]
  ├─ WindowManager.h [Layer 3]
  ├─ EditorOperations.h [Layer 3]
  └─ RouterOperations.h [Layer 3]
      ├─ ToolRegistry.h [Layer 3]
      └─ logger_interface.h [Layer 1]

AgentOrchestrator.cpp
  ├─ ToolRegistry.h [Layer 3]
  └─ <nlohmann/json.hpp>
```

## AUTOMATED VERIFICATION RULES

### Rule 1: No System Headers After Local Headers
Each `.cpp`/`.h` must follow:
1. `#pragma once` (if .h)
2. System headers (`<...>`)
3. Third-party (`<...>`)
4. Local headers (`"..."`)

**Pattern:** `^#include\s+(?:"` must come after all `^#include\s+<`

### Rule 2: No Circular Includes
For every `#include "Header.h"`, verify Header.h does NOT `#include` any file that includes this file.

**Detection:** Build with `--warn-cycle` or static analysis tool

### Rule 3: Layer Isolation
- Layer N files must NOT `#include` from Layer N+1
- Layer 4 may depend on all; Layer 0 may depend on none

**Validation:** Parse dependency tree, enforce DAG (directed acyclic graph)

### Rule 4: Alphabetical Within Group
```cpp
#include <cstdint>      // Sort alphabetically
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>
```

**Not this:**
```cpp
#include <vector>       // ❌ OUT OF ORDER
#include <cstdint>
#include <string>
```

## REMEDIATION CHECKLIST

For each module, perform:

- [ ] **Audit:** Extract all `#include` statements
- [ ] **Classify:** Map each to layer (0, 1, 2, 3, or 4)
- [ ] **Detect Cycles:** Check for mutual includes
- [ ] **Reorder:** Apply canonical ordering (sys → 3rd → local)
- [ ] **Alphabetize:** Within each group, sort by filename
- [ ] **Verify:** Rebuild with strict -Werror=cycle warnings
- [ ] **Document:** Add module dependency diagram to module header

## VERIFICATION COMMANDS

```bash
# Check for circular dependencies
# (requires clang or similar):
clang++ -Weverything -fmodules -fmodules-strict-implicit-module-maps ...

# Manual check via grep:
for f in src/**/*.cpp; do
  echo "=== $f ===";
  grep "^#include" "$f" | sort;
done | tee include_audit.txt
```

## LAYER ASSIGNMENT REFERENCE

| Module | Layer | Reason |
|--------|-------|--------|
| `utils/ErrorReporter.h` | 0-1 | No internal deps |
| `core/ToolRegistry.h` | 3 | Depends on Layer 1 logging |
| `win32app/WindowManager.h` | 3 | Depends on Lock (Layer 1) |
| `win32app/Win32IDE.h` | 4 | Depends on Layer 3 components |
| `<windows.h>` | 0 | System |
| `<vector>` | 0 | System |

---
**Status:** FIX #7 reference document
**Last Updated:** 2026-03-16
**Owner:** RawrXD Build Engineering
