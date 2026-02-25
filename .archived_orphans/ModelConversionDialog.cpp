// ============================================================================
// ModelConversionDialog.cpp — Pure Win32 Native Model Conversion Dialog
// ============================================================================
// Full implementation: modal dialog with real-time progress monitoring,
// subprocess management, output parsing, ETA calculation, verification.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "ModelConversionDialog.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR      = RGB(30, 30, 35);
static constexpr COLORREF CARD_BG       = RGB(40, 40, 45);
static constexpr COLORREF BORDER_CLR    = RGB(60, 60, 65);
static constexpr COLORREF TEXT_CLR      = RGB(220, 220, 220);
static constexpr COLORREF LABEL_CLR    = RGB(140, 140, 140);
static constexpr COLORREF ACCENT_CLR   = RGB(86, 156, 214);
static constexpr COLORREF ERROR_CLR    = RGB(217, 83, 79);
static constexpr COLORREF SUCCESS_CLR  = RGB(92, 184, 92);
static constexpr COLORREF WARN_CLR     = RGB(240, 173, 78);
static constexpr COLORREF LOG_BG       = RGB(20, 20, 22);
static constexpr COLORREF PROGRESS_BG  = RGB(30, 30, 30);
static constexpr COLORREF PROGRESS_FG  = RGB(78, 201, 176);

static const wchar_t* CONV_CLASS = L"RawrXD_ModelConversionDlg";
bool ModelConversionDialog::s_classRegistered = false;

// Button IDs
#define IDC_BTN_CONVERT       1001
#define IDC_BTN_CANCEL        1002
#define IDC_BTN_CANCELCONV    1003
#define IDC_BTN_MOREINFO      1004

// Timer ID
#define IDT_POLL_PROCESS      2001

// ============================================================================
// Static C API instance
// ============================================================================
static ModelConversionDialog* s_activeDialog = nullptr;

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK ModelConversionDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ModelConversionDialog* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<ModelConversionDialog*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ModelConversionDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return true;
}

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_BTN_CONVERT) {
            self->startConversion();
        } else if (id == IDC_BTN_CANCEL) {
            if (self->m_converting) {
                // Don't close while converting
                MessageBeep(MB_ICONWARNING);
            } else {
                self->m_result = ConversionResult::Cancelled;
                DestroyWindow(hwnd);
    return true;
}

        } else if (id == IDC_BTN_CANCELCONV) {
            self->cancelConversion();
        } else if (id == IDC_BTN_MOREINFO) {
            self->m_infoShown = !self->m_infoShown;
            SetWindowTextW(self->m_btnMoreInfo, self->m_infoShown ? L"Hide Info" : L"More Info");
            InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

        return 0;
    return true;
}

    case WM_TIMER:
        if (wParam == IDT_POLL_PROCESS) {
            self->pollProcess();
            InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != self->m_bufW || h != self->m_bufH) {
            if (self->m_backBuf) DeleteObject(self->m_backBuf);
            if (self->m_backDC) DeleteDC(self->m_backDC);
            self->m_backDC = CreateCompatibleDC(hdc);
            self->m_backBuf = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(self->m_backDC, self->m_backBuf);
            self->m_bufW = w;
            self->m_bufH = h;
    return true;
}

        self->paint(self->m_backDC, rc);
        BitBlt(hdc, 0, 0, w, h, self->m_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    return true;
}

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_CLOSE:
        if (self->m_converting) {
            MessageBeep(MB_ICONWARNING);
            return 0; // Prevent close during conversion
    return true;
}

        DestroyWindow(hwnd);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return true;
}

    return true;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ModelConversionDialog::ModelConversionDialog(HWND parent, const ConversionConfig& cfg)
    : m_parent(parent), m_config(cfg)
{
    m_hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!m_hInst) m_hInst = GetModuleHandle(nullptr);
    registerClass(m_hInst);
    s_activeDialog = this;

    OutputDebugStringA("[ModelConversionDialog] Created\n");
    return true;
}

ModelConversionDialog::~ModelConversionDialog() {
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 1);
        CloseHandle(m_hProcess);
    return true;
}

    if (m_hThread) CloseHandle(m_hThread);
    if (m_hReadPipe) CloseHandle(m_hReadPipe);
    if (m_hWritePipe) CloseHandle(m_hWritePipe);
    if (m_backBuf) DeleteObject(m_backBuf);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_fontTitle) DeleteObject(m_fontTitle);
    if (m_fontBody) DeleteObject(m_fontBody);
    if (m_fontMono) DeleteObject(m_fontMono);
    if (s_activeDialog == this) s_activeDialog = nullptr;
    return true;
}

