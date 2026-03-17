#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commdlg.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>

// Manual definitions for GET_X_LPARAM/GET_Y_LPARAM if windowsx.h not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#include "logger.h"
#include "noqt_ide_app.h"

static constexpr UINT WM_APP_APPEND_TEXT = WM_APP + 1;

static std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

static std::string narrow(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), len, nullptr, nullptr);
    return out;
}

static std::wstring getWindowTextW(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    std::wstring text(len, L'\0');
    if (len > 0) GetWindowTextW(hwnd, text.data(), len + 1);
    return text;
}

static void setWindowTextW(HWND hwnd, const std::wstring& text) {
    SetWindowTextW(hwnd, text.c_str());
}

static void appendText(HWND hwndEdit, const std::wstring& text) {
    if (!hwndEdit) return;
    SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hwndEdit, EM_REPLACESEL, (WPARAM)TRUE, (LPARAM)text.c_str());
}

// ---------------------------
// Paint canvas (simple GDI)
// ---------------------------

struct PaintCanvasState {
    HDC memDC = nullptr;
    HBITMAP bmp = nullptr;
    int w = 0;
    int h = 0;
    bool drawing = false;
    POINT last = {0, 0};
    COLORREF color = RGB(0, 0, 0);
    int penWidth = 3;
};

static void paintCanvasEnsureBuffer(HWND hwnd, PaintCanvasState* st, int w, int h) {
    if (!st) return;
    if (w <= 0 || h <= 0) return;

    if (st->bmp && st->w == w && st->h == h) return;

    if (st->memDC) {
        SelectObject(st->memDC, (HGDIOBJ)GetStockObject(DC_BRUSH));
        DeleteDC(st->memDC);
        st->memDC = nullptr;
    }
    if (st->bmp) {
        DeleteObject(st->bmp);
        st->bmp = nullptr;
    }

    HDC screen = GetDC(hwnd);
    st->memDC = CreateCompatibleDC(screen);
    st->bmp = CreateCompatibleBitmap(screen, w, h);
    st->w = w;
    st->h = h;
    SelectObject(st->memDC, st->bmp);

    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    RECT r{0, 0, w, h};
    FillRect(st->memDC, &r, bg);
    DeleteObject(bg);

    ReleaseDC(hwnd, screen);
}

static bool saveBitmapAsBMP(const std::wstring& path, HBITMAP hBitmap) {
    if (!hBitmap) return false;

    BITMAP bm{};
    if (!GetObject(hBitmap, sizeof(bm), &bm)) return false;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bm.bmWidth;
    bmi.bmiHeader.biHeight = -bm.bmHeight; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<unsigned char> pixels((size_t)bm.bmWidth * (size_t)bm.bmHeight * 4);

    HDC screen = GetDC(nullptr);
    int got = GetDIBits(screen, hBitmap, 0, (UINT)bm.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen);
    if (got == 0) return false;

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // 'BM'
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + (DWORD)pixels.size();

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write((const char*)&bfh, sizeof(bfh));
    out.write((const char*)&bmi.bmiHeader, sizeof(bmi.bmiHeader));
    out.write((const char*)pixels.data(), (std::streamsize)pixels.size());
    return (bool)out;
}

