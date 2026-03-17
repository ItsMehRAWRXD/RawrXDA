/**
 * @file RawrXD_DockManager.cpp
 * @brief Dynamic Sovereign Docking Manager implementation.
 * Maps logic bounds over pure mathematical spatial splits.
 */

#include "RawrXD_DockManager.h"
#include <iostream>

extern "C" {
    extern HWND hExplorer;
    extern HWND hEditor;
    extern HWND hChat;
    extern HWND hRuntime;
}

static UI_STATE g_UiState = {0};
static DOCK_NODE g_Nodes[7];

extern "C" {

UI_STATE* DockManager_Init(HWND hDockHost) {
    g_UiState.hwndDockHost = hDockHost;
    g_UiState.layoutDirty = TRUE;
    g_UiState.panelCount = 0;
    
    // Node 0: Root (VSPLIT)
    g_Nodes[0].id = 1;
    g_Nodes[0].kind = DNK_SPLIT;
    g_Nodes[0].u.split.axis = AXIS_VERT;
    g_Nodes[0].u.split.splitPos = 250;
    g_Nodes[0].parent = NULL;
    
    // Node 1: Left (ZONE_LEFT_RAIL)
    g_Nodes[1].id = 2;
    g_Nodes[1].kind = DNK_LEAF;
    g_Nodes[1].u.leaf.panelId = ZONE_LEFT_RAIL;
    g_Nodes[1].parent = &g_Nodes[0];
    g_Nodes[1].nextSibling = &g_Nodes[2];
    g_Nodes[1].firstChild = NULL;
    
    // Node 2: Right child of 0 (HSPLIT)
    g_Nodes[2].id = 3;
    g_Nodes[2].kind = DNK_SPLIT;
    g_Nodes[2].u.split.axis = AXIS_HORZ;
    g_Nodes[2].u.split.splitPos = 600;
    g_Nodes[2].parent = &g_Nodes[0];
    g_Nodes[2].nextSibling = NULL;
    
    g_Nodes[0].firstChild = &g_Nodes[1];
    
    // Node 3: Top child of 2 (VSPLIT)
    g_Nodes[3].id = 4;
    g_Nodes[3].kind = DNK_SPLIT;
    g_Nodes[3].u.split.axis = AXIS_VERT;
    g_Nodes[3].u.split.splitPos = 700;
    g_Nodes[3].parent = &g_Nodes[2];
    g_Nodes[3].nextSibling = &g_Nodes[4];
    
    // Node 4: Bottom child of 2 (TABS)
    g_Nodes[4].id = 5;
    g_Nodes[4].kind = DNK_TABS;
    g_Nodes[4].u.tabs.tabCount = 1;
    g_Nodes[4].u.tabs.tabPanels[0] = ZONE_BOTTOM_RUNTIME;
    g_Nodes[4].u.tabs.activeTabIndex = 0;
    g_Nodes[4].parent = &g_Nodes[2];
    g_Nodes[4].nextSibling = NULL;
    g_Nodes[4].firstChild = NULL;
    
    g_Nodes[2].firstChild = &g_Nodes[3];
    
    // Node 5: Left child of 3 (ZONE_CENTER_WORKSPACE)
    g_Nodes[5].id = 6;
    g_Nodes[5].kind = DNK_LEAF;
    g_Nodes[5].u.leaf.panelId = ZONE_CENTER_WORKSPACE;
    g_Nodes[5].parent = &g_Nodes[3];
    g_Nodes[5].nextSibling = &g_Nodes[6];
    g_Nodes[5].firstChild = NULL;
    
    // Node 6: Right child of 3 (ZONE_RIGHT_INSPECTOR)
    g_Nodes[6].id = 7;
    g_Nodes[6].kind = DNK_LEAF;
    g_Nodes[6].u.leaf.panelId = ZONE_RIGHT_INSPECTOR;
    g_Nodes[6].parent = &g_Nodes[3];
    g_Nodes[6].nextSibling = NULL;
    g_Nodes[6].firstChild = NULL;
    
    g_Nodes[3].firstChild = &g_Nodes[5];
    
    g_UiState.pShellRoot = &g_Nodes[0];
    
    return &g_UiState;
}

static void LayoutRecursive(DOCK_NODE* node, RECT rc) {
    node->rcBounds = rc;
    node->rcContent = rc;

    if (node->kind == DNK_TABS) {
        node->rcTabStrip = rc;
        node->rcTabStrip.bottom = node->rcTabStrip.top + 28;
        
        node->rcContent = rc;
        node->rcContent.top += 28;
    } else if (node->kind == DNK_SPLIT) {
        if (!node->firstChild || !node->firstChild->nextSibling) return;
        
        DOCK_NODE* leftChild = node->firstChild;
        DOCK_NODE* rightChild = leftChild->nextSibling;
        
        RECT rcLeft = rc;
        RECT rcRight = rc;
        
        if (node->u.split.axis == AXIS_VERT) {
            rcLeft.right = rcLeft.left + node->u.split.splitPos;
            rcRight.left = rcLeft.right;
        } else {
            rcLeft.bottom = rcLeft.top + node->u.split.splitPos;
            rcRight.top = rcLeft.bottom;
        }
        
        LayoutRecursive(leftChild, rcLeft);
        LayoutRecursive(rightChild, rcRight);
    }
}

void DockManager_LayoutPass(UI_STATE* state, RECT* clientRect) {
    if (!state || !state->pShellRoot || !clientRect) return;
    state->rcDockHost = *clientRect;
    LayoutRecursive(state->pShellRoot, *clientRect);
static void VirtualPaintChromeRecursive(DOCK_NODE* node, HDC hdc) {
    if (!node) return;
    
    if (node->kind == DNK_SPLIT) {
        RECT splitRect = {0};
        if (node->u.split.axis == AXIS_VERT) {
            splitRect.left = node->rcBounds.left + node->u.split.splitPos;
            splitRect.right = splitRect.left + 5;
            splitRect.top = node->rcBounds.top;
            splitRect.bottom = node->rcBounds.bottom;
        } else if (node->u.split.axis == AXIS_HORZ) {
            splitRect.top = node->rcBounds.top + node->u.split.splitPos;
            splitRect.bottom = splitRect.top + 5;
            splitRect.left = node->rcBounds.left;
            splitRect.right = node->rcBounds.right;
        }
        
        HBRUSH brush = CreateSolidBrush(RGB(45,45,45));
        FillRect(hdc, &splitRect, brush);
        DeleteObject(brush);
        
        node->rcSplitter = splitRect;
        
        VirtualPaintChromeRecursive(node->firstChild, hdc);
        if (node->firstChild) {
            VirtualPaintChromeRecursive(node->firstChild->nextSibling, hdc);
        }
    } else if (node->kind == DNK_TABS) {
        HBRUSH bgBrush = CreateSolidBrush(RGB(30,30,30));
        FillRect(hdc, &node->rcTabStrip, bgBrush);
        DeleteObject(bgBrush);

        HBRUSH tabBrush = CreateSolidBrush(RGB(60,60,60));
        int tabX = node->rcTabStrip.left + 5;
        for (uint32_t i = 0; i < node->u.tabs.tabCount; i++) {
            RECT tabRect = {tabX, node->rcTabStrip.top + 5, tabX + 100, node->rcTabStrip.bottom - 2};
            FillRect(hdc, &tabRect, tabBrush);
            tabX += 105;
        }
        DeleteObject(tabBrush);

        VirtualPaintChromeRecursive(node->firstChild, hdc);
        if (node->firstChild) {
            VirtualPaintChromeRecursive(node->firstChild->nextSibling, hdc);
        }
    } else {
        VirtualPaintChromeRecursive(node->firstChild, hdc);
        if (node->firstChild) {
            VirtualPaintChromeRecursive(node->firstChild->nextSibling, hdc);
        }
    }
}

void DockManager_VirtualPaintChrome(UI_STATE* state, HDC hdc) {
    if (!state || !state->pShellRoot) return;
    VirtualPaintChromeRecursive(state->pShellRoot, hdc);
}

static void ApplyGeometryRecursive(DOCK_NODE* node, HDWP& hdwp) {
    if (node->kind == DNK_LEAF) {
        HWND hTarget = NULL;
        if (node->u.leaf.panelId == ZONE_LEFT_RAIL) hTarget = hExplorer;
        else if (node->u.leaf.panelId == ZONE_CENTER_WORKSPACE) hTarget = hEditor;
        else if (node->u.leaf.panelId == ZONE_RIGHT_INSPECTOR) hTarget = hChat;
        else if (node->u.leaf.panelId == ZONE_BOTTOM_RUNTIME) hTarget = hRuntime;
        
        if (hTarget) {
            hdwp = DeferWindowPos(hdwp, hTarget, NULL,
                node->rcContent.left, node->rcContent.top,
                node->rcContent.right - node->rcContent.left,
                node->rcContent.bottom - node->rcContent.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
    } else if (node->kind == DNK_TABS) {
        if (node->u.tabs.tabCount > 0) {
            uint32_t activePanel = node->u.tabs.tabPanels[node->u.tabs.activeTabIndex];
            HWND hTarget = NULL;
            if (activePanel == ZONE_LEFT_RAIL) hTarget = hExplorer;
            else if (activePanel == ZONE_CENTER_WORKSPACE) hTarget = hEditor;
            else if (activePanel == ZONE_RIGHT_INSPECTOR) hTarget = hChat;
            else if (activePanel == ZONE_BOTTOM_RUNTIME) hTarget = hRuntime;

            if (hTarget) {
                hdwp = DeferWindowPos(hdwp, hTarget, NULL,
                    node->rcContent.left, node->rcContent.top,
                    node->rcContent.right - node->rcContent.left,
                    node->rcContent.bottom - node->rcContent.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        if (node->firstChild) ApplyGeometryRecursive(node->firstChild, hdwp);
        if (node->firstChild && node->firstChild->nextSibling) ApplyGeometryRecursive(node->firstChild->nextSibling, hdwp);
    } else if (node->kind == DNK_SPLIT) {
        if (node->firstChild) ApplyGeometryRecursive(node->firstChild, hdwp);
        if (node->firstChild && node->firstChild->nextSibling) ApplyGeometryRecursive(node->firstChild->nextSibling, hdwp);
    }
}

void DockManager_ApplyGeometry(UI_STATE* state) {
    if (!state || !state->pShellRoot) return;
    HDWP hdwp = BeginDeferWindowPos(4);
    ApplyGeometryRecursive(state->pShellRoot, hdwp);
    EndDeferWindowPos(hdwp);
}

// ---------------------------------------------
// Mouse Handlers
// ---------------------------------------------
static DOCK_NODE* FindSplitterHit(DOCK_NODE* node, int x, int y) {
    if (!node) return NULL;
    
    if (node->kind == DNK_SPLIT) {
        POINT pt = {x, y};
        if (PtInRect(&node->rcSplitter, pt)) {
            return node;
        }
        
        DOCK_NODE* hit = FindSplitterHit(node->firstChild, x, y);
        if (hit) return hit;
        
        if (node->firstChild) {
            return FindSplitterHit(node->firstChild->nextSibling, x, y);
        }
    }
    
    DOCK_NODE* hit = FindSplitterHit(node->firstChild, x, y);
    if (hit) return hit;
    if (node->firstChild) return FindSplitterHit(node->firstChild->nextSibling, x, y);
    
    return NULL;
}

static DOCK_NODE* FindNodeById(DOCK_NODE* node, int id) {
    if (!node) return NULL;
    if (node->id == id) return node;
    
    DOCK_NODE* found = FindNodeById(node->firstChild, id);
    if (found) return found;
    
    if (node->firstChild) {
        return FindNodeById(node->firstChild->nextSibling, id);
    }
    return NULL;
}

__declspec(dllexport) void DockManager_OnMouseMove(UI_STATE* state, int x, int y) {
    if (!state) return;
    
    if (state->isDraggingSplitter) {
        DOCK_NODE* node = FindNodeById(state->pShellRoot, state->splitterActiveId);
        if (node && node->kind == DNK_SPLIT) {
            if (node->u.split.axis == AXIS_VERT) {
                int newPos = x - node->rcBounds.left;
                if (newPos < 50) newPos = 50;
                if (newPos > (node->rcBounds.right - node->rcBounds.left) - 50) 
                    newPos = (node->rcBounds.right - node->rcBounds.left) - 50;
                node->u.split.splitPos = newPos;
            } else if (node->u.split.axis == AXIS_HORZ) {
                int newPos = y - node->rcBounds.top;
                if (newPos < 50) newPos = 50;
                if (newPos > (node->rcBounds.bottom - node->rcBounds.top) - 50) 
                    newPos = (node->rcBounds.bottom - node->rcBounds.top) - 50;
                node->u.split.splitPos = newPos;
            }
            InvalidateRect(state->hwndDockHost, NULL, TRUE);
        }
    } else {
        DOCK_NODE* hit = FindSplitterHit(state->pShellRoot, x, y);
        if (hit) {
            if (hit->u.split.axis == AXIS_VERT) {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            } else {
                SetCursor(LoadCursor(NULL, IDC_SIZENS));
            }
        }
    }
}

__declspec(dllexport) void DockManager_OnLButtonDown(UI_STATE* state, int x, int y) {
    if (!state) return;
    
    DOCK_NODE* hit = FindSplitterHit(state->pShellRoot, x, y);
    if (hit) {
        state->isDraggingSplitter = TRUE;
        state->splitterActiveId = hit->id;
        SetCapture(state->hwndDockHost);
    }
}

__declspec(dllexport) void DockManager_OnLButtonUp(UI_STATE* state, int x, int y) {
    if (!state) return;
    
    if (state->isDraggingSplitter) {
        state->isDraggingSplitter = FALSE;
        state->splitterActiveId = 0;
        ReleaseCapture();
    }
}

} // extern "C"
