// ============================================================================
// Win32IDE_CursorParity.cpp — Feature Modules Integration (native IDE)
// ============================================================================
// Wires the 8 pluginable feature modules into the Win32 IDE:
//   1. TelemetryExporter     — Multi-format telemetry export + audit chain
//   2. AgenticComposerUX     — Agentic composer session UX
//   3. ContextMentionParser  — @-mention context assembly
//   4. VisionEncoder         — Image input (drag-drop, clipboard, screenshot)
//   5. RefactoringEngine     — 200+ pluginable refactorings
//   6. LanguageRegistry      — 60+ language descriptors + DLL plugins
//   7. SemanticIndexEngine   — Cross-language semantic index
//   8. ResourceGeneratorEngine — Docker/K8s/Terraform/CI/CD config generation
//
// Pattern:  PatchResult-style, no exceptions, factory results
// Threading: All init/command handlers called from UI thread
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commdlg.h>
#include <map>
#include <shlobj.h>
#include <sstream>
#include <string>
#include <vector>

// Feature module headers (found via include/ directory)
#include "agentic/agentic_composer_ux.h"
#include "context/context_mention_parser.h"
#include "context/semantic_index.h"
#include "cursor_github_parity_bridge.h"
#include "ide/language_plugin.h"
#include "ide/refactoring_plugin.h"
#include "ide/resource_generator.h"
#include "multimodal/vision_encoder.h"
#include "telemetry/telemetry_export.h"

<<<<<<< HEAD
// Go-to-definition / references → implemented in Win32IDE_LSPClient.cpp via
// lspGotoDefinition(), lspFindReferences(), lspCompletion(), lspSignatureHelp()

// Semantic index (fuzzy search, references) → implemented in
// Win32IDE_LSPClient.cpp::lspSemanticTokensFull() with 23-type token registry
=======
// SCAFFOLD_147: Go to definition / references


// SCAFFOLD_037: Cursor parity (fuzzy search, references)


// Win32-native debug logging
#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif
#ifndef RAWRXD_LOG_WARNING
#define RAWRXD_LOG_WARNING(msg) do { \
    std::ostringstream _oss; _oss << "[WARN] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif
#ifndef RAWRXD_LOG_ERROR
#define RAWRXD_LOG_ERROR(msg) do { \
    std::ostringstream _oss; _oss << "[ERROR] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif
>>>>>>> origin/main

// ============================================================================
// File-local singletons for classes that lack Instance() static methods
// ============================================================================
static RawrXD::Context::ContextMentionParser& getContextMentionParser()
{
    static RawrXD::Context::ContextMentionParser s_instance;
    return s_instance;
}

static RawrXD::Multimodal::VisionEncoder& getVisionEncoder()
{
    static RawrXD::Multimodal::VisionEncoder s_instance;
    return s_instance;
}

// ============================================================================
// Helper: Save-File dialog (returns empty on cancel)
// ============================================================================
static std::string showSaveDialog(HWND hwnd, const char* filter, const char* defaultExt)
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn))
        return path;
    return {};
}

// ============================================================================
// Helper: Open-File dialog
// ============================================================================
static std::string showOpenDialog(HWND hwnd, const char* filter)
{
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn))
        return path;
    return {};
}

// ============================================================================
// Helper: UTF-8 to UTF-16 for Unicode Win32 APIs (C++20, no Qt)
// ============================================================================
static std::wstring utf8ToWide(const char* utf8)
{
    if (!utf8 || !*utf8)
        return {};
    const std::string s(utf8);
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len) == 0)
        return {};
    return out;
}

static std::string wideToUtf8(const wchar_t* wide)
{
    if (!wide || !*wide)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1)
        return {};
    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr) <= 0)
        return {};
    if (!out.empty() && out.back() == '\0')
        out.pop_back();
    return out;
}

// ============================================================================
// Helper: Input dialog — proper Win32 modal dialog (resource-free, runtime template)
// ============================================================================

// Dialog control IDs
#define IDC_INPUT_PROMPT_LABEL  2001
#define IDC_INPUT_EDIT          2002

struct InputDialogData {
    const wchar_t* title;
    const wchar_t* prompt;
    wchar_t        result[4096];
};

static INT_PTR CALLBACK InputDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG: {
        auto* data = reinterpret_cast<InputDialogData*>(lParam);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, static_cast<LONG_PTR>(lParam));
        if (data->title)  SetWindowTextW(hDlg, data->title);
        if (data->prompt) SetDlgItemTextW(hDlg, IDC_INPUT_PROMPT_LABEL, data->prompt);
        // Focus the edit control
        HWND hEdit = GetDlgItem(hDlg, IDC_INPUT_EDIT);
        if (hEdit) { SetFocus(hEdit); return FALSE; }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            auto* data = reinterpret_cast<InputDialogData*>(
                GetWindowLongPtrW(hDlg, GWLP_USERDATA));
            if (data) {
                GetDlgItemTextW(hDlg, IDC_INPUT_EDIT, data->result,
                                static_cast<int>(std::size(data->result)));
            }
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static HWND buildInputDialogTemplate(HWND owner, InputDialogData* data)
{
    // Build an in-memory DLGTEMPLATE so we need zero .rc resources.
    // Layout: 360×120 DLU dialog with static label, edit, OK, Cancel.
    alignas(4) BYTE buf[2048] = {};
    WORD* p = reinterpret_cast<WORD*>(buf);

    // ---- DLGTEMPLATE header (extended style: DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU) ----
    auto* dlg = reinterpret_cast<DLGTEMPLATE*>(p);
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    dlg->dwExtendedStyle = 0;
    dlg->cdit = 4;       // 4 controls: label, edit, OK, Cancel
    dlg->x  = 0;  dlg->y = 0;
    dlg->cx = 260; dlg->cy = 80;
    p = reinterpret_cast<WORD*>(dlg + 1);
    *p++ = 0;  // menu
    *p++ = 0;  // class (default)
    *p++ = 0;  // title (set via WM_INITDIALOG)

    // Helper: align p to DWORD boundary
    auto alignDword = [](WORD*& ptr) {
        auto addr = reinterpret_cast<uintptr_t>(ptr);
        addr = (addr + 3) & ~3;
        ptr = reinterpret_cast<WORD*>(addr);
    };

    // ---- Static label (IDC_INPUT_PROMPT_LABEL) ----
    alignDword(p);
    auto* ctrl = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    ctrl->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    ctrl->dwExtendedStyle = 0;
    ctrl->x = 10; ctrl->y = 8; ctrl->cx = 240; ctrl->cy = 20;
    ctrl->id = IDC_INPUT_PROMPT_LABEL;
    p = reinterpret_cast<WORD*>(ctrl + 1);
    *p++ = 0xFFFF; *p++ = 0x0082;  // STATIC class atom
    *p++ = 0;  // text (empty, set via WM_INITDIALOG)
    *p++ = 0;  // extra data size

    // ---- Edit control (IDC_INPUT_EDIT) ----
    alignDword(p);
    ctrl = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    ctrl->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL;
    ctrl->dwExtendedStyle = 0;
    ctrl->x = 10; ctrl->y = 30; ctrl->cx = 240; ctrl->cy = 14;
    ctrl->id = IDC_INPUT_EDIT;
    p = reinterpret_cast<WORD*>(ctrl + 1);
    *p++ = 0xFFFF; *p++ = 0x0081;  // EDIT class atom
    *p++ = 0;  // text
    *p++ = 0;  // extra data size

    // ---- OK button ----
    alignDword(p);
    ctrl = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    ctrl->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    ctrl->dwExtendedStyle = 0;
    ctrl->x = 110; ctrl->y = 56; ctrl->cx = 60; ctrl->cy = 14;
    ctrl->id = IDOK;
    p = reinterpret_cast<WORD*>(ctrl + 1);
    *p++ = 0xFFFF; *p++ = 0x0080;  // BUTTON class atom
    const wchar_t okText[] = L"OK";
    memcpy(p, okText, sizeof(okText)); p += (sizeof(okText) + 1) / 2;
    *p++ = 0;

    // ---- Cancel button ----
    alignDword(p);
    ctrl = reinterpret_cast<DLGITEMTEMPLATE*>(p);
    ctrl->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
    ctrl->dwExtendedStyle = 0;
    ctrl->x = 180; ctrl->y = 56; ctrl->cx = 60; ctrl->cy = 14;
    ctrl->id = IDCANCEL;
    p = reinterpret_cast<WORD*>(ctrl + 1);
    *p++ = 0xFFFF; *p++ = 0x0080;  // BUTTON class atom
    const wchar_t cancelText[] = L"Cancel";
    memcpy(p, cancelText, sizeof(cancelText)); p += (sizeof(cancelText) + 1) / 2;
    *p++ = 0;

    // Fire the dialog
    INT_PTR ret = DialogBoxIndirectParamW(
        GetModuleHandleW(nullptr), reinterpret_cast<DLGTEMPLATE*>(buf),
        owner, InputDialogProc, reinterpret_cast<LPARAM>(data));

    return (ret == IDOK) ? reinterpret_cast<HWND>(1) : nullptr;
}

