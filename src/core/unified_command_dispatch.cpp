// ============================================================================
// unified_command_dispatch.cpp — Implementation of Unified Dispatch System
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// This file provides:
//   1. Auto-registration bridge: COMMAND_TABLE → SharedFeatureRegistry
//   2. Win32 WM_COMMAND entry point replacement
//   3. CLI terminal entry point replacement
//   4. Extern "C" bridges for MASM hot-paths
//   5. Startup integrity verification
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "unified_command_dispatch.hpp"
#include "shared_feature_dispatch.h"
#include "command_registry.hpp"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <mutex>
#include <fstream>
#include <unordered_set>
#include <algorithm>
#include <sstream>

// ============================================================================
// AUTO-REGISTER FROM COMMAND_TABLE → SharedFeatureRegistry
// ============================================================================
// This bridge eliminates feature_registration.cpp entirely.
// SharedFeatureRegistry becomes a thin query layer over g_commandRegistry[].
// The runtime hash maps in SharedFeatureRegistry are populated from the
// compile-time table — zero manual maintenance.

namespace {

// Map CmdExposure → gui/cli booleans for FeatureDescriptor
inline void exposureToBools(CmdExposure exp, bool& gui, bool& cli) {
    switch (exp) {
        case CmdExposure::GUI_ONLY:  gui = true;  cli = false; break;
        case CmdExposure::CLI_ONLY:  gui = false; cli = true;  break;
        case CmdExposure::BOTH:      gui = true;  cli = true;  break;
        case CmdExposure::INTERNAL:  gui = false; cli = false; break;
    }
}

// Map category string → FeatureGroup enum (best-effort)
inline FeatureGroup categoryToGroup(const char* cat) {
    if (!cat) return FeatureGroup::Tools;
    if (std::strcmp(cat, "File") == 0)         return FeatureGroup::FileOps;
    if (std::strcmp(cat, "Edit") == 0)         return FeatureGroup::Editing;
    if (std::strcmp(cat, "View") == 0)         return FeatureGroup::View;
    if (std::strcmp(cat, "Terminal") == 0)      return FeatureGroup::Terminal;
    if (std::strcmp(cat, "Agent") == 0)         return FeatureGroup::Agent;
    if (std::strcmp(cat, "SubAgent") == 0)      return FeatureGroup::SubAgent;
    if (std::strcmp(cat, "Autonomy") == 0)      return FeatureGroup::Autonomy;
    if (std::strcmp(cat, "AIMode") == 0)        return FeatureGroup::AIMode;
    if (std::strcmp(cat, "AIContext") == 0)     return FeatureGroup::AIMode;
    if (std::strcmp(cat, "ReverseEng") == 0)    return FeatureGroup::ReverseEng;
    if (std::strcmp(cat, "Backend") == 0)       return FeatureGroup::LLMRouter;
    if (std::strcmp(cat, "Router") == 0)        return FeatureGroup::LLMRouter;
    if (std::strcmp(cat, "LSP") == 0)           return FeatureGroup::LSP;
    if (std::strcmp(cat, "LSPServer") == 0)     return FeatureGroup::LSP;
    if (std::strcmp(cat, "ASM") == 0)           return FeatureGroup::Decompiler;
    if (std::strcmp(cat, "Hybrid") == 0)        return FeatureGroup::LSP;
    if (std::strcmp(cat, "MultiResp") == 0)     return FeatureGroup::Agent;
    if (std::strcmp(cat, "Governor") == 0)      return FeatureGroup::Agent;
    if (std::strcmp(cat, "Safety") == 0)        return FeatureGroup::Security;
    if (std::strcmp(cat, "Replay") == 0)        return FeatureGroup::Session;
    if (std::strcmp(cat, "Confidence") == 0)    return FeatureGroup::Agent;
    if (std::strcmp(cat, "Swarm") == 0)         return FeatureGroup::Swarm;
    if (std::strcmp(cat, "Debug") == 0)         return FeatureGroup::Debug;
    if (std::strcmp(cat, "Plugin") == 0)        return FeatureGroup::Modules;
    if (std::strcmp(cat, "Hotpatch") == 0)      return FeatureGroup::Hotpatch;
    if (std::strcmp(cat, "Monaco") == 0)        return FeatureGroup::View;
    if (std::strcmp(cat, "Editor") == 0)        return FeatureGroup::Editing;
    if (std::strcmp(cat, "PDB") == 0)           return FeatureGroup::Debug;
    if (std::strcmp(cat, "Audit") == 0)         return FeatureGroup::Tools;
    if (std::strcmp(cat, "Gauntlet") == 0)      return FeatureGroup::Tools;
    if (std::strcmp(cat, "Voice") == 0)         return FeatureGroup::Voice;
    if (std::strcmp(cat, "QW") == 0)            return FeatureGroup::Tools;
    if (std::strcmp(cat, "Telemetry") == 0)     return FeatureGroup::Performance;
    if (std::strcmp(cat, "Theme") == 0)         return FeatureGroup::Themes;
    if (std::strcmp(cat, "Transparency") == 0)  return FeatureGroup::View;
    if (std::strcmp(cat, "Git") == 0)           return FeatureGroup::Git;
    if (std::strcmp(cat, "Help") == 0)          return FeatureGroup::Help;
    if (std::strcmp(cat, "CLI") == 0)           return FeatureGroup::Terminal;
    return FeatureGroup::Tools;
}

// ── Static initializer: runs before main() ──
// Populates SharedFeatureRegistry from g_commandRegistry[]
struct AutoRegistrar {
    AutoRegistrar() {
        auto& registry = SharedFeatureRegistry::instance();
        
        for (size_t i = 0; i < g_commandRegistrySize; ++i) {
            const auto& cmd = g_commandRegistry[i];
            
            FeatureDescriptor fd{};
            fd.id          = cmd.canonicalName;    // "file.new" etc.
            fd.name        = cmd.symbol;           // "FILE_NEW" etc.
            fd.description = cmd.canonicalName;    // Use canonical as description
            fd.group       = categoryToGroup(cmd.category);
            fd.commandId   = cmd.id;
            fd.cliCommand  = cmd.cliAlias;
            fd.shortcut    = "";                   // Shortcuts from accelerator table, not registry
            fd.handler     = cmd.handler;
            
            bool gui = false, cli = false;
            exposureToBools(cmd.exposure, gui, cli);
            fd.guiSupported = gui;
            fd.cliSupported = cli;
            fd.asmHotPath   = (cmd.flags & CMD_ASM_HOTPATH) != 0;
            
            registry.registerFeature(fd);
        }
        
        // Startup integrity check
        auto audit = RawrXD::Dispatch::auditRegistry();
        if (!audit.isClean) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[WARN] Command registry integrity: %zu null handlers, "
                "%zu duplicate IDs, %zu duplicate aliases",
                audit.nullHandlers, audit.duplicateIds, audit.duplicateAliases);
            // Log to stderr in debug builds
#ifdef _DEBUG
            std::fprintf(stderr, "%s\n", buf);
#endif
        }
    }
};

