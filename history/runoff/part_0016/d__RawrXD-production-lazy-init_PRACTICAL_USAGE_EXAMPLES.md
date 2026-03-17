// ===============================================================================
// RAWRXD - PRACTICAL USAGE EXAMPLES & INTEGRATION GUIDE
// ===============================================================================

# RawrXD IDE - Practical Examples & Usage Patterns

## Part 1: CLI Usage Examples

### Example 1: Interactive Project Development

```bash
# Start interactive session
$ rawrxd-cli --interactive

# Open your project
rawrxd> project open ~/my-cpp-project

# Configure and build
rawrxd [my-cpp-project]> configure
rawrxd [my-cpp-project]> build --config Release

# Check Git status
rawrxd [my-cpp-project]> git status

# Stage and commit changes
rawrxd [my-cpp-project]> git add .
rawrxd [my-cpp-project]> git commit "Added feature X"

# Push to remote
rawrxd [my-cpp-project]> git push origin main

# Run tests
rawrxd [my-cpp-project]> test discover
rawrxd [my-cpp-project]> test run

# Get AI help
rawrxd [my-cpp-project]> ai explain "// Show me what this function does"

# Exit
rawrxd [my-cpp-project]> exit
```

### Example 2: Single Command Execution

```bash
# Build a specific project
$ rawrxd-cli project open ~/project && rawrxd-cli build

# Compile a file with language framework
$ rawrxd-cli compile ~/source/main.cpp

# Get AI suggestions
$ rawrxd-cli ai infer "How can I optimize this code?"

# Get system info
$ rawrxd-cli diag info
```

### Example 3: Batch File for CI/CD Pipeline

**File: build_pipeline.txt**
```
# Build pipeline batch script
project open .

# Build with Release config
build --config Release

# Run tests
test discover
test run

# Get coverage
test coverage

# Generate diagnostics
diag run

# Print success
echo "Pipeline completed successfully"
```

**Execute:**
```bash
$ rawrxd-cli --headless --batch build_pipeline.txt --json
```

**Output (JSON):**
```json
{
  "success": true,
  "commands_executed": 4,
  "total_duration_ms": 15230,
  "results": [
    {
      "command": "build",
      "success": true,
      "duration_ms": 10000,
      "output": {...}
    },
    {
      "command": "test run",
      "success": true,
      "duration_ms": 5000,
      "tests_passed": 42,
      "tests_failed": 0
    },
    ...
  ]
}
```

### Example 4: Language-Specific Compilation

```bash
# Compile C++ file
$ rawrxd-cli compile src/main.cpp
# Output: main (executable)

# Compile Python script
$ rawrxd-cli compile scripts/analyze.py
# Validates syntax, returns status

# Compile Rust project
$ rawrxd-cli compile Cargo.toml
# Output: target/release/project

# Run compiled program
$ rawrxd-cli exec ./main --args "hello world"
```

---

## Part 2: GUI Usage Examples

### Example 1: Opening a Project & Building

```
1. Start RawrXD IDE
   $ RawrXD.exe

2. File → Open Project
   - Select ~/my-project
   - Project opens, files show in left panel

3. View → Show Terminal
   - Terminal opens at bottom

4. Build → Configure
   - CMake runs, initializes build system

5. Build → Build (or Ctrl+B)
   - Ninja build starts
   - Output streams to terminal in real-time
   - Errors appear in Problems panel
   - Green checkmark in status bar = success

6. Build → Clean (or Ctrl+Shift+B)
   - Cleans build artifacts
   - Ready for fresh build
```

### Example 2: AI-Assisted Code Review

```
1. Open file with code you want reviewed
   - Click on file in File Browser
   - Code appears in editor

2. Select problematic code
   - Triple-click to select function
   - Or drag to select region

3. Right-click → "Explain with AI"
   - Or use Ctrl+Alt+E

4. AI Chat panel opens with analysis:
   "This function has O(n²) complexity.
    Consider using a hash map instead..."

5. Click "Generate Fix"
   - AI generates improved version

6. Review and Accept
   - Modified code appears
   - Use Ctrl+Z to undo if needed
```

