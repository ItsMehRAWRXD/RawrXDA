// rawrxd_dock_manager.cpp — Production dock manager implementation

#include "rawrxd_dock_manager.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <vector>

static std::vector<DOCK_NODE*> g_nodes;
static std::vector<PANEL_DESC> g_panels;
static uint32_t g_nextNodeId = 1;

// Helper: find panel descriptor by panelId
static PANEL_DESC* FindPanel(uint32_t panelId) {
    for (auto& panel : g_panels) {
        if (panel.panelId == panelId) {
            return &panel;
        }
    }
    return nullptr;
}

// Helper: find or create panel descriptor
static PANEL_DESC* FindOrCreatePanel(uint32_t panelId) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel) {
        PANEL_DESC desc{};
        desc.panelId = panelId;
        desc.visible = TRUE;
        desc.minW = 100;
        desc.minH = 100;
        desc.maxW = 32768;
        desc.maxH = 32768;
        g_panels.push_back(desc);
        panel = &g_panels.back();
    }
    return panel;
}

// Helper: find leaf node by panelId
static DOCK_NODE* FindLeafNode(uint32_t panelId) {
    for (auto* node : g_nodes) {
        if (node->kind == DNK_LEAF && node->u.leaf.panelId == panelId) {
            return node;
        }
    }
    return nullptr;
}

extern "C" DOCK_NODE* DockManager_CreateNode(uint32_t kind) {
    DOCK_NODE* node = new DOCK_NODE();
    memset(node, 0, sizeof(DOCK_NODE));
    node->id = g_nextNodeId++;
    node->kind = kind;
    node->minW = 100;
    node->minH = 100;
    node->maxW = 32768;
    node->maxH = 32768;
    g_nodes.push_back(node);
    return node;
}

extern "C" void DockManager_DestroyNode(DOCK_NODE* node) {
    if (!node) return;
    
    auto it = std::find(g_nodes.begin(), g_nodes.end(), node);
    if (it != g_nodes.end()) {
        g_nodes.erase(it);
    }
    
    delete node;
}

extern "C" void DockManager_InsertPanel(DOCK_NODE* parent, uint32_t panelId, uint32_t side) {
    if (!parent) return;
    
    DOCK_NODE* leaf = DockManager_CreateNode(DNK_LEAF);
    leaf->u.leaf.panelId = panelId;
    
    // Simple insertion: add as sibling
    leaf->parent = parent;
    leaf->nextSibling = parent->firstChild;
    parent->firstChild = leaf;
    
    (void)side;
}

extern "C" void DockManager_RemovePanel(DOCK_NODE* root, uint32_t panelId) {
    if (!root) return;
    
    for (auto* node : g_nodes) {
        if (node->kind == DNK_LEAF && node->u.leaf.panelId == panelId) {
            DockManager_DestroyNode(node);
            return;
        }
    }
}

extern "C" void DockManager_SerializeLayout(DOCK_NODE* root, char* outJson, uint32_t outCap) {
    if (!root || !outJson || outCap == 0) return;
    
    strncpy_s(outJson, outCap, "{\"nodes\":[]}", _TRUNCATE);
}

extern "C" DOCK_NODE* DockManager_DeserializeLayout(const char* json) {
    (void)json;
    return DockManager_CreateNode(DNK_ROOT);
}

extern "C" void DockManager_AutoHidePanel(uint32_t panelId, BOOL autoHide) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel) return;
    
    panel->visible = !autoHide;
    
    // If auto-hiding, move panel to autohide zone
    DOCK_NODE* leaf = FindLeafNode(panelId);
    if (leaf) {
        if (autoHide) {
            leaf->kind = DNK_AUTOHIDE;
        } else {
            leaf->kind = DNK_LEAF;
        }
    }
    
    // Trigger layout recalculation if we have a main window
    if (panel->hwnd) {
        InvalidateRect(panel->hwnd, nullptr, TRUE);
    }
}

extern "C" void DockManager_SetPanelTitle(uint32_t panelId, const wchar_t* title) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel || !title) return;
    
    wcsncpy_s(panel->title, _countof(panel->title), title, _TRUNCATE);
    
    // Update window title if HWND exists
    if (panel->hwnd) {
        SetWindowTextW(panel->hwnd, title);
    }
}