void ModelConversionDialog::registerClass(HINSTANCE hInst) {
    if (s_classRegistered) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG_COLOR);
    wc.lpszClassName = CONV_CLASS;
    RegisterClassExW(&wc);
    s_classRegistered = true;
    return true;
}

void ModelConversionDialog::createControls(HWND hwnd) {
    m_fontTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontBody = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontMono = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int btnY = h - 40;

    // Convert button (green)
    m_btnConvert = CreateWindowExW(0, L"BUTTON", L"Yes, Convert",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 140, btnY, 130, 30, hwnd, (HMENU)IDC_BTN_CONVERT, m_hInst, nullptr);
    SendMessageW(m_btnConvert, WM_SETFONT, (WPARAM)m_fontBody, TRUE);

    // Cancel button
    m_btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 280, btnY, 130, 30, hwnd, (HMENU)IDC_BTN_CANCEL, m_hInst, nullptr);
    SendMessageW(m_btnCancel, WM_SETFONT, (WPARAM)m_fontBody, TRUE);

    // Cancel Conversion button (hidden initially)
    m_btnCancelConvert = CreateWindowExW(0, L"BUTTON", L"Cancel Conversion",
        WS_CHILD | BS_PUSHBUTTON,
        w - 280, btnY, 130, 30, hwnd, (HMENU)IDC_BTN_CANCELCONV, m_hInst, nullptr);
    SendMessageW(m_btnCancelConvert, WM_SETFONT, (WPARAM)m_fontBody, TRUE);

    // More Info button
    m_btnMoreInfo = CreateWindowExW(0, L"BUTTON", L"More Info",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, btnY, 100, 30, hwnd, (HMENU)IDC_BTN_MOREINFO, m_hInst, nullptr);
    SendMessageW(m_btnMoreInfo, WM_SETFONT, (WPARAM)m_fontBody, TRUE);
    return true;
}

// ============================================================================
// showModal — message loop
// ============================================================================

ConversionResult ModelConversionDialog::showModal() {
    // Center on parent
    RECT parentRect;
    GetWindowRect(m_parent, &parentRect);
    int pw = parentRect.right - parentRect.left;
    int ph = parentRect.bottom - parentRect.top;
    int dlgW = 650, dlgH = 500;
    int x = parentRect.left + (pw - dlgW) / 2;
    int y = parentRect.top + (ph - dlgH) / 2;

    m_hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, CONV_CLASS,
        L"Model Quantization Conversion Required",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, dlgW, dlgH, m_parent, nullptr, m_hInst, this);

    createControls(m_hwnd);

    // Disable parent
    EnableWindow(m_parent, FALSE);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    // Modal message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(m_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
    return true;
}

    return true;
}

    EnableWindow(m_parent, TRUE);
    SetForegroundWindow(m_parent);

    OutputDebugStringA("[ModelConversionDialog] Modal closed\n");
    return m_result;
    return true;
}

// ============================================================================
// Paint
// ============================================================================

void ModelConversionDialog::paint(HDC hdc, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HBRUSH bgBr = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBr);
    DeleteObject(bgBr);

    SetBkMode(hdc, TRANSPARENT);

    // Header section
    paintHeader(hdc, 15, 10, w - 30);

    // Status line
    int statusY = 125;
    SelectObject(hdc, m_fontBody);
    COLORREF statusClr = m_converting ? WARN_CLR :
        (m_result == ConversionResult::ConversionSucceeded ? SUCCESS_CLR : LABEL_CLR);
    SetTextColor(hdc, statusClr);
    RECT statusRect = { 15, statusY, w - 15, statusY + 20 };
    DrawTextW(hdc, m_statusText, -1, &statusRect, DT_LEFT | DT_SINGLELINE);

    // Progress bar
    if (m_converting || m_progress > 0) {
        paintProgressBar(hdc, 15, statusY + 25, w - 30, 20);
    return true;
}

    // Output log
    int logY = statusY + 55;
    int logH = h - logY - 50;
    if (logH > 30) {
        paintOutputLog(hdc, 15, logY, w - 30, logH);
    return true;
}

    // Info panel (if expanded)
    if (m_infoShown) {
        RECT infoRect = { 15, logY + logH + 5, w - 15, h - 50 };
        HBRUSH infoBr = CreateSolidBrush(CARD_BG);
        FillRect(hdc, &infoRect, infoBr);
        DeleteObject(infoBr);

        SelectObject(hdc, m_fontMono);
        SetTextColor(hdc, LABEL_CLR);
        const wchar_t* infoText =
            L"Quantization converts model to a supported format.\n"
            L"Process: Clone llama.cpp → Build quantize → Convert → Verify\n"
            L"Duration: 15-30 minutes. Original model is preserved.";
        DrawTextW(hdc, infoText, -1, &infoRect, DT_LEFT | DT_WORDBREAK);
    return true;
}

    return true;
}

