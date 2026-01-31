# RawrXD IDE - AI Context Rules (2026 Standard)
# This file acts as a permanent prompt for Cursor IDE
# It tells the AI exactly how to write code for this project

## 🎯 PROJECT OVERVIEW

**Project**: RawrXD IDE - Custom C++ IDE with AI Integration  
**Architecture**: Qt-based core with PowerShell bridge and SQLite registry  
**Goal**: Build a high-performance IDE that integrates Cursor features and Codex reverse engineering

---

## 💻 C++ STYLE GUIDELINES (2026 Standard)

### **Error Handling**
- **ALWAYS** use `std::expected<T, E>` for error handling
- **NEVER** use exceptions or raw error codes
- **NEVER** use `std::cout` for logging
- **ALWAYS** use the custom `Logger` class

**Correct**:
```cpp
std::expected<Result, Error> processData() {
    if (failed) {
        return std::unexpected(Error::InvalidData);
    }
    return Result{data};
}

auto result = processData();
if (!result) {
    Logger::error("Process failed: {}", result.error());
    return std::unexpected(result.error());
}
```

**Incorrect**:
```cpp
// ❌ Never do this
Result processData() {
    if (failed) {
        throw std::runtime_error("Failed");
    }
    return Result{data};
}

// ❌ Never do this
if (error) {
    std::cout << "Error: " << error << std::endl;  // WRONG!
}
```

### **Logging**
- **ALWAYS** use `Logger::info()`, `Logger::warn()`, `Logger::error()`
- **NEVER** use `std::cout`, `printf()`, or `qDebug()`
- Use structured logging with format strings

**Correct**:
```cpp
Logger::info("Processing file: {}", filename);
Logger::warn("Deprecated API used: {}", apiName);
Logger::error("Failed to load: {}", error.message());
```

**Incorrect**:
```cpp
// ❌ Never do this
std::cout << "Processing file: " << filename << std::endl;
qDebug() << "Error occurred";
```

### **Qt Conventions**
- Use `QString` for UI strings, `std::string` for core logic
- Use `QProcess` for PowerShell bridging (async only)
- Use `QSqlDatabase` for SQLite operations
- Use `QHttpServer` for REST API endpoints
- **NEVER** mix Qt and std containers in public APIs

**Correct**:
```cpp
// Core logic uses std
std::expected<std::string, Error> processCoreLogic(const std::string& input);

// UI layer uses QString
QString displayResult = QString::fromStdString(result.value());
```

**Incorrect**:
```cpp
// ❌ Never do this
std::vector<QString> mixedContainer;  // Wrong!
QString coreLogicResult;  // Should be std::string
```

### **Memory Management**
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- **NEVER** use raw `new`/`delete`
- For Qt objects, use parent-child hierarchy
- For non-Qt objects, use RAII

**Correct**:
```cpp
auto logger = std::make_unique<Logger>();
auto db = std::make_shared<Database>();

// Qt objects
auto* widget = new QWidget(parent);  // Will be deleted with parent
```

**Incorrect**:
```cpp
// ❌ Never do this
Logger* logger = new Logger();  // Memory leak!
delete logger;  // Manual deletion is error-prone
```

