# Integration Guide - Context System & Drawing Engine

## Overview

This guide explains how to integrate the Breadcrumb Context System and Custom Drawing Engine with the existing RawrXD IDE.

---

## Integration Architecture

```
RawrXD IDE
├── Existing Components
│   ├── MainWindow
│   ├── MultiTabEditor
│   ├── TaskOrchestrator
│   ├── AgenticIDE
│   └── ModelTrainer
│
├── New Context System
│   ├── BreadcrumbContextManager
│   ├── ContextVisualizer
│   └── BreadcrumbRenderer
│
└── Custom Drawing Engine
    ├── DrawingContext
    ├── Path & Surface
    └── GUI Components
```

---

## Step 1: Update CMakeLists.txt

```cmake
# In e:\RawrXD\CMakeLists.txt

# Add new include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include/context
    ${PROJECT_SOURCE_DIR}/include/drawing
    ${PROJECT_SOURCE_DIR}/include/visualization
)

# Add new source files
set(SOURCES
    ${SOURCES}
    src/context/BreadcrumbContextManager.cpp
    src/drawing/DrawingEngine.cpp
    src/visualization/ContextVisualizer.cpp
)

# Ensure Qt6 components are linked
find_package(Qt6 COMPONENTS 
    Core 
    Gui 
    Widgets 
    Network
    REQUIRED
)

target_link_libraries(RawrXD-AgenticIDE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)
```

---

## Step 2: Update MainWindow Integration

### Header (ide_main_window.h)

```cpp
#pragma once

#include <QMainWindow>
#include "context/BreadcrumbContextManager.h"
#include "visualization/ContextVisualizer.h"

using namespace RawrXD::Context;
using namespace RawrXD::Visualization;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    // Existing members
    // ...

    // New context system
    BreadcrumbContextManager* m_contextManager;
    ContextWindow* m_contextWindow;
    
    void initializeContextSystem();
    void connectContextSignals();
};
```

### Implementation (ide_main_window.cpp)

```cpp
#include "ide_main_window.h"
#include <QVBoxLayout>
#include <QSplitter>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_contextManager(nullptr),
      m_contextWindow(nullptr) {
    
    // ... existing initialization ...
    
    // Initialize context system
    initializeContextSystem();
    connectContextSignals();
}

void MainWindow::initializeContextSystem() {
    // Create context manager
    m_contextManager = new BreadcrumbContextManager(this);
    m_contextManager->initialize(QCoreApplication::applicationDirPath());
    
    // Create visualization window
    Rect contextBounds(0, 0, 300, 600);
    m_contextWindow = new ContextWindow(*m_contextManager, contextBounds);
    m_contextWindow->setShowBreadcrumbs(true);
    m_contextWindow->setShowContextPanel(true);
    m_contextWindow->setShowGraphView(true);
    
    // Add as dockable widget to main window
    QWidget* contextWidget = new QWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, 
                  dynamic_cast<QDockWidget*>(contextWidget));
}

void MainWindow::connectContextSignals() {
    // Connect file open events
    connect(this, &MainWindow::fileOpened, 
            [this](const QString& filePath) {
                m_contextManager->registerFile(filePath);
            });
    
    // Connect editor state changes
    connect(this, &MainWindow::editorStateChanged,
            [this](const QString& filePath, int line, int col) {
                OpenEditorContext editorCtx;
                editorCtx.cursorLine = line;
                editorCtx.cursorColumn = col;
                m_contextManager->registerOpenEditor(filePath, editorCtx);
            });
}
```

---

## Step 3: Integrate with MultiTabEditor

### Connect File Operations

```cpp
// In multi_tab_editor.cpp

void MultiTabEditor::onFileOpened(const QString& filePath) {
    // Existing code...
    
    // Register with context manager
    if (m_contextManager) {
        m_contextManager->registerFile(filePath);
        m_contextManager->pushContextBreadcrumb(ContextType::File, filePath);
    }
}

void MultiTabEditor::onCursorPositionChanged(int line, int column) {
    // Existing code...
    
    // Update editor context
    if (m_contextManager) {
        auto currentFile = getCurrentFilePath();
        m_contextManager->updateEditorState(currentFile, line, column);
    }
}

void MultiTabEditor::onSymbolClicked(const QString& symbolName) {
    // Existing code...
    
    // Navigate context
    if (m_contextManager) {
        m_contextManager->pushContextBreadcrumb(ContextType::Symbol, symbolName);
    }
}
```

---

## Step 4: Integrate with TaskOrchestrator

### Track Task Execution

```cpp
// In orchestration/TaskOrchestrator.cpp

void TaskOrchestrator::executeTask(const TaskDefinition& task) {
    // Existing orchestration code...
    
    // Register task in context
    if (m_contextManager) {
        ToolContext toolCtx;
        toolCtx.toolName = task.model;
        toolCtx.version = "1.0";
        toolCtx.isAvailable = true;
        m_contextManager->registerTool(task.model, toolCtx);
        
        m_contextManager->pushContextBreadcrumb(
            ContextType::Tool,
            task.model
        );
    }
}
```

---

## Step 5: Integrate with AgenticIDE

