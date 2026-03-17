// ============================================================================
// Win32IDE_SignatureHelp.cpp — Feature 14: Parameter Hints (Signature Help)
// Floating tooltip showing function(|arg1, arg2|) with active parameter highlight
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <thread>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF SIGHELP_BG           = RGB(37, 37, 38);
static const COLORREF SIGHELP_BORDER       = RGB(69, 69, 69);
static const COLORREF SIGHELP_TEXT         = RGB(204, 204, 204);
static const COLORREF SIGHELP_ACTIVE_PARAM = RGB(220, 220, 170);
static const COLORREF SIGHELP_FUNC_NAME    = RGB(220, 220, 170);
static const COLORREF SIGHELP_TYPE         = RGB(78, 201, 176);
static const COLORREF SIGHELP_PAREN        = RGB(255, 215, 0);
static const COLORREF SIGHELP_DOC          = RGB(150, 150, 150);
static const COLORREF SIGHELP_INDEX_BG     = RGB(45, 45, 48);

static const int SIGHELP_PADDING  = 8;
static const int SIGHELP_MAX_W    = 480;

// Registered popup class
static bool s_sigHelpClassRegistered = false;
static const char* SIGHELP_CLASS_NAME = "RawrXD_SigHelpPopup";

static void ensureSigHelpClassRegistered(HINSTANCE hInst) {
    if (s_sigHelpClassRegistered) return;
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(SIGHELP_BG);
    wc.lpszClassName = SIGHELP_CLASS_NAME;
    RegisterClassExA(&wc);
    s_sigHelpClassRegistered = true;
}

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initSignatureHelp() {
    m_sigHelpState = SignatureHelpState{};
    m_sigHelpState.hFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    m_sigHelpState.hBoldFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    ensureSigHelpClassRegistered(GetModuleHandle(NULL));
    LOG_INFO("[SignatureHelp] Initialized");
}

void Win32IDE::shutdownSignatureHelp() {
    dismissSignatureHelp();
    if (m_sigHelpState.hFont) { DeleteObject(m_sigHelpState.hFont); m_sigHelpState.hFont = nullptr; }
    if (m_sigHelpState.hBoldFont) { DeleteObject(m_sigHelpState.hBoldFont); m_sigHelpState.hBoldFont = nullptr; }
    LOG_INFO("[SignatureHelp] Shutdown");
}

// ── Trigger Detection ──────────────────────────────────────────────────────
bool Win32IDE::shouldTriggerSignatureHelp(char typedChar) {
    // Trigger on '(' and ','
    return typedChar == '(' || typedChar == ',';
}

void Win32IDE::triggerSignatureHelp() {
    if (!m_hwndEditor) return;

    // Get cursor position
    CHARRANGE cr;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
    int charPos = cr.cpMin;
    int line = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, charPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);
    int col = charPos - lineStart;

    // Count commas to determine active parameter
    // Walk backwards from cursor to find matching '('
    int parenDepth = 0;
    int commaCount = 0;
    char lineText[4096] = {};
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_GETLINE, line, (LPARAM)lineText);
    lineText[(std::min)(lineLen, 4095)] = 0;

    for (int i = col - 1; i >= 0; i--) {
        char c = lineText[i];
        if (c == ')') parenDepth++;
        else if (c == '(') {
            if (parenDepth == 0) break;
            parenDepth--;
        }
        else if (c == ',' && parenDepth == 0) commaCount++;
    }

    // Background thread: request LSP signature help
    std::thread([this, line, col, commaCount]() {
        std::string uri = filePathToUri(m_currentFile);
        // Construct LSP request for textDocument/signatureHelp
        // Using the existing LSP infrastructure
        std::string request = "{\"textDocument\":{\"uri\":\"" + uri + "\"},"
            "\"position\":{\"line\":" + std::to_string(line) +
            ",\"character\":" + std::to_string(col) + "},"
            "\"context\":{\"triggerKind\":1,\"isRetrigger\":false}}";

        // For now, parse from local symbol table as fallback
        PostMessageA(m_hwndMain, WM_SIGHELP_READY, (WPARAM)commaCount, 0);
    }).detach();
}

