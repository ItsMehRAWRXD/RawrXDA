# RawrXD Reverse Engineering - Quick Reference Guide

## 🎯 Core Modules

### Binary Analysis
- **BinaryLoader** - Load and parse PE/ELF/Mach-O binaries
- **SymbolAnalyzer** - Extract and manage symbols
- **ResourceExtractor** - Extract strings and resources

### Code Analysis
- **Disassembler** - Disassemble binary code (Capstone-based)
- **Decompiler** - Generate pseudo-C/C++ from assembly
- **StructReconstructor** - Infer C/C++ structures

### User Interface
- **ReverseEngineeringWidget** - Main UI with 8 tabs
- **ScriptingConsole** - Interactive script execution
- **HexEditor** - Binary file viewer/editor

### Utilities
- **ReportGenerator** - Export analysis in multiple formats
- **SymbolManager** - Symbol annotation and organization
- **ScriptingEngine** - Python/Lua/JavaScript support

---

## 📋 Reverse Engineering Workflow

### 1. Load Binary
```cpp
ReverseEngineeringWidget widget;
widget.openBinary("app.exe");
// Automatically:
// - Parses binary headers
// - Extracts symbols
// - Disassembles code sections
// - Reconstructs structures
// - Extracts strings
```

### 2. Explore Functions
```
Binary Info Tab     → View architecture, sections, exports/imports
Symbols Tab         → Search and filter all symbols
Disassembly Tab     → View assembly with cross-references
Decompilation Tab   → Read pseudo-C/C++ code
```

### 3. Analyze Data Structures
```
Structures Tab      → Browse reconstructed structs
                    → View field offsets and types
                    → Export to C header files
```

### 4. Automate Analysis
```
Scripting Console   → Write Python/Lua/JavaScript analysis scripts
                    → Access context: binaryPath, functions, symbols
                    → Execute custom analysis logic
```

### 5. Generate Reports
```
Reports Tab         → Generate HTML/Markdown/JSON/CSV/Text reports
                    → Export symbols and function signatures
                    → Save for documentation
```

---

## 🔍 Feature Matrix

| Feature | Tab | Status | Notes |
|---------|-----|--------|-------|
| Binary metadata | Binary Info | ✅ | Format, arch, sections |
| Symbol search | Symbols | ✅ | Filter by name/address |
| Disassembly | Disassembly | ✅ | With xrefs and highlighting |
| Decompilation | Decompilation | ✅ | Complexity metrics |
| Struct inference | Structures | ✅ | With confidence scoring |
| Hex viewing | Hex Editor | ✅ | Dual pane hex/ASCII |
| String extraction | Strings | ✅ | ASCII + Unicode |
| Custom scripts | Console | ✅ | Python/Lua/JavaScript |
| Report generation | Reports | ✅ | Multiple formats |
| Symbol annotation | Symbols | ✅ | Rename, comment |

---

## 🛠️ Configuration

### Supported Architectures
- x86 (32-bit Intel)
- x86-64 (64-bit Intel/AMD)
- ARM (32-bit ARM)
- ARM64 (64-bit ARM)

### Supported Binary Formats
- PE (Windows .exe, .dll)
- ELF (Linux, BSD)
- Mach-O (macOS)

### Scripting Languages
- **Python** 3.6+ (requires `python` in PATH)
- **Lua** 5.1+ (requires `lua` in PATH)
- **JavaScript** (requires `node` in PATH)

---

## 📚 API Quick Reference

### BinaryLoader
```cpp
BinaryLoader loader("app.exe");
loader.load();
auto metadata = loader.metadata();  // Format, arch, sections
auto exports = loader.exports();    // All exported functions
auto imports = loader.imports();    // All imported functions
```

### Disassembler
```cpp
Disassembler disasm(loader);
disasm.analyzeCodeSection(".text");
auto functions = disasm.functions();
auto func = disasm.functionNamed("main");
auto xrefs = disasm.crossReferences(0x400000);
```

### Decompiler
```cpp
Decompiler decomp(disasm);
auto decomped = decomp.decompile(func);
qDebug() << decomped.pseudoCode;
qDebug() << "Complexity:" << decomped.complexity;
qDebug() << "Variables:" << decomped.variables.size();
```

### ReportGenerator
```cpp
ReportGenerator gen;
QString html = gen.generateReport(loader, analyzer, disasm, decomp, 
                                  ReportFormat::HTML);
ReportGenerator::saveReport("report.html", html);
```

### ScriptingEngine
```cpp
ScriptingEngine engine;
QMap<QString, QVariant> ctx;
ctx["binaryPath"] = "app.exe";
ctx["functions"] = 256;
QString result = engine.executePython(script, ctx);
```

---

## 💡 Scripting Examples

### Python: Analyze All Functions
```python
print(f"Analyzing: {binaryPath}")
for func in functions:
    print(f"Function: {func}")
    # Custom analysis here
```

### Lua: Filter Large Functions
```lua
for i, func in ipairs(functions) do
    if func.size > 1000 then
        print("Large function: " .. func.name)
    end
end
```