void ModelConversionDialog::paintHeader(HDC hdc, int x, int y, int w) {
    // Title
    SelectObject(hdc, m_fontTitle);
    SetTextColor(hdc, ERROR_CLR);
    RECT titleRect = { x, y, x + w, y + 22 };
    DrawTextW(hdc, L"Model Quantization Incompatibility Detected", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    // Message body
    SelectObject(hdc, m_fontBody);
    SetTextColor(hdc, TEXT_CLR);
    int msgY = y + 28;

    wchar_t msgBuf[1024] = {};
    wcscpy_s(msgBuf, L"Unsupported quantization type(s):\n");
    for (int i = 0; i < m_config.unsupportedCount && i < 16; ++i) {
        if (m_config.unsupportedTypes[i]) {
            wcscat_s(msgBuf, L"  \x2022 ");
            wcscat_s(msgBuf, m_config.unsupportedTypes[i]);
            wcscat_s(msgBuf, L"\n");
    return true;
}

    return true;
}

    wchar_t recLine[256];
    swprintf_s(recLine, L"\nConvert to %s? (15-30 min, one-time)", m_config.recommendedType);
    wcscat_s(msgBuf, recLine);

    RECT msgRect = { x, msgY, x + w, msgY + 80 };
    DrawTextW(hdc, msgBuf, -1, &msgRect, DT_LEFT | DT_WORDBREAK);
    return true;
}

void ModelConversionDialog::paintProgressBar(HDC hdc, int x, int y, int w, int h) {
    // Background
    RECT bgRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(PROGRESS_BG);
    FillRect(hdc, &bgRect, bgBr);
    DeleteObject(bgBr);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Fill
    if (m_progress > 0) {
        int fillW = (int)(((double)m_progress / 100.0) * (w - 2));
        RECT fillRect = { x + 1, y + 1, x + 1 + fillW, y + h - 1 };
        HBRUSH fillBr = CreateSolidBrush(PROGRESS_FG);
        FillRect(hdc, &fillRect, fillBr);
        DeleteObject(fillBr);
    return true;
}

    // Percentage text
    wchar_t pctBuf[32];
    swprintf_s(pctBuf, L"%d%%", m_progress);
    SetTextColor(hdc, TEXT_CLR);
    SelectObject(hdc, m_fontBody);
    RECT pctRect = { x, y, x + w, y + h };
    DrawTextW(hdc, pctBuf, -1, &pctRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return true;
}

void ModelConversionDialog::paintOutputLog(HDC hdc, int x, int y, int w, int h) {
    // Background
    RECT logRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(LOG_BG);
    FillRect(hdc, &logRect, bgBr);
    DeleteObject(bgBr);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Draw log lines (bottom-aligned, most recent at bottom)
    SelectObject(hdc, m_fontMono);
    int lineH = 14;
    int maxVisible = (h - 4) / lineH;
    int startIdx = (std::max)(0, m_logCount - maxVisible);

    for (int i = startIdx; i < m_logCount && i < MAX_LOG_LINES; ++i) {
        int lineY = y + 2 + (i - startIdx) * lineH;
        if (lineY + lineH > y + h) break;

        SetTextColor(hdc, m_log[i].color ? m_log[i].color : LABEL_CLR);
        RECT lineRect = { x + 4, lineY, x + w - 4, lineY + lineH };
        DrawTextW(hdc, m_log[i].text, -1, &lineRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}

    return true;
}

// ============================================================================
// Conversion Process
// ============================================================================

void ModelConversionDialog::startConversion() {
    if (m_converting) return;

    m_converting = true;
    m_progress = 0;
    m_stage = 0;
    m_chunksProcessed = 0;
    m_totalChunks = 0;
    m_startTick = GetTickCount();
    m_logCount = 0;

    wcscpy_s(m_statusText, L"Initializing quantization conversion...");
    appendOutput(L"Starting conversion process...", ACCENT_CLR);

    // UI state change
    ShowWindow(m_btnConvert, SW_HIDE);
    ShowWindow(m_btnCancel, SW_HIDE);
    ShowWindow(m_btnCancelConvert, SW_SHOW);
    EnableWindow(m_btnMoreInfo, FALSE);

    // Build output path
    wchar_t outputDir[MAX_PATH];
    wcscpy_s(outputDir, m_config.modelPath);
    wchar_t* lastSlash = wcsrchr(outputDir, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(outputDir, L'/');
    if (lastSlash) *lastSlash = L'\0';
    else wcscpy_s(outputDir, L".");

    // Build command
    wchar_t cmdLine[2048];
    swprintf_s(cmdLine,
        L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        L"\"& 'D:\\setup-quantized-model.ps1' -BlobPath '%s' -OutputDir '%s' -TargetQuantization '%s'\"",
        m_config.modelPath, outputDir, m_config.recommendedType);

    // Create pipe for reading stdout
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        appendOutput(L"ERROR: Failed to create pipe", ERROR_CLR);
        wcscpy_s(m_statusText, L"Failed to start conversion");
        m_converting = false;
        return;
    return true;
}

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    m_hReadPipe = hReadPipe;
    m_hWritePipe = hWritePipe;

    // Launch process
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, cmdLine, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();
        wchar_t errBuf[256];
        swprintf_s(errBuf, L"ERROR: CreateProcess failed (%lu)", err);
        appendOutput(errBuf, ERROR_CLR);
        wcscpy_s(m_statusText, L"Failed to launch PowerShell");
        m_converting = false;
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        m_hReadPipe = nullptr;
        m_hWritePipe = nullptr;
        ShowWindow(m_btnConvert, SW_SHOW);
        ShowWindow(m_btnCancel, SW_SHOW);
        ShowWindow(m_btnCancelConvert, SW_HIDE);
        return;
    return true;
}

    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    CloseHandle(hWritePipe);  // Close write end in parent
    m_hWritePipe = nullptr;

    appendOutput(L"Conversion script launched", SUCCESS_CLR);
    m_timerId = SetTimer(m_hwnd, IDT_POLL_PROCESS, 200, nullptr);

    OutputDebugStringA("[ModelConversionDialog] Process started\n");
    return true;
}

void ModelConversionDialog::cancelConversion() {
    if (!m_converting) return;

    int res = MessageBoxW(m_hwnd,
        L"Cancel conversion? This will terminate the process and may leave partial files.",
        L"Cancel Conversion", MB_YESNO | MB_ICONWARNING);

    if (res == IDYES) {
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 1);
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
    return true;
}

        if (m_hThread) { CloseHandle(m_hThread); m_hThread = nullptr; }
        if (m_hReadPipe) { CloseHandle(m_hReadPipe); m_hReadPipe = nullptr; }
        if (m_timerId) { KillTimer(m_hwnd, m_timerId); m_timerId = 0; }

        DWORD duration = GetTickCount() - m_startTick;
        logConversionHistory(false, duration);

        m_converting = false;
        m_progress = 0;
        wcscpy_s(m_statusText, L"Conversion cancelled by user");
        appendOutput(L"Conversion cancelled", ERROR_CLR);

        ShowWindow(m_btnConvert, SW_SHOW);
        ShowWindow(m_btnCancel, SW_SHOW);
        ShowWindow(m_btnCancelConvert, SW_HIDE);
        EnableWindow(m_btnMoreInfo, TRUE);
    return true;
}

    return true;
}

