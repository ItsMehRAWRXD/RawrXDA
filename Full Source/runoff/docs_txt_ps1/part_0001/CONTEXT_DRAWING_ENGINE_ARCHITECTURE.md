# Architecture Overview - Breadcrumb Context & Drawing System

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RawrXD IDE Main Window                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                               │
│  ┌─────────────────────────┐    ┌──────────────────────────────────────┐  │
│  │   Multi-Tab Editor      │    │   Context Dock Widget (New)          │  │
│  ├─────────────────────────┤    ├──────────────────────────────────────┤  │
│  │ • file1.cpp             │    │  ┌─ Breadcrumb Trail ───────────────┐│  │
│  │ • file2.h           [x] │    │  │ [Desktop] > [Project] > [File]   ││  │
│  │                         │    │  └───────────────────────────────────┘│  │
│  │ [Cursor: Line 42]       │    │  ┌─ Context Tabs ────────────────────┐│  │
│  │ [Selection: 5 chars]    │    │  │ [Breadcrumbs] [Context] [Graph]   ││  │
│  │                         │    │  └───────────────────────────────────┘│  │
│  └─────────────────────────┘    │  ┌─ Current Context ──────────────────┐│  │
│          │                       │  │ File: main.cpp                     ││  │
│          │ (Signals)             │  │ Size: 2.3 KB                       ││  │
│          │                       │  │ Modified: 2025-12-17 10:30        ││  │
│          └───────────────────────┼──┤ Symbols: 5                         ││  │
│                                  │  │ Dependencies: 3                    ││  │
│  ┌─────────────────────────┐    │  └───────────────────────────────────┘│  │
│  │  Task Orchestrator      │    │  ┌─ Dependency Graph ────────────────┐│  │
│  ├─────────────────────────┤    │  │                                    ││  │
│  │ • Task 1                │    │  │    main.cpp                        ││  │
│  │ • Task 2                │    │  │       ├─ stdio.h                  ││  │
│  │ • Task 3            [►] │    │  │       └─ math.h                   ││  │
│  │                         │    │  │                                    ││  │
│  │ Status: Running...      │    │  └───────────────────────────────────┘│  │
│  └─────────────────────────┘    │                                         │  │
│          │                       └──────────────────────────────────────┘  │
│          │ (Signals)                                                        │
│          │                                                                  │
│  ┌───────▼─────────────────┐    ┌──────────────────────────────────────┐  │
│  │   Agentic IDE           │    │   Custom Drawing Engine              │  │
│  ├─────────────────────────┤    ├──────────────────────────────────────┤  │
│  │ • Agent 1               │    │ • Surface Buffer (Pixels)            │  │
│  │ • Agent 2               │    │ • Path Construction                  │  │
│  │ • Agent 3               │    │ • Transformations                    │  │
│  │                         │    │ • GUI Components                     │  │
│  │ Running: Agent 2        │    │ • Text Rendering                     │  │
│  └─────────────────────────┘    │ • Gradients & Fills                  │  │
│                                  └──────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       │ (Both feed data to)
                                       ▼
                    ┌──────────────────────────────────────┐
                    │  Breadcrumb Context Manager          │
                    ├──────────────────────────────────────┤
                    │  • Symbol Registry          [Hash]   │
                    │  • File Registry            [Hash]   │
                    │  • Tool Registry            [Hash]   │
                    │  • Relationships            [Graph]  │
                    │  • Breadcrumb Chain         [Stack]  │
                    │  • Workspace Index          [Tree]   │
                    └──────────────────────────────────────┘
                                       │
                                       │ (Data flows to)
                                       ▼
                    ┌──────────────────────────────────────┐
                    │  Context Visualizer                  │
                    ├──────────────────────────────────────┤
                    │  • BreadcrumbRenderer                │
                    │  • ContextPanelRenderer              │
                    │  • ContextGraphRenderer              │
                    │  • ScreenshotAnnotator               │
                    │  • FileBrowser                       │
                    └──────────────────────────────────────┘
                                       │
                                       │ (Renders to)
                                       ▼
                    ┌──────────────────────────────────────┐
                    │  DrawingContext (Canvas)             │
                    ├──────────────────────────────────────┤
                    │  • Rasterization Pipeline            │
                    │  • Clipping & Transforms             │
                    │  • Blending Modes                    │
                    │  • Final Buffer Output               │
                    └──────────────────────────────────────┘
