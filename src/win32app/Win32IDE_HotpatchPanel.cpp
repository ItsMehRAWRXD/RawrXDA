// Win32IDE_HotpatchPanel.cpp — Hotpatch UI Integration (Phase 14.2)
// Wires the three-layer hotpatch system into the Win32IDE command palette
// and menu bar. Handles all IDM_HOTPATCH_* commands (9001–9030).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "../agentic/agent_operations.h"
#include "../agentic_agent_coordinator.h"
#include "../core/byte_level_hotpatcher.hpp"
#include "../core/address_hotpatcher.hpp"
#include "../core/proxy_hotpatcher.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../swarm_orchestrator.h"
#include "Win32IDE.h"
#include "Win32IDE_AgentOperationsBridge.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <commdlg.h>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>


using json = nlohmann::json;

namespace
{

std::string ReadClipboardText(HWND owner)
{
    std::string out;
    if (!OpenClipboard(owner))
    {
        return out;
    }

    HANDLE h = GetClipboardData(CF_TEXT);
    if (h)
    {
        const char* p = static_cast<const char*>(GlobalLock(h));
        if (p)
        {
            out = p;
            GlobalUnlock(h);
        }
    }

    CloseClipboard();
    return out;
}

bool ParseHexBytes(const std::string& input, std::vector<uint8_t>& out)
{
    std::string filtered;
    filtered.reserve(input.size());
    for (char c : input)
    {
        if (std::isxdigit(static_cast<unsigned char>(c)))
        {
            filtered.push_back(c);
        }
    }

    if (filtered.empty() || (filtered.size() % 2) != 0)
    {
        return false;
    }

    out.clear();
    out.reserve(filtered.size() / 2);
    for (size_t i = 0; i + 1 < filtered.size(); i += 2)
    {
        const std::string byteStr = filtered.substr(i, 2);
        out.push_back(static_cast<uint8_t>(std::strtoul(byteStr.c_str(), nullptr, 16)));
    }
    return !out.empty();
}

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle)
{
    if (needle.empty() || haystack.size() < needle.size())
    {
        return false;
    }

    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
    {
        size_t matched = 0;
        while (matched < needle.size())
        {
            const unsigned char a = static_cast<unsigned char>(haystack[i + matched]);
            const unsigned char b = static_cast<unsigned char>(needle[matched]);
            if (std::tolower(a) != std::tolower(b))
            {
                break;
            }
            ++matched;
        }
        if (matched == needle.size())
        {
            return true;
        }
    }
    return false;
}

bool isActionableHotpatchFailure(const AddressPatchDiagnostics& diag, std::string_view dump)
{
    if (diag.failed > 0)
    {
        return true;
    }

    return containsCaseInsensitive(dump, "ProtectFail") ||
           containsCaseInsensitive(dump, "InvalidAddress") ||
           containsCaseInsensitive(dump, "already_active") ||
           containsCaseInsensitive(dump, "already active") ||
           containsCaseInsensitive(dump, "protect failed") ||
           containsCaseInsensitive(dump, "dump error");
}

struct AdvisoryEscalationSnapshot
{
    bool shouldTrigger = false;
    int failuresInWindow = 0;
    int consecutiveFailureSignals = 0;
};

AdvisoryEscalationSnapshot updateAdvisoryEscalationMonitor(bool failureSignal, std::uint64_t failedCounter)
{
    using Clock = std::chrono::steady_clock;
    static constexpr auto kWindow = std::chrono::seconds(60);
    static std::deque<Clock::time_point> s_failureEvents;
    static int s_consecutiveFailureSignals = 0;
    static std::uint64_t s_lastFailedCounter = 0;

    const auto now = Clock::now();

    if (failureSignal)
    {
        ++s_consecutiveFailureSignals;
    }
    else
    {
        s_consecutiveFailureSignals = 0;
    }

    if (failedCounter > s_lastFailedCounter)
    {
        std::uint64_t delta = failedCounter - s_lastFailedCounter;
        if (delta > 32)
        {
            delta = 32;
        }
        for (std::uint64_t i = 0; i < delta; ++i)
        {
            s_failureEvents.push_back(now);
        }
    }
    s_lastFailedCounter = failedCounter;

    while (!s_failureEvents.empty() && (now - s_failureEvents.front()) > kWindow)
    {
        s_failureEvents.pop_front();
    }

    AdvisoryEscalationSnapshot snapshot;
    snapshot.failuresInWindow = static_cast<int>(s_failureEvents.size());
    snapshot.consecutiveFailureSignals = s_consecutiveFailureSignals;
    snapshot.shouldTrigger = (snapshot.failuresInWindow >= 3) && (snapshot.consecutiveFailureSignals >= 3);
    return snapshot;
}

std::string buildAdvisoryAutoFixPayload(const AddressPatchDiagnostics& diag,
                                        std::string_view dump,
                                        const AdvisoryEscalationSnapshot& snapshot)
{
    const bool protectFail = containsCaseInsensitive(dump, "ProtectFail") || containsCaseInsensitive(dump, "protect failed");
    const bool alreadyActive = containsCaseInsensitive(dump, "already_active") || containsCaseInsensitive(dump, "already active");
    const bool invalidAddress = containsCaseInsensitive(dump, "InvalidAddress");

    std::string reason = "Repeated hotpatch failures detected in 60s window";
    if (protectFail)
    {
        reason = "VirtualProtect collision detected";
    }
    else if (alreadyActive)
    {
        reason = "Patch slot collision (already active) detected";
    }
    else if (invalidAddress)
    {
        reason = "Invalid patch target address detected";
    }

    const std::string primaryAction = (protectFail || alreadyActive) ? "REVERT_ALL" : "RETRY_COOLDOWN";

    json payload;
    payload["type"] = "SYSTEM_ADVISORY";
    payload["schema"] = "rawrxd.hotpatch.advisory.v1";
    payload["severity"] = "high";
    payload["priority"] = "system";
    payload["source"] = "Win32IDE.HotpatchDiagnostics";
    payload["window_seconds"] = 60;
    payload["failures_in_window"] = snapshot.failuresInWindow;
    payload["consecutive_failure_signals"] = snapshot.consecutiveFailureSignals;
    payload["diagnostics"] = {
        {"failed", diag.failed},
        {"active", diag.active},
        {"applied", diag.applied},
        {"reverted", diag.reverted}
    };
    payload["reason"] = reason;
    payload["operator_confirmation_required"] = true;
    payload["action_candidates"] = json::array({
        {
            {"action", primaryAction},
            {"verb", (primaryAction == "REVERT_ALL") ? "rawrxd_addr_patch_revert_all" : "retry_after_cooldown"},
            {"confidence", (primaryAction == "REVERT_ALL") ? 0.88 : 0.72},
            {"cooldown_ms", (primaryAction == "RETRY_COOLDOWN") ? 1500 : 0}
        },
        {
            {"action", "RETRY_COOLDOWN"},
            {"verb", "retry_after_cooldown"},
            {"confidence", 0.61},
            {"cooldown_ms", 1500}
        }
    });

    // Keep advisory payload bounded so it does not flood Librarian context.
    std::string excerpt(dump.substr(0, 768));
    payload["dump_excerpt"] = excerpt;

    return payload.dump();
}