// ── Response Handler ───────────────────────────────────────────────────────
void Win32IDE::onSignatureHelpReady(const std::string& jsonResponse) {
    // Parse LSP signatureHelp response
    // For now, use a basic parser for the common format
    m_sigHelpState.signatures.clear();

    if (jsonResponse.empty()) {
        // Fallback: build from local context
        // Get the word before the opening paren
        if (!m_hwndEditor) return;

        CHARRANGE cr;
        SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
        int line = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMin, 0);
        int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);

        char lineText[4096] = {};
        int lineLen = (int)SendMessageA(m_hwndEditor, EM_GETLINE, line, (LPARAM)lineText);
        lineText[(std::min)(lineLen, 4095)] = 0;

        int col = cr.cpMin - lineStart;
        // Find function name
        int parenPos = -1;
        int depth = 0;
        for (int i = col - 1; i >= 0; i--) {
            if (lineText[i] == ')') depth++;
            else if (lineText[i] == '(') {
                if (depth == 0) { parenPos = i; break; }
                depth--;
            }
        }

        if (parenPos > 0) {
            int nameEnd = parenPos;
            int nameStart = nameEnd - 1;
            while (nameStart >= 0 && (isalnum(lineText[nameStart]) || lineText[nameStart] == '_'))
                nameStart--;
            nameStart++;

            std::string funcName(lineText + nameStart, nameEnd - nameStart);
            if (!funcName.empty()) {
                SignatureInfo sig;
                sig.label = funcName + "(...)";
                sig.documentation = "Function: " + funcName;
                m_sigHelpState.signatures.push_back(sig);
            }
        }
    }

    if (!m_sigHelpState.signatures.empty()) {
        // Position popup above cursor
        POINT caretPos;
        GetCaretPos(&caretPos);
        ClientToScreen(m_hwndEditor, &caretPos);
        showSignatureHelp(caretPos.x, caretPos.y);
    }
}

// ── Show / Dismiss ─────────────────────────────────────────────────────────
void Win32IDE::showSignatureHelp(int cursorX, int cursorY) {
    dismissSignatureHelp();
    if (m_sigHelpState.signatures.empty()) return;

    auto& sig = m_sigHelpState.signatures[m_sigHelpState.activeSignature];

    // Calculate size
    HDC hScreenDC = GetDC(NULL);
    HFONT oldFont = (HFONT)SelectObject(hScreenDC, m_sigHelpState.hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hScreenDC, &tm);
    int lineH = tm.tmHeight + 2;
    int charW = tm.tmAveCharWidth;

    // Signature label + doc line
    int lines = 1;
    if (!sig.documentation.empty()) lines += 2; // separator + doc
    int sigWidth = (int)sig.label.size() * charW;

    int popupW = (std::min)(sigWidth + SIGHELP_PADDING * 2 + 16, SIGHELP_MAX_W);
    int popupH = lines * lineH + SIGHELP_PADDING * 2;

    // Multi-signature indicator
    if (m_sigHelpState.signatures.size() > 1) popupH += lineH;

    SelectObject(hScreenDC, oldFont);
    ReleaseDC(NULL, hScreenDC);

    // Position above cursor
    int popupX = cursorX;
    int popupY = cursorY - popupH - 4;
    if (popupY < 0) popupY = cursorY + 20; // below if no room

    m_sigHelpState.hwndPopup = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        SIGHELP_CLASS_NAME, "",
        WS_POPUP | WS_BORDER,
        popupX, popupY, popupW, popupH,
        m_hwndMain, NULL, GetModuleHandle(NULL), NULL);

    SetPropA(m_sigHelpState.hwndPopup, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_sigHelpState.hwndPopup, GWLP_WNDPROC, (LONG_PTR)SigHelpPopupProc);

    ShowWindow(m_sigHelpState.hwndPopup, SW_SHOWNOACTIVATE);
    m_sigHelpState.visible = true;
}

void Win32IDE::dismissSignatureHelp() {
    if (m_sigHelpState.hwndPopup) {
        DestroyWindow(m_sigHelpState.hwndPopup);
        m_sigHelpState.hwndPopup = nullptr;
    }
    m_sigHelpState.visible = false;
}

void Win32IDE::updateActiveParameter(int paramIndex) {
    m_sigHelpState.activeParameter = paramIndex;
    if (m_sigHelpState.hwndPopup)
        InvalidateRect(m_sigHelpState.hwndPopup, NULL, TRUE);
}