```

---

## Component Interaction Diagram

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT SYSTEM - DATA FLOW                              │
└────────────────────────────────────────────────────────────────────────────┘

IDE Events                   Context Manager                 Visualization
─────────────────────────────────────────────────────────────────────────────

File Opened ────────────────►  registerFile() ──────────────► FileBrowser
                                                              Panel

Symbol Found ───────────────►  registerSymbol() ─────────────► Symbol Panel
                               registerRelationship()        Graph Renderer

Tool Executed ───────────────► registerTool() ──────────────► Tools Panel

Instruction ────────────────► registerInstruction() ────────► Instruction
Detected                                                      Panel

Build Event ────────────────► updateSourceControlContext() ─► SCStatus
                                                              Panel

Cursor Moved ───────────────► updateEditorState() ──────────► Breadcrumb
                             pushContextBreadcrumb()         Trail


┌────────────────────────────────────────────────────────────────────────────┐
│                   DRAWING SYSTEM - RENDER PIPELINE                         │
└────────────────────────────────────────────────────────────────────────────┘

Context Data ──►  Panel Renderer ──►  Path Construction ──►  Rasterization
                                                             │
                                                             ▼
GUI Events   ──►  Component Events ──► Component Render ──► Surface Blend
                                                             │
                                                             ▼
Custom Draw ──►  Canvas Render ──────► Custom Path Draw ──► Buffer Update
                                                             │
                                                             ▼
                                                    Display Buffer (Qt)


┌────────────────────────────────────────────────────────────────────────────┐
│                    MEMORY MANAGEMENT HIERARCHY                             │
└────────────────────────────────────────────────────────────────────────────┘

Main Heap
├── BreadcrumbContextManager
│   ├── Symbol Registry [QMap]
│   ├── File Registry [QMap]
│   ├── Tool Registry [QMap]
│   ├── Relationships [QMap]
│   └── Breadcrumb Chain [QVector]
│
├── DrawingEngine
│   ├── Surface
│   │   ├── Pixel Buffer [uint32_t*]
│   │   └── Width/Height
│   ├── Path
│   │   ├── Points [std::vector]
│   │   └── Commands [std::vector]
│   └── DrawingContext
│       ├── Transform Stack [QVector]
│       ├── Clip Stack [QVector]
│       └── Surface [unique_ptr]
│
└── Visualization
    ├── ContextWindow [Component]
    ├── Renderers
    │   ├── BreadcrumbRenderer
    │   ├── ContextPanelRenderer
    │   └── ContextGraphRenderer
    └── GUI Components [shared_ptr]
        ├── Button
        ├── Panel
        ├── Label
        └── Canvas
```

---

## Performance Characteristics Matrix

```
┌────────────────────┬──────────────┬────────────────────┬─────────────────┐
│ Operation          │ Complexity   │ Cache Strategy     │ Optimization    │
├────────────────────┼──────────────┼────────────────────┼─────────────────┤
│ Symbol Lookup      │ O(1)         │ Hash Table         │ Indexed Registry│
│ File Search        │ O(n)         │ LRU Cache          │ Workspace Index │
│ Relationship Query │ O(n)         │ Graph Cache        │ Lazy Build      │
│ Breadcrumb Jump    │ O(1)         │ Direct Access      │ N/A             │
│ Path Rasterization │ O(m)         │ Cached Paths       │ Path Pooling    │
│ Text Rendering     │ O(k)         │ Glyph Cache        │ Atlas Rendering │
│ Surface Composite  │ O(w×h)       │ Tile-based Cache   │ GPU Accel.      │
│ Dependency Graph   │ O(n+e)       │ Adjacency Matrix   │ Graph Compress. │
│ Workspace Index    │ O(n log n)   │ B-Tree             │ Incremental     │
└────────────────────┴──────────────┴────────────────────┴─────────────────┘
```

---

## Signal/Slot Connections