### Track Agent Operations

```cpp
// In agentic/agentic_ide.cpp

void AgenticIDE::executeAgentTask(const QString& taskDescription) {
    // Existing agent code...
    
    // Track agent operations
    if (m_contextManager) {
        InstructionBlock instr;
        instr.id = "agent_" + QUuid::createUuid().toString();
        instr.title = "Agent Task";
        instr.content = taskDescription;
        instr.isVisible = true;
        m_contextManager->registerInstruction(instr);
    }
}
```

---

## Step 6: Create Context Panels

### Symbols Panel

```cpp
// Create a custom panel for symbols
class SymbolsPanel : public QWidget {
public:
    SymbolsPanel(BreadcrumbContextManager& contextMgr, QWidget* parent = nullptr);
    
private:
    BreadcrumbContextManager& m_contextManager;
    QListWidget* m_symbolList;
    
    void populateSymbols(const QString& filePath);
    void onSymbolClicked(const QString& symbol);
};
```

### File Hierarchy Panel

```cpp
class FileHierarchyPanel : public QWidget {
public:
    FileHierarchyPanel(BreadcrumbContextManager& contextMgr, QWidget* parent = nullptr);
    
private:
    BreadcrumbContextManager& m_contextManager;
    QTreeWidget* m_fileTree;
    
    void buildFileTree();
    void onFileClicked(const QString& filePath);
};
```

### Relationships Panel

```cpp
class RelationshipsPanel : public QWidget {
public:
    RelationshipsPanel(BreadcrumbContextManager& contextMgr, QWidget* parent = nullptr);
    
private:
    BreadcrumbContextManager& m_contextManager;
    QGraphicsView* m_graphicsView;
    
    void renderDependencyGraph(const QString& entity);
};
```

---

## Step 7: Create Custom Renderers

### IDE-Specific Context Renderer

```cpp
class IDEContextRenderer {
public:
    IDEContextRenderer(BreadcrumbContextManager& contextMgr);
    
    // Render current file with symbols
    void renderFileContext(DrawingContext& ctx, const QString& filePath);
    
    // Render open editors
    void renderOpenEditors(DrawingContext& ctx);
    
    // Render recent files
    void renderRecentFiles(DrawingContext& ctx);
    
    // Render project structure
    void renderProjectStructure(DrawingContext& ctx);
    
private:
    BreadcrumbContextManager& m_contextManager;
};
```

---

## Step 8: Add Context Dock Widget

### Create QDockWidget Wrapper

```cpp
// contextdockwidget.h
#pragma once

#include <QDockWidget>
#include "context/BreadcrumbContextManager.h"
#include "visualization/ContextVisualizer.h"

class ContextDockWidget : public QDockWidget {
    Q_OBJECT

public:
    ContextDockWidget(const QString& title, QWidget* parent = nullptr);
    ~ContextDockWidget();
    
    void initialize(BreadcrumbContextManager& contextMgr);
    void navigateTo(const QString& entity);

private:
    BreadcrumbContextManager* m_contextManager;
    RawrXD::Visualization::ContextWindow* m_contextWindow;
    
    void setupUI();
};
```

### Add to Main Window

```cpp
// In ide_main_window.cpp

void MainWindow::setupUI() {
    // ... existing UI setup ...
    
    // Create context dock
    ContextDockWidget* contextDock = new ContextDockWidget("Context", this);
    contextDock->initialize(*m_contextManager);
    addDockWidget(Qt::RightDockWidgetArea, contextDock);
    
    // Connect signals
    connect(this, &MainWindow::entitySelected,
            contextDock, &ContextDockWidget::navigateTo);
}
```

---

## Step 9: Keyboard Shortcuts

### Register Context Navigation Shortcuts

```cpp
// In MainWindow::setupShortcuts()

// Breadcrumb navigation
new QShortcut(Qt::CTRL + Qt::Key_Left, this, 
              [this]() { m_contextManager->getBreadcrumbChain().pop(); });

new QShortcut(Qt::CTRL + Qt::Key_Right, this,
              [this]() { 
                  auto& chain = m_contextManager->getBreadcrumbChain();
                  chain.jump(chain.getCurrentIndex() + 1);
              });

// Show context panel
new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_P, this,
              [this]() { m_contextWindow->setShowContextPanel(true); });

// Show dependency graph
new QShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G, this,
              [this]() { m_contextWindow->setShowGraphView(true); });
```

---

## Step 10: Custom Rendering Integration

### Render to Qt Widget

```cpp
// contextcanvaswidget.h
#pragma once

#include <QWidget>
#include "drawing/DrawingEngine.h"

class ContextCanvasWidget : public QWidget {
    Q_OBJECT

public:
    ContextCanvasWidget(QWidget* parent = nullptr);
    
    DrawingContext& getDrawingContext();
    void updateContent();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::unique_ptr<RawrXD::Drawing::DrawingContext> m_drawingContext;
};
```

### Implementation