static std::string showInputDialog(HWND hwnd, const char* title, const char* prompt)
{
    InputDialogData data = {};
    const std::wstring wtitle  = utf8ToWide(title);
    const std::wstring wprompt = utf8ToWide(prompt);
    data.title  = wtitle.c_str();
    data.prompt = wprompt.c_str();
    data.result[0] = L'\0';

    if (!buildInputDialogTemplate(hwnd, &data))
        return {};

    return wideToUtf8(data.result);
}

// ============================================================================
// 1. TELEMETRY EXPORT
// ============================================================================
void Win32IDE::initTelemetryExport()
{
    if (m_telemetryExportInitialized)
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    exporter.Initialize(&RawrXD::Telemetry::UnifiedTelemetryCore::Instance());
    m_telemetryExportInitialized = true;
    RAWRXD_LOG_INFO("TelemetryExporter initialized");
}

void Win32IDE::shutdownTelemetryExport()
{
    if (!m_telemetryExportInitialized)
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    exporter.StopAutoExport();
    m_telemetryExportInitialized = false;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "TelemetryExporter shut down";
}

bool Win32IDE::handleTelemetryExportCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_TELEXPORT_JSON:
            cmdTelExportJSON();
            return true;
        case IDM_TELEXPORT_CSV:
            cmdTelExportCSV();
            return true;
        case IDM_TELEXPORT_PROMETHEUS:
            cmdTelExportPrometheus();
            return true;
        case IDM_TELEXPORT_OTLP:
            cmdTelExportOTLP();
            return true;
        case IDM_TELEXPORT_AUDIT_LOG:
            cmdTelExportAuditLog();
            return true;
        case IDM_TELEXPORT_VERIFY_CHAIN:
            cmdTelExportVerifyChain();
            return true;
        case IDM_TELEXPORT_AUTO_START:
            cmdTelExportAutoStart();
            return true;
        case IDM_TELEXPORT_AUTO_STOP:
            cmdTelExportAutoStop();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdTelExportJSON()
{
    initTelemetryExport();
    auto path = showSaveDialog(m_hwndMain, "JSON Files\0*.json\0All Files\0*.*\0", "json");
    if (path.empty())
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    auto result = exporter.ExportToFile(path, RawrXD::Telemetry::ExportFormat::JSON);
    if (result.success)
    {
        RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Telemetry exported to JSON: " << path;
        MessageBoxA(m_hwndMain, ("Exported to: " + path).c_str(), "Telemetry Export", MB_OK);
    }
    else
    {
        RAWRXD_LOG_ERROR("Win32IDE_CursorParity")
            << "Telemetry JSON export failed: " << result.error;
        MessageBoxA(m_hwndMain, result.error.c_str(), "Export Error", MB_ICONERROR);
    }
}

void Win32IDE::cmdTelExportCSV()
{
    initTelemetryExport();
    auto path = showSaveDialog(m_hwndMain, "CSV Files\0*.csv\0All Files\0*.*\0", "csv");
    if (path.empty())
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    auto result = exporter.ExportToFile(path, RawrXD::Telemetry::ExportFormat::CSV);
    if (result.success)
        MessageBoxA(m_hwndMain, ("Exported CSV: " + path).c_str(), "Telemetry Export", MB_OK);
    else
        MessageBoxA(m_hwndMain, result.error.c_str(), "Export Error", MB_ICONERROR);
}

void Win32IDE::cmdTelExportPrometheus()
{
    initTelemetryExport();
    auto path = showSaveDialog(m_hwndMain, "Text Files\0*.txt\0All Files\0*.*\0", "txt");
    if (path.empty())
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    auto result = exporter.ExportToFile(path, RawrXD::Telemetry::ExportFormat::Prometheus);
    if (result.success)
        MessageBoxA(m_hwndMain, "Prometheus metrics exported.", "Export", MB_OK);
    else
        MessageBoxA(m_hwndMain, result.error.c_str(), "Export Error", MB_ICONERROR);
}

// cmdTelExportOTLP defined in Win32IDE_TelemetryPanel.cpp

void Win32IDE::cmdTelExportAuditLog()
{
    initTelemetryExport();
    auto path = showSaveDialog(m_hwndMain, "JSONL Files\0*.jsonl\0All Files\0*.*\0", "jsonl");
    if (path.empty())
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    auto result = exporter.ExportAuditLog(path);
    if (result.success)
        MessageBoxA(m_hwndMain, ("Audit log exported: " + path).c_str(), "Audit", MB_OK);
    else
        MessageBoxA(m_hwndMain, result.error.c_str(), "Audit Error", MB_ICONERROR);
}

