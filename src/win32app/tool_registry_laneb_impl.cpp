// Lane B (RawrEngine): ToolRegistry implementation for agentic_bridge_headless.cpp
// Bridges to the full RawrXD_ToolRegistry.cpp when available, provides built-in
// MASM tool handlers as fallback for standalone Lane B builds.

#include "../agentic/RawrXD_ToolRegistry.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <functional>

namespace RawrXD
{
namespace Agent
{

namespace {
std::atomic<uint64_t> g_toolRegistryStubHits{0};
std::atomic<uint64_t> g_toolRegistryExecuteCalls{0};
std::atomic<uint64_t> g_toolRegistrySuccessCount{0};
std::atomic<uint64_t> g_toolRegistryErrorCount{0};
std::atomic<uint64_t> g_toolRegistryUnknownToolCount{0};

// Built-in MASM tool handlers for Lane B standalone builds
ToolResult handleMasmAssemble(const nlohmann::json& args, std::string& output) {
    std::string source = args.value("source", "");
    std::string outPath = args.value("output", "output.obj");
    if (source.empty()) {
        output = R"({"ok":false,"error":"missing source parameter"})";
        return ToolResult::ValidationFailed;
    }
    // Write source to temp file
    std::string tempPath = std::getenv("TEMP") ? std::string(std::getenv("TEMP")) + "\\masm_src.asm" : "masm_src.asm";
    FILE* f = nullptr;
    fopen_s(&f, tempPath.c_str(), "w");
    if (!f) {
        output = R"({"ok":false,"error":"cannot write temp source file"})";
        return ToolResult::ExecutionError;
    }
    fwrite(source.c_str(), 1, source.size(), f);
    fclose(f);
    // Assemble with ml64
    std::string cmd = "ml64 /c /nologo /Fo" + outPath + " " + tempPath;
    int rc = system(cmd.c_str());
    if (rc != 0) {
        output = R"({"ok":false,"error":"assembly failed","rc":)" + std::to_string(rc) + "}";
        return ToolResult::ExecutionError;
    }
    output = R"({"ok":true,"output":")" + outPath + R"(","bytes":)" + std::to_string(std::filesystem::file_size(outPath)) + "}";
    return ToolResult::Success;
}

ToolResult handleVectorCosine(const nlohmann::json& args, std::string& output) {
    auto a = args.value("a", nlohmann::json::array());
    auto b = args.value("b", nlohmann::json::array());
    if (a.empty() || b.empty() || a.size() != b.size()) {
        output = R"({"ok":false,"error":"vectors must be non-empty and same size"})";
        return ToolResult::ValidationFailed;
    }
    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        double av = a[i].get<double>();
        double bv = b[i].get<double>();
        dot += av * bv;
        normA += av * av;
        normB += bv * bv;
    }
    double similarity = (normA > 0 && normB > 0) ? dot / (std::sqrt(normA) * std::sqrt(normB)) : 0.0;
    output = R"({"ok":true,"similarity":)" + std::to_string(similarity) + "}";
    return ToolResult::Success;
}

ToolResult handleNLShellValidate(const nlohmann::json& args, std::string& output) {
    std::string cmd = args.value("command", "");
    if (cmd.empty()) {
        output = R"({"ok":false,"error":"empty command"})";
        return ToolResult::ValidationFailed;
    }
    // Basic safety check
    std::vector<std::string> dangerous = {"rm -rf", "del /f /s /q", "format", "rd /s /q"};
    for (const auto& d : dangerous) {
        if (cmd.find(d) != std::string::npos) {
            output = R"({"ok":false,"error":"dangerous command blocked","blocked":true})";
            return ToolResult::SandboxBlocked;
        }
    }
    output = R"({"ok":true,"safe":true,"command_length":)" + std::to_string(cmd.size()) + "}";
    return ToolResult::Success;
}

std::unordered_map<std::string, std::function<ToolResult(const nlohmann::json&, std::string&)>> g_builtinHandlers;
std::once_flag g_initFlag;

void initBuiltinHandlers() {
    g_builtinHandlers["masm_assemble"] = handleMasmAssemble;
    g_builtinHandlers["vector_cosine_similarity"] = handleVectorCosine;
    g_builtinHandlers["nlshell_validate"] = handleNLShellValidate;
}
} // namespace

ToolRegistry::ToolRegistry() = default;
ToolRegistry::~ToolRegistry() = default;

ToolRegistry& ToolRegistry::Instance()
{
    static ToolRegistry s_instance;
    return s_instance;
}

void ToolRegistry::RegisterBuiltinMasmTools()
{
    std::call_once(g_initFlag, initBuiltinHandlers);

    // Register built-in tools with full definitions
    ToolDefinition masmDef;
    masmDef.name = "masm_assemble";
    masmDef.description = "Assemble MASM x64 source code to object file";
    masmDef.category = "build";
    masmDef.danger = DangerLevel::Normal;
    masmDef.handler = handleMasmAssemble;
    RegisterNativeTool(std::move(masmDef));

    ToolDefinition vecDef;
    vecDef.name = "vector_cosine_similarity";
    vecDef.description = "Compute cosine similarity between two vectors";
    vecDef.category = "math";
    vecDef.danger = DangerLevel::Safe;
    vecDef.handler = handleVectorCosine;
    RegisterNativeTool(std::move(vecDef));

    ToolDefinition nlsDef;
    nlsDef.name = "nlshell_validate";
    nlsDef.description = "Validate natural language shell command for safety";
    nlsDef.category = "security";
    nlsDef.danger = DangerLevel::Safe;
    nlsDef.handler = handleNLShellValidate;
    RegisterNativeTool(std::move(nlsDef));
}

ToolResult ToolRegistry::Execute(const std::string& tool_name, const std::string& json_args, std::string& output)
{
    g_toolRegistryStubHits.fetch_add(1, std::memory_order_relaxed);
    g_toolRegistryExecuteCalls.fetch_add(1, std::memory_order_relaxed);

    std::call_once(g_initFlag, initBuiltinHandlers);

    // Try built-in handlers first
    auto it = g_builtinHandlers.find(tool_name);
    if (it != g_builtinHandlers.end()) {
        try {
            nlohmann::json args = json_args.empty() ? nlohmann::json::object() : nlohmann::json::parse(json_args);
            ToolResult result = it->second(args, output);
            if (result == ToolResult::Success) {
                g_toolRegistrySuccessCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_toolRegistryErrorCount.fetch_add(1, std::memory_order_relaxed);
            }
            return result;
        } catch (const std::exception& e) {
            output = std::string(R"({"ok":false,"error":"exception: )") + e.what() + "\"}";
            g_toolRegistryErrorCount.fetch_add(1, std::memory_order_relaxed);
            return ToolResult::ExecutionError;
        }
    }

    // Try to delegate to full registry if available
    auto* fullTool = GetTool(tool_name);
    if (fullTool && fullTool->handler) {
        try {
            nlohmann::json args = json_args.empty() ? nlohmann::json::object() : nlohmann::json::parse(json_args);
            ToolResult result = fullTool->handler(args, output);
            if (result == ToolResult::Success) {
                g_toolRegistrySuccessCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_toolRegistryErrorCount.fetch_add(1, std::memory_order_relaxed);
            }
            return result;
        } catch (const std::exception& e) {
            output = std::string(R"({"ok":false,"error":"exception: )") + e.what() + "\"}";
            g_toolRegistryErrorCount.fetch_add(1, std::memory_order_relaxed);
            return ToolResult::ExecutionError;
        }
    }

    g_toolRegistryUnknownToolCount.fetch_add(1, std::memory_order_relaxed);
    output = R"({"ok":false,"error":"unknown tool: )" + tool_name + R"(","available":[)";
    bool first = true;
    for (const auto& [name, _] : g_builtinHandlers) {
        if (!first) output += ",";
        first = false;
        output += "\"" + name + "\"";
    }
    output += "]}";
    return ToolResult::ExecutionError;
}

}  // namespace Agent
}  // namespace RawrXD
