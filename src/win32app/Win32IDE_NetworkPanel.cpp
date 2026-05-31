// ============================================================================
// Win32IDE_NetworkPanel.cpp — Tier 5 Gap #41: Port Forwarding UI
// ============================================================================
//
// PURPOSE:
//   Provides a VS Code-style "Ports" panel for local network tunneling and
//   port forwarding.  Displays a ListView of forwarded ports with columns:
//     Port | Label | Protocol | Local Address | Forwarded Address | Status
//   Supports add/remove/toggle operations via toolbar buttons.
//
// Status: Production UI (ListView, toolbar); port-forwarding backend may be extended (e.g. SSH tunnel).
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// Port forwarding state
// ============================================================================

struct PortForwardEntry {
    uint16_t    localPort;
    uint16_t    remotePort;
    std::string label;
    std::string protocol;      // "HTTP", "TCP", "HTTPS"
    std::string localAddress;  // "localhost"
    std::string forwardAddress;
    bool        active;
    uint64_t    bytesTransferred;
    SOCKET      listenSocket;
    std::atomic<bool> running;

    PortForwardEntry()
        : localPort(0), remotePort(0), protocol("TCP"),
          localAddress("localhost"), active(false),
          bytesTransferred(0), listenSocket(INVALID_SOCKET), running(false) {}
};

static std::vector<PortForwardEntry*> s_portEntries;
static std::mutex s_portMutex;
static bool   s_networkPanelClassRegistered = false;
static HWND   s_hwndNetworkPanel = nullptr;
static HWND   s_hwndPortListView = nullptr;

static const wchar_t* NETWORK_PANEL_CLASS = L"RawrXD_NetworkPanel";

// ============================================================================
// ListView helpers (Wide-suffix)
// ============================================================================

#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif

static int NP_InsertColumnW(HWND hwnd, int col, int width, const wchar_t* text) {
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt  = LVCFMT_LEFT;
    lvc.cx   = width;
    lvc.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTCOLUMNW, col, (LPARAM)&lvc);
}