static AutoRegistrar s_autoRegistrar;

} // anonymous namespace

namespace RawrXD::Dispatch {

namespace {
std::mutex g_usageMutex;
std::vector<CommandUsageStat> g_usageStats;
bool g_cfgLoaded = false;
bool g_disableIncomplete = true;
bool g_telemetryEnabled = true;
std::unordered_set<std::string> g_disabledCanonical;
std::unordered_set<std::string> g_enabledCanonical;
std::unordered_set<std::string> g_disabledCategories;
std::unordered_set<std::string> g_incompleteCanonical;

CommandUsageStat* findUsage(uint32_t id) {
    for (auto& s : g_usageStats) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

std::string trimCopy(const std::string& text) {
    size_t b = 0;
    while (b < text.size() && static_cast<unsigned char>(text[b]) <= 32u) ++b;
    size_t e = text.size();
    while (e > b && static_cast<unsigned char>(text[e - 1]) <= 32u) --e;
    return text.substr(b, e - b);
}

void parseListIntoSet(const char* text, std::unordered_set<std::string>& out) {
    if (!text || !text[0]) return;
    std::string s(text);
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find_first_of(",;", start);
        if (end == std::string::npos) end = s.size();
        std::string item = trimCopy(s.substr(start, end - start));
        if (!item.empty()) out.insert(item);
        start = end + 1;
    }
}

void loadRuntimeConfigLocked() {
    if (g_cfgLoaded) return;

    // Default incomplete command set (override via config/env).

    char buf[8192] = {};

    DWORD n = GetEnvironmentVariableA("RAWRXD_COMMAND_TELEMETRY", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) {
        std::string v = trimCopy(buf);
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        g_telemetryEnabled = !(v == "0" || v == "off" || v == "false");
    }

    std::memset(buf, 0, sizeof(buf));
    n = GetEnvironmentVariableA("RAWRXD_DISABLE_INCOMPLETE_COMMANDS", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) {
        std::string v = trimCopy(buf);
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        g_disableIncomplete = !(v == "0" || v == "off" || v == "false");
    }

    std::memset(buf, 0, sizeof(buf));
    n = GetEnvironmentVariableA("RAWRXD_DISABLE_COMMANDS", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) parseListIntoSet(buf, g_disabledCanonical);

    std::memset(buf, 0, sizeof(buf));
    n = GetEnvironmentVariableA("RAWRXD_ENABLE_COMMANDS", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) parseListIntoSet(buf, g_enabledCanonical);

    std::memset(buf, 0, sizeof(buf));
    n = GetEnvironmentVariableA("RAWRXD_INCOMPLETE_COMMANDS", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) parseListIntoSet(buf, g_incompleteCanonical);

    std::memset(buf, 0, sizeof(buf));
    n = GetEnvironmentVariableA("RAWRXD_DISABLE_COMMAND_CATEGORIES", buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) parseListIntoSet(buf, g_disabledCategories);

    const char* configCandidates[] = {
        "config\\command_feature_flags.ini",
        ".rawrxd_command_feature_flags.ini"
    };

    for (const char* path : configCandidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in.good()) continue;

        std::string line;
        while (std::getline(in, line)) {
            line = trimCopy(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trimCopy(line.substr(0, eq));
            std::string value = trimCopy(line.substr(eq + 1));
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });

            if (key == "disable_incomplete") {
                std::string v = value;
                std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
                g_disableIncomplete = !(v == "0" || v == "off" || v == "false");
            } else if (key == "telemetry") {
                std::string v = value;
                std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
                g_telemetryEnabled = !(v == "0" || v == "off" || v == "false");
            } else if (key == "disable_command") {
                if (!value.empty()) g_disabledCanonical.insert(value);
            } else if (key == "enable_command") {
                if (!value.empty()) g_enabledCanonical.insert(value);
            } else if (key == "disable_category") {
                if (!value.empty()) g_disabledCategories.insert(value);
            } else if (key == "incomplete_command") {
                if (!value.empty()) g_incompleteCanonical.insert(value);
            }
        }
    }

