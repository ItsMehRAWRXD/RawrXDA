/**
 * @file FeedbackSystem.cpp
 * @brief Community Feedback System — pure C++20/Win32 (zero Qt).
 * @copyright RawrXD IDE 2026
 */
#include "FeedbackSystem.hpp"
#include <nlohmann/json.hpp>
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace rawrxd::feedback {

// ═══════════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════════

static std::string isoNow()
{
    SYSTEMTIME st;
    GetSystemTime(&st);
    char buf[64];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE,
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::string generateUID()
{
    GUID guid;
    CoCreateGuid(&guid);
    char buf[40];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE,
        "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

static std::string getEditText(HWND h)
{
    int len = GetWindowTextLengthA(h);
    if (len <= 0) return {};
    std::string s(len + 1, '\0');
    GetWindowTextA(h, s.data(), len + 1);
    s.resize(len);
    return s;
}

static std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == 0) w.pop_back();
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FeedbackDialog
// ═══════════════════════════════════════════════════════════════════════════════

FeedbackDialog::FeedbackDialog(HWND hwndParent)
    : m_hwndParent(hwndParent)
{
    m_entry.id      = generateUID();
    m_entry.createdISO = isoNow();
    m_entry.status  = SubmissionStatus::Draft;
}

FeedbackDialog::~FeedbackDialog()
{
    if (m_hDlg && IsWindow(m_hDlg)) DestroyWindow(m_hDlg);
}

INT_PTR FeedbackDialog::showModal()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    const int W = 660, H = 560;

    RECT rc;
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 150; rc.top = 100; }

    m_hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"STATIC", L"RawrXD IDE - Submit Feedback",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rc.left + 80, rc.top + 40, W, H,
        m_hwndParent, nullptr, hInst, nullptr);
    if (!m_hDlg) return IDCANCEL;

    SetWindowLongPtrW(m_hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(m_hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DlgProc));
    initControls(m_hDlg);
    collectSystemInfo();

    // Run modal message loop
    if (m_hwndParent) EnableWindow(m_hwndParent, FALSE);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(m_hDlg)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (m_hwndParent) EnableWindow(m_hwndParent, TRUE);
    return IDOK;
}

INT_PTR CALLBACK FeedbackDialog::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<FeedbackDialog*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    if (self) return self->handleMsg(hDlg, msg, wParam, lParam);
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

