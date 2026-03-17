#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cwctype>
#include <cstdint>

#include <nlohmann/json.hpp>

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

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
static constexpr UINT WM_APP_SET_TEXT = WM_APP + 2;
static constexpr UINT WM_APP_CREATE_EDITOR_TAB = WM_APP + 3;

struct CreateEditorTabPayload {
    std::wstring title;
    std::wstring path;
    std::wstring content;
    bool activate = true;
};

static const wchar_t* g_richEditClass = L"RICHEDIT50W";
static void initRichEditSupport() {
    // Prefer RichEdit 5.0 (Msftedit.dll). Fallback to RichEdit 2.0 (Riched20.dll) then EDIT.
    HMODULE msft = LoadLibraryW(L"Msftedit.dll");
    if (msft) {
        g_richEditClass = L"RICHEDIT50W";
        Logger::instance().info("RichEdit: loaded Msftedit.dll (RICHEDIT50W)");
        return;
    }
    HMODULE riched = LoadLibraryW(L"Riched20.dll");
    if (riched) {
        g_richEditClass = L"RichEdit20W";
        Logger::instance().warn("RichEdit: fallback to Riched20.dll (RichEdit20W)");
        return;
    }
    g_richEditClass = L"EDIT";
    Logger::instance().error("RichEdit: failed to load Msftedit.dll and Riched20.dll; using EDIT fallback");
}

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

static bool jsonGetBool(const nlohmann::json& j, const char* key, bool def = false) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j[key];
    return v.is_boolean() ? v.get<bool>() : def;
}

static std::string jsonGetString(const nlohmann::json& j, const char* key, const std::string& def = {}) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j[key];
    return v.is_string() ? v.get<std::string>() : def;
}

static int jsonGetInt(const nlohmann::json& j, const char* key, int def = 0) {
    if (!j.is_object() || !j.contains(key)) return def;
    const auto& v = j[key];
    return v.is_number() ? v.get<int>() : def;
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

static void initCommonControls() {
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
}

static std::wstring getEnvVarW(const wchar_t* name) {
    DWORD need = GetEnvironmentVariableW(name, nullptr, 0);
    if (need == 0) return L"";
    std::wstring val(need, L'\0');
    DWORD got = GetEnvironmentVariableW(name, val.data(), need);
    if (got == 0) return L"";
    val.resize(got);
    return val;
}

static std::wstring getExeDirW() {
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    std::filesystem::path p(path);
    return p.parent_path().wstring();
}

static std::wstring searchPathForExeW(const std::wstring& exeName) {
    wchar_t buf[MAX_PATH];
    DWORD got = SearchPathW(nullptr, exeName.c_str(), nullptr, MAX_PATH, buf, nullptr);
    if (got == 0 || got >= MAX_PATH) return L"";
    return std::wstring(buf);
}

static std::wstring findNodeExeW() {
    std::wstring explicitNode = getEnvVarW(L"RAWRXD_NODE_PATH");
    if (!explicitNode.empty() && GetFileAttributesW(explicitNode.c_str()) != INVALID_FILE_ATTRIBUTES) return explicitNode;

    std::wstring fromPath = searchPathForExeW(L"node.exe");
    if (!fromPath.empty()) return fromPath;

    std::wstring pf = getEnvVarW(L"ProgramFiles");
    if (!pf.empty()) {
        std::filesystem::path p = std::filesystem::path(pf) / L"nodejs" / L"node.exe";
        if (std::filesystem::exists(p)) return p.wstring();
    }
    std::wstring pfx86 = getEnvVarW(L"ProgramFiles(x86)");
    if (!pfx86.empty()) {
        std::filesystem::path p = std::filesystem::path(pfx86) / L"nodejs" / L"node.exe";
        if (std::filesystem::exists(p)) return p.wstring();
    }
    return L"";
}

static std::wstring findCursorWin32DirW() {
    // Highest priority: explicit env var
    std::wstring explicitDir = getEnvVarW(L"RAWRXD_CURSOR_WIN32_DIR");
    if (!explicitDir.empty()) {
        std::filesystem::path p = explicitDir;
        if (std::filesystem::exists(p / L"orchestration" / L"ide_bridge.js")) return p.wstring();
    }

    // Otherwise: search upward from exe directory (handles build/bin/Release layouts)
    std::filesystem::path start = getExeDirW();
    if (start.empty()) return L"";

    std::filesystem::path p = start;
    for (int i = 0; i < 8; ++i) {
        std::filesystem::path candidate = p / L"cursor-ai-copilot-extension-win32";
        if (std::filesystem::exists(candidate / L"orchestration" / L"ide_bridge.js")) return candidate.wstring();
        if (!p.has_parent_path()) break;
        p = p.parent_path();
    }
    return L"";
}

static std::wstring quoteArgW(const std::wstring& s) {
    // Minimal quoting for CreateProcessW command line.
    if (s.find_first_of(L" \t\"") == std::wstring::npos) return s;
    std::wstring out;
    out.reserve(s.size() + 2);
    out.push_back(L'"');
    for (wchar_t ch : s) {
        if (ch == L'"') out += L"\\\"";
        else out.push_back(ch);
    }
    out.push_back(L'"');
    return out;
}

static std::string detectLanguageFromPathUtf8(const std::wstring& pathW) {
    if (pathW.empty()) return "cpp";
    std::filesystem::path p(pathW);
    std::wstring ext = p.extension().wstring();
    for (auto& c : ext) c = (wchar_t)towlower(c);
    if (ext == L".ts" || ext == L".tsx") return "typescript";
    if (ext == L".js" || ext == L".jsx") return "javascript";
    if (ext == L".py") return "python";
    if (ext == L".cs") return "csharp";
    if (ext == L".c" || ext == L".cc" || ext == L".cxx" || ext == L".cpp" || ext == L".h" || ext == L".hpp") return "cpp";
    return "cpp";
}

static std::wstring truncateW(std::wstring s, size_t maxChars) {
    if (s.size() <= maxChars) return s;
    s.resize(maxChars);
    s += L"\r\n[...truncated...]\r\n";
    return s;
}

static std::vector<uint8_t> base64Decode(const std::string& b64) {
    static const int8_t kDec[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4);

    uint32_t val = 0;
    int valb = -8;
    for (unsigned char c : b64) {
        int8_t d = kDec[c];
        if (d == -1) continue;      // skip whitespace/unknown
        if (d == -2) break;         // '=' padding
        val = (val << 6) | (uint32_t)d;
        valb += 6;
        if (valb >= 0) {
            out.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static HBITMAP loadPngBytesToHBITMAP(const std::vector<uint8_t>& pngBytes) {
    if (pngBytes.empty()) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, pngBytes.size());
    if (!hMem) return nullptr;
    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        return nullptr;
    }
    memcpy(pMem, pngBytes.data(), pngBytes.size());
    GlobalUnlock(hMem);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &stream) != S_OK) {
        GlobalFree(hMem);
        return nullptr;
    }

    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        stream->Release();
        return nullptr;
    }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    stream->Release();
    if (FAILED(hr)) {
        factory->Release();
        return nullptr;
    }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr)) {
        factory->Release();
        return nullptr;
    }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0) {
        frame->Release();
        factory->Release();
        return nullptr;
    }

    IWICFormatConverter* conv = nullptr;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr)) {
        frame->Release();
        factory->Release();
        return nullptr;
    }

    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    frame->Release();
    if (FAILED(hr)) {
        conv->Release();
        factory->Release();
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -(LONG)h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        if (hbmp) DeleteObject(hbmp);
        conv->Release();
        factory->Release();
        return nullptr;
    }

    const UINT stride = w * 4;
    const UINT bufSize = stride * h;
    hr = conv->CopyPixels(nullptr, stride, bufSize, (BYTE*)bits);
    conv->Release();
    factory->Release();
    if (FAILED(hr)) {
        DeleteObject(hbmp);
        return nullptr;
    }

    return hbmp;
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
    ID_VIEW_MAXMODE,
    ID_TOOLS_PALETTE,
    ID_CANVAS_SAVE_BMP,
    ID_CANVAS_LOAD_BMP,
    ID_CHAT_SEND,
    ID_TERM_RUN,
    ID_ORCH_RUN,
};

