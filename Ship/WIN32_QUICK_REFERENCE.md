# Win32 UI Components - Quick Reference

## Include All 3 Components
```cpp
#include "Win32_MainWindow.hpp"         // Includes HexConsole + HotpatchManager
#include "Win32_HexConsole.hpp"         // Optional: standalone use
#include "Win32_HotpatchManager.hpp"    // Optional: standalone use
```

---

## MainWindow (Complete Application)

### Create Window
```cpp
RawrXD::Win32::MainWindow window(GetModuleHandle(nullptr));
HWND hwnd = window.Create(L"App Title", 1024, 768);
```

### Access Components
```cpp
auto console = window.GetHexConsole();
auto manager = window.GetHotpatchManager();
```

### Message Loop
```cpp
MSG msg = {};
while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
}
```

---

## HexConsole (Text Display Widget)

### Create
```cpp
RawrXD::Win32::HexConsole console(hwndParent);
HWND hwndConsole = console.Create(10, 10, 500, 400);
```

### Output Text
```cpp
console.AppendLog(L"Simple message");
console.AppendLogFormatted(L"ERROR", L"Error occurred", RGB(255, 0, 0));
```

### Display Hex
```cpp
RawrXD::Vector<uint8_t> data = { 0x48, 0x89, 0x5C, 0x24 };
console.DisplayHex(data);
// Output:
// 00000000: 48 89 5C 24                               | H.\$
```

### Manage Content
```cpp
console.Clear();
size_t lines = console.GetLineCount();
console.SetMaxLines(5000);
console.SaveToFile(L"D:\\output.txt");
```

---

## HotpatchManager (Memory Patching)

### Create Manager
```cpp
RawrXD::Win32::HotpatchManager manager;
```

### Register Callbacks
```cpp
manager.SetOnLogMessage([](const RawrXD::String& msg) {
    std::wcout << msg << L"\n";
});

manager.SetOnPatchProgress([](int current, int total) {
    std::wcout << L"Progress: " << current << L"/" << total << L"\n";
});

manager.SetOnPatchComplete([](const RawrXD::Win32::PatchResult& result) {
    if (result.success) {
        std::wcout << L"✓ Patched " << result.bytesPatched << L" bytes\n";
    } else {
        std::wcout << L"✗ Error: " << result.message << L"\n";
    }
});
```

### Create Patch
```cpp
RawrXD::Win32::PatchTarget target;
target.targetName = L"MyFunction";
target.address = reinterpret_cast<DWORD_PTR>(MyFunction);
target.originalBytes = { 0x55, 0x48, 0x89, 0xE5 };  // Original code
target.patchedBytes = { 0x48, 0xC7, 0xC0, 0x00 };   // Patched code
manager.RegisterPatchTarget(target);
```

### Execute Patch
```cpp
RawrXD::Win32::PatchResult result = manager.PerformHotpatch();
if (result.success) {
    std::wcout << L"Success: " << result.message << L"\n";
} else {
    std::wcout << L"Failed: " << result.message << L"\n";
}
```

### Rollback
```cpp
manager.RollbackPatches();
```

### Query Status
```cpp
bool isPatching = manager.IsPatching();
uint32_t bytesPatched = manager.GetTotalBytesPatched();
```

---

## InferenceEngine (Async Inference)

### Create Engine
```cpp
RawrXD::Win32::InferenceEngine engine;
engine.Start();
```

### Load Model
```cpp
if (!engine.LoadModel(L"model.bin", L"tokenizer.bin")) {
    // Handle error
}
```

### Sync Inference (Blocking)
```cpp
RawrXD::String result = engine.Infer(L"Hello world", 256, 0.8f);
```

### Async Inference (Non-blocking)
```cpp
RawrXD::String requestId = engine.QueueInferenceRequest(L"Prompt", 256, 0.8f);
// Later, check result via callback
```

### Register Callbacks
```cpp
engine.SetOnModelLoaded([]() {
    std::wcout << L"Model loaded\n";
});

engine.SetOnInferenceComplete([](const RawrXD::Win32::InferenceResult& result) {
    std::wcout << L"Result: " << result.result << L"\n";
    std::wcout << L"Latency: " << result.latencyMs << L"ms\n";
});

engine.SetOnError([](RawrXD::Win32::InferenceErrorCode code, const RawrXD::String& msg) {
    std::wcout << L"Error: " << msg << L"\n";
});
```

### Query Status
```cpp
auto health = engine.GetHealthStatus();
std::wcout << L"Avg Latency: " << health.avgLatencyMs << L"ms\n";
std::wcout << L"Tokens/sec: " << engine.GetTokensPerSecond() << L"\n";
```

### Shutdown
```cpp
engine.Shutdown();
```

---