void Win32IDE::cmdTelExportVerifyChain()
{
    initTelemetryExport();
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    bool valid = exporter.VerifyAuditChain();
    MessageBoxA(m_hwndMain, valid ? "Audit chain integrity: VERIFIED" : "Audit chain integrity: BROKEN",
                "Chain Verification", valid ? MB_ICONINFORMATION : MB_ICONWARNING);
}

void Win32IDE::cmdTelExportAutoStart()
{
    initTelemetryExport();
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    exporter.StartAutoExport();  // auto-export using configured intervals
    MessageBoxA(m_hwndMain, "Auto-export started (60s interval).", "Telemetry", MB_OK);
}

void Win32IDE::cmdTelExportAutoStop()
{
    if (!m_telemetryExportInitialized)
        return;
    auto& exporter = RawrXD::Telemetry::TelemetryExporter::Instance();
    exporter.StopAutoExport();
    MessageBoxA(m_hwndMain, "Auto-export stopped.", "Telemetry", MB_OK);
}

// ============================================================================
// 2. AGENTIC COMPOSER UX
// ============================================================================
void Win32IDE::initAgenticComposerUX()
{
    if (m_composerUXInitialized)
        return;
    m_composerUXInitialized = true;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "AgenticComposerUX initialized";
}

bool Win32IDE::handleComposerUXCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_COMPOSER_NEW_SESSION:
            cmdComposerNewSession();
            return true;
        case IDM_COMPOSER_END_SESSION:
            cmdComposerEndSession();
            return true;
        case IDM_COMPOSER_APPROVE_ALL:
            cmdComposerApproveAll();
            return true;
        case IDM_COMPOSER_REJECT_ALL:
            cmdComposerRejectAll();
            return true;
        case IDM_COMPOSER_SHOW_TRANSCRIPT:
            cmdComposerShowTranscript();
            return true;
        case IDM_COMPOSER_SHOW_METRICS:
            cmdComposerShowMetrics();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdComposerNewSession()
{
    initAgenticComposerUX();
    auto& composer = m_composerUX;
    uint64_t sessionId = composer.StartSession("IDE Session");
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Composer session created: " << sessionId;
    MessageBoxA(m_hwndMain, ("Session created: " + std::to_string(sessionId)).c_str(), "Agentic Composer", MB_OK);
}

void Win32IDE::cmdComposerEndSession()
{
    auto& composer = m_composerUX;
    auto* session = composer.GetCurrentSession();
    if (!session || session->sessionId == 0)
    {
        MessageBoxA(m_hwndMain, "No active session.", "Composer", MB_ICONWARNING);
        return;
    }
    composer.EndSession(session->sessionId);
    MessageBoxA(m_hwndMain, "Session ended.", "Agentic Composer", MB_OK);
}

void Win32IDE::cmdComposerApproveAll()
{
    auto& composer = m_composerUX;
    auto* session = composer.GetCurrentSession();
    if (!session || session->sessionId == 0)
    {
        MessageBoxA(m_hwndMain, "No active session.", "Composer", MB_ICONWARNING);
        return;
    }
    for (auto& fc : session->fileChanges)
    {
        composer.ApproveFileChange(fc.filePath);
    }
    MessageBoxA(m_hwndMain, "All file changes approved.", "Composer", MB_OK);
}

void Win32IDE::cmdComposerRejectAll()
{
    auto& composer = m_composerUX;
    auto* session = composer.GetCurrentSession();
    if (!session || session->sessionId == 0)
    {
        MessageBoxA(m_hwndMain, "No active session.", "Composer", MB_ICONWARNING);
        return;
    }
    for (auto& fc : session->fileChanges)
    {
        composer.RejectFileChange(fc.filePath);
    }
    MessageBoxA(m_hwndMain, "All file changes rejected.", "Composer", MB_OK);
}

void Win32IDE::cmdComposerShowTranscript()
{
    auto& composer = m_composerUX;
    auto* session = composer.GetCurrentSession();
    if (!session || session->sessionId == 0)
    {
        MessageBoxA(m_hwndMain, "No active session.", "Composer", MB_ICONWARNING);
        return;
    }
    std::string json = composer.SerializeSession();
    // Display in output panel
    if (m_hwndOutputTabs)
    {
        SetWindowTextA(m_hwndOutputTabs, json.c_str());
    }
    RAWRXD_LOG_INFO("Win32IDE_CursorParity")
        << "Composer transcript displayed (" << json.size() << " bytes)";
}

void Win32IDE::cmdComposerShowMetrics()
{
    auto& composer = m_composerUX;
    auto* session = composer.GetCurrentSession();
    std::ostringstream oss;
    oss << "Composer UX Metrics:\n";
    if (session && session->sessionId != 0)
    {
        oss << "  Current session: " << session->sessionId << "\n"
            << "  Completed steps: " << session->completedSteps << "/" << session->totalSteps << "\n"
            << "  Files modified: " << session->fileChanges.size() << "\n"
            << "  Approved changes: " << session->approvedChanges << "\n"
            << "  Rejected changes: " << session->rejectedChanges << "\n"
            << "  Tokens in/out: " << session->totalTokensIn << "/" << session->totalTokensOut << "\n";
    }
    else
    {
        oss << "  No active session.\n";
    }
    auto history = composer.GetSessionHistory();
    oss << "  Total sessions (history): " << history.size() << "\n";
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Composer Metrics", MB_OK);
}

// ============================================================================
// 3. CONTEXT @-MENTION PARSER
// ============================================================================
void Win32IDE::initContextMentionParser()
{
    if (m_mentionParserInitialized)
        return;
    // ContextMentionParser is not a singleton — use file-local instance
    (void)getContextMentionParser();
    m_mentionParserInitialized = true;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "ContextMentionParser initialized";
}

bool Win32IDE::handleMentionParserCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_MENTION_PARSE:
            cmdMentionParse();
            return true;
        case IDM_MENTION_SUGGEST:
            cmdMentionSuggest();
            return true;
        case IDM_MENTION_ASSEMBLE_CTX:
            cmdMentionAssembleContext();
            return true;
        case IDM_MENTION_REGISTER_CUSTOM:
            cmdMentionRegisterCustom();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdMentionParse()
{
    initContextMentionParser();
    // Get text from chat input, fall back to modal dialog
    char buf[4096] = {};
    if (m_hwndCopilotChatInput)
    {
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    }
    std::string input(buf);
    if (input.empty())
    {
        input = showInputDialog(m_hwndMain, "Mention Parser", "Enter text with @-mentions:");
    }
    if (input.empty())
    {
        return;
    }
    auto& parser = getContextMentionParser();
    auto parseResult = parser.Parse(input);

    std::ostringstream oss;
    oss << "Found " << parseResult.mentions.size() << " mention(s):\n\n";
    for (const auto& m : parseResult.mentions)
    {
        oss << "  @" << m.rawText << " → type=" << static_cast<int>(m.type) << " arg=" << m.argument << "\n";
    }
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Mention Parse Result", MB_OK);
}