    g_cfgLoaded = true;
}
} // namespace

bool isCommandEnabledRuntime(const CmdDescriptor& cmd, const char** reasonOut) {
    std::lock_guard<std::mutex> lock(g_usageMutex);
    loadRuntimeConfigLocked();

    static thread_local std::string reason;
    reason.clear();

    if (cmd.handler == nullptr) {
        reason = "handler not linked";
        if (reasonOut) *reasonOut = reason.c_str();
        return false;
    }

    if (g_enabledCanonical.find(cmd.canonicalName) != g_enabledCanonical.end()) {
        if (reasonOut) *reasonOut = nullptr;
        return true;
    }

    if (g_disabledCanonical.find(cmd.canonicalName) != g_disabledCanonical.end()) {
        reason = std::string("disabled by runtime flag: ") + cmd.canonicalName;
        if (reasonOut) *reasonOut = reason.c_str();
        return false;
    }

    if (g_disabledCategories.find(cmd.category) != g_disabledCategories.end()) {
        reason = std::string("category disabled by runtime flag: ") + cmd.category;
        if (reasonOut) *reasonOut = reason.c_str();
        return false;
    }

    if (g_disableIncomplete && g_incompleteCanonical.find(cmd.canonicalName) != g_incompleteCanonical.end()) {
        reason = std::string("command flagged incomplete: ") + cmd.canonicalName;
        if (reasonOut) *reasonOut = reason.c_str();
        return false;
    }

    if (reasonOut) *reasonOut = nullptr;
    return true;
}

void recordCommandUsage(const CmdDescriptor* cmd, DispatchStatus status, const char* source) {
    (void)source;
    if (!cmd) return;
    std::lock_guard<std::mutex> lock(g_usageMutex);
    loadRuntimeConfigLocked();
    if (!g_telemetryEnabled) return;

    CommandUsageStat* stat = findUsage(cmd->id);
    if (!stat) {
        CommandUsageStat init{};
        init.id = cmd->id;
        init.canonicalName = cmd->canonicalName;
        init.handlerName = cmd->handlerName;
        init.category = cmd->category;
        g_usageStats.push_back(init);
        stat = &g_usageStats.back();
    }

    stat->attempts++;
    if (status == DispatchStatus::OK) stat->okCount++;
    else stat->errorCount++;
    stat->lastStatus = status;
    stat->lastTickMs = GetTickCount64();
}

void resetCommandUsage() {
    std::lock_guard<std::mutex> lock(g_usageMutex);
    g_usageStats.clear();
}

size_t getCommandUsageStats(CommandUsageStat* out, size_t maxOut) {
    if (!out || maxOut == 0) return 0;
    std::lock_guard<std::mutex> lock(g_usageMutex);
    size_t n = (g_usageStats.size() < maxOut) ? g_usageStats.size() : maxOut;
    for (size_t i = 0; i < n; ++i) out[i] = g_usageStats[i];
    return n;
}