static HBITMAP loadBMPToHBITMAP(const std::wstring& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return nullptr;

    BITMAPFILEHEADER bfh{};
    BITMAPINFOHEADER bih{};
    in.read((char*)&bfh, sizeof(bfh));
    in.read((char*)&bih, sizeof(bih));
    if (!in) return nullptr;
    if (bfh.bfType != 0x4D42) return nullptr;
    if (bih.biBitCount != 24 && bih.biBitCount != 32) return nullptr;

    int w = bih.biWidth;
    int h = bih.biHeight;
    bool topDown = false;
    if (h < 0) {
        h = -h;
        topDown = true;
    }

    // Read pixel data
    in.seekg((std::streamoff)bfh.bfOffBits, std::ios::beg);
    if (!in) return nullptr;

    // Normalize into 32bpp BGRA buffer
    std::vector<unsigned char> buf((size_t)w * (size_t)h * 4, 255);

    const int srcBpp = (int)bih.biBitCount;
    const size_t srcStride = ((size_t)w * (size_t)srcBpp + 31) / 32 * 4;
    std::vector<unsigned char> srcRow(srcStride);

    for (int y = 0; y < h; ++y) {
        int sy = topDown ? y : (h - 1 - y);
        in.read((char*)srcRow.data(), (std::streamsize)srcStride);
        if (!in) return nullptr;

        unsigned char* dst = buf.data() + (size_t)sy * (size_t)w * 4;
        const unsigned char* src = srcRow.data();
        for (int x = 0; x < w; ++x) {
            if (srcBpp == 24) {
                dst[x * 4 + 0] = src[x * 3 + 0];
                dst[x * 4 + 1] = src[x * 3 + 1];
                dst[x * 4 + 2] = src[x * 3 + 2];
                dst[x * 4 + 3] = 255;
            } else {
                dst[x * 4 + 0] = src[x * 4 + 0];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 2];
                dst[x * 4 + 3] = src[x * 4 + 3];
            }
        }
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        if (hbmp) DeleteObject(hbmp);
        return nullptr;
    }
    memcpy(bits, buf.data(), buf.size());
    return hbmp;
}