```
IDE Components                          Context Manager
────────────────────────────────────────────────────────
MultiTabEditor::fileOpened()     ───►  BreadcrumbContextManager::registerFile()
MultiTabEditor::fileClosed()     ───►  BreadcrumbContextManager::closeEditor()
MultiTabEditor::symbolFound()    ───►  BreadcrumbContextManager::registerSymbol()
MultiTabEditor::cursorMoved()    ───►  BreadcrumbContextManager::updateEditorState()

TaskOrchestrator::taskStarted()  ───►  BreadcrumbContextManager::registerTool()
TaskOrchestrator::taskCompleted()───►  BreadcrumbContextManager::updateSymbolUsage()

AgenticIDE::agentStarted()       ───►  BreadcrumbContextManager::pushContextBreadcrumb()
AgenticIDE::instructionGenerated()──►  BreadcrumbContextManager::registerInstruction()

Git Integration::statusChanged() ───►  BreadcrumbContextManager::updateSourceControlContext()
Build System::buildStarted()     ───►  BreadcrumbContextManager::scanRepositoryStatus()

Context Manager                         Visualization
────────────────────────────────────────────────────────
contextChanged()                 ───►  ContextWindow::render()
breadcrumbNavigated()            ───►  ContextWindow::navigateToBreadcrumb()
symbolRegistered()               ───►  SymbolPanel::refresh()
fileRegistered()                 ───►  FileBrowser::addFile()
sourceControlUpdated()           ───►  SourceControlPanel::refresh()
```

---

## Data Structure Relationships

```
┌─────────────────────┐
│   Breadcrumb Chain  │
├─────────────────────┤
│ [0] Desktop         │
│ [1] Project         │
│ [2] src             │────────────┐
│ [3] main.cpp        │            │
│ [4] main() ◄ Current │            │
└─────────────────────┘            │
         │                         │
         └──────────────┬──────────┘
                        │
                 Links to ▼
        ┌──────────────────────────┐
        │   Symbol Registry        │
        ├──────────────────────────┤
        │ "main()" ──────────────┐ │
        │   • Kind: Function     │ │
        │   • File: main.cpp     │ │
        │   • Line: 42           │ │
        │   • Refs: [3]          │ │
        │   • Called By: [2]     │ │
        └──────────────────────────┘
                  │
                  │ Has Relations
                  ▼
        ┌──────────────────────────┐
        │  Relationships           │
        ├──────────────────────────┤
        │ main() ──calls──► printf()│
        │ main() ──calls──► malloc()│
        │ main() ──includes──> stdio.h
        │ main() ──depends──> libc.so
        └──────────────────────────┘


        ┌──────────────────────────┐
        │   File Registry          │
        ├──────────────────────────┤
        │ "main.cpp"               │
        │   • Size: 2.3 KB         │
        │   • Modified: 2025-12-17 │
        │   • Symbols: [main]      │
        │   • Deps: [stdio.h]      │
        │   • Includes: 2          │
        └──────────────────────────┘
```

---

## Rendering Pipeline Stages

