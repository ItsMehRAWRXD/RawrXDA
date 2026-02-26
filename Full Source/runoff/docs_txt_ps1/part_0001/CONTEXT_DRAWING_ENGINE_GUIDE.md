# Breadcrumb Context System & Drawing Engine

## Overview

This comprehensive system provides:

1. **BreadcrumbContextManager** - A sophisticated context tracking system for:
   - Tools and executables
   - Code symbols (functions, classes, variables)
   - Files and file hierarchies
   - Source control information
   - Screenshots with annotations
   - Inline instructions/documentation
   - Entity relationships and dependencies
   - Open editor states

2. **DrawingEngine** - A complete custom drawing engine from scratch featuring:
   - Geometric primitives (lines, rectangles, circles, polygons, curves)
   - Path construction and manipulation
   - Text rendering with font support
   - Gradient fills and patterns
   - Transformations (translate, rotate, scale, matrix transforms)
   - Clipping regions and layer management
   - GUI component framework (Button, Panel, TextBox, Label, Canvas)
   - Surface composition and blending

3. **ContextVisualizer** - Integration layer for rendering context:
   - Breadcrumb trail navigation
   - Dependency and relationship graphs
   - File browsers and symbol trees
   - Instruction panels
   - Screenshot annotations
   - Context windows with tabbed interfaces

## Architecture

### Component Hierarchy

```
RawrXD::Context
├── BreadcrumbContextManager    (Context storage & management)
├── BreadcrumbChain             (Navigation history)
├── SymbolContext               (Code symbol tracking)
├── ToolContext                 (Tool/executable tracking)
├── FileContext                 (File metadata)
├── SourceControlContext        (Git/VCS info)
├── InstructionBlock            (Documentation)
└── RelationshipContext         (Entity dependencies)

RawrXD::Drawing
├── DrawingEngine               (Core rendering)
├── Surface                     (Pixel buffer)
├── Path                        (Vector paths)
├── Color & StrokeStyle         (Styling)
├── Component                   (GUI base)
├── Button, Panel, TextBox      (UI widgets)
└── Canvas                      (Custom rendering area)

RawrXD::Visualization
├── ContextWindow               (Main UI container)
├── BreadcrumbRenderer          (Trail rendering)
├── ContextPanelRenderer        (Info panels)
├── ContextGraphRenderer        (Dependency graphs)
├── ScreenshotAnnotator        (Visual annotations)
└── FileBrowser                (File hierarchy view)
```

## Usage Examples

### 1. Initialize and Register Context

```cpp
#include "include/context/BreadcrumbContextManager.h"
#include "include/drawing/DrawingEngine.h"

using namespace RawrXD::Context;
using namespace RawrXD::Drawing;

int main() {
    // Create context manager
    BreadcrumbContextManager contextMgr;
    contextMgr.initialize("/path/to/workspace");
    
    // Register a tool
    ToolContext toolCtx;
    toolCtx.toolName = "gcc";
    toolCtx.executablePath = "/usr/bin/gcc";
    toolCtx.version = "11.2.0";
    toolCtx.isAvailable = true;
    contextMgr.registerTool("gcc", toolCtx);
    
    // Register a file
    contextMgr.registerFile("/path/to/file.cpp");
    
    // Register a symbol
    SymbolContext symCtx;
    symCtx.name = "MyFunction";
    symCtx.kind = SymbolKind::Function;
    symCtx.filePath = "/path/to/file.cpp";
    symCtx.lineNumber = 42;
    symCtx.signature = "void MyFunction(int x, const QString& str)";
    contextMgr.registerSymbol("/path/to/file.cpp", symCtx);
    
    return 0;
}
```

### 2. Navigation with Breadcrumbs

```cpp
// Push breadcrumbs as user navigates
contextMgr.pushContextBreadcrumb(ContextType::File, "/path/to/file.cpp");
contextMgr.pushContextBreadcrumb(ContextType::Symbol, "MyFunction");

// Get breadcrumb chain
auto& chain = contextMgr.getBreadcrumbChain();
auto trail = chain.getChain();

// Navigate backward
contextMgr.navigateToBreadcrumb(0);  // Jump to first breadcrumb

// Query current context
auto currentBreadcrumb = chain.getCurrentBreadcrumb();
qDebug() << "Currently at:" << currentBreadcrumb.displayName;
```

### 3. Drawing Graphics

