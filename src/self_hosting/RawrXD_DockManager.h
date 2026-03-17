#pragma once
#include <windows.h>
#include <stdint.h>

extern "C" {          
    void Dock_Initialize(HINSTANCE hInst);
    HWND Dock_GetHostWindow(HWND hwndParent);
    void Dock_UpdateSize(HWND hwndDockHost, int right, int bottom);
    void Dock_RegisterPanels(HWND hwndExplorer, HWND hwndEditor, HWND hwndChat);
}

struct PANEL_DESC {
    uint32_t panelId;
    HWND hwnd;                  
};

struct DOCK_NODE {
    uint32_t id;
};

struct SPLITTER_DRAG_STATE {
    BOOL active;
    uint32_t nodeId;
    uint32_t axis;
    int anchorMouse;
    int originalSplitPos;
    RECT rcTrack;
};

struct DOCK_DRAG_STATE {
    BOOL active;
};

struct UI_STATE {
    HWND hwndMain;
    HWND hwndDockHost;
    RECT rcClient;
    RECT rcDockHost;
    UINT dpi;
    BOOL layoutDirty;
    POINT ptMouseLast;
    int splitterHotId;
    int nodeHotId;
    int nodeDropTargetId;
    DOCK_NODE* pShellRoot;      
    PANEL_DESC* pPanels;        
    SPLITTER_DRAG_STATE splitterDrag;
    DOCK_DRAG_STATE dockDrag;
};

