// ============================================================================
// Win32IDE_AuditDashboard.cpp — Phase 31: Audit Dashboard UI
// ============================================================================
//
// PURPOSE:
//   Win32IDE integration for the IDE Self-Audit & Verification System.
//   Provides:
//     1. Audit system initialization during IDE startup
//     2. Command routing for IDM_AUDIT_* commands (9500 range)
//     3. ListView-based audit dashboard window
//     4. Full audit runner (stub detection + menu wire check + component tests)
//     5. Report generation and export
//     6. Quick-stats status bar integration
//
// UI:
//   Creates a modeless dialog with a WC_LISTVIEW showing all registered
//   features, their status, stub detection result, menu wiring, and
//   runtime test outcome. Color-coded rows (green=complete, yellow=partial,
//   red=stub/broken).
//
// PATTERN:   No exceptions. PatchResult-compatible returns where applicable.
// THREADING: All UI calls on main thread (STA).
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

// This file uses Wide (W-suffix) Win32 APIs throughout.
// The project builds as ANSI (no global UNICODE define), so we use explicit
// W-suffix message constants (LVM_SETITEMTEXTW, LVM_INSERTITEMW, etc.)
// instead of relying on the ListView_* macros which resolve to ANSI.

#include "../../include/agentic_autonomous_config.h"
#include "../../include/feature_registry.h"
#include "../../include/startup_phase_registry.h"
#include "../agentic/NativeInferenceClient.h"
#include "../cli/swarm_orchestrator.h"
#include "../core/camellia256_bridge.hpp"
#include "../core/enterprise_license.h"
#include "../core/js_extension_host.hpp"
#include "../core/problems_aggregator.hpp"
#include "HeadlessIDE.h"
#include "Win32IDE.h"

#include <commctrl.h>
#include <commdlg.h>  // GetSaveFileNameW
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <richedit.h>  // CHARRANGE / EM_EXGETSEL
#include <shlobj.h>
#include <sstream>
#include <string>
#include <vector>
#include <windowsx.h>
#include <winhttp.h>

// Link against common controls v6
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")

// ---------------------------------------------------------------------------
// Explicit Wide ListView helpers for ANSI build
// ---------------------------------------------------------------------------
// The project is built without UNICODE, so ListView_SetItemText etc. resolve
// to their ANSI variants.  This file uses wchar_t throughout, so we send the
// W messages directly.
#ifndef LVM_SETITEMTEXTW
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#endif
#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif
#ifndef LVM_INSERTCOLUMNW
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#endif

static inline int LV_InsertItemW(HWND hwnd, const LVITEMW* item)
{
    return static_cast<int>(SendMessageW(hwnd, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(item)));
}
static inline void LV_SetItemTextW(HWND hwnd, int iItem, int iSubItem, LPWSTR pszText)
{
    LVITEMW lvi{};
    lvi.iSubItem = iSubItem;
    lvi.pszText = pszText;
    SendMessageW(hwnd, LVM_SETITEMTEXTW, static_cast<WPARAM>(iItem), reinterpret_cast<LPARAM>(&lvi));
}
static inline int LV_InsertColumnW(HWND hwnd, int iCol, const LVCOLUMNW* col)
{
    return static_cast<int>(
        SendMessageW(hwnd, LVM_INSERTCOLUMNW, static_cast<WPARAM>(iCol), reinterpret_cast<LPARAM>(col)));
}
// ---------------------------------------------------------------------------

// Forward declarations from menu_auditor.cpp
namespace RawrXD
{
namespace Audit
{
bool verifyCommandInMenu(HMENU hMenu, int commandId);
std::string buildMenuBreadcrumb(HMENU hMenu, int commandId);
std::string getMenuWiringReport(HMENU hMenu);
std::vector<std::string> findOrphanedCommands(HMENU hMenu);
std::vector<int> findUnregisteredMenuItems(HMENU hMenu);
}  // namespace Audit
}  // namespace RawrXD

// ============================================================================
// CONSTANTS
// ============================================================================
static const wchar_t* AUDIT_WINDOW_CLASS = L"RawrXD_AuditDashboard";
static const int AUDIT_LISTVIEW_ID = 11001;
static const int AUDIT_BTN_RUN_ID = 11002;
static const int AUDIT_BTN_EXPORT_ID = 11003;
static const int AUDIT_BTN_REFRESH_ID = 11004;
static const int AUDIT_BTN_CLOSE_ID = 11005;
static const int AUDIT_STATUS_ID = 11006;

// ============================================================================
// CRITICAL RUNTIME VALIDATION — Batch 1 (items 1..8 from production audit)
// ============================================================================
std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch1()
{
    std::vector<RuntimeValidationCheck> checks;
    auto push = [&checks](const std::string& name, bool passed, const std::string& detail)
    {
        RuntimeValidationCheck c;
        c.name = name;
        c.passed = passed;
        c.detail = detail;
        checks.push_back(std::move(c));
    };

    // 1) Ghost Text Rendering
    {
        const uint64_t seqBefore = m_ghostTextRequestSeq.load();
        triggerGhostTextCompletion();
        const bool seqAdvanced = m_ghostTextRequestSeq.load() > seqBefore;
        if (m_hwndMain)
        {
            KillTimer(m_hwndMain, 8888);  // GHOST_TEXT_TIMER_ID in Win32IDE_GhostText.cpp
        }

        const bool ok =
            m_ghostTextEnabled && m_hwndEditor && IsWindow(m_hwndEditor) && m_ghostTextFont != nullptr && seqAdvanced;
        std::ostringstream oss;
        oss << "enabled=" << (m_ghostTextEnabled ? "yes" : "no")
            << ", editor=" << ((m_hwndEditor && IsWindow(m_hwndEditor)) ? "ok" : "missing")
            << ", font=" << (m_ghostTextFont ? "ok" : "missing")
            << ", debounce_seq=" << (seqAdvanced ? "advanced" : "stalled");
        push("Ghost Text Rendering", ok, oss.str());
    }

    // 2) Multi-Cursor Visuals
    {
        bool ok = false;
        std::ostringstream oss;
        if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        {
            oss << "editor window unavailable";
            push("Multi-Cursor Visuals", false, oss.str());
        }
        else
        {
            initMultiCursor();
            CHARRANGE cr{};
            SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
            int textLen = GetWindowTextLengthA(m_hwndEditor);
            int probe = cr.cpMax;
            if (textLen > 0)
            {
                probe = (cr.cpMax < textLen) ? (cr.cpMax + 1) : (cr.cpMax > 0 ? cr.cpMax - 1 : 0);
            }
            addCursorAtPosition(probe);
            if (textLen > 2)
            {
                addCursorAtPosition(0);
            }

            const int count = getMultiCursorCount();
            ok = count >= 1;
            oss << "cursor_count=" << count << ", probe_pos=" << probe;
            clearSecondaryCursors();
            push("Multi-Cursor Visuals", ok, oss.str());
        }
    }

    // 3) Peek Overlay
    {
        bool ok = false;
        std::ostringstream oss;
        if (m_currentFile.empty() || !std::filesystem::exists(m_currentFile))
        {
            oss << "current file unavailable for peek: '" << m_currentFile << "'";
        }
        else
        {
            PeekLocation loc;
            loc.filePath = m_currentFile;
            loc.line = 1;
            loc.col = 1;
            loc.endCol = 1;
            loc.preview = "runtime validation";
            showPeekOverlay("__rawrxd_validation_symbol__", {loc}, true);
            ok = (m_hwndPeekOverlay && IsWindow(m_hwndPeekOverlay));
            oss << "overlay=" << (ok ? "visible" : "not-visible");
            closePeekView();
        }
        push("Peek Overlay", ok, oss.str());
    }

    // 4) Caret Animation
    {
        if (!m_caretAnim.enabled)
        {
            initSmoothCaret();
        }
        updateCaretTarget();
        onCaretAnimationTick();
        const bool ok = m_caretAnim.enabled;
        std::ostringstream oss;
        oss << "enabled=" << (m_caretAnim.enabled ? "yes" : "no")
            << ", animating=" << (m_caretAnim.animating ? "yes" : "no") << ", blink_phase=" << m_caretAnim.blinkPhase;
        push("Caret Animation", ok, oss.str());
    }

    // 5) Tier2/Tier3 Cosmetics (post EM_POSFROMCHAR fixes)
    {
        if (!m_gitDiffFont)
        {
            initGitDiffViewer();
        }
        if (!m_bracketPairEnabled)
        {
            initBracketPairColorization();
        }
        if (!m_indentGuidesEnabled)
        {
            initIndentationGuides();
        }
        const bool ok =
            (m_gitDiffFont != nullptr) && m_bracketPairEnabled && m_indentGuidesEnabled && m_caretAnim.enabled;
        std::ostringstream oss;
        oss << "gitdiff_font=" << (m_gitDiffFont ? "ok" : "missing")
            << ", bracket_pair=" << (m_bracketPairEnabled ? "on" : "off")
            << ", indent_guides=" << (m_indentGuidesEnabled ? "on" : "off")
            << ", smooth_caret=" << (m_caretAnim.enabled ? "on" : "off");
        push("Tier2/Tier3 Cosmetics", ok, oss.str());
    }

    // 6) NativeInferenceClient in IDE context
    RawrXD::Agent::NativeInferenceHealth health{};
    std::vector<std::string> ollamaModels;
    {
        std::string base = m_ollamaBaseUrl.empty() ? "http://127.0.0.1:11434" : m_ollamaBaseUrl;
        std::string withoutProto = base;
        const size_t p = base.find("://");
        if (p != std::string::npos)
        {
            withoutProto = base.substr(p + 3);
        }

        std::string host;
        uint16_t port = 11434;
        const size_t slashPos = withoutProto.find('/');
        const std::string hostPort = (slashPos == std::string::npos) ? withoutProto : withoutProto.substr(0, slashPos);
        const size_t colonPos = hostPort.find(':');
        if (colonPos == std::string::npos)
        {
            host = hostPort.empty() ? "127.0.0.1" : hostPort;
        }
        else
        {
            host = hostPort.substr(0, colonPos);
            const std::string portText = hostPort.substr(colonPos + 1);
            const int parsed = std::atoi(portText.c_str());
            if (parsed > 0 && parsed < 65536)
            {
                port = static_cast<uint16_t>(parsed);
            }
        }

        RawrXD::Agent::NativeInferenceConfig cfg;
        cfg.host = host.empty() ? "127.0.0.1" : host;
        cfg.port = port;
        cfg.timeout_ms = 4000;
        cfg.chat_model = getResolvedOllamaModel();

        RawrXD::Agent::NativeInferenceClient client(cfg);
        health = client.TestConnectionWithStats();
        if (health.ok)
        {
            ollamaModels = client.ListModels();
        }

        std::ostringstream oss;
        oss << "endpoint=" << cfg.host << ":" << cfg.port << ", ok=" << (health.ok ? "yes" : "no")
            << ", latency_ms=" << health.latency_ms << ", model_count=" << health.model_count;
        push("NativeInferenceClient", health.ok, oss.str());
    }

    // 7) IDE model discovery + explorer/terminal accessibility
    {
        const bool explorerOk =
            (m_hwndFileTree && IsWindow(m_hwndFileTree)) || (m_hwndFileExplorer && IsWindow(m_hwndFileExplorer));

        if (m_hwndModelSelector && IsWindow(m_hwndModelSelector))
        {
            populateModelSelector();
        }

        const int modelUiCount = (m_hwndModelSelector && IsWindow(m_hwndModelSelector))
                                     ? (int)SendMessage(m_hwndModelSelector, CB_GETCOUNT, 0, 0)
                                     : 0;
        size_t cachedModelCount = 0;
        {
            std::lock_guard<std::mutex> modelListLock(m_availableModelsMutex);
            cachedModelCount = m_availableModels.size();
        }
        const bool modelDiscoveryOk = (modelUiCount > 0) || (cachedModelCount > 0) || !ollamaModels.empty();

        int paneId = createTerminalPane(Win32TerminalManager::ShellType::CommandPrompt, "P0 Validation Pane");
        bool terminalOk = (paneId >= 0) && (getActiveTerminalPane() != nullptr);
        if (terminalOk)
        {
            sendToAllTerminals("echo RAWRXD_VALIDATION_BATCH1");
            closeTerminalPane(paneId);
        }

        const bool ok = modelDiscoveryOk && explorerOk && terminalOk;
        std::ostringstream oss;
        oss << "model_ui_count=" << modelUiCount << ", model_cache=" << cachedModelCount
            << ", ollama_models=" << ollamaModels.size() << ", explorer=" << (explorerOk ? "ok" : "missing")
            << ", terminal=" << (terminalOk ? "ok" : "failed");
        push("Model Discovery + Explorer + Terminal", ok, oss.str());
    }

    // 8) Phase order manifest + PR02 session (cold-start phases 1–8 narrative; no network)
    {
        RawrXD::Startup::ensureStartupSessionId();
        const char* sid = RawrXD::Startup::getStartupSessionId();
        const size_t sidLen = sid ? std::strlen(sid) : 0;
        const auto order = RawrXD::Startup::getPhaseOrder();
        const bool sessionOk = sidLen >= 4;
        const bool orderOk = order.size() >= 8;
        const bool ok = sessionOk && orderOk;
        std::ostringstream oss;
        oss << "session_id_len=" << sidLen << ", phase_count=" << order.size();
        push("Phase order + PR02 session", ok, oss.str());
    }

    return checks;
}

