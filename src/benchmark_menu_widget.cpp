// benchmark_menu_widget.cpp — Qt-free Win32 implementation of the benchmark suite UI.

#include "../include/benchmark_menu_widget.hpp"
#include "../include/benchmark_runner.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <commctrl.h>
#include <commdlg.h>
#include <windows.h>
#pragma comment(lib, "comctl32.lib")
#endif

static LRESULT CALLBACK BenchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

static std::wstring u8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out;
    out.resize((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
    return out;
}

static std::string wToU8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out;
    out.resize((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), n, nullptr, nullptr);
    return out;
}

static std::wstring getWindowTextWString(HWND h) {
    if (!h) return L"";
    int len = GetWindowTextLengthW(h);
    std::wstring ws;
    ws.resize((size_t)len);
    if (len > 0) GetWindowTextW(h, ws.data(), len + 1);
    return ws;
}

static void setWindowTextU8(HWND h, const std::string& s) {
    if (!h) return;
    const std::wstring ws = u8ToW(s);
    SetWindowTextW(h, ws.c_str());
}

static void appendEditU8(HWND hEdit, const std::string& s) {
    if (!hEdit) return;
    const std::wstring ws = u8ToW(s);
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)ws.c_str());
}

static std::string nowTimeStamp() {
    using namespace std::chrono;
    const auto t = system_clock::now();
    const auto tt = system_clock::to_time_t(t);
    tm local = {};
    localtime_s(&local, &tt);
    char buf[32] = {};
    sprintf_s(buf, "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return buf;
}

static const char* kTestNames[] = {
    "cold_start",
    "warm_cache",
    "rapid_fire",
    "multi_lang",
    "context_aware",
    "multi_line",
    "gpu",
    "memory",
};

enum : UINT {
    WM_BENCH_LOG = WM_APP + 501,
    WM_BENCH_PROGRESS = WM_APP + 502,
    WM_BENCH_TEST_DONE = WM_APP + 503,
    WM_BENCH_FINISHED = WM_APP + 504,
};

struct BenchLogMsg {
    int level = 1;
    std::string text;
};

struct BenchTestDoneMsg {
    std::string name;
    bool passed = false;
    double avgLatencyMs = 0.0;
};

struct BenchFinishedMsg {
    int passed = 0;
    int total = 0;
    double sec = 0.0;
};

enum ControlId : int {
    IDC_MODEL_EDIT = 1101,
    IDC_MODEL_BROWSE = 1102,
    IDC_GPU = 1103,
    IDC_VERBOSE = 1104,
    IDC_SELECT_ALL = 1105,
    IDC_SELECT_NONE = 1106,
    IDC_RUN = 1107,
    IDC_STOP = 1108,
    IDC_CLOSE = 1109,
    IDC_LOG_EDIT = 1110,
    IDC_RESULTS_EDIT = 1111,
    IDC_PROGRESS = 1112,
    IDC_STATUS = 1113,
    IDC_TEST_BASE = 1200,
};

static void ensureCommonControls() {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

static std::string openModelDialog(HWND owner, const std::string& initialPathU8) {
    wchar_t fileBuf[MAX_PATH] = {};
    if (!initialPathU8.empty()) {
        const std::wstring w = u8ToW(initialPathU8);
        wcsncpy_s(fileBuf, w.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = (DWORD)std::size(fileBuf);
    ofn.lpstrTitle = L"Select GGUF Model";
    ofn.lpstrFilter = L"GGUF models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (!GetOpenFileNameW(&ofn)) return "";
    return wToU8(std::wstring(fileBuf));
}

static const wchar_t* kBenchWndClass = L"RawrXD_BenchmarkSuite";

static void ensureClassRegistered() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = BenchWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kBenchWndClass;
    wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);
    registered = true;
}

static void layoutControls(HWND hwnd) {
    RECT rc = {};
    GetClientRect(hwnd, &rc);

    const int pad = 10;
    const int leftW = 310;
    const int statusH = 22;
    const int btnH = 26;

    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;

    const int leftX = pad;
    const int leftY = pad;
    const int leftH = h - pad * 2 - statusH;

    const int rightX = leftX + leftW + pad;
    const int rightW = std::max(200, w - rightX - pad);
    const int rightY = pad;
    const int rightH = leftH;

    HWND hStatus = GetDlgItem(hwnd, IDC_STATUS);
    if (hStatus) MoveWindow(hStatus, pad, h - pad - statusH, w - pad * 2, statusH, TRUE);

    // Left panel controls.
    HWND hModelEdit = GetDlgItem(hwnd, IDC_MODEL_EDIT);
    HWND hBrowse = GetDlgItem(hwnd, IDC_MODEL_BROWSE);
    HWND hGpu = GetDlgItem(hwnd, IDC_GPU);
    HWND hVerbose = GetDlgItem(hwnd, IDC_VERBOSE);
    HWND hSelAll = GetDlgItem(hwnd, IDC_SELECT_ALL);
    HWND hSelNone = GetDlgItem(hwnd, IDC_SELECT_NONE);
    HWND hRun = GetDlgItem(hwnd, IDC_RUN);
    HWND hStop = GetDlgItem(hwnd, IDC_STOP);
    HWND hClose = GetDlgItem(hwnd, IDC_CLOSE);

    int y = leftY;
    const int labelH = 18;
    const int editH = 22;
    const int rowGap = 8;
    const int smallBtnW = 74;

    // Labels are static controls created without IDs; find by walking? We keep them fixed via stored handles in state,
    // but for simplicity we lay out by IDs only (labels remain where created).

    if (hModelEdit) MoveWindow(hModelEdit, leftX, y + labelH, leftW - smallBtnW - 6, editH, TRUE);
    if (hBrowse) MoveWindow(hBrowse, leftX + (leftW - smallBtnW), y + labelH, smallBtnW, editH, TRUE);
    y += labelH + editH + rowGap;

    if (hGpu) MoveWindow(hGpu, leftX, y, leftW, btnH, TRUE);
    y += btnH + 2;
    if (hVerbose) MoveWindow(hVerbose, leftX, y, leftW, btnH, TRUE);
    y += btnH + rowGap;

    // Tests checkboxes.
    for (int i = 0; i < (int)std::size(kTestNames); ++i) {
        HWND hChk = GetDlgItem(hwnd, IDC_TEST_BASE + i);
        if (!hChk) continue;
        MoveWindow(hChk, leftX, y, leftW, btnH, TRUE);
        y += btnH + 2;
    }
    y += rowGap;

    if (hSelAll) MoveWindow(hSelAll, leftX, y, (leftW / 2) - 4, btnH, TRUE);
    if (hSelNone) MoveWindow(hSelNone, leftX + (leftW / 2) + 4, y, (leftW / 2) - 4, btnH, TRUE);
    y += btnH + rowGap;

    if (hRun) MoveWindow(hRun, leftX, y, leftW, btnH, TRUE);
    y += btnH + 6;
    if (hStop) MoveWindow(hStop, leftX, y, leftW, btnH, TRUE);
    y += btnH + 6;
    if (hClose) MoveWindow(hClose, leftX, y, leftW, btnH, TRUE);

    // Right panel.
    HWND hLog = GetDlgItem(hwnd, IDC_LOG_EDIT);
    HWND hRes = GetDlgItem(hwnd, IDC_RESULTS_EDIT);
    HWND hProg = GetDlgItem(hwnd, IDC_PROGRESS);

    const int progH = 18;
    const int midGap = 8;
    const int logH = (rightH - progH - midGap) / 2;
    const int resH = rightH - progH - midGap - logH;

    if (hLog) MoveWindow(hLog, rightX, rightY, rightW, logH, TRUE);
    if (hRes) MoveWindow(hRes, rightX, rightY + logH + midGap, rightW, resH, TRUE);
    if (hProg) MoveWindow(hProg, rightX, rightY + logH + midGap + resH + 2, rightW, progH, TRUE);
}

} // namespace

// ============================================================================
// BenchmarkSelector
// ============================================================================

void BenchmarkSelector::create(HWND parent, int x, int y, int w, int h) {
    parent_ = parent;
    (void)x; (void)y; (void)w; (void)h;
    setupUI();
}

void BenchmarkSelector::setupUI() {
    if (!parent_) return;
    if (modelCombo_ && IsWindow(modelCombo_)) return;

    // Model path
    CreateWindowExW(0, L"STATIC", L"Model Path (GGUF):",
        WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, parent_, nullptr, GetModuleHandleW(nullptr), nullptr);

    modelCombo_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_MODEL_EDIT, GetModuleHandleW(nullptr), nullptr);

    CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_MODEL_BROWSE, GetModuleHandleW(nullptr), nullptr);

    gpuCheckbox_ = CreateWindowExW(0, L"BUTTON", L"Enable GPU (if available)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_GPU, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(gpuCheckbox_, BM_SETCHECK, BST_CHECKED, 0);

    verboseCheckbox_ = CreateWindowExW(0, L"BUTTON", L"Verbose logging",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_VERBOSE, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(verboseCheckbox_, BM_SETCHECK, BST_UNCHECKED, 0);

    // Tests
    for (int i = 0; i < (int)std::size(kTestNames); ++i) {
        std::wstring label = u8ToW(std::string(kTestNames[i]));
        // Friendlier labels.
        if (std::string(kTestNames[i]) == "cold_start") label = L"cold_start (cold start)";
        if (std::string(kTestNames[i]) == "warm_cache") label = L"warm_cache (warm cache)";
        if (std::string(kTestNames[i]) == "rapid_fire") label = L"rapid_fire (rapid fire)";
        if (std::string(kTestNames[i]) == "multi_lang") label = L"multi_lang (multi-language)";
        if (std::string(kTestNames[i]) == "context_aware") label = L"context_aware (context-aware)";
        if (std::string(kTestNames[i]) == "multi_line") label = L"multi_line (multi-line)";
        if (std::string(kTestNames[i]) == "gpu") label = L"gpu (GPU acceleration)";
        if (std::string(kTestNames[i]) == "memory") label = L"memory (memory)";

        HWND hChk = CreateWindowExW(0, L"BUTTON", label.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 10, 10, parent_, (HMENU)(INT_PTR)(IDC_TEST_BASE + i), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(hChk, BM_SETCHECK, BST_CHECKED, 0);
        testCheckboxes_.push_back(hChk);
    }

    CreateWindowExW(0, L"BUTTON", L"Select All",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_SELECT_ALL, GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"Select None",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_SELECT_NONE, GetModuleHandleW(nullptr), nullptr);
}

std::vector<std::string> BenchmarkSelector::getSelectedTests() const {
    std::vector<std::string> out;
    const int n = (int)testCheckboxes_.size();
    for (int i = 0; i < n && i < (int)std::size(kTestNames); ++i) {
        HWND h = testCheckboxes_[i];
        if (!h) continue;
        if (SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED) out.push_back(kTestNames[i]);
    }
    return out;
}

std::string BenchmarkSelector::getModelPath() const {
    return wToU8(getWindowTextWString(modelCombo_));
}

void BenchmarkSelector::setModelPath(const std::string& path) {
    setWindowTextU8(modelCombo_, path);
}

bool BenchmarkSelector::isGPUEnabled() const {
    return gpuCheckbox_ && (SendMessageW(gpuCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

bool BenchmarkSelector::isVerbose() const {
    return verboseCheckbox_ && (SendMessageW(verboseCheckbox_, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void BenchmarkSelector::selectAll() {
    for (HWND h : testCheckboxes_) if (h) SendMessageW(h, BM_SETCHECK, BST_CHECKED, 0);
}

void BenchmarkSelector::deselectAll() {
    for (HWND h : testCheckboxes_) if (h) SendMessageW(h, BM_SETCHECK, BST_UNCHECKED, 0);
}

// ============================================================================
// BenchmarkLogOutput
// ============================================================================

void BenchmarkLogOutput::attach(HWND hwnd) {
    m_hwnd = hwnd;
}

void BenchmarkLogOutput::logMessage(const std::string& message, LogLevel level) {
    formatLog(message, level);
}

void BenchmarkLogOutput::logProgress(int current, int total) {
    std::ostringstream oss;
    oss << "Progress: " << current << "/" << total;
    formatLog(oss.str(), INFO);
}

void BenchmarkLogOutput::logTestResult(const std::string& testName, bool passed, double latencyMs) {
    std::ostringstream oss;
    oss << (passed ? "[PASS] " : "[FAIL] ") << testName << " avg=" << latencyMs << "ms";
    formatLog(oss.str(), passed ? SUCCESS : WARNING);
}

void BenchmarkLogOutput::clear() {
    if (m_hwnd) SetWindowTextW(m_hwnd, L"");
}

void BenchmarkLogOutput::formatLog(const std::string& message, LogLevel level) {
    if (!m_hwnd) return;
    std::ostringstream oss;
    oss << "[" << nowTimeStamp() << "] " << levelToString(level) << " " << message << "\r\n";
    appendEditU8(m_hwnd, oss.str());
}

std::string BenchmarkLogOutput::levelToString(LogLevel level) {
    switch (level) {
        case DEBUG: return "DEBUG";
        case INFO: return "INFO ";
        case SUCCESS: return "OK   ";
        case WARNING: return "WARN ";
        case LOG_ERROR: return "ERR  ";
        default: return "????";
    }
}

uint32_t BenchmarkLogOutput::levelToColor(LogLevel) {
    return 0;
}

// ============================================================================
// BenchmarkResultsDisplay
// ============================================================================

void BenchmarkResultsDisplay::create(HWND parent, int x, int y, int w, int h) {
    parent_ = parent;
    (void)x; (void)y; (void)w; (void)h;
    setupUI();
}

void BenchmarkResultsDisplay::setupUI() {
    if (!parent_) return;
    if (resultsDisplay_ && IsWindow(resultsDisplay_)) return;

    resultsDisplay_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_RESULTS_EDIT, GetModuleHandleW(nullptr), nullptr);

    progressBar_ = CreateWindowExW(0, PROGRESS_CLASSW, L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10, parent_, (HMENU)(INT_PTR)IDC_PROGRESS, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(progressBar_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(progressBar_, PBM_SETPOS, 0, 0);
}

void BenchmarkResultsDisplay::setTotalTests(int count) {
    totalTests_ = count;
    results_.clear();
    if (progressBar_) {
        SendMessageW(progressBar_, PBM_SETRANGE, 0, MAKELPARAM(0, std::max(1, count)));
        SendMessageW(progressBar_, PBM_SETPOS, 0, 0);
    }
    if (resultsDisplay_) SetWindowTextW(resultsDisplay_, L"");
}

void BenchmarkResultsDisplay::updateProgress(int current) {
    if (progressBar_) SendMessageW(progressBar_, PBM_SETPOS, current, 0);
}

void BenchmarkResultsDisplay::addResult(const std::string& testName, bool passed,
                                       double avgLatencyMs, double p95LatencyMs, double successRate) {
    TestResult r;
    r.testName = testName;
    r.passed = passed;
    r.avgLatencyMs = avgLatencyMs;
    r.p95LatencyMs = p95LatencyMs;
    r.successRate = successRate;
    results_.push_back(r);

    if (!resultsDisplay_) return;
    std::ostringstream oss;
    oss << (passed ? "PASS " : "FAIL ") << testName
        << " avg=" << avgLatencyMs << "ms"
        << " p95=" << p95LatencyMs << "ms"
        << " succ=" << successRate << "%\r\n";
    appendEditU8(resultsDisplay_, oss.str());
}

void BenchmarkResultsDisplay::showSummary(int passed, int total, double executionTimeSec) {
    if (!resultsDisplay_) return;
    std::ostringstream oss;
    oss << "\r\n============================================================\r\n";
    oss << "SUMMARY: " << passed << "/" << total << " passed"
        << "  time=" << executionTimeSec << "s\r\n";
    oss << "============================================================\r\n";
    appendEditU8(resultsDisplay_, oss.str());
}

void BenchmarkResultsDisplay::reset() {
    setTotalTests(0);
}

// ============================================================================
// BenchmarkMenu (Win32 integration)
// ============================================================================

BenchmarkMenu::BenchmarkMenu(HWND mainWindow)
    : mainWindow_(mainWindow) {
}

BenchmarkMenu::~BenchmarkMenu() {
    stopBenchmarks();
    if (runnerThread_.joinable()) runnerThread_.join();
    delete selector_;
    delete logOutput_;
    delete resultsDisplay_;
    selector_ = nullptr;
    logOutput_ = nullptr;
    resultsDisplay_ = nullptr;
}

BenchmarkSelector* BenchmarkMenu::ensureSelectorAttached(HWND parent) {
    if (!selector_) selector_ = new BenchmarkSelector();
    selector_->create(parent, 0, 0, 0, 0);
    return selector_;
}

BenchmarkLogOutput* BenchmarkMenu::ensureLogAttached(HWND logEdit) {
    if (!logOutput_) logOutput_ = new BenchmarkLogOutput();
    logOutput_->attach(logEdit);
    return logOutput_;
}

BenchmarkResultsDisplay* BenchmarkMenu::ensureResultsAttached(HWND parent) {
    if (!resultsDisplay_) resultsDisplay_ = new BenchmarkResultsDisplay();
    resultsDisplay_->create(parent, 0, 0, 0, 0);
    return resultsDisplay_;
}

void BenchmarkMenu::notifyFinished() {
    runnerActive_.store(false, std::memory_order_release);
}

void BenchmarkMenu::initialize() {
    createMenu();
    connectHandlers();
}

void BenchmarkMenu::createMenu() {
    // Win32IDE already wires menu commands; we only provide the dialog behavior.
}

void BenchmarkMenu::createDialog() {
    ensureCommonControls();
    ensureClassRegistered();

    if (dialogHwnd_ && IsWindow(dialogHwnd_)) return;

    dialogHwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kBenchWndClass,
        L"RawrXD Benchmark Suite",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1060, 720,
        mainWindow_,
        nullptr,
        GetModuleHandleW(nullptr),
        this);
}

void BenchmarkMenu::connectHandlers() {
    if (!runner_) runner_ = std::make_unique<BenchmarkRunner>();
}

void BenchmarkMenu::openBenchmarkDialog() {
    createDialog();
    if (dialogHwnd_ && IsWindow(dialogHwnd_)) {
        ShowWindow(dialogHwnd_, SW_SHOW);
        SetForegroundWindow(dialogHwnd_);
    }
}

void BenchmarkMenu::show() {
    openBenchmarkDialog();
}

void BenchmarkMenu::runSelectedBenchmarks() {
    if (!runner_) runner_ = std::make_unique<BenchmarkRunner>();
    if (!dialogHwnd_ || !IsWindow(dialogHwnd_)) return;

    if (!selector_ || !logOutput_ || !resultsDisplay_) return;

    const auto tests = selector_->getSelectedTests();
    const std::string model = selector_->getModelPath();
    const bool gpu = selector_->isGPUEnabled();
    const bool verbose = selector_->isVerbose();

    if (tests.empty()) {
        logOutput_->logMessage("No tests selected.", BenchmarkLogOutput::WARNING);
        return;
    }
    if (model.empty()) {
        logOutput_->logMessage("No model path set. Browse to a GGUF path first.", BenchmarkLogOutput::WARNING);
    }

    resultsDisplay_->setTotalTests((int)tests.size());
    logOutput_->clear();
    logOutput_->logMessage("Benchmark run starting...", BenchmarkLogOutput::INFO);

    runnerActive_.store(true, std::memory_order_release);

    const HWND hwnd = dialogHwnd_;

    runner_->setOnStarted([hwnd]() {
        auto* m = new BenchLogMsg();
        m->level = 1;
        m->text = "Runner started.";
        PostMessageW(hwnd, WM_BENCH_LOG, 0, (LPARAM)m);
    });
    runner_->setOnTestStarted([hwnd](const std::string& name) {
        auto* m = new BenchLogMsg();
        m->level = 1;
        m->text = "Test: " + name;
        PostMessageW(hwnd, WM_BENCH_LOG, 0, (LPARAM)m);
    });
    runner_->setOnProgress([hwnd](int cur, int total) {
        PostMessageW(hwnd, WM_BENCH_PROGRESS, (WPARAM)cur, (LPARAM)total);
    });
    runner_->setOnTestCompleted([hwnd](const std::string& name, bool passed, double latencyMs) {
        auto* m = new BenchTestDoneMsg();
        m->name = name;
        m->passed = passed;
        m->avgLatencyMs = latencyMs;
        PostMessageW(hwnd, WM_BENCH_TEST_DONE, 0, (LPARAM)m);
    });
    runner_->setOnFinished([hwnd](int passed, int total, double sec) {
        auto* m = new BenchFinishedMsg();
        m->passed = passed;
        m->total = total;
        m->sec = sec;
        PostMessageW(hwnd, WM_BENCH_FINISHED, 0, (LPARAM)m);
    });
    runner_->setOnError([hwnd](const std::string& err) {
        auto* m = new BenchLogMsg();
        m->level = 4;
        m->text = err;
        PostMessageW(hwnd, WM_BENCH_LOG, 0, (LPARAM)m);
    });
    runner_->setOnLogMessage([hwnd](const std::string& msg, int level) {
        auto* m = new BenchLogMsg();
        m->level = level;
        m->text = msg;
        PostMessageW(hwnd, WM_BENCH_LOG, 0, (LPARAM)m);
    });

    // UI state
    EnableWindow(GetDlgItem(dialogHwnd_, IDC_RUN), FALSE);
    EnableWindow(GetDlgItem(dialogHwnd_, IDC_STOP), TRUE);
    setWindowTextU8(GetDlgItem(dialogHwnd_, IDC_STATUS), "Running...");

    runner_->runBenchmarks(tests, model, gpu, verbose);
}

void BenchmarkMenu::stopBenchmarks() {
    if (runner_) runner_->stop();
    runnerActive_.store(false, std::memory_order_release);
    if (dialogHwnd_ && IsWindow(dialogHwnd_)) {
        EnableWindow(GetDlgItem(dialogHwnd_, IDC_RUN), TRUE);
        EnableWindow(GetDlgItem(dialogHwnd_, IDC_STOP), FALSE);
        setWindowTextU8(GetDlgItem(dialogHwnd_, IDC_STATUS), "Stopped.");
    }
}

void BenchmarkMenu::viewBenchmarkResults() {
    openBenchmarkDialog();
}

// ============================================================================
// Window procedure
// ============================================================================

static LRESULT CALLBACK BenchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* menu = reinterpret_cast<BenchmarkMenu*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_CREATE: {
            menu = reinterpret_cast<BenchmarkMenu*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (!menu) return 0;

            // Create left-side selector controls.
            menu->ensureSelectorAttached(hwnd);

            // Buttons (run/stop/close) live directly on the dialog.
            CreateWindowExW(0, L"BUTTON", L"Run Benchmarks",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_RUN, GetModuleHandleW(nullptr), nullptr);
            CreateWindowExW(0, L"BUTTON", L"Stop",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_STOP, GetModuleHandleW(nullptr), nullptr);
            EnableWindow(GetDlgItem(hwnd, IDC_STOP), FALSE);
            CreateWindowExW(0, L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_CLOSE, GetModuleHandleW(nullptr), nullptr);

            // Right-side outputs.
            HWND hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_LOG_EDIT, GetModuleHandleW(nullptr), nullptr);

            menu->ensureLogAttached(hLog);
            menu->ensureResultsAttached(hwnd);

            HWND hStatus = CreateWindowExW(0, L"STATIC", L"Ready.",
                WS_CHILD | WS_VISIBLE,
                0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)IDC_STATUS, GetModuleHandleW(nullptr), nullptr);
            (void)hStatus;

            // Create runner if not present.
            menu->initialize();

            layoutControls(hwnd);
            return 0;
        }
        case WM_SIZE: {
            layoutControls(hwnd);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            if (!menu) return 0;
            switch (id) {
                case IDC_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                case IDC_SELECT_ALL:
                    if (auto* s = menu->ensureSelectorAttached(hwnd)) s->selectAll();
                    return 0;
                case IDC_SELECT_NONE:
                    if (auto* s = menu->ensureSelectorAttached(hwnd)) s->deselectAll();
                    return 0;
                case IDC_MODEL_BROWSE: {
                    auto* s = menu->ensureSelectorAttached(hwnd);
                    std::string cur = s ? s->getModelPath() : "";
                    std::string p = openModelDialog(hwnd, cur);
                    if (!p.empty() && s) s->setModelPath(p);
                    return 0;
                }
                case IDC_RUN:
                    menu->runSelectedBenchmarks();
                    return 0;
                case IDC_STOP:
                    menu->stopBenchmarks();
                    return 0;
            }
            return 0;
        }
        case WM_BENCH_LOG: {
            if (!menu) return 0;
            auto* m = reinterpret_cast<BenchLogMsg*>(lParam);
            auto* lo = menu->ensureLogAttached(GetDlgItem(hwnd, IDC_LOG_EDIT));
            if (m && lo) {
                BenchmarkLogOutput::LogLevel lvl = BenchmarkLogOutput::INFO;
                if (m->level <= 0) lvl = BenchmarkLogOutput::DEBUG;
                else if (m->level == 1) lvl = BenchmarkLogOutput::INFO;
                else if (m->level == 2) lvl = BenchmarkLogOutput::SUCCESS;
                else if (m->level == 3) lvl = BenchmarkLogOutput::WARNING;
                else lvl = BenchmarkLogOutput::LOG_ERROR;
                lo->logMessage(m->text, lvl);
            }
            delete m;
            return 0;
        }
        case WM_BENCH_PROGRESS: {
            if (!menu) return 0;
            auto* rd = menu->ensureResultsAttached(hwnd);
            if (!rd) return 0;
            const int cur = (int)wParam;
            const int total = (int)lParam;
            (void)total;
            rd->updateProgress(cur);
            return 0;
        }
        case WM_BENCH_TEST_DONE: {
            if (!menu) {
                delete reinterpret_cast<BenchTestDoneMsg*>(lParam);
                return 0;
            }
            auto* m = reinterpret_cast<BenchTestDoneMsg*>(lParam);
            auto* lo = menu->ensureLogAttached(GetDlgItem(hwnd, IDC_LOG_EDIT));
            auto* rd = menu->ensureResultsAttached(hwnd);
            if (lo) lo->logTestResult(m->name, m->passed, m->avgLatencyMs);
            if (rd) rd->addResult(m->name, m->passed, m->avgLatencyMs, m->avgLatencyMs, m->passed ? 100.0 : 0.0);
            delete m;
            return 0;
        }
        case WM_BENCH_FINISHED: {
            if (!menu) {
                delete reinterpret_cast<BenchFinishedMsg*>(lParam);
                return 0;
            }
            auto* m = reinterpret_cast<BenchFinishedMsg*>(lParam);
            auto* rd = menu->ensureResultsAttached(hwnd);
            auto* lo = menu->ensureLogAttached(GetDlgItem(hwnd, IDC_LOG_EDIT));
            if (rd) rd->showSummary(m->passed, m->total, m->sec);
            if (lo) lo->logMessage("Benchmark run finished.", BenchmarkLogOutput::SUCCESS);

            EnableWindow(GetDlgItem(hwnd, IDC_RUN), TRUE);
            EnableWindow(GetDlgItem(hwnd, IDC_STOP), FALSE);
            setWindowTextU8(GetDlgItem(hwnd, IDC_STATUS), "Finished.");

            menu->notifyFinished();
            delete m;
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (menu) menu->stopBenchmarks();
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
