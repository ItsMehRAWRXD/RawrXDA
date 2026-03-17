# Universal MASM Wrapper - Quick Reference

## Files Created

| File | Type | Size | Purpose |
|------|------|------|---------|
| `src/masm/universal_format_loader/universal_wrapper.asm` | MASM x64 | 450+ LOC | Pure assembly implementation |
| `src/masm/universal_format_loader/universal_wrapper.inc` | MASM Include | 200+ LOC | Structure definitions & extern C |
| `src/qtapp/universal_wrapper_masm.hpp` | C++ Header | 250+ LOC | Unified wrapper class interface |
| `src/qtapp/universal_wrapper_masm.cpp` | C++ Source | 450+ LOC | Implementation with Qt integration |

## Class Hierarchy

```
UniversalWrapperMASM
├── Format (enum)
│   ├── UNKNOWN
│   ├── GGUF_LOCAL
│   ├── SAFETENSORS
│   ├── PYTORCH
│   ├── TENSORFLOW
│   ├── ONNX
│   └── NUMPY
├── Compression (enum)
│   ├── NONE
│   ├── GZIP
│   ├── ZSTD
│   └── LZ4
├── WrapperMode (enum)
│   ├── PURE_MASM (0)
│   ├── CPP_QT (1)
│   └── AUTO_SELECT (2)
└── ErrorCode (enum)
    ├── OK
    ├── NOT_INITIALIZED
    ├── FILE_NOT_FOUND
    └── ...8 more error types
```

## Core API Methods

### Creation & Lifecycle
```cpp
UniversalWrapperMASM wrapper;                              // Create
UniversalWrapperMASM wrapper(Mode::CPP_QT);                // With mode
~UniversalWrapperMASM();                                   // Destroy (auto)
```

### Format Detection
```cpp
wrapper.detectFormat("path");                              // Extension + Magic
wrapper.detectFormatExtension("path");                     // Extension only
wrapper.detectFormatMagic("path");                         // Magic bytes only
wrapper.detectCompression("path");                         // Detect compression type
wrapper.validateModelPath("path");                         // Validate
```

### Model Loading
```cpp
wrapper.loadUniversalFormat("path");                       // Auto-detect & load
wrapper.loadSafeTensors("path");                           // → loadUniversalFormat
wrapper.loadPyTorch("path");                               // → loadUniversalFormat
wrapper.loadTensorFlow("path");                            // → loadUniversalFormat
wrapper.loadONNX("path");                                  // → loadUniversalFormat
wrapper.loadNumPy("path");                                 // → loadUniversalFormat
```

### Conversion
```cpp
wrapper.convertToGGUF("output_path");                      // From loaded model
wrapper.convertToGGUFWithInput("in", "out");               // Direct conversion
```

### File I/O
```cpp
wrapper.readFileChunked("path", buffer);                   // Read in 64KB chunks
wrapper.writeBufferToFile("path", buffer);                 // Write buffer
wrapper.getTempDirectory();                                // Get temp dir
wrapper.generateTempGGUFPath("name", path);                // Generate temp path
wrapper.cleanupTempFiles();                                // Clean up
```

### Cache Management
```cpp
wrapper.getCacheHits();                                    // Cache hit count
wrapper.getCacheMisses();                                  // Cache miss count
wrapper.getCacheSize();                                    // Current entries
wrapper.clearCache();                                      // Clear cache
```

### Mode Control
```cpp
wrapper.setMode(Mode::CPP_QT);                             // Change mode
wrapper.getMode();                                         // Get current mode
UniversalWrapperMASM::SetGlobalMode(Mode::PURE_MASM);      // Global mode
UniversalWrapperMASM::GetGlobalMode();                     // Get global mode
```

### Statistics & Status
```cpp
auto stats = wrapper.getStatistics();                      // Get stats
wrapper.resetStatistics();                                 // Reset stats
wrapper.getLastError();                                    // Error message
wrapper.getLastErrorCode();                                // Error code
wrapper.getLastDurationMs();                               // Operation duration
wrapper.isInitialized();                                   // Check init
wrapper.getTempOutputPath();                               // Last output path
wrapper.getDetectedFormat();                               // Last detected format
```

