// ============================================================================
// Win32IDE_SidebarBridge.cpp — Phase 17: MASM64 Sidebar Bridge
// ============================================================================
// Window subclassing bridge between MASM64 and Win32 sidebar rendering:
//   - WM_USER+100 message handling from MASM64
//   - Sidebar content rendering and updates
//   - Cross-process sidebar state synchronization
//   - Real-time UI updates from beacon system
// ============================================================================

#include "BATCH2_CONTEXT.h"
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <string>
#include <windows.h>
#include <map>
#include <cstring>
#include <ctime>

// ============================================================================
// SIDEBAR BRIDGE STATE
// ============================================================================

struct SidebarBridgeState {
    HWND hwndSidebar;
    WNDPROC originalWndProc;
    HANDLE hMmf;
    void* mmfBase;
    std::map<int, std::string> sidebarItems;
    int selectedItem;
    bool isVisible;
};

static SidebarBridgeState g_sidebarState = { nullptr, nullptr, nullptr, nullptr, {}, -1, false };

// Forward declarations for handlers used before definition.
LRESULT HandleMASM64Message(UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarUpdateItems(WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarSelectItem(WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarSetVisibility(WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarRefreshContent(WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarBeaconStatus(WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarPaint(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarClick(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarResize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleSidebarDestroy(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// SUBCLASSING WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK SidebarBridgeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Handle MASM64 messages (WM_USER+100 and above)
    if (msg >= WM_USER + 100) {
        return HandleMASM64Message(msg, wParam, lParam);
    }

    // Handle standard Windows messages
    switch (msg) {
        case WM_PAINT:
            return HandleSidebarPaint(hwnd, msg, wParam, lParam);

        case WM_LBUTTONDOWN:
            return HandleSidebarClick(hwnd, msg, wParam, lParam);

        case WM_SIZE:
            return HandleSidebarResize(hwnd, msg, wParam, lParam);

        case WM_DESTROY:
            return HandleSidebarDestroy(hwnd, msg, wParam, lParam);
    }

    // Call original window procedure for unhandled messages
    if (g_sidebarState.originalWndProc) {
        return CallWindowProc(g_sidebarState.originalWndProc, hwnd, msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// MASM64 MESSAGE HANDLERS
// ============================================================================

LRESULT HandleMASM64Message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_USER + 100: // SIDEBAR_UPDATE_ITEMS
            return HandleSidebarUpdateItems(wParam, lParam);

        case WM_USER + 101: // SIDEBAR_SELECT_ITEM
            return HandleSidebarSelectItem(wParam, lParam);

        case WM_USER + 102: // SIDEBAR_SET_VISIBILITY
            return HandleSidebarSetVisibility(wParam, lParam);

        case WM_USER + 103: // SIDEBAR_REFRESH_CONTENT
            return HandleSidebarRefreshContent(wParam, lParam);

        case WM_USER + 104: // SIDEBAR_BEACON_STATUS
            return HandleSidebarBeaconStatus(wParam, lParam);

        default:
            // Forward unknown MASM64 messages to beacon system
            RawrXD_SidebarWndProc(g_sidebarState.hwndSidebar, msg, wParam, lParam);
            return 0;
    }
}

LRESULT HandleSidebarUpdateItems(WPARAM wParam, LPARAM lParam) {
    // wParam = item count, lParam = pointer to item array in MMF
    int itemCount = (int)wParam;
    const char* itemData = (const char*)lParam;

    g_sidebarState.sidebarItems.clear();

    // Parse items from MMF data
    const char* current = itemData;
    for (int i = 0; i < itemCount && current; ++i) {
        // Assume items are null-terminated strings
        std::string itemText = current;
        g_sidebarState.sidebarItems[i] = itemText;

        // Move to next item
        current += itemText.length() + 1;
    }

    // Trigger repaint
    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
    }

    return itemCount;
}

LRESULT HandleSidebarSelectItem(WPARAM wParam, LPARAM lParam) {
    int selectedIndex = (int)wParam;
    g_sidebarState.selectedItem = selectedIndex;

    // Trigger repaint to show selection
    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
    }

    return 0;
}

LRESULT HandleSidebarSetVisibility(WPARAM wParam, LPARAM lParam) {
    bool visible = (wParam != 0);
    g_sidebarState.isVisible = visible;

    if (g_sidebarState.hwndSidebar) {
        ShowWindow(g_sidebarState.hwndSidebar, visible ? SW_SHOW : SW_HIDE);
    }

    return 0;
}

LRESULT HandleSidebarRefreshContent(WPARAM wParam, LPARAM lParam) {
    // Force complete refresh of sidebar content
    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
        UpdateWindow(g_sidebarState.hwndSidebar);
    }

    return 0;
}

LRESULT HandleSidebarBeaconStatus(WPARAM wParam, LPARAM lParam) {
    // Update sidebar with beacon status information
    BeaconStage stage = (BeaconStage)wParam;
    const char* statusText = (const char*)lParam;

    // Add status item to sidebar
    g_sidebarState.sidebarItems[-1] = std::string("Beacon: ") + statusText;

    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
    }

    return 0;
}

// ============================================================================
// STANDARD WINDOWS MESSAGE HANDLERS
// ============================================================================

LRESULT HandleSidebarPaint(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    if (!hdc) return 0;

    // Get client area
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    // Fill background
    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
    FillRect(hdc, &clientRect, hBrush);
    DeleteObject(hBrush);

    // Set up fonts
    HFONT hFont = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    // Draw sidebar items
    int y = 10;
    for (const auto& item : g_sidebarState.sidebarItems) {
        RECT itemRect = { 10, y, clientRect.right - 10, y + 20 };

        // Highlight selected item
        if (item.first == g_sidebarState.selectedItem) {
            HBRUSH hSelBrush = CreateSolidBrush(RGB(200, 220, 255));
            FillRect(hdc, &itemRect, hSelBrush);
            DeleteObject(hSelBrush);
        }

        // Draw item text
        SetBkMode(hdc, TRANSPARENT);
        DrawTextA(hdc, item.second.c_str(), -1, &itemRect,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        y += 25;
    }

    // Clean up
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT HandleSidebarClick(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    // Calculate which item was clicked
    int itemIndex = (y - 10) / 25;

    if (itemIndex >= 0 && itemIndex < (int)g_sidebarState.sidebarItems.size()) {
        g_sidebarState.selectedItem = itemIndex;

        // Notify MASM64 of selection change
        RawrXD_SidebarWndProc(hwnd, WM_USER + 105, itemIndex, 0);

        // Trigger repaint
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    return 0;
}

LRESULT HandleSidebarResize(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Handle sidebar resizing
    InvalidateRect(hwnd, nullptr, TRUE);
    return 0;
}

LRESULT HandleSidebarDestroy(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Clean up subclassing
    if (g_sidebarState.originalWndProc) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)g_sidebarState.originalWndProc);
        g_sidebarState.originalWndProc = nullptr;
    }

    return 0;
}

// ============================================================================
// SIDEBAR BRIDGE INITIALIZATION
// ============================================================================

extern "C" __declspec(dllexport) CommandResult SidebarBridge_Create(HWND hwndParent, int x, int y, int width, int height) {
    CommandResult result = { false, "", 0 };

    // Forward declare for early error paths.
    void SidebarBridge_Destroy();

    if (g_sidebarState.hwndSidebar) {
        strcpy_s(result.message, "Sidebar bridge already created");
        result.success = true;
        return result;
    }

    // Create sidebar window
    g_sidebarState.hwndSidebar = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "STATIC",  // Use static control as base
        "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height,
        hwndParent,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (!g_sidebarState.hwndSidebar) {
        strcpy_s(result.message, "Failed to create sidebar window");
        return result;
    }

    // Subclass the window
    g_sidebarState.originalWndProc = (WNDPROC)SetWindowLongPtr(
        g_sidebarState.hwndSidebar,
        GWLP_WNDPROC,
        (LONG_PTR)SidebarBridgeWndProc
    );

    if (!g_sidebarState.originalWndProc) {
        DestroyWindow(g_sidebarState.hwndSidebar);
        g_sidebarState.hwndSidebar = nullptr;
        strcpy_s(result.message, "Failed to subclass sidebar window");
        return result;
    }

    // Initialize MMF for sidebar communication
    g_sidebarState.hMmf = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        4096,  // 4KB for sidebar data
        "RawrXD_Sidebar_MMF"
    );

    if (!g_sidebarState.hMmf) {
        SidebarBridge_Destroy();
        strcpy_s(result.message, "Failed to create sidebar MMF");
        return result;
    }

    g_sidebarState.mmfBase = MapViewOfFile(
        g_sidebarState.hMmf,
        FILE_MAP_ALL_ACCESS,
        0, 0, 4096
    );

    if (!g_sidebarState.mmfBase) {
        SidebarBridge_Destroy();
        strcpy_s(result.message, "Failed to map sidebar MMF");
        return result;
    }

    // Initialize default sidebar items
    g_sidebarState.sidebarItems = {
        {0, "Explorer"},
        {1, "Search"},
        {2, "Source Control"},
        {3, "Run & Debug"},
        {4, "Extensions"}
    };
    g_sidebarState.selectedItem = 0;
    g_sidebarState.isVisible = true;

    strcpy_s(result.message, "Sidebar bridge created successfully");
    result.success = true;
    return result;
}

// ============================================================================
// SIDEBAR BRIDGE MANAGEMENT
// ============================================================================

extern "C" __declspec(dllexport) void SidebarBridge_UpdateContent(const std::map<int, std::string>& items) {
    g_sidebarState.sidebarItems = items;

    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
    }
}

void SidebarBridge_SetSelection(int itemIndex) {
    g_sidebarState.selectedItem = itemIndex;

    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
    }
}

void SidebarBridge_SetVisibility(bool visible) {
    g_sidebarState.isVisible = visible;

    if (g_sidebarState.hwndSidebar) {
        ShowWindow(g_sidebarState.hwndSidebar, visible ? SW_SHOW : SW_HIDE);
    }
}

extern "C" __declspec(dllexport) void SidebarBridge_Refresh() {
    if (g_sidebarState.hwndSidebar) {
        InvalidateRect(g_sidebarState.hwndSidebar, nullptr, TRUE);
        UpdateWindow(g_sidebarState.hwndSidebar);
    }
}

// ============================================================================
// CROSS-PROCESS COMMUNICATION
// ============================================================================

bool SidebarBridge_SendToMASM64(UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_sidebarState.hwndSidebar) return false;

    // Send message to MASM64 through the bridged window procedure
    return (LRESULT)RawrXD_SidebarWndProc(g_sidebarState.hwndSidebar, message, wParam, lParam) == 0;
}

bool SidebarBridge_ReceiveFromMASM64(void* buffer, size_t bufferSize) {
    if (!g_sidebarState.mmfBase || !buffer || bufferSize == 0) return false;

    // Copy data from MMF to buffer
    const size_t copySize = (bufferSize < 4096) ? bufferSize : 4096;
    memcpy(buffer, g_sidebarState.mmfBase, copySize);
    return true;
}

bool SidebarBridge_SendToMASM64(void* data, size_t dataSize) {
    if (!g_sidebarState.mmfBase || !data || dataSize == 0 || dataSize > 4096) return false;

    // Copy data to MMF
    memcpy(g_sidebarState.mmfBase, data, dataSize);
    return true;
}

// ============================================================================
// CLEANUP AND SHUTDOWN
// ============================================================================

extern "C" __declspec(dllexport) void SidebarBridge_Destroy() {
    // Clean up MMF
    if (g_sidebarState.mmfBase) {
        UnmapViewOfFile(g_sidebarState.mmfBase);
        g_sidebarState.mmfBase = nullptr;
    }

    if (g_sidebarState.hMmf) {
        CloseHandle(g_sidebarState.hMmf);
        g_sidebarState.hMmf = nullptr;
    }

    // Clean up window
    if (g_sidebarState.hwndSidebar) {
        if (g_sidebarState.originalWndProc) {
            SetWindowLongPtr(g_sidebarState.hwndSidebar, GWLP_WNDPROC,
                           (LONG_PTR)g_sidebarState.originalWndProc);
        }
        DestroyWindow(g_sidebarState.hwndSidebar);
        g_sidebarState.hwndSidebar = nullptr;
    }

    // Reset state
    g_sidebarState = { nullptr, nullptr, nullptr, nullptr, {}, -1, false };
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

HWND SidebarBridge_GetHWND() {
    return g_sidebarState.hwndSidebar;
}

bool SidebarBridge_IsVisible() {
    return g_sidebarState.isVisible;
}

int SidebarBridge_GetSelectedItem() {
    return g_sidebarState.selectedItem;
}

size_t SidebarBridge_GetItemCount() {
    return g_sidebarState.sidebarItems.size();
}

std::string SidebarBridge_GetItemText(int index) {
    auto it = g_sidebarState.sidebarItems.find(index);
    return it != g_sidebarState.sidebarItems.end() ? it->second : "";
}

// Force symbol export
#pragma comment(linker, "/EXPORT:SidebarBridge_Create")
#pragma comment(linker, "/EXPORT:SidebarBridge_Destroy")
#pragma comment(linker, "/EXPORT:SidebarBridge_UpdateContent")
#pragma comment(linker, "/EXPORT:SidebarBridge_Refresh")