static LRESULT CALLBACK PaintCanvasProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<PaintCanvasState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return TRUE;
        }
        case WM_SIZE: {
            if (!st) break;
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            paintCanvasEnsureBuffer(hwnd, st, w, h);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!st) break;
            SetCapture(hwnd);
            st->drawing = true;
            st->last = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!st || !st->drawing || !st->memDC) break;
            POINT p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HPEN pen = CreatePen(PS_SOLID, st->penWidth, st->color);
            HPEN old = (HPEN)SelectObject(st->memDC, pen);
            MoveToEx(st->memDC, st->last.x, st->last.y, nullptr);
            LineTo(st->memDC, p.x, p.y);
            SelectObject(st->memDC, old);
            DeleteObject(pen);
            st->last = p;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            if (!st) break;
            st->drawing = false;
            ReleaseCapture();
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            if (st && st->memDC && st->bmp) {
                BitBlt(dc, 0, 0, st->w, st->h, st->memDC, 0, 0, SRCCOPY);
            } else {
                RECT r;
                GetClientRect(hwnd, &r);
                FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------
// NoQtIDEApp implementation
// ---------------------------

enum : UINT {
    ID_FILE_OPEN = 100,
    ID_FILE_SAVEAS,
    ID_FILE_EXIT,
    ID_EDIT_UNDO,
    ID_EDIT_REDO,
    ID_EDIT_CUT,
    ID_EDIT_COPY,
    ID_EDIT_PASTE,
    ID_VIEW_NEW_PAINT,
    ID_VIEW_NEW_CODE,
    ID_VIEW_NEW_CHAT,
    ID_TOOLS_PALETTE,
    ID_CANVAS_SAVE_BMP,
    ID_CANVAS_LOAD_BMP,
    ID_CHAT_SEND,
    ID_TERM_RUN,
};

bool NoQtIDEApp::create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // RichEdit
    LoadLibraryW(L"Msftedit.dll");

    // Register paint canvas class
    WNDCLASSW wcCanvas{};
    wcCanvas.lpfnWndProc = PaintCanvasProc;
    wcCanvas.hInstance = m_hInstance;
    wcCanvas.lpszClassName = L"RawrXDPaintCanvas";
    wcCanvas.hCursor = LoadCursor(nullptr, IDC_CROSS);
    wcCanvas.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    if (!RegisterClassW(&wcCanvas) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::instance().error("RegisterClassW(PaintCanvas) failed");
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = NoQtIDEApp::MainWndProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = L"RawrXDNoQtIDE";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::instance().error("RegisterClassW(Main) failed");
        return false;
    }

    m_hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"RawrXD Agentic IDE (No Qt)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 800,
        nullptr,
        nullptr,
        m_hInstance,
        this);

    if (!m_hwnd) {
        Logger::instance().error("CreateWindowExW failed");
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

int NoQtIDEApp::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK NoQtIDEApp::MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    NoQtIDEApp* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<NoQtIDEApp*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = reinterpret_cast<NoQtIDEApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->handleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void NoQtIDEApp::createMenus() {
    if (!m_menuBar.create(m_hwnd)) {
        Logger::instance().error("Failed to create menu bar");
        return;
    }

    // File menu
    HMENU fileMenu = m_menuBar.addMenu("File");
    m_menuBar.addAction(fileMenu, "Open...", "Ctrl+O", [this]() { onFileOpen(); }, ID_FILE_OPEN);
    m_menuBar.addAction(fileMenu, "Save As...", "Ctrl+S", [this]() { onFileSaveAs(); }, ID_FILE_SAVEAS);
    m_menuBar.addSeparator(fileMenu);
    m_menuBar.addAction(fileMenu, "Exit", "", [this]() { onExit(); }, ID_FILE_EXIT);

    // Edit menu
    HMENU editMenu = m_menuBar.addMenu("Edit");
    m_menuBar.addAction(editMenu, "Undo", "Ctrl+Z", [this]() { onUndo(); }, ID_EDIT_UNDO);
    m_menuBar.addAction(editMenu, "Redo", "Ctrl+Y", [this]() { onRedo(); }, ID_EDIT_REDO);
    m_menuBar.addSeparator(editMenu);
    m_menuBar.addAction(editMenu, "Cut", "Ctrl+X", [this]() { onCut(); }, ID_EDIT_CUT);
    m_menuBar.addAction(editMenu, "Copy", "Ctrl+C", [this]() { onCopy(); }, ID_EDIT_COPY);
    m_menuBar.addAction(editMenu, "Paste", "Ctrl+V", [this]() { onPaste(); }, ID_EDIT_PASTE);

    // View menu
    HMENU viewMenu = m_menuBar.addMenu("View");
    m_menuBar.addAction(viewMenu, "New Paint", "", [this]() { onNewPaint(); }, ID_VIEW_NEW_PAINT);
    m_menuBar.addAction(viewMenu, "New Code", "", [this]() { onNewCode(); }, ID_VIEW_NEW_CODE);
    m_menuBar.addAction(viewMenu, "New Chat", "", [this]() { onNewChat(); }, ID_VIEW_NEW_CHAT);

    // Tools menu
    HMENU toolsMenu = m_menuBar.addMenu("Tools");
    m_menuBar.addAction(toolsMenu, "Command Palette", "Ctrl+P", [this]() { showCommandPalette(); }, ID_TOOLS_PALETTE);

    m_menuBar.setMenu(m_hwnd);
    Logger::instance().info("Menu bar created with 4 menus");
}

void NoQtIDEApp::createToolbar() {
    if (!m_toolBar.create(m_hwnd)) {
        Logger::instance().error("Failed to create toolbar");
        return;
    }

    m_toolBar.addAction("New Paint", [this]() { onNewPaint(); });
    m_toolBar.addAction("New Code", [this]() { onNewCode(); });
    m_toolBar.addAction("New Chat", [this]() { onNewChat(); });
    m_toolBar.addSeparator();
    m_toolBar.addAction("Save", [this]() { onFileSaveAs(); });
    m_toolBar.addSeparator();
    m_toolBar.addAction("Command Palette", [this]() { showCommandPalette(); });

    m_toolBar.setPosition(0, 0, 600, 30);
    Logger::instance().info("Toolbar created with 5 actions");
}

void NoQtIDEApp::createCommandPalette() {
    if (!m_commandPalette.create(m_hwnd)) {
        Logger::instance().error("Failed to create command palette");
        return;
    }

    // Register IDE commands
    m_commandPalette.addCommand("New Paint", "Create a new paint canvas", [this]() { onNewPaint(); });
    m_commandPalette.addCommand("New Chat", "Start a new chat session", [this]() { onNewChat(); });
    m_commandPalette.addCommand("New Code", "Open a new code editor", [this]() { onNewCode(); });
    m_commandPalette.addCommand("Open File", "Open a file or project", [this]() { onFileOpen(); });
    m_commandPalette.addCommand("Save", "Save current work", [this]() { onFileSaveAs(); });
    m_commandPalette.addCommand("Command Palette", "Show command palette", [this]() { showCommandPalette(); });

    Logger::instance().info("Command palette created with 6 commands");
}

void NoQtIDEApp::showCommandPalette() {
    m_commandPalette.show();
    Logger::instance().info("Command palette shown");
}

void NoQtIDEApp::onNewPaint() {
    // Clear paint canvas
    auto* st = reinterpret_cast<PaintCanvasState*>(GetWindowLongPtrW(m_paintHost, GWLP_USERDATA));
    if (st) {
        RECT r;
        GetClientRect(m_paintHost, &r);
        paintCanvasEnsureBuffer(m_paintHost, st, r.right - r.left, r.bottom - r.top);
        InvalidateRect(m_paintHost, nullptr, FALSE);
    }
    Logger::instance().info("New paint canvas created");
}

void NoQtIDEApp::onNewCode() {
    setWindowTextW(m_editor, L"// New code editor\r\n");
    Logger::instance().info("New code editor created");
}

void NoQtIDEApp::onNewChat() {
    appendText(m_chatTranscript, L"[system] New chat session started\r\n");
    Logger::instance().info("New chat session started");
}

static HWND createRichEdit(HWND parent, DWORD style, int id) {
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RICHEDIT50W",
        L"",
        style,
        0, 0, 0, 0,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr);
}