### Example 3: Running & Debugging Tests

```
1. Build → Show Test Explorer
   - Test Explorer panel opens
   - Lists all discovered tests

2. Select test suite
   - Expands to show individual tests
   - Blue icon = not run
   - Green = passed
   - Red = failed

3. Right-click test → Run
   - Test executes
   - Output in terminal
   - Status updates immediately

4. Debug → Show Debugger
   - Debugger panel opens
   - Set breakpoints (click margin)

5. Right-click test → Debug
   - Test runs with debugger
   - Stops at breakpoint
   - Inspect variables in debugger panel

6. Debug → Step Over/Into/Out
   - Navigate through code
   - Watch expressions update
```

### Example 4: Using Multiple Files & Tabs

```
1. File → New Tab
   - New empty tab appears

2. File → Open... (multiple times)
   - Each file opens in separate tab
   - Tab shows filename

3. Click tabs to switch
   - Active tab highlighted
   - Editor updates immediately
   - Syntax highlighting adjusts for language

4. Split View (View → Split Horizontal/Vertical)
   - Editor area splits
   - Drag tab to split
   - Edit two files side-by-side

5. Search → Find in Files
   - Search dialog appears
   - Shows matches across all files
   - Click to jump to file & location
```

---

## Part 3: Language Framework Integration

### Example 1: Compile Multiple Languages

**CLI:**
```bash
# Single command to compile multiple files
$ rawrxd-cli compile src/main.cpp
$ rawrxd-cli compile scripts/process.py
$ rawrxd-cli compile web/app.js
$ rawrxd-cli compile backend/routes.go

# Auto-detects language from extension
# Returns appropriate compiler output
```

**GUI:**
1. Languages → C++ (from menu)
   - C++ selected in language widget
   - Widget shows C++ configuration

2. Languages → Python
   - Switches to Python config
   - Shows Python compiler settings

3. File → Open src/main.rs
   - Rust detected automatically
   - Language widget updates to Rust
   - Keyboard shortcut Ctrl+B now compiles Rust
```

### Example 2: Language Management

**CLI:**
```bash
# List all supported languages
$ rawrxd-cli languages list
Output:
  C               (c, h)
  C++             (cpp, cxx, hpp)
  Rust            (rs)
  Python          (py, pyw)
  ... 44 more languages

# Check if language is installed
$ rawrxd-cli languages check cpp
Output: C++ compiler found: g++ (v11.2)

# Get language details
$ rawrxd-cli languages info rust
Output:
  Name: Rust
  Compiler: rustc
  Category: Systems Programming
  Extensions: .rs
```

**GUI:**
1. Languages → Manage Languages
   - Language widget opens in dock
   - Hierarchical view by category
   - Shows installation status

2. Search for language
   - Type "rust" in search
   - Rust appears with details

3. Click language
   - Shows details panel
   - Compiler path, extensions, etc.
   - "Reinstall" button if needed

4. Right-click category
   - Shows all languages in category
   - Install/uninstall options
```

### Example 3: Code Compilation & Execution

**C++ Example:**
```bash
# CLI
$ rawrxd-cli compile src/main.cpp --output bin/main
Successfully compiled: bin/main (5.2s)

$ rawrxd-cli exec ./bin/main --args "param1 param2"
Output: Hello World!
```

**Python Example:**
```bash
# CLI
$ rawrxd-cli compile scripts/analyze.py
Syntax OK (Python 3.9)

$ rawrxd-cli exec ./scripts/analyze.py --args "input.txt"
Output: Analysis complete. Results: ...
```

**Web Example:**
```bash
# CLI
$ rawrxd-cli compile web/app.js
Compiled: web/app.js

$ rawrxd-cli exec node web/app.js --args "--port 3000"
Output: Server running on port 3000
```

---

## Part 4: Advanced Integration Patterns

### Pattern 1: Custom CLI Command Handler

**Adding new command to CLI:**

