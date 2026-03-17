#ifndef RAWRXD_DOCK_MANAGER_H
#define RAWRXD_DOCK_MANAGER_H

#include <windows.h>
#include <stdint.h>

/* --- Docking Manager Blueprints (Sovereign IDE Architecture) --- */

typedef enum {
    DNK_ROOT = 0,
    DNK_SPLIT,
    DNK_TABS,
    DNK_LEAF,
    DNK_AUTOHIDE
} DOCK_NODE_KIND;

typedef enum {
    AXIS_NONE = 0,
    AXIS_HORZ,
    AXIS_VERT
} SPLIT_AXIS;

typedef enum {
    PK_EDITOR,
    PK_TERMINAL,
    PK_SYMBOL_TREE,
    PK_MODULE_TREE,
    PK_TENSOR_MAP,
    PK_DISASM,
    PK_MEMORY,
    PK_REGISTERS,
    PK_BREAKPOINTS,
    PK_STACK,
    PK_THREADS,
    PK_AGENT_TELEMETRY,
    PK_GPU_TELEMETRY,
    PK_NVME_TELEMETRY,
    PK_LOG_STREAM,
    PK_GRAPH
} PANEL_KIND;

typedef struct DOCK_NODE {
    uint32_t id;
    uint32_t kind;  // DOCK_NODE_KIND
    uint32_t flags;

    struct DOCK_NODE* parent;
    struct DOCK_NODE* firstChild;
    struct DOCK_NODE* nextSibling;

    RECT rcBounds;   // Total area including chrome
    RECT rcContent;  // Area for child HWND or virtual content
    RECT rcTabStrip; // Area for tab rendering (if DNK_TABS)
    RECT rcSplitter; // Area for splitter hit-testing (if DNK_SPLIT)

    int minW, minH;
    int maxW, maxH;

    union {
        struct {
            uint32_t axis;      // SPLIT_AXIS
            int splitPos;       // pixels or logical units
            int splitSize;      // thickness (e.g., 4px)
            BOOL splitPosIsRatio;
            int splitRatioQ16;  // Persisted ratio (fixed point)
        } split;

        struct {
            uint32_t activeTabIndex;
            uint32_t tabCount;
        } tabs;

        struct {
            uint32_t panelId;
        } leaf;
    } u;
} DOCK_NODE;

typedef struct {
    uint32_t panelId;
    uint32_t kind;      // PANEL_KIND
    uint32_t flags;

    wchar_t title[64];
    HWND hwnd;          // Real HWND if heavy view, 0 if virtual
    BOOL visible;
    BOOL detached;

    uint32_t preferredZone; // 0=Left, 1=Center, 2=Right, 3=Bottom
    int minW, minH;
    int maxW, maxH;
} PANEL_DESC;

typedef struct {
    HWND hwndMain;
    HWND hwndDockHost;

    RECT rcClient;
    RECT rcDockHost;

    UINT dpi;
    BOOL layoutDirty;
    BOOL dragActive;
    BOOL captureSplitter;
    BOOL captureDockDrag;

    POINT ptMouseLast;
    POINT ptDragStart;

    int splitterHotId;
    int splitterActiveId;
    int nodeHotId;
    int nodeDropTargetId;

    struct DOCK_NODE* pShellRoot;
} UI_STATE;

typedef struct {
    BOOL active;
    uint32_t nodeId;
    uint32_t axis;
    int anchorMouse;
    int originalSplitPos;
    RECT rcTrack;
} SPLITTER_DRAG_STATE;

// External UI Functions (from RawrXD_Native_UI.dll)
#ifdef __cplusplus
extern "C" {
#endif
    __declspec(dllexport) HWND CreateRawrXDTriPane(HWND hParent);
    __declspec(dllexport) void ResizeRawrXDLanes(HWND hParent);
    __declspec(dllexport) void SwitchWorkspaceDomain(int domainId);
    __declspec(dllexport) void AppendTerminalText(const char* text);
    __declspec(dllexport) void RunNativeMessageLoop();
    __declspec(dllexport) void PopulateExplorer(const char* path);
    __declspec(dllexport) void HandleExplorerNotify(LPARAM lParam);
    
    // Layout Engine Prototypes
    void RawrMeasureNode(DOCK_NODE* node);
    void RawrArrangeNode(DOCK_NODE* node, RECT targetRect);
    void RawrApplyLayout(UI_STATE* state);
#ifdef __cplusplus
}
#endif

#endif // RAWRXD_DOCK_MANAGER_H