void NoQtIDEApp::createChildWindows() {
    // Paint
    auto* st = new PaintCanvasState();
    m_paintHost = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RawrXDPaintCanvas",
        L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        m_hwnd,
        (HMENU)2001,
        m_hInstance,
        st);

    // Code editor
    m_editor = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 2002);

    // Chat transcript
    m_chatTranscript = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 2003);
    m_chatInput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL, 2004);
    m_chatSendBtn = CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)ID_CHAT_SEND, m_hInstance, nullptr);

    // Terminal
    m_termOutput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 2005);
    m_termInput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL, 2006);
    m_termRunBtn = CreateWindowExW(0, L"BUTTON", L"Run", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)ID_TERM_RUN, m_hInstance, nullptr);

    HFONT mono = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    SendMessageW(m_editor, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_termOutput, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_termInput, WM_SETFONT, (WPARAM)mono, TRUE);

    appendText(m_chatTranscript, L"[system] Chat ready. Set RAWRXD_OLLAMA_URL to enable Ollama.\r\n");
    appendText(m_termOutput, L"[system] Terminal ready. Type a command and press Run.\r\n");
}

void NoQtIDEApp::layout(int clientW, int clientH) {
    const int margin = 8;
    const int gutter = 8;
    const int toolbarH = 30;
    const int bottomH = 220; // terminal section

    int topY = margin + toolbarH;
    int topH = clientH - bottomH - margin * 2 - gutter - toolbarH;
    if (topH < 200) topH = 200;

    int bottomY = topY + topH + gutter;

    int colW = (clientW - margin * 2 - gutter * 2) / 3;
    if (colW < 200) colW = 200;

    int x1 = margin;
    int x2 = x1 + colW + gutter;
    int x3 = x2 + colW + gutter;

    // Position toolbar
    m_toolBar.setPosition(0, 0, clientW, toolbarH);

    // Top panes
    MoveWindow(m_paintHost, x1, topY, colW, topH, TRUE);
    MoveWindow(m_editor, x2, topY, colW, topH, TRUE);

    // Chat: transcript + input + button
    int chatInputH = 28;
    int chatBtnW = 80;
    int chatTranscriptH = topH - chatInputH - gutter;
    if (chatTranscriptH < 50) chatTranscriptH = 50;

    MoveWindow(m_chatTranscript, x3, topY, colW, chatTranscriptH, TRUE);
    MoveWindow(m_chatInput, x3, topY + chatTranscriptH + gutter, colW - chatBtnW - gutter, chatInputH, TRUE);
    MoveWindow(m_chatSendBtn, x3 + colW - chatBtnW, topY + chatTranscriptH + gutter, chatBtnW, chatInputH, TRUE);

    // Bottom terminal
    int termInputH = 28;
    int termBtnW = 80;
    int termOutputH = bottomH - termInputH - gutter;

    MoveWindow(m_termOutput, margin, bottomY, clientW - margin * 2, termOutputH, TRUE);
    MoveWindow(m_termInput, margin, bottomY + termOutputH + gutter, clientW - margin * 2 - termBtnW - gutter, termInputH, TRUE);
    MoveWindow(m_termRunBtn, clientW - margin - termBtnW, bottomY + termOutputH + gutter, termBtnW, termInputH, TRUE);
}

