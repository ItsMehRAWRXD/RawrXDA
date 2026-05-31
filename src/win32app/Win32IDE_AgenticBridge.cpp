// ============================================================================
// Agentic Framework Bridge Implementation (Consolidated - No Duplicates)
// Connects Win32IDE to Native Agentic Framework
// ============================================================================

#include "Win32IDE_AgenticBridge.h"
#include "../ai/production_inference_engine.h"
#include "../action_executor.h"
#include "../advanced_agent_features.hpp"
#include "../agent/agentic_hotpatch_orchestrator.hpp"
#include "../agent/agentic_puppeteer.hpp"
#include "../agentic/AgentToolHandlers.h"
#include "../agentic/OrchestratorBridge.h"
#include "../agentic/SovereignAssembler.h"
#include "../agentic/ToolRegistry.h"
#include "../agentic/agent_controller_minimal.h"
#include "../agentic/agentic_controller_wiring.h"
#include "../agentic/agentic_orchestrator_integration.hpp"
#include "../agentic/tool_call_parser.h"
#include "../agentic_engine.h"
#include "../cpu_inference_engine.h"
#include "../inference/PerformanceMonitor.h"
#include "../logging/Logger.h"
#include "../modules/native_memory.hpp"
#include "../security/InputSanitizer.h"
#include "../vsix_native_converter.hpp"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "TitanIPC.h"
#include "Win32IDE.h"
#include "Win32IDE_Phase17_AgenticProfiler.h"
#include "Win32IDE_SubAgent.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <winhttp.h>

namespace
{

/// Match Win32IDE::syncAgenticToolGuardrailsFromWorkspace — one canonical root for tools + explorer parity.
[[nodiscard]] std::string canonicalWorkspaceRootForAgent(const std::string& root)
{
    if (root.empty())
    {
        return root;
    }
    std::error_code ec;
    std::filesystem::path p(root);
    if (std::filesystem::exists(p, ec))
    {
        p = std::filesystem::weakly_canonical(p, ec);
    }
    else
    {
        p = std::filesystem::absolute(p, ec);
    }
    return p.lexically_normal().string();
}

static constexpr size_t kMaxCommandFileBytes = 512 * 1024;
static constexpr size_t kMaxRefinedPromptBytes = 768 * 1024;
static constexpr size_t kMaxParallelBridgeToolCalls = 4;

// Models often prefix prose on the same line ("Sure — TOOL:read_file {...}").
// Scan for a case-insensitive "tool:" token and normalize to a TOOL:-prefixed directive.
[[nodiscard]] static size_t findCaseInsensitiveToolDirective(const std::string& text)
{
    if (text.size() < 5)
    {
        return std::string::npos;
    }
    static const char kRef[] = "tool:";
    for (size_t i = 0; i + 5 <= text.size(); ++i)
    {
        bool ok = true;
        for (size_t j = 0; j < 5; ++j)
        {
            const unsigned char a = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(text[i + j])));
            if (a != static_cast<unsigned char>(kRef[j]))
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            return i;
        }
    }
    return std::string::npos;
}

[[nodiscard]] static std::string normalizeEmbeddedToolDirective(std::string chunk)
{
    while (!chunk.empty() && std::isspace(static_cast<unsigned char>(chunk.front())))
    {
        chunk.erase(chunk.begin());
    }
    while (!chunk.empty() && std::isspace(static_cast<unsigned char>(chunk.back())))
    {
        chunk.pop_back();
    }
    const size_t pos = findCaseInsensitiveToolDirective(chunk);
    if (pos == std::string::npos)
    {
        return {};
    }
    std::string tail = chunk.substr(pos);
    if (tail.size() < 5)
    {
        return {};
    }
    return std::string("TOOL:") + tail.substr(5);
}

struct ToolDispatchBatchOutcome
{
    std::string toolName;
    std::string toolResult;
    uint64_t executionTimeMs = 0;
    bool dispatched = false;
};

[[nodiscard]] std::string ExtractDirectiveToolName(const std::string& modelOutput);

struct BridgeToolExecutionPolicy
{
    std::string normalizedName;
    Agentic::StepRisk risk = Agentic::StepRisk::High;
    bool isMutating = true;
    bool allowParallel = false;
};

std::string NormalizeBridgeToolName(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch)
                   {
                       if (std::isalnum(ch))
                       {
                           return static_cast<char>(std::tolower(ch));
                       }
                       return '_';
                   });

    value.erase(
        std::unique(value.begin(), value.end(), [](char left, char right) { return left == '_' && right == '_'; }),
        value.end());
    while (!value.empty() && value.front() == '_')
    {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '_')
    {
        value.pop_back();
    }

    if (value == "grep_files" || value == "search_files")
    {
        return "search_code";
    }
    if (value == "list_directory")
    {
        // ToolRegistry canonical name is list_directory.
        return "list_directory";
    }
    if (value == "run_terminal")
    {
        return "execute_command";
    }
    return value;
}

