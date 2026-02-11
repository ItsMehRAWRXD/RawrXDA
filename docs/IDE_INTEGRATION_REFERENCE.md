# RawrXD IDE Integration - Complete Reference

## Overview

Three IDE integration options are now available, each optimized for different use cases:

| Component | File | Use Case |
|-----------|------|----------|
| C++ Pipe Client | `include/RawrXD_PipeClient.h` | IDE plugins (VS, VSCode, JetBrains) |
| FileSystemWatcher | `scripts/Watch-PatternChanges.ps1` | Real-time monitoring with auto-fix |
| MASM Native Host | `bin/RawrXD_NativeHost.exe` | Zero-overhead standalone server |

---

## 1. C++ Pipe Client (`RawrXD_PipeClient.h`)

### Features
- **Full C++17 header-only library**
- **JSON response parsing** (no external dependencies)
- **C-style API exports** for FFI (Python, C#, Lua)
- **Automatic reconnection**
- **Statistics tracking**

### Usage in C++
```cpp
#include "RawrXD_PipeClient.h"

RawrXD::PipeClient client;
if (client.Connect()) {
    auto result = client.ClassifyFile("D:\\project\\main.cpp");
    
    if (result.IsCritical()) {
        // BUG or high-priority FIXME
        ShowError(result.typeName, result.lineNumber);
    }
    else if (result.HasMatch()) {
        AddTODO(result.typeName, result.lineNumber, result.content);
    }
}
```

### C API for FFI
```c
// Define RAWRXD_EXPORT_C_API before including
#define RAWRXD_EXPORT_C_API
#include "RawrXD_PipeClient.h"

// Initialize
RawrXD_Init(NULL);  // Uses default pipe name

// Classify
double confidence;
int line, priority;
int pattern = RawrXD_ClassifyFile("file.cpp", &confidence, &line, &priority);

// Get pattern name
const char* name = RawrXD_GetPatternName(pattern);  // "TODO", "FIXME", etc.

// Cleanup
RawrXD_Shutdown();
```

### Pattern Types
| ID | Name | Priority |
|----|------|----------|
| 0 | Unknown | N/A |
| 1 | TODO | Medium |
| 2 | FIXME | High |
| 3 | XXX | High |
| 4 | HACK | Medium |
| 5 | BUG | Critical |
| 6 | NOTE | Low |
| 7 | IDEA | Low |
| 8 | REVIEW | Medium |

---

## 2. FileSystemWatcher (`Watch-PatternChanges.ps1`)

### Features
- **Real-time monitoring** using .NET FileSystemWatcher
- **Pattern caching** with configurable TTL
- **IDE notification** via named pipe
- **Auto-fix engine** for safe, low-risk patterns
- **Rollback support** with TODO-RollbackSystem.psm1

### Basic Usage
```powershell
# Watch a directory
.\Watch-PatternChanges.ps1 -WatchPath "D:\project\src"

# With specific extensions
.\Watch-PatternChanges.ps1 -WatchPath "." -Extensions @("*.cpp", "*.h")

# Enable caching (30-second TTL)
.\Watch-PatternChanges.ps1 -EnableCache -CacheTTLSeconds 30

# Enable auto-fix with rollback
.\Watch-PatternChanges.ps1 -AutoFix -CreateRollback

# Send notifications to IDE
.\Watch-PatternChanges.ps1 -NotifyIDE -NotifyPipeName "\\.\pipe\MyIDE_Notify"

# Filter by priority
.\Watch-PatternChanges.ps1 -MinPriority High
```

### Parameters
| Parameter | Default | Description |
|-----------|---------|-------------|
| `-WatchPath` | `D:\lazy init ide\src` | Directory to watch |
| `-Extensions` | `*.ps1, *.cpp, *.h, ...` | File patterns to monitor |
| `-IncludeSubdirectories` | `$true` | Recursive watch |
| `-DebounceMs` | `500` | Debounce interval |
| `-MinPriority` | `All` | Minimum priority filter |
| `-EnableCache` | `$false` | Enable pattern caching |
| `-CacheTTLSeconds` | `30` | Cache lifetime |
| `-AutoFix` | `$false` | Enable auto-fix engine |
| `-CreateRollback` | `$false` | Create rollback points |
| `-NotifyIDE` | `$false` | Send IDE notifications |
| `-NotifyPipeName` | `\\.\pipe\RawrXD_IDE_Notify` | IDE pipe name |
| `-OutputLog` | `$null` | Log file path |

### Auto-Fix Safety
The auto-fix engine only processes **low-risk patterns**:
- `// TODO: remove this` → Line removed
- `// TODO: uncomment` → Comment prefix removed
- `// FIXME: delete` → Line removed

All other patterns require manual review.

---

## 3. MASM Native Host (`RawrXD_NativeHost.exe`)

### Features
- **Pure MASM64 executable** (6.6 KB)
- **Zero PowerShell overhead**
- **AVX-512 detection** at startup
- **Named pipe server** for client connections
- **JSON protocol** compatible with PipeClient

### Build
```batch
ml64 /c src\RawrXD_NativeHost.asm
link /SUBSYSTEM:CONSOLE /LARGEADDRESSAWARE:NO /ENTRY:main ^
     RawrXD_NativeHost.obj kernel32.lib user32.lib ^
     /OUT:bin\RawrXD_NativeHost.exe
```

### Usage
```powershell
# Start server (default pipe: \\.\pipe\RawrXD_PatternBridge)
.\bin\RawrXD_NativeHost.exe

# With custom pipe name (planned)
.\bin\RawrXD_NativeHost.exe --pipe "\\.\pipe\MyPipe"
```

### Protocol
The server uses a length-prefixed message protocol:

**Request Format:**
```
[4 bytes: command length][command string]
[4 bytes: data length][data bytes]  (for CLASSIFY command)
```

**Commands:**
| Command | Description | Data Required |
|---------|-------------|---------------|
| `PING` | Health check | No |
| `CLASSIFY` | Classify buffer | Yes (buffer bytes) |
| `FILE` | Classify file | Yes (file path) |
| `STATS` | Get statistics | No |
| `INFO` | Get engine info | No |
| `QUIT` | Shutdown server | No |

**Response Format:**
```
[4 bytes: response length][JSON response]
```

### Example Responses
```json
// PING
{"Status":"PONG","Version":"1.0.0"}

// INFO
{"Engine":"RawrXD_NativeHost","Version":"1.0.0","Mode":"AVX-512","CPU":"x64+AVX512"}

// STATS
{"TotalRequests":42,"TotalMatches":15,"TotalBytes":8192,"Uptime":60000}

// CLASSIFY
{"Pattern":"TODO","Confidence":0.88,"Line":1,"Priority":1}
```

---

## Integration Examples

### VS Code Extension
```typescript
import * as net from 'net';

const PIPE_NAME = '\\\\.\\pipe\\RawrXD_PatternBridge';

async function classifyDocument(text: string): Promise<PatternResult> {
    return new Promise((resolve, reject) => {
        const client = net.connect(PIPE_NAME, () => {
            // Send CLASSIFY command
            const cmd = Buffer.from('CLASSIFY');
            const cmdLen = Buffer.alloc(4);
            cmdLen.writeInt32LE(cmd.length);
            
            const data = Buffer.from(text, 'utf8');
            const dataLen = Buffer.alloc(4);
            dataLen.writeInt32LE(data.length);
            
            client.write(cmdLen);
            client.write(cmd);
            client.write(dataLen);
            client.write(data);
        });
        
        client.on('data', (response) => {
            const len = response.readInt32LE(0);
            const json = response.slice(4, 4 + len).toString();
            resolve(JSON.parse(json));
            client.end();
        });
    });
}
```

### Python Integration
```python
import ctypes
import os

# Load the DLL
dll = ctypes.CDLL("bin/RawrXD_AVX512_v2.dll")

# Define function signatures
dll.InitializePatternEngine.restype = ctypes.c_int
dll.ClassifyPattern.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(ctypes.c_double)]
dll.ClassifyPattern.restype = ctypes.c_int

# Initialize
dll.InitializePatternEngine()

# Classify
text = b"// TODO: implement this"
confidence = ctypes.c_double()
pattern = dll.ClassifyPattern(text, len(text), None, ctypes.byref(confidence))

PATTERNS = ['Unknown', 'TODO', 'FIXME', 'XXX', 'HACK', 'BUG', 'NOTE', 'IDEA', 'REVIEW']
print(f"Pattern: {PATTERNS[pattern]}, Confidence: {confidence.value:.2f}")

# Cleanup
dll.ShutdownPatternEngine()
```

---

## File Locations

```
D:\lazy init ide\
├── bin\
│   ├── RawrXD_AVX512_v2.dll       # Pattern engine DLL (AVX-512)
│   └── RawrXD_NativeHost.exe      # MASM native server
├── include\
│   └── RawrXD_PipeClient.h        # C++ pipe client header
├── scripts\
│   ├── Watch-PatternChanges.ps1   # FileSystemWatcher v2.0
│   ├── Resolve-TODO-v2.ps1        # Full scanner with P/Invoke
│   └── TODO-RollbackSystem.psm1   # Rollback module
└── src\
    ├── RawrXD_AVX512_SIMD.asm     # AVX-512 pattern engine source
    └── RawrXD_NativeHost.asm      # Native host source
```

---

## Performance

| Component | Latency | Throughput |
|-----------|---------|------------|
| DLL (AVX-512) | ~41 μs | 24,204 ops/s |
| DLL (Scalar) | ~63 μs | 15,811 ops/s |
| Native Host | ~50 μs | ~20,000 ops/s |
| FileSystemWatcher | <100 ms | Real-time |

---

## Quick Start

1. **Start the server:**
   ```powershell
   .\bin\RawrXD_NativeHost.exe
   ```

2. **Monitor files:**
   ```powershell
   .\scripts\Watch-PatternChanges.ps1 -WatchPath "D:\project" -AutoFix
   ```

3. **Run full scan:**
   ```powershell
   .\scripts\Resolve-TODO-v2.ps1 -Path "D:\project" -GenerateReport
   ```