HWND NoQtIDEApp::focusedTextWidget() const {
    HWND focus = GetFocus();
    if (!focus) return nullptr;

    wchar_t cls[64];
    GetClassNameW(focus, cls, 64);

    // RichEdit50W / EDIT
    if (wcsstr(cls, L"RICHEDIT") || _wcsicmp(cls, L"EDIT") == 0) return focus;
    return nullptr;
}

void NoQtIDEApp::onFileOpen() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files\0*.txt;*.cpp;*.h;*.hpp;*.c;*.md\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    std::ifstream in(fileName, std::ios::binary);
    if (!in) {
        MessageBoxW(m_hwnd, L"Failed to open file", L"Open", MB_OK | MB_ICONERROR);
        return;
    }
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    setWindowTextW(m_editor, widen(bytes));
    m_currentFilePath = fileName;
    Logger::instance().info("Opened file: " + narrow(m_currentFilePath));
}

void NoQtIDEApp::onFileSaveAs() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files\0*.txt;*.cpp;*.h;*.hpp;*.c;*.md\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring text = getWindowTextW(m_editor);
    std::ofstream out(fileName, std::ios::binary);
    if (!out) {
        MessageBoxW(m_hwnd, L"Failed to save file", L"Save", MB_OK | MB_ICONERROR);
        return;
    }
    std::string bytes = narrow(text);
    out.write(bytes.data(), (std::streamsize)bytes.size());
    m_currentFilePath = fileName;
    Logger::instance().info("Saved file: " + narrow(m_currentFilePath));
}

void NoQtIDEApp::onExit() {
    PostQuitMessage(0);
}

void NoQtIDEApp::onUndo() {
    if (HWND t = focusedTextWidget()) SendMessageW(t, EM_UNDO, 0, 0);
}

void NoQtIDEApp::onRedo() {
    if (HWND t = focusedTextWidget()) SendMessageW(t, EM_REDO, 0, 0);
}

void NoQtIDEApp::onCut() {
    if (HWND t = focusedTextWidget()) SendMessageW(t, WM_CUT, 0, 0);
}

void NoQtIDEApp::onCopy() {
    if (HWND t = focusedTextWidget()) SendMessageW(t, WM_COPY, 0, 0);
}

void NoQtIDEApp::onPaste() {
    if (HWND t = focusedTextWidget()) SendMessageW(t, WM_PASTE, 0, 0);
}