## Statistics Structure

```cpp
struct Statistics {
    uint64_t total_detections;                             // Total detections
    uint64_t total_conversions;                            // Total conversions
    uint64_t total_errors;                                 // Total errors
    uint64_t cache_hits;                                   // Cache hits
    uint64_t cache_misses;                                 // Cache misses
    uint32_t cache_entries;                                // Current cache size
    uint32_t current_mode;                                 // Current mode
};
```

## Error Codes

```
0 = OK
1 = INVALID_PTR
2 = NOT_INITIALIZED
3 = ALLOC_FAILED
4 = MUTEX_FAILED
5 = FILE_NOT_FOUND
6 = FORMAT_UNKNOWN
7 = LOAD_FAILED
8 = CACHE_FULL
9 = MODE_INVALID
```

## Memory Layout

### Per-Instance (512 bytes + buffers)
```
UNIVERSAL_WRAPPER struct:     512 bytes
Detection cache (32 entries):  1 KB
Error buffer:                512 bytes
Temp path buffer:            1 KB
─────────────────────────────────
Total per instance:         3,024 bytes
```

### Global State
```
Global error buffer:     1 KB
Global mode:            4 bytes
Global wrapper ptr:     8 bytes
─────────────────────────────
Total global:        1,036 bytes (one-time)
```

## Supported Formats

| Format | Extension(s) | Handling |
|--------|-----------|----------|
| GGUF | `.gguf` | Direct (no conversion) |
| SafeTensors | `.safetensors` | Auto-detect → Convert to GGUF |
| PyTorch | `.pt`, `.pth` | Auto-detect → Convert to GGUF |
| TensorFlow | `.pb` | Auto-detect → Convert to GGUF |
| ONNX | `.onnx` | Auto-detect → Convert to GGUF |
| NumPy | `.npy`, `.npz` | Auto-detect → Convert to GGUF |
| Gzip | `.gz` | Auto-decompress |
| Zstandard | `.zst` | Auto-decompress |
| LZ4 | `.lz4` | Auto-decompress |

## MASM Global Functions

```asm
wrapper_global_init(mode: uint32_t) → uint32_t
wrapper_create(mode: uint32_t) → UNIVERSAL_WRAPPER*
wrapper_detect_format_unified(wrapper, path) → uint32_t
wrapper_load_model_auto(wrapper, path, result) → uint32_t
wrapper_convert_to_gguf(wrapper, input, output) → uint32_t
wrapper_set_mode(wrapper, mode) → uint32_t
wrapper_get_statistics(wrapper, stats) → uint32_t
wrapper_destroy(wrapper) → void
detect_extension_unified(path) → uint32_t
detect_magic_bytes_unified(path, buffer) → uint32_t
```

## Include Path

```cpp
#include "universal_wrapper_masm.hpp"
```

## Namespace

```cpp
namespace UniversalWrapperFormat {
    enum class Format { ... };
    enum class Compression { ... };
    enum class WrapperMode { ... };
    enum class ErrorCode { ... };
}
```

## Common Patterns

### Load Any Format
```cpp
UniversalWrapperMASM w;
w.loadUniversalFormat("model.safetensors");      // SafeTensors
w.loadUniversalFormat("model.pt");               // PyTorch
w.loadUniversalFormat("model.onnx");             // ONNX
// All handled by single method!
```

### Batch Loading
```cpp
auto results = loadModelsUniversal(
    QStringList{"m1.pt", "m2.safetensors", "m3.onnx"},
    WrapperMode::PURE_MASM
);
for (const auto& r : results) {
    if (r.success) qDebug() << r.filePath << "loaded";
}
```

### Mode Switching
```cpp
// Global mode affects AUTO_SELECT
UniversalWrapperMASM::SetGlobalMode(WrapperMode::PURE_MASM);
UniversalWrapperMASM w1(WrapperMode::AUTO_SELECT);    // Uses PURE_MASM

// Or per-instance
UniversalWrapperMASM w2(WrapperMode::CPP_QT);         // Always CPP_QT
w2.setMode(WrapperMode::PURE_MASM);                   // Change at runtime
```