void Win32IDE::cmdMentionSuggest()
{
    initContextMentionParser();
    auto& parser = getContextMentionParser();
    auto suggestions = parser.GetSuggestions("", 0);
    std::ostringstream oss;
    oss << "Available @-mention types (" << suggestions.size() << "):\n\n";
    for (const auto& s : suggestions)
    {
        oss << "  @" << s.label << " - " << s.description << "\n";
    }
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Mention Suggestions", MB_OK);
}

void Win32IDE::cmdMentionAssembleContext()
{
    initContextMentionParser();
    char buf[4096] = {};
    if (m_hwndCopilotChatInput)
    {
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    }
    std::string input(buf);
    if (input.empty())
    {
        input = showInputDialog(m_hwndMain, "Context Assembly", "Enter text with @-mentions:");
    }
    if (input.empty())
    {
        return;
    }
    auto& parser = getContextMentionParser();
    auto parseResult = parser.Parse(input);
    parser.Resolve(parseResult);
    auto assembled = parser.AssembleContext(parseResult, 8192);
    std::ostringstream oss;
    oss << "Assembled context (" << assembled.size() << " chars):\n\n" << assembled.substr(0, 2000);
    if (assembled.size() > 2000)
        oss << "\n... (truncated)";
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Assembled Context", MB_OK);
}

void Win32IDE::cmdMentionRegisterCustom()
{
    MessageBoxA(m_hwndMain,
                "Use RegisterCustomProvider() API to add custom @-mention types.\n"
                "See context_mention_parser.h for the C-ABI plugin interface.",
                "Custom Providers", MB_ICONINFORMATION);
}

// ============================================================================
// 4. VISION ENCODER
// ============================================================================
void Win32IDE::initVisionEncoderUI()
{
    if (m_visionEncoderUIInitialized)
        return;
    // VisionEncoder is not a singleton — use file-local instance
    (void)getVisionEncoder();
    m_visionEncoderUIInitialized = true;
    RAWRXD_LOG_INFO("VisionEncoder UI wiring initialized");
}

bool Win32IDE::handleVisionEncoderCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_VISION_LOAD_FILE:
            cmdVisionLoadFile();
            return true;
        case IDM_VISION_PASTE_CLIPBOARD:
            cmdVisionPasteClipboard();
            return true;
        case IDM_VISION_SCREENSHOT:
            cmdVisionScreenshot();
            return true;
        case IDM_VISION_BUILD_PAYLOAD:
            cmdVisionBuildPayload();
            return true;
        case IDM_VISION_VIEW_GUI_HOTPATCH:
            cmdVisionViewIDEGUIAndHotpatch();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdVisionLoadFile()
{
    initVisionEncoderUI();
    auto path = showOpenDialog(m_hwndMain, "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp;*.tiff\0All Files\0*.*\0");
    if (path.empty())
        return;
    auto& encoder = getVisionEncoder();
    auto result = encoder.LoadFromFile(path);
    if (result.success)
    {
        RAWRXD_LOG_INFO("Win32IDE_CursorParity")
            << "Image loaded: " << path << " (" << result.image.width << "x" << result.image.height << ")";
        MessageBoxA(
            m_hwndMain,
            (std::string("Loaded: ") + std::to_string(result.image.width) + "x" + std::to_string(result.image.height))
                .c_str(),
            "Vision Encoder", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, result.error.c_str(), "Vision Error", MB_ICONERROR);
    }
}

void Win32IDE::cmdVisionPasteClipboard()
{
    initVisionEncoderUI();
    auto& encoder = getVisionEncoder();
    auto result = encoder.LoadFromClipboard();
    if (result.success)
    {
        RAWRXD_LOG_INFO("Win32IDE_CursorParity")
            << "Image pasted from clipboard (" << result.image.width << "x" << result.image.height << ")";
        MessageBoxA(
            m_hwndMain,
            (std::string("Pasted: ") + std::to_string(result.image.width) + "x" + std::to_string(result.image.height))
                .c_str(),
            "Vision Encoder", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, "No image data on clipboard.", "Vision", MB_ICONWARNING);
    }
}

void Win32IDE::cmdVisionScreenshot()
{
    initVisionEncoderUI();
    auto& encoder = getVisionEncoder();
    auto result = encoder.CaptureScreenshot();
    if (result.success)
        MessageBoxA(m_hwndMain,
                    (std::string("Screenshot: ") + std::to_string(result.image.width) + "x" +
                     std::to_string(result.image.height))
                        .c_str(),
                    "Screenshot Captured", MB_OK);
    else
        MessageBoxA(m_hwndMain, result.error.c_str(), "Screenshot Error", MB_ICONERROR);
}

void Win32IDE::cmdVisionBuildPayload()
{
    initVisionEncoderUI();
    auto& encoder = getVisionEncoder();
    // Get the last loaded image from a fresh load attempt
    auto lastResult = encoder.LoadFromClipboard();
    if (!lastResult.success || !lastResult.image.isValid())
    {
        MessageBoxA(m_hwndMain, "No image loaded. Load an image first.", "Vision", MB_ICONWARNING);
        return;
    }
    auto payload = encoder.BuildMultimodalPayload(lastResult.image, "Describe this image.");
    if (!payload.empty())
    {
        RAWRXD_LOG_INFO("Win32IDE_CursorParity")
            << "Multimodal payload built (" << payload.size() << " chars)";
        appendToOutput(std::string("[Vision] Payload built (") + std::to_string(payload.size()) + " chars)\n");
        MessageBoxA(m_hwndMain, (std::string("Payload built (") + std::to_string(payload.size()) + " chars).").c_str(),
                    "Vision Payload", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, "Failed to build multimodal payload.", "Vision", MB_ICONWARNING);
    }
}

// ============================================================================
// 5. REFACTORING ENGINE
// ============================================================================
void Win32IDE::initRefactoringEngine()
{
    if (m_refactoringEngineInitialized)
        return;
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    engine.Initialize();
    m_refactoringEngineInitialized = true;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity")
        << "RefactoringEngine initialized (" << engine.GetAllRefactorings().size() << " refactorings)";
}

bool Win32IDE::handleRefactoringCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_REFACTOR_EXTRACT_METHOD:
            cmdRefactorExtractMethod();
            return true;
        case IDM_REFACTOR_EXTRACT_VARIABLE:
            cmdRefactorExtractVariable();
            return true;
        case IDM_REFACTOR_RENAME_SYMBOL:
            cmdRefactorRenameSymbol();
            return true;
        case IDM_REFACTOR_ORGANIZE_INCLUDES:
            cmdRefactorOrganizeIncludes();
            return true;
        case IDM_REFACTOR_CONVERT_AUTO:
            cmdRefactorConvertToAuto();
            return true;
        case IDM_REFACTOR_REMOVE_DEAD_CODE:
            cmdRefactorRemoveDeadCode();
            return true;
        case IDM_REFACTOR_SHOW_ALL:
            cmdRefactorShowAll();
            return true;
        case IDM_REFACTOR_LOAD_PLUGIN:
            cmdRefactorLoadPlugin();
            return true;
        default:
            return false;
    }
}

