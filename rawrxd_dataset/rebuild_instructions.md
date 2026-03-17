# RawrXD Deterministic Rebuild Instructions
Generated: 2026-02-20T16:50:24
Version: 14.2.0
Files: 2639 | Lines: 1279928

## Boot Sequence (White-Screen Prevention Checklist)

### 1. Entry Points
- WinMain: PRESENT
- WindowProc count: 106
- Thread procs: 63

### 2. Critical Win32 Messages
- WM_CREATE : Present - WM_PAINT : Present - WM_DESTROY : Present - WM_COMMAND : Present - WM_SIZE : Present - WM_CLOSE : Present

### 3. Rendering Subsystem
Status: Direct2D


### 4. Command Dispatch
Handlers registered: 436
Table entries: 1274


### 5. Subsystem Status
| Subsystem | Status |
|-----------|--------|
| Crypto | Integrated | | IO | Win32IO | | LLM | Integrated | | LSP | Client | | MASM | Active | | Neural | Active | | Rendering | Direct2D | | Streaming | Present | | UI | Win32 | | Vulkan | Present |

### 6. MASM Bridges
- ASM Exports: 697
- ASM Imports: 412
- ASM Procedures: 1138
- C++ extern "C" decls: 321

### 7. Agent System
- Tools : 27 - Models : 104 - Workers : 0 - Hooks : 10 - Orchestrators : 78

### 8. Entropy / Random Bucket
- Unmapped Functions: 1659
- Orphan ASM Exports: 4
- Mystery Structs: 48
- **Total Entropy: 1711**

> **Rule**: `random.json` must be empty before release.
> If it is not empty, the build has undefined behavior.