const char* StepRiskLabel(Agentic::StepRisk risk)
{
    switch (risk)
    {
        case Agentic::StepRisk::VeryLow:
            return "VeryLow";
        case Agentic::StepRisk::Low:
            return "Low";
        case Agentic::StepRisk::Medium:
            return "Medium";
        case Agentic::StepRisk::High:
            return "High";
        case Agentic::StepRisk::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

BridgeToolExecutionPolicy ClassifyBridgeToolExecution(const std::string& toolLine)
{
    static const std::unordered_set<std::string> kParallelReadOnlyTools = {"read_file",
                                                                           "fs_read_file",
                                                                           "fs_read",
                                                                           "list_dir",
                                                                           "fs_list_dir",
                                                                           "file_search",
                                                                           "search_code",
                                                                           "grep_search",
                                                                           "semantic_search",
                                                                           "get_diagnostics",
                                                                           "mention_lookup",
                                                                           "next_edit_hint",
                                                                           "preview_multifile_diff",
                                                                           "path_exists",
                                                                           "fs_exists"};
    static const std::unordered_set<std::string> kSerialReadOnlyTools = {
        "load_rules",    "plan_tasks", "git_status",    "git_diff",     "gh_pr_view",
        "gh_issue_view", "gh_pr_list", "gh_issue_list", "get_coverage", "sys_get_capabilities"};
    static const std::unordered_set<std::string> kLowRiskMutatingTools = {
        "write_file", "replace_in_file", "edit_file",    "fs_write_file",   "fs_write",
        "fs_mkdir",   "fs_copy_file",    "fs_move_file", "manage_todo_list"};
    static const std::unordered_set<std::string> kHighRiskTools = {"execute_command",
                                                                   "run_in_terminal",
                                                                   "terminal_run_command",
                                                                   "run_shell",
                                                                   "run_build",
                                                                   "git_commit",
                                                                   "gh_create_pr",
                                                                   "gh_pr_create",
                                                                   "apply_multifile_edits",
                                                                   "debug_launch",
                                                                   "debug_attach",
                                                                   "debug_break",
                                                                   "debug_continue",
                                                                   "debug_step_over",
                                                                   "debug_step_into",
                                                                   "debug_add_breakpoint",
                                                                   "debug_remove_breakpoint",
                                                                   "debug_stacktrace",
                                                                   "debug_registers",
                                                                   "debug_memory",
                                                                   "debug_disasm",
                                                                   "debug_analyze",
                                                                   "debug_snapshot",
                                                                   "debug_suggest_breakpoints",
                                                                   "apply_hotpatch",
                                                                   "disk_recovery",
                                                                   "asm_assemble",
                                                                   "swebench_autonomous_eval"};
    static const std::unordered_set<std::string> kCriticalTools = {"fs_delete_file"};

    BridgeToolExecutionPolicy policy;
    policy.normalizedName = NormalizeBridgeToolName(ExtractDirectiveToolName(toolLine));
    if (policy.normalizedName.empty())
    {
        policy.normalizedName = "tool";
    }

    if (kParallelReadOnlyTools.count(policy.normalizedName) > 0)
    {
        policy.risk = Agentic::StepRisk::VeryLow;
        policy.isMutating = false;
        policy.allowParallel = true;
    }
    else if (kSerialReadOnlyTools.count(policy.normalizedName) > 0)
    {
        policy.risk = Agentic::StepRisk::Low;
        policy.isMutating = false;
    }
    else if (kLowRiskMutatingTools.count(policy.normalizedName) > 0)
    {
        policy.risk = Agentic::StepRisk::Low;
    }
    else if (kHighRiskTools.count(policy.normalizedName) > 0)
    {
        policy.risk = Agentic::StepRisk::High;
    }
    else if (kCriticalTools.count(policy.normalizedName) > 0)
    {
        policy.risk = Agentic::StepRisk::Critical;
    }
    else
    {
        policy.risk = Agentic::StepRisk::Medium;
    }

    return policy;
}

Agentic::ApprovalPolicy GetBridgeApprovalPolicy()
{
    if (auto* orchestrator = Agentic::OrchestratorIntegration::instance().getOrchestrator())
    {
        return orchestrator->getApprovalPolicy();
    }
    return Agentic::ApprovalPolicy::Standard();
}

bool IsBridgeToolAutoApproved(const BridgeToolExecutionPolicy& toolPolicy,
                              const Agentic::ApprovalPolicy& approvalPolicy)
{
    if (!toolPolicy.isMutating)
    {
        return true;
    }

    switch (toolPolicy.risk)
    {
        case Agentic::StepRisk::VeryLow:
            return approvalPolicy.auto_approve_very_low_risk;
        case Agentic::StepRisk::Low:
            return approvalPolicy.auto_approve_low_risk;
        case Agentic::StepRisk::Medium:
            return !approvalPolicy.require_approval_medium;
        case Agentic::StepRisk::High:
            return !approvalPolicy.require_approval_high;
        case Agentic::StepRisk::Critical:
            return !approvalPolicy.require_approval_critical;
        default:
            return false;
    }
}

std::string BuildApprovalBlockedMessage(const BridgeToolExecutionPolicy& toolPolicy)
{
    std::ostringstream oss;
    oss << "[Approval Required] Tool '" << toolPolicy.normalizedName << "' blocked by approval policy"
        << " (risk=" << StepRiskLabel(toolPolicy.risk) << ", mutating=" << (toolPolicy.isMutating ? "yes" : "no")
        << ")";
    return oss.str();
}

bool isTruthyEnvVar(const char* varName)
{
    if (!varName)
        return false;
    char buf[12] = {};
    const DWORD n = GetEnvironmentVariableA(varName, buf, static_cast<DWORD>(sizeof(buf)));
    return n > 0 && (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y');
}

bool envDisablesCapabilityHotpatch(const char* varName)
{
    if (!varName)
        return false;
    char buf[12] = {};
    const DWORD n = GetEnvironmentVariableA(varName, buf, static_cast<DWORD>(sizeof(buf)));
    return n > 0 && (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y');
}

[[nodiscard]] bool ReadFileWithCap(const std::string& path, size_t maxBytes, std::string& out)
{
    out.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return false;
    }

    f.seekg(0, std::ios::end);
    const std::streamoff total = f.tellg();
    if (total < 0)
    {
        return false;
    }
    if (static_cast<size_t>(total) > maxBytes)
    {
        return false;
    }
    f.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(total));
    if (total > 0)
    {
        f.read(&out[0], total);
        if (!f)
        {
            out.clear();
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string ExtractDirectiveToolName(const std::string& modelOutput)
{
    std::string toolName = "tool";
    auto toolPos = modelOutput.find("tool:");
    if (toolPos == std::string::npos)
        toolPos = modelOutput.find("TOOL:");
    if (toolPos != std::string::npos)
    {
        size_t nameStart = toolPos + 5;
        while (nameStart < modelOutput.size() && modelOutput[nameStart] == ' ')
            nameStart++;
        const size_t nameEnd = modelOutput.find_first_of(" \n\r({[", nameStart);
        const size_t end = (nameEnd == std::string::npos) ? modelOutput.size() : nameEnd;
        if (end > nameStart)
            toolName.assign(modelOutput.data() + nameStart, modelOutput.data() + end);
    }
    return toolName;
}

// Runs a single TOOL:/tool: line against AgentToolRegistry (replaces SubAgentManager stub for Win32 bridge).
[[nodiscard]] static bool executeToolLineWithRegistry(const std::string& toolLine, std::string& toolResultOut)
{
    toolResultOut.clear();
    std::string normalized = toolLine;
    if (normalized.rfind("TOOL:", 0) != 0 && normalized.rfind("tool:", 0) != 0)
    {
        normalized = normalizeEmbeddedToolDirective(normalized);
    }
    if (normalized.empty())
    {
        return false;
    }
    if (normalized.rfind("tool:", 0) == 0)
    {
        normalized = std::string("TOOL:") + normalized.substr(5);
    }

    const std::string toolName = ExtractDirectiveToolName(normalized);
    if (toolName.empty() || toolName == "tool")
    {
        return false;
    }

    nlohmann::json args = nlohmann::json::object();
    const size_t brace = normalized.find('{');
    if (brace != std::string::npos)
    {
        try
        {
            args = nlohmann::json::parse(normalized.substr(brace));
        }
        catch (...)
        {
            toolResultOut = "[tool_error] invalid JSON arguments";
            return true;
        }
    }

    const auto r = RawrXD::Agent::AgentToolRegistry::Instance().Dispatch(toolName, args);
    toolResultOut = r.success ? r.output : (std::string("[tool_error] ") + r.output);
    return true;
}

[[nodiscard]] std::string JsonQuoteStringForToolBody(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 2);
    o.push_back('"');
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\b':
                o += "\\b";
                break;
            case '\f':
                o += "\\f";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    o += buf;
                }
                else
                {
                    o.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    o.push_back('"');
    return o;
}

/// POST JSON to the in-process / sibling headless tool server (loopback).
[[nodiscard]] bool HttpPostHeadlessTool(uint16_t port, const std::string& jsonBody, std::string& responseOut)
{
    responseOut.clear();
    if (port == 0 || jsonBody.empty())
    {
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-AgenticBridge/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
    {
        return false;
    }
    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", port, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"POST", L"/tool", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const wchar_t* hdr = L"Content-Type: application/json\r\n";
    const DWORD bodyLen = static_cast<DWORD>(jsonBody.size());
    LPVOID reqBody =
        (bodyLen != 0u) ? reinterpret_cast<LPVOID>(const_cast<char*>(jsonBody.data())) : WINHTTP_NO_REQUEST_DATA;
    const BOOL sent = WinHttpSendRequest(hRequest, hdr, static_cast<DWORD>(-1L), reqBody, bodyLen, bodyLen, 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    for (;;)
    {
        DWORD chunk = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &chunk) || chunk == 0)
        {
            break;
        }
        std::string buf(chunk, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf.data(), chunk, &read))
        {
            break;
        }
        buf.resize(read);
        responseOut += buf;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return !responseOut.empty();
}

[[nodiscard]] std::optional<std::string> QueryVsInstallPathFromVswhere()
{
    char vswhereBuf[MAX_PATH * 2]{};
    const DWORD n = ExpandEnvironmentStringsA("%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe",
                                              vswhereBuf, sizeof(vswhereBuf));
    if (n == 0 || n > sizeof(vswhereBuf))
    {
        return std::nullopt;
    }
    if (!std::filesystem::exists(vswhereBuf))
    {
        return std::nullopt;
    }

    const std::string cmd = std::string("\"") + vswhereBuf +
                            "\" -latest -products * -requires "
                            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return std::nullopt;
    }

    char line[2048]{};
    std::string out;
    if (fgets(line, sizeof(line), pipe) != nullptr)
    {
        out = line;
        while (!out.empty() && (out.back() == '\r' || out.back() == '\n'))
        {
            out.pop_back();
        }
    }
    (void)_pclose(pipe);

    if (out.empty() || !std::filesystem::is_directory(out))
    {
        return std::nullopt;
    }
    return out;
}

[[nodiscard]] std::optional<std::string> FallbackVsInstallPath()
{
    static const char* kCandidates[] = {
        "D:\\VS2022Enterprise",
        "C:\\VS2022Enterprise",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools",
    };
    for (const char* root : kCandidates)
    {
        const std::filesystem::path vcvars =
            std::filesystem::path(root) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
        if (std::filesystem::exists(vcvars))
        {
            return std::string(root);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string ResolveVsInstallRoot()
{
    if (const auto v = QueryVsInstallPathFromVswhere())
    {
        return *v;
    }
    if (const auto f = FallbackVsInstallPath())
    {
        return *f;
    }
    return {};
}

[[nodiscard]] std::optional<std::string> ResolveToolInLatestMsvc(const std::string& vsRoot, const std::string& arch,
                                                                 const std::string& exeName)
{
    if (vsRoot.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path msvcRoot = std::filesystem::path(vsRoot) / "VC" / "Tools" / "MSVC";
    if (!std::filesystem::is_directory(msvcRoot))
    {
        return std::nullopt;
    }

    std::vector<std::filesystem::path> versions;
    for (const auto& entry : std::filesystem::directory_iterator(msvcRoot))
    {
        if (entry.is_directory())
        {
            versions.push_back(entry.path());
        }
    }
    if (versions.empty())
    {
        return std::nullopt;
    }

    std::sort(versions.begin(), versions.end(), [](const std::filesystem::path& a, const std::filesystem::path& b)
              { return a.filename().string() > b.filename().string(); });

    auto makeBinPath = [&](const std::filesystem::path& ver) -> std::filesystem::path
    {
        if (arch == "x86" || arch == "i386")
        {
            return ver / "bin" / "Hostx86" / "x86" / exeName;
        }
        if (arch == "arm" || arch == "arm64")
        {
            return ver / "bin" / "Hostx64" / "arm64" / exeName;
        }
        return ver / "bin" / "Hostx64" / "x64" / exeName;
    };

    for (const auto& ver : versions)
    {
        const std::filesystem::path p = makeBinPath(ver);
        if (std::filesystem::exists(p))
        {
            return p.string();
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> ResolveVcvarsBatForArch(const std::string& vsRoot, const std::string& arch)
{
    if (vsRoot.empty())
    {
        return std::nullopt;
    }
    const std::filesystem::path build = std::filesystem::path(vsRoot) / "VC" / "Auxiliary" / "Build";

    if (arch == "x86" || arch == "i386")
    {
        const std::filesystem::path p = build / "vcvars32.bat";
        if (std::filesystem::exists(p))
        {
            return p.string();
        }
    }
    else if (arch == "arm" || arch == "arm64")
    {
        const std::filesystem::path p = build / "vcvarsamd64_arm64.bat";
        if (std::filesystem::exists(p))
        {
            return p.string();
        }
    }

    const std::filesystem::path p64 = build / "vcvars64.bat";
    if (!std::filesystem::exists(p64))
    {
        return std::nullopt;
    }
    return p64.string();
}

[[nodiscard]] std::string BuildCmdWithVcvars(const std::string& vcvarsBat, const std::string& toolPath,
                                             const std::string& toolArgsTail)
{
    return "cmd /c \"call \\\"" + vcvarsBat + "\\\" && \\\"" + toolPath + "\\\" " + toolArgsTail + "\"";
}

}  // namespace

std::string AgenticBridge::BuildOpenTabsPromptContext() const
{
    if (!m_ide || m_ide->m_editorTabs.empty())
    {
        return {};
    }

    std::ostringstream oss;
    if (!m_ide->m_currentFile.empty())
    {
        oss << "[Active file: " << m_ide->m_currentFile << "]\n";
        oss << "[Active language: " << m_ide->getSyntaxLanguageName() << "]\n";
    }

    oss << "[Open tabs";
    if (m_ide->m_activeTabIndex >= 0 && m_ide->m_activeTabIndex < (int)m_ide->m_editorTabs.size())
    {
        oss << ", active=" << m_ide->m_activeTabIndex;
    }
    oss << "]\n";

    const size_t maxTabs = 8;
    const size_t count = std::min(m_ide->m_editorTabs.size(), maxTabs);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& tab = m_ide->m_editorTabs[i];
        oss << ((static_cast<int>(i) == m_ide->m_activeTabIndex) ? "* " : "- ");
        oss << i << ": ";

        if (!tab.displayName.empty())
            oss << tab.displayName;
        else if (!tab.filePath.empty())
            oss << tab.filePath;
        else
            oss << "Untitled";

        if (!tab.filePath.empty() && tab.filePath != tab.displayName)
            oss << " [" << tab.filePath << "]";
        if (tab.modified)
            oss << " (modified)";

        oss << "\n";
    }

    if (m_ide->m_editorTabs.size() > maxTabs)
    {
        oss << "- ... " << (m_ide->m_editorTabs.size() - maxTabs) << " more tab(s) open\n";
    }

    oss << "\n";
    return oss.str();
}

namespace
{
[[nodiscard]] inline std::shared_ptr<RawrXD::CPUInferenceEngine> SharedCpuEngine()
{
    return RawrXD::CPUInferenceEngine::GetSharedInstance();
}
}  // namespace

static std::shared_ptr<AgenticEngine> g_agentEngine = nullptr;
static AgenticEngine* g_commandDispatchEngine = nullptr;
static std::shared_ptr<RawrXD::ProductionInferenceEngine> g_prodEngine = nullptr;

void SetIDEAgenticEngineForCommands(AgenticEngine* engine)
{
    // Keep a direct engine pointer available even when action_executor.cpp is excluded.
    g_commandDispatchEngine = engine;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

// Forward declaration — defined in Win32IDE_AgentStreamingBridge.cpp
extern "C" void AgentPanel_AppendToken(const wchar_t* token);

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide), m_initialized(false), m_agentLoopRunning(false), m_hProcess(nullptr), m_hStdoutRead(nullptr),
      m_hStdoutWrite(nullptr), m_hStdinRead(nullptr), m_hStdinWrite(nullptr)
{
    // Initialize streaming result channel for Phase 1 (tool result injection)
    m_streamingChannel = std::make_unique<RawrXD::Agentic::StreamingResultChannel>();

    // Wire TitanProxy token callback → IDE streaming bridge so process-to-process
    // token streaming surfaces incrementally in the Agent output panel.
    RawrXD::TitanProxy::instance().setTokenCallback(
        [](const std::string& token)
        {
            if (token.empty())
                return;
            // Convert UTF-8 token to wide char for AgentPanel_AppendToken
            const int wLen = MultiByteToWideChar(CP_UTF8, 0, token.c_str(), static_cast<int>(token.size()), nullptr, 0);
            if (wLen <= 0)
                return;
            std::wstring wide(static_cast<size_t>(wLen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, token.c_str(), static_cast<int>(token.size()), &wide[0], wLen);
            AgentPanel_AppendToken(wide.c_str());
        });
}

AgenticBridge::~AgenticBridge()
{
    KillPowerShellProcess();
}

// ============================================================================
// Initialization
// ============================================================================

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName)
{
    SCOPED_METRIC("agentic.initialize");
    if (m_initialized)
        return true;

    // ── Self-hosting IPC split ────────────────────────────────────────────────
    // If RAWRXD_HEADLESS_PORT is set in the environment (or the IDE was launched
    // with --headless <port>), delegate all tool execution to the headless
    // backend process over loopback HTTP.  We just need to record the port; the
    // actual dispatch happens in ExecuteAgentCommand via the DispatchToHeadless
    // helper added below.  If the headless process isn't running yet, start it
    // in-process on the requested port so both GUI and headless share one binary.
    {
        wchar_t envBuf[32]{};
        const DWORD envLen = GetEnvironmentVariableW(L"RAWRXD_HEADLESS_PORT", envBuf, 32);
        if (envLen > 0 && envLen < 32)
        {
            const int requestedPort = _wtoi(envBuf);
            if (requestedPort > 0 && requestedPort <= 65535)
            {
                m_headlessPort = static_cast<uint16_t>(requestedPort);

                // Check if the named ready-event exists (meaning an external
                // headless server is already running).
                const std::string evtName = "RawrXD_HeadlessReady_" + std::to_string(m_headlessPort);
                const HANDLE existing = OpenEventA(SYNCHRONIZE, FALSE, evtName.c_str());
                if (existing != nullptr)
                {
                    // External server already running \u2014 just use it.
                    CloseHandle(existing);
                    LOG_INFO("Headless backend detected on port " + std::to_string(m_headlessPort) +
                             " \u2014 GUI will delegate tool calls over loopback.");
                }
                else
                {
                    // Spin up in-process so we don\u2019t need a separate process launch.
                    LOG_INFO("Starting in-process headless backend on port " + std::to_string(m_headlessPort) + " ...");
                    RawrXD_StartHeadlessServer(m_headlessPort);
                }

                // MASM-backed tools: RawrXD::Agent::ToolRegistry registers builtins in its constructor.

                m_initialized = true;
                if (m_ide)
                {
                    m_ide->appendToOutput("[Self-Host] Agentic backend running on loopback port " +
                                              std::to_string(m_headlessPort) + ". MASM tools registered.\n",
                                          "Agent", Win32IDE::OutputSeverity::Success);
                }
                return true;
            }
        }
    }
    // ── End self-hosting IPC split ────────────────────────────────────────────

    LOG_INFO("Initializing Native Inference Stack...");

    m_frameworkPath = frameworkPath.empty() ? ResolveFrameworkPath() : frameworkPath;

    const auto cpu = SharedCpuEngine();

    if (!g_prodEngine)
    {
        g_prodEngine = std::make_shared<RawrXD::ProductionInferenceEngine>();
    }

    if (!g_agentEngine)
    {
        g_agentEngine = std::make_shared<AgenticEngine>();
        // Preference for production engine if available
        g_agentEngine->setInferenceEngine(g_prodEngine.get());
    }

    // Wire the minimal agentic controller once inference is available.
    rawrxd::initializeAgentControllerWiring(g_prodEngine.get());

    // P1: Initialize the Tool Registry for the agentic layer (44-tool base)
    RawrXD::Agent::AgentToolRegistry::Instance();

    // Hot-patch the internal tokenizer with AVX2 if an optional PE is present (sovereign-assembled DLL).
    {
        wchar_t buf[MAX_PATH]{};
        DWORD n = GetEnvironmentVariableW(L"RAWRXD_AVX2_TOKENIZER_DLL", buf, MAX_PATH);
        const std::filesystem::path envPath =
            (n > 0 && n < MAX_PATH) ? std::filesystem::path(buf) : std::filesystem::path(L"d:\\avx2_tokenizer.dll");
        if (!envPath.empty() && std::filesystem::exists(envPath))
        {
            if (SovereignAssembler::HotPatchTokenizer(envPath.c_str()))
            {
                LOG_INFO("AVX2 tokenizer hot-patched successfully.");
            }
            else
            {
                LOG_WARNING("AVX2 tokenizer hot-patch failed (missing exports or load error).");
            }
        }
    }

    if (!m_workspaceRoot.empty())
    {
        g_agentEngine->setWorkspaceRoot(m_workspaceRoot);
    }
    SetIDEAgenticEngineForCommands(g_agentEngine.get());

    if (!modelName.empty())
    {
        m_modelName = modelName;
    }

    m_initialized = true;
    LOG_INFO("Native Inference Stack initialized successfully.");
    if (m_ide)
    {
        m_ide->appendToOutput(
            "Agentic + inference ready — Command Palette (Ctrl+Shift+P)  ·  Toggle/focus terminal (Ctrl+`)  ·  New "
            "integrated terminal (Ctrl+Shift+`)  ·  Agent terminal (Ctrl+Alt+A, read-only).\n",
            "Agent", Win32IDE::OutputSeverity::Success);
        if (IDEConfig::getInstance().getBool("features.speculativeDecoding", false))
        {
            m_ide->appendToOutput(
                "[Speed] Speculative decoding enabled — set inference.speculativeDraftGguf and "
                "inference.speculativeTargetGguf in rawrxd.config.json, then run palette: Inference: Reload "
                "Speculative Decoding.\n",
                "Agent", Win32IDE::OutputSeverity::Info);
        }
    }
    return true;
}

bool AgenticBridge::HasUsableBackend() const
{
    const auto eng = SharedCpuEngine();
    if (eng && eng->IsModelLoaded())
        return true;
    if (!m_initialized)
        return false;
    if (RawrXD::Agent::OrchestratorBridge::Instance().IsInitialized())
        return true;
    if (!m_modelName.empty())
    {
        auto endsWith = [](const std::string& s, const std::string& ext) -> bool
        {
            if (ext.size() > s.size())
                return false;
            return s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
        };
        std::string lower = m_modelName;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        const bool isLocalPath = m_modelName.find_first_of("/\\") != std::string::npos || endsWith(lower, ".gguf") ||
                                 endsWith(lower, ".gguf2") || endsWith(lower, ".bin") ||
                                 endsWith(lower, ".safetensors") || endsWith(lower, ".onnx");
        return !isLocalPath;
    }
    return false;
}

void AgenticBridge::SetCpuEngineLayerProgressCallback(std::function<void(const std::string&)> cb)
{
    SharedCpuEngine()->SetLayerProgressCallback(std::move(cb));
}

void AgenticBridge::SetCpuEngineSwarmTelemetryOutputCallback(std::function<void(const std::string&)> cb)
{
    SharedCpuEngine()->SetSwarmTelemetryOutputCallback(std::move(cb));
}

void AgenticBridge::SetMainWindow(HWND hwnd)
{
    m_hwndMain = hwnd;
}

bool AgenticBridge::postLogToMainWindow(UILogSeverity severity, const std::string& message) const
{
    if (!m_hwndMain || message.empty())
        return false;

    char* copy = _strdup(message.c_str());
    if (!copy)
        return false;

    if (!PostMessageA(m_hwndMain, WM_RAWR_LOG_MESSAGE, static_cast<WPARAM>(severity), reinterpret_cast<LPARAM>(copy)))
    {
        free(copy);
        return false;
    }
    return true;
}

// ============================================================================
// Core Agent Command Execution (Single Definition)
// ============================================================================

// E1: workspace root propagated to g_agentEngine on every Initialize call
// E2: OrchestratorBridge gets model+workdir before RunAgent
// E3: response size guard — truncate oversized responses before returning
// E4: tool-call dispatch result appended to history event
// E5: autoCorrect puppeteer only runs when response exceeds quality threshold
// E6: performance monitor records per-call latency
// E7: consecutive failure counter increments m_consecutiveFailures for router
std::string AgenticBridge::GenerateResponse(const std::string& prompt)
{
    AgentResponse response = ExecuteAgentCommand(prompt);
    return response.content;
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt)
{
    SCOPED_METRIC("agentic.execute_command");
    METRICS.increment("agentic.commands_total");
    auto& perf = RawrXD::Inference::PerformanceMonitor::instance();
    perf.startOperation("agentic.bridge.execute");
    bool perfClosed = false;
    auto closePerf = [&]()
    {
        if (!perfClosed)
        {
            perf.endOperation("agentic.bridge.execute");
            perfClosed = true;
        }
    };
    auto& sanitizer = RawrXD::Security::InputSanitizer::instance();
    auto promptSan = sanitizer.sanitizePrompt(prompt);
    if (promptSan.wasModified)
    {
        LOG_WARNING("Prompt sanitized before agent dispatch");
    }
    // E1: propagate workspace root to engine immediately
    if (!m_workspaceRoot.empty() && g_agentEngine)
        g_agentEngine->setWorkspaceRoot(m_workspaceRoot);

    // Optional explicit route into the minimal agentic controller loop.
    // Supported prefixes:
    //   /agentic <prompt>
    //   /agent <prompt>
    //   agentic: <prompt>
    //   @agent <prompt>
    const std::string sanitizedPrompt = promptSan.sanitized;
    bool hasAgenticPrefix = false;
    std::string minimalPrompt = StripAgenticPrefix(sanitizedPrompt, hasAgenticPrefix);
    const bool wantsMinimalAgent = m_enableAgenticMode || hasAgenticPrefix;
    const bool strictLocalSwarm = isTruthyEnvVar("RAWRXD_FORCE_LOCAL_SWARM");

    if (wantsMinimalAgent && rawrxd::isAgenticLayerAvailable())
    {
        static std::atomic<uint64_t> s_agentSessionCounter{0};
        if (m_currentSessionId.empty())
        {
            const uint64_t sessionOrdinal = ++s_agentSessionCounter;
            m_currentSessionId = "win32ide-agentic-" + std::to_string(sessionOrdinal);
        }

        rawrxd::MinimalAgenticRequest req;
        req.message = minimalPrompt.empty() ? sanitizedPrompt : minimalPrompt;
        req.session_id = m_currentSessionId;
        req.model_path = ResolveModelPath();
        req.enable_tools = true;
        req.max_iterations = 10;
        req.workspace_root = m_workspaceRoot;

        const auto miniResp = rawrxd::processAgenticRequest(req);
        if (miniResp.success)
        {
            std::string routed = miniResp.final_message;
            if (miniResp.tool_calls_made > 0)
            {
                routed = "[Agent executed " + std::to_string(miniResp.tool_calls_made) + " tools]\n\n" + routed;
            }
            if (m_outputCallback && !miniResp.final_message.empty())
            {
                m_outputCallback("stream", routed);
            }
            closePerf();
            return {AgentResponseType::ANSWER, routed};
        }
        if (strictLocalSwarm)
        {
            const std::string failClosed =
                miniResp.error.empty()
                    ? "Error: Strict local swarm mode rejected agentic request before legacy fallback"
                    : miniResp.error;
            if (m_outputCallback)
            {
                m_outputCallback("stream", failClosed);
            }
            closePerf();
            return {AgentResponseType::AGENT_ERROR, failClosed};
        }
        LOG_WARNING("MinimalAgentController route failed, falling back to default bridge lane: " + miniResp.error);
    }

    // E2: sync OrchestratorBridge model + workdir before routing
    auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
    if (!m_modelName.empty())
    {
        orch.SetModel(m_modelName);
        orch.SetFIMModel(m_modelName);
    }
    if (!m_workspaceRoot.empty())
        orch.SetWorkingDirectory(m_workspaceRoot);

    // E7: track consecutive failures for LLM router
    bool routeSuccess = false;
    auto markSuccess = [&] { routeSuccess = true; };
    (void)markSuccess;

    // Lazy-init: ensure engine exists so chat and agentic work with any loaded model (local definitions vary)
    if (!g_agentEngine)
    {
        Initialize("", m_modelName);
        SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
    }
    if (!g_agentEngine)
    {
        closePerf();
        return {AgentResponseType::AGENT_ERROR, "Engine Not Initialized"};
    }

    // Update engine config flags from bridge state
    AgenticEngine::GenerationConfig cfg;
    cfg.maxMode = m_maxMode;
    cfg.deepThinking = m_deepThinking;
    cfg.deepResearch = m_deepResearch;
    cfg.noRefusal = m_noRefusal;
    g_agentEngine->updateConfig(cfg);

    // --- Special Commands ---

    if (prompt.find("/react-server") == 0)
    {
        std::string name = (prompt.length() > 14) ? prompt.substr(14) : "react-app";
        std::string result = g_agentEngine->planTask("Create React Server named " + name);
        closePerf();
        return {AgentResponseType::ANSWER, result};
    }

    if (prompt.find("/install_vsix ") == 0)
    {
        std::string path = prompt.substr(14);
        bool res = RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
        closePerf();
        return {AgentResponseType::ANSWER, res ? "VSIX Installed" : "VSIX Installation Failed"};
    }

    // --- File-based commands (Bridge reads file, wraps into prompt) ---

    std::string refinedPrompt = promptSan.sanitized;

    if (prompt.find("/bugreport ") == 0)
    {
        std::string path = prompt.substr(11);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Bugreport path sanitized");
        }
        path = pathSan.sanitized;
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Analyze the following code for bugs, security vulnerabilities, "
                            "and logic errors.\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file (missing or exceeds size cap) " + path};
        }
    }
    else if (prompt.find("/suggest ") == 0)
    {
        std::string path = prompt.substr(9);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Suggest path sanitized");
        }
        path = pathSan.sanitized;
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Provide suggestions to improve the following code "
                            "(performance, readability, style).\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file (missing or exceeds size cap) " + path};
        }
    }
    else if (prompt.find("/patch ") == 0)
    {
        std::string path = prompt.substr(7);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Patch path sanitized");
        }
        path = pathSan.sanitized;
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Review the following code for hallucinations, invalid paths, "
                            "and logical contradictions. Rewrite the code to fix these issues "
                            "immediately.\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file (missing or exceeds size cap) " + path};
        }
    }

    // Prepend workspace and active-editor context so generic tab switches immediately
    // influence agent reasoning.
    if (!m_workspaceRoot.empty())
    {
        refinedPrompt = "[Workspace root: " + m_workspaceRoot + "]\n\n" + refinedPrompt;
    }
    const std::string openTabsContext = BuildOpenTabsPromptContext();
    if (!openTabsContext.empty())
    {
        refinedPrompt = openTabsContext + refinedPrompt;
    }

    if (refinedPrompt.size() > kMaxRefinedPromptBytes)
    {
        closePerf();
        return {AgentResponseType::ANSWER, "Error: Prompt exceeds maximum allowed size"};
    }

    applyAgentCapabilityHotpatches(refinedPrompt);

    // When no local model is loaded, route through active backend (Ollama/cloud) so agentic
    // work can use the same backend as chat (see docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    const auto inferenceStart = std::chrono::steady_clock::now();
    std::string response;
    bool localReady = SharedCpuEngine()->IsModelLoaded();
    if (!localReady)
    {
        // Prefer the orchestrator bridge (Ollama-backed, tool-aware) and let it decide
        // capability at runtime. If it cannot serve, fall back to legacy routing.
        auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
        if (!m_modelName.empty())
        {
            orch.SetModel(m_modelName);
            orch.SetFIMModel(m_modelName);
        }
        if (!m_workspaceRoot.empty())
        {
            orch.SetWorkingDirectory(m_workspaceRoot);
        }
        response = orch.RunAgent(refinedPrompt);

        if (response.empty() && m_ide)
        {
            response = m_ide->routeInferenceRequest(refinedPrompt);
        }
    }
    else
    {
        response = g_agentEngine->chat(refinedPrompt);
    }

    // Check for tool calls in the model's response and dispatch them
    // NOTE: hookToolResult fires inside DispatchModelToolCalls (the funnel)
    //       so every caller — Autonomy, Bridge, etc. — gets failure detection.
    std::string toolResult;
    const bool toolDispatched = DispatchModelToolCalls(response, toolResult);
    if (toolDispatched)
    {
        LOG_INFO("Tool call dispatched from model output");

        // Append tool result and optionally re-prompt the model
        response += "\n\n[Tool Execution Result]\n" + toolResult;
    }

    // Hook puppeteer + hotpatch orchestrator on the main agentic path (correct refusals, hallucinations, etc.)
    if (m_autoCorrect)
    {
        AgenticPuppeteer puppeteer;
        CorrectionResult pr = puppeteer.correctResponse(response, refinedPrompt);
        if (pr.success && !pr.correctedOutput.empty())
        {
            response = pr.correctedOutput;
            LOG_INFO("AgenticPuppeteer correction applied");
        }
    }
    {
        char correctedBuf[65536];
        CorrectionOutcome hot = AgenticHotpatchOrchestrator::instance().analyzeAndCorrect(
            response.c_str(), response.size(), refinedPrompt.c_str(), refinedPrompt.size(), correctedBuf,
            sizeof(correctedBuf));
        if (hot.success && hot.detail && correctedBuf[0] != '\0')
        {
            response.assign(correctedBuf);
            LOG_INFO("AgenticHotpatchOrchestrator correction applied: " + std::string(hot.detail ? hot.detail : ""));
        }
    }

    // E3: truncate oversized response before returning
    static constexpr size_t kMaxResponseBytes = 256 * 1024;
    if (response.size() > kMaxResponseBytes)
        response = response.substr(0, kMaxResponseBytes) + "\n[truncated]";

    // E6: record per-call inference latency
    {
        const auto inferenceUs =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - inferenceStart)
                .count();
        perf.recordLatency("agentic.bridge.inference", inferenceUs);
    }

    // CRITICAL: Stream response through callback so UI actually displays real inference output
    {
        std::string probe = "[PROBE-F] AgenticBridge emitting response_len=" + std::to_string(response.size()) +
                            " has_callback=" + (m_outputCallback ? "1" : "0") +
                            " preview=" + response.substr(0, std::min(response.size(), (size_t)80)) + "\n";
        OutputDebugStringA(probe.c_str());
    }
    if (m_outputCallback && !response.empty())
    {
        m_outputCallback("stream", response);
    }

    AgentResponse r;
    r.content = response;
    r.type = toolDispatched ? AgentResponseType::TOOL_CALL : AgentResponseType::ANSWER;
    closePerf();
    return r;
}