```
┌──────────────────────────────────────────────────────────────────────────┐
│ STAGE 1: CONTEXT PREPARATION                                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Input: Entity ID (e.g., "main.cpp")                                   │
│         │                                                                │
│         ▼                                                                │
│  Lookup in Context Manager:                                            │
│         │                                                                │
│         ├─► Symbol List                                                │
│         ├─► File Metadata                                              │
│         ├─► Relationships                                              │
│         └─► Instructions                                               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STAGE 2: LAYOUT CALCULATION                                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Input: Context Data, Canvas Bounds                                    │
│         │                                                                │
│         ▼                                                                │
│  Calculate Panel Layouts:                                              │
│         │                                                                │
│         ├─► Breadcrumb Area (top)                                      │
│         ├─► Symbol Panel (left)                                        │
│         ├─► Context Panel (center)                                     │
│         ├─► Graph Panel (right)                                        │
│         └─► Status Bar (bottom)                                        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STAGE 3: DRAWING COMMAND GENERATION                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  For each Panel:                                                       │
│         │                                                                │
│         ├─► Generate Paths                                             │
│         │   • Background rectangles                                    │
│         │   • Text paths                                              │
│         │   • Icon shapes                                             │
│         │                                                                │
│         ├─► Apply Styling                                              │
│         │   • Fill colors                                             │
│         │   • Stroke styles                                           │
│         │   • Gradients                                               │
│         │                                                                │
│         └─► Add Interactive Elements                                    │
│             • Button regions                                          │
│             • Hover states                                            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STAGE 4: RASTERIZATION                                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  For each Drawing Command:                                             │
│         │                                                                │
│         ├─► Apply Transform Stack                                       │
│         │   • Translation                                             │
│         │   • Rotation                                                │
│         │   • Scaling                                                 │
│         │                                                                │
│         ├─► Apply Clipping                                             │
│         │   • Check clip stack                                        │
│         │   • Discard outside pixels                                  │
│         │                                                                │
│         ├─► Fill/Stroke Path                                           │
│         │   • Bresenham's algorithm (lines)                           │
│         │   • Scan conversion (fills)                                 │
│         │   • Anti-aliasing (optional)                                │
│         │                                                                │
│         └─► Blend to Surface                                           │
│             • Alpha blending                                          │
│             • Color blending                                          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ STAGE 5: OUTPUT                                                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Final Pixel Buffer (uint32_t[w×h])                                    │
│         │                                                                │
│         ├─► Qt QImage Conversion                                        │
│         │                                                                │
│         ├─► Display in Widget                                           │
│         │                                                                │
│         └─► Save to File (Optional)                                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Class Hierarchy

```
Component (Base class for all UI elements)
├── Button
├── Panel
│   └── ContextWindow
│       ├── BreadcrumbRenderer
│       ├── ContextPanelRenderer
│       └── ContextGraphRenderer
├── TextBox
├── Label
├── Canvas
├── InstructionPanel
└── FileBrowser


Renderer (Base for all renderers)
├── BreadcrumbRenderer
├── ContextPanelRenderer
├── ContextGraphRenderer
└── ScreenshotAnnotator


Context (Base for context data)
├── Breadcrumb
├── SymbolContext
├── ToolContext
├── FileContext
├── SourceControlContext
├── ScreenshotAnnotation
├── InstructionBlock
├── RelationshipContext
└── OpenEditorContext


Container (Holds registries)
└── BreadcrumbContextManager
    ├── Symbol Registry [QMap]
    ├── File Registry [QMap]
    ├── Tool Registry [QMap]
    ├── Relationship Registry [QMap]
    ├── Breadcrumb Chain [BreadcrumbChain]
    └── Screenshot Registry [QMap]
```

---

## State Transitions

```
                    ┌─────────────────┐
                    │   Uninitialized │
                    └────────┬────────┘
                             │
                        initialize()
                             │
                             ▼
                    ┌─────────────────┐
         ┌──────────│    Indexing     │──────────┐
         │          └────────┬────────┘          │
         │                   │                   │
         │              indexing()               │
         │                   │                   │
         │                   ▼                   │
         │          ┌─────────────────┐          │
         │          │   Ready         │          │
         │          └────────┬────────┘          │
         │                   │                   │
    registerFile()  updateContext()        closeEditor()
         │                   │                   │
         ▼                   ▼                   ▼
    ┌──────────┐      ┌──────────┐      ┌──────────────┐
    │ Tracking │ ────►│Visualizing├────►│ Interacting  │
    │  Items   │      │  Output   │      │  with UI     │
    └──────────┘      └──────────┘      └──────────────┘
         │                   │                   │
         │              invalidate()             │
         │                   │                   │
         └───────────┬────────┴───────────┘────┘
                     │
                shutdown()
                     │
                     ▼
            ┌──────────────────┐
            │   Shutdown       │
            └──────────────────┘
```

---

## Deployment Architecture

```
┌────────────────────────────────────────────┐
│        User's Computer                     │
├────────────────────────────────────────────┤
│                                            │
│  RawrXD.exe                                │
│  ├── Qt Runtime                            │
│  ├── OpenGL/Vulkan (optional)              │
│  └── System Libraries                      │
│                                            │
│  Application Data                          │
│  ├── Configuration                         │
│  ├── Context Cache (JSON)                  │
│  ├── Recent Files Index                    │
│  └── User Settings                         │
│                                            │
└────────────────────────────────────────────┘
```

This comprehensive architecture ensures:
- ✅ Modularity and extensibility
- ✅ Clear separation of concerns
- ✅ Performance optimization
- ✅ Memory efficiency
- ✅ Scalability for large projects
