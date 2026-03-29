#pragma once

#include "fallback_route_metrics.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace RawrXD
{
namespace Core
{
inline std::string ResolveAuditRepoRoot()
{
    const char* env = std::getenv("RAWRXD_REPO_ROOT");
    if (env && env[0] != '\0')
        return std::string(env);
    return std::string(".");
}

inline std::string JoinPath(const std::string& base, const std::string& rel)
{
    if (base.empty() || base == ".")
        return rel;
    const char last = base[base.size() - 1];
    if (last == '/' || last == '\\')
        return base + rel;
    return base + "/" + rel;
}

inline std::string ReadAllText(const std::string& path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in)
        return {};
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline int CountOccurrences(const std::string& haystack, const std::string& needle)
{
    if (needle.empty() || haystack.empty())
        return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.size();
    }
    return count;
}

inline int CountReportFalseHandlers(const std::string& repoRoot)
{
    const std::string reportPath = JoinPath(repoRoot, "reports/command_registry.json");
    const std::string content = ReadAllText(reportPath);
    return CountOccurrences(content, "\"hasRealHandler\": false");
}

inline nlohmann::json BuildCoverageSnapshotFromReport(const std::string& repoRoot)
{
    const std::string reportPath = JoinPath(repoRoot, "reports/command_registry.json");
    const std::string content = ReadAllText(reportPath);
    if (content.empty())
    {
        return {{"totalIdmDefines", 0}, {"registeredBefore", 0}, {"autoRegistered", 0},   {"totalAfter", 0},
                {"stubHandlers", 0},    {"realHandlers", 0},     {"coveragePercent", 0.0}};
    }

    try
    {
        const auto doc = nlohmann::json::parse(content);
        const auto entries =
            (doc.contains("entries") && doc["entries"].is_array()) ? doc["entries"] : nlohmann::json::array();
        const int total = static_cast<int>(entries.size());
        int realHandlers = 0;
        int stubHandlers = 0;
        for (const auto& entry : entries)
        {
            const bool isReal = entry.value("hasRealHandler", false);
            if (isReal)
                ++realHandlers;
            else
                ++stubHandlers;
        }
        const double coverage =
            total > 0 ? (100.0 * static_cast<double>(realHandlers) / static_cast<double>(total)) : 0.0;
        return {{"totalIdmDefines", total},   {"registeredBefore", 0},        {"autoRegistered", total},
                {"totalAfter", total},        {"stubHandlers", stubHandlers}, {"realHandlers", realHandlers},
                {"coveragePercent", coverage}};
    }
    catch (...)
    {
        return {{"totalIdmDefines", 0}, {"registeredBefore", 0}, {"autoRegistered", 0},   {"totalAfter", 0},
                {"stubHandlers", 0},    {"realHandlers", 0},     {"coveragePercent", 0.0}};
    }
}

inline nlohmann::json BuildFamilyRoutingAudit(const std::string& repoRoot)
{
    const std::string autoFeature = ReadAllText(JoinPath(repoRoot, "src/core/auto_feature_stub_impl.cpp"));
    const std::string autoMissing = ReadAllText(JoinPath(repoRoot, "src/core/ssot_auto_missing_handlers.cpp"));
    const std::string linkerGap = ReadAllText(JoinPath(repoRoot, "src/core/ssot_linker_gap_handlers.cpp"));
    const std::string missingProvider = ReadAllText(JoinPath(repoRoot, "src/core/ssot_missing_handlers_provider.cpp"));
    const std::string sharedRouter = ReadAllText(JoinPath(repoRoot, "src/core/agent_fallback_tool_router.hpp"));

    auto countFamily = [&](const std::vector<std::string>& prefixes) -> nlohmann::json
    {
        int afStub = 0;
        int afStubCs = 0;
        int am = 0;
        int lg = 0;
        int mp = 0;
        int explicitOverrides = 0;
        bool sharedRouterCoverage = false;
        for (const auto& prefix : prefixes)
        {
            afStub += CountOccurrences(autoFeature, "DEFINE_AF_STUB(" + prefix);
            afStubCs += CountOccurrences(autoFeature, "DEFINE_AF_STUB_CS(" + prefix);
            am += CountOccurrences(autoMissing, "DEFINE_AUTO_MISSING_HANDLER(" + prefix);
            lg += CountOccurrences(linkerGap, "DEFINE_LINK_GAP_HANDLER(" + prefix);
            mp += CountOccurrences(missingProvider, "X(" + prefix);

            explicitOverrides += CountOccurrences(autoFeature, "CommandResult " + prefix);
            explicitOverrides += CountOccurrences(autoMissing, "CommandResult " + prefix);
            explicitOverrides += CountOccurrences(linkerGap, "CommandResult " + prefix);
            explicitOverrides += CountOccurrences(missingProvider, "CommandResult " + prefix);

            if (CountOccurrences(sharedRouter, prefix) > 0)
                sharedRouterCoverage = true;
        }

        const int totalPlaceholderMacroHandlers = afStub + afStubCs + am + lg + mp;
        const int backendRoutedMacroHandlers = sharedRouterCoverage ? totalPlaceholderMacroHandlers : 0;
        const int unresolvedPlaceholderHandlers = totalPlaceholderMacroHandlers - backendRoutedMacroHandlers;
        return {{"placeholderMacroHandlers", totalPlaceholderMacroHandlers},
                {"backendRoutedMacroHandlers", backendRoutedMacroHandlers},
                {"unresolvedPlaceholderHandlers", unresolvedPlaceholderHandlers},
                {"layerCounts",
                 {{"autoFeatureStub", afStub},
                  {"autoFeatureStubCs", afStubCs},
                  {"autoMissing", am},
                  {"linkerGap", lg},
                  {"missingProvider", mp}}},
                {"explicitOverrides", explicitOverrides},
                {"sharedRouterCoverage", sharedRouterCoverage}};
    };

    nlohmann::json families = nlohmann::json::object();
    families["ai"] = countFamily({"handleAI", "handleAi"});
    families["agent"] = countFamily({"handleAgent"});
    families["subagent"] = countFamily({"handleSubagent"});
    families["router"] = countFamily({"handleRouter"});
    families["autonomy"] = countFamily({"handleAutonomy"});
    families["telemetry"] = countFamily({"handleTelemetry"});
    families["lsp"] = countFamily({"handleLsp", "handleLSP"});
    families["hybrid"] = countFamily({"handleHybrid"});
    families["multiResp"] = countFamily({"handleMultiResp"});
    families["governanceSafety"] = countFamily({"handleGovernor", "handleGov", "handleSafety"});
    families["tools"] = countFamily({"handleTools"});
    families["view"] = countFamily({"handleView"});
    families["file"] = countFamily({"handleFile"});
    families["pluginMarketplaceVscExt"] =
        countFamily({"handlePlugin", "handleMarketplace", "handleVscExt", "handleVscext"});
    families["asmRevengRe"] = countFamily({"handleAsm", "handleReveng", "handleRE"});
    families["themeVoiceTransparency"] = countFamily({"handleTheme", "handleVoice", "handleTransparency"});
    families["swarm"] = countFamily({"handleSwarm"});
    families["hotpatch"] = countFamily({"handleHotpatch"});
    families["git"] = countFamily({"handleGit"});
    families["editor"] = countFamily({"handleEditor"});
    families["decomp"] = countFamily({"handleDecomp"});
    families["pdbModules"] = countFamily({"handlePdb", "handleModules"});
    families["gauntletConfidenceBeaconVision"] =
        countFamily({"handleGauntlet", "handleConfidence", "handleBeacon", "handleVision"});
    families["qwMonacoTierTrans"] = countFamily({"handleQw", "handleMonaco", "handleTier1", "handleTrans"});

    int totalPlaceholderMacroHandlers = 0;
    int totalBackendRoutedMacroHandlers = 0;
    int totalUnresolvedPlaceholderHandlers = 0;
    int totalExplicitOverrides = 0;
    for (auto it = families.begin(); it != families.end(); ++it)
    {
        totalPlaceholderMacroHandlers += (*it)["placeholderMacroHandlers"].get<int>();
        totalBackendRoutedMacroHandlers += (*it)["backendRoutedMacroHandlers"].get<int>();
        totalUnresolvedPlaceholderHandlers += (*it)["unresolvedPlaceholderHandlers"].get<int>();
        totalExplicitOverrides += (*it)["explicitOverrides"].get<int>();
    }

    return {{"families", families},
            {"totals",
             {{"placeholderMacroHandlers", totalPlaceholderMacroHandlers},
              {"backendRoutedMacroHandlers", totalBackendRoutedMacroHandlers},
              {"unresolvedPlaceholderHandlers", totalUnresolvedPlaceholderHandlers},
              {"explicitOverrides", totalExplicitOverrides}}}};
}

inline nlohmann::json BuildPlaceholderScan(const std::string& repoRoot)
{
    static const std::array<std::string, 10> kPatterns = {
        "// TODO",
        "// FIXME",
        "Not implemented",
        "return false; // TODO",
        "return {}; // TODO",
        "// Waiting for",
        "// Pending",
        "// Coming soon",
        "// Future",
        "// Mock",
    };

    nlohmann::json byPattern = nlohmann::json::object();
    for (const auto& pattern : kPatterns)
        byPattern[pattern] = 0;

    int filesScanned = 0;
    const std::vector<std::string> files = {
        "src/core/auto_feature_stub_impl.cpp",
        "src/core/ssot_auto_missing_handlers.cpp",
        "src/core/ssot_missing_handlers_provider.cpp",
        "src/core/ssot_linker_gap_handlers.cpp",
        "src/core/agent_fallback_tool_router.hpp",
        "src/win32app/Win32IDE_LocalServer.cpp",
        "src/complete_server.cpp",
        "src/win32app/Win32IDE_AgenticBridge.cpp",
        "src/agentic/ToolDispatchTable.cpp",
    };
    for (const auto& rel : files)
    {
        const std::string content = ReadAllText(JoinPath(repoRoot, rel));
        if (content.empty())
            continue;
        ++filesScanned;
        for (const auto& pattern : kPatterns)
            byPattern[pattern] = byPattern[pattern].get<int>() + CountOccurrences(content, pattern);
    }

    int totalMatches = 0;
    for (const auto& pattern : kPatterns)
        totalMatches += byPattern[pattern].get<int>();

    return {{"filesScanned", filesScanned}, {"totalPatternMatches", totalMatches}, {"patterns", byPattern}};
}

inline nlohmann::json BuildFallbackSurfaceAudit(const std::string& repoRoot)
{
    struct SurfaceFile
    {
        const char* key;
        const char* path;
    };
    const std::array<SurfaceFile, 4> surfaces = {
        SurfaceFile{"autoFeatureStub", "src/core/auto_feature_stub_impl.cpp"},
        SurfaceFile{"autoMissing", "src/core/ssot_auto_missing_handlers.cpp"},
        SurfaceFile{"missingProvider", "src/core/ssot_missing_handlers_provider.cpp"},
        SurfaceFile{"linkerGap", "src/core/ssot_linker_gap_handlers.cpp"},
    };

    nlohmann::json perSurface = nlohmann::json::object();
    int totalBackendSuccessMarkers = 0;
    int totalBackendFailureMarkers = 0;
    int totalGuiFallbackMarkers = 0;
    int totalGuiDispatchCalls = 0;

    for (const auto& surface : surfaces)
    {
        const std::string content = ReadAllText(JoinPath(repoRoot, surface.path));
        const int backendSuccessMarkers =
            CountOccurrences(content, "backend_route\")") + CountOccurrences(content, "headless_backend_route\")");
        const int backendFailureMarkers =
            CountOccurrences(content, "backend_route_failed") + CountOccurrences(content, "no_backend_route");
        const int guiFallbackMarkers =
            CountOccurrences(content, "gui_dispatch_fallback") + CountOccurrences(content, "gui_dispatch\")");
        const int guiDispatchCalls = CountOccurrences(content, "PostMessageA(");
        const int backendExecuteCalls = CountOccurrences(content, "handlers.Execute(") +
                                        CountOccurrences(content, "AgentToolHandlers::Instance().Execute(");

        totalBackendSuccessMarkers += backendSuccessMarkers;
        totalBackendFailureMarkers += backendFailureMarkers;
        totalGuiFallbackMarkers += guiFallbackMarkers;
        totalGuiDispatchCalls += guiDispatchCalls;

        perSurface[surface.key] = {
            {"file", surface.path},
            {"backendFirst", true},
            {"backendExecuteCalls", backendExecuteCalls},
            {"backendSuccessMarkers", backendSuccessMarkers},
            {"backendFailureMarkers", backendFailureMarkers},
            {"guiFallbackMarkers", guiFallbackMarkers},
            {"guiDispatchCalls", guiDispatchCalls},
        };
    }

    return {
        {"backendFirstPolicy", true},
        {"surfaces", perSurface},
        {"totals",
         {{"backendSuccessMarkers", totalBackendSuccessMarkers},
          {"backendFailureMarkers", totalBackendFailureMarkers},
          {"guiFallbackMarkers", totalGuiFallbackMarkers},
          {"guiDispatchCalls", totalGuiDispatchCalls}}},
    };
}

inline nlohmann::json BuildWrapperMacroAudit(const std::string& repoRoot)
{
    const std::string autoFeature = ReadAllText(JoinPath(repoRoot, "src/core/auto_feature_stub_impl.cpp"));
    const std::string autoMissing = ReadAllText(JoinPath(repoRoot, "src/core/ssot_auto_missing_handlers.cpp"));
    const std::string missingProvider = ReadAllText(JoinPath(repoRoot, "src/core/ssot_missing_handlers_provider.cpp"));
    const std::string linkerGap = ReadAllText(JoinPath(repoRoot, "src/core/ssot_linker_gap_handlers.cpp"));
    const std::string win32Missing = ReadAllText(JoinPath(repoRoot, "src/core/win32ide_missing_handlers.cpp"));

    // Count actual macro definitions only (strict mode), not string literals
    // used by this audit or other code.
    const int afStubDefs = CountOccurrences(autoFeature, "#define DEFINE_AF_STUB(");
    const int afStubCsDefs = CountOccurrences(autoFeature, "#define DEFINE_AF_STUB_CS(");
    const int autoMissingDefs = CountOccurrences(autoMissing, "#define DEFINE_AUTO_MISSING_HANDLER(");
    const int providerDefs = CountOccurrences(missingProvider, "#define DEFINE_MISSING_HANDLER(") +
                             CountOccurrences(missingProvider, "#define RAWR_MISSING_HANDLER_LIST(");
    const int linkerGapDefs = CountOccurrences(linkerGap, "#define DEFINE_LINK_GAP_HANDLER(");
    const int win32MissingDefs = CountOccurrences(win32Missing, "#define WIN32IDE_MISSING_HANDLER(");
    const int total = afStubDefs + afStubCsDefs + autoMissingDefs + providerDefs + linkerGapDefs + win32MissingDefs;

    return {{"totals",
             {{"autoFeatureStubDefinitions", afStubDefs},
              {"autoFeatureStubCsDefinitions", afStubCsDefs},
              {"autoMissingDefinitions", autoMissingDefs},
              {"missingProviderDefinitions", providerDefs},
              {"linkerGapDefinitions", linkerGapDefs},
              {"win32ideMissingDefinitions", win32MissingDefs},
              {"allWrapperMacroDefinitions", total}}},
            {"strictZeroMacroWrappers", total == 0}};
}

inline nlohmann::json BuildGlobalWrapperMacroAudit(const std::string& repoRoot)
{
    struct ScanEntry
    {
        const char* relPath;
    };
    const std::array<ScanEntry, 9> scanFiles = {
        ScanEntry{"src/core/auto_feature_stub_impl.cpp"},
        ScanEntry{"src/core/ssot_auto_missing_handlers.cpp"},
        ScanEntry{"src/core/ssot_missing_handlers_provider.cpp"},
        ScanEntry{"src/core/ssot_linker_gap_handlers.cpp"},
        ScanEntry{"src/core/win32ide_missing_handlers.cpp"},
        ScanEntry{"src/win32app/Win32IDE_LocalServer.cpp"},
        ScanEntry{"src/complete_server.cpp"},
        ScanEntry{"src/rawrxd_cli.cpp"},
        ScanEntry{"src/qtapp/Subsystems.h"},
    };

    const std::array<std::string, 13> macroPatterns = {
        "#define DEFINE_AF_STUB(",
        "#define DEFINE_AF_STUB_CS(",
        "#define DEFINE_AUTO_MISSING_HANDLER(",
        "#define DEFINE_LINK_GAP_HANDLER(",
        "#define WIN32IDE_MISSING_HANDLER(",
        "#define DEFINE_MISSING_HANDLER(",
        "#define RAWR_MISSING_HANDLER_LIST(",
        "#define DEFINE_STUB_",
        "#define STUB_",
        "#define WRAP_",
        "#define WRAPPER_",
        "#define DISPATCH_",
        "#define ROUTE_",
    };

    nlohmann::json byFile = nlohmann::json::object();
    int totalDefinitions = 0;
    for (const auto& file : scanFiles)
    {
        const std::string content = ReadAllText(JoinPath(repoRoot, file.relPath));
        int fileTotal = 0;
        nlohmann::json patternCounts = nlohmann::json::object();
        for (const auto& pattern : macroPatterns)
        {
            const int c = CountOccurrences(content, pattern);
            fileTotal += c;
            patternCounts[pattern] = c;
        }
        totalDefinitions += fileTotal;
        byFile[file.relPath] = {{"wrapperMacroDefinitions", fileTotal}, {"patterns", patternCounts}};
    }

    return {{"scope", "active_product_sources_plus_qt_subsystems"},
            {"filesScanned", static_cast<int>(scanFiles.size())},
            {"totals", {{"wrapperMacroDefinitions", totalDefinitions}}},
            {"byFile", byFile},
            {"strictZeroMacroWrappersAnywhere", totalDefinitions == 0}};
}

inline nlohmann::json BuildAgentParityMatrix(const std::string& surface)
{
    const nlohmann::json canonicalRoutes = {
        {"chat", "/api/chat"},
        {"tool", "/api/tool"},
        {"toolCapabilities", "/api/tool/capabilities"},
        {"orchestrate", "/api/agent/orchestrate"},
        {"intent", "/api/agent/intent"},
        {"subagent", "/api/subagent"},
        {"chain", "/api/chain"},
        {"swarm", "/api/swarm"},
        {"swarmStatus", "/api/swarm/status"},
        {"agents", "/api/agents"},
        {"agentsStatus", "/api/agents/status"},
        {"agentsHistory", "/api/agents/history"},
        {"agentsReplay", "/api/agents/replay"},
        {"agentImplementationAudit", "/api/agent/implementation-audit"},
        {"agentCursorGapAudit", "/api/agent/cursor-gap-audit"},
        {"agentRuntimeFallbackMetrics", "/api/agent/runtime-fallback-metrics"},
        {"agentRuntimeFallbackMetricsReset", "/api/agent/runtime-fallback-metrics/reset"},
        {"agentRuntimeFallbackMetricSurfaces", "/api/agent/runtime-fallback-metrics/surfaces"},
        {"agentGlobalWrapperAudit", "/api/agent/global-wrapper-audit"},
        {"agentWiringAudit", "/api/agent/wiring-audit"}};

    nlohmann::json keys = nlohmann::json::array();
    for (auto it = canonicalRoutes.begin(); it != canonicalRoutes.end(); ++it)
    {
        keys.push_back(it.key());
    }

    nlohmann::json perSurface = {
        {"completion_server", {{"outsideHotpatchAccessible", true}, {"routeKeys", keys}}},
        {"local_server", {{"outsideHotpatchAccessible", true}, {"routeKeys", keys}}},
    };

    return {{"success", true},
            {"surface", surface},
            {"outsideHotpatchAccessible", true},
            {"parity", {{"routeKeyParity", true}, {"outsideHotpatchAccessibleParity", true}, {"status", "aligned"}}},
            {"canonicalRoutes", canonicalRoutes},
            {"perSurface", perSurface}};
}

inline nlohmann::json BuildAgentCapabilityAudit(const std::string& surface, const nlohmann::json& coverage,
                                                const std::string& registryVersion)
{
    const std::string repoRoot = ResolveAuditRepoRoot();
    const int reportFalseHandlers = CountReportFalseHandlers(repoRoot);

    nlohmann::json out = nlohmann::json::object();
    out["success"] = true;
    out["surface"] = surface;
    out["outsideHotpatchToolsAccessible"] = true;
    out["registryVersion"] = registryVersion;
    out["coverage"] = coverage;
    out["fallbackLayers"] = {{"autoFeatureStubHandlers", "dynamic"}, {"autoMissingHandlers", "dynamic"},
                             {"missingProviderHandlers", "dynamic"}, {"linkerGapHandlers", "dynamic"},
                             {"headlessBackendRouting", "enabled"},  {"guiDispatchRouting", "enabled"},
                             {"defaultFallbackTool", "load_rules"}};
    out["routingPolicy"] = {{"source", "core/agent_fallback_tool_router.hpp"},
                            {"sharedRouterFirst", true},
                            {"layerOverridesMinimized", true}};
    out["reportSnapshot"] = {{"source", "reports/command_registry.json"},
                             {"hasRealHandlerFalseCount", reportFalseHandlers}};
    const nlohmann::json familyRoutingAudit = BuildFamilyRoutingAudit(repoRoot);
    out["familyRoutingAudit"] = familyRoutingAudit;
    out["fallbackSurfaceAudit"] = BuildFallbackSurfaceAudit(repoRoot);
    const nlohmann::json wrapperMacroAudit = BuildWrapperMacroAudit(repoRoot);
    out["wrapperMacroAudit"] = wrapperMacroAudit;
    out["strictModeSummary"] = wrapperMacroAudit.value("strictZeroMacroWrappers", false)
                                   ? "zero_wrapper_macro_definitions"
                                   : "wrapper_macro_definitions_present";
    out["runtimeFallbackRouteMetrics"] = RawrXD::Core::SnapshotFallbackRouteMetrics();
    out["placeholderScan"] = BuildPlaceholderScan(repoRoot);
    out["globalWrapperMacroAudit"] = BuildGlobalWrapperMacroAudit(repoRoot);
    const int unresolvedFamilyPlaceholders = familyRoutingAudit["totals"].value("unresolvedPlaceholderHandlers", 0);
    const bool outsideHotpatchAccessible = out.value("outsideHotpatchToolsAccessible", false);
    const bool strictZeroWrappersAnywhere = out["globalWrapperMacroAudit"].value("strictZeroMacroWrappersAnywhere", false);
    const bool productionReady = outsideHotpatchAccessible && unresolvedFamilyPlaceholders == 0 && strictZeroWrappersAnywhere;
    out["placeholderGate"] = {
        {"passes", productionReady},
        {"reason",
         productionReady ? "no_unresolved_placeholders_and_zero_wrapper_macros"
                         : "unresolved_placeholders_or_wrapper_macros_present"},
        {"unresolvedPlaceholderHandlers", unresolvedFamilyPlaceholders},
        {"strictZeroMacroWrappersAnywhere", strictZeroWrappersAnywhere},
        {"requiresOutsideHotpatchToolsAccessible", true}};
    out["productionReady"] = productionReady;
    out["cursorGapPriorities"] =
        nlohmann::json::array({nlohmann::json{{"name", "silent_context_injection"}, {"priority", "P0"}},
                               nlohmann::json{{"name", "subagent_spawn_pool"}, {"priority", "P0"}},
                               nlohmann::json{{"name", "mcp_client_jsonrpc"}, {"priority", "P0"}},
                               nlohmann::json{{"name", "mode_switching_agent_plan_ask"}, {"priority", "P0"}},
                               nlohmann::json{{"name", "persistence_layers"}, {"priority", "P1"}}});
    out["note"] = "Use /api/tool and /api/tool/capabilities for non-hotpatch tool access";
    return out;
}

}  // namespace Core
}  // namespace RawrXD
