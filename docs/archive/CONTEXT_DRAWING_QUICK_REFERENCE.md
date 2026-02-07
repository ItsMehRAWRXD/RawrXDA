# Quick Reference - Breadcrumb Context & Drawing Engine

## Quick Start

### 1. Initialize System
```cpp
#include "include/context/BreadcrumbContextManager.h"
#include "include/drawing/DrawingEngine.h"
#include "include/visualization/ContextVisualizer.h"

using namespace RawrXD::Context;
using namespace RawrXD::Drawing;
using namespace RawrXD::Visualization;

BreadcrumbContextManager contextMgr;
contextMgr.initialize("/workspace/path");
```

### 2. Track Items
```cpp
// Track a file
contextMgr.registerFile("/path/to/main.cpp");

// Track a symbol
SymbolContext sym;
sym.name = "main";
sym.kind = SymbolKind::Function;
sym.filePath = "/path/to/main.cpp";
sym.lineNumber = 42;
contextMgr.registerSymbol(sym.filePath, sym);

// Track a tool
ToolContext tool;
tool.toolName = "g++";
tool.executablePath = "/usr/bin/g++";
tool.isAvailable = true;
contextMgr.registerTool("g++", tool);
```

### 3. Draw Graphics
```cpp
DrawingContext ctx(800, 600);
ctx.clear(Color(255, 255, 255));

// Draw rectangle
FillStyle fill;
fill.solidColor = Color(52, 152, 219);
ctx.drawRect(Rect(50, 50, 200, 100), fill);

// Draw text
ctx.drawText("Hello!", Point(100, 150), "Arial", 24, Color(0, 0, 0));

// Get buffer
const uint32_t* buffer = ctx.getBuffer();
```

### 4. Navigate with Breadcrumbs
```cpp
auto& chain = contextMgr.getBreadcrumbChain();
auto breadcrumbs = chain.getChain();  // Get all breadcrumbs
chain.pop();                           // Go back
chain.jump(0);                         // Jump to first
```

### 5. Visualize Everything
```cpp
ContextWindow window(contextMgr, Rect(0, 0, 1200, 800));
window.setShowBreadcrumbs(true);
window.setShowContextPanel(true);
window.setShowGraphView(true);

DrawingContext ctx(1200, 800);
window.render(ctx);
```

---

## API Cheat Sheet

### Context Manager

| Task | Method |
|------|--------|
| Initialize | `contextMgr.initialize(path)` |
| Add file | `contextMgr.registerFile(path)` |
| Add symbol | `contextMgr.registerSymbol(file, symbol)` |
| Add tool | `contextMgr.registerTool(name, context)` |
| Add instruction | `contextMgr.registerInstruction(instr)` |
| Add relationship | `contextMgr.registerRelationship(rel)` |
| Get file info | `contextMgr.getFileContext(path)` |
| Get symbol | `contextMgr.getSymbol(name)` |
| Get tool | `contextMgr.getTool(name)` |
| Find dependencies | `contextMgr.getDependencies(id)` |
| Find dependents | `contextMgr.getDependents(id)` |
| Get breadcrumbs | `contextMgr.getBreadcrumbChain()` |
| Navigate | `contextMgr.navigateToBreadcrumb(index)` |
| Export | `contextMgr.exportContextToJSON(path)` |
| Generate report | `contextMgr.generateContextReport()` |

### Drawing Context

| Shape | Method |
|-------|--------|
| Rectangle | `ctx.drawRect(rect, fill, stroke)` |
| Circle | `ctx.drawCircle(center, radius, fill, stroke)` |
| Ellipse | `ctx.drawEllipse(center, rx, ry, fill, stroke)` |
| Triangle | `ctx.drawTriangle(p1, p2, p3, fill, stroke)` |
| Polygon | `ctx.drawPolygon(vertices, fill, stroke)` |
| Line | `ctx.drawLine(p1, p2, stroke)` |
| Arc | `ctx.drawArc(center, radius, start, end, stroke)` |
| Text | `ctx.drawText(text, pos, font, size, color)` |
| Rounded Rect | `ctx.drawRoundedRect(rect, radius, fill, stroke)` |

### Transformations

```cpp
ctx.save();           // Save transform stack
ctx.translate(x, y);  // Move origin
ctx.rotate(angle);    // Rotate (radians)
ctx.scale(sx, sy);    // Scale
ctx.restore();        // Restore previous transform
```

### Paths

```cpp
Path p;
p.moveTo(Point(0, 0));
p.lineTo(Point(100, 0));
p.curveTo(cp1, cp2, end);      // Cubic bezier
p.quadraticCurveTo(cp, end);   // Quadratic bezier
p.arcTo(center, r, start, end);// Arc
p.closePath();
p.getBounds();                  // Get bounding box
p.isPointInside(p);             // Hit test
```

### GUI Components

```cpp
auto btn = std::make_shared<Button>(bounds, "Click");
btn->onClick = []() { /* ... */ };

auto panel = std::make_shared<Panel>(bounds);
panel->addChild(btn);

auto label = std::make_shared<Label>(bounds, "Text");
auto textBox = std::make_shared<TextBox>(bounds);
auto canvas = std::make_shared<Canvas>(bounds);

canvas->onCustomRender = [](DrawingContext& ctx) {
    // Custom drawing
};

panel->render(ctx);
```