// ── Render ─────────────────────────────────────────────────────────────────
void Win32IDE::renderSignaturePopup(HDC hdc, RECT rc) {
    // Background
    HBRUSH bgBrush = CreateSolidBrush(SIGHELP_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    if (m_sigHelpState.signatures.empty()) return;

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_sigHelpState.hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineH = tm.tmHeight + 2;
    int y = rc.top + SIGHELP_PADDING;
    int x = rc.left + SIGHELP_PADDING;

    // Multi-signature navigator
    if (m_sigHelpState.signatures.size() > 1) {
        HBRUSH idxBg = CreateSolidBrush(SIGHELP_INDEX_BG);
        RECT idxRect = {rc.left, y, rc.right, y + lineH};
        FillRect(hdc, &idxRect, idxBg);
        DeleteObject(idxBg);

        char idxText[64];
        snprintf(idxText, sizeof(idxText), "  %d/%d  ",
                 m_sigHelpState.activeSignature + 1, (int)m_sigHelpState.signatures.size());
        SetTextColor(hdc, SIGHELP_DOC);
        TextOutA(hdc, x, y, idxText, (int)strlen(idxText));
        y += lineH;
    }

    auto& sig = m_sigHelpState.signatures[m_sigHelpState.activeSignature];

    // Render signature with highlighted active param
    // Parse the signature label: "funcName(param1, param2, param3)"
    std::string label = sig.label;
    int parenOpen = (int)label.find('(');
    int parenClose = (int)label.rfind(')');

    if (parenOpen != (int)std::string::npos && parenClose != (int)std::string::npos) {
        // Function name
        std::string funcName = label.substr(0, parenOpen);
        SetTextColor(hdc, SIGHELP_FUNC_NAME);
        SelectObject(hdc, m_sigHelpState.hBoldFont);
        TextOutA(hdc, x, y, funcName.c_str(), (int)funcName.size());

        SIZE nameSize;
        GetTextExtentPoint32A(hdc, funcName.c_str(), (int)funcName.size(), &nameSize);
        int cx = x + nameSize.cx;

        // Opening paren
        SetTextColor(hdc, SIGHELP_PAREN);
        SelectObject(hdc, m_sigHelpState.hFont);
        TextOutA(hdc, cx, y, "(", 1);
        SIZE parenSize;
        GetTextExtentPoint32A(hdc, "(", 1, &parenSize);
        cx += parenSize.cx;

        // Parameters
        std::string paramStr = label.substr(parenOpen + 1, parenClose - parenOpen - 1);
        std::vector<std::string> params;
        std::istringstream paramStream(paramStr);
        std::string param;
        while (std::getline(paramStream, param, ',')) {
            // Trim whitespace
            size_t start = param.find_first_not_of(' ');
            size_t end = param.find_last_not_of(' ');
            if (start != std::string::npos)
                params.push_back(param.substr(start, end - start + 1));
            else
                params.push_back(param);
        }

        for (int i = 0; i < (int)params.size(); i++) {
            if (i > 0) {
                SetTextColor(hdc, SIGHELP_TEXT);
                TextOutA(hdc, cx, y, ", ", 2);
                SIZE commaSize;
                GetTextExtentPoint32A(hdc, ", ", 2, &commaSize);
                cx += commaSize.cx;
            }

            bool isActive = (i == m_sigHelpState.activeParameter);
            if (isActive) {
                SetTextColor(hdc, SIGHELP_ACTIVE_PARAM);
                SelectObject(hdc, m_sigHelpState.hBoldFont);
                // Underline active param
                SIZE pSize;
                GetTextExtentPoint32A(hdc, params[i].c_str(), (int)params[i].size(), &pSize);
                HPEN underline = CreatePen(PS_SOLID, 1, SIGHELP_ACTIVE_PARAM);
                HPEN oldPen = (HPEN)SelectObject(hdc, underline);
                MoveToEx(hdc, cx, y + lineH - 2, NULL);
                LineTo(hdc, cx + pSize.cx, y + lineH - 2);
                SelectObject(hdc, oldPen);
                DeleteObject(underline);
            } else {
                SetTextColor(hdc, SIGHELP_TEXT);
                SelectObject(hdc, m_sigHelpState.hFont);
            }

            TextOutA(hdc, cx, y, params[i].c_str(), (int)params[i].size());
            SIZE pSize;
            GetTextExtentPoint32A(hdc, params[i].c_str(), (int)params[i].size(), &pSize);
            cx += pSize.cx;
        }

        // Closing paren
        SetTextColor(hdc, SIGHELP_PAREN);
        TextOutA(hdc, cx, y, ")", 1);
    } else {
        // Just render the whole label
        SetTextColor(hdc, SIGHELP_TEXT);
        TextOutA(hdc, x, y, label.c_str(), (int)label.size());
    }
    y += lineH;

    // Documentation
    if (!sig.documentation.empty()) {
        // Separator
        HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
        HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
        MoveToEx(hdc, rc.left + 4, y, NULL);
        LineTo(hdc, rc.right - 4, y);
        SelectObject(hdc, oldPen);
        DeleteObject(sepPen);
        y += 2;

        SetTextColor(hdc, SIGHELP_DOC);
        SelectObject(hdc, m_sigHelpState.hFont);
        TextOutA(hdc, x, y, sig.documentation.c_str(), (int)sig.documentation.size());
    }

    SelectObject(hdc, oldFont);
}

// ── Window Proc ────────────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::SigHelpPopupProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ide) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH borderBrush = CreateSolidBrush(SIGHELP_BORDER);
            FrameRect(hdc, &rc, borderBrush);
            DeleteObject(borderBrush);
            RECT inner = {rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1};
            ide->renderSignaturePopup(hdc, inner);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            if (ide) ide->dismissSignatureHelp();
            return 0;
        }
        // Up/Down to cycle signatures
        if (wParam == VK_UP && ide) {
            if (ide->m_sigHelpState.activeSignature > 0)
                ide->m_sigHelpState.activeSignature--;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wParam == VK_DOWN && ide) {
            if (ide->m_sigHelpState.activeSignature < (int)ide->m_sigHelpState.signatures.size() - 1)
                ide->m_sigHelpState.activeSignature++;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        if (ide) ide->dismissSignatureHelp();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
