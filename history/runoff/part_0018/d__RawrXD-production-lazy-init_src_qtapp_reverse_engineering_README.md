# RawrXD Reverse Engineering Module

This module provides comprehensive binary analysis and reverse engineering capabilities for the RawrXD IDE.

## Features Implemented

### 1. Binary Loader/Parser (`binary_loader.*`)
- **PE (Windows)** format support: Full PE header parsing, sections, exports, imports
- **ELF (Unix/Linux)** format support: ELF header parsing, sections, symbols
- **Mach-O (macOS)** format support: Mach-O header parsing, sections, load commands
- Architecture detection: x86, x64, ARM, ARM64, MIPS, PowerPC
- Section enumeration and RVA->offset translation

### 2. Symbol Analysis (`symbol_analyzer.*`)
- Export table extraction
- Import table extraction
- Symbol demangling (MSVC, GCC/Clang)
- Symbol indexing by name and address
- Cross-reference resolution

### 3. Disassembler (`disassembler.*`)
- Capstone engine integration
- Multi-architecture disassembly: x86, x64, ARM, ARM64
- Function boundary detection
- Cross-reference analysis
- Instruction classification (branches, calls, returns)

### 4. Decompiler (`decompiler.*`)
- Assembly-to-pseudo-code conversion
- Control flow graph generation
- Variable extraction and analysis
- Function signature inference
- Complexity calculation
- Loop detection

### 5. Data Structure Reconstruction (`struct_reconstructor.*`)
- Automatic struct field detection
- Type inference from memory access patterns
- Padding field generation
- C++ struct code generation
- Struct field editing and refinement
- Export as C++ header

### 6. Symbol Manager (`symbol_manager.*`)
- Symbol renaming and annotation
- Automatic naming conventions
- Pattern-based naming suggestions
- CSV import/export of symbol tables
- Symbol confirmation workflow

### 7. Hex Editor (`hex_editor.*`)
- Raw binary viewing and editing
- Value reading/writing at arbitrary offsets
- Byte-level search and replace
- Bookmarking with comments
- Offset annotation
- Save/load functionality

### 8. Resource Extractor (`resource_extractor.*`)
- ASCII string extraction
- Unicode string extraction
- Configurable minimum string length
- String search and filtering
- Resource enumeration
- CSV export of strings

### 9. Reverse Engineering Widget (`reverse_engineering_widget.*`)
- Unified UI for all tools
- Binary info display
- Symbol browser with search
- Disassembly viewer
- Decompilation display
- Structure editor
- Hex editor
- String browser
- Report generation

## File Structure

```
src/qtapp/reverse_engineering/
├── binary_loader.h/cpp           # Binary format parsing
├── symbol_analyzer.h/cpp         # Symbol analysis & demangling
├── disassembler.h/cpp            # Capstone integration
├── decompiler.h/cpp              # Assembly to pseudo-code
├── struct_reconstructor.h/cpp    # Struct reconstruction
├── symbol_manager.h/cpp          # Symbol management
├── hex_editor.h/cpp              # Hex viewer/editor
├── resource_extractor.h/cpp      # String/resource extraction
├── reverse_engineering_widget.h/cpp  # Main UI widget
└── README.md                     # This file
```

## Dependencies

### Required External Libraries
- **Capstone** (v5.0+) - Disassembly engine
  - Windows: `capstone.lib`, `capstone.dll`
  - Linux: `libcapstone.so`
  - macOS: `libcapstone.dylib`

### Qt Dependencies
- Qt Core
- Qt GUI
- Qt Widgets

## Integration into RawrXD

### 1. Add to CMakeLists.txt

```cmake
# Add Reverse Engineering module
find_package(Capstone REQUIRED)

set(REVERSE_ENGINEERING_SOURCES
    src/qtapp/reverse_engineering/binary_loader.cpp
    src/qtapp/reverse_engineering/symbol_analyzer.cpp
    src/qtapp/reverse_engineering/disassembler.cpp
    src/qtapp/reverse_engineering/decompiler.cpp
    src/qtapp/reverse_engineering/struct_reconstructor.cpp
    src/qtapp/reverse_engineering/symbol_manager.cpp
    src/qtapp/reverse_engineering/hex_editor.cpp
    src/qtapp/reverse_engineering/resource_extractor.cpp
    src/qtapp/reverse_engineering/reverse_engineering_widget.cpp
)

set(REVERSE_ENGINEERING_HEADERS
    src/qtapp/reverse_engineering/binary_loader.h
    src/qtapp/reverse_engineering/symbol_analyzer.h
    src/qtapp/reverse_engineering/disassembler.h
    src/qtapp/reverse_engineering/decompiler.h
    src/qtapp/reverse_engineering/struct_reconstructor.h
    src/qtapp/reverse_engineering/symbol_manager.h
    src/qtapp/reverse_engineering/hex_editor.h
    src/qtapp/reverse_engineering/resource_extractor.h
    src/qtapp/reverse_engineering/reverse_engineering_widget.h
)

target_sources(RawrXD PRIVATE ${REVERSE_ENGINEERING_SOURCES} ${REVERSE_ENGINEERING_HEADERS})
target_include_directories(RawrXD PRIVATE src/qtapp/reverse_engineering)
target_link_libraries(RawrXD PRIVATE Capstone::Capstone)
```