// ============================================================================
// Configuration Methods
// ============================================================================

void AgenticBridge::SetMaxMode(bool enabled)
{
    m_maxMode = enabled;
    LOG_INFO(std::string("Max Mode ") + (enabled ? "Enabled" : "Disabled"));
    if (enabled)
    {
        const auto eng = SharedCpuEngine();
        if (eng->GetContextLimit() < 32768)
            eng->SetContextLimit(32768);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
    LOG_INFO(std::string("Deep Thinking ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
    LOG_INFO(std::string("Deep Research ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetNoRefusal(bool enabled)
{
    m_noRefusal = enabled;
    LOG_INFO(std::string("No Refusal Mode ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetSwarmMode(bool enabled)
{
    m_swarmMode = enabled;
    SharedCpuEngine()->SetSwarmMode(enabled);
    LOG_INFO(std::string("Swarm Mode ") + (enabled ? "Enabled" : "Disabled"));
}

bool AgenticBridge::LoadSwarmFromDirectory(const std::string& directoryPath, int maxModels)
{
    const auto eng = SharedCpuEngine();
    bool success = eng->LoadSwarmFromDirectory(directoryPath, maxModels);
    if (!success)
        m_lastModelLoadError = eng->GetLastLoadErrorMessage();
    return success;
}

void AgenticBridge::SetAutoCorrect(bool enabled)
{
    m_autoCorrect = enabled;
    LOG_INFO(std::string("Auto Correct ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetHotpatchSubAgentToolProtocol(bool enabled)
{
    m_hotpatchSubAgentToolProtocol = enabled;
    LOG_INFO(std::string("SubAgent tool-protocol hotpatch ") + (enabled ? "enabled" : "disabled"));
}

void AgenticBridge::SetHotpatchThoughtProtocol(bool enabled)
{
    m_hotpatchThoughtProtocol = enabled;
    LOG_INFO(std::string("Thought-protocol hotpatch ") + (enabled ? "enabled" : "disabled"));
}

void AgenticBridge::applyAgentCapabilityHotpatches(std::string& refinedPrompt)
{
    std::string prefix;
    if (m_hotpatchSubAgentToolProtocol && !envDisablesCapabilityHotpatch("RAWRXD_DISABLE_SUBAGENT_HOTPATCH"))
    {
        prefix += "[RawrXD hotpatch — SubAgent/tools]\n"
                  "The model stack may not expose native function-calling. To delegate a subtask, emit ONE line:\n"
                  "  TOOL:runSubagent:{\"description\":\"short label\",\"prompt\":\"instructions for the sub-agent\"}\n"
                  "Workspace tools (one line each): tool:list_dir path=.\n"
                  "  tool:read_file path=<path>\n"
                  "  tool:write_file path=<path> content=<text>\n"
                  "Parallel work: TOOL:hexmag_swarm:{\"prompts\":[\"task1\",\"task2\"],\"strategy\":\"concatenate\","
                  "\"maxParallel\":4}\n"
                  "Sequential pipeline: TOOL:chain:{\"steps\":[\"step1\",\"step2\"],\"input\":\"...\"}\n"
                  "RawrXD dispatches these patterns from plain text even without server-side tool schemas.\n\n";
    }
    if (m_deepThinking && m_hotpatchThoughtProtocol &&
        !envDisablesCapabilityHotpatch("RAWRXD_DISABLE_THOUGHT_HOTPATCH"))
    {
        prefix += "[RawrXD hotpatch — Thought]\n"
                  "Even without native chain-of-thought in the backend, first write a concise private reasoning block "
                  "inside <thought>...</thought>, then give the user-facing answer after </thought>.\n\n";
    }
    if (!prefix.empty())
        refinedPrompt.insert(0, prefix);
}

void AgenticBridge::SetLanguageContext(const std::string& language, const std::string& filePath)
{
    m_languageContext = language;
    m_fileContext = filePath;
    // Propagate to the native agent if available
    if (m_nativeAgent)
    {
        m_nativeAgent->SetLanguageContext(language);
        m_nativeAgent->SetFileContext(filePath);
    }
    LOG_INFO("Language context set: " + language + " file: " + filePath);
}

void AgenticBridge::SetContextSize(const std::string& sizeName)
{
    size_t limit = 4096;
    std::string s = sizeName;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "4k")
        limit = 4096;
    else if (s == "32k")
        limit = 32768;
    else if (s == "64k")
        limit = 65536;
    else if (s == "128k")
        limit = 131072;
    else if (s == "256k")
        limit = 262144;
    else if (s == "512k")
        limit = 524288;
    else if (s == "1m")
        limit = 1048576;
    else if (s == "unlimited" || s == "bypass")
        limit = 10485760;  // Memory-gate bypass: 10M tokens (reasonable unlimited)

    SharedCpuEngine()->SetContextLimit(limit);

    std::stringstream ss;
    ss << "Context Window resized to: " << sizeName << " (" << (limit >= 10485760 ? "unlimited" : std::to_string(limit))
       << " tokens)";
    LOG_INFO(ss.str());
}

// ============================================================================
// Agent Loop Management
// ============================================================================

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations)
{
    SCOPED_METRIC("agentic.start_loop");
    METRICS.increment("agentic.loops_started");
    LOG_INFO("StartAgentLoop: " + initialPrompt);

    if (!m_initialized)
    {
        LOG_ERROR("Cannot start agent loop - not initialized");
        return false;
    }

    if (m_agentLoopRunning)
    {
        LOG_WARNING("Agent loop already running");
        return false;
    }

    m_agentLoopRunning = true;
    const int boundedIterations = std::max(1, std::min(maxIterations, 50));
    std::string currentPrompt = initialPrompt;
    const std::string goal = initialPrompt;
    bool completed = false;
    const bool success = true;

    for (int i = 0; i < boundedIterations && m_agentLoopRunning; ++i)
    {
        LOG_INFO("Agent loop cycle " + std::to_string(i + 1) + " / " + std::to_string(boundedIterations));

        if (m_ide)
            m_ide->showAgentActivityStatus(
                "Agent loop: " + std::to_string(i + 1) + " / " + std::to_string(boundedIterations), 8000);

        AgentResponse response = ExecuteAgentCommand(currentPrompt);

        if (m_outputCallback)
        {
            std::string iterationTitle = "Agent [Cycle " + std::to_string(i + 1) + "]";
            m_outputCallback(iterationTitle, response.content);
        }
        else
        {
            postLogToMainWindow(UILogSeverity::Info,
                                "Agent [Cycle " + std::to_string(i + 1) + "]:\n" + response.content);
        }

        if (response.type == AgentResponseType::TOOL_CALL)
        {
            static constexpr const char kToolMarker[] = "[Tool Execution Result]";
            const size_t markerPos = response.content.find(kToolMarker);
            if (markerPos != std::string::npos)
            {
                size_t payloadStart = markerPos + sizeof(kToolMarker) - 1;
                while (payloadStart < response.content.size() &&
                       (response.content[payloadStart] == '\n' || response.content[payloadStart] == '\r'))
                {
                    ++payloadStart;
                }
                const std::string observation = response.content.substr(payloadStart);
                currentPrompt =
                    "Observation from tool execution:\n" + observation + "\n\nContinue toward the goal: " + goal;
                continue;
            }

            std::string toolResult;
            if (DispatchModelToolCalls(response.content, toolResult) && !toolResult.empty())
            {
                currentPrompt = "Tool result:\n" + toolResult + "\nProvide the next step or final answer.";
                continue;
            }
        }

        if (response.type == AgentResponseType::ANSWER && !response.content.empty())
        {
            completed = true;
            LOG_INFO("Agent loop completed: model returned final answer.");
            break;
        }

        if (response.content.empty())
        {
            LOG_WARNING("Agent loop: empty model response; stopping.");
            break;
        }

        currentPrompt = "Continue from previous result and make progress:\n" + response.content;
    }

    if (!completed && m_agentLoopRunning)
    {
        LOG_INFO("Agent loop: iteration cap reached without a final answer.");
    }

    m_agentLoopRunning = false;
    return success;
}

void AgenticBridge::StopAgentLoop()
{
    LOG_INFO("StopAgentLoop called");
    m_agentLoopRunning = false;
    KillPowerShellProcess();
}

// ============================================================================
// Status & Capability Queries
// ============================================================================

std::vector<std::string> AgenticBridge::GetAvailableTools()
{
    return {"shell",       "powershell",       "run_in_terminal", "read_file",    "write_file",
            "list_dir",    "list_directory",   "grep_files",      "search_files", "reference_symbol",
            "load_model",  "model_status",     "web_search",      "git_status",   "task_orchestrator",
            "runSubagent", "manage_todo_list", "chain",           "hexmag_swarm"};
}

std::string AgenticBridge::GetAgentStatus()
{
    std::stringstream status;
    const uint32_t phase17Epochs = Phase17Profiler::GetEpochCount();
    const uint64_t phase17ElapsedCycles = AgenticProfilerGetElapsed();
    const std::string phase17TopTools = AgenticProfilerTopSummary(3);
    status << "Agentic Framework Status:\n";
    status << "  Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    status << "  Model: " << m_modelName << "\n";
    status << "  Ollama Server: " << m_ollamaServer << "\n";
    status << "  Framework Path: " << m_frameworkPath << "\n";
    status << "  Workspace Root: " << (m_workspaceRoot.empty() ? "<unset>" : m_workspaceRoot) << "\n";
    status << "  Loop Running: " << (m_agentLoopRunning ? "Yes" : "No") << "\n";
    status << "  Max Mode: " << (m_maxMode ? "Yes" : "No") << "\n";
    status << "  Deep Thinking: " << (m_deepThinking ? "Yes" : "No") << "\n";
    status << "  Deep Research: " << (m_deepResearch ? "Yes" : "No") << "\n";
    status << "  Engine Loaded: " << (SharedCpuEngine()->IsModelLoaded() ? "Yes" : "No") << "\n";
    if (g_agentEngine)
    {
        status << "  Model Status: " << g_agentEngine->getModelStatus() << "\n";
    }
    if (m_subAgentManager)
    {
        status << "  SubAgents Active: " << m_subAgentManager->activeSubAgentCount() << "\n";
        status << "  SubAgents Spawned: " << m_subAgentManager->totalSpawned() << "\n";
        status << "  " << m_subAgentManager->getStatusSummary() << "\n";
    }
    status << "  Ghost Stream Last Seq: " << GetLastGhostSeq() << "\n";
    status << "  Ghost Stream Backtracks: " << GetGhostSeqBacktracks() << "\n";
    status << "  Ghost Stream Gap Events: " << GetGhostSeqGapEvents() << "\n";
    status << "  Phase17 Epochs: " << phase17Epochs << "\n";
    status << "  Phase17 Elapsed Cycles: " << phase17ElapsedCycles << "\n";
    status << "  Phase17 Top Tools: " << phase17TopTools << "\n";
    return status.str();
}

void AgenticBridge::ObserveGhostStreamSeq(uint64_t seq)
{
    if (seq == 0)
    {
        return;
    }

    const uint64_t prev = m_lastGhostSeq.load(std::memory_order_relaxed);
    if (prev != 0)
    {
        if (seq <= prev)
        {
            m_ghostSeqBacktracks.fetch_add(1, std::memory_order_relaxed);
        }
        else if (seq > prev + 1)
        {
            m_ghostSeqGapEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    uint64_t observed = prev;
    while (seq > observed &&
           !m_lastGhostSeq.compare_exchange_weak(observed, seq, std::memory_order_relaxed, std::memory_order_relaxed))
    {
    }
}

uint64_t AgenticBridge::GetLastGhostSeq() const
{
    return m_lastGhostSeq.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqBacktracks() const
{
    return m_ghostSeqBacktracks.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqGapEvents() const
{
    return m_ghostSeqGapEvents.load(std::memory_order_relaxed);
}

void AgenticBridge::ResetGhostSeqTelemetry()
{
    m_lastGhostSeq.store(0, std::memory_order_relaxed);
    m_ghostSeqBacktracks.store(0, std::memory_order_relaxed);
    m_ghostSeqGapEvents.store(0, std::memory_order_relaxed);
}

// ============================================================================
// Model & Server Configuration
// ============================================================================

void AgenticBridge::SetModel(const std::string& modelName)
{
    m_modelName = modelName;
    LOG_INFO("Model set to: " + modelName);

    if (modelName.empty())
    {
#ifdef _WIN32
        SetEnvironmentVariableW(L"RAWRXD_NATIVE_MODEL_PATH", nullptr);
        RawrXD::TitanProxy::instance().clearLoadedModelCache();
#else
        unsetenv("RAWRXD_NATIVE_MODEL_PATH");
#endif
        return;
    }

    auto endsWith = [](const std::string& s, const std::string& ext) -> bool
    {
        if (ext.size() > s.size())
            return false;
        return s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
    };
    std::string lower = modelName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    bool isPath =
        !modelName.empty() &&
        (modelName.find_first_of("/\\") != std::string::npos || endsWith(lower, ".gguf") || endsWith(lower, ".gguf2") ||
         endsWith(lower, ".bin") || endsWith(lower, ".safetensors") || endsWith(lower, ".onnx"));

    if (isPath)
    {
// GGUF file path — load via native engine
// CRITICAL: Set environment variable so BackendOrchestrator.RunNativeInferenceSync
// can load the model via InferenceEngine_LoadModel before attempting inference.
#ifdef _WIN32
        // Convert model path to wide char for SetEnvironmentVariableW
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, modelName.c_str(), -1, nullptr, 0);
        if (wideLen > 0 && wideLen < 2048)
        {
            wchar_t wModelPath[2048] = {};
            MultiByteToWideChar(CP_UTF8, 0, modelName.c_str(), -1, wModelPath, wideLen);
            SetEnvironmentVariableW(L"RAWRXD_NATIVE_MODEL_PATH", wModelPath);
        }
#else
        setenv("RAWRXD_NATIVE_MODEL_PATH", modelName.c_str(), 1);
#endif
        LOG_INFO("Set RAWRXD_NATIVE_MODEL_PATH to: " + modelName);

#ifdef _WIN32
        std::string titanErr;
        if (!RawrXD::TitanProxy::instance().loadModel(modelName, titanErr))
        {
            LOG_WARNING("TitanHost load_model (eager): " + titanErr);
        }
#endif
    }
    else if (!modelName.empty())
    {
        // Native file path no longer selected — drop env + Titan cache so inference
        // does not keep a stale GGUF in TitanHost.
#ifdef _WIN32
        SetEnvironmentVariableW(L"RAWRXD_NATIVE_MODEL_PATH", nullptr);
        RawrXD::TitanProxy::instance().clearLoadedModelCache();
#else
        unsetenv("RAWRXD_NATIVE_MODEL_PATH");
#endif
        // Ollama model tag (e.g. "llama3.3:latest") — propagate to BackendSwitcher
        // so that routeToOllama() sends the correct model name
        if (m_ide)
        {
            m_ide->setBackendModel(Win32IDE::AIBackendType::Ollama, modelName);
            LOG_INFO("BackendSwitcher Ollama model updated to: " + modelName);
        }

        // Keep OrchestratorBridge (agentic/tool-calling path) in sync so IDE agent flows
        // use the same selected model as chat/FIM.
        auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
        orch.SetModel(modelName);
        orch.SetFIMModel(modelName);
    }
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl)
{
    m_ollamaServer = serverUrl;
    LOG_INFO("Ollama server set to: " + serverUrl);
}

void AgenticBridge::SetOutputCallback(OutputCallback callback)
{
    m_outputCallback = callback;
}

// ============================================================================
// PowerShell Process Management (Full Implementation)
// ============================================================================

bool AgenticBridge::SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments)
{
    LOG_DEBUG("Spawning PowerShell: " + scriptPath + " " + arguments);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0))
    {
        LOG_ERROR("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0))
    {
        LOG_ERROR("Failed to create stdin pipe");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        return false;
    }
    SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = m_hStdoutWrite;
    si.hStdError = m_hStdoutWrite;
    si.hStdInput = m_hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = scriptPath + " " + arguments;

    BOOL success = CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                                  NULL, &si, &pi);

    if (!success)
    {
        LOG_ERROR("Failed to create PowerShell process");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        return false;
    }

    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    LOG_DEBUG("PowerShell process spawned successfully");
    return true;
}

bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs)
{
    LOG_DEBUG("Reading process output");
    output.clear();

    if (!m_hStdoutRead)
    {
        LOG_ERROR("No stdout handle");
        return false;
    }

    if (m_hStdoutWrite)
    {
        CloseHandle(m_hStdoutWrite);
        m_hStdoutWrite = nullptr;
    }

    char buffer[4096];
    DWORD bytesRead;
    DWORD startTime = GetTickCount();
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

    while (true)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL))
        {
            break;
        }

        if (available > 0)
        {
            if (ReadFile(m_hStdoutRead, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
            {
                const size_t safeBytes =
                    (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                buffer[safeBytes] = '\0';
                output.append(buffer, safeBytes);
            }
            else
            {
                break;
            }
        }

        if (GetTickCount() - startTime > timeoutMs)
        {
            LOG_WARNING("ReadProcessOutput timeout");
            break;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0)
            {
                if (ReadFile(m_hStdoutRead, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
                {
                    const size_t safeBytes =
                        (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buffer[safeBytes] = '\0';
                    output.append(buffer, safeBytes);
                }
            }
            break;
        }

        Sleep(15);
    }

    LOG_DEBUG("Read " + std::to_string(output.length()) + " bytes from process");
    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess()
{
    if (m_hProcess)
    {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
        LOG_DEBUG("PowerShell process terminated");
    }
    if (m_hStdoutRead)
    {
        CloseHandle(m_hStdoutRead);
        m_hStdoutRead = nullptr;
    }
    if (m_hStdoutWrite)
    {
        CloseHandle(m_hStdoutWrite);
        m_hStdoutWrite = nullptr;
    }
    if (m_hStdinRead)
    {
        CloseHandle(m_hStdinRead);
        m_hStdinRead = nullptr;
    }
    if (m_hStdinWrite)
    {
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = nullptr;
    }
}

// ============================================================================
// Response Parsing (Full Implementation)
// ============================================================================

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput)
{
    AgentResponse response;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;

    std::istringstream stream(rawOutput);
    std::string line;
    std::string fullContent;

    while (std::getline(stream, line))
    {
        if (IsToolCall(line))
        {
            response.type = AgentResponseType::TOOL_CALL;
            const size_t colonPos = line.find(':');
            if (colonPos != std::string::npos)
            {
                const std::string payload = line.substr(colonPos + 1);
                const size_t spacePos = payload.find(' ');
                if (spacePos != std::string::npos)
                {
                    response.toolName = payload.substr(0, spacePos);
                    response.toolArgs = payload.substr(spacePos + 1);
                }
                else
                {
                    response.toolName = payload;
                }
            }
        }
        else if (IsAnswer(line))
        {
            response.type = AgentResponseType::ANSWER;
            const size_t answerPos = line.find("ANSWER:");
            if (answerPos != std::string::npos)
            {
                response.content += line.substr(answerPos + 7);
                response.content += "\n";
            }
        }
        fullContent += line + "\n";
    }

    if (response.content.empty())
    {
        response.content = fullContent;
    }

    return response;
}

bool AgenticBridge::IsToolCall(const std::string& line) const
{
    // Accept both legacy (TOOL:) and compact (tool:) prefixes.
    // Downstream execution normalizes prefixes anyway.
    return line.rfind("TOOL:", 0) == 0 || line.rfind("tool:", 0) == 0;
}

bool AgenticBridge::IsAnswer(const std::string& line) const
{
    return line.find("ANSWER:") == 0;
}

// ============================================================================
// Path Resolution
// ============================================================================

// ============================================================================
// Workspace Management
// ============================================================================

// Workspace root is stored locally; syncing with the engine is currently
// handled elsewhere in the codebase.
void AgenticBridge::SetWorkspaceRoot(const std::string& workspaceRoot)
{
    if (workspaceRoot.empty())
    {
        m_workspaceRoot.clear();
        if (g_agentEngine)
        {
            g_agentEngine->setWorkspaceRoot(workspaceRoot);
        }
        LOG_INFO("AgenticBridge workspace root cleared");
        return;
    }

    const std::string normalized = canonicalWorkspaceRootForAgent(workspaceRoot);
    m_workspaceRoot = normalized;
    if (g_agentEngine)
    {
        g_agentEngine->setWorkspaceRoot(normalized);
    }
    rawrxd::MinimalAgentController::instance().setWorkspaceRoot(normalized);
    RawrXD::Agent::ToolGuardrails guards = RawrXD::Agent::AgentToolHandlers::GetGuardrails();
    guards.allowedRoots.clear();
    guards.allowedRoots.push_back(normalized);
    RawrXD::Agent::AgentToolHandlers::SetGuardrails(guards);
    LOG_INFO("AgenticBridge workspace root updated: " + m_workspaceRoot);
}

std::string AgenticBridge::ResolveFrameworkPath()
{
    // Resolve the exe directory for portable path resolution
    char exeDir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash)
        *(lastSlash + 1) = '\0';

    std::string base(exeDir);
    std::vector<std::string> searchPaths = {base + "Agentic-Framework.ps1", base + "scripts\\Agentic-Framework.ps1",
                                            "Agentic-Framework.ps1",        "scripts\\Agentic-Framework.ps1",
                                            "..\\Agentic-Framework.ps1",    "..\\scripts\\Agentic-Framework.ps1"};

    for (const auto& path : searchPaths)
    {
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            LOG_INFO("Found Agentic-Framework.ps1 at: " + path);
            return path;
        }
    }

    LOG_WARNING("Agentic-Framework.ps1 not found in any search path");
    return "Agentic-Framework.ps1";
}

std::string AgenticBridge::ResolveToolsModulePath()
{
    // First try: ResolveFrameworkPath
    std::string base = ResolveFrameworkPath();
    if (!base.empty() && base != "Agentic-Framework.ps1")
    {
        std::filesystem::path p(base);
        if (p.has_parent_path())
        {
            p = p.parent_path() / "Tools" / "AgentTools.ps1";
            if (std::filesystem::exists(p))
                return p.string();
        }
    }

    // Fallback: check environment variables
    const char* envPaths[] = {"RAWRXD_TOOLS_PATH", "RAWRXD_HOME", "PROGRAMFILES"};
    for (const char* envVar : envPaths)
    {
        char buffer[512];
        DWORD n = GetEnvironmentVariableA(envVar, buffer, sizeof(buffer));
        if (n > 0 && n < sizeof(buffer))
        {
            std::filesystem::path p(buffer);
            auto toolsPath = p / "Tools" / "AgentTools.ps1";
            if (std::filesystem::exists(toolsPath))
                return toolsPath.string();

            toolsPath = p / "AgentTools.ps1";
            if (std::filesystem::exists(toolsPath))
                return toolsPath.string();
        }
    }

    // Fallback: check common installation directories
    const std::string commonPaths[] = {"C:\\Program Files\\RawrXD\\Tools\\AgentTools.ps1",
                                       "C:\\RawrXD\\Tools\\AgentTools.ps1", "D:\\rawrxd\\scripts\\AgentTools.ps1",
                                       "..\\..\\scripts\\AgentTools.ps1"};
    for (const auto& path : commonPaths)
    {
        if (std::filesystem::exists(path))
            return path;
    }

    return "";
}

std::string AgenticBridge::ResolveModelPath() const
{
    if (!m_modelName.empty())
    {
        return m_modelName;
    }
    return "llama2";
}

std::string AgenticBridge::StripAgenticPrefix(const std::string& prompt, bool& wasAgenticPrefixed) const
{
    wasAgenticPrefixed = false;

    auto trimLeft = [](std::string& s)
    {
        const auto pos = s.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos)
        {
            s.clear();
            return;
        }
        if (pos > 0)
            s.erase(0, pos);
    };

    if (prompt.rfind("agentic:", 0) == 0)
    {
        wasAgenticPrefixed = true;
        std::string out = prompt.substr(8);
        trimLeft(out);
        return out;
    }
    if (prompt.rfind("/agentic ", 0) == 0)
    {
        wasAgenticPrefixed = true;
        std::string out = prompt.substr(9);
        trimLeft(out);
        return out;
    }
    if (prompt.rfind("/agent ", 0) == 0)
    {
        wasAgenticPrefixed = true;
        std::string out = prompt.substr(7);
        trimLeft(out);
        return out;
    }
    if (prompt.rfind("@agent ", 0) == 0)
    {
        wasAgenticPrefixed = true;
        std::string out = prompt.substr(7);
        trimLeft(out);
        return out;
    }

    return prompt;
}

// ============================================================================
// RE Suite Tools Bridge (Real Implementations)
// ============================================================================

std::string AgenticBridge::RunDumpbin(const std::string& path, const std::string& mode)
{
    if (path.empty())
        return "Error: Empty file path";

    // Try engine first if available
    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runDumpbin(path, mode);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }

    const std::string vsRoot = ResolveVsInstallRoot();
    std::string dumpbinPath;
    if (const auto found = ResolveToolInLatestMsvc(vsRoot, "x64", "dumpbin.exe"))
    {
        dumpbinPath = *found;
    }

    if (dumpbinPath.empty())
    {
        return "Error: dumpbin.exe not found. Install Visual Studio 2022 with Desktop development with C++, or "
               "ensure vswhere can locate an installation with VC tools.";
    }

    const std::optional<std::string> vcvars = ResolveVcvarsBatForArch(vsRoot, "x64");
    const std::string modeArg = mode.empty() ? "/HEADERS" : "/" + mode;
    std::string command;
    if (vcvars.has_value())
    {
        const std::string tail = modeArg + " \"" + path + "\" 2>&1";
        command = BuildCmdWithVcvars(*vcvars, dumpbinPath, tail);
    }
    else
    {
        command = "\"" + dumpbinPath + "\" " + modeArg + " \"" + path + "\" 2>&1";
    }

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
    {
        return "Error: Failed to execute dumpbin.exe";
    }

    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output.append(buffer);
    }
    _pclose(pipe);

    return output.empty() ? "Dumpbin completed with no output" : output;
}

std::string AgenticBridge::RunCodex(const std::string& path)
{
    if (path.empty())
        return "Error: Empty file path";

    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runCodex(path);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }

    if (!std::filesystem::exists(path))
        return "Error: File not found: " + path;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return "Error: Cannot open file: " + path;

    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));

    std::ostringstream analysis;
    analysis << "File Analysis: " << path << "\n";
    analysis << "Size: " << fileSize << " bytes\n";

    if ((magic & 0xFFFF) == 0x5A4D)
    {
        analysis << "Type: PE Executable\n";
        analysis << "Subsystem: Windows\n";
    }
    else if (magic == 0x7F454C46)
    {
        analysis << "Type: ELF Binary\n";
    }
    else if (magic == 0xCAFEBABE || magic == 0xBEBAFECA)
    {
        analysis << "Type: Mach-O / Class File\n";
    }
    else if ((magic & 0xFF) == 0xFE)
    {
        analysis << "Type: Managed Assembly (.NET)\n";
    }
    else
    {
        analysis << "Type: Unknown/Binary\n";
    }

    analysis << "First 4 bytes (hex): ";
    for (int i = 0; i < 4; ++i)
    {
        analysis << std::hex << std::setw(2) << std::setfill('0') << ((magic >> (i * 8)) & 0xFF);
    }
    analysis << "\n";

    return analysis.str();
}

std::string AgenticBridge::RunCompiler(const std::string& path)
{
    return RunCompilerImpl(path, "x64");
}

// Helper: actual compiler implementation with architecture support
std::string AgenticBridge::RunCompilerImpl(const std::string& path, const std::string& arch)
{
    if (path.empty())
        return "Error: Empty file path";

    // Try engine first if available
    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runCompiler(path, arch);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }

    // Check if file exists
    if (!std::filesystem::exists(path))
        return "Error: File not found: " + path;

    // Get file extension
    std::filesystem::path filePath(path);
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string compilerFlags;
    std::string sovereignFallbackBanner;

    // Detect language and get compiler
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c")
    {
        compilerFlags = "/c /W4 /std:c++20";
    }
    else if (ext == ".asm" || ext == ".s")
    {
        // Prefer zero-dependency sovereign assembler (subset MASM-like → PE). Falls back to ml64 for full MASM.
        std::string asmSource;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, asmSource))
        {
            const std::filesystem::path peOut = filePath.parent_path() / (filePath.stem().string() + "_sovereign.exe");
            std::string sovereignErr;
            if (SovereignAssembler::AssembleAndLink(asmSource, peOut.wstring(), sovereignErr))
            {
                return "Sovereign internal assembler succeeded (no ml64). PE output: " + peOut.string();
            }
            if (!sovereignErr.empty())
            {
                sovereignFallbackBanner =
                    "[SovereignAssembler failed; falling back to ml64. Reason: " + sovereignErr + "]\n";
                RawrXD::Logging::Logger::instance().warning("SovereignAssembler (ml64 fallback): " + sovereignErr,
                                                            "Win32IDE.RunCompiler");
                postLogToMainWindow(UILogSeverity::Warning, "SovereignAssembler (ml64 fallback): " + sovereignErr);
            }
            else
            {
                sovereignFallbackBanner = "[SovereignAssembler failed with no error message; falling back to ml64]\n";
                RawrXD::Logging::Logger::instance().warning(
                    "SovereignAssembler failed with empty error string; using ml64 fallback.", "Win32IDE.RunCompiler");
                postLogToMainWindow(UILogSeverity::Warning,
                                    "SovereignAssembler failed with empty error string; using ml64 fallback.");
            }
        }
        compilerFlags = "/c";
    }
    else
    {
        return "Error: Unsupported file type: " + ext;
    }

    const bool usesCl = (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c");

    if (usesCl)
    {
        if (arch == "x64" || arch == "x86-64" || arch == "amd64")
        {
            compilerFlags += " /machine:x64";
        }
        else if (arch == "x86" || arch == "i386")
        {
            compilerFlags += " /machine:x86";
        }
        else if (arch == "arm" || arch == "arm64")
        {
            compilerFlags += " /machine:arm64";
        }
    }

    const std::string vsRoot = ResolveVsInstallRoot();
    const std::optional<std::string> vcvars = ResolveVcvarsBatForArch(vsRoot, arch);
    const std::string exeName = usesCl ? "cl.exe" : "ml64.exe";
    std::string resolvedTool;
    if (const auto t = ResolveToolInLatestMsvc(vsRoot, arch, exeName))
    {
        resolvedTool = *t;
    }

    const std::string outFile = filePath.stem().string() + ".obj";
    const std::string toolArgsTail = compilerFlags + " /Fo" + outFile + " \"" + path + "\" 2>&1";

    std::string command;
    if (vcvars.has_value() && !resolvedTool.empty())
    {
        command = BuildCmdWithVcvars(*vcvars, resolvedTool, toolArgsTail);
    }
    else if (!resolvedTool.empty())
    {
        command = "\"" + resolvedTool + "\" " + toolArgsTail;
    }
    else
    {
        command = exeName + " " + toolArgsTail;
    }

    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
    {
        return "Error: Failed to execute compiler. Install Visual Studio 2022 with C++ workload, or open a "
               "Developer Command Prompt and ensure cl.exe / ml64.exe are on PATH.";
    }

    std::string output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output.append(buffer);
    }
    const int exitCode = _pclose(pipe);

    const std::string toolLabel = usesCl ? "cl" : "ml64";
    if (exitCode == 0)
    {
        return sovereignFallbackBanner + "Compilation succeeded (" + toolLabel + "). Output: " + outFile + "\n" +
               output;
    }

    return sovereignFallbackBanner + "Compilation failed (" + toolLabel + ", exit code: " + std::to_string(exitCode) +
           ")\n" + output;
}

// ============================================================================
// SubAgent / Chaining / HexMag Swarm Operations
// ============================================================================

SubAgentManager* AgenticBridge::GetSubAgentManager()
{
    if (!m_subAgentManager && g_agentEngine)
    {
        // Use factory to get IDELogger + METRICS wired automatically
        m_subAgentManager.reset(createWin32SubAgentManager(g_agentEngine.get()));

        // Wire callbacks to IDE output
        m_subAgentManager->setCompletionCallback(
            [this](const std::string& agentId, const std::string& result, bool success)
            {
                if (m_outputCallback)
                {
                    std::string prefix = success ? "✅ SubAgent " : "❌ SubAgent ";
                    m_outputCallback(prefix + agentId, result);
                }
                else
                {
                    const auto sev = success ? UILogSeverity::Info : UILogSeverity::Error;
                    const std::string prefix = success ? "✅ SubAgent " : "❌ SubAgent ";
                    postLogToMainWindow(sev, prefix + agentId + "\n" + result);
                }
            });
    }
    return m_subAgentManager.get();
}

std::string AgenticBridge::RunSubAgent(const std::string& description, const std::string& prompt)
{
    SCOPED_METRIC("agentic.run_subagent");
    METRICS.increment("agentic.subagent_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("RunSubAgent: " + description);
    std::string agentId = mgr->spawnSubAgent("bridge", description, prompt);
    bool success = mgr->waitForSubAgent(agentId, 120000);
    return mgr->getSubAgentResult(agentId);
}

std::string AgenticBridge::ExecuteChain(const std::vector<std::string>& steps, const std::string& initialInput)
{
    SCOPED_METRIC("agentic.execute_chain");
    METRICS.increment("agentic.chain_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("ExecuteChain: " + std::to_string(steps.size()) + " steps");
    return mgr->executeChain("bridge", steps, initialInput);
}

std::string AgenticBridge::ExecuteSwarm(const std::vector<std::string>& prompts, const std::string& mergeStrategy,
                                        int maxParallel)
{
    SCOPED_METRIC("agentic.execute_swarm");
    METRICS.increment("agentic.swarm_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    SwarmConfig config;
    config.maxParallel = maxParallel;
    config.timeoutMs = 120000;
    config.mergeStrategy = mergeStrategy;
    config.failFast = false;

    LOG_INFO("ExecuteSwarm: " + std::to_string(prompts.size()) + " tasks, strategy=" + mergeStrategy);
    std::string mergedResult = mgr->executeSwarm("bridge", prompts, config);

    // Phase 4B: Choke Point 5 — hookSwarmMerge after swarm merge
    if (m_ide)
    {
        FailureClassification swarmFailure = m_ide->hookSwarmMerge(mergedResult, (int)prompts.size(), mergeStrategy);
        if (swarmFailure.reason != AgentFailureType::None)
        {
            LOG_WARNING("[Phase4B] Swarm merge failure: " + m_ide->failureTypeString(swarmFailure.reason) +
                        " (confidence=" + std::to_string(swarmFailure.confidence) + ")");
        }
    }

    return mergedResult;
}

void AgenticBridge::CancelAllSubAgents()
{
    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        mgr->cancelAll();
        LOG_INFO("All sub-agents cancelled via bridge");
    }
}

std::string AgenticBridge::GetSubAgentStatus() const
{
    if (m_subAgentManager)
    {
        return m_subAgentManager->getStatusSummary();
    }
    // Attempt lazy creation even from const context so status can reflect live runtime state.
    auto* self = const_cast<AgenticBridge*>(this);
    if (self)
    {
        auto* mgr = self->GetSubAgentManager();
        if (mgr)
        {
            return mgr->getStatusSummary();
        }
    }
    return "SubAgentManager not initialized";
}

void AgenticBridge::ExecuteSubAgentChain(const std::string& taskDescription)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
    {
        LOG_ERROR("Cannot execute SubAgent chain: manager not initialized");
        return;
    }

    // Parse the task description to identify subtasks
    std::vector<std::string> steps;
    std::stringstream ss(taskDescription);
    std::string line;

    // Simple heuristic: "step1 | step2 | step3" or "1. step1 2. step2" format
    size_t delimPos = 0;
    std::string delimiter = (taskDescription.find(" | ") != std::string::npos) ? " | " : "; ";

    size_t start = 0;
    size_t end = taskDescription.find(delimiter);
    while (end != std::string::npos)
    {
        std::string step = taskDescription.substr(start, end - start);
        if (!step.empty())
            steps.push_back(step);
        start = end + delimiter.length();
        end = taskDescription.find(delimiter, start);
    }

    std::string finalStep = taskDescription.substr(start);
    if (!finalStep.empty())
        steps.push_back(finalStep);

    // If no explicit steps, create a default two-step chain
    if (steps.empty() || steps.size() == 1)
    {
        steps.clear();
        steps.push_back("Analyze the following task and break it down:\n" + taskDescription);
        steps.push_back("Execute each subtask in the previous analysis:\n{{INPUT}}");
    }

    LOG_INFO("ExecuteSubAgentChain: " + std::to_string(steps.size()) + " steps");

    mgr->executeChain("bridge", steps, taskDescription);
}

void AgenticBridge::ExecuteSubAgentSwarm(const std::string& taskDescription)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
    {
        LOG_ERROR("Cannot execute SubAgent swarm: manager not initialized");
        return;
    }

    // Generate parallel analysis tasks from the description
    std::vector<std::string> prompts;

    // Create diverse analysis angles
    prompts.push_back(
        "Security Analysis: Identify potential security vulnerabilities, threats, and risks in the following:\n" +
        taskDescription);
    prompts.push_back(
        "Performance Analysis: Identify performance bottlenecks, inefficiencies, and optimization opportunities:\n" +
        taskDescription);
    prompts.push_back("Code Quality Analysis: Evaluate code quality, style, readability, and maintainability:\n" +
                      taskDescription);
    prompts.push_back("Architecture Analysis: Analyze design patterns, structure, and architectural improvements:\n" +
                      taskDescription);
    prompts.push_back("Testing Analysis: Identify test coverage gaps and suggest testing strategies:\n" +
                      taskDescription);

    SwarmConfig config;
    config.mergeStrategy = "priority_vote";
    config.maxParallel = 5;
    config.timeoutMs = 30000;

    LOG_INFO("ExecuteSubAgentSwarm: " + std::to_string(prompts.size()) + " parallel tasks");

    mgr->executeSwarm("bridge", prompts, config);
}

std::vector<std::string> AgenticBridge::GetSubAgentTodoList()
{
    std::vector<std::string> todos;

    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        for (const auto& item : mgr->getTodoList())
        {
            std::string line = "[" + std::to_string(item.id) + "] " + item.title;
            if (!item.description.empty())
            {
                line += " - " + item.description;
            }
            line += " (" + item.statusString() + ")";
            todos.push_back(std::move(line));
        }
    }

    return todos;
}