```cpp
#include "include/drawing/DrawingEngine.h"

// Create drawing context
DrawingContext ctx(800, 600);

// Clear canvas
ctx.clear(Color(255, 255, 255, 255));  // White background

// Draw shapes
// Rectangle
FillStyle fillStyle;
fillStyle.solidColor = Color(52, 152, 219, 255);  // Blue
StrokeStyle strokeStyle;
strokeStyle.width = 2.0f;
strokeStyle.color = Color(0, 0, 0, 255);  // Black border
ctx.drawRect(Rect(50, 50, 200, 100), fillStyle, strokeStyle);

// Circle
ctx.drawCircle(Point(300, 150), 50.0f, fillStyle, strokeStyle);

// Text
ctx.drawText("Hello Drawing Engine!", Point(100, 300), "Arial", 
             24.0f, Color(0, 0, 0, 255));

// Line
ctx.drawLine(Point(50, 400), Point(400, 400), strokeStyle);

// Rounded rectangle
ctx.drawRoundedRect(Rect(100, 450, 150, 80), 10.0f, fillStyle, strokeStyle);

// Triangle
std::vector<Point> trianglePoints = {
    Point(450, 50), Point(500, 150), Point(400, 150)
};
ctx.drawPolygon(trianglePoints, fillStyle, strokeStyle);

// Get the rendered buffer
const uint32_t* buffer = ctx.getBuffer();
// ... save or display buffer ...
```

### 4. Create Custom Paths

```cpp
// Create a custom path
Path customPath;
customPath.moveTo(Point(50, 50));
customPath.lineTo(Point(200, 50));
customPath.curveTo(Point(300, 50), Point(300, 150), Point(200, 150));
customPath.lineTo(Point(50, 150));
customPath.closePath();

FillStyle fill;
fill.solidColor = Color(150, 200, 255, 200);  // Semi-transparent blue

StrokeStyle stroke;
stroke.width = 3.0f;
stroke.color = Color(50, 100, 150, 255);

ctx.fillAndStroke(customPath, fill, stroke);

// Transform the path
Path scaledPath = customPath.scaled(1.5f, 1.5f);
Path rotatedPath = customPath.rotated(45 * M_PI / 180, Point(400, 400));
Path offsetPath = customPath.offset(Point(100, 100));
```

### 5. Use Gradients

```cpp
// Create linear gradient
std::vector<Color> colors = {
    Color(255, 0, 0, 255),      // Red
    Color(0, 255, 0, 255),      // Green
    Color(0, 0, 255, 255)       // Blue
};
std::vector<float> stops = {0.0f, 0.5f, 1.0f};

FillStyle gradientFill;
ctx.createLinearGradient(Point(50, 50), Point(300, 300), colors, stops, gradientFill);
ctx.drawRect(Rect(50, 50, 250, 250), gradientFill);

// Create radial gradient
FillStyle radialFill;
ctx.createRadialGradient(Point(400, 200), 100.0f, colors, stops, radialFill);
ctx.drawCircle(Point(400, 200), 100.0f, radialFill);
```

### 6. GUI Components

```cpp
#include "include/drawing/DrawingEngine.h"

// Create components
auto button = std::make_shared<Button>(Rect(50, 50, 120, 40), "Click Me");
auto panel = std::make_shared<Panel>(Rect(0, 0, 800, 600));
auto label = std::make_shared<Label>(Rect(50, 100, 200, 30), "Hello World");
auto textBox = std::make_shared<TextBox>(Rect(50, 150, 300, 30));

// Set button callback
button->onClick = []() {
    qDebug() << "Button clicked!";
};

// Add components to panel
panel->addChild(button);
panel->addChild(label);
panel->addChild(textBox);

// Render
DrawingContext ctx(800, 600);
panel->render(ctx);

// Handle events
Point mousePos(100, 60);
button->onMouseDown(mousePos);
button->onMouseUp(mousePos);
```

### 7. Visualization with Context

```cpp
#include "include/visualization/ContextVisualizer.h"

using namespace RawrXD::Visualization;

// Create visualization window
ContextWindow contextWindow(contextMgr, Rect(0, 0, 1200, 800));

// Configure what to show
contextWindow.setShowBreadcrumbs(true);
contextWindow.setShowContextPanel(true);
contextWindow.setShowGraphView(true);

// Add tabs
contextWindow.addTab("breadcrumbs");
contextWindow.addTab("context");
contextWindow.addTab("graph");

// Navigate to entity
contextWindow.navigateToEntity("MyFunction");

// Select tab
contextWindow.selectTab("graph");

// Render everything
DrawingContext ctx(1200, 800);
contextWindow.render(ctx);
```