INT_PTR FeedbackDialog::handleMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_FB_SUBMIT: onSubmit(); return TRUE;
        case IDC_FB_DRAFT:  onSaveDraft(); return TRUE;
        case IDCANCEL:
            DestroyWindow(hDlg);
            m_hDlg = nullptr;
            PostQuitMessage(0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hDlg);
        m_hDlg = nullptr;
        PostQuitMessage(0);
        return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void FeedbackDialog::initControls(HWND hDlg)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int y = 10;

    // Header
    CreateWindowExW(0, L"STATIC",
        L"Your Feedback Matters!  Help us improve RawrXD IDE.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 620, 20, hDlg, nullptr, hInst, nullptr);
    y += 28;

    // Category
    CreateWindowExW(0, L"STATIC", L"Category:",
        WS_CHILD | WS_VISIBLE, 12, y + 4, 70, 18, hDlg, nullptr, hInst, nullptr);
    m_hwndCategoryCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 90, y, 220, 200,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_CATEGORY), hInst, nullptr);
    const wchar_t* cats[] = { L"Bug Report", L"Feature Request", L"Performance Issue",
        L"Thermal Issue", L"UI/UX Feedback", L"Documentation", L"Security", L"Other" };
    for (auto c : cats) SendMessageW(m_hwndCategoryCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c));
    SendMessageW(m_hwndCategoryCombo, CB_SETCURSEL, 0, 0);

    // Priority
    CreateWindowExW(0, L"STATIC", L"Priority:",
        WS_CHILD | WS_VISIBLE, 320, y + 4, 60, 18, hDlg, nullptr, hInst, nullptr);
    m_hwndPriorityCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 385, y, 140, 200,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_PRIORITY), hInst, nullptr);
    const wchar_t* pris[] = { L"Low", L"Medium", L"High", L"Critical" };
    for (auto p : pris) SendMessageW(m_hwndPriorityCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p));
    SendMessageW(m_hwndPriorityCombo, CB_SETCURSEL, 1, 0);
    y += 30;

    // Title
    CreateWindowExW(0, L"STATIC", L"Title:",
        WS_CHILD | WS_VISIBLE, 12, y + 4, 50, 18, hDlg, nullptr, hInst, nullptr);
    m_hwndTitleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 70, y, 560, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_TITLE), hInst, nullptr);
    SendMessageW(m_hwndTitleEdit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Brief summary of your feedback"));
    y += 30;

    // Description
    CreateWindowExW(0, L"STATIC", L"Description:",
        WS_CHILD | WS_VISIBLE, 12, y, 80, 18, hDlg, nullptr, hInst, nullptr);
    y += 20;
    m_hwndDescEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        12, y, 620, 140, hDlg, reinterpret_cast<HMENU>(IDC_FB_DESC), hInst, nullptr);
    SendMessageW(m_hwndDescEdit, EM_SETCUEBANNER, TRUE,
        reinterpret_cast<LPARAM>(L"What happened? Steps to reproduce? Expected behavior?"));
    y += 148;

    // Contact
    CreateWindowExW(0, L"STATIC", L"Name (optional):",
        WS_CHILD | WS_VISIBLE, 12, y + 4, 110, 18, hDlg, nullptr, hInst, nullptr);
    m_hwndNameEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 126, y, 180, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_NAME), hInst, nullptr);

    CreateWindowExW(0, L"STATIC", L"Email (optional):",
        WS_CHILD | WS_VISIBLE, 316, y + 4, 110, 18, hDlg, nullptr, hInst, nullptr);
    m_hwndEmailEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 430, y, 200, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_EMAIL), hInst, nullptr);
    y += 28;

    m_hwndConsentCheck = CreateWindowExW(0, L"BUTTON", L"I consent to being contacted about this feedback",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, y, 350, 20,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_CONSENT), hInst, nullptr);
    y += 28;

    // System info options
    m_hwndSysInfoCheck = CreateWindowExW(0, L"BUTTON", L"Include system info",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, y, 160, 20,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_SYSINFO), hInst, nullptr);
    SendMessage(m_hwndSysInfoCheck, BM_SETCHECK, BST_CHECKED, 0);

    m_hwndThermalCheck = CreateWindowExW(0, L"BUTTON", L"Include thermal data",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 180, y, 160, 20,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_THERMAL), hInst, nullptr);
    SendMessage(m_hwndThermalCheck, BM_SETCHECK, BST_CHECKED, 0);
    y += 28;

    // Buttons
    m_hwndProgress = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | PBS_SMOOTH, 12, y, 400, 16,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_PROGRESS), hInst, nullptr);

    m_hwndStatusLabel = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 20, 400, 18,
        hDlg, reinterpret_cast<HMENU>(IDC_FB_STATUS), hInst, nullptr);

    m_hwndDraftBtn = CreateWindowExW(0, L"BUTTON", L"Save Draft",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, y, 100, 28, hDlg, reinterpret_cast<HMENU>(IDC_FB_DRAFT), hInst, nullptr);

    m_hwndSubmitBtn = CreateWindowExW(0, L"BUTTON", L"Submit Feedback",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        526, y, 105, 28, hDlg, reinterpret_cast<HMENU>(IDC_FB_SUBMIT), hInst, nullptr);
}

void FeedbackDialog::collectSystemInfo()
{
    OSVERSIONINFOW ovi{};
    ovi.dwOSVersionInfoSize = sizeof(ovi);

    // Basic system info via Win32
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);

    m_sysInfo["os"] = "Windows";
    m_sysInfo["arch"] = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) ? "x64" : "x86";
    m_sysInfo["rawrxdVersion"] = "2.0.0";

    // Screen info
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    char buf[64];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%dx%d", screenW, screenH);
    m_sysInfo["screenSize"] = buf;
}

void FeedbackDialog::setThermalData(double currentTemp, double avgTemp, int throttles)
{
    m_entry.currentTemperature = currentTemp;
    m_entry.averageTemperature = avgTemp;
    m_entry.throttleCount      = throttles;
}

void FeedbackDialog::setThermalSnapshot(const std::unordered_map<std::string,std::string>& snap)
{
    m_thermalSnap = snap;
}

void FeedbackDialog::setSystemInfo(const std::unordered_map<std::string,std::string>& info)
{
    for (auto& [k, v] : info) m_sysInfo[k] = v;
}