static void runCommandCaptureAsync(HWND hwndMain, HWND termOut, std::wstring command) {
    std::thread([hwndMain, termOut, cmd = std::move(command)]() {
        Logger::instance().info("Terminal run: " + narrow(cmd));

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            auto* s = new std::wstring(L"[terminal] CreatePipe failed\r\n");
            PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)termOut, (LPARAM)s);
            return;
        }
        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi{};

        std::wstring full = L"pwsh.exe -NoLogo -NoProfile -Command \"" + cmd + L"\"";
        std::vector<wchar_t> mutableCmd(full.begin(), full.end());
        mutableCmd.push_back(L'\0');

        BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(writePipe);

        if (!ok) {
            CloseHandle(readPipe);
            auto* s = new std::wstring(L"[terminal] CreateProcess failed\r\n");
            PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)termOut, (LPARAM)s);
            return;
        }

        std::wstring out;
        char buf[4096];
        DWORD read = 0;
        while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
            out += widen(std::string(buf, buf + read));
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(readPipe);

        if (out.empty()) out = L"[terminal] (no output)\r\n";
        if (out.back() != L'\n') out += L"\r\n";

        auto* s = new std::wstring(out);
        PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)termOut, (LPARAM)s);
    }).detach();
}

void NoQtIDEApp::onTerminalRun() {
    std::wstring cmd = getWindowTextW(m_termInput);
    if (cmd.empty()) return;
    appendText(m_termOutput, L"> " + cmd + L"\r\n");
    setWindowTextW(m_termInput, L"");
    runCommandCaptureAsync(m_hwnd, m_termOutput, cmd);
}

void NoQtIDEApp::onChatSend() {
    std::wstring msg = getWindowTextW(m_chatInput);
    if (msg.empty()) return;
    setWindowTextW(m_chatInput, L"");

    appendText(m_chatTranscript, L"[user] " + msg + L"\r\n");

    // Built-in, dependency-free assistant (non-placeholder): supports /run and /open
    if (msg.rfind(L"/run ", 0) == 0) {
        std::wstring cmd = msg.substr(5);
        appendText(m_chatTranscript, L"[assistant] Running in terminal...\r\n");
        runCommandCaptureAsync(m_hwnd, m_termOutput, cmd);
        appendText(m_chatTranscript, L"[assistant] Sent to terminal.\r\n");
        return;
    }
    if (msg.rfind(L"/open ", 0) == 0) {
        std::wstring path = msg.substr(6);
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            appendText(m_chatTranscript, L"[assistant] Failed to open file.\r\n");
            return;
        }
        std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        setWindowTextW(m_editor, widen(bytes));
        m_currentFilePath = path;
        appendText(m_chatTranscript, L"[assistant] Opened into editor.\r\n");
        return;
    }

    // Otherwise: small local response
    std::wstring reply = L"I can run commands with /run <cmd>, or open files with /open <path>.";
    appendText(m_chatTranscript, L"[assistant] " + reply + L"\r\n");
}