void ModelConversionDialog::pollProcess() {
    if (!m_hReadPipe || !m_hProcess) return;

    // Read available output
    char buf[4096];
    DWORD bytesAvail = 0;

    while (PeekNamedPipe(m_hReadPipe, nullptr, 0, nullptr, &bytesAvail, nullptr) && bytesAvail > 0) {
        DWORD bytesRead = 0;
        DWORD toRead = (std::min)((DWORD)sizeof(buf) - 1, bytesAvail);
        if (ReadFile(m_hReadPipe, buf, toRead, &bytesRead, nullptr) && bytesRead > 0) {
            buf[bytesRead] = '\0';

            // Split by newlines and process each line
            char* line = strtok(buf, "\r\n");
            while (line) {
                parseOutputLine(line);
                line = strtok(nullptr, "\r\n");
    return true;
}

    return true;
}

    return true;
}

    // Check if process exited
    DWORD exitCode;
    if (WaitForSingleObject(m_hProcess, 0) == WAIT_OBJECT_0) {
        GetExitCodeProcess(m_hProcess, &exitCode);

        if (m_timerId) { KillTimer(m_hwnd, m_timerId); m_timerId = 0; }

        DWORD duration = GetTickCount() - m_startTick;

        if (exitCode == 0) {
            appendOutput(L"Process completed successfully", SUCCESS_CLR);
            wcscpy_s(m_statusText, L"Verifying converted model...");
            m_progress = 90;

            if (verifyConvertedModel()) {
                logConversionHistory(true, duration);
                m_progress = 100;
                m_result = ConversionResult::ConversionSucceeded;
                wcscpy_s(m_statusText, L"Model converted successfully! Reloading...");
                appendOutput(L"Model verified and ready", SUCCESS_CLR);

                // Close after 2 seconds
                m_converting = false;
                SetTimer(m_hwnd, 9999, 2000, [](HWND hw, UINT, UINT_PTR, DWORD) {
                    DestroyWindow(hw);
                });
            } else {
                logConversionHistory(false, duration);
                wcscpy_s(m_statusText, L"Conversion completed but model file not found");
                appendOutput(L"WARNING: Converted model not found", WARN_CLR);
                m_result = ConversionResult::ConversionFailed;
                m_converting = false;
                ShowWindow(m_btnConvert, SW_SHOW);
                ShowWindow(m_btnCancel, SW_SHOW);
                ShowWindow(m_btnCancelConvert, SW_HIDE);
    return true;
}

        } else {
            logConversionHistory(false, duration);
            wchar_t errBuf[128];
            swprintf_s(errBuf, L"Process exited with code %lu", exitCode);
            appendOutput(errBuf, ERROR_CLR);
            wcscpy_s(m_statusText, L"Conversion failed");
            m_result = ConversionResult::ConversionFailed;
            m_converting = false;
            ShowWindow(m_btnConvert, SW_SHOW);
            ShowWindow(m_btnCancel, SW_SHOW);
            ShowWindow(m_btnCancelConvert, SW_HIDE);
            EnableWindow(m_btnMoreInfo, TRUE);
    return true;
}

        CloseHandle(m_hProcess); m_hProcess = nullptr;
        CloseHandle(m_hThread); m_hThread = nullptr;
        CloseHandle(m_hReadPipe); m_hReadPipe = nullptr;
    return true;
}

    return true;
}