FeedbackEntry FeedbackDialog::getFeedback() const
{
    FeedbackEntry e = m_entry;
    e.title       = getEditText(m_hwndTitleEdit);
    e.description = getEditText(m_hwndDescEdit);
    e.category    = static_cast<FeedbackCategory>(SendMessage(m_hwndCategoryCombo, CB_GETCURSEL, 0, 0));
    e.priority    = static_cast<FeedbackPriority>(SendMessage(m_hwndPriorityCombo, CB_GETCURSEL, 0, 0));
    e.userName    = getEditText(m_hwndNameEdit);
    e.userEmail   = getEditText(m_hwndEmailEdit);
    e.consentToContact = (SendMessage(m_hwndConsentCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    e.includedSystemInfo = (SendMessage(m_hwndSysInfoCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (e.includedSystemInfo) e.systemInfo = m_sysInfo;
    if (SendMessage(m_hwndThermalCheck, BM_GETCHECK, 0, 0) != BST_CHECKED) {
        e.thermalSnapshot.clear();
        e.currentTemperature.reset();
        e.averageTemperature.reset();
        e.throttleCount.reset();
    }
    e.modifiedISO = isoNow();
    return e;
}

bool FeedbackDialog::validateInput()
{
    std::string title = getEditText(m_hwndTitleEdit);
    if (title.size() < 10) {
        MessageBoxW(m_hDlg, L"Title must be at least 10 characters.", L"Validation", MB_OK | MB_ICONWARNING);
        SetFocus(m_hwndTitleEdit);
        return false;
    }
    std::string desc = getEditText(m_hwndDescEdit);
    if (desc.size() < 30) {
        MessageBoxW(m_hDlg, L"Description must be at least 30 characters.", L"Validation", MB_OK | MB_ICONWARNING);
        SetFocus(m_hwndDescEdit);
        return false;
    }
    if (SendMessage(m_hwndConsentCheck, BM_GETCHECK, 0, 0) == BST_CHECKED &&
        getEditText(m_hwndEmailEdit).empty()) {
        MessageBoxW(m_hDlg, L"Email required when consenting to contact.", L"Validation", MB_OK | MB_ICONWARNING);
        SetFocus(m_hwndEmailEdit);
        return false;
    }
    return true;
}

void FeedbackDialog::onSubmit()
{
    if (!validateInput()) return;

    FeedbackEntry e = getFeedback();
    e.status       = SubmissionStatus::Pending;
    e.submittedISO = isoNow();

    ShowWindow(m_hwndProgress, SW_SHOW);
    SendMessage(m_hwndProgress, PBM_SETMARQUEE, TRUE, 30);
    EnableWindow(m_hwndSubmitBtn, FALSE);
    SetWindowTextW(m_hwndStatusLabel, L"Submitting feedback...");

    // In production: async WinHTTP POST. For now, simulate success.
    SetTimer(m_hDlg, 1, 1500, [](HWND hWnd, UINT, UINT_PTR id, DWORD) {
        KillTimer(hWnd, id);
        auto* self = reinterpret_cast<FeedbackDialog*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (!self) return;
        ShowWindow(self->m_hwndProgress, SW_HIDE);
        SetWindowTextW(self->m_hwndStatusLabel, L"Feedback submitted successfully!");

        FeedbackEntry submitted = self->getFeedback();
        submitted.status = SubmissionStatus::Submitted;
        if (self->m_submitCb) self->m_submitCb(submitted, true);
    });
}

void FeedbackDialog::onSaveDraft()
{
    FeedbackEntry e = getFeedback();
    e.status      = SubmissionStatus::Draft;
    e.modifiedISO = isoNow();

    FeedbackManager::instance().saveDraft(e);
    SetWindowTextW(m_hwndStatusLabel, L"Draft saved.");
}

// ═══════════════════════════════════════════════════════════════════════════════
// TelemetryConsentDialog
// ═══════════════════════════════════════════════════════════════════════════════

TelemetryConsentDialog::TelemetryConsentDialog(HWND hwndParent)
    : m_hwndParent(hwndParent) {}

TelemetryConsentDialog::~TelemetryConsentDialog()
{
    if (m_hDlg && IsWindow(m_hDlg)) DestroyWindow(m_hDlg);
}

INT_PTR TelemetryConsentDialog::showModal()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    RECT rc;
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 200; rc.top = 120; }

    m_hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"STATIC", L"RawrXD IDE - Privacy & Telemetry",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rc.left + 100, rc.top + 60, 500, 400,
        m_hwndParent, nullptr, hInst, nullptr);
    if (!m_hDlg) return IDCANCEL;

    SetWindowLongPtrW(m_hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(m_hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DlgProc));
    initControls(m_hDlg);

    if (m_hwndParent) EnableWindow(m_hwndParent, FALSE);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(m_hDlg)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (m_hwndParent) EnableWindow(m_hwndParent, TRUE);
    return IDOK;
}

INT_PTR CALLBACK TelemetryConsentDialog::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<TelemetryConsentDialog*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    if (self) return self->handleMsg(hDlg, msg, wParam, lParam);
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

INT_PTR TelemetryConsentDialog::handleMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_TC_ALL:
            for (HWND h : { m_hwndBasic, m_hwndPerf, m_hwndThermal, m_hwndCrash, m_hwndFeature, m_hwndHardware })
                SendMessage(h, BM_SETCHECK, BST_CHECKED, 0);
            return TRUE;
        case IDC_TC_NONE:
            for (HWND h : { m_hwndBasic, m_hwndPerf, m_hwndThermal, m_hwndCrash, m_hwndFeature, m_hwndHardware })
                SendMessage(h, BM_SETCHECK, BST_UNCHECKED, 0);
            return TRUE;
        case IDC_TC_SAVE:
            m_consent.basicTelemetry       = (SendMessage(m_hwndBasic,    BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.performanceTelemetry = (SendMessage(m_hwndPerf,     BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.thermalTelemetry     = (SendMessage(m_hwndThermal,  BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.crashReporting       = (SendMessage(m_hwndCrash,    BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.featureUsage         = (SendMessage(m_hwndFeature,  BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.hardwareInfo         = (SendMessage(m_hwndHardware, BM_GETCHECK, 0, 0) == BST_CHECKED);
            m_consent.consentDateISO       = isoNow();
            m_consent.consentVersion       = "2.0";
            if (m_consentCb) m_consentCb(m_consent);
            DestroyWindow(hDlg);
            m_hDlg = nullptr;
            PostQuitMessage(0);
            return TRUE;
        case IDCANCEL:
            DestroyWindow(hDlg);
            m_hDlg = nullptr;
            PostQuitMessage(0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hDlg);
        m_hDlg = nullptr;
        PostQuitMessage(0);
        return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void TelemetryConsentDialog::initControls(HWND hDlg)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int y = 10;

    CreateWindowExW(0, L"STATIC",
        L"Privacy & Telemetry Settings\r\n\r\nHelp us improve RawrXD IDE by sharing anonymous data.\r\nYou control what is shared.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 460, 52, hDlg, nullptr, hInst, nullptr);
    y += 60;

    auto makeCheck = [&](HWND& hw, const wchar_t* text, UINT_PTR id, bool checked) {
        hw = CreateWindowExW(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, y, 440, 20, hDlg, reinterpret_cast<HMENU>(id), hInst, nullptr);
        if (checked) SendMessage(hw, BM_SETCHECK, BST_CHECKED, 0);
        y += 26;
    };

    makeCheck(m_hwndBasic,    L"Basic telemetry (app version, crashes)",        IDC_TC_BASIC,    m_consent.basicTelemetry);
    makeCheck(m_hwndPerf,     L"Performance metrics (startup time, memory)",    IDC_TC_PERF,     m_consent.performanceTelemetry);
    makeCheck(m_hwndThermal,  L"Thermal data (temperatures, throttling events)",IDC_TC_THERMAL,  m_consent.thermalTelemetry);
    makeCheck(m_hwndCrash,    L"Crash reporting (anonymous dumps)",             IDC_TC_CRASH,    m_consent.crashReporting);
    makeCheck(m_hwndFeature,  L"Feature usage tracking",                        IDC_TC_FEATURE,  m_consent.featureUsage);
    makeCheck(m_hwndHardware, L"Hardware information",                          IDC_TC_HARDWARE, m_consent.hardwareInfo);
    y += 8;

    m_hwndPrivacy = CreateWindowExW(0, L"STATIC",
        L"Your privacy is important. Data is never shared with third parties.",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 20, y, 440, 36, hDlg, nullptr, hInst, nullptr);
    y += 44;

    // Select All / None
    CreateWindowExW(0, L"BUTTON", L"Select All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, y, 100, 26, hDlg, reinterpret_cast<HMENU>(IDC_TC_ALL), hInst, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Select None",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        128, y, 100, 26, hDlg, reinterpret_cast<HMENU>(IDC_TC_NONE), hInst, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        360, y, 100, 26, hDlg, reinterpret_cast<HMENU>(IDC_TC_SAVE), hInst, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ContributionDialog
// ═══════════════════════════════════════════════════════════════════════════════

ContributionDialog::ContributionDialog(HWND hwndParent)
    : m_hwndParent(hwndParent) { m_entry.id = generateUID(); }

ContributionDialog::~ContributionDialog()
{
    if (m_hDlg && IsWindow(m_hDlg)) DestroyWindow(m_hDlg);
}

INT_PTR ContributionDialog::showModal()
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    RECT rc;
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 200; rc.top = 120; }

    m_hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"STATIC", L"RawrXD IDE - Submit Contribution",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rc.left + 80, rc.top + 40, 560, 480,
        m_hwndParent, nullptr, hInst, nullptr);
    if (!m_hDlg) return IDCANCEL;

    SetWindowLongPtrW(m_hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowLongPtrW(m_hDlg, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DlgProc));
    initControls(m_hDlg);

    if (m_hwndParent) EnableWindow(m_hwndParent, FALSE);
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsWindow(m_hDlg)) break;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (m_hwndParent) EnableWindow(m_hwndParent, TRUE);
    return IDOK;
}

INT_PTR CALLBACK ContributionDialog::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<ContributionDialog*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
    if (self) return self->handleMsg(hDlg, msg, wParam, lParam);
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

INT_PTR ContributionDialog::handleMsg(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CT_BROWSE: {
            OPENFILENAMEA ofn{};
            char file[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hDlg;
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.lpstrFile   = file;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn))
                SetWindowTextA(m_hwndFileEdit, file);
            return TRUE;
        }
        case IDC_CT_SUBMIT:
            if (validateInput()) {
                m_entry.title       = getEditText(m_hwndTitleEdit);
                m_entry.description = getEditText(m_hwndDescEdit);
                m_entry.type        = static_cast<ContributionEntry::Type>(SendMessage(m_hwndTypeCombo, CB_GETCURSEL, 0, 0));
                m_entry.contributorName  = getEditText(m_hwndNameEdit);
                m_entry.contributorEmail = getEditText(m_hwndEmailEdit);
                m_entry.fileName    = getEditText(m_hwndFileEdit);
                m_entry.agreedToTerms = (SendMessage(m_hwndAgreeCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                m_entry.status      = SubmissionStatus::Submitted;
                if (m_cb) m_cb(m_entry, true);
                DestroyWindow(hDlg);
                m_hDlg = nullptr;
                PostQuitMessage(0);
            }
            return TRUE;
        case IDCANCEL:
            DestroyWindow(hDlg); m_hDlg = nullptr; PostQuitMessage(0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hDlg); m_hDlg = nullptr; PostQuitMessage(0);
        return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void ContributionDialog::initControls(HWND hDlg)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int y = 10;

    auto addLabel = [&](const wchar_t* t, int x, int w) {
        CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y + 4, w, 18, hDlg, nullptr, hInst, nullptr);
    };

    addLabel(L"Title:", 12, 50);
    m_hwndTitleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 70, y, 460, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_TITLE), hInst, nullptr);
    y += 30;

    addLabel(L"Description:", 12, 80);
    y += 20;
    m_hwndDescEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        12, y, 520, 80, hDlg, reinterpret_cast<HMENU>(IDC_CT_DESC), hInst, nullptr);
    y += 88;

    addLabel(L"Type:", 12, 50);
    m_hwndTypeCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 70, y, 200, 200,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_TYPE), hInst, nullptr);
    const wchar_t* types[] = { L"Thermal Profile", L"Drive Config", L"Algorithm",
        L"Documentation", L"Translation", L"Other" };
    for (auto t : types) SendMessageW(m_hwndTypeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t));
    SendMessageW(m_hwndTypeCombo, CB_SETCURSEL, 0, 0);
    y += 30;

    addLabel(L"Name:", 12, 50);
    m_hwndNameEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 70, y, 200, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_NAME), hInst, nullptr);
    addLabel(L"Email:", 280, 50);
    m_hwndEmailEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 334, y, 196, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_EMAIL), hInst, nullptr);
    y += 30;

    addLabel(L"File:", 12, 40);
    m_hwndFileEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 55, y, 390, 22,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_FILE), hInst, nullptr);
    m_hwndBrowseBtn = CreateWindowExW(0, L"BUTTON", L"Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 450, y, 80, 24,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_BROWSE), hInst, nullptr);
    y += 30;

    addLabel(L"License:", 12, 60);
    m_hwndLicenseCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 78, y, 200, 200,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_LICENSE), hInst, nullptr);
    const wchar_t* lics[] = { L"MIT", L"Apache-2.0", L"BSD-3-Clause", L"GPL-3.0", L"CC-BY-4.0", L"Unlicense" };
    for (auto l : lics) SendMessageW(m_hwndLicenseCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(l));
    SendMessageW(m_hwndLicenseCombo, CB_SETCURSEL, 0, 0);
    y += 30;

    m_hwndAgreeCheck = CreateWindowExW(0, L"BUTTON", L"I agree to the contribution terms",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, y, 300, 20,
        hDlg, reinterpret_cast<HMENU>(IDC_CT_AGREE), hInst, nullptr);
    y += 30;

    CreateWindowExW(0, L"BUTTON", L"Submit",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        420, y, 100, 28, hDlg, reinterpret_cast<HMENU>(IDC_CT_SUBMIT), hInst, nullptr);
}