std::deque<std::string> g_serverPatchNames;
std::deque<ServerHotpatch> g_serverPatches;
std::string g_lastServerPatchName;

struct TrackedMemoryPatch
{
    MemoryPatchEntry entry{};
    std::vector<uint8_t> bytes;
};

std::deque<TrackedMemoryPatch> g_trackedMemoryPatches;

std::deque<std::string> g_proxyRewriteNames;
std::deque<std::string> g_proxyRewritePatterns;
std::deque<std::string> g_proxyRewriteReplacements;
std::string g_lastProxyRewriteName;

std::deque<std::string> g_proxyTerminationNames;
std::deque<std::string> g_proxyTerminationStops;
std::string g_lastProxyTerminationName;

std::deque<std::string> g_proxyValidatorNames;
std::array<std::string, 3> g_knownValidators = {"length_check", "json_syntax", "safety_filter"};

std::vector<std::string> SplitByPipe(const std::string& input)
{
    std::vector<std::string> out;
    std::string current;
    for (char c : input)
    {
        if (c == '|')
        {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    out.push_back(current);
    return out;
}

std::string TrimAscii(std::string value)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.front())))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && isSpace(static_cast<unsigned char>(value.back())))
    {
        value.pop_back();
    }
    return value;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool ValidatorLengthCheck(const char* output, size_t outputLen, void*)
{
    (void)output;
    return outputLen <= 16384;
}

bool ValidatorJsonSyntax(const char* output, size_t outputLen, void*)
{
    if (!output || outputLen == 0)
        return true;
    int braceDepth = 0;
    int bracketDepth = 0;
    bool inString = false;
    bool escaped = false;

    for (size_t i = 0; i < outputLen; ++i)
    {
        const char ch = output[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }
        if (ch == '{')
            ++braceDepth;
        if (ch == '}')
            --braceDepth;
        if (ch == '[')
            ++bracketDepth;
        if (ch == ']')
            --bracketDepth;
        if (braceDepth < 0 || bracketDepth < 0)
        {
            return false;
        }
    }

    return !inString && braceDepth == 0 && bracketDepth == 0;
}

bool ValidatorSafetyFilter(const char* output, size_t outputLen, void*)
{
    if (!output || outputLen == 0)
        return true;
    std::string text(output, outputLen);
    const std::string lowered = ToLowerAscii(text);
    return lowered.find("malware") == std::string::npos && lowered.find("ransomware") == std::string::npos &&
           lowered.find("exploit") == std::string::npos;
}

ProxyValidatorFn ResolveBuiltInValidator(const std::string& name)
{
    if (name == "length_check")
        return &ValidatorLengthCheck;
    if (name == "json_syntax")
        return &ValidatorJsonSyntax;
    if (name == "safety_filter")
        return &ValidatorSafetyFilter;
    return nullptr;
}

double ComputeTemperatureLinkedTargetTps(float temperature)
{
    const float clampedTemp = std::clamp(temperature, 0.0f, 2.0f);
    return 8.0 + (static_cast<double>(clampedTemp) * 96.0);
}

}  // namespace

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initHotpatchUI()
{
    if (m_hotpatchUIInitialized)
        return;
    m_hotpatchEnabled = true;
    m_hotpatchSavedTargetTps = ComputeTemperatureLinkedTargetTps(m_inferenceConfig.temperature);
    UnifiedHotpatchManager::instance().set_target_tps(m_hotpatchSavedTargetTps);
    ProxyHotpatcher::instance().setEnabled(true);
    m_hotpatchUIInitialized = true;
    if (m_hwndMain)
    {
        SetTimer(m_hwndMain, HOTPATCH_DIAGNOSTICS_TIMER_ID, HOTPATCH_DIAGNOSTICS_INTERVAL_MS, nullptr);
    }
    refreshHotpatchDiagnosticsView();
    appendToOutput("[Hotpatch] Three-layer hotpatch system initialized.\n");
}