bool NoQtIDEApp::create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    initRichEditSupport();
    initCommonControls();

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
        if (self) {
            self->m_hwnd = hwnd; // Ensure a valid parent handle during WM_CREATE
        }
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
    m_menuBar.addAction(viewMenu, "New Paint Window", "", [this]() { onNewPaint(); }, ID_VIEW_NEW_PAINT);
    m_menuBar.addAction(viewMenu, "New Code Tab", "Ctrl+T", [this]() { onNewCode(); }, ID_VIEW_NEW_CODE);
    m_menuBar.addAction(viewMenu, "New Chat Tab", "", [this]() { onNewChat(); }, ID_VIEW_NEW_CHAT);
    m_menuBar.addSeparator(viewMenu);
    m_menuBar.addAction(viewMenu, "Max Mode (Focus Editor)", "F11", [this]() { toggleMaxMode(); }, ID_VIEW_MAXMODE);

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
    m_toolBar.addAction("New Tab", [this]() { onNewCode(); });
    m_toolBar.addAction("New Chat", [this]() { onNewChat(); });
    m_toolBar.addSeparator();
    m_toolBar.addAction("Save", [this]() { onFileSaveAs(); });
    m_toolBar.addSeparator();
    m_toolBar.addAction("Max Mode", [this]() { toggleMaxMode(); });
    m_toolBar.addAction("Command Palette", [this]() { showCommandPalette(); });

    m_toolBar.setPosition(0, 0, 600, 30);
    Logger::instance().info("Toolbar created with 6 actions");
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
    // Open a separate paint window (keeps main IDE layout file-tree/editor/chat-focused).
    HWND w = CreateWindowExW(
        0,
        L"RawrXDNoQtIDE",
        L"RawrXD Paint",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 700,
        nullptr,
        nullptr,
        m_hInstance,
        nullptr);
    if (!w) return;

    auto* st = new PaintCanvasState();
    HWND canvas = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RawrXDPaintCanvas",
        L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        w,
        (HMENU)(INT_PTR)5001,
        m_hInstance,
        st);

    if (!canvas) {
        delete st;
        DestroyWindow(w);
        return;
    }

    RECT r{};
    GetClientRect(w, &r);
    MoveWindow(canvas, 0, 0, r.right - r.left, r.bottom - r.top, TRUE);

    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    Logger::instance().info("Opened paint window");
}

void NoQtIDEApp::onNewCode() {
    editorAddTab(L"untitled.cpp", L"", L"// New file\r\n", true);
    Logger::instance().info("New code tab created");
}

void NoQtIDEApp::onNewChat() {
    std::wstring title = L"chat-" + std::to_wstring((int)m_chatSessions.size() + 1);
    chatAddTab(title, true);
    appendText(m_chatTranscript, L"[system] New chat session started\r\n");
    Logger::instance().info("New chat tab created");
}

static HWND createRichEdit(HWND parent, DWORD style, int id) {
    SetLastError(0);
    HWND hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        g_richEditClass,
        L"",
        style,
        0, 0, 0, 0,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr);
    if (!hwnd) {
        const DWORD err = GetLastError();
        std::ostringstream oss;
        std::wstring cls = g_richEditClass;
        oss << "CreateWindowExW(";
        oss << narrow(cls);
        oss << ") failed id=" << id << " err=" << err;
        Logger::instance().error(oss.str());

        if (wcscmp(g_richEditClass, L"EDIT") != 0) {
            SetLastError(0);
            hwnd = CreateWindowExW(
                WS_EX_CLIENTEDGE,
                L"EDIT",
                L"",
                style,
                0, 0, 0, 0,
                parent,
                (HMENU)(INT_PTR)id,
                GetModuleHandleW(nullptr),
                nullptr);
            if (!hwnd) {
                Logger::instance().error("CreateWindowExW(EDIT) fallback also failed");
            } else {
                Logger::instance().warn("RichEdit: using EDIT control fallback");
            }
        }
    }
    return hwnd;
}

static HWND createTabControl(HWND parent, int id) {
    return CreateWindowExW(
        0,
        WC_TABCONTROLW,
        L"",
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 0, 0,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr);
}

static HWND createTreeView(HWND parent, int id) {
    return CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
        0, 0, 0, 0,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr);
}

static HWND createCombo(HWND parent, int id) {
    return CreateWindowExW(
        0,
        WC_COMBOBOXW,
        L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0,
        parent,
        (HMENU)(INT_PTR)id,
        GetModuleHandleW(nullptr),
        nullptr);
}