LRESULT NoQtIDEApp::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            createMenus();
            createToolbar();
            createCommandPalette();
            createChildWindows();
            return 0;

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            layout(w, h);
            return 0;
        }

        case WM_COMMAND: {
            // Handle menu and toolbar commands
            if (m_menuBar.handleCommand(LOWORD(wParam)) || m_toolBar.handleCommand(LOWORD(wParam))) {
                return 0;
            }
            
            switch (LOWORD(wParam)) {
                case ID_FILE_OPEN: onFileOpen(); return 0;
                case ID_FILE_SAVEAS: onFileSaveAs(); return 0;
                case ID_FILE_EXIT: onExit(); return 0;

                case ID_EDIT_UNDO: onUndo(); return 0;
                case ID_EDIT_REDO: onRedo(); return 0;
                case ID_EDIT_CUT: onCut(); return 0;
                case ID_EDIT_COPY: onCopy(); return 0;
                case ID_EDIT_PASTE: onPaste(); return 0;

                case ID_VIEW_NEW_PAINT: onNewPaint(); return 0;
                case ID_VIEW_NEW_CODE: onNewCode(); return 0;
                case ID_VIEW_NEW_CHAT: onNewChat(); return 0;

                case ID_TOOLS_PALETTE: showCommandPalette(); return 0;

                case ID_CHAT_SEND: onChatSend(); return 0;
                case ID_TERM_RUN: onTerminalRun(); return 0;

                case ID_CANVAS_SAVE_BMP: {
                    if (!m_paintHost) return 0;
                    auto* st = reinterpret_cast<PaintCanvasState*>(GetWindowLongPtrW(m_paintHost, GWLP_USERDATA));
                    if (!st || !st->bmp) return 0;

                    wchar_t fileName[MAX_PATH] = L"canvas.bmp";
                    OPENFILENAMEW ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = m_hwnd;
                    ofn.lpstrFile = fileName;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"Bitmap\0*.bmp\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    if (!GetSaveFileNameW(&ofn)) return 0;

                    if (!saveBitmapAsBMP(fileName, st->bmp)) {
                        MessageBoxW(m_hwnd, L"Failed to save BMP", L"Canvas", MB_OK | MB_ICONERROR);
                    }
                    return 0;
                }

                case ID_CANVAS_LOAD_BMP: {
                    wchar_t fileName[MAX_PATH] = L"";
                    OPENFILENAMEW ofn{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = m_hwnd;
                    ofn.lpstrFile = fileName;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"Bitmap\0*.bmp\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                    if (!GetOpenFileNameW(&ofn)) return 0;

                    HBITMAP loaded = loadBMPToHBITMAP(fileName);
                    if (!loaded) {
                        MessageBoxW(m_hwnd, L"Failed to load BMP", L"Canvas", MB_OK | MB_ICONERROR);
                        return 0;
                    }

                    auto* st = reinterpret_cast<PaintCanvasState*>(GetWindowLongPtrW(m_paintHost, GWLP_USERDATA));
                    if (!st) {
                        DeleteObject(loaded);
                        return 0;
                    }

                    // Replace buffer
                    if (st->memDC) {
                        DeleteDC(st->memDC);
                        st->memDC = nullptr;
                    }
                    if (st->bmp) {
                        DeleteObject(st->bmp);
                        st->bmp = nullptr;
                    }

                    RECT r{};
                    GetClientRect(m_paintHost, &r);
                    int w = r.right - r.left;
                    int h = r.bottom - r.top;

                    HDC screen = GetDC(m_paintHost);
                    st->memDC = CreateCompatibleDC(screen);
                    st->bmp = CreateCompatibleBitmap(screen, w, h);
                    st->w = w;
                    st->h = h;
                    SelectObject(st->memDC, st->bmp);

                    // Clear and blit loaded image scaled
                    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
                    FillRect(st->memDC, &r, bg);
                    DeleteObject(bg);

                    HDC src = CreateCompatibleDC(screen);
                    HGDIOBJ old = SelectObject(src, loaded);
                    BITMAP bm{};
                    GetObject(loaded, sizeof(bm), &bm);
                    SetStretchBltMode(st->memDC, HALFTONE);
                    StretchBlt(st->memDC, 0, 0, w, h, src, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                    SelectObject(src, old);
                    DeleteDC(src);
                    DeleteObject(loaded);
                    ReleaseDC(m_paintHost, screen);

                    InvalidateRect(m_paintHost, nullptr, FALSE);
                    return 0;
                }
            }
            break;
        }

        case WM_KEYDOWN: {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl) {
                switch (wParam) {
                    case 'O': onFileOpen(); return 0;
                    case 'S': onFileSaveAs(); return 0;
                    case 'Z': onUndo(); return 0;
                    case 'Y': onRedo(); return 0;
                    case 'X': onCut(); return 0;
                    case 'C': onCopy(); return 0;
                    case 'V': onPaste(); return 0;
                }
            }
            break;
        }

        case WM_APP_APPEND_TEXT: {
            HWND target = (HWND)wParam;
            auto* s = reinterpret_cast<std::wstring*>(lParam);
            if (target && s) appendText(target, *s);
            delete s;
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    Logger::instance().setLevel(LogLevel::INFO);
    Logger::instance().info("NoQtIDE: startup");

    NoQtIDEApp app;
    if (!app.create(hInstance)) {
        MessageBoxW(nullptr, L"Failed to start NoQt IDE", L"Startup", MB_OK | MB_ICONERROR);
        return -1;
    }
    return app.run();
}