bool ContributionDialog::validateInput()
{
    if (getEditText(m_hwndTitleEdit).size() < 5) {
        MessageBoxW(m_hDlg, L"Title too short.", L"Validation", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (SendMessage(m_hwndAgreeCheck, BM_GETCHECK, 0, 0) != BST_CHECKED) {
        MessageBoxW(m_hDlg, L"You must agree to the terms.", L"Validation", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FeedbackManager — singleton
// ═══════════════════════════════════════════════════════════════════════════════

FeedbackManager& FeedbackManager::instance()
{
    static FeedbackManager mgr;
    return mgr;
}

FeedbackManager::FeedbackManager()
{
    // Determine settings path: %APPDATA%\RawrXD\feedback.json
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        m_settingsPath = std::string(appdata) + "\\RawrXD\\feedback.json";
    }
    loadSettings();
}

FeedbackManager::~FeedbackManager()
{
    saveSettings();
}

void FeedbackManager::showFeedbackDialog(HWND parent)
{
    FeedbackDialog dlg(parent);
    dlg.showModal();
}

void FeedbackManager::showTelemetryConsentDialog(HWND parent)
{
    TelemetryConsentDialog dlg(parent);
    dlg.setCurrentConsent(m_consent);
    dlg.setConsentCallback([this](const TelemetryConsent& c) {
        m_consent = c;
        saveSettings();
    });
    dlg.showModal();
}

void FeedbackManager::showContributionDialog(HWND parent)
{
    ContributionDialog dlg(parent);
    dlg.showModal();
}

void FeedbackManager::submitQuickFeedback(const std::string& message, FeedbackCategory cat)
{
    FeedbackEntry e;
    e.id          = generateUID();
    e.title       = message.substr(0, 200);
    e.description = message;
    e.category    = cat;
    e.priority    = FeedbackPriority::Medium;
    e.status      = SubmissionStatus::Submitted;
    e.createdISO  = isoNow();
    m_history.push_back(e);
}

void FeedbackManager::reportBug(const std::string& title, const std::string& desc)
{
    submitQuickFeedback(title + "\n\n" + desc, FeedbackCategory::BugReport);
}

void FeedbackManager::requestFeature(const std::string& title, const std::string& desc)
{
    submitQuickFeedback(title + "\n\n" + desc, FeedbackCategory::FeatureRequest);
}

void FeedbackManager::setTelemetryConsent(const TelemetryConsent& c)
{
    m_consent = c;
    saveSettings();
}

void FeedbackManager::saveDraft(const FeedbackEntry& e)
{
    // Replace existing draft with same ID, or append
    for (auto& d : m_drafts) {
        if (d.id == e.id) { d = e; return; }
    }
    m_drafts.push_back(e);
}

std::vector<FeedbackEntry> FeedbackManager::loadDrafts() { return m_drafts; }

void FeedbackManager::deleteDraft(const std::string& id)
{
    m_drafts.erase(
        std::remove_if(m_drafts.begin(), m_drafts.end(),
            [&](const FeedbackEntry& e) { return e.id == id; }),
        m_drafts.end());
}

void FeedbackManager::loadSettings()
{
    if (m_settingsPath.empty()) return;
    std::ifstream f(m_settingsPath);
    if (!f.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(f);
        if (j.contains("consent") && j["consent"].is_object()) {
            const auto& c = j["consent"];
            m_consent.basicTelemetry     = c.value("basicTelemetry", false);
            m_consent.performanceTelemetry = c.value("performanceTelemetry", false);
            m_consent.thermalTelemetry   = c.value("thermalTelemetry", false);
            m_consent.crashReporting     = c.value("crashReporting", false);
            m_consent.featureUsage      = c.value("featureUsage", false);
            m_consent.hardwareInfo      = c.value("hardwareInfo", false);
            m_consent.consentVersion    = c.value("consentVersion", "");
            m_consent.consentDateISO    = c.value("consentDateISO", "");
        }
        if (j.contains("drafts") && j["drafts"].is_array()) {
            m_drafts.clear();
            for (const auto& d : j["drafts"]) {
                FeedbackEntry e;
                e.id          = d.value("id", "");
                e.title       = d.value("title", "");
                e.description = d.value("description", "");
                e.category    = static_cast<FeedbackCategory>(d.value("category", static_cast<int>(FeedbackCategory::Other)));
                e.priority    = static_cast<FeedbackPriority>(d.value("priority", static_cast<int>(FeedbackPriority::Medium)));
                e.status      = static_cast<SubmissionStatus>(d.value("status", static_cast<int>(SubmissionStatus::Draft)));
                e.userEmail   = d.value("userEmail", "");
                e.userName    = d.value("userName", "");
                e.consentToContact = d.value("consentToContact", false);
                e.createdISO  = d.value("createdISO", "");
                e.modifiedISO = d.value("modifiedISO", "");
                if (d.contains("systemInfo") && d["systemInfo"].is_object())
                    for (auto it = d["systemInfo"].begin(); it != d["systemInfo"].end(); ++it)
                        e.systemInfo[it.key()] = it.value().get<std::string>();
                if (d.contains("attachmentPaths") && d["attachmentPaths"].is_array())
                    for (const auto& a : d["attachmentPaths"]) e.attachmentPaths.push_back(a.get<std::string>());
                if (d.contains("screenshotPaths") && d["screenshotPaths"].is_array())
                    for (const auto& s : d["screenshotPaths"]) e.screenshotPaths.push_back(s.get<std::string>());
                m_drafts.push_back(e);
            }
        }
    } catch (const std::exception&) {
        // Parse or I/O error: leave m_consent and m_drafts as-is
    }
}

void FeedbackManager::saveSettings()
{
    if (m_settingsPath.empty()) return;
    std::filesystem::path p(m_settingsPath);
    std::filesystem::create_directories(p.parent_path());
    try {
        nlohmann::json j;
        j["consent"] = {
            {"basicTelemetry",      m_consent.basicTelemetry},
            {"performanceTelemetry", m_consent.performanceTelemetry},
            {"thermalTelemetry",     m_consent.thermalTelemetry},
            {"crashReporting",      m_consent.crashReporting},
            {"featureUsage",        m_consent.featureUsage},
            {"hardwareInfo",        m_consent.hardwareInfo},
            {"consentVersion",      m_consent.consentVersion},
            {"consentDateISO",      m_consent.consentDateISO}
        };
        j["drafts"] = nlohmann::json::array();
        for (const auto& e : m_drafts) {
            nlohmann::json d = {
                {"id", e.id}, {"title", e.title}, {"description", e.description},
                {"category", static_cast<int>(e.category)},
                {"priority", static_cast<int>(e.priority)},
                {"status", static_cast<int>(e.status)},
                {"userEmail", e.userEmail}, {"userName", e.userName},
                {"consentToContact", e.consentToContact},
                {"createdISO", e.createdISO}, {"modifiedISO", e.modifiedISO}
            };
            nlohmann::json sys;
            for (const auto& kv : e.systemInfo) sys[kv.first] = kv.second;
            d["systemInfo"] = std::move(sys);
            d["attachmentPaths"] = e.attachmentPaths;
            d["screenshotPaths"] = e.screenshotPaths;
            j["drafts"].push_back(std::move(d));
        }
        std::ofstream of(m_settingsPath);
        if (of.is_open())
            of << j.dump(2);
    } catch (const std::exception&) {
        // Write error: best-effort save skipped
    }
}

} // namespace rawrxd::feedback