```cpp
// contextcanvaswidget.cpp

void ContextCanvasWidget::paintEvent(QPaintEvent* event) {
    if (!m_drawingContext) return;
    
    const uint32_t* buffer = m_drawingContext->getBuffer();
    int width = m_drawingContext->getWidth();
    int height = m_drawingContext->getHeight();
    
    QImage image(reinterpret_cast<const uchar*>(buffer),
                 width, height, QImage::Format_RGBA8888);
    
    QPainter painter(this);
    painter.drawImage(0, 0, image);
}

void ContextCanvasWidget::updateContent() {
    m_drawingContext->clear(Color(255, 255, 255));
    // ... render content ...
    update();
}
```

---

## Step 11: Configuration & Settings

### Add Context Settings

```cpp
// In settings/settings.h

class ContextSettings {
public:
    bool showBreadcrumbs;
    bool showContextPanel;
    bool showGraphView;
    bool autoIndexWorkspace;
    int contextPanelWidth;
    bool enableSymbolTracking;
    bool enableRelationshipTracking;
    
    void load();
    void save();
};
```

---

## Step 12: Signals and Slots

### Define Context Manager Signals

```cpp
// Connect to IDE events

connect(m_multiTabEditor, &MultiTabEditor::fileOpened,
        m_contextManager, [this](const QString& file) {
            m_contextManager->registerFile(file);
        });

connect(m_taskOrchestrator, &TaskOrchestrator::taskStarted,
        m_contextManager, [this](const TaskDefinition& task) {
            // Register task
        });

connect(m_contextManager, &BreadcrumbContextManager::contextChanged,
        this, &MainWindow::onContextChanged);

connect(m_contextManager, &BreadcrumbContextManager::breadcrumbNavigated,
        m_contextWindow, &ContextWindow::navigateToBreadcrumb);
```

---

## Step 13: Testing

### Unit Tests

```cpp
// tests/test_context_integration.cpp

#include <gtest/gtest.h>
#include "context/BreadcrumbContextManager.h"

class ContextIntegrationTest : public ::testing::Test {
protected:
    BreadcrumbContextManager contextMgr;
    
    void SetUp() override {
        contextMgr.initialize("/tmp/test_workspace");
    }
};

TEST_F(ContextIntegrationTest, FileRegistration) {
    contextMgr.registerFile("test.cpp");
    auto ctx = contextMgr.getFileContext("test.cpp");
    EXPECT_EQ(ctx.fileName, "test.cpp");
}

TEST_F(ContextIntegrationTest, BreadcrumbNavigation) {
    contextMgr.pushContextBreadcrumb(ContextType::File, "test.cpp");
    auto& chain = contextMgr.getBreadcrumbChain();
    EXPECT_EQ(chain.getChainLength(), 1);
    
    chain.pop();
    EXPECT_EQ(chain.getCurrentIndex(), -1);
}
```

---

## Step 14: Performance Optimization

### Lazy Loading

```cpp
class LazyContextManager {
private:
    bool m_indexedWorkspace;
    
public:
    void ensureIndexed() {
        if (!m_indexedWorkspace) {
            m_contextManager->indexWorkspace();
            m_indexedWorkspace = true;
        }
    }
};
```

### Caching

```cpp
// Cache frequently accessed relationships
class CachedRelationshipQuery {
private:
    QMap<QString, QList<RelationshipContext>> m_relationshipCache;
    
public:
    void invalidateCache(const QString& entityId) {
        m_relationshipCache.remove(entityId);
    }
};
```

---

## Step 15: UI/UX Polish

### Breadcrumb Tooltip

```cpp
void BreadcrumbRenderer::renderBreadcrumbItem(DrawingContext& ctx, 
                                               const Breadcrumb& crumb,
                                               const Rect& bounds,
                                               bool isActive) {
    // Render tooltip on hover
    if (m_hoveredIndex >= 0 && m_hoveredIndex == index) {
        renderTooltip(ctx, crumb.displayName, bounds);
    }
}
```

### Smooth Transitions

```cpp
class ContextWindowAnimator {
public:
    void animateTabChange(const QString& fromTab, const QString& toTab);
    void animateBreadcrumbNavigation(int fromIndex, int toIndex);
    
private:
    QTimer m_animationTimer;
    float m_animationProgress;
};
```

---

## Deployment Checklist

- [ ] Update CMakeLists.txt with new source files
- [ ] Add include paths for new headers
- [ ] Link Qt6::Core and Qt6::Gui
- [ ] Create context manager in MainWindow
- [ ] Connect file open/close events
- [ ] Add context dock widget
- [ ] Setup keyboard shortcuts
- [ ] Configure settings/preferences
- [ ] Add unit tests
- [ ] Performance testing
- [ ] Documentation updates
- [ ] User guide updates

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Context not updating | Check signal connections |
| Slow rendering | Profile DrawingContext |
| Memory leak | Verify smart pointer usage |
| Symbols not indexed | Call rebuildIndices() |
| Relationships not showing | Check relationship registration |

---

## Next Steps

1. Implement custom context types for your domain
2. Add more specialized visualization panels
3. Integrate with debugger for breakpoint tracking
4. Connect with build system for compilation context
5. Add version control integration for branch/commit tracking

---

## Support

For integration help, refer to:
- Source code comments in header files
- Example usage in this guide
- Unit test implementations
- Main documentation files
