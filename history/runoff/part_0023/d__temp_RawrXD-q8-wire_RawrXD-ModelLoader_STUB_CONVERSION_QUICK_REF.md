# Stub to Utility Conversion - Quick Reference

## Files Modified

### 1. Win32IDE.cpp
**Path:** `d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\src\win32app\Win32IDE.cpp`

#### Changes:
- **Line ~960:** WM_USER+100 handler - Added streaming token display logic
- **Line ~3240:** `refreshModuleList()` - Converted to dynamic PowerShell module discovery

### 2. StreamingGGUFLoader.cpp
**Path:** `e:\src\qtapp\gguf\StreamingGGUFLoader.cpp`

#### Changes:
- **Line 1-5:** Added headers: `<QFileInfo>`, `<chrono>`, `<cstring>`
- **Line ~19:** `BuildTensorIndex()` - Full GGUF format parsing (143 lines)
- **Line ~143:** `LoadZone()` - Memory-mapped zone loading (68 lines)
- **Line ~212:** `GetTensorData()` - Tensor data extraction (49 lines)

---

## API Reference

### Win32IDE::HandleCopilotStreamUpdate (via WM_USER+100)
```cpp
// Send streaming token to Copilot chat output
std::string* token = new std::string("Hello, world!");
PostMessage(ideWindow, WM_USER+100, (WPARAM)token, 0);
// Note: Handler will delete the token pointer
```

### Win32IDE::refreshModuleList()
```cpp
// Dynamically discover PowerShell modules (requires active terminal)
ide.refreshModuleList();

// Access discovered modules
for (const auto& mod : ide.m_modules) {
    std::cout << mod.name << " v" << mod.version << std::endl;
}
```

### StreamingGGUFLoader::BuildTensorIndex()
```cpp
StreamingGGUFLoader loader;
if (loader.Open("model.gguf")) {
    if (loader.BuildTensorIndex()) {
        qDebug() << "Indexed" << loader.tensorIndex_.size() << "tensors";
    }
}
```

### StreamingGGUFLoader::LoadZone()
```cpp
// Load a specific zone (256MB chunk of the model)
if (loader.LoadZone("zone_0")) {
    qDebug() << "Zone 0 loaded successfully";
}
```

### StreamingGGUFLoader::GetTensorData()
```cpp
std::vector<uint8_t> tensorData;
if (loader.GetTensorData("model.layers.0.attn.q.weight", tensorData)) {
    qDebug() << "Retrieved" << tensorData.size() << "bytes";
    // Use tensorData...
}
```

---

## Performance Expectations

| Operation | Typical Duration | Notes |
|-----------|-----------------|-------|
| `refreshModuleList()` | 10-50ms | Async (PowerShell response comes later) |
| `BuildTensorIndex()` | 100-500ms | Depends on tensor count |
| `LoadZone(256MB)` | 50-200ms | Depends on disk speed |
| `GetTensorData()` | 10-100µs | Depends on tensor size |

---

## Error Handling Patterns

### All functions return `bool` or handle errors gracefully:

```cpp
// Pattern 1: Bool return with logging
if (!loader.BuildTensorIndex()) {
    qWarning() << "Failed to build tensor index";
    // Check logs for specific reason
}

// Pattern 2: Null checks before use
if (pane && pane->manager && pane->manager->isRunning()) {
    // Safe to use
}

// Pattern 3: Bounds validation
if (offsetInZone + tensor.size_bytes > zone.size_bytes) {
    qWarning() << "Tensor data exceeds zone boundary";
    return false;
}
```

---

## Logging Examples

### Win32IDE (IDELogger)
```cpp
LOG_FUNCTION();  // Entry point
LOG_DEBUG("Processing token: " + token);
LOG_INFO("Module list refreshed");
LOG_WARNING("No active PowerShell session");
LOG_ERROR("Failed to parse GGUF header");
```

### StreamingGGUFLoader (Qt)
```cpp
qDebug() << "Zone boundaries: start=" << alignedStart;
qInfo() << "Indexed" << tensorCount << "tensors";
qWarning() << "Failed to memory map zone" << zoneName;
```

---

## Memory Management

### WM_USER+100 Handler
```cpp
// CALLER must allocate on heap:
std::string* token = new std::string("data");
PostMessage(hwnd, WM_USER+100, (WPARAM)token, 0);

// HANDLER will delete:
delete pToken;  // Automatic cleanup
```

### Zone Memory Mapping
```cpp
// QFile handles mmap/munmap automatically via RAII
// Explicit cleanup:
loader.UnloadZone("zone_0");  // Unmaps memory
loader.Close();  // Unmaps all zones
```

---

## Configuration

### Module Discovery
- **Requirement:** Active PowerShell terminal (`TerminalPane` with running `Win32TerminalManager`)
- **Fallback:** Static list of 6 default modules (PowerShell 7.x)

### GGUF Zone Size
- **Default:** 256MB per zone
- **Customizable:** Modify line in `BuildTensorIndex()`:
  ```cpp
  tensor.zone_id = QString("zone_%1").arg(tensor.absolute_offset / (256 * 1024 * 1024));
  //                                                                ^^^^^^^^^^^^^^^^^^^
  //                                                                Change this value
  ```

### Logging Levels
- **Debug:** Detailed execution flow (token content, zone boundaries)
- **Info:** High-level operations (index build, zone load)
- **Warning:** Fallback scenarios (no PowerShell, out-of-bounds)
- **Error:** Critical failures (invalid GGUF, mmap failure)

---

## Troubleshooting

### "Failed to memory map zone"
**Possible causes:**
1. File not open
2. Insufficient memory
3. Invalid offset/size (exceeds file size)

**Solution:**
```cpp
qWarning() << "Reason:" << file_.errorString();  // Check this output
```

### "No active PowerShell session - using default module list"
**Expected behavior:** Fallback to static list when terminal not available

**To enable dynamic discovery:**
1. Start PowerShell terminal: `ide.startPowerShell()`
2. Wait for terminal to be ready
3. Call `ide.refreshModuleList()`

### "Tensor not found in index"
**Cause:** `BuildTensorIndex()` not called or failed

**Solution:**
```cpp
if (!loader.BuildTensorIndex()) {
    qWarning() << "Index build failed - check file format";
}
```

---

## Testing Commands

### Verify Win32IDE Changes
```powershell
cd "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader"
Get-Content "src\win32app\Win32IDE.cpp" | Select-String "WM_USER \+ 100" -Context 5
Get-Content "src\win32app\Win32IDE.cpp" | Select-String "refreshModuleList" -Context 5
```

### Verify GGUF Loader Changes
```powershell
cd "e:\src\qtapp\gguf"
Get-Content "StreamingGGUFLoader.cpp" | Select-String "BuildTensorIndex|LoadZone|GetTensorData" -Context 2
```

### Check Compilation
```powershell
cd "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build"
cmake --build . --config Release --target RawrXD-AgenticIDE 2>&1 | Select-String "error|warning"
```

---

**Quick Reference Version:** 1.0  
**Last Updated:** December 11, 2025  
**Compatibility:** Win32IDE v0.1, StreamingGGUFLoader (Qt-based)