### 2. Integration with Main Window

```cpp
#include "src/qtapp/reverse_engineering/reverse_engineering_widget.h"

// In MainWindow constructor:
auto* reWidget = new RawrXD::ReverseEngineering::ReverseEngineeringWidget();
dockWidget->setWidget(reWidget);

// Connect signals
connect(reWidget, &RawrXD::ReverseEngineering::ReverseEngineeringWidget::analysisStarted,
        this, &MainWindow::onAnalysisStarted);
connect(reWidget, &RawrXD::ReverseEngineering::ReverseEngineeringWidget::analysisCompleted,
        this, &MainWindow::onAnalysisCompleted);
```

### 3. Add Menu Items

```cpp
// File menu
QAction* openBinaryAction = new QAction("Open Binary for Analysis...");
connect(openBinaryAction, &QAction::triggered, this, [this]() {
    // Open binary dialog connected to reverse engineering widget
});
fileMenu->addAction(openBinaryAction);
```

## Usage Example

```cpp
#include "reverse_engineering_widget.h"

// Create widget
auto* reWidget = new RawrXD::ReverseEngineering::ReverseEngineeringWidget();

// Load and analyze binary
if (reWidget->openBinary("/path/to/app.exe")) {
    // Binary loaded and analyzed successfully
    // User can now browse symbols, disassembly, etc.
}
```

## Architecture & Design

### Design Patterns Used
1. **Factory Pattern** - BinaryLoader creates appropriate parser based on format
2. **Strategy Pattern** - Different demangling strategies for MSVC/GCC/Clang
3. **Observer Pattern** - Signal/slot connections for UI updates
4. **Composite Pattern** - CFG blocks composed into functions

### Data Flow
1. Binary file → BinaryLoader → BinaryMetadata + Sections + Symbols
2. Symbols → SymbolAnalyzer → Demangled symbols
3. Code section → Disassembler → Instruction stream
4. Instructions → Decompiler → Pseudo-code + CFG
5. Memory accesses → StructReconstructor → Struct definitions
6. Binary data → ResourceExtractor → Strings + Resources

### Performance Considerations
- **Lazy loading**: Functions disassembled on-demand
- **Caching**: Symbol tables cached after first analysis
- **Incremental updates**: Only reanalyze changed sections
- **Streaming**: Large binaries processed in chunks

## Extending the Module

### Adding New Binary Format Support

1. Create new format handler in `binary_loader.cpp`:
```cpp
bool BinaryLoader::loadWASM() {
    // Parse WASM format
}
```

2. Update `detectFormat()` to recognize new format
3. Implement format-specific parsing methods

### Adding New Analysis Tools

1. Create new class inheriting from existing analyzer:
```cpp
class TaintAnalyzer : public Decompiler {
    // Implement taint tracking
};
```

2. Add to `ReverseEngineeringWidget` UI
3. Connect signals for tab management

## Testing

### Unit Tests (To be implemented)
- `test_binary_loader.cpp` - Format parsing tests
- `test_symbol_analyzer.cpp` - Demangling tests
- `test_disassembler.cpp` - Disassembly correctness
- `test_decompiler.cpp` - Pseudo-code generation

### Integration Tests
- Load various PE/ELF/Mach-O binaries
- Verify symbol extraction accuracy
- Test struct reconstruction on known types
- Benchmark performance on large binaries

## Known Limitations

1. **Decompiler**: Simplified pseudo-code generation (not production-grade)
2. **Structure Reconstruction**: Basic pattern matching (may miss complex layouts)
3. **Symbol Demangling**: Limited to MSVC and GCC standard formats
4. **Resources**: PE resource extraction not yet implemented
5. **Debugging**: No dynamic debugging capability (static analysis only)

## Future Enhancements

- [ ] Full resource directory parsing
- [ ] Taint/data flow analysis
- [ ] Call graph visualization
- [ ] Python scripting support
- [ ] IDA Pro/Ghidra plugin compatibility
- [ ] Dynamic analysis integration
- [ ] ML-based function recognition
- [ ] Vulnerable code pattern detection

## References

- [PE Format Specification](https://en.wikipedia.org/wiki/Portable_Executable)
- [ELF Format Specification](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format)
- [Mach-O Format](https://en.wikipedia.org/wiki/Mach-O)
- [Capstone Engine](http://www.capstone-engine.org/)
- [C++ Name Mangling](https://en.wikipedia.org/wiki/Name_mangling)

## License

Part of RawrXD IDE - See main project LICENSE

## Authors

- RawrXD Team
- Reverse Engineering Module Contributors