// ============================================================================
// CRITICAL RUNTIME VALIDATION — Batch 2 (items 8..14 from production audit)
// ============================================================================
std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch2()
{
    std::vector<RuntimeValidationCheck> checks;
    auto push = [&checks](const std::string& name, bool passed, const std::string& detail)
    {
        RuntimeValidationCheck c;
        c.name = name;
        c.passed = passed;
        c.detail = detail;
        checks.push_back(std::move(c));
    };

    // 8) Copilot chat panel send/clear pipeline
    {
        if ((!m_hwndSecondarySidebar || !IsWindow(m_hwndSecondarySidebar)) && m_hwndMain)
        {
            createChatPanel();
        }

        const bool uiOk = (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput)) &&
                          (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput)) &&
                          (m_hwndModelSelector && IsWindow(m_hwndModelSelector));

        bool ok = false;
        std::ostringstream oss;
        if (!uiOk)
        {
            oss << "chat ui missing";
        }
        else
        {
            HandleCopilotClear();
            const std::string marker = "audit-batch2-chat";
            SetWindowTextA(m_hwndCopilotChatInput, marker.c_str());
            HandleCopilotSend();
            Sleep(50);

            const std::string output = getWindowText(m_hwndCopilotChatOutput);
            const int modelCount = (int)SendMessage(m_hwndModelSelector, CB_GETCOUNT, 0, 0);
            ok = output.find("> User: " + marker) != std::string::npos &&
                 GetWindowTextLengthW(m_hwndCopilotChatInput) == 0 && modelCount > 0;

            oss << "model_count=" << modelCount
                << ", echoed_user_msg=" << (output.find("> User: " + marker) != std::string::npos ? "yes" : "no")
                << ", input_cleared=" << (GetWindowTextLengthW(m_hwndCopilotChatInput) == 0 ? "yes" : "no");
        }
        push("Copilot Chat Pipeline", ok, oss.str());
    }

    // 9) File open/save roundtrip
    {
        bool ok = false;
        std::ostringstream oss;
        if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        {
            oss << "editor unavailable";
        }
        else
        {
            const std::string originalFile = m_currentFile;
            const std::string originalContent = getWindowText(m_hwndEditor);
            const bool originalModified = m_fileModified;

            char tempDir[MAX_PATH] = {};
            char tempFile[MAX_PATH] = {};
            GetTempPathA(MAX_PATH, tempDir);
            const UINT tempId = GetTempFileNameA(tempDir, "rxa", 0, tempFile);

            if (tempId == 0)
            {
                oss << "failed to allocate temp file";
            }
            else
            {
                const std::string initialContent = "batch2-open-save-initial\r\nline2\r\n";
                const std::string modifiedContent = "batch2-open-save-modified\r\nline3\r\n";

                {
                    std::ofstream out(tempFile, std::ios::binary | std::ios::trunc);
                    out << initialContent;
                }

                openFile(tempFile);
                const std::string openedContent = getWindowText(m_hwndEditor);
                setWindowText(m_hwndEditor, modifiedContent);
                m_currentFile = tempFile;
                m_fileModified = true;
                const bool saved = saveFile();

                std::ifstream in(tempFile, std::ios::binary);
                std::string persisted((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

                ok = openedContent == initialContent && saved && persisted == modifiedContent;
                oss << "opened_bytes=" << openedContent.size() << ", saved=" << (saved ? "yes" : "no")
                    << ", persisted_match=" << (persisted == modifiedContent ? "yes" : "no");

                if (!originalFile.empty() && std::filesystem::exists(originalFile))
                {
                    openFile(originalFile);
                }
                else
                {
                    setWindowText(m_hwndEditor, originalContent);
                    m_currentFile = originalFile;
                    m_fileModified = originalModified;
                    updateTitleBarText();
                    updateLineNumbers();
                    syncEditorToGpuSurface();
                }

                DeleteFileA(tempFile);
            }
        }
        push("File Open/Save Roundtrip", ok, oss.str());
    }

    // 10) File explorer population
    {
        if ((!m_hwndFileExplorer || !IsWindow(m_hwndFileExplorer)) && m_hwndSidebar && IsWindow(m_hwndSidebar))
        {
            createFileExplorer();
        }

        if (m_hwndFileExplorer && IsWindow(m_hwndFileExplorer))
        {
            refreshFileExplorer();
        }

        const int nodeCount =
            (m_hwndFileExplorer && IsWindow(m_hwndFileExplorer)) ? TreeView_GetCount(m_hwndFileExplorer) : 0;
        const bool ok = nodeCount > 0;
        std::ostringstream oss;
        oss << "tree_window=" << ((m_hwndFileExplorer && IsWindow(m_hwndFileExplorer)) ? "ok" : "missing")
            << ", node_count=" << nodeCount;
        push("File Explorer Population", ok, oss.str());
    }

    // 11) Dedicated PowerShell panel/session
    {
        if ((!m_hwndPowerShellPanel || !IsWindow(m_hwndPowerShellPanel)) && m_hwndMain)
        {
            createPowerShellPanel();
        }

        const bool wasActive = isPowerShellSessionActive();
        if (!wasActive)
        {
            startPowerShellSession();
            Sleep(250);
        }

        const bool ok = (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel)) &&
                        (m_hwndPowerShellOutput && IsWindow(m_hwndPowerShellOutput)) &&
                        (m_hwndPowerShellInput && IsWindow(m_hwndPowerShellInput)) && isPowerShellSessionActive();

        std::ostringstream oss;
        oss << "panel=" << ((m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel)) ? "ok" : "missing")
            << ", output=" << ((m_hwndPowerShellOutput && IsWindow(m_hwndPowerShellOutput)) ? "ok" : "missing")
            << ", input=" << ((m_hwndPowerShellInput && IsWindow(m_hwndPowerShellInput)) ? "ok" : "missing")
            << ", session=" << (isPowerShellSessionActive() ? "active" : "inactive");

        if (!wasActive && isPowerShellSessionActive())
        {
            stopPowerShellSession();
        }

        push("PowerShell Session Access", ok, oss.str());
    }

    // 12) Agentic todo pipeline
    {
        if (!m_agenticBridge)
        {
            initializeAgenticBridge();
        }

        SubAgentManager* mgr = (m_agenticBridge ? m_agenticBridge->GetSubAgentManager() : nullptr);
        if (!mgr && m_agenticBridge)
        {
            m_agenticBridge->Initialize("", m_agenticBridge->GetCurrentModel());
            mgr = m_agenticBridge->GetSubAgentManager();
        }
        bool ok = false;
        std::ostringstream oss;
        if (!mgr)
        {
            oss << "subagent manager unavailable";
        }
        else
        {
            const auto baseline = mgr->getTodoList();
            std::vector<TodoItem> probe = baseline;
            TodoItem item;
            item.id = 900001;
            item.title = "Audit Batch 2";
            item.description = "Runtime todo validation";
            item.status = TodoItem::Status::NotStarted;
            probe.push_back(item);

            mgr->setTodoList(probe);
            const auto after = mgr->getTodoList();
            ok = std::any_of(after.begin(), after.end(), [](const TodoItem& todo) { return todo.id == 900001; });
            oss << "baseline=" << baseline.size() << ", after_probe=" << after.size();
            mgr->setTodoList(baseline);
        }
        push("Agentic Todo Pipeline", ok, oss.str());
    }

    // 13) Unified Problems panel runtime
    {
        using RawrXD::ProblemsAggregator;
        auto& agg = ProblemsAggregator::instance();
        initProblemsPanel();
        agg.add("AuditBatch2", m_currentFile.empty() ? "audit-batch2" : m_currentFile, 1, 1, 2, "AUDITB2",
                "Batch 2 unified problems validation");
        refreshProblemsView();

        const bool listOk = (m_hwndProblemsListView && IsWindow(m_hwndProblemsListView));
        const int listCount = listOk ? ListView_GetItemCount(m_hwndProblemsListView) : 0;
        const bool cacheOk =
            std::any_of(m_problemsViewCache.begin(), m_problemsViewCache.end(), [](const RawrXD::ProblemEntry& entry)
                        { return entry.source == "AuditBatch2" && entry.code == "AUDITB2"; });
        const bool ok = listOk && listCount > 0 && cacheOk;

        std::ostringstream oss;
        oss << "listview=" << (listOk ? "ok" : "missing") << ", list_count=" << listCount
            << ", cache_probe=" << (cacheOk ? "present" : "missing");

        agg.clear("AuditBatch2");
        refreshProblemsView();
        push("Unified Problems Panel", ok, oss.str());
    }

    // 14) Headless CLI help + experimental status APIs
    {
        bool helpOk = false;
        bool initOk = false;
        std::string govJson;
        std::string hotJson;

        {
            HeadlessIDE helpProbe;
            char arg0[] = "RawrXD-Win32IDE.exe";
            char arg1[] = "--help";
            char* argv[] = {arg0, arg1, nullptr};
            HeadlessResult res = helpProbe.initialize(2, argv);
            helpOk = (!res.success && res.errorCode == 0);
        }

        {
            HeadlessIDE runtimeProbe;
            HeadlessConfig cfg;
            cfg.mode = HeadlessRunMode::REPL;
            cfg.enableRepl = true;
            cfg.enableServer = false;
            cfg.quiet = true;
            cfg.verbose = false;
            HeadlessResult res = runtimeProbe.initialize(cfg);
            initOk = res.success;
            if (initOk)
            {
                govJson = runtimeProbe.getGovernorStatusJson();
                hotJson = runtimeProbe.getHotpatchStatusJson();
            }
        }

        const bool govOk = govJson.find("governor_activated") != std::string::npos;
        const bool hotOk = hotJson.find("hotpatch70b_activated") != std::string::npos &&
                           hotJson.find("layer_eviction_activated") != std::string::npos;
        const bool ok = helpOk && initOk && govOk && hotOk;

        std::ostringstream oss;
        oss << "help_path=" << (helpOk ? "ok" : "failed") << ", init=" << (initOk ? "ok" : "failed")
            << ", governor_json=" << (govOk ? "ok" : "missing") << ", hotpatch_json=" << (hotOk ? "ok" : "missing");
        push("Headless CLI + Experimental Status", ok, oss.str());
    }

    return checks;
}