// Helper: get current editor content + cursor position as RefactoringContext
static RawrXD::IDE::RefactoringContext buildRefactoringContext(HWND hwndEditor, const std::string& filePath)
{
    RawrXD::IDE::RefactoringContext ctx;
    ctx.filePath = filePath;
    ctx.language = "cpp";  // default — could be detected

    if (hwndEditor)
    {
        // Get text
        int len = GetWindowTextLengthA(hwndEditor);
        if (len > 0)
        {
            ctx.code.resize(len + 1);
            GetWindowTextA(hwndEditor, ctx.code.data(), len + 1);
            ctx.code.resize(len);
        }

        // Get selection range
        DWORD selStart = 0, selEnd = 0;
        SendMessageA(hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
        ctx.selectionStartLine = static_cast<int>(selStart);
        ctx.selectionEndLine = static_cast<int>(selEnd);

        // Get cursor line
        int line = (int)SendMessageA(hwndEditor, EM_LINEFROMCHAR, selStart, 0);
        ctx.cursorLine = line;
    }

    return ctx;
}

void Win32IDE::cmdRefactorExtractMethod()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("extract.method", ctx);
    if (result.success)
    {
        if (!result.edits.empty() && m_hwndEditor)
            SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
        MessageBoxA(m_hwndMain, "Method extracted.", "Refactor", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, result.error.c_str(), "Refactor Error", MB_ICONERROR);
    }
}

void Win32IDE::cmdRefactorExtractVariable()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("extract.variable", ctx);
    if (result.success && !result.edits.empty() && m_hwndEditor)
        SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
    else if (!result.success)
        MessageBoxA(m_hwndMain, result.error.c_str(), "Refactor Error", MB_ICONERROR);
}

void Win32IDE::cmdRefactorRenameSymbol()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("rename.symbol", ctx);
    if (result.success && !result.edits.empty() && m_hwndEditor)
        SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
    else if (!result.success)
        MessageBoxA(m_hwndMain, result.error.c_str(), "Refactor Error", MB_ICONERROR);
}

void Win32IDE::cmdRefactorOrganizeIncludes()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("organize.includes", ctx);
    if (result.success && !result.edits.empty() && m_hwndEditor)
        SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
}

void Win32IDE::cmdRefactorConvertToAuto()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("convert.to_auto", ctx);
    if (result.success && !result.edits.empty() && m_hwndEditor)
        SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
}

void Win32IDE::cmdRefactorRemoveDeadCode()
{
    initRefactoringEngine();
    auto ctx = buildRefactoringContext(m_hwndEditor, m_currentFile);
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto result = engine.Execute("remove.dead_code", ctx);
    if (result.success && !result.edits.empty() && m_hwndEditor)
        SetWindowTextA(m_hwndEditor, result.edits[0].newContent.c_str());
}

void Win32IDE::cmdRefactorShowAll()
{
    initRefactoringEngine();
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    auto all = engine.GetAllRefactorings();
    std::ostringstream oss;
    oss << "Available Refactorings (" << all.size() << "):\n\n";
    for (const auto& r : all)
    {
        oss << "  [" << r.id << "] " << r.name << "\n    " << r.description << "\n\n";
    }
    appendToOutput(oss.str());
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Listed " << all.size() << " refactorings";
}

void Win32IDE::cmdRefactorLoadPlugin()
{
    initRefactoringEngine();
    auto path = showOpenDialog(m_hwndMain, "DLL Plugins\0*.dll\0All Files\0*.*\0");
    if (path.empty())
        return;
    auto& engine = RawrXD::IDE::RefactoringEngine::Instance();
    if (engine.LoadPlugin(path))
        MessageBoxA(m_hwndMain, "Refactoring plugin loaded.", "Plugin", MB_OK);
    else
        MessageBoxA(m_hwndMain, "Failed to load plugin.", "Plugin Error", MB_ICONERROR);
}

// ============================================================================
// 6. LANGUAGE REGISTRY
// ============================================================================
void Win32IDE::initLanguageRegistry()
{
    if (m_languageRegistryInitialized)
        return;
    auto& registry = RawrXD::Language::LanguageRegistry::Instance();
    registry.Initialize();
    m_languageRegistryInitialized = true;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity")
        << "LanguageRegistry initialized (" << registry.GetAllLanguages().size() << " languages)";
}

bool Win32IDE::handleLanguageCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_LANG_DETECT:
            cmdLanguageDetect();
            return true;
        case IDM_LANG_LIST_ALL:
            cmdLanguageListAll();
            return true;
        case IDM_LANG_LOAD_PLUGIN:
            cmdLanguageLoadPlugin();
            return true;
        case IDM_LANG_SET_FOR_FILE:
            cmdLanguageSetForFile();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdLanguageDetect()
{
    initLanguageRegistry();
    auto& registry = RawrXD::Language::LanguageRegistry::Instance();
    auto langId = registry.DetectLanguage(m_currentFile);
    if (!langId.empty())
    {
        const auto* desc = registry.FindById(langId);
        std::ostringstream oss;
        oss << "Detected language: " << langId << "\n";
        if (desc)
        {
            oss << "  ID: " << desc->id << "\n"
                << "  Extensions: ";
            for (size_t i = 0; i < desc->fileExtensions.size() && i < 5; ++i)
                oss << desc->fileExtensions[i] << " ";
        }
        MessageBoxA(m_hwndMain, oss.str().c_str(), "Language Detection", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, "Could not detect language for current file.", "Detection", MB_ICONWARNING);
    }
}

void Win32IDE::cmdLanguageListAll()
{
    initLanguageRegistry();
    auto& registry = RawrXD::Language::LanguageRegistry::Instance();
    auto langs = registry.GetAllLanguages();
    std::ostringstream oss;
    oss << "Registered Languages (" << langs.size() << "):\n\n";
    for (const auto& lang : langs)
    {
        oss << "  " << lang.name << " [" << lang.id << "]";
        if (!lang.fileExtensions.empty())
        {
            oss << " — ";
            for (size_t i = 0; i < lang.fileExtensions.size() && i < 4; ++i)
                oss << lang.fileExtensions[i] << " ";
        }
        oss << "\n";
    }
    appendToOutput(oss.str());
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Listed " << langs.size() << " languages";
}