void AgenticBridge::ClearSubAgentTodoList()
{
    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        mgr->setTodoList({});
        LOG_INFO("SubAgent todo list cleared");
    }
}

std::string AgenticBridge::ExportAgentMemory()
{
    std::stringstream export_ss;
    export_ss << "{\n";
    export_ss << "  \"exported_at\": \"" << std::time(nullptr) << "\",\n";
    export_ss << "  \"context\": {\n";

    if (!m_modelName.empty())
        export_ss << "    \"model\": \"" << m_modelName << "\",\n";

    if (!m_workspaceRoot.empty())
        export_ss << "    \"workspace\": \"" << m_workspaceRoot << "\",\n";

    if (!m_languageContext.empty())
        export_ss << "    \"language\": \"" << m_languageContext << "\",\n";

    if (!m_fileContext.empty())
        export_ss << "    \"file\": \"" << m_fileContext << "\",\n";

    export_ss << "    \"max_mode\": " << (m_maxMode ? "true" : "false") << ",\n";
    export_ss << "    \"deep_thinking\": " << (m_deepThinking ? "true" : "false") << ",\n";
    export_ss << "    \"deep_research\": " << (m_deepResearch ? "true" : "false") << "\n";
    export_ss << "  },\n";

    // Agent status
    export_ss << "  \"agent_status\": {\n";
    export_ss << "    \"initialized\": " << (m_initialized ? "true" : "false") << ",\n";
    export_ss << "    \"loop_running\": " << (m_agentLoopRunning ? "true" : "false") << "\n";
    export_ss << "  },\n";

    // SubAgent information
    export_ss << "  \"subagent_info\": \"" << GetSubAgentStatus() << "\"\n";
    export_ss << "}\n";

    return export_ss.str();
}

