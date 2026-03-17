#ifndef RAWRXD_DOCK_MANAGER_H
#define RAWRXD_DOCK_MANAGER_H

#include <windows.h>
#include <stdint.h>

extern "C" {

// ============================================================================
// 1. DOCK NODE KINDS & SPLIT AXIS
// ============================================================================
enum DOCK_NODE_KIND {
    DNK_INVALID  = 0,
    DNK_SPLIT    = 1,
    DNK_TABS     = 2,
    DNK_LEAF     = 3,
    DNK_AUTOHIDE = 4,
    DNK_ROOT     = 5
};

enum SPLIT_AXIS {
    AXIS_NONE = 0,
    AXIS_HORZ = 1,
    AXIS_VERT = 2
};

// ============================================================================
// 2. PANEL IDENTITIES
// ============================================================================
enum PANEL_KIND {
    PK_EDITOR          = 0,
    PK_TERMINAL        = 1,
    PK_SYMBOL_TREE     = 2,
    PK_MODULE_TREE     = 3,
    PK_TENSOR_MAP      = 4,
    PK_DISASM          = 5,
    PK_MEMORY          = 6,
    PK_REGISTERS       = 7,
    PK_BREAKPOINTS     = 8,
    PK_STACK           = 9,
    PK_THREADS         = 10,
    PK_AGENT_TELEMETRY = 11,
    PK_GPU_TELEMETRY   = 12,
    PK_NVME_TELEMETRY  = 13,
    PK_LOG_STREAM      = 14,
    PK_GRAPH           = 15
};

enum SHELL_ZONE {
    ZONE_LEFT_RAIL       = 0,
    ZONE_CENTER_WORKSPACE= 1,
    ZONE_RIGHT_INSPECTOR = 2,
    ZONE_BOTTOM_RUNTIME  = 3,
    ZONE_OVERLAY_LAYER   = 4,
    ZONE_MODAL_FLOAT     = 5
};

// ============================================================================
// 3. CORE STRUCTS
// ============================================================================

struct PANEL_DESC {
    uint32_t panelId;
    uint32_t kind;          // PANEL_KIND
    uint32_t flags;

    wchar_t title[64];
    wchar_t persistKey[64];

    HWND hwnd;              // 0 if virtual-only (like a drawn graph vs Real EDIT control)
    BOOL visible;
    BOOL detached;
    BOOL wantsFocus;
    BOOL needsOwnScroll;
    BOOL selectionDriven;
    BOOL timeDriven;

    uint32_t preferredZone; // SHELL_ZONE
    int preferredW;
    int preferredH;
    int minW;
    int minH;
};

struct DOCK_NODE {
    uint32_t id;
    uint32_t kind;          // DOCK_NODE_KIND
    uint32_t flags;

    struct DOCK_NODE* parent;
    struct DOCK_NODE* firstChild;
    struct DOCK_NODE* nextSibling;

    RECT rcBounds;
    RECT rcContent;
    RECT rcHeader;
    RECT rcTabStrip;
    RECT rcSplitter;

    int minW;
    int minH;
    int maxW;
    int maxH;

    union {
        struct {
            uint32_t axis;          // SPLIT_AXIS
            int splitPos;           // pixels or logical units
            int splitSize;          // thickness of divider
            BOOL splitPosIsRatio;
            int splitRatioQ16;      // optional persistent ratio
        } split;

        struct {
            uint32_t activeTabIndex;
            uint32_t tabCount;
            uint32_t tabPanels[8];
        } tabs;

        struct {
            uint32_t panelId;
        } leaf;
    } u;
};

// ============================================================================
// 4. INTERACTION STATES
// ============================================================================
struct SPLITTER_DRAG_STATE {
    BOOL active;
    uint32_t nodeId;
    uint32_t axis;
    int anchorMouseX;
    int anchorMouseY;
    int originalSplitPos;
    RECT rcTrack;
};

struct DOCK_DRAG_STATE {
    BOOL active;
    uint32_t draggedPanelId;
    uint32_t sourceNodeId;
    uint32_t targetNodeId;
    uint32_t targetZone;
    RECT rcPreview;
    POINT ptScreen;
};

struct UI_STATE {
    HWND hwndMain;
    HWND hwndDockHost;

    RECT rcClient;
    RECT rcDockHost;

    UINT dpi;
    BOOL layoutDirty;
    BOOL isDraggingSplitter;
    BOOL captureSplitter;
    BOOL captureDockDrag;

    POINT ptMouseLast;
    POINT ptDragStart;

    int splitterHotId;
    int splitterActiveId;
    int nodeHotId;
    int nodeDropTargetId;

    struct DOCK_NODE* pShellRoot;
    
    // Simplistic array-based registries for phase 1
    struct PANEL_DESC panels[64];
    uint32_t panelCount;

    struct SPLITTER_DRAG_STATE splitDrag;
    struct DOCK_DRAG_STATE dockDrag;
};

// API Surface
__declspec(dllexport) UI_STATE* DockManager_Init(HWND hDockHost);
__declspec(dllexport) void DockManager_LayoutPass(UI_STATE* state, RECT* clientRect);
__declspec(dllexport) void DockManager_VirtualPaintChrome(UI_STATE* state, HDC hdc);
__declspec(dllexport) void DockManager_ApplyGeometry(UI_STATE* state);
__declspec(dllexport) void DockManager_OnLButtonDown(UI_STATE* state, int x, int y);
__declspec(dllexport) void DockManager_OnMouseMove(UI_STATE* state, int x, int y);

} // extern "C"

#endif // RAWRXD_DOCK_MANAGER_H