bool exportCommandUsageJson(const char* path) {
    if (!path || path[0] == '\0') return false;
    std::lock_guard<std::mutex> lock(g_usageMutex);
    loadRuntimeConfigLocked();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    f << "{\n";
    f << "  \"telemetryEnabled\": " << (g_telemetryEnabled ? "true" : "false") << ",\n";
    f << "  \"disableIncomplete\": " << (g_disableIncomplete ? "true" : "false") << ",\n";
    f << "  \"usage\": [\n";
    for (size_t i = 0; i < g_usageStats.size(); ++i) {
        const auto& s = g_usageStats[i];
        f << "    {\"id\":" << s.id
          << ",\"canonical\":\"" << (s.canonicalName ? s.canonicalName : "")
          << "\",\"handler\":\"" << (s.handlerName ? s.handlerName : "")
          << "\",\"category\":\"" << (s.category ? s.category : "")
          << "\",\"attempts\":" << s.attempts
          << ",\"ok\":" << s.okCount
          << ",\"error\":" << s.errorCount
          << ",\"lastTickMs\":" << s.lastTickMs
          << ",\"lastStatus\":" << static_cast<unsigned>(s.lastStatus)
          << "}" << (i + 1 < g_usageStats.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

bool exportCommandMapMarkdown(const char* path, const char* proofTag) {
    if (!path || path[0] == '\0') return false;

    std::vector<CommandUsageStat> usageSnapshot;
    {
        std::lock_guard<std::mutex> lock(g_usageMutex);
        loadRuntimeConfigLocked();
        usageSnapshot = g_usageStats;
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    f << "# Command Map\n\n";
    if (proofTag && proofTag[0] != '\0') f << "Proof baseline: " << proofTag << "\n\n";
    f << "| cmdId | canonical | handler | category | enabled | attempts | proof note |\n";
    f << "|---:|---|---|---|---|---:|---|\n";

    const uint32_t selftestIds[] = {1002u, 2028u, 3200u, 4009u, 10000u};

    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        const auto& c = g_commandRegistry[i];

        const char* reason = nullptr;
        const bool enabled = isCommandEnabledRuntime(c, &reason);

        uint64_t attempts = 0;
        for (const auto& s : usageSnapshot) {
            if (s.id == c.id) {
                attempts = s.attempts;
                break;
            }
        }

        bool selftestCovered = false;
        for (uint32_t id : selftestIds) {
            if (id == c.id) {
                selftestCovered = true;
                break;
            }
        }

        std::string proof = "Registry wired + unified dispatch path";
        if (attempts > 0) proof = "Observed in runtime telemetry";
        else if (selftestCovered) proof = "Covered by --selftest dispatch probe";
        else if (!enabled && reason) proof = reason;

        f << "| " << c.id
          << " | " << c.canonicalName
          << " | " << (c.handlerName ? c.handlerName : "")
          << " | " << c.category
          << " | " << (enabled ? "yes" : "no")
          << " | " << attempts
          << " | " << proof
          << " |\n";
    }
    return true;
}

} // namespace RawrXD::Dispatch


// ============================================================================
// EXTERN "C" BRIDGE — For MASM hot-paths and cross-module dispatch
// ============================================================================

extern "C" {

int rawrxd_dispatch_feature(const char* featureId, const char* args, void* idePtr) {
    CommandContext ctx{};
    ctx.rawInput = featureId;
    ctx.args     = args;
    ctx.idePtr   = idePtr;
    ctx.isGui    = (idePtr != nullptr);
    ctx.isHeadless = false;
    
    auto result = RawrXD::Dispatch::dispatchByCanonical(featureId, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_dispatch_command(uint32_t commandId, void* idePtr) {
    CommandContext ctx{};
    ctx.commandId = commandId;
    ctx.idePtr    = idePtr;
    ctx.isGui     = true;
    ctx.isHeadless = false;
    
    auto result = RawrXD::Dispatch::dispatchByGuiId(commandId, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_dispatch_cli(const char* cliCommand, const char* args, void* cliStatePtr) {
    CommandContext ctx{};
    ctx.rawInput    = cliCommand;
    ctx.args        = args;
    ctx.cliStatePtr = cliStatePtr;
    ctx.isGui       = false;
    ctx.isHeadless  = false;
    
    auto result = RawrXD::Dispatch::dispatchByCli(cliCommand, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_get_feature_count(void) {
    return static_cast<int>(g_commandRegistrySize);
}

int rawrxd_audit_registry(void) {
    auto audit = RawrXD::Dispatch::auditRegistry();
    return audit.isClean ? 1 : 0;
}

} // extern "C"