void Win32IDE::refreshHotpatchDiagnosticsView()
{
    if (!m_hotpatchUIInitialized)
    {
        return;
    }

    auto& hotpatcher = AddressHotpatcher::instance();
    const AddressPatchDiagnostics diag = hotpatcher.getDiagnostics();
    const std::string dump = hotpatcher.debugDump();
    const bool failureSignal = isActionableHotpatchFailure(diag, dump);
    const AdvisoryEscalationSnapshot escalation =
        updateAdvisoryEscalationMonitor(failureSignal, static_cast<std::uint64_t>(diag.failed));

    // Phase 6: Feed actionable patch failures to Librarian as a one-shot diagnostics hint.
    if (failureSignal)
    {
        std::ostringstream hint;
        hint << "Hotpatch failure diagnostics\n";
        hint << "failed=" << diag.failed << " active=" << diag.active << "\n";
        hint << dump.substr(0, 1536);
        RawrXD::SwarmOrchestrator::PublishHotpatchDiagnosticsHint(hint.str());

        // Phase 6.1: Escalate repeated failures into a structured advisory payload.
        if (escalation.shouldTrigger)
        {
            static std::uint64_t s_lastAdvisoryFailedCounter = 0;
            if (static_cast<std::uint64_t>(diag.failed) > s_lastAdvisoryFailedCounter)
            {
                s_lastAdvisoryFailedCounter = static_cast<std::uint64_t>(diag.failed);
                RawrXD::SwarmOrchestrator::PublishHotpatchDiagnosticsHint(
                    buildAdvisoryAutoFixPayload(diag, dump, escalation));
            }
        }
    }

    std::ostringstream out;
    out << "=== Address Hotpatch Diagnostics ===\n";
    out << "Applied=" << diag.applied << " Reverted=" << diag.reverted << " Failed=" << diag.failed
        << " Active=" << diag.active << "\n";
    out << "------------------------------------\n";
    out << dump;
    if (dump.empty() || dump.back() != '\n')
    {
        out << "\n";
    }

    const std::string rendered = out.str();
    if (rendered == m_lastHotpatchDiagnosticsDump)
    {
        return;
    }
    m_lastHotpatchDiagnosticsDump = rendered;

    clearOutput("Hotpatch Diagnostics");
    appendToOutput(rendered, "Hotpatch Diagnostics", OutputSeverity::Info);
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleHotpatchCommand(int commandId)
{
    // Lazy-init the hotpatch subsystem on first command
    if (!m_hotpatchUIInitialized)
    {
        initHotpatchUI();
    }

    // Temperature-driven hotpatch intensity:
    // colder model => gentler intervention, hotter model => more aggressive patch throughput.
    if (m_hotpatchEnabled)
    {
        const double targetTps = ComputeTemperatureLinkedTargetTps(m_inferenceConfig.temperature);
        UnifiedHotpatchManager::instance().set_target_tps(targetTps);
        m_hotpatchSavedTargetTps = targetTps;

        static double s_lastAnnouncedTps = -1.0;
        if (std::fabs(targetTps - s_lastAnnouncedTps) > 0.1)
        {
            s_lastAnnouncedTps = targetTps;
            std::ostringstream tune;
            tune << "[Hotpatch] Temp-linked tuning active: temp="
                 << std::clamp(m_inferenceConfig.temperature, 0.0f, 2.0f) << " => target_tps=" << targetTps << "\n";
            appendToOutput(tune.str());
        }
    }

    switch (commandId)
    {
        case IDM_HOTPATCH_SHOW_STATUS:
            cmdHotpatchShowStatus();
            break;
        case IDM_HOTPATCH_TOGGLE_ALL:
            cmdHotpatchToggleAll();
            break;
        case IDM_HOTPATCH_SHOW_EVENT_LOG:
            cmdHotpatchShowEventLog();
            break;
        case IDM_HOTPATCH_RESET_STATS:
            cmdHotpatchResetStats();
            break;
        case IDM_HOTPATCH_MEMORY_APPLY:
            cmdHotpatchMemoryApply();
            break;
        case IDM_HOTPATCH_MEMORY_REVERT:
            cmdHotpatchMemoryRevert();
            break;
        case IDM_HOTPATCH_BYTE_APPLY:
            cmdHotpatchByteApply();
            break;
        case IDM_HOTPATCH_BYTE_SEARCH:
            cmdHotpatchByteSearch();
            break;
        case IDM_HOTPATCH_SERVER_ADD:
            cmdHotpatchServerAdd();
            break;
        case IDM_HOTPATCH_SERVER_REMOVE:
            cmdHotpatchServerRemove();
            break;
        case IDM_HOTPATCH_PROXY_BIAS:
            cmdHotpatchProxyBias();
            break;
        case IDM_HOTPATCH_PROXY_REWRITE:
            cmdHotpatchProxyRewrite();
            break;
        case IDM_HOTPATCH_PROXY_TERMINATE:
            cmdHotpatchProxyTerminate();
            break;
        case IDM_HOTPATCH_PROXY_VALIDATE:
            cmdHotpatchProxyValidate();
            break;
        case IDM_HOTPATCH_SHOW_PROXY_STATS:
            cmdHotpatchShowProxyStats();
            break;
        case IDM_HOTPATCH_SET_TARGET_TPS:
            cmdHotpatchSetTargetTps();
            break;
        case IDM_HOTPATCH_REVERT_ALL_CONFIRMED:
            cmdHotpatchConfirmRevertAll();
            break;
        case IDM_HOTPATCH_PRESET_SAVE:
            cmdHotpatchPresetSave();
            break;
        case IDM_HOTPATCH_PRESET_LOAD:
            cmdHotpatchPresetLoad();
            break;

        // Agent Operations
        case IDM_HOTPATCH_COMPACT_CONVERSATION:
            cmdHotpatchCompactConversation();
            break;
        case IDM_HOTPATCH_OPTIMIZE_TOOL_SELECTION:
            cmdHotpatchOptimizeToolSelection();
            break;
        case IDM_HOTPATCH_RESOLVING:
            cmdHotpatchResolving();
            break;
        case IDM_HOTPATCH_READ_LINES:
            cmdHotpatchReadLines();
            break;
        case IDM_HOTPATCH_PLANNING_EXPLORATION:
            cmdHotpatchPlanningExploration();
            break;
        case IDM_HOTPATCH_SEARCH_FILES:
            cmdHotpatchSearchFiles();
            break;
        case IDM_HOTPATCH_EVALUATE_INTEGRATION:
            cmdHotpatchEvaluateIntegration();
            break;
        case IDM_HOTPATCH_RESTORE_CHECKPOINT:
            cmdHotpatchRestoreCheckpoint();
            break;

        default:
            appendToOutput("[Hotpatch] Unknown hotpatch command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Status & Toggle
// ============================================================================

void Win32IDE::cmdHotpatchShowStatus()
{
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();

    auto& proxy = ProxyHotpatcher::instance();
    const auto& pstats = proxy.getStats();

    auto& memStats = get_memory_patch_stats();

    std::ostringstream ss;
    ss << "=== RawrXD Hotpatch System Status ===\n";
    ss << "  System Enabled:    " << (m_hotpatchEnabled ? "YES" : "NO") << "\n";
    double targetTps = mgr.get_target_tps();
    ss << "  Target TPS:        "
       << (targetTps > 0.0 ? std::to_string(targetTps) + " (force hotpatching)" : "off (run normally)") << "\n";
    ss << "\n--- Unified Manager ---\n";
    ss << "  Memory Patches:    " << stats.memoryPatchCount.load() << "\n";
    ss << "  Byte Patches:      " << stats.bytePatchCount.load() << "\n";
    ss << "  Server Patches:    " << stats.serverPatchCount.load() << "\n";
    ss << "  Total Operations:  " << stats.totalOperations.load() << "\n";
    ss << "  Total Failures:    " << stats.totalFailures.load() << "\n";
    ss << "\n--- Memory Layer (Layer 1) ---\n";
    ss << "  Applied:           " << memStats.totalApplied.load() << "\n";
    ss << "  Reverted:          " << memStats.totalReverted.load() << "\n";
    ss << "  Failed:            " << memStats.totalFailed.load() << "\n";
    ss << "  Protect Changes:   " << memStats.protectionChanges.load() << "\n";
    ss << "\n--- Proxy Hotpatcher ---\n";
    ss << "  Tokens Processed:  " << pstats.tokensProcessed.load() << "\n";
    ss << "  Biases Applied:    " << pstats.biasesApplied.load() << "\n";
    ss << "  Streams Terminated:" << pstats.streamsTerminated.load() << "\n";
    ss << "  Rewrites Applied:  " << pstats.rewritesApplied.load() << "\n";
    ss << "  Valid. Passed:     " << pstats.validationsPassed.load() << "\n";
    ss << "  Valid. Failed:     " << pstats.validationsFailed.load() << "\n";
    ss << "=====================================\n";

    appendToOutput(ss.str());
    MessageBoxA(m_hwndMain, ss.str().c_str(), "Hotpatch System Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdHotpatchSetTargetTps()
{
    auto& mgr = UnifiedHotpatchManager::instance();
    char buf[64] = {};
    if (OpenClipboard(m_hwndMain))
    {
        HANDLE h = GetClipboardData(CF_TEXT);
        if (h)
        {
            const char* p = static_cast<const char*>(GlobalLock(h));
            if (p)
            {
                strncpy(buf, p, sizeof(buf) - 1);
                GlobalUnlock(h);
            }
        }
        CloseClipboard();
    }
    double value = 0.0;
    if (buf[0])
    {
        try
        {
            value = std::stod(buf);
        }
        catch (...)
        {
        }
        if (value < 0.0)
            value = 0.0;
    }
    mgr.set_target_tps(value);
    std::ostringstream ss;
    if (value > 0.0)
    {
        ss << "[Hotpatch] Target TPS set to " << value
           << " (force hotpatching). Clear clipboard and run again to set 0 (run normally).\n";
    }
    else
    {
        ss << "[Hotpatch] Target TPS cleared — run normally. To set: copy a number (e.g. 50) to clipboard and run "
              "Hotpatch > Set target TPS again.\n";
    }
    appendToOutput(ss.str());
    MessageBoxA(m_hwndMain, ss.str().c_str(), "Target TPS", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdHotpatchToggleAll()
{
    auto& unified = UnifiedHotpatchManager::instance();
    auto& proxy = ProxyHotpatcher::instance();

    m_hotpatchEnabled = !m_hotpatchEnabled;
    if (m_hotpatchEnabled)
    {
        const double restoredTps = (m_hotpatchSavedTargetTps > 0.0)
                                       ? m_hotpatchSavedTargetTps
                                       : ComputeTemperatureLinkedTargetTps(m_inferenceConfig.temperature);
        unified.set_target_tps(restoredTps);
        proxy.setEnabled(true);
        if (m_hwndMain)
        {
            SetTimer(m_hwndMain, HOTPATCH_DIAGNOSTICS_TIMER_ID, HOTPATCH_DIAGNOSTICS_INTERVAL_MS, nullptr);
        }
        refreshHotpatchDiagnosticsView();
    }
    else
    {
        m_hotpatchSavedTargetTps = unified.get_target_tps();
        unified.set_target_tps(0.0);
        proxy.setEnabled(false);
        if (m_hwndMain)
        {
            KillTimer(m_hwndMain, HOTPATCH_DIAGNOSTICS_TIMER_ID);
        }
    }

    std::ostringstream ss;
    ss << "[Hotpatch] System " << (m_hotpatchEnabled ? "ENABLED" : "DISABLED")
       << ": target_tps=" << unified.get_target_tps() << " proxy=" << (proxy.isEnabled() ? "on" : "off") << "\n";
    const std::string msg = ss.str();
    appendToOutput(msg);

    // Update status bar
    if (m_hwndStatusBar)
    {
        std::string sbText = std::string("Hotpatch: ") + (m_hotpatchEnabled ? "ON" : "OFF");
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)sbText.c_str());
    }
}

void Win32IDE::cmdHotpatchResetStats()
{
    UnifiedHotpatchManager::instance().resetStats();
    ProxyHotpatcher::instance().resetStats();
    reset_memory_patch_stats();
    appendToOutput("[Hotpatch] All statistics reset.\n");
}

// ============================================================================
// Event Log
// ============================================================================

void Win32IDE::cmdHotpatchShowEventLog()
{
    auto& mgr = UnifiedHotpatchManager::instance();
    std::ostringstream ss;
    ss << "=== Hotpatch Event Log ===\n";

    HotpatchEvent evt;
    int count = 0;
    while (mgr.poll_event(&evt) && count < 64)
    {
        static const char* typeNames[] = {"MemPatchApplied", "MemPatchReverted", "BytePatchApplied",
                                          "BytePatchFailed", "ServerPatchAdded", "ServerPatchRemoved",
                                          "PresetLoaded",    "PresetSaved"};
        const char* tname = (evt.type < 8) ? typeNames[evt.type] : "Unknown";
        ss << "  [" << evt.sequenceId << "] " << tname << " @ tick " << evt.timestamp;
        if (evt.detail)
            ss << " — " << evt.detail;
        ss << "\n";
        ++count;
    }

    if (count == 0)
    {
        ss << "  (No events in ring buffer)\n";
    }
    ss << "==========================\n";

    appendToOutput(ss.str());
}

// ============================================================================
// Memory Layer (Layer 1)
// ============================================================================

void Win32IDE::cmdHotpatchMemoryApply()
{
    // Input contract: <address_hex> <hex_bytes>
    // Example: 0x7FFE1234 909090
    std::string input = ReadClipboardText(m_hwndMain);
    if (input.empty() && m_hwndCopilotChatInput)
    {
        input = getWindowText(m_hwndCopilotChatInput);
    }
    if (input.empty())
    {
        appendToOutput("[Hotpatch] Memory apply requires input: <address_hex> <hex_bytes> (clipboard or chat input)\n");
        return;
    }

    std::istringstream iss(input);
    std::string addrStr;
    std::string hexData;
    iss >> addrStr >> hexData;
    if (addrStr.empty() || hexData.empty())
    {
        appendToOutput("[Hotpatch] Invalid input format. Expected: <address_hex> <hex_bytes>\n");
        return;
    }

    uintptr_t addr = 0;
    try
    {
        addr = static_cast<uintptr_t>(std::stoull(addrStr, nullptr, 0));
    }
    catch (...)
    {
        appendToOutput("[Hotpatch] Invalid address in clipboard.\n");
        return;
    }

    std::vector<uint8_t> data;
    if (!ParseHexBytes(hexData, data))
    {
        appendToOutput("[Hotpatch] Invalid hex bytes in clipboard.\n");
        return;
    }

    TrackedMemoryPatch tracked{};
    tracked.bytes = data;
    tracked.entry.targetAddr = addr;
    tracked.entry.patchSize = tracked.bytes.size();
    tracked.entry.patchData = tracked.bytes.data();
    tracked.entry.originalSize = 0;
    tracked.entry.applied = false;

    auto ur = UnifiedHotpatchManager::instance().apply_memory_patch_tracked(&tracked.entry);

    std::ostringstream ss;
    ss << "[Hotpatch] Memory patch @0x" << std::hex << addr << std::dec << " size=" << data.size() << " => "
       << (ur.result.success ? "OK" : "FAILED") << " (" << ur.result.detail << ")\n";
    appendToOutput(ss.str());

    if (ur.result.success)
    {
        g_trackedMemoryPatches.push_back(std::move(tracked));
    }
}

void Win32IDE::cmdHotpatchMemoryRevert()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    if (g_trackedMemoryPatches.empty())
    {
        appendToOutput("[Hotpatch] No tracked memory patches to revert.\n");
        return;
    }

    TrackedMemoryPatch tracked = std::move(g_trackedMemoryPatches.back());
    g_trackedMemoryPatches.pop_back();

    auto ur = mgr.revert_memory_patch(&tracked.entry);
    if (!ur.result.success)
    {
        g_trackedMemoryPatches.push_back(std::move(tracked));
    }

    std::ostringstream ss;
    ss << "[Hotpatch] Revert last tracked memory patch @0x" << std::hex << tracked.entry.targetAddr << std::dec
       << " size=" << tracked.entry.patchSize << " => " << (ur.result.success ? "OK" : "FAILED") << " ("
       << ur.result.detail << ")\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchConfirmRevertAll()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    auto& hotpatcher = AddressHotpatcher::instance();
    const PatchResult result = hotpatcher.revertAll();
    refreshHotpatchDiagnosticsView();

    json recovery;
    recovery["type"] = "SYSTEM_RECOVERY";
    recovery["schema"] = "rawrxd.hotpatch.recovery.v1";
    recovery["source"] = "Win32IDE.WM_COMMAND";
    recovery["operator_confirmed"] = true;
    recovery["action"] = "REVERT_ALL";
    recovery["success"] = result.success;
    recovery["detail"] = result.detail;
    recovery["error_code"] = result.errorCode;

    if (result.success)
    {
        appendToOutput("[Hotpatch] Advisory action confirmed: reverted all active address patches.\n");
    }
    else
    {
        appendToOutput("[Hotpatch] Advisory action failed: " + result.detail + "\n", "Output", OutputSeverity::Error);
    }

    RawrXD::SwarmOrchestrator::PublishHotpatchDiagnosticsHint(recovery.dump());
}

// ============================================================================
// Byte Layer (Layer 2)
// ============================================================================

void Win32IDE::cmdHotpatchByteApply()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    // Open file dialog for GGUF selection
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select GGUF File for Byte Patching";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        appendToOutput(std::string("[Hotpatch] Byte-level target: ") + filename + "\n");

        // Open a second dialog for the .hotpatch patch definition file
        char patchFile[MAX_PATH] = {};
        OPENFILENAMEA ofnPatch = {};
        ofnPatch.lStructSize = sizeof(ofnPatch);
        ofnPatch.hwndOwner = m_hwndMain;
        ofnPatch.lpstrFilter = "Hotpatch Files (*.hotpatch)\0*.hotpatch\0All Files (*.*)\0*.*\0";
        ofnPatch.lpstrFile = patchFile;
        ofnPatch.nMaxFile = MAX_PATH;
        ofnPatch.lpstrTitle = "Select Byte Patch Definition";
        ofnPatch.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameA(&ofnPatch))
        {
            // Parse the .hotpatch file: each line is OFFSET:HEX_BYTES
            FILE* fp = fopen(patchFile, "r");
            if (!fp)
            {
                appendToOutput("[Hotpatch] ERROR: Cannot open patch file.\n");
                return;
            }
            auto& mgr = UnifiedHotpatchManager::instance();
            int applied = 0, failed = 0;
            char line[512];
            while (fgets(line, sizeof(line), fp))
            {
                // Skip comments and blank lines
                if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
                    continue;
                // Parse OFFSET:HEX_BYTES
                char* colon = strchr(line, ':');
                if (!colon)
                    continue;
                *colon = '\0';
                uint64_t offset = strtoull(line, nullptr, 16);
                std::vector<uint8_t> bytes;
                char* hex = colon + 1;
                while (*hex)
                {
                    while (*hex == ' ' || *hex == '\t')
                        hex++;
                    if (*hex == '\n' || *hex == '\r' || *hex == '\0')
                        break;
                    char byteStr[3] = {hex[0], hex[1], '\0'};
                    bytes.push_back((uint8_t)strtoul(byteStr, nullptr, 16));
                    hex += 2;
                }
                if (!bytes.empty())
                {
                    BytePatch bp;
                    bp.offset = offset;
                    bp.data = bytes;
                    auto ur = mgr.apply_byte_patch(filename, bp);
                    if (ur.result.success)
                        applied++;
                    else
                        failed++;
                }
            }
            fclose(fp);
            appendToOutput("[Hotpatch] Byte patches applied: " + std::to_string(applied) +
                           ", failed: " + std::to_string(failed) + "\n");
        }
        else
        {
            const std::string clip = ReadClipboardText(m_hwndMain);
            if (clip.empty())
            {
                appendToOutput("[Hotpatch] Patch definition selection canceled. No byte patches applied.\n");
                return;
            }

            auto& mgr = UnifiedHotpatchManager::instance();
            std::istringstream clipStream(clip);
            std::string patchLine;
            int applied = 0;
            int failed = 0;

            while (std::getline(clipStream, patchLine))
            {
                patchLine = TrimAscii(patchLine);
                if (patchLine.empty() || patchLine[0] == '#')
                {
                    continue;
                }

                const size_t colonPos = patchLine.find(':');
                if (colonPos == std::string::npos)
                {
                    ++failed;
                    continue;
                }

                const std::string offText = TrimAscii(patchLine.substr(0, colonPos));
                const std::string hexText = TrimAscii(patchLine.substr(colonPos + 1));
                if (offText.empty() || hexText.empty())
                {
                    ++failed;
                    continue;
                }

                uint64_t offset = 0;
                try
                {
                    offset = std::stoull(offText, nullptr, 16);
                }
                catch (...)
                {
                    ++failed;
                    continue;
                }

                std::vector<uint8_t> bytes;
                if (!ParseHexBytes(hexText, bytes))
                {
                    ++failed;
                    continue;
                }

                BytePatch bp;
                bp.offset = offset;
                bp.data = bytes;
                auto ur = mgr.apply_byte_patch(filename, bp);
                if (ur.result.success)
                {
                    ++applied;
                }
                else
                {
                    ++failed;
                }
            }

            appendToOutput("[Hotpatch] Clipboard byte patches applied: " + std::to_string(applied) +
                           ", failed: " + std::to_string(failed) + "\n");
        }
    }
}

void Win32IDE::cmdHotpatchByteSearch()
{
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select File for Pattern Search & Replace";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        appendToOutput(std::string("[Hotpatch] Search target: ") + filename + "\n");

        // Input contract: <hex_pattern> [hex_replacement]
        std::string input = ReadClipboardText(m_hwndMain);
        if (input.empty() && m_hwndCopilotChatInput)
        {
            input = getWindowText(m_hwndCopilotChatInput);
        }
        if (input.empty())
        {
            appendToOutput(
                "[Hotpatch] Byte search requires input: <hex_pattern> [hex_replacement] (clipboard or chat input)\n");
            return;
        }

        std::istringstream iss(input);
        std::string patternHex;
        std::string replaceHex;
        iss >> patternHex >> replaceHex;

        std::vector<uint8_t> pattern;
        if (!ParseHexBytes(patternHex, pattern))
        {
            appendToOutput("[Hotpatch] Invalid search pattern hex.\n");
            return;
        }

        auto result = direct_search(filename, pattern.data(), pattern.size());
        if (!result.found)
        {
            appendToOutput("[Hotpatch] Pattern not found in target file.\n");
            return;
        }

        std::ostringstream foundMsg;
        foundMsg << "[Hotpatch] Pattern found at offset 0x" << std::hex << result.offset << std::dec
                 << " length=" << result.length << "\n";
        appendToOutput(foundMsg.str());

        if (!replaceHex.empty())
        {
            std::vector<uint8_t> replacement;
            if (!ParseHexBytes(replaceHex, replacement))
            {
                appendToOutput("[Hotpatch] Invalid replacement hex; search completed without replace.\n");
                return;
            }

            auto ur = UnifiedHotpatchManager::instance().apply_byte_search_patch(filename, pattern, replacement);
            appendToOutput(std::string("[Hotpatch] Byte search/replace => ") + (ur.result.success ? "OK" : "FAILED") +
                           " (" + ur.result.detail + ")\n");
        }
    }
}

// ============================================================================
// Server Layer (Layer 3)
// ============================================================================

void Win32IDE::cmdHotpatchServerAdd()
{
    auto& mgr = UnifiedHotpatchManager::instance();

    g_serverPatchNames.emplace_back("ide_menu_patch_" + std::to_string(GetTickCount64()));
    g_serverPatches.emplace_back();

    ServerHotpatch& patch = g_serverPatches.back();
    patch.name = g_serverPatchNames.back().c_str();
    patch.hit_count = 0;
    patch.transform = [](Request* req, Response*) -> bool
    {
        // Keep pass-through semantics while guaranteeing a stable temperature field.
        if (req && req->params.find("temperature") == req->params.end())
        {
            req->params["temperature"] = 0.7f;
        }
        return true;
    };

    auto ur = mgr.add_server_patch(&patch);
    appendToOutput(std::string("[Hotpatch] Server patch add => ") + (ur.result.success ? "OK" : "FAILED") + " (" +
                   ur.result.detail + ")\n");
    if (ur.result.success)
    {
        g_lastServerPatchName = patch.name;
        appendToOutput(std::string("[Hotpatch] Added patch: ") + patch.name + "\n");
    }
}

void Win32IDE::cmdHotpatchServerRemove()
{
    if (g_lastServerPatchName.empty())
    {
        appendToOutput("[Hotpatch] No menu-added server patch to remove.\n");
        return;
    }

    auto ur = UnifiedHotpatchManager::instance().remove_server_patch(g_lastServerPatchName.c_str());
    appendToOutput(std::string("[Hotpatch] Server patch remove => ") + (ur.result.success ? "OK" : "FAILED") + " (" +
                   ur.result.detail + ")\n");
    if (ur.result.success)
    {
        g_lastServerPatchName.clear();
    }
}

// ============================================================================
// Proxy Hotpatcher
// ============================================================================

void Win32IDE::cmdHotpatchProxyBias()
{
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    appendToOutput("[Hotpatch] Token Bias Injection:\n");
    appendToOutput("  Adjusts logit scores before sampling to boost/suppress tokens.\n");
    appendToOutput("  Positive bias = boost token probability\n");
    appendToOutput("  Negative bias = suppress token probability\n\n");
    appendToOutput("  Biases applied so far: " + std::to_string(ps.biasesApplied.load()) + "\n\n");

    // Apply a deterministic temperature-linked bias profile immediately.
    const float clampedTemp = std::clamp(m_inferenceConfig.temperature, 0.0f, 2.0f);
    const float biasValue = -6.0f + (clampedTemp * 6.0f);  // colder => stronger suppression.

    TokenBias bias{};
    bias.tokenId = 0;
    bias.biasValue = biasValue;
    bias.permanent = false;
    proxy.add_token_bias(bias);

    std::ostringstream ss;
    ss << "[Hotpatch] Applied temp-linked proxy bias: token=0 bias=" << biasValue << " temp=" << clampedTemp << "\n"
       << "[Hotpatch] Proxy Stats: tokens=" << ps.tokensProcessed.load() << " biases=" << ps.biasesApplied.load()
       << "\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchProxyRewrite()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    auto& proxy = ProxyHotpatcher::instance();
    proxy.setEnabled(true);
    const auto& ps = proxy.getStats();
    const std::string clipRaw = TrimAscii(ReadClipboardText(m_hwndMain));
    const std::string clipLower = ToLowerAscii(clipRaw);

    if (clipLower == "clear")
    {
        PatchResult clearResult = proxy.clear_rewrite_rules();
        g_proxyRewriteNames.clear();
        g_proxyRewritePatterns.clear();
        g_proxyRewriteReplacements.clear();
        g_lastProxyRewriteName.clear();
        appendToOutput("[Hotpatch] Proxy rewrite clear => " + std::string(clearResult.success ? "OK" : "FAILED") +
                       " (" + clearResult.detail + ")\n");
        return;
    }

    std::string name;
    std::string pattern;
    std::string replacement;

    if (!clipRaw.empty())
    {
        const std::vector<std::string> parts = SplitByPipe(clipRaw);
        if (parts.size() >= 3)
        {
            name = TrimAscii(parts[0]);
            pattern = parts[1];
            replacement = parts[2];
        }
    }

    if (name.empty())
    {
        name = "ide_rewrite_" + std::to_string(GetTickCount64());
    }
    if (pattern.empty())
    {
        pattern = "I cannot";
    }
    if (replacement.empty())
    {
        const float clampedTemp = std::clamp(m_inferenceConfig.temperature, 0.0f, 2.0f);
        replacement = (clampedTemp >= 1.0f) ? "I can" : "I can try";
    }

    g_proxyRewriteNames.push_back(name);
    g_proxyRewritePatterns.push_back(pattern);
    g_proxyRewriteReplacements.push_back(replacement);

    OutputRewriteRule rule{};
    rule.name = g_proxyRewriteNames.back().c_str();
    rule.pattern = g_proxyRewritePatterns.back().c_str();
    rule.replacement = g_proxyRewriteReplacements.back().c_str();
    rule.hitCount = 0;
    rule.enabled = true;

    if (!g_lastProxyRewriteName.empty())
    {
        proxy.remove_rewrite_rule(g_lastProxyRewriteName.c_str());
    }

    PatchResult addResult = proxy.add_rewrite_rule(rule);
    if (addResult.success)
    {
        g_lastProxyRewriteName = name;
    }

    std::ostringstream ss;
    ss << "[Hotpatch] Proxy rewrite => " << (addResult.success ? "OK" : "FAILED") << " (" << addResult.detail << ")\n"
       << "[Hotpatch] rule=" << name << " pattern='" << pattern << "' replacement='" << replacement << "'\n"
       << "[Hotpatch] rewrites_applied=" << ps.rewritesApplied.load() << "\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchProxyTerminate()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& proxy = ProxyHotpatcher::instance();
    proxy.setEnabled(true);
    const auto& ps = proxy.getStats();

    const std::string clipRaw = TrimAscii(ReadClipboardText(m_hwndMain));
    const std::string clipLower = ToLowerAscii(clipRaw);
    if (clipLower == "clear")
    {
        PatchResult clearResult = proxy.clear_termination_rules();
        g_proxyTerminationNames.clear();
        g_proxyTerminationStops.clear();
        g_lastProxyTerminationName.clear();
        appendToOutput("[Hotpatch] Proxy termination clear => " + std::string(clearResult.success ? "OK" : "FAILED") +
                       " (" + clearResult.detail + ")\n");
        return;
    }

    std::string name;
    std::string stopSequence;
    size_t maxTokens = 0;
    if (!clipRaw.empty())
    {
        const std::vector<std::string> parts = SplitByPipe(clipRaw);
        if (parts.size() >= 2)
        {
            name = TrimAscii(parts[0]);
            stopSequence = parts[1];
            if (parts.size() >= 3)
            {
                try
                {
                    maxTokens = static_cast<size_t>(std::stoull(TrimAscii(parts[2])));
                }
                catch (...)
                {
                    maxTokens = 0;
                }
            }
        }
    }

    if (name.empty())
    {
        name = "ide_terminate_" + std::to_string(GetTickCount64());
    }
    if (stopSequence.empty())
    {
        stopSequence = "\n\nUser:";
    }
    if (maxTokens == 0)
    {
        const float clampedTemp = std::clamp(m_inferenceConfig.temperature, 0.0f, 2.0f);
        maxTokens = static_cast<size_t>(64 + clampedTemp * 256.0f);
    }

    g_proxyTerminationNames.push_back(name);
    g_proxyTerminationStops.push_back(stopSequence);

    StreamTerminationRule rule{};
    rule.name = g_proxyTerminationNames.back().c_str();
    rule.stopSequence = g_proxyTerminationStops.back().c_str();
    rule.maxTokens = maxTokens;
    rule.enabled = true;

    if (!g_lastProxyTerminationName.empty())
    {
        proxy.remove_termination_rule(g_lastProxyTerminationName.c_str());
    }

    PatchResult addResult = proxy.add_termination_rule(rule);
    if (addResult.success)
    {
        g_lastProxyTerminationName = name;
    }

    std::ostringstream ss;
    ss << "[Hotpatch] Proxy termination => " << (addResult.success ? "OK" : "FAILED") << " (" << addResult.detail
       << ")\n"
       << "[Hotpatch] rule=" << name << " stop='" << stopSequence << "' max_tokens=" << maxTokens << "\n"
       << "[Hotpatch] streams_terminated=" << ps.streamsTerminated.load() << "\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchProxyValidate()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    auto& proxy = ProxyHotpatcher::instance();
    proxy.setEnabled(true);
    const auto& ps = proxy.getStats();

    const std::string clipRaw = TrimAscii(ReadClipboardText(m_hwndMain));
    const std::string clipLower = ToLowerAscii(clipRaw);

    if (clipLower == "clear")
    {
        PatchResult clearResult = proxy.clear_validators();
        g_proxyValidatorNames.clear();
        appendToOutput("[Hotpatch] Proxy validators clear => " + std::string(clearResult.success ? "OK" : "FAILED") +
                       " (" + clearResult.detail + ")\n");
        return;
    }

    std::string validatorName = "length_check";
    bool enable = true;
    if (!clipRaw.empty())
    {
        const std::vector<std::string> parts = SplitByPipe(clipRaw);
        if (!parts.empty())
        {
            validatorName = ToLowerAscii(TrimAscii(parts[0]));
        }
        if (parts.size() >= 2)
        {
            const std::string mode = ToLowerAscii(TrimAscii(parts[1]));
            enable = !(mode == "off" || mode == "disable" || mode == "remove" || mode == "0");
        }
    }

    ProxyValidatorFn validatorFn = ResolveBuiltInValidator(validatorName);
    if (!validatorFn)
    {
        appendToOutput("[Hotpatch] Unknown validator name. Use: length_check|json_syntax|safety_filter\n");
        return;
    }

    PatchResult result = PatchResult::ok("no-op");
    if (enable)
    {
        g_proxyValidatorNames.push_back(validatorName);
        ProxyValidator v{};
        v.name = g_proxyValidatorNames.back().c_str();
        v.validate = validatorFn;
        v.userData = nullptr;
        v.enabled = true;
        result = proxy.add_validator(v);
    }
    else
    {
        result = proxy.remove_validator(validatorName.c_str());
    }

    std::ostringstream ss;
    ss << "[Hotpatch] Proxy validator " << (enable ? "enable" : "disable") << " => "
       << (result.success ? "OK" : "FAILED") << " (" << result.detail << ")\n"
       << "[Hotpatch] validator=" << validatorName << "\n"
       << "[Hotpatch] validations_passed=" << ps.validationsPassed.load()
       << " validations_failed=" << ps.validationsFailed.load() << "\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchShowProxyStats()
{
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    std::ostringstream ss;
    ss << "=== Proxy Hotpatcher Statistics ===\n";
    ss << "  Tokens Processed:     " << ps.tokensProcessed.load() << "\n";
    ss << "  Biases Applied:       " << ps.biasesApplied.load() << "\n";
    ss << "  Streams Terminated:   " << ps.streamsTerminated.load() << "\n";
    ss << "  Rewrites Applied:     " << ps.rewritesApplied.load() << "\n";
    ss << "  Validations Passed:   " << ps.validationsPassed.load() << "\n";
    ss << "  Validations Failed:   " << ps.validationsFailed.load() << "\n";
    ss << "===================================\n";

    appendToOutput(ss.str());
}

// ============================================================================
// Presets
// ============================================================================

void Win32IDE::cmdHotpatchPresetSave()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Hotpatch Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save Hotpatch Preset";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "json";

    if (GetSaveFileNameA(&ofn))
    {
        HotpatchPreset preset = {};
        strncpy(preset.name, "IDE Preset", sizeof(preset.name) - 1);

        PatchResult r = UnifiedHotpatchManager::instance().save_preset(filename, preset);
        if (r.success)
        {
            appendToOutput(std::string("[Hotpatch] Preset saved: ") + filename + "\n");
        }
        else
        {
            appendToOutput(std::string("[Hotpatch] Save failed: ") + r.detail + "\n");
        }
    }
}

void Win32IDE::cmdHotpatchPresetLoad()
{
    if (!m_hotpatchEnabled)
    {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Hotpatch Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Load Hotpatch Preset";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        HotpatchPreset preset = {};
        PatchResult r = UnifiedHotpatchManager::instance().load_preset(filename, &preset);
        if (r.success)
        {
            appendToOutput(std::string("[Hotpatch] Preset loaded: ") + filename + "\n");
            appendToOutput(std::string("[Hotpatch] Preset name: ") + preset.name + "\n");
        }
        else
        {
            appendToOutput(std::string("[Hotpatch] Load failed: ") + r.detail + "\n");
        }
    }
}

// ============================================================================
// Agent Operations — Advanced IDE operations accessible via hotpatch
// ============================================================================

void Win32IDE::cmdHotpatchCompactConversation()
{
    if (!m_hwndCopilotChatInput)
    {
        appendToOutput("[Hotpatch] Compact Conversation: Chat pane not available.\n");
        return;
    }

    std::string conversationText = getWindowText(m_hwndCopilotChatInput);
    if (conversationText.empty())
    {
        appendToOutput("[Hotpatch] Compact Conversation: No conversation to compact.\n");
        return;
    }

    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteCompactConversation(
        conversationText, 2048);
    appendToOutput(result);

    // Parse compacted text from result and update chat input
    try {
        auto parsed = json::parse(result.substr(result.find('{'), result.rfind('}') - result.find('{') + 1));
        if (parsed.contains("compacted_text") && parsed["compacted_text"].is_string()) {
            SetWindowTextA(m_hwndCopilotChatInput, parsed["compacted_text"].get<std::string>().c_str());
        }
    } catch (...) {
        // Best-effort: backend result displayed regardless
    }
}

void Win32IDE::cmdHotpatchOptimizeToolSelection()
{
    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
    std::vector<std::string> tools = {
        "read_file", "write_file", "replace_in_file", "list_dir",
        "execute_command", "search_code", "get_diagnostics",
        "plan_code_exploration", "resolve_symbol", "search_files"
    };
    std::string intent = "optimize tool selection for current IDE task";
    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteOptimizeToolSelection(intent, tools);
    appendToOutput(result);

    if (m_hwndStatusBar)
    {
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM) "Tool Opt: ON");
    }
}