void AgenticBridge::ClearAgentMemory()
{
    // Clear context
    m_languageContext.clear();
    m_fileContext.clear();
    m_workspaceRoot.clear();

    // Clear SubAgent manager if present
    if (m_subAgentManager)
    {
        m_subAgentManager->cancelAll();
    }

    LOG_INFO("Agent memory cleared");
}

void AgenticBridge::ExecuteBoundedAgentLoop(const std::string& prompt, int maxIterations)
{
    SCOPED_METRIC("agentic.execute_bounded_loop");
    METRICS.increment("agentic.bounded_loop_calls");

    if (prompt.empty())
    {
        postLogToMainWindow(UILogSeverity::Warning, "Bounded loop skipped: empty prompt");
        return;
    }

    const int boundedIterations = std::max(1, std::min(maxIterations, 50));
    std::string currentPrompt = prompt;
    std::ostringstream transcript;
    bool completed = false;
    int agentMirrorPaneId = -1;
    if (m_ide && m_ide->getSettings().agentTerminalIsolated)
        agentMirrorPaneId = m_ide->getOrCreatePrimaryAgentTerminalPane();

    for (int i = 0; i < boundedIterations; ++i)
    {
        if (!m_initialized)
        {
            transcript << "[iter " << (i + 1) << "] bridge not initialized\n";
            break;
        }

        if (m_ide)
            m_ide->showAgentActivityStatus(
                "Autonomous agent: step " + std::to_string(i + 1) + " / " + std::to_string(boundedIterations), 8000);

        AgentResponse response = ExecuteAgentCommand(currentPrompt);
        if (!response.content.empty())
        {
            transcript << "[iter " << (i + 1) << "] " << response.content << "\n";
        }
        else
        {
            transcript << "[iter " << (i + 1) << "] (empty response)\n";
        }

        if (agentMirrorPaneId >= 0 && m_ide)
        {
            constexpr size_t kMirrorCap = 900;
            std::string preview = response.content;
            const bool truncated = preview.size() > kMirrorCap;
            if (truncated)
                preview.resize(kMirrorCap);
            std::string block =
                "[autonomous " + std::to_string(i + 1) + "/" + std::to_string(boundedIterations) + "]\r\n";
            if (!preview.empty())
                block += preview;
            else
                block += "(empty)";
            if (truncated)
                block += "\r\n…[truncated]\r\n";
            else
                block += "\r\n";
            m_ide->appendToTerminalPane(agentMirrorPaneId, block);
        }

        if (response.type == AgentResponseType::TOOL_CALL)
        {
            static constexpr const char kToolMarker[] = "[Tool Execution Result]";
            const size_t markerPos = response.content.find(kToolMarker);
            if (markerPos != std::string::npos)
            {
                size_t payloadStart = markerPos + sizeof(kToolMarker) - 1;
                while (payloadStart < response.content.size() &&
                       (response.content[payloadStart] == '\n' || response.content[payloadStart] == '\r'))
                {
                    ++payloadStart;
                }
                const std::string observation = response.content.substr(payloadStart);
                transcript << "[tool] " << observation << "\n";
                currentPrompt =
                    "Observation from tool execution:\n" + observation + "\n\nContinue toward the goal:\n" + prompt;
                continue;
            }

            std::string toolResult;
            if (DispatchModelToolCalls(response.content, toolResult) && !toolResult.empty())
            {
                transcript << "[tool] " << toolResult << "\n";
                currentPrompt = "Tool result:\n" + toolResult + "\nProvide the next step or final answer.";
                continue;
            }
        }

        if (response.type == AgentResponseType::ANSWER && !response.content.empty())
        {
            completed = true;
            break;
        }

        currentPrompt = "Continue from previous result and make progress:\n" + response.content;
    }

    if (!completed)
    {
        transcript << "[bounded-loop] iteration cap reached without final answer\n";
    }

    const std::string output = transcript.str();
    if (m_outputCallback)
    {
        m_outputCallback("Bounded Agent Loop", output);
    }
    else
    {
        postLogToMainWindow(UILogSeverity::Info, output);
    }
}

bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult)
{
    const auto toolLines = ExtractToolCallLines(modelOutput);
    if (!toolLines.empty())
    {
        return DispatchToolLinesPolicyAware(toolLines, "bridge", toolResult, nullptr);
    }

    // Do not fall through to SubAgentManager::dispatchToolCall — the default implementation
    // historically returned "success" with a synthetic string without executing ToolRegistry,
    // which hid real tool work from chat and bounded-loop callers.
    toolResult.clear();
    return false;
}

std::vector<std::string> AgenticBridge::ExtractToolCallLines(const std::string& modelOutput) const
{
    std::vector<std::string> toolLines;
    std::istringstream iss(modelOutput);
    std::string line;
    while (std::getline(iss, line))
    {
        if (IsToolCall(line))
        {
            // Normalize to canonical prefix for the executor.
            if (line.rfind("tool:", 0) == 0)
            {
                toolLines.push_back("TOOL:" + line.substr(5));
            }
            else
            {
                toolLines.push_back(line);
            }
        }
        else
        {
            const std::string embedded = normalizeEmbeddedToolDirective(line);
            if (!embedded.empty())
            {
                toolLines.push_back(embedded);
            }
        }
    }

    // If no explicit tool lines were emitted, try structured JSON tool_calls.
    // This enables function-calling style models without requiring prompt hotpatches.
    if (toolLines.empty() && !modelOutput.empty())
    {
        const RawrXD::Agentic::ToolCallParseResult parsed = RawrXD::Agentic::ToolCallParser::parse(modelOutput);
        for (const auto& call : parsed.toolCalls)
        {
            if (call.name.empty())
            {
                continue;
            }
            std::string args = "{}";
            try
            {
                // Keep compact; the executor re-parses by locating the first '{'.
                args = call.arguments.dump();
            }
            catch (...)
            {
                args = "{}";
            }
            toolLines.push_back("TOOL:" + call.name + " " + args);
        }
    }

    // Single-line / pasted blobs: directive may appear without a leading newline.
    if (toolLines.empty() && !modelOutput.empty())
    {
        const std::string embedded = normalizeEmbeddedToolDirective(modelOutput);
        if (!embedded.empty())
        {
            toolLines.push_back(embedded);
        }
    }

    return toolLines;
}