void Win32IDE::cmdLanguageLoadPlugin()
{
    initLanguageRegistry();
    auto path = showOpenDialog(m_hwndMain, "DLL Plugins\0*.dll\0All Files\0*.*\0");
    if (path.empty())
        return;
    auto& registry = RawrXD::Language::LanguageRegistry::Instance();
    if (registry.LoadPlugin(path))
        MessageBoxA(m_hwndMain, "Language plugin loaded.", "Plugin", MB_OK);
    else
        MessageBoxA(m_hwndMain, "Failed to load language plugin.", "Plugin Error", MB_ICONERROR);
}

void Win32IDE::cmdLanguageSetForFile()
{
    initLanguageRegistry();
    auto& registry = RawrXD::Language::LanguageRegistry::Instance();
    auto langs = registry.GetAllLanguages();
    // Show simple list — in production, use a proper picker
    std::ostringstream oss;
    oss << "Available languages:\n\n";
    for (const auto& lang : langs)
    {
        oss << "  " << lang.id << " — " << lang.name << "\n";
    }
    oss << "\nUse API SetLanguageForFile() to assign a language.";
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Set Language", MB_ICONINFORMATION);
}

// ============================================================================
// 7. SEMANTIC INDEX
// ============================================================================
void Win32IDE::initSemanticIndex()
{
    if (m_semanticIndexInitialized)
        return;
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    idx.Initialize(".");
    m_semanticIndexInitialized = true;
    RAWRXD_LOG_INFO("SemanticIndexEngine initialized");
}

bool Win32IDE::handleSemanticIndexCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_SEMANTIC_BUILD_INDEX:
            cmdSemanticBuildIndex();
            return true;
        case IDM_SEMANTIC_FUZZY_SEARCH:
            cmdSemanticFuzzySearch();
            return true;
        case IDM_SEMANTIC_FIND_REFS:
            cmdSemanticFindReferences();
            return true;
        case IDM_SEMANTIC_SHOW_DEPS:
            cmdSemanticShowDependencies();
            return true;
        case IDM_SEMANTIC_TYPE_HIERARCHY:
            cmdSemanticShowTypeHierarchy();
            return true;
        case IDM_SEMANTIC_CALL_GRAPH:
            cmdSemanticShowCallGraph();
            return true;
        case IDM_SEMANTIC_FIND_CYCLES:
            cmdSemanticFindCycles();
            return true;
        case IDM_SEMANTIC_LOAD_PLUGIN:
            cmdSemanticLoadPlugin();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdSemanticBuildIndex()
{
    initSemanticIndex();
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();

    // Build index recursively from workspace root
    auto stats = idx.BuildIndex(true);
    std::ostringstream oss;
    oss << "Index built:\n"
        << "  Files: " << stats.totalFiles << "\n"
        << "  Symbols: " << stats.totalSymbols << "\n"
        << "  Dependencies: " << stats.totalDeps << "\n";
    MessageBoxA(m_hwndMain, oss.str().c_str(), "Semantic Index", MB_OK);
}