static int NP_InsertItemW(HWND hwnd, int row, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.mask    = LVIF_TEXT;
    lvi.iItem   = row;
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(text);
    return (int)SendMessageW(hwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
}

static void NP_SetItemTextW(HWND hwnd, int row, int col, const wchar_t* text) {
    LVITEMW lvi{};
    lvi.iSubItem = col;
    lvi.pszText  = const_cast<LPWSTR>(text);
    SendMessageW(hwnd, LVM_SETITEMTEXTW, row, (LPARAM)&lvi);
}

// ============================================================================
// Refresh the port list view
// ============================================================================

static void refreshPortListView() {
    if (!s_hwndPortListView || !IsWindow(s_hwndPortListView)) return;

    SendMessageW(s_hwndPortListView, LVM_DELETEALLITEMS, 0, 0);

    std::lock_guard<std::mutex> lock(s_portMutex);
    for (size_t i = 0; i < s_portEntries.size(); ++i) {
        PortForwardEntry* e = s_portEntries[i];

        wchar_t buf[128];

        // Column 0: Port
        swprintf(buf, 128, L"%u", e->localPort);
        NP_InsertItemW(s_hwndPortListView, (int)i, buf);

        // Column 1: Label
        wchar_t label[128] = {};
        MultiByteToWideChar(CP_UTF8, 0, e->label.c_str(), -1, label, 127);
        NP_SetItemTextW(s_hwndPortListView, (int)i, 1, label);

        // Column 2: Protocol
        wchar_t proto[32] = {};
        MultiByteToWideChar(CP_UTF8, 0, e->protocol.c_str(), -1, proto, 31);
        NP_SetItemTextW(s_hwndPortListView, (int)i, 2, proto);

        // Column 3: Local Address
        swprintf(buf, 128, L"%hs:%u", e->localAddress.c_str(), e->localPort);
        NP_SetItemTextW(s_hwndPortListView, (int)i, 3, buf);

        // Column 4: Forwarded Address
        if (!e->forwardAddress.empty()) {
            wchar_t fwd[128] = {};
            MultiByteToWideChar(CP_UTF8, 0, e->forwardAddress.c_str(), -1, fwd, 127);
            NP_SetItemTextW(s_hwndPortListView, (int)i, 4, fwd);
        } else {
            NP_SetItemTextW(s_hwndPortListView, (int)i, 4, L"—");
        }

        // Column 5: Status
        NP_SetItemTextW(s_hwndPortListView, (int)i, 5,
                        e->active ? L"● Active" : L"○ Stopped");

        // Column 6: Bytes
        swprintf(buf, 128, L"%llu", (unsigned long long)e->bytesTransferred);
        NP_SetItemTextW(s_hwndPortListView, (int)i, 6, buf);
    }
}

// ============================================================================
// Custom Draw (color: green for active, gray for stopped)
// ============================================================================

static LRESULT handleNetworkCustomDraw(LPNMLVCUSTOMDRAW lpcd) {
    switch (lpcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;
        case CDDS_ITEMPREPAINT: {
            int item = (int)lpcd->nmcd.dwItemSpec;
            std::lock_guard<std::mutex> lock(s_portMutex);
            if (item >= 0 && item < (int)s_portEntries.size()) {
                if (s_portEntries[item]->active) {
                    lpcd->clrText   = RGB(50, 205, 50);
                    lpcd->clrTextBk = RGB(20, 40, 20);
                } else {
                    lpcd->clrText   = RGB(180, 180, 180);
                    lpcd->clrTextBk = RGB(30, 30, 30);
                }
            }
            return CDRF_DODEFAULT;
        }
        default:
            return CDRF_DODEFAULT;
    }
}

// ============================================================================
// TCP Port-Forwarding Relay Backend
// ============================================================================

static void portForwardRelayThread(PortForwardEntry* entry, SOCKET clientSock) {
    // Connect to the remote (forwarded) address
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string remoteHost = "127.0.0.1";
    uint16_t remotePort    = entry->remotePort;

    // Parse forwardAddress if it contains host:port
    if (!entry->forwardAddress.empty()) {
        size_t colon = entry->forwardAddress.rfind(':');
        if (colon != std::string::npos) {
            std::string h = entry->forwardAddress.substr(0, colon);
            if (h == "localhost") h = "127.0.0.1";
            if (!h.empty()) remoteHost = h;
            int p = std::atoi(entry->forwardAddress.c_str() + colon + 1);
            if (p > 0 && p <= 65535) remotePort = static_cast<uint16_t>(p);
        }
    }

    std::string portStr = std::to_string(remotePort);
    if (getaddrinfo(remoteHost.c_str(), portStr.c_str(), &hints, &result) != 0 || !result) {
        closesocket(clientSock);
        return;
    }

    SOCKET remoteSock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (remoteSock == INVALID_SOCKET) {
        freeaddrinfo(result);
        closesocket(clientSock);
        return;
    }

    if (connect(remoteSock, result->ai_addr, (int)result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        closesocket(remoteSock);
        closesocket(clientSock);
        return;
    }
    freeaddrinfo(result);

    // Bidirectional relay: use select() to shuttle data between client <-> remote
    char buf[8192];
    fd_set readfds;
    while (entry->running.load()) {
        FD_ZERO(&readfds);
        FD_SET(clientSock, &readfds);
        FD_SET(remoteSock, &readfds);

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        SOCKET maxFd = (clientSock > remoteSock ? clientSock : remoteSock) + 1;
        int sel = select((int)maxFd, &readfds, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        // Client -> Remote
        if (FD_ISSET(clientSock, &readfds)) {
            int n = recv(clientSock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            int sent = 0;
            while (sent < n) {
                int s = send(remoteSock, buf + sent, n - sent, 0);
                if (s <= 0) goto relay_done;
                sent += s;
            }
            entry->bytesTransferred += (uint64_t)n;
        }

        // Remote -> Client
        if (FD_ISSET(remoteSock, &readfds)) {
            int n = recv(remoteSock, buf, sizeof(buf), 0);
            if (n <= 0) break;
            int sent = 0;
            while (sent < n) {
                int s = send(clientSock, buf + sent, n - sent, 0);
                if (s <= 0) goto relay_done;
                sent += s;
            }
            entry->bytesTransferred += (uint64_t)n;
        }
    }

relay_done:
    closesocket(remoteSock);
    closesocket(clientSock);
}

static void portForwardListenThread(PortForwardEntry* entry) {
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    std::string portStr = std::to_string(entry->localPort);
    if (getaddrinfo(nullptr, portStr.c_str(), &hints, &result) != 0 || !result) {
        entry->running.store(false);
        entry->active = false;
        return;
    }

    SOCKET listenSock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listenSock == INVALID_SOCKET) {
        freeaddrinfo(result);
        entry->running.store(false);
        entry->active = false;
        return;
    }

    // Allow address reuse
    int optval = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    if (bind(listenSock, result->ai_addr, (int)result->ai_addrlen) != 0) {
        freeaddrinfo(result);
        closesocket(listenSock);
        entry->running.store(false);
        entry->active = false;
        return;
    }
    freeaddrinfo(result);

    if (listen(listenSock, SOMAXCONN) != 0) {
        closesocket(listenSock);
        entry->running.store(false);
        entry->active = false;
        return;
    }

    entry->listenSocket = listenSock;

    // Set socket to non-blocking for clean shutdown via running flag
    u_long nonBlocking = 1;
    ioctlsocket(listenSock, FIONBIO, &nonBlocking);

    while (entry->running.load()) {
        fd_set acceptSet;
        FD_ZERO(&acceptSet);
        FD_SET(listenSock, &acceptSet);

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        int sel = select((int)(listenSock + 1), &acceptSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) continue;

        // Spawn relay thread for this connection
        PortForwardEntry* e = entry;
        SOCKET cs = clientSock;
        std::thread([e, cs]() {
            portForwardRelayThread(e, cs);
        }).detach();
    }

    closesocket(listenSock);
    entry->listenSocket = INVALID_SOCKET;
    entry->active = false;
}

static void startPortForward(PortForwardEntry* entry) {
    if (entry->running.load()) return;
    entry->running.store(true);
    entry->active = true;
    std::thread(portForwardListenThread, entry).detach();
}

static void stopPortForward(PortForwardEntry* entry) {
    entry->running.store(false);
    if (entry->listenSocket != INVALID_SOCKET) {
        closesocket(entry->listenSocket);
        entry->listenSocket = INVALID_SOCKET;
    }
    entry->active = false;
}

// ============================================================================
// Window Procedure
// ============================================================================

#define IDC_NP_ADD_BTN   2001
#define IDC_NP_REMOVE_BTN 2002
#define IDC_NP_TOGGLE_BTN 2003

static LRESULT CALLBACK networkPanelWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Toolbar buttons
        CreateWindowExW(0, L"BUTTON", L"+ Add Port",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 5, 90, 28, hwnd, (HMENU)IDC_NP_ADD_BTN,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"- Remove",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            105, 5, 80, 28, hwnd, (HMENU)IDC_NP_REMOVE_BTN,
            GetModuleHandleW(nullptr), nullptr);

        CreateWindowExW(0, L"BUTTON", L"Toggle",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            190, 5, 70, 28, hwnd, (HMENU)IDC_NP_TOGGLE_BTN,
            GetModuleHandleW(nullptr), nullptr);

        // ListView
        s_hwndPortListView = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
            0, 38, 0, 0, hwnd, (HMENU)2010,
            GetModuleHandleW(nullptr), nullptr);

        if (s_hwndPortListView) {
            SendMessageW(s_hwndPortListView, LVM_SETEXTENDEDLISTVIEWSTYLE, 0,
                         LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
            SendMessageW(s_hwndPortListView, LVM_SETBKCOLOR,     0, RGB(30, 30, 30));
            SendMessageW(s_hwndPortListView, LVM_SETTEXTBKCOLOR, 0, RGB(30, 30, 30));
            SendMessageW(s_hwndPortListView, LVM_SETTEXTCOLOR,   0, RGB(220, 220, 220));

            NP_InsertColumnW(s_hwndPortListView, 0, 60,  L"Port");
            NP_InsertColumnW(s_hwndPortListView, 1, 120, L"Label");
            NP_InsertColumnW(s_hwndPortListView, 2, 70,  L"Protocol");
            NP_InsertColumnW(s_hwndPortListView, 3, 140, L"Local Address");
            NP_InsertColumnW(s_hwndPortListView, 4, 140, L"Forwarded");
            NP_InsertColumnW(s_hwndPortListView, 5, 80,  L"Status");
            NP_InsertColumnW(s_hwndPortListView, 6, 80,  L"Bytes");
        }
        return 0;
    }

    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (s_hwndPortListView)
            MoveWindow(s_hwndPortListView, 0, 38, rc.right, rc.bottom - 38, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_NP_ADD_BTN: {
            // Simple input dialog for new port
            char portStr[16] = {};
            // Use a simple MessageBox-based prompt
            int port = 0;
            // InputBox emulation via a simple dialog
            port = 3000; // Default port for web dev

            PortForwardEntry* entry = new PortForwardEntry();
            entry->localPort     = static_cast<uint16_t>(port);
            entry->remotePort    = static_cast<uint16_t>(port);
            entry->label         = "Web Server";
            entry->protocol      = "HTTP";
            entry->localAddress  = "localhost";
            entry->forwardAddress = "localhost:" + std::to_string(port);
            entry->active        = false;

            {
                std::lock_guard<std::mutex> lock(s_portMutex);
                s_portEntries.push_back(entry);
            }
            refreshPortListView();
            break;
        }
        case IDC_NP_REMOVE_BTN: {
            int sel = (int)SendMessageW(s_hwndPortListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
            if (sel >= 0) {
                std::lock_guard<std::mutex> lock(s_portMutex);
                if (sel < (int)s_portEntries.size()) {
                    PortForwardEntry* e = s_portEntries[sel];
                    e->running.store(false);
                    if (e->listenSocket != INVALID_SOCKET) {
                        closesocket(e->listenSocket);
                    }
                    delete e;
                    s_portEntries.erase(s_portEntries.begin() + sel);
                }
            }
            refreshPortListView();
            break;
        }
        case IDC_NP_TOGGLE_BTN: {
            int sel = (int)SendMessageW(s_hwndPortListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
            if (sel >= 0) {
                std::lock_guard<std::mutex> lock(s_portMutex);
                if (sel < (int)s_portEntries.size()) {
                    PortForwardEntry* e = s_portEntries[sel];
                    if (e->active) {
                        stopPortForward(e);
                    } else {
                        startPortForward(e);
                    }
                }
            }
            refreshPortListView();
            break;
        }
        }
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* nmh = (NMHDR*)lParam;
        if (nmh && nmh->code == NM_CUSTOMDRAW) {
            return handleNetworkCustomDraw((LPNMLVCUSTOMDRAW)lParam);
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        s_hwndNetworkPanel   = nullptr;
        s_hwndPortListView   = nullptr;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Ensure window class registration
// ============================================================================

static bool ensureNetworkPanelClass() {
    if (s_networkPanelClassRegistered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = networkPanelWndProc;
    wc.cbWndExtra    = sizeof(void*);
    wc.hInstance      = GetModuleHandleW(nullptr);
    wc.hCursor        = LoadCursorW(nullptr, (LPCWSTR)(uintptr_t)IDC_ARROW);
    wc.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName  = NETWORK_PANEL_CLASS;

    if (!RegisterClassExW(&wc)) return false;
    s_networkPanelClassRegistered = true;
    return true;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initNetworkPanel() {
    if (m_networkPanelInitialized) return;
    OutputDebugStringA("[NetworkPanel] Tier 5 — Port Forwarding UI initialized.\n");
    m_networkPanelInitialized = true;
    appendToOutput("[NetworkPanel] Port forwarding panel ready.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleNetworkCommand(int commandId) {
    if (!m_networkPanelInitialized) initNetworkPanel();
    switch (commandId) {
        case IDM_NETWORK_SHOW:       cmdNetworkShowPanel();   return true;
        case IDM_NETWORK_ADD_PORT:   cmdNetworkAddPort();     return true;
        case IDM_NETWORK_REMOVE_PORT:cmdNetworkRemovePort();  return true;
        case IDM_NETWORK_TOGGLE:     cmdNetworkTogglePort();  return true;
        case IDM_NETWORK_LIST:       cmdNetworkListPorts();   return true;
        case IDM_NETWORK_STATUS:     cmdNetworkStatus();      return true;
        default: return false;
    }
}

// ============================================================================
// Show Panel
// ============================================================================

void Win32IDE::cmdNetworkShowPanel() {
    if (s_hwndNetworkPanel && IsWindow(s_hwndNetworkPanel)) {
        SetForegroundWindow(s_hwndNetworkPanel);
        return;
    }

    if (!ensureNetworkPanelClass()) {
        MessageBoxW(m_hwndMain, L"Failed to register network panel class.",
                    L"Network Panel Error", MB_OK | MB_ICONERROR);
        return;
    }

    s_hwndNetworkPanel = CreateWindowExW(
        WS_EX_OVERLAPPEDWINDOW,
        NETWORK_PANEL_CLASS,
        L"RawrXD — Port Forwarding",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 780, 400,
        m_hwndMain, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (s_hwndNetworkPanel) {
        refreshPortListView();
        ShowWindow(s_hwndNetworkPanel, SW_SHOW);
        UpdateWindow(s_hwndNetworkPanel);
        if (m_hwndStatusBar) SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)"Network: Port Forwarding");
    }
}

// ============================================================================
// Add Port (with dialog)
// ============================================================================

void Win32IDE::cmdNetworkAddPort() {
    // Modal dialog to capture port forwarding parameters
    const int DLG_W = 360, DLG_H = 260;
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    MapWindowPoints(m_hwndMain, HWND_DESKTOP, (LPPOINT)&rc, 2);
    int dx = rc.left + (rc.right - rc.left - DLG_W) / 2;
    int dy = rc.top  + (rc.bottom - rc.top  - DLG_H) / 2;

    // Default port: 3000 + existing count
    int defaultPort;
    {
        std::lock_guard<std::mutex> lock(s_portMutex);
        defaultPort = 3000 + (int)s_portEntries.size();
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"STATIC", L"Add Port Forward",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        dx, dy, DLG_W, DLG_H,
        m_hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return;

    auto lbl = [&](const wchar_t* t, int y) {
        CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, 15, y, 100, 20, hDlg, 0, 0, 0);
    };

    lbl(L"Local Port:",  15);
    HWND hLocal = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT",
        std::to_string(defaultPort).c_str(),
        WS_CHILD | WS_VISIBLE | ES_NUMBER, 120, 13, 80, 22, hDlg, (HMENU)4001, 0, 0);

    lbl(L"Remote Port:", 45);
    HWND hRemote = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT",
        std::to_string(defaultPort).c_str(),
        WS_CHILD | WS_VISIBLE | ES_NUMBER, 120, 43, 80, 22, hDlg, (HMENU)4002, 0, 0);

    lbl(L"Label:",       75);
    HWND hLabel = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT",
        ("Port " + std::to_string(defaultPort)).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 120, 73, 210, 22, hDlg, (HMENU)4003, 0, 0);

    lbl(L"Protocol:",    105);
    HWND hProto = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 120, 103, 100, 100, hDlg, (HMENU)4004, 0, 0);
    SendMessageA(hProto, CB_ADDSTRING, 0, (LPARAM)"TCP");
    SendMessageA(hProto, CB_ADDSTRING, 0, (LPARAM)"HTTP");
    SendMessageA(hProto, CB_ADDSTRING, 0, (LPARAM)"HTTPS");
    SendMessageA(hProto, CB_SETCURSEL, 0, 0);

    CreateWindowExW(0, L"BUTTON", L"Add",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        DLG_W - 180, DLG_H - 55, 75, 28, hDlg, (HMENU)IDOK, 0, 0);
    CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DLG_W - 95, DLG_H - 55, 75, 28, hDlg, (HMENU)IDCANCEL, 0, 0);

    EnableWindow(m_hwndMain, FALSE);
    MSG msg;
    bool running = true;
    while (running && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_COMMAND) {
                WORD id = LOWORD(msg.wParam);
                if (id == IDOK) {
                    char buf[128];
                    GetWindowTextA(hLocal, buf, sizeof(buf));
                    int lp = atoi(buf);
                    GetWindowTextA(hRemote, buf, sizeof(buf));
                    int rp = atoi(buf);
                    if (lp < 1 || lp > 65535) lp = defaultPort;
                    if (rp < 1 || rp > 65535) rp = lp;

                    char labelBuf[128];
                    GetWindowTextA(hLabel, labelBuf, sizeof(labelBuf));

                    int protoIdx = (int)SendMessageA(hProto, CB_GETCURSEL, 0, 0);
                    const char* protocols[] = { "TCP", "HTTP", "HTTPS" };
                    const char* proto = (protoIdx >= 0 && protoIdx < 3) ? protocols[protoIdx] : "TCP";

                    PortForwardEntry* entry = new PortForwardEntry();
                    entry->localPort      = static_cast<uint16_t>(lp);
                    entry->remotePort     = static_cast<uint16_t>(rp);
                    entry->label          = labelBuf[0] ? labelBuf : ("Port " + std::to_string(lp));
                    entry->protocol       = proto;
                    entry->localAddress   = "localhost";
                    entry->forwardAddress = "localhost:" + std::to_string(rp);
                    entry->active         = false;

                    {
                        std::lock_guard<std::mutex> lock(s_portMutex);
                        s_portEntries.push_back(entry);
                    }
                    refreshPortListView();

                    std::ostringstream oss;
                    oss << "[NetworkPanel] Added port forward: " << lp
                        << " -> " << rp << " (" << proto << ")\n";
                    appendToOutput(oss.str());
                    running = false;
                } else if (id == IDCANCEL) {
                    running = false;
                }
            }
            if (msg.message == WM_CLOSE || msg.message == WM_DESTROY)
                running = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);
}

// ============================================================================
// Remove Port (selected)
// ============================================================================

void Win32IDE::cmdNetworkRemovePort() {
    if (!s_hwndPortListView) {
        appendToOutput("[NetworkPanel] No port list view open.\n");
        return;
    }

    int sel = (int)SendMessageW(s_hwndPortListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (sel < 0) {
        appendToOutput("[NetworkPanel] No port selected.\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_portMutex);
        if (sel < (int)s_portEntries.size()) {
            PortForwardEntry* e = s_portEntries[sel];
            e->running.store(false);
            if (e->listenSocket != INVALID_SOCKET)
                closesocket(e->listenSocket);
            appendToOutput("[NetworkPanel] Removed port: " + std::to_string(e->localPort) + "\n");
            delete e;
            s_portEntries.erase(s_portEntries.begin() + sel);
        }
    }
    refreshPortListView();
}

// ============================================================================
// Toggle Port (active/stopped)
// ============================================================================

void Win32IDE::cmdNetworkTogglePort() {
    if (!s_hwndPortListView) return;

    int sel = (int)SendMessageW(s_hwndPortListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (sel < 0) {
        appendToOutput("[NetworkPanel] No port selected.\n");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_portMutex);
        if (sel < (int)s_portEntries.size()) {
            PortForwardEntry* e = s_portEntries[sel];
            if (e->active) {
                stopPortForward(e);
                appendToOutput("[NetworkPanel] Port " + std::to_string(e->localPort) +
                              " stopped (socket closed, relay threads terminating)\n");
            } else {
                startPortForward(e);
                appendToOutput("[NetworkPanel] Port " + std::to_string(e->localPort) +
                              " started (TCP relay listening on 0.0.0.0:" +
                              std::to_string(e->localPort) + ")\n");
            }
        }
    }
    refreshPortListView();
}

// ============================================================================
// List all ports to Output
// ============================================================================

void Win32IDE::cmdNetworkListPorts() {
    std::lock_guard<std::mutex> lock(s_portMutex);

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║               PORT FORWARDING TABLE                        ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    if (s_portEntries.empty()) {
        oss << "║  (no ports configured)                                     ║\n";
    } else {
        for (size_t i = 0; i < s_portEntries.size(); ++i) {
            auto* e = s_portEntries[i];
            char line[128];
            snprintf(line, sizeof(line),
                     "║  %3zu  %-12s  %-6s  %-20s  %-8s  ║\n",
                     i + 1, e->label.c_str(), e->protocol.c_str(),
                     e->forwardAddress.c_str(),
                     e->active ? "Active" : "Stopped");
            oss << line;
        }
    }
    oss << "╚══════════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

// ============================================================================
// Network Status summary
// ============================================================================

void Win32IDE::cmdNetworkStatus() {
    std::lock_guard<std::mutex> lock(s_portMutex);

    int total  = (int)s_portEntries.size();
    int active = 0;
    uint64_t totalBytes = 0;
    for (auto* e : s_portEntries) {
        if (e->active) ++active;
        totalBytes += e->bytesTransferred;
    }

    std::ostringstream oss;
    oss << "[NetworkPanel] Status: " << total << " ports configured, "
        << active << " active, " << totalBytes << " bytes transferred total.\n";
    appendToOutput(oss.str());
}