### **Naming Conventions**
- **Classes**: `PascalCase` (e.g., `ExtensionManager`)
- **Functions**: `camelCase` (e.g., `installExtension()`)
- **Variables**: `camelCase` (e.g., `extensionName`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRIES`)
- **Private members**: `m_` prefix (e.g., `m_extensionCount`)
- **Static members**: `s_` prefix (e.g., `s_instance`)

**Correct**:
```cpp
class ExtensionManager {
    static constexpr int MAX_RETRIES = 3;
    static ExtensionManager* s_instance;
    
    std::expected<bool, Error> installExtension(const std::string& extensionName);
    
private:
    int m_extensionCount = 0;
};
```

---

## 🏗️ ARCHITECTURAL PATTERNS

### **Layer Architecture**
```
┌─────────────────────────────────────┐
│  UI Layer (Qt Widgets/QML)         │
├─────────────────────────────────────┤
│  Business Logic Layer (std)        │
├─────────────────────────────────────┤
│  Data Layer (SQLite via QtSql)     │
├─────────────────────────────────────┤
│  PowerShell Bridge (QProcess)      │
└─────────────────────────────────────┘
```

### **Communication Flow**
```
VS Code Extension → QHttpServer → Business Logic → QProcess (PowerShell)
                    ↓
                    SQLite (Registry)
```

### **Key Principles**
1. **Separation of Concerns**: UI, logic, and data layers are separate
2. **Dependency Injection**: Pass dependencies, don't create them internally
3. **Interface Segregation**: Small, focused interfaces
4. **Error Transparency**: Use `std::expected` to propagate errors
5. **Async by Default**: All I/O operations are asynchronous

---

## 🎯 PROJECT-SPECIFIC RULES

### **Extension Management**
- **Extensions** are stored in `extensions/` directory
- **Registry** is SQLite database at `registry/registry.db`
- **PowerShell modules** go to `~/.local/share/powershell/Modules/`
- **VS Code extensions** go to `~/.vscode/extensions/`

### **Reverse Engineering Integration**
- **Cursor features** extracted to `Cursor_Source_Extracted/`
- **Codex output** generated to `CodexReverse.asm`
- **Integration files** created in `src/` with TODO comments
- **Reports** generated to `reverse_engineering_reports/`

### **Build System**
- **Main build**: `BUILD_ORCHESTRATOR.ps1` (7 modes)
- **Fast build**: `BUILD_IDE_FAST.ps1` (2-5 min)
- **Production build**: `BUILD_IDE_PRODUCTION.ps1` (15-30 min)
- **Output**: `build/` directory

### **Testing**
- **Unit tests**: Use Catch2 framework
- **Integration tests**: Use custom test harness
- **Validation**: Run `VALIDATE_REVERSE_ENGINEERING.ps1`

---

## 🎨 CODE VISUALIZATION

### **Theme**: Rose Pine (2026 Standard)
- **Base**: `#191724` (dark background)
- **Surface**: `#1f1d2e` (slightly lighter)
- **Overlay**: `#26233a` (borders)
- **Muted**: `#6e6a86` (comments)
- **Text**: `#e0def4` (primary text)
- **Love**: `#eb6f92` (errors)
- **Gold**: `#f6c177` (warnings)
- **Rose**: `#ebbcba` (strings)
- **Pine**: `#31748f` (keywords)
- **Foam**: `#9ccfd8` (functions)
- **Iris**: `#c4a7e7` (types)

### **Font**: Berkeley Mono or JetBrains Mono
- **Size**: 14pt
- **Ligatures**: Enabled
- **Line height**: 1.5

### **Terminal**: Warp or Rio
- **Shell**: PowerShell 7+ (pwsh)
- **Prompt**: Starship with custom config
- **Icons**: Nerd Fonts

---

## 🤖 AI INTEGRATION RULES

### **When AI Suggests Code**
1. **Check for `std::cout`**: Replace with `Logger::info()`
2. **Check for exceptions**: Replace with `std::expected`
3. **Check for raw pointers**: Replace with smart pointers
4. **Check for Qt/std mixing**: Separate UI and core logic
5. **Check for sync I/O**: Make it async with callbacks

### **Code Generation Prompts**

**For C++ Core Logic**:
```
Write a function that [DESCRIPTION] using std::expected for error handling.
Use Logger for logging, not std::cout.
Return std::expected<Result, Error>.
```

**For Qt UI**:
```
Create a QWidget that [DESCRIPTION].
Use QString for UI strings.
Connect signals to slots using modern syntax.
Keep business logic separate.
```

**For PowerShell Bridge**:
```
Create a QProcess that runs [COMMAND] asynchronously.
Capture stdout and stderr.
Use std::expected for error handling.
Log output with Logger.
```

---

## 📁 PROJECT STRUCTURE

```
d:\lazy init ide\
├── .cursorrules                          # AI context (this file)
├── .github/
│   └── copilot-instructions.md          # GitHub Copilot rules
├── src/                                 # IDE source files
│   ├── main.cpp
│   ├── mainwindow.cpp
│   ├── chatpanel.cpp
│   ├── editorwidget.cpp
│   ├── agentic_core.cpp
│   ├── mcp_integration.cpp
│   ├── ai_model_loader.cpp
│   └── codex_integration.cpp
├── include/                            # Header files
├── extensions/                         # Installed extensions
├── registry/                           # SQLite database
│   └── registry.db
├── reverse_engineering_reports/        # Analysis reports
│   ├── completeness_analysis.json
│   └── integration_manifest.json
├── Cursor_Source_Extracted/            # Cursor extraction
├── Cursor_Reverse_Engineered_Fork/     # Cursor fork
├── CodexReverse.asm                    # Codex output
├── BUILD_ORCHESTRATOR.ps1              # Main build system
├── REVERSE_ENGINEERING_MASTER.ps1      # Master integration
├── LAUNCH_REVERSE_ENGINEERING.ps1      # Simple launcher
└── VALIDATE_REVERSE_ENGINEERING.ps1    # Validation suite
```

---

## 🚀 DEVELOPMENT WORKFLOW

### **1. Start Development**
```powershell
# Validate system
.\VALIDATE_REVERSE_ENGINEERING.ps1

# Check completeness
.\REVERSE_ENGINEERING_MASTER.ps1 -Mode analyze

# Auto-integrate features
.\REVERSE_ENGINEERING_MASTER.ps1 -Mode auto -AutoDetect -IntegrateAll
```

### **2. Write Code**
- Follow C++ style guidelines above
- Use `std::expected` for errors
- Use `Logger` for logging
- Keep layers separate
- Write unit tests with Catch2

### **3. Build**
```powershell
# Quick build (2-5 min)
.\BUILD_ORCHESTRATOR.ps1 -Mode quick

# Production build (15-30 min)
.\BUILD_ORCHESTRATOR.ps1 -Mode production
```

### **4. Test**
```powershell
# Run validation
.\VALIDATE_REVERSE_ENGINEERING.ps1

# Run unit tests
.\run_tests.ps1
```

---

## 🎓 LEARNING RESOURCES

### **C++20 Features**
- `std::expected` - https://en.cppreference.com/w/cpp/utility/expected
- Concepts - https://en.cppreference.com/w/cpp/language/constraints
- Ranges - https://en.cppreference.com/w/cpp/ranges

### **Qt6 Documentation**
- QHttpServer - https://doc.qt.io/qt-6/qhttpserver.html
- QProcess - https://doc.qt.io/qt-6/qprocess.html
- QSqlDatabase - https://doc.qt.io/qt-6/qsqldatabase.html

### **PowerShell 7+**
- Install-Module - https://docs.microsoft.com/en-us/powershell/module/powershellget/install-module
- QProcess integration - https://doc.qt.io/qt-6/qprocess.html#details

---

## ⚠️ COMMON MISTAKES TO AVOID

### **❌ Mistake 1: Using std::cout**
```cpp
// Wrong
std::cout << "Error: " << error << std::endl;

// Right
Logger::error("Error: {}", error);
```

### **❌ Mistake 2: Using exceptions**
```cpp
// Wrong
throw std::runtime_error("Failed");

// Right
return std::unexpected(Error::Failed);
```

### **❌ Mistake 3: Raw pointers**
```cpp
// Wrong
Logger* logger = new Logger();
delete logger;

// Right
auto logger = std::make_unique<Logger>();
```

### **❌ Mistake 4: Mixing Qt/std**
```cpp
// Wrong
std::vector<QString> mixed;

// Right
std::vector<std::string> coreData;
QString uiString = QString::fromStdString(coreData[0]);
```

### **❌ Mistake 5: Sync I/O**
```cpp
// Wrong
QProcess process;
process.start("command");
process.waitForFinished();  // Blocks UI!

// Right
connect(&process, &QProcess::finished, this, &MyClass::onFinished);
process.start("command");
```

---

## 🎯 SUCCESS CRITERIA

### **Code Quality**
- ✅ All functions return `std::expected`
- ✅ All logging uses `Logger`
- ✅ No raw pointers
- ✅ No `std::cout`
- ✅ No exceptions
- ✅ Layers are separate

### **Build Success**
- ✅ `VALIDATE_REVERSE_ENGINEERING.ps1` passes (18/18)
- ✅ `BUILD_ORCHESTRATOR.ps1 -Mode quick` succeeds
- ✅ No compiler warnings
- ✅ All tests pass

### **Integration Success**
- ✅ Completeness score: 100%
- ✅ All integration files created
- ✅ Reports generated
- ✅ Codex accessible

---

## 🚀 NEXT STEPS

### **Immediate**
1. Read this file carefully
2. Run validation: `.\VALIDATE_REVERSE_ENGINEERING.ps1`
3. Check completeness: `.\REVERSE_ENGINEERING_MASTER.ps1 -Mode analyze`
4. Auto-integrate: `.\REVERSE_ENGINEERING_MASTER.ps1 -Mode auto -AutoDetect -IntegrateAll`

### **Short Term**
1. Implement the integration stubs in `src/`
2. Write unit tests with Catch2
3. Set up CI/CD pipeline
4. Create devcontainer configuration

### **Long Term**
1. Implement full MCP server
2. Create VS Code extension
3. Add more reverse engineering features
4. Optimize performance

---

## 📞 SUPPORT

### **If AI Suggests Wrong Code**
1. Check this file for correct patterns
2. Use the prompt templates above
3. Remind AI: "Use std::expected, not exceptions"
4. Remind AI: "Use Logger, not std::cout"

### **If Build Fails**
1. Run validation: `.\VALIDATE_REVERSE_ENGINEERING.ps1`
2. Check completeness: `.\REVERSE_ENGINEERING_MASTER.ps1 -Mode analyze`
3. Review integration manifest: `reverse_engineering_reports/integration_manifest.json`

### **If Integration Fails**
1. Check Cursor extraction: `Cursor_Source_Extracted/`
2. Check Codex output: `CodexReverse.asm`
3. Run integration manually: `.\REVERSE_ENGINEERING_MASTER.ps1 -Mode integrate -IntegrateAll`

---

**Last Updated**: 2026-01-25  
**Version**: 1.0.0  
**Maintainer**: RawrXD IDE Team