### Error Handling
```cpp
if (!w.loadUniversalFormat("model.pt")) {
    auto code = w.getLastErrorCode();
    auto msg = w.getLastError();
    qWarning() << code << ":" << msg;
}
```

### Performance Monitoring
```cpp
auto stats = w.getStatistics();
float hit_rate = (stats.cache_hits * 100.0) / 
                 (stats.cache_hits + stats.cache_misses);
qDebug() << "Cache hit rate:" << hit_rate << "%";
```

## Integration Points

### With MainWindow
```cpp
class MainWindow : public QMainWindow {
    std::unique_ptr<UniversalWrapperMASM> m_wrapper;
public:
    MainWindow() : m_wrapper(std::make_unique<UniversalWrapperMASM>()) {}
};
```

### With HotpatchManager
```cpp
UnifiedHotpatchManager manager;
UniversalWrapperMASM wrapper;
wrapper.loadUniversalFormat("model.safetensors");
// Now apply patches...
manager.applyMemoryPatch(patch);
```

### With Qt Signals
```cpp
connect(this, &MyClass::loadModelRequested, this, [this](QString path) {
    if (m_wrapper->loadUniversalFormat(path)) {
        emit modelLoaded();
    } else {
        emit modelLoadFailed(m_wrapper->getLastError());
    }
});
```

## Deprecation Path

Old classes can be deprecated with optional compatibility adapters:

```cpp
// In universal_format_loader_masm.hpp
[[deprecated("Use UniversalWrapperMASM instead")]]
class UniversalFormatLoaderMASM { /* thin adapter */ };
```

## Performance Tips

1. **Reuse instances**: Create once, keep alive for multiple operations
2. **Enable caching**: Detection results cached for 5 minutes
3. **Batch loading**: Use `loadModelsUniversal()` for multiple files
4. **Mode selection**:
   - PURE_MASM: Fastest, pure assembly
   - CPP_QT: Qt integration, more features
   - AUTO_SELECT: Adaptive based on global setting

## Cmake Integration

```cmake
# Add sources
target_sources(RawrXD-QtShell PRIVATE
    src/masm/universal_format_loader/universal_wrapper.asm
    src/qtapp/universal_wrapper_masm.cpp
)

# Enable MASM language
enable_language(ASM_MASM)

# Include paths
target_include_directories(RawrXD-QtShell PRIVATE
    src/qtapp
    src/masm/universal_format_loader
)
```

## Migration Checklist

- [ ] Replace includes: Remove 3 old headers, add `universal_wrapper_masm.hpp`
- [ ] Replace instances: Use single `UniversalWrapperMASM` instance
- [ ] Update calls: Use unified API methods
- [ ] Test detection: Verify format detection works
- [ ] Test loading: Verify model loading works
- [ ] Test conversion: Verify GGUF conversion works
- [ ] Verify mode toggle: Test PURE_MASM vs CPP_QT
- [ ] Check statistics: Verify stats collection works
- [ ] Performance test: Compare performance vs old wrapper
- [ ] Memory test: Verify memory usage is acceptable

## Thread Safety

✅ All methods are thread-safe via mutex
✅ Wrapper instances can be used from multiple threads
✅ Global mode can be changed safely
✅ Cache is protected by wrapper mutex

## Known Limitations

- Cache size is fixed at 32 entries
- Cache TTL is fixed at 5 minutes
- One global mode (affects all AUTO_SELECT instances)
- Format detection is extension + magic only (no deep parsing)

## See Also

- `UNIVERSAL_WRAPPER_INTEGRATION_GUIDE.md` - Full integration guide
- `src/masm/universal_format_loader/universal_wrapper.inc` - MASM structures
- `src/qtapp/universal_wrapper_masm.hpp` - C++ API
- `src/qtapp/universal_wrapper_masm.cpp` - Implementation