## Integration Example: Complete App

```cpp
#include "Win32_MainWindow.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // Create main window (initializes all components)
    RawrXD::Win32::MainWindow window(hInstance);
    HWND hwnd = window.Create(L"RawrXD Hotpatcher");
    
    if (!hwnd) return -1;
    
    // Get components
    auto console = window.GetHexConsole();
    auto manager = window.GetHotpatchManager();
    
    // Log startup
    console->AppendLog(L"Application initialized");
    console->AppendLog(L"Ready for hotpatching");
    
    // Standard Win32 message loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    return (int)msg.wParam;
}
```

---

## Data Structures

### PatchResult
```cpp
struct PatchResult {
    bool success;                                    // Success indicator
    String message;                                  // Result message
    std::chrono::system_clock::time_point timestamp; // When completed
    uint32_t bytesPatched;                          // Bytes modified
    
    static PatchResult Ok(const String& msg = L"");
    static PatchResult Error(const String& msg);
};
```

### PatchTarget
```cpp
struct PatchTarget {
    DWORD_PTR address;              // Memory address to patch
    Vector<uint8_t> originalBytes;  // Original code
    Vector<uint8_t> patchedBytes;   // Replacement code
    String targetName;              // Name/description
    bool applied;                   // Patch applied flag
};
```

### HealthStatus
```cpp
struct HealthStatus {
    bool modelLoaded;               // Model loaded
    bool gpuAvailable;              // GPU available
    bool inferenceReady;            // Ready for inference
    double avgLatencyMs;            // Average latency
    double p95LatencyMs;            // 95th percentile latency
    double p99LatencyMs;            // 99th percentile latency
    int pendingRequests;            // Queued requests
    int totalRequestsProcessed;     // Total processed
    String lastError;               // Last error message
};
```

### InferenceResult
```cpp
struct InferenceResult {
    String requestId;               // Request ID
    String result;                  // Generated text
    InferenceErrorCode errorCode;   // Error code if failed
    String errorMessage;            // Error message
    int tokensGenerated;            // Tokens produced
    double latencyMs;               // Execution time
    std::chrono::system_clock::time_point completionTime;
};
```

---

## Callback Types

### HotpatchManager
```cpp
using OnLogMessageCallback = std::function<void(const String&)>;
using OnPatchProgressCallback = std::function<void(int, int)>;
using OnPatchCompleteCallback = std::function<void(const PatchResult&)>;
using OnPatchErrorCallback = std::function<void(const String&)>;
```

### InferenceEngine
```cpp
using OnModelLoadedCallback = std::function<void()>;
using OnInferenceCompleteCallback = std::function<void(const InferenceResult&)>;
using OnErrorCallback = std::function<void(InferenceErrorCode, const String&)>;
using OnHealthStatusChangedCallback = std::function<void(const HealthStatus&)>;
```

---

## Error Codes

### InferenceErrorCode
```cpp
enum class InferenceErrorCode {
    SUCCESS = 0,
    MODEL_LOAD_FAILED = 4001,
    INVALID_MODEL_PATH = 4002,
    TOKENIZER_NOT_INITIALIZED = 4101,
    EMPTY_REQUEST = 4201,
    INSUFFICIENT_MEMORY = 4301,
    INFERENCE_FAILURE = 4402,
    // ... and more
};
```

---

## Compilation

### Include Paths
```
D:\RawrXD\Ship\
```

### Required Libraries
```
kernel32.lib    # Win32 API
comctl32.lib    # Common controls
ole32.lib       # OLE (for UUID generation if needed)
```

### Build
```powershell
cl.exe /std:c++20 /EHsc /W4 /DNOMINMAX myapp.cpp kernel32.lib comctl32.lib
```

---

## Performance Tips

1. **Batch Patches**: Register all patches before calling PerformHotpatch()
2. **Limit Console**: Set MaxLines on HexConsole to manage memory
3. **Async Inference**: Use QueueInferenceRequest for non-blocking UI
4. **Error Callbacks**: Handle OnError to prevent cascading failures

---

## Common Tasks

### Display Binary File Content
```cpp
std::vector<uint8_t> ReadBinaryFile(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

// Usage
auto data = ReadBinaryFile(L"D:\\data.bin");
console->DisplayHex(data);
```

### Export Console to File
```cpp
console->SaveToFile(L"D:\\log.txt");
```

### Periodic Status Updates
```cpp
std::thread([&manager, &console]() {
    while (true) {
        auto status = manager.GetHealthStatus();
        console->AppendLog(L"Pending: " + std::to_wstring(status.pendingRequests));
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}).detach();
```

---

**Quick Reference Version**: 1.0  
**Last Updated**: January 29, 2026  
**Status**: Production Ready - Zero Qt Dependencies