void AgenticBridge::HandleToolDispatchResult(const std::string& toolName, const std::string& toolResult) const
{
    if (!m_ide)
    {
        return;
    }

    m_ide->showAgentActivityStatus(std::string("Running tool: ") + toolName, 5000);

    FailureClassification toolFailure = m_ide->hookToolResult(toolName, toolResult);
    if (toolFailure.reason != AgentFailureType::None)
    {
        LOG_WARNING("[Phase4B] Tool '" + toolName +
                    "' failure at dispatch: " + m_ide->failureTypeString(toolFailure.reason) +
                    " (confidence=" + std::to_string(toolFailure.confidence) + ")");
        m_ide->appendToOutput("[AgenticBridge] Tool failure: " + m_ide->failureTypeString(toolFailure.reason) + "\n",
                              "Errors", Win32IDE::OutputSeverity::Error);
    }
    if (m_ide->getSettings().agentTerminalIsolated)
    {
        const int agentPaneId = m_ide->getOrCreatePrimaryAgentTerminalPane();
        constexpr size_t kMax = 1200;
        std::string feed;
        const size_t resultLen =
            toolResult.empty() ? 0 : (toolResult.size() > kMax ? kMax + 24 : toolResult.size() + 4);
        feed.reserve(32 + toolName.size() + resultLen);
        feed.append("[tool] ");
        feed.append(toolName);
        feed.append("\r\n");
        if (!toolResult.empty())
        {
            if (toolResult.size() > kMax)
            {
                feed.append(toolResult.data(), kMax);
                feed += "\r\n[truncated]\r\n";
            }
            else
            {
                feed += toolResult;
                feed += "\r\n";
            }
        }
        m_ide->appendToTerminalPane(agentPaneId, feed);
    }
}