```cpp
// In src/cli/cli_main.cpp

// Step 1: Add handler declaration
private:
    int handleMyCommand(const QStringList& args);

// Step 2: Route in executeCommand()
else if (cmd == "mycommand") {
    return handleMyCommand(cmdArgs);
}

// Step 3: Implement handler
int CLIApp::handleMyCommand(const QStringList& args) {
    // Parse arguments
    QString param1 = args.isEmpty() ? "" : args[0];
    
    // Call OrchestraManager
    auto& om = OrchestraManager::instance();
    auto result = om.myFeature(param1);
    
    // Output result
    if (m_jsonOutput) {
        QJsonDocument doc(result.toJson());
        m_out << doc.toJson();
    } else {
        if (result.success) {
            printSuccess(result.message);
        } else {
            printError(result.errorMessage);
        }
    }
    
    return result.success ? 0 : 1;
}

// Step 4: Use immediately
// rawrxd-cli mycommand param1
```

### Pattern 2: Custom GUI Panel

**Adding new panel to GUI:**

```cpp
// In src/qtapp/MainWindow_v5.h
private:
    class MyCustomPanel* m_myPanel;

// In src/qtapp/MainWindow_v5.cpp

void MainWindow_v5::setupDockWidgets() {
    // Create panel
    m_myPanel = new MyCustomPanel(this);
    
    // Create dock widget
    auto dock = new QDockWidget("My Feature", this);
    dock->setWidget(m_myPanel);
    dock->setObjectName("MyFeatureDock");
    addDockWidget(Qt::BottomDockWidgetArea, dock);
    
    // Connect OrchestraManager signals
    auto& om = OrchestraManager::instance();
    connect(&om, &OrchestraManager::mySignal,
            m_myPanel, &MyCustomPanel::onMySignal);
    
    // Connect panel actions
    connect(m_myPanel, &MyCustomPanel::actionRequested,
            this, [&om](const QString& action) {
        om.performAction(action);
    });
    
    // Add to View menu
    auto viewMenu = menuBar()->findChild<QMenu*>();
    auto toggleAction = viewMenu->addAction("Show My Feature");
    connect(toggleAction, &QAction::triggered, [dock]() {
        dock->show();
        dock->raise();
    });
}
```

### Pattern 3: Language Extension

**Adding new language support:**

```cpp
// In src/languages/language_framework.cpp

void LanguageFactory::initializeLanguages() {
    // ... existing languages ...
    
    // Add new language
    auto mylan_config = std::make_shared<LanguageConfig>();
    mylan_config->type = LanguageType::MyLanguage;
    mylan_config->name = "mylanguage";
    mylan_config->displayName = "My Language";
    mylan_config->extensions = {".ml", ".myl"};
    mylan_config->compilerPath = "mylang-compiler";
    mylan_config->linkerPath = "mylang-linker";
    mylan_config->category = "Custom Languages";
    mylan_config->isSupported = true;
    mylan_config->priority = 100;
    
    configs_[LanguageType::MyLanguage] = mylan_config;
}

// Now available in both CLI & GUI:
// CLI: rawrxd-cli compile file.ml
// GUI: Languages → Custom Languages → My Language
```

---

## Part 5: Workflow Scenarios

### Scenario 1: Daily Development Workflow

**Morning - Start Session:**
```bash
$ rawrxd-cli --interactive

rawrxd> project open ~/customer-project

rawrxd [customer-project]> git pull origin main
# Updates from team

rawrxd [customer-project]> build --config Debug
# Builds with debug symbols for local testing

rawrxd [customer-project]> test run
# All tests pass
```

**During Day - Feature Development:**
```bash
# In GUI IDE (RawrXD.exe)
1. Open feature branch
   File → Open Project → recent "customer-project"

2. Create new file
   File → New File → name "feature_x.cpp"

3. Write code
   - Code editor with syntax highlighting
   - Real-time error detection

4. Get AI help
   Languages → Explain with AI
   - AI provides guidance on implementation

5. Build
   Build → Build (Ctrl+B)
   - Incremental build
   - Errors shown in Problems panel

6. Test
   Build → Show Test Explorer
   - Run related tests
   - All pass
```