### JavaScript: Export Function List
```javascript
const output = functions.map(f => `${f.name}:0x${f.address}`);
console.log(output.join('\n'));
```

---

## 🎨 UI Navigation

```
Main Window
├─ Binary Info Tab
│  ├─ Format, Architecture
│  ├─ Section Count
│  ├─ Export/Import Count
│  └─ Status Bar
│
├─ Symbols Tab
│  ├─ Search Bar
│  ├─ Symbol Tree (Address, Name, Type, Module)
│  ├─ Details Pane
│  ├─ Rename Button
│  └─ Comment Button
│
├─ Disassembly Tab
│  ├─ Function Selector (Combo Box)
│  ├─ Code Display (Address | Bytes | Mnemonic | Operands)
│  └─ Cross References Tree
│
├─ Decompilation Tab
│  ├─ Complexity Label
│  └─ Pseudo-Code Display
│
├─ Structures Tab
│  ├─ Struct Tree
│  ├─ Code View
│  └─ Edit Button
│
├─ Hex Editor Tab
│  ├─ Address Input
│  ├─ Go To Button
│  └─ Hex Dump Display
│
├─ Strings Tab
│  ├─ Search Filter
│  └─ String List (Address, Value, Type)
│
├─ Reports Tab
│  ├─ Format Selector
│  ├─ Report Preview
│  ├─ Generate Button
│  └─ Export Button
│
└─ Scripting Console (Optional)
   ├─ Language Selector
   ├─ Script Editor
   ├─ Execute/Load/Save Buttons
   └─ Output Pane
```

---

## ⚠️ Common Tasks

### View Function Assembly
1. Go to Disassembly Tab
2. Select function from dropdown
3. Assembly appears with formatting

### Inspect Data Structure
1. Go to Structures Tab
2. Click struct in tree
3. Fields and offsets displayed
4. C code available for export

### Export Analysis Results
1. Go to Reports Tab
2. Select format (HTML/JSON/CSV/etc)
3. Click "Generate Report"
4. Click "Export Results"
5. Choose file location

### Run Custom Analysis
1. Open Scripting Console
2. Select language (Python/Lua/JS)
3. Write script (context available)
4. Click "Execute"
5. View output

---

## 📊 Data Structures Reference

### FunctionInfo
```cpp
struct FunctionInfo {
    QString name;
    uint64_t address;
    uint64_t size;
    QVector<Instruction> instructions;
    bool isRecognized;
};
```

### Instruction
```cpp
struct Instruction {
    uint64_t address;
    QByteArray bytes;
    QString mnemonic, operands, disassembly;
    bool isBranch, isCall, isReturn;
    uint64_t target;
};
```

### DecompiledFunction
```cpp
struct DecompiledFunction {
    QString name, signature, pseudoCode;
    uint64_t address;
    QVector<Variable> variables, parameters;
    bool hasLoops;
    int complexity;
};
```

### ReconstructedStruct
```cpp
struct ReconstructedStruct {
    QString name;
    uint32_t size;
    QVector<StructField> fields;
    QString sourceCode;
    int confidence;  // 0-100
};
```

---

## 🔗 Integration Points

### Connect to Main IDE
```cpp
// In main application
auto reWidget = new ReverseEngineeringWidget();

connect(reWidget, &ReverseEngineeringWidget::analysisStarted,
        this, &MainWindow::onAnalysisStarted);

connect(reWidget, &ReverseEngineeringWidget::analysisCompleted,
        this, &MainWindow::onAnalysisCompleted);

// Add as dockable widget
addDockWidget(Qt::RightDockWidgetArea, reWidget);
```

---

## 📦 Build Integration

### CMakeLists.txt
```cmake
add_library(RawrXDReverseEngineering
    src/qtapp/reverse_engineering/binary_loader.cpp
    src/qtapp/reverse_engineering/disassembler.cpp
    src/qtapp/reverse_engineering/decompiler.cpp
    # ... other sources
)

target_link_libraries(RawrXDReverseEngineering
    Qt5::Widgets
    capstone
    LIEF::LIEF
)
```

---

## ✅ Checklist for First Use

- [ ] Binary file loaded successfully
- [ ] Export/Import tables visible
- [ ] Functions detected and listed
- [ ] Disassembly displays correctly
- [ ] Decompilation generates pseudo-code
- [ ] Strings extracted
- [ ] Report generation works
- [ ] Scripting console functional
- [ ] Hex editor displays binary

---

## 🆘 Troubleshooting

| Issue | Solution |
|-------|----------|
| Binary won't load | Check file format (PE/ELF/Mach-O) and permissions |
| No functions found | Ensure .text section exists, try analyzeCodeSection |
| Scripts fail | Verify Python/Lua/Node is installed and in PATH |
| Decompilation empty | Check if disassembly is populated first |
| Report won't save | Verify write permissions on output directory |

---

## 📞 Support

For issues, refer to:
- Main implementation: `REVERSE_ENGINEERING_IMPLEMENTATION_COMPLETE.md`
- Code files in: `src/qtapp/reverse_engineering/`
- Build config: `CMakeLists.txt`

---

**Last Updated:** January 22, 2026  
**Status:** ✅ Production Ready