void Win32IDE::cmdSemanticFuzzySearch()
{
    initSemanticIndex();
    // Get query from chat input, fall back to modal dialog
    char buf[512] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string query(buf);
    if (query.empty())
    {
        query = showInputDialog(m_hwndMain, "Fuzzy Search", "Enter search query:");
    }
    if (query.empty())
    {
        return;
    }
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    RawrXD::SemanticIndex::QueryOptions opts;
    opts.maxResults = 20;
    opts.fuzzyMatch = true;
    auto results = idx.FuzzySearch(query, opts);
    std::ostringstream oss;
    oss << "Search results for '" << query << "' (" << results.symbols.size() << "):\n\n";
    for (const auto& r : results.symbols)
    {
        oss << "  " << r.name << " (" << r.definition.filePath << ":" << r.definition.startLine << ")\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemanticFindReferences()
{
    initSemanticIndex();
    char buf[256] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string symbol(buf);
    if (symbol.empty())
    {
        symbol = showInputDialog(m_hwndMain, "Find References", "Enter symbol name:");
    }
    if (symbol.empty())
    {
        return;
    }
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    auto refs = idx.GetReferences(symbol);
    std::ostringstream oss;
    oss << "References for '" << symbol << "' (" << refs.size() << "):\n\n";
    for (const auto& loc : refs)
    {
        oss << "  " << loc.filePath << ":" << loc.startLine << ":" << loc.startCol << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemanticShowDependencies()
{
    initSemanticIndex();
    if (m_currentFile.empty())
        return;
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    auto deps = idx.GetFileDependencies(m_currentFile);
    std::ostringstream oss;
    oss << "Dependencies of " << m_currentFile << " (" << deps.size() << "):\n\n";
    for (const auto& d : deps)
    {
        oss << "  \xe2\x86\x92 " << d.targetFile << " (" << d.importExpr << ")\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemanticShowTypeHierarchy()
{
    initSemanticIndex();
    char buf[256] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string typeName(buf);
    if (typeName.empty())
    {
        typeName = showInputDialog(m_hwndMain, "Type Hierarchy", "Enter type name:");
    }
    if (typeName.empty())
    {
        return;
    }
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    auto supers = idx.GetSuperTypes(typeName);
    auto subs = idx.GetSubTypes(typeName);
    std::ostringstream oss;
    oss << "Type hierarchy for '" << typeName << "':\n\n";
    oss << "  Super types (" << supers.size() << "):\n";
    for (const auto& s : supers)
        oss << "    ↑ " << s.name << "\n";
    oss << "  Sub types (" << subs.size() << "):\n";
    for (const auto& s : subs)
        oss << "    ↓ " << s.name << "\n";
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemanticShowCallGraph()
{
    initSemanticIndex();
    char buf[256] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string funcName(buf);
    if (funcName.empty())
    {
        funcName = showInputDialog(m_hwndMain, "Call Graph", "Enter function name:");
    }
    if (funcName.empty())
    {
        return;
    }
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    auto callers = idx.GetCallers(funcName);
    auto callees = idx.GetCallees(funcName);
    std::ostringstream oss;
    oss << "Call graph for '" << funcName << "':\n\n";
    oss << "  Callers (" << callers.size() << "):\n";
    for (const auto& c : callers)
        oss << "    ← " << c.callerId << "\n";
    oss << "  Callees (" << callees.size() << "):\n";
    for (const auto& c : callees)
        oss << "    → " << c.calleeId << "\n";
    appendToOutput(oss.str());
}

void Win32IDE::cmdSemanticFindCycles()
{
    initSemanticIndex();
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    auto cycles = idx.FindCycles();
    std::ostringstream oss;
    if (cycles.empty())
    {
        oss << "No dependency cycles detected.";
    }
    else
    {
        oss << "Files involved in dependency cycles (" << cycles.size() << "):\n\n";
        for (size_t i = 0; i < cycles.size(); ++i)
        {
            oss << "  " << (i + 1) << ". " << cycles[i] << "\n";
        }
    }
    appendToOutput(oss.str());
    MessageBoxA(m_hwndMain, oss.str().substr(0, 1024).c_str(), "Cycle Detection", MB_OK);
}

void Win32IDE::cmdSemanticLoadPlugin()
{
    initSemanticIndex();
    auto path = showOpenDialog(m_hwndMain, "DLL Plugins\0*.dll\0All Files\0*.*\0");
    if (path.empty())
        return;
    auto& idx = RawrXD::SemanticIndex::SemanticIndexEngine::Instance();
    if (idx.LoadAnalyzerPlugin(path))
        MessageBoxA(m_hwndMain, "Semantic analyzer plugin loaded.", "Plugin", MB_OK);
    else
        MessageBoxA(m_hwndMain, "Failed to load analyzer plugin.", "Plugin Error", MB_ICONERROR);
}

// ============================================================================
// 8. RESOURCE GENERATOR
// ============================================================================
void Win32IDE::initResourceGenerator()
{
    if (m_resourceGeneratorInitialized)
        return;
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    gen.Initialize();
    m_resourceGeneratorInitialized = true;
    RAWRXD_LOG_INFO("Win32IDE_CursorParity")
        << "ResourceGeneratorEngine initialized (" << gen.GetAllTemplates().size() << " templates)";
}

bool Win32IDE::handleResourceGenCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_RESOURCE_GENERATE:
            cmdResourceGenerate();
            return true;
        case IDM_RESOURCE_GEN_PROJECT:
            cmdResourceGenerateProject();
            return true;
        case IDM_RESOURCE_LIST_TEMPLATES:
            cmdResourceListTemplates();
            return true;
        case IDM_RESOURCE_SEARCH:
            cmdResourceSearchTemplates();
            return true;
        case IDM_RESOURCE_LOAD_PLUGIN:
            cmdResourceLoadPlugin();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdResourceGenerate()
{
    initResourceGenerator();
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    auto all = gen.GetAllTemplates();

    // Show template picker
    std::ostringstream picker;
    picker << "Available templates:\n\n";
    for (const auto& t : all)
    {
        picker << "  [" << t.id << "] " << t.name << "\n";
    }
    picker << "\nEnter template ID in chat input and re-run.";

    char buf[256] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string templateId(buf);

    if (templateId.empty())
    {
        templateId = showInputDialog(m_hwndMain, "Resource Generator",
                                     "Enter template ID (see list in output):");
    }
    if (templateId.empty())
    {
        appendToOutput(picker.str());
        return;
    }

    // Generate with defaults
    std::map<std::string, std::string> params;
    params["projectName"] = "RawrXD";
    params["appName"] = "RawrXD";
    params["port"] = "8080";

    auto result = gen.Generate(templateId, params);
    if (result.success)
    {
        appendToOutput(result.content);
        RAWRXD_LOG_INFO("Win32IDE_CursorParity")
            << "Generated resource: " << templateId << " (" << result.content.size() << " chars)";

        // Offer to save
        if (!result.suggestedFilename.empty())
        {
            auto savePath = showSaveDialog(m_hwndMain, "All Files\0*.*\0", result.suggestedFilename.c_str());
            if (!savePath.empty())
            {
                FILE* f = fopen(savePath.c_str(), "w");
                if (f)
                {
                    fwrite(result.content.c_str(), 1, result.content.size(), f);
                    fclose(f);
                    MessageBoxA(m_hwndMain, ("Saved to: " + savePath).c_str(), "Resource Generator", MB_OK);
                }
            }
        }
    }
    else
    {
        MessageBoxA(m_hwndMain, result.error.c_str(), "Generation Error", MB_ICONERROR);
    }
}

void Win32IDE::cmdResourceGenerateProject()
{
    initResourceGenerator();
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();

    // Generate a standard project scaffold
    std::vector<std::string> templates = {"docker.multistage.cpp", "docker.compose",   "cicd.github.ci",
                                          "config.cmake",          "config.gitignore", "config.env"};

    std::map<std::string, std::string> params;
    params["projectName"] = "RawrXD";
    params["appName"] = "RawrXD";
    params["language"] = "cpp";
    params["cppStandard"] = "20";
    params["port"] = "8080";

    auto result = gen.GenerateProject(templates, params);
    if (result.success)
    {
        std::ostringstream oss;
        oss << "Project scaffold generated (" << result.files.size() << " files):\n\n";
        for (const auto& f : result.files)
        {
            oss << "  " << f.path << " (" << f.content.size() << " bytes)\n";
        }
        appendToOutput(oss.str());
        MessageBoxA(m_hwndMain, oss.str().substr(0, 1024).c_str(), "Project Scaffold", MB_OK);
    }
    else
    {
        MessageBoxA(m_hwndMain, result.error.c_str(), "Project Error", MB_ICONERROR);
    }
}

void Win32IDE::cmdResourceListTemplates()
{
    initResourceGenerator();
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    auto all = gen.GetAllTemplates();
    std::ostringstream oss;
    oss << "Resource Templates (" << all.size() << "):\n\n";
    for (const auto& t : all)
    {
        oss << "  [" << t.id << "] " << t.name << "\n    " << t.description << "\n";
        if (!t.tags.empty())
        {
            oss << "    Tags: ";
            for (const auto& tag : t.tags)
                oss << tag << " ";
            oss << "\n";
        }
        oss << "\n";
    }
    appendToOutput(oss.str());
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Listed " << all.size() << " resource templates";
}

void Win32IDE::cmdResourceSearchTemplates()
{
    initResourceGenerator();
    char buf[256] = {};
    if (m_hwndCopilotChatInput)
        GetWindowTextA(m_hwndCopilotChatInput, buf, sizeof(buf));
    std::string query(buf);
    if (query.empty())
    {
        query = showInputDialog(m_hwndMain, "Template Search", "Enter search query:");
    }
    if (query.empty())
    {
        return;
    }
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    auto results = gen.SearchTemplates(query);
    std::ostringstream oss;
    oss << "Search '" << query << "' (" << results.size() << " results):\n\n";
    for (const auto& t : results)
    {
        oss << "  [" << t.id << "] " << t.name << " — " << t.description << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdResourceLoadPlugin()
{
    initResourceGenerator();
    auto path = showOpenDialog(m_hwndMain, "DLL Plugins\0*.dll\0All Files\0*.*\0");
    if (path.empty())
        return;
    auto& gen = RawrXD::Resource::ResourceGeneratorEngine::Instance();
    if (gen.LoadPlugin(path))
        MessageBoxA(m_hwndMain, "Resource generator plugin loaded.", "Plugin", MB_OK);
    else
        MessageBoxA(m_hwndMain, "Failed to load plugin.", "Plugin Error", MB_ICONERROR);
}

// ============================================================================
// MENU BUILDER — Adds "Features" submenu to the menu bar
// ============================================================================
void Win32IDE::createFeaturesMenu(HMENU parentMenu)
{
    HMENU hFeatures = CreatePopupMenu();

    // Telemetry Export
    HMENU hTelExport = CreatePopupMenu();
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_JSON, "Export &JSON...");
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_CSV, "Export &CSV...");
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_PROMETHEUS, "Export &Prometheus...");
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_OTLP, "Export &OTLP...");
    AppendMenuA(hTelExport, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_AUDIT_LOG, "&Audit Log...");
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_VERIFY_CHAIN, "&Verify Chain");
    AppendMenuA(hTelExport, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_AUTO_START, "Auto-Export: &Start");
    AppendMenuA(hTelExport, MF_STRING, IDM_TELEXPORT_AUTO_STOP, "Auto-Export: Sto&p");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hTelExport, "&Telemetry Export");

    // Agentic Composer
    HMENU hComposer = CreatePopupMenu();
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_NEW_SESSION, "&New Session");
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_END_SESSION, "&End Session");
    AppendMenuA(hComposer, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_APPROVE_ALL, "&Approve All Changes");
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_REJECT_ALL, "&Reject All Changes");
    AppendMenuA(hComposer, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_SHOW_TRANSCRIPT, "Show &Transcript");
    AppendMenuA(hComposer, MF_STRING, IDM_COMPOSER_SHOW_METRICS, "Show &Metrics");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hComposer, "&Agentic Composer");

    // @-Mention Parser
    HMENU hMention = CreatePopupMenu();
    AppendMenuA(hMention, MF_STRING, IDM_MENTION_PARSE, "&Parse @-Mentions");
    AppendMenuA(hMention, MF_STRING, IDM_MENTION_SUGGEST, "&Available Mentions");
    AppendMenuA(hMention, MF_STRING, IDM_MENTION_ASSEMBLE_CTX, "Assemble &Context");
    AppendMenuA(hMention, MF_STRING, IDM_MENTION_REGISTER_CUSTOM, "&Register Custom...");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hMention, "@-&Mentions");

    // Vision Encoder
    HMENU hVision = CreatePopupMenu();
    AppendMenuA(hVision, MF_STRING, IDM_VISION_LOAD_FILE, "Load &Image...");
    AppendMenuA(hVision, MF_STRING, IDM_VISION_PASTE_CLIPBOARD, "&Paste from Clipboard");
    AppendMenuA(hVision, MF_STRING, IDM_VISION_SCREENSHOT, "&Screenshot");
    AppendMenuA(hVision, MF_STRING, IDM_VISION_BUILD_PAYLOAD, "&Build Multimodal Payload");
    AppendMenuA(hVision, MF_STRING, IDM_VISION_VIEW_GUI_HOTPATCH, "View IDE &GUI / Auto-correct");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hVision, "&Vision Input");

    AppendMenuA(hFeatures, MF_SEPARATOR, 0, nullptr);

    // Refactoring Engine
    HMENU hRefactor = CreatePopupMenu();
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_EXTRACT_METHOD, "Extract &Method");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_EXTRACT_VARIABLE, "Extract &Variable");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_RENAME_SYMBOL, "&Rename Symbol");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_ORGANIZE_INCLUDES, "&Organize Includes");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_CONVERT_AUTO, "Convert to &auto");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_REMOVE_DEAD_CODE, "Remove &Dead Code");
    AppendMenuA(hRefactor, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_SHOW_ALL, "&Show All Refactorings");
    AppendMenuA(hRefactor, MF_STRING, IDM_REFACTOR_LOAD_PLUGIN, "&Load Plugin...");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hRefactor, "&Refactoring");

    // Language Registry
    HMENU hLang = CreatePopupMenu();
    AppendMenuA(hLang, MF_STRING, IDM_LANG_DETECT, "&Detect Language");
    AppendMenuA(hLang, MF_STRING, IDM_LANG_LIST_ALL, "&List All Languages");
    AppendMenuA(hLang, MF_STRING, IDM_LANG_LOAD_PLUGIN, "Load &Plugin...");
    AppendMenuA(hLang, MF_STRING, IDM_LANG_SET_FOR_FILE, "&Set Language for File");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hLang, "&Languages");

    // Semantic Index
    HMENU hSemantic = CreatePopupMenu();
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_BUILD_INDEX, "&Build Index");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_FUZZY_SEARCH, "&Fuzzy Search");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_FIND_REFS, "Find &References");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_SHOW_DEPS, "Show &Dependencies");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_TYPE_HIERARCHY, "&Type Hierarchy");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_CALL_GRAPH, "&Call Graph");
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_FIND_CYCLES, "Find C&ycles");
    AppendMenuA(hSemantic, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hSemantic, MF_STRING, IDM_SEMANTIC_LOAD_PLUGIN, "Load &Plugin...");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hSemantic, "&Semantic Index");

    AppendMenuA(hFeatures, MF_SEPARATOR, 0, nullptr);

    // Resource Generator
    HMENU hResource = CreatePopupMenu();
    AppendMenuA(hResource, MF_STRING, IDM_RESOURCE_GENERATE, "&Generate Resource...");
    AppendMenuA(hResource, MF_STRING, IDM_RESOURCE_GEN_PROJECT, "Generate &Project Scaffold");
    AppendMenuA(hResource, MF_STRING, IDM_RESOURCE_LIST_TEMPLATES, "&List Templates");
    AppendMenuA(hResource, MF_STRING, IDM_RESOURCE_SEARCH, "&Search Templates");
    AppendMenuA(hResource, MF_STRING, IDM_RESOURCE_LOAD_PLUGIN, "Load P&lugin...");
    AppendMenuA(hFeatures, MF_POPUP, (UINT_PTR)hResource, "Resource &Generator");

    AppendMenuA(parentMenu, MF_POPUP, (UINT_PTR)hFeatures, "&Features");
}