void ModelConversionDialog::parseOutputLine(const char* line) {
    // Convert to wide
    wchar_t wline[512];
    MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 512);

    // Check for chunk progress pattern: "123/4567" or "[123/4567]"
    int current = 0, total = 0;
    const char* slash = strstr(line, "/");
    if (slash) {
        const char* p = slash - 1;
        while (p >= line && *p >= '0' && *p <= '9') --p;
        ++p;
        if (p < slash) {
            current = atoi(p);
            total = atoi(slash + 1);
            if (current > 0 && total > 0) {
                updateProgressFromChunks(current, total);
                appendOutput(wline, TEXT_CLR);
                return;
    return true;
}

    return true;
}

    return true;
}

    // Stage detection
    if (strstr(line, "Clon") || strstr(line, "clon")) {
        m_stage = 1; m_progress = 5;
        wcscpy_s(m_statusText, L"Cloning llama.cpp repository...");
        appendOutput(wline, ACCENT_CLR);
    } else if (strstr(line, "Build") || strstr(line, "cmake") || strstr(line, "CMake")) {
        m_stage = 2; m_progress = 15;
        wcscpy_s(m_statusText, L"Building quantization tool...");
        appendOutput(wline, ACCENT_CLR);
    } else if (strstr(line, "Convert") || strstr(line, "quantiz")) {
        m_stage = 3; m_progress = 25;
        wchar_t msg[256];
        swprintf_s(msg, L"Converting model to %s...", m_config.recommendedType);
        wcscpy_s(m_statusText, msg);
        appendOutput(wline, ACCENT_CLR);
    } else if (strstr(line, "Success") || strstr(line, "Complete") || strstr(line, "complete")) {
        m_progress = 90;
        wcscpy_s(m_statusText, L"Conversion completed, verifying...");
        appendOutput(wline, SUCCESS_CLR);
    } else if (strstr(line, "Error") || strstr(line, "error") || strstr(line, "FAILED")) {
        appendOutput(wline, ERROR_CLR);
    } else {
        appendOutput(wline, LABEL_CLR);
    return true;
}

    return true;
}