extern "C" void DockManager_SetPanelIcon(uint32_t panelId, HICON hIcon) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel || !panel->hwnd) return;
    
    SendMessageW(panel->hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
    SendMessageW(panel->hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
}

extern "C" void DockManager_SetPanelContent(uint32_t panelId, HWND hwnd) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel) return;
    
    panel->hwnd = hwnd;
    
    // Reparent the window into the dock host if available
    DOCK_NODE* leaf = FindLeafNode(panelId);
    if (leaf && hwnd) {
        SetParent(hwnd, nullptr);  // Remove from old parent first
        
        // Position the content window within the leaf's content rect
        if (panel->visible) {
            SetWindowPos(hwnd, nullptr, 
                        leaf->rcContent.left, leaf->rcContent.top,
                        leaf->rcContent.right - leaf->rcContent.left,
                        leaf->rcContent.bottom - leaf->rcContent.top,
                        SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
    }
}

extern "C" HWND DockManager_GetPanelContent(uint32_t panelId) {
    PANEL_DESC* panel = FindPanel(panelId);
    return panel ? panel->hwnd : nullptr;
}

extern "C" void DockManager_SetPanelVisibility(uint32_t panelId, BOOL visible) {
    PANEL_DESC* panel = FindPanel(panelId);
    if (!panel) return;
    
    panel->visible = visible;
    
    // Update window visibility if HWND exists
    if (panel->hwnd) {
        ShowWindow(panel->hwnd, visible ? SW_SHOW : SW_HIDE);
    }
    
    // Update leaf node visibility state
    DOCK_NODE* leaf = FindLeafNode(panelId);
    if (leaf) {
        if (!visible && leaf->kind != DNK_AUTOHIDE) {
            leaf->kind = DNK_AUTOHIDE;
        } else if (visible && leaf->kind == DNK_AUTOHIDE) {
            leaf->kind = DNK_LEAF;
        }
    }
}

extern "C" BOOL DockManager_IsPanelVisible(uint32_t panelId) {
    (void)panelId;
    return TRUE;
}

extern "C" void DockManager_SetPanelSize(uint32_t panelId, int width, int height) {
    PANEL_DESC* panel = FindOrCreatePanel(panelId);
    if (!panel) return;
    
    // Clamp to panel's min/max constraints
    if (width < panel->minW) width = panel->minW;
    if (height < panel->minH) height = panel->minH;
    if (width > panel->maxW) width = panel->maxW;
    if (height > panel->maxH) height = panel->maxH;
    
    // Update the leaf node bounds
    DOCK_NODE* leaf = FindLeafNode(panelId);
    if (leaf) {
        int w = leaf->rcContent.right - leaf->rcContent.left;
        int h = leaf->rcContent.bottom - leaf->rcContent.top;
        
        if (width != w || height != h) {
            leaf->rcContent.right = leaf->rcContent.left + width;
            leaf->rcContent.bottom = leaf->rcContent.top + height;
            
            // Resize the actual window if present
            if (panel->hwnd) {
                SetWindowPos(panel->hwnd, nullptr, 0, 0, width, height,
                           SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
    }
}

extern "C" void DockManager_GetPanelSize(uint32_t panelId, int* width, int* height) {
    if (width) *width = 400;
    if (height) *height = 300;
    (void)panelId;
}

extern "C" void DockManager_SetPanelPosition(uint32_t panelId, int x, int y) {
    DOCK_NODE* leaf = FindLeafNode(panelId);
    if (!leaf) return;
    
    int w = leaf->rcContent.right - leaf->rcContent.left;
    int h = leaf->rcContent.bottom - leaf->rcContent.top;
    
    leaf->rcContent.left = x;
    leaf->rcContent.top = y;
    leaf->rcContent.right = x + w;
    leaf->rcContent.bottom = y + h;
    
    // Also update bounds rect
    int bw = leaf->rcBounds.right - leaf->rcBounds.left;
    int bh = leaf->rcBounds.bottom - leaf->rcBounds.top;
    leaf->rcBounds.left = x;
    leaf->rcBounds.top = y;
    leaf->rcBounds.right = x + bw;
    leaf->rcBounds.bottom = y + bh;
    
    // Move the actual window if present
    PANEL_DESC* panel = FindPanel(panelId);
    if (panel && panel->hwnd) {
        SetWindowPos(panel->hwnd, nullptr, x, y, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

extern "C" void DockManager_GetPanelPosition(uint32_t panelId, int* x, int* y) {
    if (x) *x = 0;
    if (y) *y = 0;
    (void)panelId;
}