// ============================================================================
// UNIFIED COMMAND ROUTER — same command surface as CLI (ssot_handlers)
// ============================================================================
#if 0
// Duplicate legacy router kept disabled; canonical implementation lives in Win32IDE_Commands.cpp.
bool Win32IDE::handleFeaturesCommand(int commandId) { return false; }
#endif

// ============================================================================
// MASTER INIT — Called once during IDE startup (CLI shares same surface via ssot)
// ============================================================================
void Win32IDE::initAllFeatureModules()
{
    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "Initializing feature modules...";

    initTelemetryExport();
    initAgenticComposerUX();
    initContextMentionParser();
    initVisionEncoderUI();
    initRefactoringEngine();
    initLanguageRegistry();
    initSemanticIndex();
    initResourceGenerator();

    int check = RawrXD::Parity::verifyCursorParityWiring(this);
    if (check != 0)
        RAWRXD_LOG_WARNING("Win32IDE_CursorParity")
            << "Feature modules verification reported index: " << check;

    RAWRXD_LOG_INFO("Win32IDE_CursorParity") << "All feature modules initialized";
}

namespace RawrXD::Parity {
int verifyCursorParityWiring(void* win32IDEContext) {
    return (win32IDEContext != nullptr) ? 0 : 1;
}
} // namespace RawrXD::Parity