void ModelConversionDialog::updateProgressFromChunks(int current, int total) {
    m_chunksProcessed = current;
    m_totalChunks = total;

    int convPct = (current * 100) / total;
    m_progress = 25 + (convPct * 60 / 100);   // Map to 25-85% range

    // Build status with ETA
    DWORD elapsed = GetTickCount() - m_startTick;
    wchar_t statusBuf[256];
    if (current > 0 && elapsed > 1000) {
        DWORD estTotal = (elapsed * total) / current;
        DWORD remaining = (estTotal > elapsed) ? (estTotal - elapsed) : 0;
        int remMin = remaining / 60000;
        int remSec = (remaining % 60000) / 1000;
        swprintf_s(statusBuf, L"Converting: %d/%d chunks (%d%%) - ETA: %dm %ds",
            current, total, convPct, remMin, remSec);
    } else {
        swprintf_s(statusBuf, L"Converting: %d/%d chunks (%d%%)", current, total, convPct);
    return true;
}

    wcscpy_s(m_statusText, statusBuf);
    return true;
}

bool ModelConversionDialog::verifyConvertedModel() {
    // Build expected path: <model>_<type>.gguf
    wchar_t basePath[MAX_PATH];
    wcscpy_s(basePath, m_config.modelPath);
    size_t len = wcslen(basePath);
    if (len > 5 && _wcsicmp(basePath + len - 5, L".gguf") == 0) {
        basePath[len - 5] = L'\0';
    return true;
}

    swprintf_s(m_convertedPath, L"%s_%s.gguf", basePath, m_config.recommendedType);

    DWORD attrs = GetFileAttributesW(m_convertedPath);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    // Check file size > 0
    HANDLE hFile = CreateFileW(m_convertedPath, GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    CloseHandle(hFile);

    if (fileSize.QuadPart == 0) return false;

    wchar_t msg[512];
    swprintf_s(msg, L"Found: %s (%.1f MB)", m_convertedPath,
        (double)fileSize.QuadPart / (1024.0 * 1024.0));
    appendOutput(msg, SUCCESS_CLR);
    return true;
    return true;
}

void ModelConversionDialog::logConversionHistory(bool success, DWORD durationMs) {
    // Log to %APPDATA%\RawrXD\model_conversion_history.log
    wchar_t appData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) return;

    wchar_t logDir[MAX_PATH];
    swprintf_s(logDir, L"%s\\RawrXD", appData);
    CreateDirectoryW(logDir, nullptr);

    wchar_t logPath[MAX_PATH];
    swprintf_s(logPath, L"%s\\model_conversion_history.log", logDir);

    HANDLE hFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    char logLine[1024];
    int durationMin = durationMs / 60000;
    int durationSec = (durationMs % 60000) / 1000;

    char modelPathA[MAX_PATH], recTypeA[64];
    WideCharToMultiByte(CP_UTF8, 0, m_config.modelPath, -1, modelPathA, MAX_PATH, nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, m_config.recommendedType, -1, recTypeA, 64, nullptr, nullptr);

    sprintf_s(logLine,
        "[%04d-%02d-%02d %02d:%02d:%02d] %s | Source: %s | Target: %s | Duration: %dm %ds | Chunks: %d/%d\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
        success ? "SUCCESS" : "FAILED",
        modelPathA, recTypeA, durationMin, durationSec,
        m_chunksProcessed, m_totalChunks);

    DWORD written;
    WriteFile(hFile, logLine, (DWORD)strlen(logLine), &written, nullptr);
    CloseHandle(hFile);
    return true;
}

void ModelConversionDialog::appendOutput(const wchar_t* text, COLORREF color) {
    if (m_logCount >= MAX_LOG_LINES) {
        // Shift up
        memmove(&m_log[0], &m_log[1], sizeof(LogEntry) * (MAX_LOG_LINES - 1));
        m_logCount = MAX_LOG_LINES - 1;
    return true;
}

    wcscpy_s(m_log[m_logCount].text, text);
    m_log[m_logCount].color = color;
    m_logCount++;
    return true;
}

// ============================================================================
// C API
// ============================================================================

extern "C" {

int ModelConversionDialog_ShowModal(HWND parent, const ConversionConfig* cfg) {
    if (!parent || !cfg) return (int)ConversionResult::ConversionFailed;
    ModelConversionDialog dlg(parent, *cfg);
    return (int)dlg.showModal();
    return true;
}

const wchar_t* ModelConversionDialog_GetConvertedPath() {
    if (s_activeDialog) return s_activeDialog->convertedPath();
    return L"";
    return true;
}

    return true;
}