**End of Day - Commit:**
```bash
$ rawrxd-cli

rawrxd [customer-project]> git status
# Shows modified files

rawrxd [customer-project]> git add .

rawrxd [customer-project]> git commit "Implement feature X"

rawrxd [customer-project]> git push origin feature-x-branch
# Ready for code review
```

### Scenario 2: CI/CD Pipeline Integration

**Batch File: ci_build.txt**
```
# Load project
project open .

# Build for all configurations
build --config Debug
build --config Release

# Run full test suite
test discover
test run

# Generate coverage report
test coverage

# Run diagnostics
diag run

# Generate build artifact
exec mkdir -p artifacts
exec cp build/Release/app artifacts/
```

**Execute in CI/CD:**
```bash
# In GitHub Actions / Azure Pipelines
$ rawrxd-cli --headless --batch ci_build.txt --json --fail-fast

# Outputs JSON for:
# - Build results
# - Test results
# - Coverage metrics
# - Diagnostic info

# Set exit code for pipeline
# 0 = success, 1 = failure
```

### Scenario 3: Multi-Language Project

**Project Structure:**
```
myproject/
├── cpp/
│   └── core.cpp
├── python/
│   └── analysis.py
├── web/
│   └── app.js
└── go/
    └── server.go
```

**Build All Components:**

**CLI:**
```bash
rawrxd> project open .

# Build C++ component
rawrxd [myproject]> compile cpp/core.cpp --output bin/core

# Validate Python
rawrxd [myproject]> compile python/analysis.py

# Bundle JavaScript
rawrxd [myproject]> compile web/app.js --output dist/app.js

# Build Go binary
rawrxd [myproject]> compile go/server.go --output bin/server

# All components built successfully
```

**GUI:**
1. File Browser shows all files
2. Select cpp/core.cpp
   - Languages → C++
   - Build → Build (compiles C++)

3. Select python/analysis.py
   - Languages → Python (auto-detected)
   - Build → Build (validates syntax)

4. Select web/app.js
   - Languages → JavaScript (auto-detected)
   - Build → Build (transpiles)

5. Select go/server.go
   - Languages → Go (auto-detected)
   - Build → Build (builds binary)

---

## Part 6: Command Reference Quick Guide

### Project Commands
```bash
project open <path>          # Open existing project
project close                # Close current project
project create <path> <tpl>  # Create new from template
```

### Build Commands
```bash
build [--config Release]     # Build project
clean                        # Remove build artifacts
rebuild                      # Clean + build
configure                    # Run CMake configure
```

### Language Commands
```bash
compile <file>               # Compile single file
languages list               # List all supported languages
languages check <lang>       # Check if installed
languages info <lang>        # Get language details
```

### Git Commands
```bash
git status                   # Show status
git add [files]              # Stage changes
git commit <msg>             # Commit staged
git push <remote> <branch>   # Push to remote
git pull <remote> <branch>   # Pull from remote
git branch <name>            # Create branch
git checkout <branch>        # Switch branch
git log [--count N]          # Show commit log
```

### AI Commands
```bash
ai infer <prompt>            # Get AI response
ai complete <context>        # Code completion
ai explain <code>            # Explain code
ai refactor <code> <instr>   # Refactor code
```

### Test Commands
```bash
test discover                # Find all tests
test run [test-ids]          # Execute tests
test coverage                # Get coverage report
```

### System Commands
```bash
diag run                     # Run diagnostics
diag info                    # System information
diag status                  # Current status
exec <command>               # Execute shell command
shell <command>              # Execute in shell
```

---

## CONCLUSION: Choosing CLI vs GUI

### Use CLI When:
✓ Running automated builds (CI/CD)  
✓ Running on remote/headless servers  
✓ Scripting build pipelines  
✓ Integrating with shell scripts  
✓ Batch processing multiple projects  
✓ Quick command-line development  

### Use GUI When:
✓ Interactive development  
✓ Visual debugging  
✓ Exploring project structure  
✓ Testing with visual feedback  
✓ Refactoring with IDE assistance  
✓ Team collaboration features  

### Use Both:
✓ GUI for development  
✓ CLI for CI/CD pipelines  
✓ Same project, same backend  
✓ Seamless workflow integration  

