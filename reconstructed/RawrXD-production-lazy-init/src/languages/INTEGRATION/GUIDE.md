// ===============================================================================
// INTEGRATION GUIDE - Adding Language Framework to Main RawrXD IDE
// ===============================================================================

/**
@file INTEGRATION_GUIDE.md
@brief Complete guide for integrating language framework into main IDE

## Quick Start Integration

This document provides step-by-step instructions for integrating the 
Language Framework into the main RawrXD IDE application.

### Files to Include

The language framework consists of 7 core files:

1. language_framework.hpp      - Core interface definitions
2. language_framework.cpp      - Implementation
3. language_widget.hpp         - Qt widget header
4. language_widget.cpp         - Qt widget implementation
5. language_integration.hpp    - Integration module header
6. language_integration.cpp    - Integration implementation
7. CMakeLists.txt             - Build configuration

All files should be placed in: `src/languages/`

### CMake Integration

Add to main project's CMakeLists.txt:

```cmake
# Add language framework library
add_subdirectory(src/languages)

# Link in main executable
target_link_libraries(${PROJECT_NAME} PRIVATE language_framework)

# Include paths
target_include_directories(${PROJECT_NAME} PRIVATE src/languages)
```

### Main Application Integration

In your main application class:

```cpp
// main_window.hpp
#include "language_framework.hpp"
#include "language_integration.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow();
    
private:
    void setupLanguageFramework();
};

// main_window.cpp
#include "main_window.hpp"

MainWindow::MainWindow() : QMainWindow() {
    // ... other setup code ...
    
    setupLanguageFramework();
}

void MainWindow::setupLanguageFramework() {
    // Initialize language framework in main window
    RawrXD::Languages::LanguageIntegration::initialize(this);
    
    // Connect editor signals
    connect(editorWidget, &Editor::fileOpened,
            this, &MainWindow::onEditorFileOpened);
    
    connect(editorWidget, &Editor::compileRequested,
            this, &MainWindow::onCompileRequested);
}

void MainWindow::onEditorFileOpened(const QString& filePath) {
    // Auto-detect language from opened file
    RawrXD::Languages::LanguageIntegration::onFileOpened(filePath);
}

void MainWindow::onCompileRequested(const QString& sourceFile) {
    // Handle compilation request
    RawrXD::Languages::LanguageIntegration::onCompileFile(sourceFile);
}
```

## Menu Integration

The language framework automatically creates a Languages menu in the menu bar.
The menu includes:

- Organized language list by category
- Manage Languages dialog
- Detect Language from File action
- Compile Current File action (Ctrl+B)
- Run Current File action (Ctrl+R)
- Language Settings action

No additional menu setup is required.

## Editor Integration Points

### File Detection

When a file is opened in the editor:

```cpp
// Auto-detect language
RawrXD::Languages::LanguageIntegration::onFileOpened(filePath);

// The framework will:
// 1. Detect language from extension
// 2. Select language in widget
// 3. Load language configuration
// 4. Update syntax highlighter (if integrated)
```

### Compilation

When user requests compilation:

```cpp
// Get current editor file
QString sourceFile = editorWidget->getCurrentFile();

// Request compilation
RawrXD::Languages::LanguageIntegration::onCompileFile(sourceFile);

// The framework will:
// 1. Show compilation progress in widget
// 2. Display output in integrated output window
// 3. Update status indicators
// 4. Handle errors gracefully
```

### Output Redirection

To redirect compilation output to IDE:

```cpp
// Redirect to IDE output window
void MainWindow::onLanguageOutput(bool success, const QString& output) {
    // Update your IDE's output window
    outputWindow_->appendText(output);
    outputWindow_->setStyle(success ? OutputStyle::Success : OutputStyle::Error);
}
```

## Widget Customization

### Accessing the Language Widget

```cpp
// Get reference to language widget
auto widget = RawrXD::Languages::LanguageIntegration::languageWidget();

// Connect to widget signals
connect(widget, &LanguageWidget::languageSelected, this, [this](LanguageType type) {
    // Handle language selection
    updateSyntaxHighlighter(type);
});

connect(widget, &LanguageWidget::compileRequested, this, [this](const QString& file) {
    // Handle compilation request
});

connect(widget, &LanguageWidget::outputUpdated, this, [this](const QString& text) {
    // Handle output update
});
```

### Styling the Widget

The widget includes a default theme, but can be customized:

```cpp
// Get widget
auto widget = RawrXD::Languages::LanguageIntegration::languageWidget();

// Apply custom stylesheet
widget->setStyleSheet(
    "QTreeWidget { background-color: #f0f0f0; }"
    "QTextEdit { background-color: #1e1e1e; color: #00ff00; }"
);

// Update theme
// widget->updateTheme("dark"); // Future enhancement
```

### Position and Visibility

```cpp
// Show/hide the widget
RawrXD::Languages::LanguageIntegration::setWidgetVisible(true);

// The widget is added as a dock widget in the bottom area by default
// It can be moved to other dock areas by user
```

## Syntax Highlighting Integration

If your IDE has syntax highlighting support:

```cpp
class SyntaxHighlighter {
    void updateForLanguage(RawrXD::Languages::LanguageType type) {
        switch (type) {
            case LanguageType::C:
            case LanguageType::CPP:
                // Load C/C++ rules
                loadCppRules();
                break;
            case LanguageType::Python:
                // Load Python rules
                loadPythonRules();
                break;
            // ... handle other languages
        }
    }
};

// Connect in main window
connect(widget, &LanguageWidget::languageSelected, this, 
        [this](LanguageType type) {
            highlighter->updateForLanguage(type);
        });
```

## Error Handling

The framework provides comprehensive error handling:

```cpp
// Check if language is supported
auto config = RawrXD::Languages::LanguageFactory::getLanguageConfig(type);
if (!config || !config->isSupported) {
    showErrorDialog("Language not supported");
    return;
}

// Handle compilation errors
auto result = RawrXD::Languages::LanguageManager::instance().compileFile(sourceFile);
if (!result.success) {
    qWarning() << "Compilation failed:" << result.errorMessage;
    showErrorDialog(result.errorMessage);
}

// Handle runtime errors
QString output, error;
bool success = runtime.execute(sourceFile, {}, output, error);
if (!success) {
    qWarning() << "Runtime error:" << error;
    outputWindow->appendText("Error: " + error);
}
```

## Performance Considerations

### Lazy Initialization

The language framework uses lazy initialization:

```cpp
// First access initializes the framework
auto config = LanguageFactory::getLanguageConfig(type);
// Configuration loaded on first access, cached thereafter
```

### Caching

Language configurations are cached:

```cpp
// First call initializes all languages
auto languages = LanguageFactory::getAllLanguages(); // ~1ms

// Subsequent calls return cached data
auto languages2 = LanguageFactory::getAllLanguages(); // O(1)
```

### Parallel Compilation (Future)

```cpp
// Framework ready for parallel compilation
// To enable when implemented:
#define ENABLE_PARALLEL_COMPILATION 1

// Compile multiple files in parallel
CompilationPipeline pipeline1(LanguageType::C);
CompilationPipeline pipeline2(LanguageType::CPP);

// Launch in parallel
auto result1 = pipeline1.compile(file1);
auto result2 = pipeline2.compile(file2);
```

## Testing Integration

### Unit Tests

```cpp
#include <QtTest>
#include "language_framework.hpp"

class LanguageFrameworkTest : public QObject {
    Q_OBJECT
    
private slots:
    void testLanguageDetection() {
        auto type = RawrXD::Languages::LanguageFactory::detectLanguageFromFile("test.cpp");
        QCOMPARE(type, LanguageType::CPP);
    }
    
    void testCompilation() {
        // Test compilation
        auto result = RawrXD::Languages::LanguageManager::instance()
                                        .compileFile("test.c");
        QVERIFY(result.success);
    }
};
```

### Integration Tests

```cpp
void MainWindow::testLanguageIntegration() {
    // Open file
    openFile("test.py");
    
    // Verify language detected
    QVERIFY(language == LanguageType::Python);
    
    // Verify widget shows Python
    auto selected = widget->selectedLanguage();
    QCOMPARE(selected, LanguageType::Python);
}
```

## Troubleshooting

### Issue: Language not detected
```cpp
// Check file extension mapping
auto type = LanguageFactory::detectLanguageFromExtension(".myext");
if (type == LanguageType::Unknown) {
    // Extension not in map
    qDebug() << "Unknown extension";
}
```

### Issue: Compiler not found
```cpp
// Check compiler path
auto config = LanguageFactory::getLanguageConfig(LanguageType::C);
qDebug() << "Compiler:" << config->compilerPath;

// Verify in PATH
QProcess proc;
proc.setProgram(config->compilerPath);
bool found = proc.startDetached();
```

### Issue: Widget not appearing
```cpp
// Check if initialized
auto widget = LanguageIntegration::languageWidget();
if (!widget) {
    qWarning() << "Widget not initialized";
}

// Check if visible
RawrXD::Languages::LanguageIntegration::setWidgetVisible(true);
```

## Configuration Files

### Future: Language profiles (config/languages/)

```ini
[C]
compiler=gcc
linker=ld
flags=-Wall -O2
standard=c99

[C++]
compiler=g++
linker=ld
flags=-Wall -O2
standard=c++17

[Python]
interpreter=python3
flags=--optimize
```

### Future: User settings (config/language_settings.json)

```json
{
  "default_language": "cpp",
  "auto_detect": true,
  "compilation_timeout": 30000,
  "recent_languages": ["cpp", "python", "rust"],
  "installed_languages": ["c", "cpp", "python", "go", "rust"]
}
```

## Extending the Framework

### Adding a New Language

1. Add enum value to LanguageType
2. Create configuration in initializeLanguages()
3. Add file extension mapping
4. Create parser/linker/compiler implementations

Example: Adding D Language

```cpp
// In language_framework.hpp
enum class LanguageType {
    // ... existing languages ...
    D,  // New
};

// In language_framework.cpp LanguageFactory::initializeLanguages()
auto d_config = std::make_shared<LanguageConfig>();
d_config->type = LanguageType::D;
d_config->name = "d";
d_config->displayName = "D";
d_config->extensions = {".d", ".di"};
d_config->compilerPath = "dmd";
d_config->linkerPath = "ld";
d_config->category = "Systems Programming";
d_config->isSupported = true;
d_config->priority = 6;
configs_[LanguageType::D] = d_config;

// In detectLanguageFromExtension()
{".d", LanguageType::D},
{".di", LanguageType::D},
```

## API Reference Quick Link

Key classes and their usage:

| Class | Purpose | Example |
|-------|---------|---------|
| LanguageType | Enum for all languages | `LanguageType::CPP` |
| LanguageFactory | Static factory for creation | `LanguageFactory::detectLanguageFromFile()` |
| LanguageManager | Singleton manager | `LanguageManager::instance().compileFile()` |
| LanguageWidget | Qt widget | Used in UI |
| LanguageIntegration | IDE integration | Called from main window |
| CompilationPipeline | Compilation stages | Multi-stage compilation |
| LanguageRuntime | Execution | `runtime.execute()` |

## Support

For issues or questions about integration:
1. Check LANGUAGE_FRAMEWORK_README.md
2. Review example integration code above
3. Examine CMakeLists.txt for build issues
4. Check compiler and path configurations

---

**Integration Status**: Ready for Production
**Version**: 1.0
**Last Updated**: 2024

*/