void NoQtIDEApp::createChildWindows() {
    // Left: file browser tree (all drives)
    m_fileTree = createTreeView(m_hwnd, 2100);

    // Middle: editor tabs + editor
    m_editorTabs = createTabControl(m_hwnd, 2101);
    m_editor = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 2102);

    // Right: chat tabs + transcript/input
    m_chatTabs = createTabControl(m_hwnd, 2201);
    m_chatTranscript = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 2202);
    m_chatInput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL, 2203);
    m_chatSendBtn = CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)ID_CHAT_SEND, m_hInstance, nullptr);

    // Bottom-left: terminal
    m_termOutput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 2301);
    m_termInput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL, 2302);
    m_termRunBtn = CreateWindowExW(0, L"BUTTON", L"Run", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)ID_TERM_RUN, m_hInstance, nullptr);

    // Bottom-right: orchestra (objective -> plan/code via models)
    m_orchModelCombo = createCombo(m_hwnd, 2401);
    m_orchMaxToggle = CreateWindowExW(0, L"BUTTON", L"Max", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, m_hwnd, (HMENU)ID_VIEW_MAXMODE, m_hInstance, nullptr);
    m_orchInput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | ES_AUTOVSCROLL, 2402);
    m_orchRunBtn = CreateWindowExW(0, L"BUTTON", L"Orchestrate", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hwnd, (HMENU)ID_ORCH_RUN, m_hInstance, nullptr);
    m_orchOutput = createRichEdit(m_hwnd, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 2403);

    HFONT mono = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    SendMessageW(m_editor, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_termOutput, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_termInput, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_chatTranscript, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_orchInput, WM_SETFONT, (WPARAM)mono, TRUE);
    SendMessageW(m_orchOutput, WM_SETFONT, (WPARAM)mono, TRUE);

    setWindowTextW(m_chatTranscript, L"[system] Chat ready. Use /cursor <objective> to invoke the agent.\r\n");
    setWindowTextW(m_termOutput, L"[system] Terminal ready. Type a command and press Run.\r\n");
    setWindowTextW(m_orchOutput, L"[system] Orchestra ready. Set OPENAI_API_KEY and choose a model.\r\n");

    // Populate model dropdown
    if (m_orchModelCombo) {
        SendMessageW(m_orchModelCombo, CB_ADDSTRING, 0, (LPARAM)L"gpt-5.2-pro");
        SendMessageW(m_orchModelCombo, CB_ADDSTRING, 0, (LPARAM)L"gpt-5.1-codex-max");
        SendMessageW(m_orchModelCombo, CB_ADDSTRING, 0, (LPARAM)L"gpt-4o-mini");
        SendMessageW(m_orchModelCombo, CB_ADDSTRING, 0, (LPARAM)L"o3-mini");
        SendMessageW(m_orchModelCombo, CB_SETCURSEL, 0, 0);
    }

    fileBrowserPopulateRoots();
    editorEnsureInitialTab();
    chatEnsureInitialTab();

    if (!m_fileTree && !m_editor && !m_chatTranscript && !m_termOutput) {
        Logger::instance().error("UI: no child windows created; check RichEdit/CommonControls availability");
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void NoQtIDEApp::layout(int clientW, int clientH) {
    const int margin = 8;
    const int gutter = 8;
    const int toolbarH = 30;
    const int bottomH = 260; // terminal + orchestra

    int topY = margin + toolbarH;
    int topH = clientH - bottomH - margin * 2 - gutter - toolbarH;
    if (topH < 200) topH = 200;

    int bottomY = topY + topH + gutter;

    // Position toolbar
    m_toolBar.setPosition(0, 0, clientW, toolbarH);

    if (m_maxMode) {
        // Focus editor only
        ShowWindow(m_fileTree, SW_HIDE);
        ShowWindow(m_chatTabs, SW_HIDE);
        ShowWindow(m_chatTranscript, SW_HIDE);
        ShowWindow(m_chatInput, SW_HIDE);
        ShowWindow(m_chatSendBtn, SW_HIDE);
        ShowWindow(m_termOutput, SW_HIDE);
        ShowWindow(m_termInput, SW_HIDE);
        ShowWindow(m_termRunBtn, SW_HIDE);
        ShowWindow(m_orchModelCombo, SW_HIDE);
        ShowWindow(m_orchMaxToggle, SW_HIDE);
        ShowWindow(m_orchInput, SW_HIDE);
        ShowWindow(m_orchRunBtn, SW_HIDE);
        ShowWindow(m_orchOutput, SW_HIDE);

        ShowWindow(m_editorTabs, SW_SHOW);
        ShowWindow(m_editor, SW_SHOW);

        MoveWindow(m_editorTabs, margin, topY, clientW - margin * 2, 28, TRUE);
        MoveWindow(m_editor, margin, topY + 28 + gutter, clientW - margin * 2, clientH - (topY + 28 + gutter) - margin, TRUE);
        return;
    }

    // 3-column top: file tree | editor | chat
    int leftW = (clientW - margin * 2 - gutter * 2) * 22 / 100;
    int rightW = (clientW - margin * 2 - gutter * 2) * 28 / 100;
    int midW = (clientW - margin * 2 - gutter * 2) - leftW - rightW;
    if (leftW < 220) leftW = 220;
    if (rightW < 260) rightW = 260;
    if (midW < 320) midW = 320;

    int x1 = margin;
    int x2 = x1 + leftW + gutter;
    int x3 = x2 + midW + gutter;

    // Ensure visible
    ShowWindow(m_fileTree, SW_SHOW);
    ShowWindow(m_editorTabs, SW_SHOW);
    ShowWindow(m_editor, SW_SHOW);
    ShowWindow(m_chatTabs, SW_SHOW);
    ShowWindow(m_chatTranscript, SW_SHOW);
    ShowWindow(m_chatInput, SW_SHOW);
    ShowWindow(m_chatSendBtn, SW_SHOW);
    ShowWindow(m_termOutput, SW_SHOW);
    ShowWindow(m_termInput, SW_SHOW);
    ShowWindow(m_termRunBtn, SW_SHOW);
    ShowWindow(m_orchModelCombo, SW_SHOW);
    ShowWindow(m_orchMaxToggle, SW_SHOW);
    ShowWindow(m_orchInput, SW_SHOW);
    ShowWindow(m_orchRunBtn, SW_SHOW);
    ShowWindow(m_orchOutput, SW_SHOW);

    // Left: file tree
    MoveWindow(m_fileTree, x1, topY, leftW, topH, TRUE);

    // Middle: editor tabs + editor
    const int tabsH = 28;
    MoveWindow(m_editorTabs, x2, topY, midW, tabsH, TRUE);
    MoveWindow(m_editor, x2, topY + tabsH + gutter, midW, topH - tabsH - gutter, TRUE);

    // Right: chat tabs + transcript + input + button
    const int chatTabsH = 28;
    const int chatInputH = 28;
    const int chatBtnW = 90;

    int chatBodyY = topY + chatTabsH + gutter;
    int chatBodyH = topH - chatTabsH - gutter;
    int chatTranscriptH = chatBodyH - chatInputH - gutter;
    if (chatTranscriptH < 80) chatTranscriptH = 80;

    MoveWindow(m_chatTabs, x3, topY, rightW, chatTabsH, TRUE);
    MoveWindow(m_chatTranscript, x3, chatBodyY, rightW, chatTranscriptH, TRUE);
    MoveWindow(m_chatInput, x3, chatBodyY + chatTranscriptH + gutter, rightW - chatBtnW - gutter, chatInputH, TRUE);
    MoveWindow(m_chatSendBtn, x3 + rightW - chatBtnW, chatBodyY + chatTranscriptH + gutter, chatBtnW, chatInputH, TRUE);

    // Bottom: split terminal | orchestra
    int bottomW = clientW - margin * 2 - gutter;
    int termW = bottomW * 60 / 100;
    int orchW = bottomW - termW;

    int bx1 = margin;
    int bx2 = margin + termW + gutter;

    // Terminal layout (output + input + run)
    const int termInputH = 28;
    const int termBtnW = 80;
    int termOutputH = bottomH - termInputH - gutter;
    MoveWindow(m_termOutput, bx1, bottomY, termW, termOutputH, TRUE);
    MoveWindow(m_termInput, bx1, bottomY + termOutputH + gutter, termW - termBtnW - gutter, termInputH, TRUE);
    MoveWindow(m_termRunBtn, bx1 + termW - termBtnW, bottomY + termOutputH + gutter, termBtnW, termInputH, TRUE);

    // Orchestra layout
    const int topRowH = 26;
    const int runW = 110;
    const int maxW = 60;
    MoveWindow(m_orchModelCombo, bx2, bottomY, orchW - runW - gutter - maxW - gutter, topRowH, TRUE);
    MoveWindow(m_orchMaxToggle, bx2 + (orchW - runW - gutter - maxW - gutter) + gutter, bottomY, maxW, topRowH, TRUE);
    MoveWindow(m_orchRunBtn, bx2 + orchW - runW, bottomY, runW, topRowH, TRUE);

    const int orchInputH = 54;
    MoveWindow(m_orchInput, bx2, bottomY + topRowH + gutter, orchW, orchInputH, TRUE);
    MoveWindow(m_orchOutput, bx2, bottomY + topRowH + gutter + orchInputH + gutter, orchW, bottomH - topRowH - orchInputH - gutter * 2, TRUE);
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
    std::filesystem::path fp = fileName;
    editorAddTab(fp.filename().wstring(), fileName, widen(bytes), true);
    Logger::instance().info("Opened file: " + narrow(std::wstring(fileName)));
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
    if (m_activeEditorTab >= 0 && m_activeEditorTab < (int)m_editorDocs.size()) {
        m_editorDocs[m_activeEditorTab].path = m_currentFilePath;
        m_editorDocs[m_activeEditorTab].title = std::filesystem::path(m_currentFilePath).filename().wstring();
        TCITEMW ti{};
        ti.mask = TCIF_TEXT;
        ti.pszText = const_cast<wchar_t*>(m_editorDocs[m_activeEditorTab].title.c_str());
        TabCtrl_SetItem(m_editorTabs, m_activeEditorTab, &ti);
    }
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

void NoQtIDEApp::onOrchestraRun() {
    std::wstring objective = getWindowTextW(m_orchInput);
    if (objective.empty()) return;
    setWindowTextW(m_orchInput, L"");

    int sel = m_orchModelCombo ? (int)SendMessageW(m_orchModelCombo, CB_GETCURSEL, 0, 0) : -1;
    std::wstring model;
    if (sel >= 0 && m_orchModelCombo) {
        wchar_t buf[256];
        SendMessageW(m_orchModelCombo, CB_GETLBTEXT, sel, (LPARAM)buf);
        model = buf;
    }

    appendText(m_orchOutput, L"\r\n> " + objective + L"\r\n");
    appendText(m_orchOutput, L"[orchestra] Running... (set OPENAI_API_KEY)\r\n");

    std::wstring nodeExe = findNodeExeW();
    std::wstring cursorDir = findCursorWin32DirW();
    if (nodeExe.empty() || cursorDir.empty()) {
        appendText(m_orchOutput, L"[orchestra] Missing node.exe or cursor-ai-copilot-extension-win32.\r\n");
        return;
    }

    HWND hwndMain = m_hwnd;
    HWND outCtrl = m_orchOutput;
    HWND editor = m_editor;
    std::wstring objectiveCopy = objective;
    std::wstring modelCopy = model;
    std::wstring nodeExeCopy = nodeExe;
    std::wstring cursorDirCopy = cursorDir;
    std::wstring filePath = m_currentFilePath;
    std::wstring editorText = truncateW(getWindowTextW(m_editor), 8000);

    std::thread([hwndMain, outCtrl, editor, objectiveCopy = std::move(objectiveCopy), modelCopy = std::move(modelCopy),
                    nodeExeCopy = std::move(nodeExeCopy), cursorDirCopy = std::move(cursorDirCopy),
                    filePath = std::move(filePath), editorText = std::move(editorText)]() mutable {
        try {
            std::filesystem::path bridgeJs = std::filesystem::path(cursorDirCopy) / L"orchestration" / L"ide_bridge.js";
            if (!std::filesystem::exists(bridgeJs)) {
                auto* s = new std::wstring(L"[orchestra] ide_bridge.js not found.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                return;
            }

            std::vector<std::string> contextBlocks;
            if (!filePath.empty()) contextBlocks.push_back("Current file: " + narrow(filePath));
            if (!editorText.empty()) contextBlocks.push_back("Current editor content (truncated):\n" + narrow(editorText));

            std::string lang = detectLanguageFromPathUtf8(filePath);

            wchar_t tempDir[MAX_PATH];
            wchar_t tempFile[MAX_PATH];
            if (!GetTempPathW(MAX_PATH, tempDir) || !GetTempFileNameW(tempDir, L"rxd", 0, tempFile)) {
                auto* s = new std::wstring(L"[orchestra] Failed to create temp request file.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                return;
            }
            std::wstring reqPath = tempFile;

            using json = nlohmann::json;
            json req;
            req["objective"] = narrow(objectiveCopy);
            req["language"] = lang;
            auto& ctxArr = req["contextBlocks"];
            ctxArr = nlohmann::json::parse("[]");
            for (const auto& block : contextBlocks) ctxArr.push_back(block);
            if (!modelCopy.empty()) req["model"] = narrow(modelCopy);

            {
                std::ofstream out(reqPath, std::ios::binary);
                std::string bytes = req.dump();
                out.write(bytes.data(), (std::streamsize)bytes.size());
            }

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
                auto* s = new std::wstring(L"[orchestra] CreatePipe failed\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                DeleteFileW(reqPath.c_str());
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

            std::wstring cmdLine =
                quoteArgW(nodeExeCopy) + L" " +
                quoteArgW(bridgeJs.wstring()) + L" --request " +
                quoteArgW(reqPath);

            std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
            mutableCmd.push_back(L'\0');

            BOOL ok = CreateProcessW(
                nullptr,
                mutableCmd.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                cursorDirCopy.c_str(),
                &si,
                &pi);

            CloseHandle(writePipe);
            if (!ok) {
                CloseHandle(readPipe);
                DeleteFileW(reqPath.c_str());
                auto* s = new std::wstring(L"[orchestra] CreateProcess failed (node).\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                return;
            }

            std::wstring outW;
            char buf[4096];
            DWORD read = 0;
            while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
                outW += widen(std::string(buf, buf + read));
            }

            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            CloseHandle(readPipe);
            DeleteFileW(reqPath.c_str());

            using json = nlohmann::json;
            json resp;
            std::string outUtf8 = narrow(outW);
            try {
                resp = json::parse(outUtf8);
            } catch (...) {
                std::wstring msg = L"[orchestra] Non-JSON response (exit ";
                msg += std::to_wstring(exitCode);
                msg += L"):\r\n";
                msg += outW;
                msg += L"\r\n";
                auto* s = new std::wstring(std::move(msg));
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                return;
            }

            bool okResp = jsonGetBool(resp, "ok", false);
            if (!okResp) {
                std::wstring msg = L"[orchestra] Error: ";
                msg += widen(jsonGetString(resp, "error", "Unknown error"));
                msg += L"\r\n";
                auto* s = new std::wstring(std::move(msg));
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
                return;
            }

            const json& result = resp["result"];
            std::string status = jsonGetString(result, "status", "");
            int duration = jsonGetInt(result, "duration", 0);
            std::string code = jsonGetString(result, "code", "");

            std::wstring header = L"[orchestra] " + widen(status) + L" in " + std::to_wstring(duration) + L"ms\r\n";
            auto* s1 = new std::wstring(std::move(header));
            PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s1);

            // Print plan summary if present
            if (result.contains("plan")) {
                try {
                    auto steps = result["plan"]["steps"];
                    if (steps.is_array()) {
                        std::wstring p = L"[plan]\r\n";
                        int i = 0;
                        for (size_t idx = 0; idx < steps.size(); ++idx) {
                            const auto& st = steps[idx];
                            std::string action = jsonGetString(st, "action", "");
                            p += L"  " + std::to_wstring(++i) + L". " + widen(action) + L"\r\n";
                        }
                        auto* s2 = new std::wstring(std::move(p));
                        PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s2);
                    }
                } catch (...) {
                }
            }

            if (!code.empty()) {
                auto* s3 = new std::wstring(L"[code] Inserted into editor.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s3);
                auto* s4 = new std::wstring(widen(code));
                PostMessageW(hwndMain, WM_APP_SET_TEXT, (WPARAM)editor, (LPARAM)s4);
            }
        } catch (...) {
            auto* s = new std::wstring(L"[orchestra] Unexpected error.\r\n");
            PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)outCtrl, (LPARAM)s);
        }
    }).detach();
}

void NoQtIDEApp::onChatSend() {
    std::wstring msg = getWindowTextW(m_chatInput);
    if (msg.empty()) return;
    setWindowTextW(m_chatInput, L"");

    appendText(m_chatTranscript, L"[user] " + msg + L"\r\n");

    // Built-in assistant: supports /paintgen, /cursor, /run and /open
    if (msg.rfind(L"/paintgen ", 0) == 0) {
        std::wstring prompt = msg.substr(10);
        if (prompt.empty()) {
            appendText(m_chatTranscript, L"[assistant] Usage: /paintgen <prompt>\r\n");
            return;
        }

        std::wstring nodeExe = findNodeExeW();
        std::wstring cursorDir = findCursorWin32DirW();
        if (nodeExe.empty() || cursorDir.empty()) {
            appendText(m_chatTranscript, L"[assistant] Paint generation requires node.exe and cursor-ai-copilot-extension-win32.\r\n");
            return;
        }

        appendText(m_chatTranscript, L"[assistant] Generating image... (set OPENAI_API_KEY)\r\n");

        HWND hwndMain = m_hwnd;
        HWND chatOut = m_chatTranscript;
        std::wstring promptCopy = prompt;
        std::wstring nodeExeCopy = nodeExe;
        std::wstring cursorDirCopy = cursorDir;

        std::thread([hwndMain, chatOut, promptCopy = std::move(promptCopy), nodeExeCopy = std::move(nodeExeCopy), cursorDirCopy = std::move(cursorDirCopy)]() mutable {
            try {
                std::filesystem::path bridgeJs = std::filesystem::path(cursorDirCopy) / L"orchestration" / L"ide_image_bridge.js";
                if (!std::filesystem::exists(bridgeJs)) {
                    auto* s = new std::wstring(L"[paint] ide_image_bridge.js not found.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                // Write request JSON to a temp file
                wchar_t tempDir[MAX_PATH];
                wchar_t tempFile[MAX_PATH];
                if (!GetTempPathW(MAX_PATH, tempDir) || !GetTempFileNameW(tempDir, L"rxd", 0, tempFile)) {
                    auto* s = new std::wstring(L"[paint] Failed to create temp request file.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }
                std::wstring reqPath = tempFile;

                using json = nlohmann::json;
                json req;
                req["prompt"] = narrow(promptCopy);
                req["size"] = "1024x1024";

                {
                    std::ofstream out(reqPath, std::ios::binary);
                    std::string bytes = req.dump();
                    out.write(bytes.data(), (std::streamsize)bytes.size());
                }

                SECURITY_ATTRIBUTES sa{};
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;

                HANDLE readPipe = nullptr;
                HANDLE writePipe = nullptr;
                if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
                    auto* s = new std::wstring(L"[paint] CreatePipe failed\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    DeleteFileW(reqPath.c_str());
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

                std::wstring cmdLine =
                    quoteArgW(nodeExeCopy) + L" " +
                    quoteArgW(bridgeJs.wstring()) + L" --request " +
                    quoteArgW(reqPath);

                std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
                mutableCmd.push_back(L'\0');

                BOOL ok = CreateProcessW(
                    nullptr,
                    mutableCmd.data(),
                    nullptr,
                    nullptr,
                    TRUE,
                    CREATE_NO_WINDOW,
                    nullptr,
                    cursorDirCopy.c_str(),
                    &si,
                    &pi);

                CloseHandle(writePipe);

                if (!ok) {
                    CloseHandle(readPipe);
                    DeleteFileW(reqPath.c_str());
                    auto* s = new std::wstring(L"[paint] CreateProcess failed (node).\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                std::wstring outW;
                char buf[4096];
                DWORD read = 0;
                while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
                    outW += widen(std::string(buf, buf + read));
                }

                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                CloseHandle(readPipe);
                DeleteFileW(reqPath.c_str());

                using json = nlohmann::json;
                json resp;
                try {
                    resp = json::parse(narrow(outW));
                } catch (...) {
                    std::wstring msg = L"[paint] Non-JSON response:\r\n";
                    msg += outW;
                    msg += L"\r\n";
                    auto* s = new std::wstring(std::move(msg));
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                bool okResp = jsonGetBool(resp, "ok", false);
                if (!okResp) {
                    std::wstring msg = L"[paint] Error: ";
                    msg += widen(jsonGetString(resp, "error", "Unknown error"));
                    msg += L"\r\n";
                    auto* s = new std::wstring(std::move(msg));
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                std::string b64 = jsonGetString(resp, "pngBase64", "");
                auto pngBytes = base64Decode(b64);
                HBITMAP hbmp = loadPngBytesToHBITMAP(pngBytes);
                if (!hbmp) {
                    auto* s = new std::wstring(L"[paint] Failed to decode PNG.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                // Open paint window and set background to generated image (scaled to window size)
                HWND w = CreateWindowExW(
                    0,
                    L"RawrXDNoQtIDE",
                    L"RawrXD Paint (AI)",
                    WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT, CW_USEDEFAULT,
                    900, 700,
                    nullptr,
                    nullptr,
                    GetModuleHandleW(nullptr),
                    nullptr);
                if (!w) {
                    DeleteObject(hbmp);
                    auto* s = new std::wstring(L"[paint] Failed to open paint window.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                auto* st = new PaintCanvasState();
                HWND canvas = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"RawrXDPaintCanvas",
                    L"",
                    WS_CHILD | WS_VISIBLE,
                    0, 0, 0, 0,
                    w,
                    (HMENU)(INT_PTR)5002,
                    GetModuleHandleW(nullptr),
                    st);

                if (!canvas) {
                    delete st;
                    DeleteObject(hbmp);
                    DestroyWindow(w);
                    auto* s = new std::wstring(L"[paint] Failed to create canvas.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                RECT r{};
                GetClientRect(w, &r);
                MoveWindow(canvas, 0, 0, r.right - r.left, r.bottom - r.top, TRUE);

                // Load generated bitmap into the paint buffer (scaled)
                HDC screen = GetDC(canvas);
                st->memDC = CreateCompatibleDC(screen);
                st->bmp = CreateCompatibleBitmap(screen, r.right - r.left, r.bottom - r.top);
                st->w = r.right - r.left;
                st->h = r.bottom - r.top;
                SelectObject(st->memDC, st->bmp);

                HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(st->memDC, &r, bg);
                DeleteObject(bg);

                HDC src = CreateCompatibleDC(screen);
                HGDIOBJ old = SelectObject(src, hbmp);
                BITMAP bm{};
                GetObject(hbmp, sizeof(bm), &bm);
                SetStretchBltMode(st->memDC, HALFTONE);
                StretchBlt(st->memDC, 0, 0, st->w, st->h, src, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
                SelectObject(src, old);
                DeleteDC(src);
                DeleteObject(hbmp);
                ReleaseDC(canvas, screen);

                ShowWindow(w, SW_SHOW);
                UpdateWindow(w);
                InvalidateRect(canvas, nullptr, FALSE);

                auto* s = new std::wstring(L"[assistant] Image generated in a new paint window.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
            } catch (...) {
                auto* s = new std::wstring(L"[paint] Unexpected error.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
            }
        }).detach();
        return;
    }

    if (msg.rfind(L"/cursor ", 0) == 0) {
        std::wstring objective = msg.substr(8);
        if (objective.empty()) {
            appendText(m_chatTranscript, L"[assistant] Usage: /cursor <objective>\r\n");
            return;
        }

        std::wstring nodeExe = findNodeExeW();
        std::wstring cursorDir = findCursorWin32DirW();
        if (nodeExe.empty()) {
            appendText(m_chatTranscript, L"[assistant] Cursor integration: node.exe not found. Install Node.js or set RAWRXD_NODE_PATH.\r\n");
            return;
        }
        if (cursorDir.empty()) {
            appendText(m_chatTranscript, L"[assistant] Cursor integration: cursor-ai-copilot-extension-win32 not found. Set RAWRXD_CURSOR_WIN32_DIR.\r\n");
            return;
        }

        std::wstring filePath = m_currentFilePath;
        std::wstring editorText = truncateW(getWindowTextW(m_editor), 8000);
        std::string lang = detectLanguageFromPathUtf8(filePath);

        std::vector<std::string> contextBlocks;
        if (!filePath.empty()) {
            contextBlocks.push_back("Current file: " + narrow(filePath));
        }
        if (!editorText.empty()) {
            contextBlocks.push_back("Current editor content (truncated):\n" + narrow(editorText));
        }

        appendText(m_chatTranscript, L"[assistant] Cursor agent running... (set OPENAI_API_KEY)\r\n");

        HWND hwndMain = m_hwnd;
        HWND chatOut = m_chatTranscript;
        HWND editor = m_editor;
        std::wstring objectiveCopy = objective;
        std::string langCopy = lang;
        std::wstring nodeExeCopy = nodeExe;
        std::wstring cursorDirCopy = cursorDir;
        std::vector<std::string> contextCopy = std::move(contextBlocks);

        std::thread([hwndMain, chatOut, editor, objectiveCopy = std::move(objectiveCopy), langCopy = std::move(langCopy),
                        nodeExeCopy = std::move(nodeExeCopy), cursorDirCopy = std::move(cursorDirCopy), contextCopy = std::move(contextCopy)]() mutable {
            try {
                std::filesystem::path bridgeJs = std::filesystem::path(cursorDirCopy) / L"orchestration" / L"ide_bridge.js";
                if (!std::filesystem::exists(bridgeJs)) {
                    auto* s = new std::wstring(L"[cursor] ide_bridge.js not found.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                // Write request JSON to a temp file
                wchar_t tempDir[MAX_PATH];
                wchar_t tempFile[MAX_PATH];
                if (!GetTempPathW(MAX_PATH, tempDir) || !GetTempFileNameW(tempDir, L"rxd", 0, tempFile)) {
                    auto* s = new std::wstring(L"[cursor] Failed to create temp request file.\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }
                std::wstring reqPath = tempFile;

                using json = nlohmann::json;
                json req;
                req["objective"] = narrow(objectiveCopy);
                req["language"] = langCopy;
                auto& ctxArr = req["contextBlocks"];
                ctxArr = nlohmann::json::parse("[]");
                for (const auto& block : contextCopy) ctxArr.push_back(block);

                {
                    std::ofstream out(reqPath, std::ios::binary);
                    std::string bytes = req.dump();
                    out.write(bytes.data(), (std::streamsize)bytes.size());
                }

                // Spawn node bridge and capture JSON response
                SECURITY_ATTRIBUTES sa{};
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;

                HANDLE readPipe = nullptr;
                HANDLE writePipe = nullptr;
                if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
                    auto* s = new std::wstring(L"[cursor] CreatePipe failed\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    DeleteFileW(reqPath.c_str());
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

                std::wstring cmdLine =
                    quoteArgW(nodeExeCopy) + L" " +
                    quoteArgW(bridgeJs.wstring()) + L" --request " +
                    quoteArgW(reqPath);

                std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
                mutableCmd.push_back(L'\0');

                BOOL ok = CreateProcessW(
                    nullptr,
                    mutableCmd.data(),
                    nullptr,
                    nullptr,
                    TRUE,
                    CREATE_NO_WINDOW,
                    nullptr,
                    cursorDirCopy.c_str(),
                    &si,
                    &pi);

                CloseHandle(writePipe);

                if (!ok) {
                    CloseHandle(readPipe);
                    DeleteFileW(reqPath.c_str());
                    auto* s = new std::wstring(L"[cursor] CreateProcess failed (node).\r\n");
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                std::wstring outW;
                char buf[4096];
                DWORD read = 0;
                while (ReadFile(readPipe, buf, sizeof(buf), &read, nullptr) && read > 0) {
                    outW += widen(std::string(buf, buf + read));
                }

                WaitForSingleObject(pi.hProcess, INFINITE);
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                CloseHandle(readPipe);
                DeleteFileW(reqPath.c_str());

                std::string outUtf8 = narrow(outW);
                using json = nlohmann::json;
                json resp;
                try {
                    resp = json::parse(outUtf8);
                } catch (...) {
                    std::wstring msg = L"[cursor] Non-JSON response (exit ";
                    msg += std::to_wstring(exitCode);
                    msg += L"):\r\n";
                    msg += outW;
                    msg += L"\r\n";
                    auto* s = new std::wstring(std::move(msg));
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                bool okResp = jsonGetBool(resp, "ok", false);
                if (!okResp) {
                    std::string err = jsonGetString(resp, "error", "Unknown error");
                    std::wstring msg = L"[cursor] Error: ";
                    msg += widen(err);
                    msg += L"\r\n";
                    auto* s = new std::wstring(std::move(msg));
                    PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
                    return;
                }

                const json& result = resp["result"];
                std::string code = jsonGetString(result, "code", "");
                int duration = jsonGetInt(result, "duration", 0);

                std::wstring summary = L"[cursor] Done in " + std::to_wstring(duration) + L"ms. Inserting code into editor.\r\n";
                auto* s1 = new std::wstring(std::move(summary));
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s1);

                auto* s2 = new std::wstring(widen(code));
                PostMessageW(hwndMain, WM_APP_SET_TEXT, (WPARAM)editor, (LPARAM)s2);
            } catch (...) {
                auto* s = new std::wstring(L"[cursor] Unexpected error.\r\n");
                PostMessageW(hwndMain, WM_APP_APPEND_TEXT, (WPARAM)chatOut, (LPARAM)s);
            }
        }).detach();

        return;
    }
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
        std::filesystem::path fp = path;
        editorAddTab(fp.filename().wstring(), path, widen(bytes), true);
        appendText(m_chatTranscript, L"[assistant] Opened into editor.\r\n");
        return;
    }

    // Otherwise: small local response
    std::wstring reply = L"I can run commands with /run <cmd>, open files with /open <path>, or run Cursor agent with /cursor <objective>.";
    appendText(m_chatTranscript, L"[assistant] " + reply + L"\r\n");
}

void NoQtIDEApp::toggleMaxMode() {
    m_maxMode = !m_maxMode;
    RECT r{};
    GetClientRect(m_hwnd, &r);
    layout(r.right - r.left, r.bottom - r.top);
    Logger::instance().info(std::string("Max mode: ") + (m_maxMode ? "on" : "off"));
}

void NoQtIDEApp::editorEnsureInitialTab() {
    if (m_editorDocs.empty()) {
        editorAddTab(L"untitled.cpp", L"", L"// New file\r\n", true);
    } else if (m_activeEditorTab < 0) {
        editorSetActiveTab(0);
    }
}

void NoQtIDEApp::editorSaveActiveToModel() {
    if (m_activeEditorTab < 0 || m_activeEditorTab >= (int)m_editorDocs.size()) return;
    m_editorDocs[m_activeEditorTab].content = getWindowTextW(m_editor);
    m_editorDocs[m_activeEditorTab].dirty = true;
}

void NoQtIDEApp::editorLoadActiveFromModel() {
    if (m_activeEditorTab < 0 || m_activeEditorTab >= (int)m_editorDocs.size()) return;
    setWindowTextW(m_editor, m_editorDocs[m_activeEditorTab].content);
    m_currentFilePath = m_editorDocs[m_activeEditorTab].path;
}

void NoQtIDEApp::editorSetActiveTab(int index) {
    if (index < 0 || index >= (int)m_editorDocs.size()) return;
    if (m_activeEditorTab == index) return;

    editorSaveActiveToModel();
    m_activeEditorTab = index;
    TabCtrl_SetCurSel(m_editorTabs, index);
    editorLoadActiveFromModel();
}

void NoQtIDEApp::editorAddTab(const std::wstring& title, const std::wstring& path, const std::wstring& content, bool activate) {
    EditorDoc doc;
    doc.title = title;
    doc.path = path;
    doc.content = content;
    m_editorDocs.push_back(doc);

    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<wchar_t*>(m_editorDocs.back().title.c_str());
    int idx = TabCtrl_InsertItem(m_editorTabs, (int)m_editorDocs.size() - 1, &ti);
    if (activate) {
        editorSetActiveTab(idx);
    }
}

void NoQtIDEApp::editorCloseTab(int index) {
    if (index < 0 || index >= (int)m_editorDocs.size()) return;

    TabCtrl_DeleteItem(m_editorTabs, index);
    m_editorDocs.erase(m_editorDocs.begin() + index);

    if (m_editorDocs.empty()) {
        m_activeEditorTab = -1;
        setWindowTextW(m_editor, L"");
        m_currentFilePath.clear();
        editorEnsureInitialTab();
        return;
    }

    int next = index;
    if (next >= (int)m_editorDocs.size()) next = (int)m_editorDocs.size() - 1;
    m_activeEditorTab = -1;
    editorSetActiveTab(next);
}

void NoQtIDEApp::editorUndockTab(int index) {
    if (index < 0 || index >= (int)m_editorDocs.size()) return;
    editorSaveActiveToModel();

    std::wstring title = L"Undocked: " + m_editorDocs[index].title;
    HWND w = CreateWindowExW(
        0,
        L"RawrXDNoQtIDE",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 700,
        nullptr,
        nullptr,
        m_hInstance,
        nullptr);
    if (!w) return;

    HWND editor = createRichEdit(w, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 3101);
    if (editor) {
        SendMessageW(editor, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        setWindowTextW(editor, m_editorDocs[index].content);
        RECT r{};
        GetClientRect(w, &r);
        MoveWindow(editor, 0, 0, r.right - r.left, r.bottom - r.top, TRUE);
    }
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
}

void NoQtIDEApp::chatEnsureInitialTab() {
    if (m_chatSessions.empty()) {
        chatAddTab(L"chat-1", true);
    } else if (m_activeChatTab < 0) {
        chatSetActiveTab(0);
    }
}

void NoQtIDEApp::chatSetActiveTab(int index) {
    if (index < 0 || index >= (int)m_chatSessions.size()) return;
    if (m_activeChatTab == index) return;

    if (m_activeChatTab >= 0 && m_activeChatTab < (int)m_chatSessions.size()) {
        m_chatSessions[m_activeChatTab].transcript = getWindowTextW(m_chatTranscript);
    }
    m_activeChatTab = index;
    TabCtrl_SetCurSel(m_chatTabs, index);
    setWindowTextW(m_chatTranscript, m_chatSessions[m_activeChatTab].transcript);
}

void NoQtIDEApp::chatAddTab(const std::wstring& title, bool activate) {
    ChatSession s;
    s.title = title;
    s.transcript = L"[system] Chat ready. Use /cursor <objective>.\r\n";
    m_chatSessions.push_back(s);

    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<wchar_t*>(m_chatSessions.back().title.c_str());
    int idx = TabCtrl_InsertItem(m_chatTabs, (int)m_chatSessions.size() - 1, &ti);
    if (activate) chatSetActiveTab(idx);
}

void NoQtIDEApp::chatCloseTab(int index) {
    if (index < 0 || index >= (int)m_chatSessions.size()) return;

    TabCtrl_DeleteItem(m_chatTabs, index);
    m_chatSessions.erase(m_chatSessions.begin() + index);

    if (m_chatSessions.empty()) {
        m_activeChatTab = -1;
        setWindowTextW(m_chatTranscript, L"");
        chatEnsureInitialTab();
        return;
    }

    int next = index;
    if (next >= (int)m_chatSessions.size()) next = (int)m_chatSessions.size() - 1;
    m_activeChatTab = -1;
    chatSetActiveTab(next);
}

static bool looksLikeDirectoryW(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void NoQtIDEApp::fileBrowserPopulateRoots() {
    if (!m_fileTree) return;
    TreeView_DeleteAllItems(m_fileTree);

    wchar_t drives[512];
    DWORD got = GetLogicalDriveStringsW((DWORD)std::size(drives), drives);
    if (got == 0 || got > std::size(drives)) return;

    for (wchar_t* p = drives; *p; p += wcslen(p) + 1) {
        std::wstring drive = p; // e.g. "C:\\"

        TVINSERTSTRUCTW ins{};
        ins.hParent = TVI_ROOT;
        ins.hInsertAfter = TVI_LAST;
        ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
        ins.item.pszText = const_cast<wchar_t*>(drive.c_str());
        ins.item.lParam = (LPARAM)new std::wstring(drive);
        ins.item.cChildren = 1; // show expand glyph (lazy load)
        TreeView_InsertItem(m_fileTree, &ins);
    }
}

void NoQtIDEApp::fileBrowserPopulateChildren(void* hItemVoid) {
    if (!m_fileTree) return;
    HTREEITEM hItem = (HTREEITEM)hItemVoid;
    if (!hItem) return;

    TVITEMW it{};
    it.mask = TVIF_PARAM | TVIF_CHILDREN;
    it.hItem = hItem;
    if (!TreeView_GetItem(m_fileTree, &it)) return;

    auto* basePath = reinterpret_cast<std::wstring*>(it.lParam);
    if (!basePath || basePath->empty()) return;

    // If already populated, skip
    HTREEITEM child = TreeView_GetChild(m_fileTree, hItem);
    if (child) return;

    std::filesystem::path root = *basePath;
    try {
        for (auto const& entry : std::filesystem::directory_iterator(root)) {
            std::wstring name = entry.path().filename().wstring();
            if (name == L"." || name == L"..") continue;

            bool isDir = entry.is_directory();
            if (!isDir) {
                // Keep the tree focused on directories + common code files
                std::wstring ext = entry.path().extension().wstring();
                for (auto& c : ext) c = (wchar_t)towlower(c);
                if (!(ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".c" || ext == L".md" || ext == L".txt" || ext == L".json" || ext == L".js" || ext == L".ts" || ext == L".py")) {
                    continue;
                }
            }

            std::wstring full = entry.path().wstring();

            TVINSERTSTRUCTW ins{};
            ins.hParent = hItem;
            ins.hInsertAfter = TVI_LAST;
            ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
            ins.item.pszText = const_cast<wchar_t*>(name.c_str());
            ins.item.lParam = (LPARAM)new std::wstring(full);
            ins.item.cChildren = isDir ? 1 : 0;
            TreeView_InsertItem(m_fileTree, &ins);
        }
    } catch (...) {
        // ignore
    }
}

void NoQtIDEApp::fileBrowserOpenSelected() {
    if (!m_fileTree) return;
    HTREEITEM sel = TreeView_GetSelection(m_fileTree);
    if (!sel) return;

    TVITEMW it{};
    it.mask = TVIF_PARAM;
    it.hItem = sel;
    if (!TreeView_GetItem(m_fileTree, &it)) return;
    auto* p = reinterpret_cast<std::wstring*>(it.lParam);
    if (!p || p->empty()) return;

    if (looksLikeDirectoryW(*p)) {
        TreeView_Expand(m_fileTree, sel, TVE_EXPAND);
        return;
    }

    std::ifstream in(*p, std::ios::binary);
    if (!in) return;
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::filesystem::path fp = *p;
    editorAddTab(fp.filename().wstring(), *p, widen(bytes), true);
    Logger::instance().info("File tree opened: " + narrow(*p));
}

LRESULT NoQtIDEApp::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            createMenus();
            createToolbar();
            createCommandPalette();
            createChildWindows();
            {
                RECT r{};
                GetClientRect(hwnd, &r);
                layout(r.right - r.left, r.bottom - r.top);
            }
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
                case ID_VIEW_MAXMODE: toggleMaxMode(); return 0;
 
                case ID_TOOLS_PALETTE: showCommandPalette(); return 0;
 
                case ID_CHAT_SEND: onChatSend(); return 0;
                case ID_TERM_RUN: onTerminalRun(); return 0;
                case ID_ORCH_RUN: onOrchestraRun(); return 0;

                // Paint save/load is handled in the dedicated paint window (opened via View → New Paint Window).
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR hdr = (LPNMHDR)lParam;
            if (!hdr) break;

            if (hdr->hwndFrom == m_editorTabs && hdr->code == TCN_SELCHANGE) {
                int idx = TabCtrl_GetCurSel(m_editorTabs);
                editorSetActiveTab(idx);
                return 0;
            }
            if (hdr->hwndFrom == m_chatTabs && hdr->code == TCN_SELCHANGE) {
                int idx = TabCtrl_GetCurSel(m_chatTabs);
                chatSetActiveTab(idx);
                return 0;
            }

            if (hdr->hwndFrom == m_fileTree) {
                switch (hdr->code) {
                    case NM_DBLCLK:
                        fileBrowserOpenSelected();
                        return 0;
                    case TVN_ITEMEXPANDINGW: {
                        auto* tv = (LPNMTREEVIEWW)lParam;
                        if (tv && (tv->action == TVE_EXPAND)) {
                            fileBrowserPopulateChildren(tv->itemNew.hItem);
                        }
                        return 0;
                    }
                    case TVN_DELETEITEMW: {
                        auto* tv = (LPNMTREEVIEWW)lParam;
                        if (tv) {
                            auto* p = reinterpret_cast<std::wstring*>(tv->itemOld.lParam);
                            delete p;
                        }
                        return 0;
                    }
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
                    case 'T': onNewCode(); return 0;
                    case 'W':
                        if (m_activeEditorTab >= 0) editorCloseTab(m_activeEditorTab);
                        return 0;
                }
            }
            if (wParam == VK_F11) {
                toggleMaxMode();
                return 0;
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

        case WM_APP_SET_TEXT: {
            HWND target = (HWND)wParam;
            auto* s = reinterpret_cast<std::wstring*>(lParam);
            if (target && s) setWindowTextW(target, *s);
            delete s;
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);

            if (!m_fileTree && !m_editor && !m_chatTranscript && !m_termOutput) {
                const wchar_t* msgText =
                    L"UI failed to initialize.\r\n"
                    L"Check the log next to the executable (AgenticIDEWin.exe.log).\r\n"
                    L"Common causes: missing RichEdit DLLs or common controls initialization failures.";
                RECT r{};
                GetClientRect(hwnd, &r);
                SetBkMode(dc, TRANSPARENT);
                DrawTextW(dc, msgText, -1, &r, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            }

            EndPaint(hwnd, &ps);
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