### Colors & Styling

```cpp
Color c(255, 0, 0, 255);        // RGBA
Color blended = c.lerp(other, 0.5f);

FillStyle fill;
fill.solidColor = Color(200, 200, 200);
fill.opacity = 0.8f;

StrokeStyle stroke;
stroke.width = 2.0f;
stroke.color = Color(0, 0, 0);
stroke.cap = LineCap::Round;
stroke.join = LineJoin::Round;
stroke.miterLimit = 10.0f;
```

### Gradients

```cpp
std::vector<Color> colors = {Color(255, 0, 0), Color(0, 0, 255)};
std::vector<float> stops = {0.0f, 1.0f};

FillStyle gradient;
ctx.createLinearGradient(start, end, colors, stops, gradient);
// OR
ctx.createRadialGradient(center, radius, colors, stops, gradient);

ctx.drawRect(bounds, gradient);
```

### Visualization

```cpp
ContextWindow window(contextMgr, bounds);
window.addTab("breadcrumbs");
window.addTab("context");
window.addTab("graph");
window.selectTab("graph");
window.navigateToEntity("file.cpp");

BreadcrumbRenderer bcRenderer;
bcRenderer.renderBreadcrumbs(ctx, chain, bounds);

ContextGraphRenderer graphRenderer(contextMgr);
graphRenderer.renderDependencyGraph(ctx, "entity", bounds);

FileBrowser browser(contextMgr, bounds);
browser.navigateToFile("/path/to/file");
browser.expandFolder("/path/to/folder");

ScreenshotAnnotator annotator;
annotator.addAnnotation(annotation);
annotator.renderAnnotations(ctx, surface, bounds);
```

---

## Common Patterns

### Pattern: Track File Open
```cpp
OpenEditorContext editorCtx;
editorCtx.cursorLine = 42;
editorCtx.cursorColumn = 10;
contextMgr.registerOpenEditor("/path/to/file.cpp", editorCtx);

// Update cursor
contextMgr.updateEditorState("/path/to/file.cpp", 50, 15);
```

### Pattern: Display Dependency Graph
```cpp
auto deps = contextMgr.getDependencies("main.cpp");
for (const auto& dep : deps) {
    auto relationships = contextMgr.getRelationshipsFor("main.cpp");
    // Visualize relationships
}
```

### Pattern: Custom Drawing
```cpp
Canvas canvas(bounds);
canvas->onCustomRender = [&](DrawingContext& ctx) {
    Path p;
    p.circle(Point(100, 100), 50);
    FillStyle fill;
    fill.solidColor = Color(255, 0, 0);
    ctx.fill(p, fill);
};
canvas->render(ctx);
```

### Pattern: Interactive Buttons
```cpp
auto saveBtn = std::make_shared<Button>(Rect(10, 10, 100, 30), "Save");
auto cancelBtn = std::make_shared<Button>(Rect(120, 10, 100, 30), "Cancel");

saveBtn->onClick = [&](){ contextMgr.exportContextToJSON("context.json"); };
cancelBtn->onClick = [&](){ /* cancel */ };

// Handle mouse events
saveBtn->onMouseDown(mousePos);
saveBtn->onMouseUp(mousePos);
```

---

## File Reference

| File | Purpose |
|------|---------|
| `include/context/BreadcrumbContextManager.h` | Context manager header |
| `src/context/BreadcrumbContextManager.cpp` | Context manager implementation |
| `include/drawing/DrawingEngine.h` | Drawing engine header |
| `src/drawing/DrawingEngine.cpp` | Drawing engine implementation |
| `include/visualization/ContextVisualizer.h` | Visualization header |
| `src/visualization/ContextVisualizer.cpp` | Visualization implementation |

---

## Compilation Notes

Add to `CMakeLists.txt`:

```cmake
# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(SOURCES
    ${SOURCES}
    src/context/BreadcrumbContextManager.cpp
    src/drawing/DrawingEngine.cpp
    src/visualization/ContextVisualizer.cpp
)

# Link with Qt
find_package(Qt6 COMPONENTS Core Gui REQUIRED)
target_link_libraries(YourTarget Qt6::Core Qt6::Gui)
```

---

## Performance Tips

1. **Indexing**: Call `rebuildIndices()` periodically for large workspaces
2. **Rendering**: Use clipping to limit drawing area
3. **Paths**: Cache frequently-used paths
4. **Relationships**: Use graph caching for repeated queries
5. **Memory**: Export/import context to disk for large projects

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Symbols not found | Call `contextMgr.rebuildIndices()` |
| Slow rendering | Reduce clipping region or batch draws |
| Missing relationships | Verify relationship registration |
| Memory growth | Check for leaked Surface or Path objects |
| Rendering artifacts | Verify coordinate systems and transforms |

---

## Examples Repository

See `CONTEXT_DRAWING_ENGINE_GUIDE.md` for comprehensive examples.

---

## Support

For issues, questions, or feature requests, refer to:
- Main guide: `CONTEXT_DRAWING_ENGINE_GUIDE.md`
- Delivery summary: `CONTEXT_DRAWING_ENGINE_DELIVERY_SUMMARY.md`
- Source code documentation in headers