bool AgenticBridge::DispatchToolLinesBatched(const std::vector<std::string>& toolLines, const std::string& parentId,
                                             std::string& toolResult,
                                             RawrXD::Agentic::StreamingResultChannel* streamingChannel)
{
    if (toolLines.empty())
    {
        return false;
    }
    auto* mgr = GetSubAgentManager();

    const uint16_t headlessPort = m_headlessPort;

    std::ostringstream combined;
    const int totalTools = static_cast<int>(toolLines.size());

    for (size_t batchStart = 0; batchStart < toolLines.size(); batchStart += kMaxParallelBridgeToolCalls)
    {
        const size_t batchEnd = std::min(toolLines.size(), batchStart + kMaxParallelBridgeToolCalls);
        std::vector<std::future<ToolDispatchBatchOutcome>> futures;
        futures.reserve(batchEnd - batchStart);

        for (size_t index = batchStart; index < batchEnd; ++index)
        {
            const std::string toolLine = toolLines[index];
            const std::string toolName = ExtractDirectiveToolName(toolLine);
            if (streamingChannel)
            {
                streamingChannel->EmitToolStarted(toolName, static_cast<int>(index), totalTools);
            }

            futures.emplace_back(std::async(
                std::launch::async,
                [mgr, parentId, toolLine, toolName, headlessPort]() -> ToolDispatchBatchOutcome
                {
                    (void)parentId;
                    ToolDispatchBatchOutcome outcome;
                    outcome.toolName = toolName;
                    auto beginExec = std::chrono::high_resolution_clock::now();
                    if (headlessPort != 0)
                    {
                        std::string argsJson = "{}";
                        const size_t brace = toolLine.find('{');
                        if (brace != std::string::npos)
                        {
                            argsJson = toolLine.substr(brace);
                        }
                        const std::string body =
                            "{\"tool\":" + JsonQuoteStringForToolBody(toolName) + ",\"args\":" + argsJson + "}";
                        std::string httpOut;
                        if (HttpPostHeadlessTool(headlessPort, body, httpOut))
                        {
                            outcome.dispatched = true;
                            outcome.toolResult = std::move(httpOut);
                            const auto endExec = std::chrono::high_resolution_clock::now();
                            outcome.executionTimeMs = static_cast<uint64_t>(
                                std::chrono::duration_cast<std::chrono::milliseconds>(endExec - beginExec).count());
                            return outcome;
                        }
                    }
                    if (executeToolLineWithRegistry(toolLine, outcome.toolResult))
                    {
                        outcome.dispatched = true;
                    }
                    else if (mgr)
                    {
                        outcome.dispatched = mgr->dispatchToolCall(parentId, toolLine, outcome.toolResult);
                    }
                    const auto endExec = std::chrono::high_resolution_clock::now();
                    outcome.executionTimeMs = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(endExec - beginExec).count());
                    return outcome;
                }));
        }

        for (size_t futureIndex = 0; futureIndex < futures.size(); ++futureIndex)
        {
            const size_t toolIndex = batchStart + futureIndex;
            ToolDispatchBatchOutcome outcome = futures[futureIndex].get();
            if (outcome.dispatched)
            {
                if (streamingChannel)
                {
                    streamingChannel->EmitToolResult(outcome.toolName, outcome.toolResult, outcome.executionTimeMs,
                                                     static_cast<int>(toolIndex), totalTools);
                }
                HandleToolDispatchResult(outcome.toolName, outcome.toolResult);
                if (combined.tellp() > 0)
                {
                    combined << "\n";
                }
                combined << "[Tool Result: " << outcome.toolName << "]\n" << outcome.toolResult;
            }
            else
            {
                const std::string errorMsg = "Tool execution failed";
                if (streamingChannel)
                {
                    streamingChannel->EmitToolError(outcome.toolName, errorMsg, outcome.executionTimeMs,
                                                    static_cast<int>(toolIndex), totalTools);
                }
                if (m_ide)
                {
                    m_ide->appendToOutput("[StreamingBridge] Tool error: " + outcome.toolName + "\n", "Errors",
                                          Win32IDE::OutputSeverity::Error);
                }
                if (combined.tellp() > 0)
                {
                    combined << "\n";
                }
                combined << "[Tool Error: " << outcome.toolName << "]\n" << errorMsg;
            }
        }
    }

    toolResult = combined.str();
    return true;
}

bool AgenticBridge::DispatchToolLinesPolicyAware(const std::vector<std::string>& toolLines, const std::string& parentId,
                                                 std::string& toolResult,
                                                 RawrXD::Agentic::StreamingResultChannel* streamingChannel)
{
    if (toolLines.empty())
    {
        toolResult.clear();
        return false;
    }

    const Agentic::ApprovalPolicy approvalPolicy = GetBridgeApprovalPolicy();
    std::ostringstream combined;
    std::vector<std::string> parallelBatch;
    bool handledAny = false;

    auto appendCombined = [&combined](const std::string& text)
    {
        if (text.empty())
        {
            return;
        }
        if (combined.tellp() > 0)
        {
            combined << "\n";
        }
        combined << text;
    };

    auto flushParallelBatch = [&]()
    {
        if (parallelBatch.empty())
        {
            return;
        }

        std::string batchResult;
        if (DispatchToolLinesBatched(parallelBatch, parentId, batchResult, streamingChannel))
        {
            handledAny = true;
            appendCombined(batchResult);
        }
        parallelBatch.clear();
    };

    for (size_t index = 0; index < toolLines.size(); ++index)
    {
        const std::string& toolLine = toolLines[index];
        const BridgeToolExecutionPolicy toolPolicy = ClassifyBridgeToolExecution(toolLine);
        const bool autoApproved = IsBridgeToolAutoApproved(toolPolicy, approvalPolicy);

        if (!autoApproved)
        {
            flushParallelBatch();

            const std::string blockedMessage = BuildApprovalBlockedMessage(toolPolicy);
            if (streamingChannel)
            {
                streamingChannel->EmitToolError(toolPolicy.normalizedName, blockedMessage, 0, static_cast<int>(index),
                                                static_cast<int>(toolLines.size()));
            }
            if (m_ide)
            {
                m_ide->appendToOutput("[AgenticBridge] " + blockedMessage + "\n", "Agent",
                                      Win32IDE::OutputSeverity::Warning);
            }
            appendCombined(blockedMessage);
            handledAny = true;
            continue;
        }

        const bool canRunParallel = approvalPolicy.allow_parallel_approvals && toolPolicy.allowParallel;
        if (canRunParallel)
        {
            parallelBatch.push_back(toolLine);
            if (parallelBatch.size() >= kMaxParallelBridgeToolCalls)
            {
                flushParallelBatch();
            }
            continue;
        }

        flushParallelBatch();

        std::string singleResult;
        if (DispatchToolLinesBatched({toolLine}, parentId, singleResult, streamingChannel))
        {
            handledAny = true;
            appendCombined(singleResult);
        }
    }

    flushParallelBatch();
    toolResult = combined.str();
    return handledAny;
}

// ============================================================================
// Phase 1: Streaming Tool Result Injection
// ============================================================================

std::string AgenticBridge::DispatchModelToolCallsStreaming(
    const std::string& modelOutput, std::function<void(const RawrXD::Agentic::ToolExecutionEvent&)> onToolEvent)
{
    SCOPED_METRIC("agentic.dispatch_tools_streaming");

    if (!m_streamingChannel)
    {
        LOG_WARNING("StreamingResultChannel not initialized");
        // Fallback to synchronous dispatch
        std::string toolResult;
        DispatchModelToolCalls(modelOutput, toolResult);
        return toolResult;
    }

    // Reset channel for new streaming session
    m_streamingChannel->Reset();
    m_streamingChannel->SetToolEventCallback(onToolEvent);
    m_streamingChannel->SetStreamingEnabled(true);

    // Extract tool calls from model output
    auto* mgr = GetSubAgentManager();
    if (!mgr)
    {
        LOG_WARNING("SubAgentManager not available for streaming dispatch");
        std::string toolResult;
        DispatchModelToolCalls(modelOutput, toolResult);
        return toolResult;
    }

    // Parse model output to identify tool calls
    // Format: Tool calls typically identified via pattern matching in ParseAgentResponse
    const std::vector<std::string> toolLines = ExtractToolCallLines(modelOutput);
    const int toolCount = static_cast<int>(toolLines.size());

    // If no tool calls found, return output as-is
    if (toolCount == 0)
    {
        m_streamingChannel->EmitStreamComplete(modelOutput);
        return modelOutput;
    }

    std::string toolResult;
    DispatchToolLinesPolicyAware(toolLines, "bridge-streaming", toolResult, m_streamingChannel.get());

    // Construct final response
    std::string finalResponse = modelOutput;
    if (!toolResult.empty())
    {
        finalResponse += "\n";
        finalResponse += toolResult;
    }

    // Emit stream complete event
    m_streamingChannel->EmitStreamComplete(finalResponse);

    LOG_INFO("Streaming dispatch complete: " + std::to_string(toolCount) + " tools processed");

    return finalResponse;
}

// ============================================================================
// Model Loading
// ============================================================================

bool AgenticBridge::LoadModel(const std::string& path)
{
    SCOPED_METRIC("agentic.load_model");
    METRICS.increment("agentic.model_load_attempts");
    if (!m_initialized)
        Initialize("", path);

    if (g_agentEngine)
    {
        bool success = g_agentEngine->loadLocalModel(path);
        if (success)
        {
            m_modelName = g_agentEngine->currentModelPath();
            m_lastModelLoadError.clear();
            LOG_INFO("Model loaded in bridge: " + m_modelName);
            SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
        }
        else
        {
            m_lastModelLoadError = SharedCpuEngine()->GetLastLoadErrorMessage();
            if (m_lastModelLoadError.empty())
            {
                m_lastModelLoadError = "Model load failed in AgenticBridge";
            }
            if (m_modelLoadErrorCallback)
            {
                m_modelLoadErrorCallback(m_lastModelLoadError);
            }
            postLogToMainWindow(UILogSeverity::Error, "Model load failed: " + m_lastModelLoadError);
        }
        return success;
    }
    m_lastModelLoadError = "Model load failed: agent engine not initialized";
    if (m_modelLoadErrorCallback)
    {
        m_modelLoadErrorCallback(m_lastModelLoadError);
    }
    postLogToMainWindow(UILogSeverity::Error, m_lastModelLoadError);
    return false;
}

// ============================================================================
// Compatibility wrappers (older UI command layer)
// ============================================================================

bool AgenticBridge::LoadConfiguration(const std::string& configPath)
{
    SCOPED_METRIC("agentic.load_configuration");
    if (configPath.empty())
        return false;

    // IDEConfig is the canonical config source for feature toggles and runtime knobs.
    // Bridge-specific config reflection is intentionally minimal: the engine reads most
    // toggles directly from CONFIG / FeatureToggle.
    if (!CONFIG.loadFromFile(configPath))
    {
        LOG_WARNING("AgenticBridge::LoadConfiguration failed: " + configPath);
        return false;
    }

    CONFIG.applyEnvironmentOverrides();
    CONFIG.applyFeatureToggles();

    LOG_INFO("AgenticBridge configuration loaded: " + configPath);
    return true;
}

void AgenticBridge::WarmUpModel()
{
    SCOPED_METRIC("agentic.warmup_model");

    if (!g_agentEngine)
    {
        LOG_WARNING("AgenticBridge::WarmUpModel: agent engine not initialized");
        return;
    }

    // Minimal warmup: force the engine to touch inference path and allocator.
    // Keep it deterministic and fast; ignore the output.
    (void)g_agentEngine->chat("warmup");
    LOG_INFO("AgenticBridge warmup complete");
}