void Win32IDE::cmdHotpatchResolving()
{
    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
    std::string symbolName = "Win32IDE";

    // Use current file's directory as search path if available
    std::vector<std::string> searchPaths;
    if (!m_currentFile.empty()) {
        auto parent = std::filesystem::path(m_currentFile).parent_path();
        if (!parent.empty()) searchPaths.push_back(parent.string());
    }
    if (searchPaths.empty()) {
        searchPaths.push_back(std::filesystem::current_path().string());
    }

    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteResolveSymbol(symbolName, searchPaths);
    appendToOutput(result);
}

void Win32IDE::cmdHotpatchReadLines()
{
    std::string input = ReadClipboardText(m_hwndMain);
    if (input.empty() && m_hwndCopilotChatInput)
    {
        input = getWindowText(m_hwndCopilotChatInput);
    }

    if (input.empty())
    {
        appendToOutput("[Hotpatch] Read Lines: Provide line range in clipboard (e.g., '100-150' or '42')\n");
        return;
    }

    int startLine = 0, endLine = 0;
    size_t dashPos = input.find('-');

    try
    {
        if (dashPos != std::string::npos)
        {
            startLine = std::stoi(input.substr(0, dashPos));
            endLine = std::stoi(input.substr(dashPos + 1));
        }
        else
        {
            startLine = endLine = std::stoi(input);
        }
    }
    catch (...)
    {
        appendToOutput("[Hotpatch] Read Lines: Invalid line range format\n");
        return;
    }

    if (startLine <= 0 || endLine <= 0 || endLine < startLine)
    {
        appendToOutput("[Hotpatch] Read Lines: Invalid line range\n");
        return;
    }

    // Use AgentOperationsBridge for file-based reads
    if (!m_currentFile.empty())
    {
        RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
        std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteReadFileLines(
            m_currentFile, static_cast<size_t>(startLine), static_cast<size_t>(endLine));
        appendToOutput(result);
        return;
    }

    // Fallback: extract from editor control if no file path
    if (m_hwndEditor)
    {
        int lineCount = (int)SendMessageA(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
        if (startLine > 0 && startLine <= lineCount)
        {
            std::ostringstream ss;
            ss << "[Hotpatch] Read Lines: " << startLine << "-" << endLine << " (editor)\n";
            for (int line = startLine; line <= std::min(endLine, lineCount); ++line)
            {
                char buffer[4096] = {};
                *(WORD*)buffer = sizeof(buffer);
                int len = (int)SendMessageA(m_hwndEditor, EM_GETLINE, line - 1, (LPARAM)buffer);
                if (len > 0)
                {
                    buffer[len] = 0;
                    ss << "  " << line << ": " << buffer << "\n";
                }
            }
            appendToOutput(ss.str());
        }
    }
    else
    {
        appendToOutput("[Hotpatch] Read Lines: No file or editor available\n");
    }
}

void Win32IDE::cmdHotpatchPlanningExploration()
{
    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
    std::string rootPath = m_projectRoot.empty()
        ? std::filesystem::current_path().string()
        : m_projectRoot;
    std::string query = "Map entry points, extract call graphs, identify hotspots";
    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecutePlanCodeExploration(rootPath, query);
    appendToOutput(result);
}

void Win32IDE::cmdHotpatchSearchFiles()
{
    std::string pattern = ReadClipboardText(m_hwndMain);
    if (pattern.empty() && m_hwndCopilotChatInput)
    {
        pattern = getWindowText(m_hwndCopilotChatInput);
    }

    if (pattern.empty())
    {
        appendToOutput(
            "[Hotpatch] Search Files: Provide search pattern in clipboard (e.g., '*.cpp', 'backend/**/inference*')\n");
        return;
    }

    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();
    std::vector<std::string> searchPaths;
    if (!m_projectRoot.empty()) {
        searchPaths.push_back(m_projectRoot);
    } else if (!m_currentFile.empty()) {
        auto parent = std::filesystem::path(m_currentFile).parent_path();
        if (!parent.empty()) searchPaths.push_back(parent.string());
    }
    if (searchPaths.empty()) {
        searchPaths.push_back(std::filesystem::current_path().string());
    }

    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteSearchFiles(pattern, searchPaths);
    appendToOutput(result);
}

void Win32IDE::cmdHotpatchEvaluateIntegration()
{
    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();

    // Run real integration feasibility analysis via agent operations
    json params;
    params["workspace"] = m_projectRoot.empty()
        ? std::filesystem::current_path().string()
        : m_projectRoot;
    auto opResult = RawrXD::Agentic::AgentOperations::executeOperation(
        "evaluate_integration", params.dump());

    std::ostringstream ss;
    ss << "[Agent Operation] Integration Audit Feasibility Analysis\n";
    ss << "═══════════════════════════════════════════════════════\n\n";
    try {
        auto parsed = json::parse(opResult.output);
        ss << parsed.dump(2);
    } catch (...) {
        ss << opResult.output;
    }
    ss << "\n\n";
    ss << "  Bridge Ready: " << (m_agenticBridge != nullptr ? "YES" : "NO") << "\n";
    ss << "  Hotpatch Ready: " << (m_hotpatchUIInitialized ? "YES" : "NO") << "\n";
    ss << "  Local Server: " << (m_localServerRunning.load() ? "RUNNING" : "STOPPED") << "\n";
    ss << "═══════════════════════════════════════════════════════\n";
    appendToOutput(ss.str());
}

void Win32IDE::cmdHotpatchRestoreCheckpoint()
{
    RawrXD::Win32IDE::AgentOperationsBridge::Initialize();

    // Attempt restore via real checkpoint backend
    std::string checkpointId = "latest";
    std::string rootPath = m_projectRoot.empty()
        ? std::filesystem::current_path().string()
        : m_projectRoot;
    std::string result = RawrXD::Win32IDE::AgentOperationsBridge::ExecuteCheckpointManager(
        "restore", checkpointId, rootPath);

    bool success = (result.find("\"success\":true") != std::string::npos ||
                    result.find("Restored") != std::string::npos);

    appendToOutput(result);

    // Also attempt restore via UnifiedHotpatchManager for conversation/workspace state
    if (m_hwndCopilotChatInput) {
        std::string conversation, workspace;
        auto ur = UnifiedHotpatchManager::instance().copilot_restore_checkpoint(
            checkpointId, &conversation, &workspace);
        if (ur.result.success) {
            success = true;
            if (!conversation.empty()) {
                SetWindowTextA(m_hwndCopilotChatInput, conversation.c_str());
            }
            if (!workspace.empty()) {
                m_projectRoot = workspace;
            }
            appendToOutput("[Hotpatch] Checkpoint conversation/workspace restored via UnifiedHotpatchManager\n");
        }
    }

    if (m_hwndStatusBar)
    {
        std::string status = success ? "Checkpoint: Restored" : "Checkpoint: No Data";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)status.c_str());
    }
}