### 8. Track File Relationships

```cpp
// Register relationship
RelationshipContext rel;
rel.sourceId = "file1.cpp";
rel.targetId = "file2.cpp";
rel.relationshipType = "includes";
rel.description = "file1.cpp includes file2.cpp";
rel.strength = "strong";
contextMgr.registerRelationship(rel);

// Query relationships
auto rels = contextMgr.getRelationshipsFor("file1.cpp");
auto deps = contextMgr.getDependencies("file1.cpp");
auto dependents = contextMgr.getDependents("file1.cpp");

// Visualize dependency graph
ContextGraphRenderer graphRenderer(contextMgr);
Rect graphBounds(0, 0, 600, 600);
graphRenderer.renderDependencyGraph(ctx, "file1.cpp", graphBounds);
```

### 9. Add Instructions/Documentation

```cpp
// Create instruction block
InstructionBlock instruction;
instruction.id = "instr_001";
instruction.title = "How to Build";
instruction.content = "Run: cmake --build . --config Release";
instruction.tags = {"build", "cmake", "release"};
instruction.relatedFile = "/path/to/CMakeLists.txt";
instruction.lineNumber = 1;
instruction.backgroundColor = "#FFF8DC";  // Cornsilk
instruction.borderColor = "#FF8C00";      // DarkOrange

contextMgr.registerInstruction(instruction);

// Render instruction panel
InstructionPanel instrPanel(Rect(0, 0, 400, 150));
instrPanel.setInstruction(instruction);

DrawingContext ctx(400, 150);
instrPanel.render(ctx);
```

### 10. Generate Context Reports

```cpp
// Export context to JSON
contextMgr.exportContextToJSON("/tmp/context_export.json");

// Generate report
auto report = contextMgr.generateContextReport();
qDebug() << "Context Report:";
qDebug() << "  Tools:" << report["tools"];
qDebug() << "  Symbols:" << report["symbols"];
qDebug() << "  Files:" << report["files"];
qDebug() << "  Relationships:" << report["relationships"];
qDebug() << "  Instructions:" << report["instructions"];

// Analyze relationships
auto analysis = contextMgr.analyzeContextRelationships();
qDebug() << "Total relationships:" << analysis["totalRelationships"];

// Get complete context
auto completeCtx = contextMgr.getCompleteContext("MyFunction");
qDebug() << "Complete context:" << completeCtx;
```

## Key Features

### BreadcrumbContextManager

- **Thread-safe** context storage with efficient lookups
- **Index-based** symbol and file tracking for fast searches
- **Relationship graphs** for dependency analysis
- **Breadcrumb navigation** with history
- **Workspace indexing** with progress tracking
- **Import/Export** to JSON for persistence
- **Performance metrics** and caching

### DrawingEngine

- **Low-level rasterization** with no external dependencies (except Qt)
- **Vector paths** with Bezier curves and arc support
- **Transformations** (translate, rotate, scale, skew)
- **Clipping regions** with depth management
- **Gradient fills** (linear and radial)
- **Text rendering** with font metrics
- **Surface composition** and blending modes
- **GUI components** framework for building UIs

### ContextVisualizer

- **Breadcrumb renderer** with customizable styling
- **Context panels** for multiple information types
- **Dependency graphs** with automatic layout
- **File browser** with expand/collapse
- **Screenshot annotations** with highlighting
- **Tabbed interface** for organizing views
- **Interactive components** with mouse support

## Performance Characteristics

- **Indexing**: O(1) lookups for symbols and files
- **Rendering**: Optimal rasterization with minimal memory overhead
- **Relationships**: O(n) graph traversal with caching
- **Navigation**: O(1) breadcrumb jumping

## Thread Safety

- Context manager uses atomic operations for thread-safe updates
- Drawing engine is single-threaded but composable across threads
- All public APIs maintain consistency guarantees

## Memory Management

- Smart pointer usage throughout
- RAII principles for resource management
- Automatic cleanup on destruction
- Configurable memory limits for context caching

## Extensibility

- Custom component types via `Component` base class
- Plugin system for additional context types
- Customizable renderers via override points
- Flexible relationship types and metadata

## Future Enhancements

- GPU-accelerated rasterization with Vulkan/DirectX
- Real-time collaboration support
- Advanced text layout and typography
- Animation framework
- Accessibility features (screen readers)
- Undo/redo for context modifications
