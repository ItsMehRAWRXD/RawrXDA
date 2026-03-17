# Native GGUF Loader - TerraForm Enhanced

This directory contains an enhanced native GGUF loader that is fully compatible with the RXUC-TerraForm universal compiler.

## Files

- `native_gguf_loader.h` - C++ header with TerraForm-compatible interface
- `native_gguf_loader.cpp` - C++ implementation
- `native_gguf_loader.tf` - TerraForm universal syntax implementation
- `build_terraform.bat` - Build script for TerraForm compilation

## TerraForm Compilation

The loader has been enhanced to work seamlessly with your custom TerraForm compiler:

### Using TerraForm Directly

```batch
# Compile with your TerraForm compiler
terraform.exe native_gguf_loader.tf -o native_gguf_loader.exe
```

### C++ Compilation (Standard)

```batch
cl /c native_gguf_loader.cpp /std:c++20
link native_gguf_loader.obj kernel32.lib /out:native_gguf_loader.dll
```

## Features Enhanced for TerraForm

1. **Universal Syntax Support**: Compatible with C/Rust/Python/Go/JS/Zig/Nim/Carbon/Mojo syntax
2. **Zero Dependencies**: Pure native implementation, no external libraries
3. **Kernel-Mode Capable**: Can run in kernel environment
4. **Direct Binary Emission**: TerraForm version emits raw x64 without intermediate ASM
5. **C Interface**: Provides C-compatible API for easy integration

## TerraForm Syntax Features Used

- **Enums**: DataType and MetadataType enums
- **Structs**: GGUFHeader, GGUFTensorInfo, GGUFMetadata
- **Classes**: NativeGGUFLoader with methods
- **Unions**: For metadata value storage
- **Memory Management**: alloc/dealloc for dynamic arrays
- **File I/O**: Direct Windows API calls
- **Error Handling**: Return-based error handling
- **Type Safety**: Strong typing throughout

## API Usage

### C++ Usage
```cpp
#include "native_gguf_loader.h"

NativeGGUFLoader loader;
if (loader.Open("model.gguf")) {
    auto tensors = loader.GetTensors();
    // Load tensor data...
}
```

### TerraForm Usage
```rust
let loader = native_gguf_loader_create();
if native_gguf_loader_open(loader, "model.gguf") {
    let count = native_gguf_loader_get_tensor_count(loader);
    // Load tensors...
}
```

### Direct TerraForm Class Usage
```rust
let loader = NativeGGUFLoader::new();
if loader.open("model.gguf") {
    // Access tensors directly
    for tensor in loader.tensors {
        print("Tensor: {}\n", tensor.name);
    }
}
```

## Build Instructions

1. **With TerraForm**: `terraform.exe native_gguf_loader.tf`
2. **With MSVC**: `cl native_gguf_loader.cpp /link kernel32.lib`
3. **With CMake**: Add to your CMakeLists.txt

## Integration with RawrXD

This enhanced loader integrates seamlessly with the RawrXD AI toolkit's native inference engine, providing:

- Pure native GGUF model loading
- No external dependencies
- TerraForm compiler compatibility
- Kernel-mode operation capability
- Direct binary emission support