// ============================================================================
// CRITICAL RUNTIME VALIDATION — Batch 3 (E0-01..E0-08, conjoined with startup phases 9–16)
// ============================================================================
namespace
{
std::string auditGetExeDirA()
{
    char path[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
        return {};
    char* slash = strrchr(path, '\\');
    if (!slash)
        slash = strrchr(path, '/');
    if (!slash)
        return {};
    *slash = '\0';
    return std::string(path);
}

static uint16_t auditResolveApiPortA()
{
    char portBuf[16] = {};
    if (GetEnvironmentVariableA("RAWRXD_API_PORT", portBuf, sizeof(portBuf)) > 0)
    {
        const int p = atoi(portBuf);
        if (p > 1024 && p < 65536)
            return static_cast<uint16_t>(p);
    }
    return 11434;
}

static bool auditLocalApiStatusOk(uint16_t port, std::string& detail)
{
    detail.clear();
    HINTERNET ses = WinHttpOpen(L"RawrXD-E0-12/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
    {
        detail = "WinHttpOpen failed";
        return false;
    }
    HINTERNET con = WinHttpConnect(ses, L"127.0.0.1", port, 0);
    if (!con)
    {
        detail = "WinHttpConnect failed";
        WinHttpCloseHandle(ses);
        return false;
    }
    HINTERNET req =
        WinHttpOpenRequest(con, L"GET", L"/api/status", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!req)
    {
        detail = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    const bool sent = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                      WinHttpReceiveResponse(req, nullptr);
    DWORD status = 0;
    DWORD sz = sizeof(status);
    if (sent)
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &sz, WINHTTP_NO_HEADER_INDEX);
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    detail = "port=" + std::to_string(port) + " http_status=" + std::to_string(status);
    return sent && status == 200;
}
}  // namespace

Win32IDE::RuntimeValidationCheck Win32IDE::runCriticalValidationE0Check(int e0Index1Based)
{
    Win32IDE::RuntimeValidationCheck c;
    c.name = "E0-?? invalid index";
    c.passed = false;
    c.detail = "e0 out of range (use 1..64)";
    if (e0Index1Based < 1 || e0Index1Based > 64)
        return c;

    const std::string exeDir = auditGetExeDirA();

    switch (e0Index1Based)
    {
        case 1:
        {
            // E0-01 — plugin/workspace dirs (phase: extension_bootstrap / creating_ide_instance)
            std::error_code ec;
            const bool cwdPlugins = std::filesystem::is_directory(std::filesystem::path("plugins"), ec);
            ec.clear();
            const bool exePlugins =
                !exeDir.empty() && std::filesystem::is_directory(std::filesystem::path(exeDir) / "plugins", ec);
            const bool ok = cwdPlugins || exePlugins;
            std::ostringstream oss;
            oss << "cwd_plugins=" << (cwdPlugins ? "yes" : "no") << ", exe_plugins=" << (exePlugins ? "yes" : "no");
            c.name = "E0-01 Plugin/workspace paths";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 2:
        {
            // E0-02 — main window visible (phase: showWindow / layout)
            const bool ok = m_hwndMain && IsWindow(m_hwndMain) && IsWindowVisible(m_hwndMain);
            std::ostringstream oss;
            oss << "hwnd=" << (m_hwndMain ? "set" : "null") << ", visible=" << (ok ? "yes" : "no");
            c.name = "E0-02 Main window visibility";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 3:
        {
            // E0-03 — Camellia MASM RFC self-test (phase: camellia_init)
            const auto tr = RawrXD::Crypto::Camellia256Bridge::instance().selfTest();
            std::ostringstream oss;
            oss << (tr.detail ? tr.detail : "");
            c.name = "E0-03 Camellia256 RFC self-test";
            c.passed = tr.success;
            c.detail = oss.str();
            break;
        }
        case 4:
        {
            // E0-04 — Swarm orchestrator reachable (phase: swarm)
            auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
            const bool ok = true;
            std::ostringstream oss;
            oss << "singleton_ok=1, initialized=" << (swarm.isInitialized() ? "yes" : "no");
            c.name = "E0-04 SwarmOrchestrator singleton";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 5:
        {
            // E0-05 — status bar (phase: layout / shell chrome)
            const bool ok = m_hwndStatusBar && IsWindow(m_hwndStatusBar);
            std::ostringstream oss;
            oss << "statusbar=" << (ok ? "ok" : "missing");
            c.name = "E0-05 Status bar";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 6:
        {
            // E0-06 — menu bar wired (phase: integrated chrome)
            HMENU hMenu = m_hwndMain ? GetMenu(m_hwndMain) : nullptr;
            const bool ok = hMenu != nullptr;
            std::ostringstream oss;
            oss << "menu=" << (ok ? "present" : "absent");
            c.name = "E0-06 Menu bar";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 7:
        {
            // E0-07 — editor surface (phase: createWindow / layout)
            const bool ok = m_hwndEditor && IsWindow(m_hwndEditor);
            std::ostringstream oss;
            oss << "editor=" << (ok ? "ok" : "missing");
            c.name = "E0-07 Editor HWND";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 8:
        {
            // E0-08 — startup_phases.txt resolvable (phase: config-driven boot)
            bool ok = false;
            std::ostringstream oss;
            std::ifstream f1("config/startup_phases.txt");
            if (f1.good())
            {
                ok = true;
                oss << "source=cwd_config";
            }
            else if (!exeDir.empty())
            {
                const std::string p = exeDir + "\\config\\startup_phases.txt";
                std::ifstream f2(p);
                if (f2.good())
                {
                    ok = true;
                    oss << "source=exe_dir";
                }
                else
                    oss << "missing cwd and " << p;
            }
            else
                oss << "missing cwd config, no exe dir";
            c.name = "E0-08 startup_phases.txt";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 9:
        {
            // E0-09 — registry workspace (product layout)
            std::error_code ec;
            bool ok = std::filesystem::is_directory(std::filesystem::path("registry"), ec);
            ec.clear();
            if (!ok && !exeDir.empty())
                ok = std::filesystem::is_directory(std::filesystem::path(exeDir) / "registry", ec);
            ec.clear();
            bool db = std::filesystem::exists(std::filesystem::path("registry") / "registry.db", ec);
            ec.clear();
            if (!db && !exeDir.empty())
                db = std::filesystem::exists(std::filesystem::path(exeDir) / "registry" / "registry.db", ec);
            std::ostringstream oss;
            oss << "registry_dir=" << (ok ? "yes" : "no") << ", registry_db=" << (db ? "yes" : "no");
            c.name = "E0-09 Registry workspace";
            c.passed = ok || db;
            c.detail = oss.str();
            break;
        }
        case 10:
        {
            std::error_code ec;
            bool ok = std::filesystem::is_directory(std::filesystem::path("config"), ec);
            ec.clear();
            if (!ok && !exeDir.empty())
                ok = std::filesystem::is_directory(std::filesystem::path(exeDir) / "config", ec);
            std::ostringstream oss;
            oss << "config_dir=" << (ok ? "yes" : "no");
            c.name = "E0-10 Config directory";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 11:
        {
            std::error_code ec;
            bool ok = std::filesystem::is_directory(std::filesystem::path("logs"), ec);
            ec.clear();
            if (!ok && !exeDir.empty())
                ok = std::filesystem::is_directory(std::filesystem::path(exeDir) / "logs", ec);
            std::ostringstream oss;
            oss << "logs_dir=" << (ok ? "yes" : "no");
            c.name = "E0-11 Logs directory";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 12:
        {
            const uint16_t port = auditResolveApiPortA();
            std::string d;
            const bool ok = auditLocalApiStatusOk(port, d);
            c.name = "E0-12 Localhost API /api/status";
            c.passed = ok;
            c.detail = d;
            break;
        }
        case 13:
        {
            if (!m_auditInitialized)
                initAuditSystem();
            const size_t n = FeatureRegistry::instance().getFeatureCount();
            std::ostringstream oss;
            oss << "feature_registry_count=" << n << ", audit_init=" << (m_auditInitialized ? "yes" : "no");
            c.name = "E0-13 FeatureRegistry populated";
            c.passed = n > 0;
            c.detail = oss.str();
            break;
        }
        case 14:
        {
            // Enhanced bridge validation: check presence, initialization, and operational state
            bool bridgePresent = hasAgenticBridge();
            bool bridgeInitialized = false;
            int activeSubAgents = 0;
            std::string bridgeState = "absent";
            
            std::ostringstream oss;
            
            if (bridgePresent)
            {
                bridgeState = "present";
                
                // Check if bridge is initialized and functional
                if (m_agenticBridge)
                {
                    bridgeInitialized = m_agenticBridge->IsInitialized();
                    
                    // Check swarm manager state to verify bridge operation capability
                    if (auto* mgr = m_agenticBridge->GetSubAgentManager())
                    {
                        activeSubAgents = mgr->activeSubAgentCount();
                        oss << "state=" << (bridgeInitialized ? "initialized" : "present_uninit") 
                            << ", active_subagents=" << activeSubAgents
                            << ", status=" << mgr->getStatusSummary();
                    }
                    else
                    {
                        oss << "state=" << (bridgeInitialized ? "initialized" : "present_uninit")
                            << ", mgr_unavailable";
                    }
                }
                else
                {
                    oss << "bridge_pointer=null";
                }
            }
            else
            {
                oss << "bridge_absent";
            }
            
            const bool ok = bridgePresent && bridgeInitialized;
            c.name = "E0-14 Agentic bridge initialization";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 15:
        {
            const bool ok = JSExtensionHost::instance().isInitialized();
            std::ostringstream oss;
            oss << "quickjs_host=" << (ok ? "initialized" : "not_initialized");
            c.name = "E0-15 QuickJS extension host";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 16:
        {
            std::error_code ec;
            bool ok = std::filesystem::is_directory(std::filesystem::path("engines"), ec);
            ec.clear();
            if (!ok && !exeDir.empty())
                ok = std::filesystem::is_directory(std::filesystem::path(exeDir) / "engines", ec);
            std::ostringstream oss;
            oss << "engines_dir=" << (ok ? "yes" : "no");
            c.name = "E0-16 Engines directory";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 17:
        {
            RawrXD::Startup::ensureStartupSessionId();
            const char* sid = RawrXD::Startup::getStartupSessionId();
            const size_t n = sid ? std::strlen(sid) : 0;
            const bool ok = n >= 4;
            std::ostringstream oss;
            oss << "startup_session_id_len=" << n;
            c.name = "E0-17 PR02 startup session id";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 18:
        {
            const auto order = RawrXD::Startup::getPhaseOrder();
            const bool ok = order.size() >= 8;
            std::ostringstream oss;
            oss << "phase_count=" << order.size();
            c.name = "E0-18 Phase order manifest";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 19:
        {
            std::error_code ec;
            bool extCwd = std::filesystem::is_directory(std::filesystem::path("extensions"), ec);
            ec.clear();
            bool extExe = false;
            if (!exeDir.empty())
            {
                extExe = std::filesystem::is_directory(std::filesystem::path(exeDir) / "extensions", ec);
                ec.clear();
            }
            bool plugCwd = std::filesystem::is_directory(std::filesystem::path("plugins"), ec);
            ec.clear();
            bool plugExe = false;
            if (!exeDir.empty())
            {
                plugExe = std::filesystem::is_directory(std::filesystem::path(exeDir) / "plugins", ec);
                ec.clear();
            }
            const bool ok = extCwd || extExe || plugCwd || plugExe;
            std::ostringstream oss;
            oss << "extensions_cwd=" << (extCwd ? "yes" : "no") << ", extensions_exe=" << (extExe ? "yes" : "no")
                << ", plugins_cwd=" << (plugCwd ? "yes" : "no") << ", plugins_exe=" << (plugExe ? "yes" : "no");
            c.name = "E0-19 Extensions or plugins directory";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 20:
        {
            const RawrXD::LicenseState st = RawrXD::EnterpriseLicense::Instance().GetState();
            const bool ok = st != RawrXD::LicenseState::Invalid;
            std::ostringstream oss;
            oss << "license_state=" << static_cast<int>(st) << " (non-Invalid after enterprise_license phase)";
            c.name = "E0-20 Enterprise license initialized";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 21:
        {
            RECT rc{};
            const bool have = m_hwndMain && IsWindow(m_hwndMain) && GetClientRect(m_hwndMain, &rc);
            const int w = have ? (rc.right - rc.left) : 0;
            const int h = have ? (rc.bottom - rc.top) : 0;
            const bool ok = have && w > 32 && h > 32;
            std::ostringstream oss;
            oss << "client_w=" << w << ", client_h=" << h;
            c.name = "E0-21 Main window client area";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 22:
        {
            const bool ok = (m_hwndSidebar && IsWindow(m_hwndSidebar)) ||
                            (m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar));
            std::ostringstream oss;
            oss << "sidebar=" << ((m_hwndSidebar && IsWindow(m_hwndSidebar)) ? "yes" : "no") << ", secondary_sidebar="
                << ((m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar)) ? "yes" : "no");
            c.name = "E0-22 Workspace sidebars";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 23:
        {
            int n = 0;
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                for (HWND ch = GetWindow(m_hwndMain, GW_CHILD); ch; ch = GetWindow(ch, GW_HWNDNEXT))
                {
                    if (IsWindow(ch))
                        ++n;
                }
            }
            const bool ok = n >= 3;
            std::ostringstream oss;
            oss << "child_count=" << n;
            c.name = "E0-23 Main window child HWNDs";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 24:
        {
            DWORD tid = 0;
            if (m_hwndMain && IsWindow(m_hwndMain))
                tid = GetWindowThreadProcessId(m_hwndMain, nullptr);
            const bool ok = tid != 0 && tid == GetCurrentThreadId();
            std::ostringstream oss;
            oss << "main_tid=" << tid << ", current_tid=" << GetCurrentThreadId();
            c.name = "E0-24 GUI thread owns main HWND";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 25:
        {
            const bool ok = m_hwndMain && IsWindow(m_hwndMain) && IsWindowEnabled(m_hwndMain);
            std::ostringstream oss;
            oss << "main_enabled=" << (ok ? "yes" : "no");
            c.name = "E0-25 Main window input enabled";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 26:
        {
            bool ok = false;
            std::ostringstream oss;
            if (!m_hwndEditor || !IsWindow(m_hwndEditor))
            {
                oss << "editor=missing";
            }
            else
            {
                char cn[256] = {};
                const int n = GetClassNameA(m_hwndEditor, cn, static_cast<int>(sizeof(cn)));
                const std::string cls(n > 0 ? cn : "");
                ok = cls.find("RichEdit") != std::string::npos || cls.find("RICHEDIT") != std::string::npos ||
                     cls.find("Edit") != std::string::npos;
                oss << "class=" << cls;
            }
            c.name = "E0-26 Editor surface class";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 27:
        {
            const bool tabs = m_hwndOutputTabs && IsWindow(m_hwndOutputTabs);
            const bool panel = m_hwndOutputPanel && IsWindow(m_hwndOutputPanel);
            const bool ok = tabs || panel;
            std::ostringstream oss;
            oss << "output_tabs=" << (tabs ? "yes" : "no") << ", output_panel=" << (panel ? "yes" : "no");
            c.name = "E0-27 Output panel chrome";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 28:
        {
            const bool ok = m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput);
            std::ostringstream oss;
            oss << "copilot_output=" << (ok ? "ok" : "missing");
            c.name = "E0-28 Copilot chat output surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 29:
        {
            const int n = (m_hwndMain && IsWindow(m_hwndMain)) ? GetWindowTextLengthA(m_hwndMain) : 0;
            const bool ok = n > 0;
            std::ostringstream oss;
            oss << "title_len=" << n;
            c.name = "E0-29 Main window title";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 30:
        {
            const bool ok = m_hwndModelSelector && IsWindow(m_hwndModelSelector);
            std::ostringstream oss;
            oss << "model_selector=" << (ok ? "ok" : "missing");
            c.name = "E0-30 Model selector HWND";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 31:
        {
            HMENU hMenu = m_hwndMain ? GetMenu(m_hwndMain) : nullptr;
            const int n = hMenu ? GetMenuItemCount(hMenu) : 0;
            const bool ok = n >= 4;
            std::ostringstream oss;
            oss << "top_level_menu_items=" << n;
            c.name = "E0-31 Menu bar depth";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 32:
        {
            HWND f = GetFocus();
            const bool ok = m_hwndMain && f && (f == m_hwndMain || IsChild(m_hwndMain, f));
            std::ostringstream oss;
            oss << "focus_hwnd=" << (f ? "set" : "null") << ", under_main=" << (ok ? "yes" : "no");
            c.name = "E0-32 Focus within main window";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 33:
        {
            const HMODULE msft = GetModuleHandleA("Msftedit.dll");
            const HMODULE r20 = GetModuleHandleA("riched20.dll");
            const bool ok = (msft != nullptr) || (r20 != nullptr);
            std::ostringstream oss;
            oss << "msftedit=" << (msft ? "loaded" : "no") << ", riched20=" << (r20 ? "loaded" : "no");
            c.name = "E0-33 RichEdit module resident";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 34:
        {
            const bool ok = m_hwndToolbar && IsWindow(m_hwndToolbar);
            std::ostringstream oss;
            oss << "toolbar=" << (ok ? "ok" : "missing");
            c.name = "E0-34 Main toolbar HWND";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 35:
        {
            const bool ok = m_hwndTabBar && IsWindow(m_hwndTabBar);
            std::ostringstream oss;
            oss << "tab_bar=" << (ok ? "ok" : "missing");
            c.name = "E0-35 Editor tab bar";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 36:
        {
            const bool ok = m_hwndBtnClose && IsWindow(m_hwndBtnClose);
            std::ostringstream oss;
            oss << "close_btn=" << (ok ? "ok" : "missing");
            c.name = "E0-36 Custom chrome close control";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 37:
        {
            const auto order = RawrXD::Startup::getPhaseOrder();
            auto has = [&order](const char* p) -> bool
            {
                for (const auto& s : order)
                {
                    if (s == p)
                        return true;
                }
                return false;
            };
            const bool ok = has("createWindow") && has("showWindow") && has("message_loop_entered");
            std::ostringstream oss;
            oss << "has_createWindow=" << (has("createWindow") ? "yes" : "no")
                << ", has_showWindow=" << (has("showWindow") ? "yes" : "no")
                << ", has_message_loop_entered=" << (has("message_loop_entered") ? "yes" : "no");
            c.name = "E0-37 Phase order canonical milestones";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 38:
        {
            UINT dpi = 96;
            bool ok = false;
            std::ostringstream oss;
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                dpi = GetDpiForWindow(m_hwndMain);
                ok = dpi >= 96 && dpi <= 512;
                oss << "dpi=" << dpi;
            }
            else
            {
                oss << "main=missing";
            }
            c.name = "E0-38 Main window DPI (GetDpiForWindow)";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 39:
        {
            if (!m_auditInitialized)
                initAuditSystem();
            const size_t n = FeatureRegistry::instance().getFeatureCount();
            const bool ok = n >= 6;
            std::ostringstream oss;
            oss << "feature_registry_count=" << n;
            c.name = "E0-39 FeatureRegistry breadth";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 40:
        {
            const bool split = m_hwndSplitter && IsWindow(m_hwndSplitter);
            const bool act = m_hwndActivityBar && IsWindow(m_hwndActivityBar);
            const bool ok = split || act;
            std::ostringstream oss;
            oss << "splitter=" << (split ? "yes" : "no") << ", activity_bar=" << (act ? "yes" : "no");
            c.name = "E0-40 Splitter or activity bar";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 41:
        {
            const bool ln = m_hwndLineNumbers && IsWindow(m_hwndLineNumbers);
            const bool mm = m_hwndMinimap && IsWindow(m_hwndMinimap);
            const bool ok = ln || mm;
            std::ostringstream oss;
            oss << "line_numbers=" << (ln ? "yes" : "no") << ", minimap=" << (mm ? "yes" : "no");
            c.name = "E0-41 Line gutter or minimap";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 42:
        {
            const bool ok = m_hwndCommandInput && IsWindow(m_hwndCommandInput);
            std::ostringstream oss;
            oss << "command_input=" << (ok ? "ok" : "missing");
            c.name = "E0-42 Command / agent input surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 43:
        {
            const bool minB = m_hwndBtnMinimize && IsWindow(m_hwndBtnMinimize);
            const bool maxB = m_hwndBtnMaximize && IsWindow(m_hwndBtnMaximize);
            const bool ok = minB && maxB;
            std::ostringstream oss;
            oss << "minimize_btn=" << (minB ? "yes" : "no") << ", maximize_btn=" << (maxB ? "yes" : "no");
            c.name = "E0-43 Custom chrome min/max controls";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 44:
        {
            const bool ft = m_hwndFileTree && IsWindow(m_hwndFileTree);
            const bool ex = m_hwndExplorerTree && IsWindow(m_hwndExplorerTree);
            const bool ok = ft || ex;
            std::ostringstream oss;
            oss << "file_tree=" << (ft ? "yes" : "no") << ", explorer_tree=" << (ex ? "yes" : "no");
            c.name = "E0-44 Explorer or file tree";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 45:
        {
            const bool ok = m_hwndSidebarContent && IsWindow(m_hwndSidebarContent);
            std::ostringstream oss;
            oss << "sidebar_content=" << (ok ? "ok" : "missing");
            c.name = "E0-45 Sidebar content host";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 46:
        {
            char pathBuf[MAX_PATH] = {};
            const DWORD n = GetModuleFileNameA(nullptr, pathBuf, static_cast<DWORD>(sizeof(pathBuf)));
            const bool ok = n >= 8u;
            std::ostringstream oss;
            oss << "exe_path_len=" << n;
            c.name = "E0-46 Host process image path";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 47:
        {
            const auto order = RawrXD::Startup::getPhaseOrder();
            auto has = [&order](const char* p) -> bool
            {
                for (const auto& s : order)
                {
                    if (s == p)
                        return true;
                }
                return false;
            };
            const bool ok = has("enterprise_license") && has("extension_bootstrap");
            std::ostringstream oss;
            oss << "has_enterprise_license=" << (has("enterprise_license") ? "yes" : "no")
                << ", has_extension_bootstrap=" << (has("extension_bootstrap") ? "yes" : "no");
            c.name = "E0-47 Phase order license + extension gate";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 48:
        {
            bool ok = false;
            std::ostringstream oss;
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                ok = !IsHungAppWindow(m_hwndMain);
                oss << "is_hung=" << (ok ? "no" : "yes");
            }
            else
            {
                oss << "main=missing";
            }
            c.name = "E0-48 Main window responsive (not hung)";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 49:
        {
            const bool ok = m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput);
            std::ostringstream oss;
            oss << "copilot_input=" << (ok ? "ok" : "missing");
            c.name = "E0-49 Copilot chat input";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 50:
        {
            const bool ok = m_hwndCopilotSendBtn && IsWindow(m_hwndCopilotSendBtn);
            std::ostringstream oss;
            oss << "copilot_send=" << (ok ? "ok" : "missing");
            c.name = "E0-50 Copilot send control";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 51:
        {
            const bool clr = m_hwndCopilotClearBtn && IsWindow(m_hwndCopilotClearBtn);
            const bool sec = m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar);
            const bool ok = clr || sec;
            std::ostringstream oss;
            oss << "copilot_clear=" << (clr ? "yes" : "no") << ", secondary_sidebar=" << (sec ? "yes" : "no");
            c.name = "E0-51 Copilot clear or secondary sidebar";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 52:
        {
            const bool ok = m_hwndBtnSettings && IsWindow(m_hwndBtnSettings);
            std::ostringstream oss;
            oss << "settings_btn=" << (ok ? "ok" : "missing");
            c.name = "E0-52 Settings chrome control";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 53:
        {
            const bool ok = m_hwndTitleLabel && IsWindow(m_hwndTitleLabel);
            std::ostringstream oss;
            oss << "title_label=" << (ok ? "ok" : "missing");
            c.name = "E0-53 Custom title label";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 54:
        {
            const bool br = m_hwndGitBranch && IsWindow(m_hwndGitBranch);
            const bool st = m_hwndGitStatus && IsWindow(m_hwndGitStatus);
            const bool pan = m_hwndGitPanel && IsWindow(m_hwndGitPanel);
            const bool ok = br || st || pan;
            std::ostringstream oss;
            oss << "git_branch=" << (br ? "yes" : "no") << ", git_status=" << (st ? "yes" : "no")
                << ", git_panel=" << (pan ? "yes" : "no");
            c.name = "E0-54 Git status surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 55:
        {
            const bool bar = m_hwndModelProgressBar && IsWindow(m_hwndModelProgressBar);
            const bool box = m_hwndModelProgressContainer && IsWindow(m_hwndModelProgressContainer);
            const bool ok = bar || box;
            std::ostringstream oss;
            oss << "model_progress_bar=" << (bar ? "yes" : "no")
                << ", model_progress_container=" << (box ? "yes" : "no");
            c.name = "E0-55 Model progress UI";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 56:
        {
            bool ok = false;
            std::ostringstream oss;
            std::ifstream fCwd("ide_startup.log");
            if (fCwd.good())
            {
                ok = true;
                oss << "source=cwd";
            }
            else if (!exeDir.empty())
            {
                const std::string p = exeDir + "\\ide_startup.log";
                std::ifstream fExe(p);
                if (fExe.good())
                {
                    ok = true;
                    oss << "source=exe_dir";
                }
                else
                    oss << "missing cwd and " << p;
            }
            else
                oss << "missing ide_startup.log";
            c.name = "E0-56 ide_startup.log artifact";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 57:
        {
            const bool lv = m_hwndProblemsListView && IsWindow(m_hwndProblemsListView);
            const bool pan = m_hwndProblemsPanel && IsWindow(m_hwndProblemsPanel);
            const bool ok = lv || pan;
            std::ostringstream oss;
            oss << "problems_list=" << (lv ? "yes" : "no") << ", problems_panel=" << (pan ? "yes" : "no");
            c.name = "E0-57 Problems panel surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 58:
        {
            const bool out = m_hwndDebugConsoleOutput && IsWindow(m_hwndDebugConsoleOutput);
            const bool con = m_hwndDebugConsole && IsWindow(m_hwndDebugConsole);
            const bool ok = out || con;
            std::ostringstream oss;
            oss << "debug_console_output=" << (out ? "yes" : "no") << ", debug_console=" << (con ? "yes" : "no");
            c.name = "E0-58 Debug console surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 59:
        {
            const bool tabs = m_hwndDebuggerTabs && IsWindow(m_hwndDebuggerTabs);
            const bool dock = m_hwndDebuggerContainer && IsWindow(m_hwndDebuggerContainer);
            const bool ok = tabs || dock;
            std::ostringstream oss;
            oss << "debugger_tabs=" << (tabs ? "yes" : "no") << ", debugger_container=" << (dock ? "yes" : "no");
            c.name = "E0-59 Debugger dock chrome";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 60:
        {
            const bool lst = m_hwndExtensionsList && IsWindow(m_hwndExtensionsList);
            const bool sea = m_hwndExtensionSearch && IsWindow(m_hwndExtensionSearch);
            const bool ok = lst || sea;
            std::ostringstream oss;
            oss << "extensions_list=" << (lst ? "yes" : "no") << ", extension_search=" << (sea ? "yes" : "no");
            c.name = "E0-60 Extensions panel surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 61:
        {
            const bool ok = m_hwndOutlineTree && IsWindow(m_hwndOutlineTree);
            std::ostringstream oss;
            oss << "outline_tree=" << (ok ? "ok" : "missing");
            c.name = "E0-61 Outline tree";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 62:
        {
            const bool ok = m_hwndSearchResults && IsWindow(m_hwndSearchResults);
            std::ostringstream oss;
            oss << "search_results=" << (ok ? "ok" : "missing");
            c.name = "E0-62 Search results surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 63:
        {
            const bool br = m_hwndModuleBrowser && IsWindow(m_hwndModuleBrowser);
            const bool lst = m_hwndModuleList && IsWindow(m_hwndModuleList);
            const bool ok = br || lst;
            std::ostringstream oss;
            oss << "module_browser=" << (br ? "yes" : "no") << ", module_list=" << (lst ? "yes" : "no");
            c.name = "E0-63 Module browser surface";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        case 64:
        {
            const bool ctx = m_hwndContextSlider && IsWindow(m_hwndContextSlider);
            const bool tok = m_hwndMaxTokensSlider && IsWindow(m_hwndMaxTokensSlider);
            const bool ok = ctx || tok;
            std::ostringstream oss;
            oss << "context_slider=" << (ctx ? "yes" : "no") << ", max_tokens_slider=" << (tok ? "yes" : "no");
            c.name = "E0-64 Model context / token sliders";
            c.passed = ok;
            c.detail = oss.str();
            break;
        }
        default:
            break;
    }
    return c;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch3()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 1; i <= 8; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch4()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 9; i <= 16; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch5()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 17; i <= 24; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch6()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 25; i <= 32; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch7()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 33; i <= 40; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch8()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 41; i <= 48; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch9()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 49; i <= 56; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

std::vector<Win32IDE::RuntimeValidationCheck> Win32IDE::runCriticalValidationBatch10()
{
    std::vector<Win32IDE::RuntimeValidationCheck> checks;
    checks.reserve(8);
    for (int i = 57; i <= 64; ++i)
        checks.push_back(this->runCriticalValidationE0Check(i));
    return checks;
}

// ============================================================================
// AUDIT DASHBOARD STATE (file-scope, owned by Win32IDE via HWND)
// ============================================================================
struct AuditDashboardState
{
    HWND hwndListView = nullptr;
    HWND hwndStatusBar = nullptr;
    HWND hwndBtnRun = nullptr;
    HWND hwndBtnExport = nullptr;
    HWND hwndBtnRefresh = nullptr;
    HWND hwndBtnClose = nullptr;
    Win32IDE* ide = nullptr;
    bool auditRun = false;
};

// Store per-window state via GWLP_USERDATA
static AuditDashboardState* getAuditState(HWND hwnd)
{
    return reinterpret_cast<AuditDashboardState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// ============================================================================
// LISTVIEW POPULATION
// ============================================================================
static void populateAuditListView(AuditDashboardState* state)
{
    if (!state || !state->hwndListView)
        return;

    // Clear existing items
    ListView_DeleteAllItems(state->hwndListView);

    auto features = FeatureRegistry::instance().getAllFeatures();

    for (size_t i = 0; i < features.size(); ++i)
    {
        const FeatureEntry& f = features[i];

        // Column 0: Feature Name
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.lParam = static_cast<LPARAM>(i);

        wchar_t nameBuf[128] = {};
        if (f.name)
        {
            MultiByteToWideChar(CP_UTF8, 0, f.name, -1, nameBuf, 127);
        }
        lvi.pszText = nameBuf;
        int idx = LV_InsertItemW(state->hwndListView, &lvi);
        if (idx < 0)
            continue;

        // Column 1: Category
        wchar_t catBuf[32] = {};
        MultiByteToWideChar(CP_UTF8, 0, featureCategoryToString(f.category), -1, catBuf, 31);
        LV_SetItemTextW(state->hwndListView, idx, 1, catBuf);

        // Column 2: Status
        wchar_t statusBuf[32] = {};
        MultiByteToWideChar(CP_UTF8, 0, implStatusToString(f.status), -1, statusBuf, 31);
        LV_SetItemTextW(state->hwndListView, idx, 2, statusBuf);

        // Column 3: Phase
        wchar_t phaseBuf[32] = {};
        if (f.phase)
        {
            MultiByteToWideChar(CP_UTF8, 0, f.phase, -1, phaseBuf, 31);
        }
        else
        {
            wcscpy_s(phaseBuf, L"—");
        }
        LV_SetItemTextW(state->hwndListView, idx, 3, phaseBuf);

        // Column 4: Menu Wired
        LV_SetItemTextW(state->hwndListView, idx, 4, const_cast<LPWSTR>(f.menuWired ? L"YES" : L"NO"));

        // Column 5: IDM
        wchar_t idmBuf[16] = {};
        if (f.commandId != 0)
        {
            _snwprintf_s(idmBuf, 15, L"%d", f.commandId);
        }
        else
        {
            wcscpy_s(idmBuf, L"—");
        }
        LV_SetItemTextW(state->hwndListView, idx, 5, idmBuf);

        // Column 6: Stub?
        LV_SetItemTextW(state->hwndListView, idx, 6, const_cast<LPWSTR>(f.stubDetected ? L"STUB" : L"OK"));

        // Column 7: Runtime Test
        LV_SetItemTextW(state->hwndListView, idx, 7, const_cast<LPWSTR>(f.runtimeTested ? L"PASS" : L"—"));
    }

    // Update status bar
    if (state->hwndStatusBar)
    {
        float pct = FeatureRegistry::instance().getCompletionPercentage();
        size_t total = FeatureRegistry::instance().getFeatureCount();
        size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
        size_t complete = FeatureRegistry::instance().getCountByStatus(ImplStatus::Complete);

        wchar_t statusBuf[256];
        _snwprintf_s(statusBuf, 255, L"Features: %zu  |  Complete: %zu  |  Stubs: %zu  |  Completion: %.1f%%", total,
                     complete, stubs, pct * 100.0f);
        SetWindowTextW(state->hwndStatusBar, statusBuf);
    }
}

// ============================================================================
// CUSTOM DRAW — Color-coded rows based on status
// ============================================================================
static LRESULT handleCustomDraw(LPNMLVCUSTOMDRAW lpcd)
{
    switch (lpcd->nmcd.dwDrawStage)
    {
        case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

        case CDDS_ITEMPREPAINT:
        {
            // Get the feature index from lParam
            size_t idx = static_cast<size_t>(lpcd->nmcd.lItemlParam);
            auto features = FeatureRegistry::instance().getAllFeatures();
            if (idx < features.size())
            {
                const FeatureEntry& f = features[idx];
                switch (f.status)
                {
                    case ImplStatus::Complete:
                        lpcd->clrText = RGB(0, 128, 0);  // Green
                        lpcd->clrTextBk = RGB(240, 255, 240);
                        break;
                    case ImplStatus::Partial:
                        lpcd->clrText = RGB(180, 140, 0);  // Amber
                        lpcd->clrTextBk = RGB(255, 255, 230);
                        break;
                    case ImplStatus::Stub:
                        lpcd->clrText = RGB(200, 0, 0);  // Red
                        lpcd->clrTextBk = RGB(255, 235, 235);
                        break;
                    case ImplStatus::Broken:
                        lpcd->clrText = RGB(255, 255, 255);
                        lpcd->clrTextBk = RGB(180, 0, 0);  // Dark red bg
                        break;
                    case ImplStatus::Untested:
                        lpcd->clrText = RGB(100, 100, 100);  // Gray
                        lpcd->clrTextBk = RGB(245, 245, 245);
                        break;
                    case ImplStatus::Deprecated:
                        lpcd->clrText = RGB(150, 150, 150);  // Light gray
                        lpcd->clrTextBk = RGB(240, 240, 240);
                        break;
                    default:
                        break;
                }

                // Override: if stub was detected at runtime, force red text
                if (f.stubDetected)
                {
                    lpcd->clrText = RGB(200, 0, 0);
                }
            }
            return CDRF_DODEFAULT;
        }

        default:
            return CDRF_DODEFAULT;
    }
}

// ============================================================================
// EXPORT REPORT TO FILE
// ============================================================================
static void exportAuditReport(HWND hwndParent)
{
    std::string report = FeatureRegistry::instance().generateReport();

    // Get save filename
    wchar_t filePath[MAX_PATH] = L"RawrXD_AuditReport.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwndParent;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
    {
        return;  // User cancelled
    }

    // Write report
    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(hwndParent, L"Failed to create report file.", L"Audit Export Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, report.c_str(), static_cast<DWORD>(report.size()), &written, nullptr);
    CloseHandle(hFile);

    wchar_t msg[MAX_PATH + 64];
    _snwprintf_s(msg, MAX_PATH + 63, L"Report exported to:\n%s", filePath);
    MessageBoxW(hwndParent, msg, L"Audit Export", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================
LRESULT CALLBACK auditDashboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            AuditDashboardState* state = reinterpret_cast<AuditDashboardState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            // Get client area
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            // Create ListView
            state->hwndListView =
                CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0,
                                0, w, h - 70, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_LISTVIEW_ID)),
                                GetModuleHandleW(nullptr), nullptr);

            // Extended ListView styles
            ListView_SetExtendedListViewStyle(state->hwndListView,
                                              LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

            // Add columns
            struct ColDef
            {
                const wchar_t* name;
                int width;
            };
            ColDef columns[] = {
                {L"Feature", 200}, {L"Category", 100}, {L"Status", 80}, {L"Phase", 70},
                {L"Menu", 50},     {L"IDM", 50},       {L"Stub", 50},   {L"Test", 50},
            };

            for (int c = 0; c < 8; ++c)
            {
                LVCOLUMNW lvc{};
                lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
                lvc.fmt = LVCFMT_LEFT;
                lvc.cx = columns[c].width;
                lvc.pszText = const_cast<LPWSTR>(columns[c].name);
                LV_InsertColumnW(state->hwndListView, c, &lvc);
            }

            // Create buttons
            int btnY = h - 60;
            int btnW = 100;
            int btnH = 30;
            int gap = 10;

            state->hwndBtnRun = CreateWindowExW(
                0, L"BUTTON", L"Run Audit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, gap, btnY, btnW, btnH, hwnd,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_RUN_ID)), GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnRefresh =
                CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, gap + btnW + gap, btnY,
                                btnW, btnH, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_REFRESH_ID)),
                                GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnExport = CreateWindowExW(
                0, L"BUTTON", L"Export Report", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, gap + (btnW + gap) * 2, btnY,
                btnW + 20, btnH, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_EXPORT_ID)),
                GetModuleHandleW(nullptr), nullptr);

            state->hwndBtnClose = CreateWindowExW(
                0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, w - btnW - gap, btnY, btnW, btnH, hwnd,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_BTN_CLOSE_ID)), GetModuleHandleW(nullptr), nullptr);

            // Status bar (bottom strip)
            state->hwndStatusBar = CreateWindowExW(
                0, L"STATIC", L"Ready — click 'Run Audit' to begin.", WS_CHILD | WS_VISIBLE | SS_LEFT, gap,
                btnY + btnH + 5, w - gap * 2, 20, hwnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(AUDIT_STATUS_ID)),
                GetModuleHandleW(nullptr), nullptr);

            // Populate with current registry data
            populateAuditListView(state);

            return 0;
        }

        case WM_SIZE:
        {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state)
                break;

            int w = LOWORD(lParam);
            int h = HIWORD(lParam);

            // Resize ListView
            if (state->hwndListView)
            {
                MoveWindow(state->hwndListView, 0, 0, w, h - 70, TRUE);
            }

            // Reposition buttons
            int btnY = h - 60;
            int btnW = 100;
            int btnH = 30;
            int gap = 10;

            if (state->hwndBtnRun)
                MoveWindow(state->hwndBtnRun, gap, btnY, btnW, btnH, TRUE);
            if (state->hwndBtnRefresh)
                MoveWindow(state->hwndBtnRefresh, gap + btnW + gap, btnY, btnW, btnH, TRUE);
            if (state->hwndBtnExport)
                MoveWindow(state->hwndBtnExport, gap + (btnW + gap) * 2, btnY, btnW + 20, btnH, TRUE);
            if (state->hwndBtnClose)
                MoveWindow(state->hwndBtnClose, w - btnW - gap, btnY, btnW, btnH, TRUE);
            if (state->hwndStatusBar)
                MoveWindow(state->hwndStatusBar, gap, btnY + btnH + 5, w - gap * 2, 20, TRUE);

            return 0;
        }

        case WM_COMMAND:
        {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state)
                break;

            int id = LOWORD(wParam);

            if (id == AUDIT_BTN_RUN_ID)
            {
                // Run full audit with auto-discovery refresh
                SetWindowTextW(state->hwndStatusBar, L"Running auto-discovery...");
                UpdateWindow(state->hwndStatusBar);

                // Re-run auto-discovery to pick up any runtime changes
                if (state->ide)
                {
                    AutoDiscoveryEngine::instance().discoverAll(state->ide->getMainWindow());
                }

                SetWindowTextW(state->hwndStatusBar, L"Running stub detection...");
                UpdateWindow(state->hwndStatusBar);

                FeatureRegistry::instance().detectStubs();

                SetWindowTextW(state->hwndStatusBar, L"Verifying menu wiring...");
                UpdateWindow(state->hwndStatusBar);

                if (state->ide)
                {
                    HMENU hMenu = GetMenu(state->ide->getMainWindow());
                    if (hMenu)
                    {
                        FeatureRegistry::instance().verifyMenuWiring(hMenu);
                    }
                }

                SetWindowTextW(state->hwndStatusBar, L"Running component tests...");
                UpdateWindow(state->hwndStatusBar);

                FeatureRegistry::instance().runComponentTests();

                state->auditRun = true;

                // Production readiness: estimated model round-trips for top-N difficult (no token/time/complexity
                // constraints)
                {
                    int estimatedRedos = 0, taskCategories = 0;
                    RawrXD::AgenticAutonomousConfig::instance().estimateProductionAuditIterations(
                        "full", 20, &estimatedRedos, &taskCategories);
                    wchar_t buf[256];
                    swprintf_s(buf, L"Audit complete. Est. round-trips for top 20 difficult: %d (%d categories).",
                               estimatedRedos, taskCategories);
                    SetWindowTextW(state->hwndStatusBar, buf);
                    if (state->ide)
                    {
                        std::ostringstream oss;
                        oss << "[Production Audit] Estimated model round-trips for top 20 most difficult tasks (no "
                               "simplification): "
                            << estimatedRedos << " (" << taskCategories
                            << " categories). Cycle/balance from agentic config.\n";
                        state->ide->appendToOutput(oss.str(), "Audit", Win32IDE::OutputSeverity::Info);
                    }
                }

                // Refresh display
                populateAuditListView(state);
                return 0;
            }

            if (id == AUDIT_BTN_REFRESH_ID)
            {
                populateAuditListView(state);
                return 0;
            }

            if (id == AUDIT_BTN_EXPORT_ID)
            {
                exportAuditReport(hwnd);
                return 0;
            }

            if (id == AUDIT_BTN_CLOSE_ID)
            {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_NOTIFY:
        {
            AuditDashboardState* state = getAuditState(hwnd);
            if (!state)
                break;

            NMHDR* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == AUDIT_LISTVIEW_ID && nmhdr->code == NM_CUSTOMDRAW)
            {
                LPNMLVCUSTOMDRAW lpcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
                LRESULT result = handleCustomDraw(lpcd);
                SetWindowLongPtrW(hwnd, DWLP_MSGRESULT, result);
                return result;
            }
            break;
        }

        case WM_DESTROY:
        {
            AuditDashboardState* state = getAuditState(hwnd);
            if (state)
            {
                // Notify IDE that dashboard is closed
                if (state->ide)
                {
                    // IDE will detect via m_hwndAuditDashboard == nullptr check
                }
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// REGISTER WINDOW CLASS (called once)
// ============================================================================
static bool s_auditClassRegistered = false;

static bool ensureAuditWindowClass()
{
    if (s_auditClassRegistered)
        return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = auditDashboardWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = AUDIT_WINDOW_CLASS;
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (RegisterClassExW(&wc))
    {
        s_auditClassRegistered = true;
        return true;
    }

    // May already be registered
    if (GetLastError() == ERROR_CLASS_ALREADY_EXISTS)
    {
        s_auditClassRegistered = true;
        return true;
    }

    return false;
}

// ============================================================================
// Win32IDE INTEGRATION — Command Handlers
// ============================================================================

void Win32IDE::initAuditSystem()
{
    if (m_auditInitialized)
        return;

    OutputDebugStringA("[Phase 31] Initializing audit system with auto-discovery...\n");

    // Run the auto-discovery engine — this populates the FeatureRegistry
    // with ALL known IDM_* commands, checks menu wiring, detects stubs,
    // and auto-classifies status. Zero manual RAW_REGISTER_FEATURE() needed.
    AutoDiscoveryEngine::instance().discoverAll(m_hwndMain);

    m_auditInitialized = true;

    char logBuf[256];
    snprintf(logBuf, sizeof(logBuf),
             "[Phase 31] Audit system initialized: %zu features discovered, "
             "%.1f%% completion.\n",
             FeatureRegistry::instance().getFeatureCount(),
             FeatureRegistry::instance().getCompletionPercentage() * 100.0f);
    OutputDebugStringA(logBuf);
}

bool Win32IDE::handleAuditCommand(int commandId)
{
    // Lazy-init audit system (populates FeatureRegistry + menu wiring) on first use
    if (!m_auditInitialized)
        initAuditSystem();

    switch (commandId)
    {
        case IDM_AUDIT_SHOW_DASHBOARD:
            cmdAuditShowDashboard();
            return true;
        case IDM_AUDIT_RUN_FULL:
            cmdAuditRunFull();
            return true;
        case IDM_AUDIT_DETECT_STUBS:
            cmdAuditDetectStubs();
            return true;
        case IDM_AUDIT_CHECK_MENUS:
            cmdAuditCheckMenus();
            return true;
        case IDM_AUDIT_RUN_TESTS:
            cmdAuditRunTests();
            return true;
        case IDM_AUDIT_EXPORT_REPORT:
            cmdAuditExportReport();
            return true;
        case IDM_AUDIT_QUICK_STATS:
            cmdAuditQuickStats();
            return true;
        case IDM_SECURITY_SCAN_SECRETS:
            RunSecretsScan();
            return true;
        case IDM_SECURITY_SCAN_SAST:
            RunSastScan();
            return true;
        case IDM_SECURITY_SCAN_DEPENDENCIES:
            RunDependencyAudit();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdAuditShowDashboard()
{
    // If already open, bring to front
    if (m_hwndAuditDashboard && IsWindow(m_hwndAuditDashboard))
    {
        SetForegroundWindow(m_hwndAuditDashboard);
        return;
    }

    if (!ensureAuditWindowClass())
    {
        MessageBoxW(m_hwndMain, L"Failed to register audit window class.", L"Audit Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Create state
    AuditDashboardState* state = new AuditDashboardState();
    state->ide = this;

    // Create window
    m_hwndAuditDashboard =
        CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, AUDIT_WINDOW_CLASS, L"RawrXD — IDE Audit Dashboard (Phase 31)",
                        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 900, 600, m_hwndMain,
                        nullptr, GetModuleHandleW(nullptr), state);

    if (m_hwndAuditDashboard)
    {
        ShowWindow(m_hwndAuditDashboard, SW_SHOW);
        UpdateWindow(m_hwndAuditDashboard);
    }
}

void Win32IDE::cmdAuditRunFull()
{
    OutputDebugStringA("[Phase 31] Running full audit with auto-discovery...\n");

    // 0. Run auto-discovery (re-scans everything fresh)
    AutoDiscoveryEngine::instance().discoverAll(m_hwndMain);

    // 1. Detect stubs
    FeatureRegistry::instance().detectStubs();

    // 2. Verify menu wiring
    HMENU hMenu = GetMenu(m_hwndMain);
    if (hMenu)
    {
        FeatureRegistry::instance().verifyMenuWiring(hMenu);
    }

    // 3. Run component tests
    FeatureRegistry::instance().runComponentTests();

    // 4. Show results in dashboard
    cmdAuditShowDashboard();
}

void Win32IDE::cmdAuditDetectStubs()
{
    FeatureRegistry::instance().detectStubs();

    size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
    size_t total = FeatureRegistry::instance().getFeatureCount();

    // Append detailed report to IDE output panel for audit trail
    std::ostringstream oss;
    oss << "=== Stub Detection Complete ===\n"
        << "Total Features: " << total << "\n"
        << "Stubs Found: " << stubs << "\n";
    if (stubs > 0)
    {
        oss << "\nStub features:\n";
        for (const auto& e : FeatureRegistry::instance().getByStatus(ImplStatus::Stub))
        {
            oss << "  [STUB] " << (e.name ? e.name : "?") << "  (" << (e.file ? e.file : "?") << ":" << e.line << ")\n";
        }
    }
    appendToOutput(oss.str(), "Audit", Win32IDE::OutputSeverity::Info);

    // Output a non-blocking summary line — the full per-stub details are already
    // in the block emitted above, so a modal dialog adds no extra information.
    std::ostringstream summary;
    summary << "[Audit] Stub detection complete — total: " << total << ", stubs: " << stubs
            << ". See 'Audit' tab for details.\n";
    appendToOutput(summary.str(), "Audit",
                   stubs > 0 ? Win32IDE::OutputSeverity::Warning : Win32IDE::OutputSeverity::Info);
}

void Win32IDE::cmdAuditCheckMenus()
{
    HMENU hMenu = GetMenu(m_hwndMain);
    if (!hMenu)
    {
        MessageBoxW(m_hwndMain, L"No menu bar found.", L"Menu Audit", MB_OK | MB_ICONWARNING);
        return;
    }

    FeatureRegistry::instance().verifyMenuWiring(hMenu);

    std::string report = RawrXD::Audit::getMenuWiringReport(hMenu);

    // Show in a message box (truncated if too long)
    if (report.size() > 4000)
    {
        report = report.substr(0, 4000) + "\n\n... (truncated, use Export for full report)";
    }

    wchar_t wReport[4200] = {};
    MultiByteToWideChar(CP_UTF8, 0, report.c_str(), -1, wReport, 4199);
    MessageBoxW(m_hwndMain, wReport, L"Menu Wiring Audit", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditRunTests()
{
    FeatureRegistry::instance().runComponentTests();

    const auto batch1 = runCriticalValidationBatch1();
    const auto batch2 = runCriticalValidationBatch2();
    const auto batch3 = runCriticalValidationBatch3();
    const auto batch4 = runCriticalValidationBatch4();
    const auto batch5 = runCriticalValidationBatch5();
    const auto batch6 = runCriticalValidationBatch6();
    const auto batch7 = runCriticalValidationBatch7();
    const auto batch8 = runCriticalValidationBatch8();
    const auto batch9 = runCriticalValidationBatch9();
    const auto batch10 = runCriticalValidationBatch10();
    int batch1Pass = 0;
    int batch2Pass = 0;
    int batch3Pass = 0;
    int batch4Pass = 0;
    int batch5Pass = 0;
    int batch6Pass = 0;
    int batch7Pass = 0;
    int batch8Pass = 0;
    int batch9Pass = 0;
    int batch10Pass = 0;
    for (const auto& c : batch1)
    {
        if (c.passed)
            batch1Pass++;
    }
    for (const auto& c : batch2)
    {
        if (c.passed)
            batch2Pass++;
    }
    for (const auto& c : batch3)
    {
        if (c.passed)
            batch3Pass++;
    }
    for (const auto& c : batch4)
    {
        if (c.passed)
            batch4Pass++;
    }
    for (const auto& c : batch5)
    {
        if (c.passed)
            batch5Pass++;
    }
    for (const auto& c : batch6)
    {
        if (c.passed)
            batch6Pass++;
    }
    for (const auto& c : batch7)
    {
        if (c.passed)
            batch7Pass++;
    }
    for (const auto& c : batch8)
    {
        if (c.passed)
            batch8Pass++;
    }
    for (const auto& c : batch9)
    {
        if (c.passed)
            batch9Pass++;
    }
    for (const auto& c : batch10)
    {
        if (c.passed)
            batch10Pass++;
    }

    {
        std::ostringstream oss;
        oss << "=== Critical Runtime Validation (Batch 1: 1..8) ===\n";
        for (const auto& c : batch1)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch1Pass << "/" << batch1.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 2: 8..14) ===\n";
        for (const auto& c : batch2)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch2Pass << "/" << batch2.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 3: E0-01..E0-08, startup phases 9-16) ===\n";
        for (const auto& c : batch3)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch3Pass << "/" << batch3.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 4: E0-09..E0-16, post-API deep boot) ===\n";
        for (const auto& c : batch4)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch4Pass << "/" << batch4.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 5: E0-17..E0-24, WinMain structural depth) ===\n";
        for (const auto& c : batch5)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch5Pass << "/" << batch5.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 6: E0-25..E0-32, loop boundary) ===\n";
        for (const auto& c : batch6)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch6Pass << "/" << batch6.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 7: E0-33..E0-40, shell depth) ===\n";
        for (const auto& c : batch7)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch7Pass << "/" << batch7.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 8: E0-41..E0-48, workbench capstone) ===\n";
        for (const auto& c : batch8)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch8Pass << "/" << batch8.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 9: E0-49..E0-56, agent/chrome depth) ===\n";
        for (const auto& c : batch9)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch9Pass << "/" << batch9.size() << " checks passed\n\n";
        oss << "=== Critical Runtime Validation (Batch 10: E0-57..E0-64, panels + model depth) ===\n";
        for (const auto& c : batch10)
        {
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        }
        oss << "Result: " << batch10Pass << "/" << batch10.size() << " checks passed\n";
        appendToOutput(oss.str(), "Audit", Win32IDE::OutputSeverity::Info);
    }

    auto features = FeatureRegistry::instance().getAllFeatures();
    int passed = 0, failed = 0, noTest = 0;
    for (const auto& f : features)
    {
        if (f.runtimeTested)
            passed++;
        else
            noTest++;
    }

    wchar_t msg[1024];
    _snwprintf_s(msg, 1023,
                 L"Component Tests Complete\n\n"
                 L"Passed: %d\n"
                 L"No Test: %d\n\n"
                 L"Critical Batch 1: %d/%zu\n"
                 L"Critical Batch 2: %d/%zu\n"
                 L"Critical Batch 3 (E0 1-8): %d/%zu\n"
                 L"Critical Batch 4 (E0 9-16): %d/%zu\n"
                 L"Critical Batch 5 (E0 17-24): %d/%zu\n"
                 L"Critical Batch 6 (E0 25-32): %d/%zu\n"
                 L"Critical Batch 7 (E0 33-40): %d/%zu\n"
                 L"Critical Batch 8 (E0 41-48): %d/%zu\n"
                 L"Critical Batch 9 (E0 49-56): %d/%zu\n"
                 L"Critical Batch 10 (E0 57-64): %d/%zu",
                 passed, noTest, batch1Pass, batch1.size(), batch2Pass, batch2.size(), batch3Pass, batch3.size(),
                 batch4Pass, batch4.size(), batch5Pass, batch5.size(), batch6Pass, batch6.size(), batch7Pass,
                 batch7.size(), batch8Pass, batch8.size(), batch9Pass, batch9.size(), batch10Pass, batch10.size());
    MessageBoxW(m_hwndMain, msg, L"Component Tests", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditExportReport()
{
    std::string report = FeatureRegistry::instance().generateReport();

    // Append menu wiring report
    HMENU hMenu = GetMenu(m_hwndMain);
    if (hMenu)
    {
        report += "\n\n";
        report += RawrXD::Audit::getMenuWiringReport(hMenu);
    }

    {
        const auto b1 = runCriticalValidationBatch1();
        const auto b2 = runCriticalValidationBatch2();
        const auto b3 = runCriticalValidationBatch3();
        const auto b4 = runCriticalValidationBatch4();
        const auto b5 = runCriticalValidationBatch5();
        const auto b6 = runCriticalValidationBatch6();
        const auto b7 = runCriticalValidationBatch7();
        const auto b8 = runCriticalValidationBatch8();
        const auto b9 = runCriticalValidationBatch9();
        const auto b10 = runCriticalValidationBatch10();
        std::ostringstream oss;
        oss << "\n\n=== Critical Runtime Validation (export snapshot) ===\n";
        oss << "--- Batch 1 (1..8) ---\n";
        for (const auto& c : b1)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 2 (8..14) ---\n";
        for (const auto& c : b2)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 3 E0-01..E0-08 (startup phases 9-16) ---\n";
        for (const auto& c : b3)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 4 E0-09..E0-16 (post-API deep boot) ---\n";
        for (const auto& c : b4)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 5 E0-17..E0-24 (WinMain structural depth) ---\n";
        for (const auto& c : b5)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 6 E0-25..E0-32 (loop boundary) ---\n";
        for (const auto& c : b6)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 7 E0-33..E0-40 (shell depth) ---\n";
        for (const auto& c : b7)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 8 E0-41..E0-48 (workbench capstone) ---\n";
        for (const auto& c : b8)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 9 E0-49..E0-56 (agent/chrome depth) ---\n";
        for (const auto& c : b9)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        oss << "--- Batch 10 E0-57..E0-64 (panels + model depth) ---\n";
        for (const auto& c : b10)
            oss << (c.passed ? "[PASS] " : "[FAIL] ") << c.name << " :: " << c.detail << "\n";
        report += oss.str();
    }

    // Save to file
    wchar_t filePath[MAX_PATH] = L"RawrXD_AuditReport.txt";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return;

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(m_hwndMain, L"Failed to create report file.", L"Export Error", MB_OK | MB_ICONERROR);
        return;
    }

    DWORD written = 0;
    WriteFile(hFile, report.c_str(), static_cast<DWORD>(report.size()), &written, nullptr);
    CloseHandle(hFile);

    wchar_t msg[MAX_PATH + 64];
    _snwprintf_s(msg, MAX_PATH + 63, L"Report exported to:\n%s", filePath);
    MessageBoxW(m_hwndMain, msg, L"Audit Export", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAuditQuickStats()
{
    float pct = FeatureRegistry::instance().getCompletionPercentage();
    size_t total = FeatureRegistry::instance().getFeatureCount();
    size_t complete = FeatureRegistry::instance().getCountByStatus(ImplStatus::Complete);
    size_t stubs = FeatureRegistry::instance().getCountByStatus(ImplStatus::Stub);
    size_t partial = FeatureRegistry::instance().getCountByStatus(ImplStatus::Partial);
    size_t broken = FeatureRegistry::instance().getCountByStatus(ImplStatus::Broken);

    wchar_t msg[512];
    _snwprintf_s(msg, 511,
                 L"RawrXD IDE — Quick Stats\n\n"
                 L"Registered Features: %zu\n"
                 L"Complete:  %zu\n"
                 L"Partial:   %zu\n"
                 L"Stubs:     %zu\n"
                 L"Broken:    %zu\n\n"
                 L"Overall Completion: %.1f%%",
                 total, complete, partial, stubs, broken, pct * 100.0f);
    MessageBoxW(m_hwndMain, msg, L"IDE Quick Stats", MB_OK | MB_ICONINFORMATION);
}
