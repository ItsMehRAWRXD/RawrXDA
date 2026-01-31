#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <cstdlib>

#include <nlohmann/json.hpp>

#include "backend/agentic_tools.h"

extern "C" ULONG __stdcall HttpCloseRequestQueue(HANDLE hRequestQueue);

extern "C" ULONG __stdcall HttpCloseRequestHandle(HANDLE hRequestQueue) {
    return HttpCloseRequestQueue(hRequestQueue);
}

extern "C" uint32_t __stdcall InferenceEngine_Initialize() {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_LoadModel(const char* /*modelPath*/) {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_Generate(const char* /*prompt*/, char* /*outBuffer*/, uint32_t /*outBufferSize*/) {
    return 1;
}

static std::string GetWorkspaceRoot() {
    const char* env = std::getenv("RAWRXD_WORKSPACE");
    if (env && env[0]) {
        return std::string(env);
    }
    return std::filesystem::current_path().string();
}

extern "C" uint32_t __stdcall ToolExecuteJson(const char* jsonText, char* outBuffer, uint32_t outBufferSize) {
    if (!outBuffer || outBufferSize == 0) {
        return 1;
    }

    using json = nlohmann::json;

    json request;
    try {
        request = json::parse(jsonText ? jsonText : "{}");
    } catch (...) {
        std::snprintf(outBuffer, outBufferSize,
                      "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return 1;
    }

    std::string tool = request.value("tool", "");
    if (tool.empty()) {
        std::snprintf(outBuffer, outBufferSize,
                      "{\"success\":false,\"error\":\"Missing tool field\"}");
        return 1;
    }

    nlohmann::json params;  // Default constructor creates null, we'll populate it
    if (request.contains("params") && request["params"].is_object()) {
        params = request["params"];
    } else {
        // Build params from all request fields except "tool"
        for (auto it = request.begin(); it != request.end(); ++it) {
            if (it.key() != "tool") {
                params[it.key()] = it.value();
            }
        }
    }

    RawrXD::Backend::AgenticToolExecutor executor(GetWorkspaceRoot());
    RawrXD::Backend::ToolResult result = executor.executeTool(tool, params.dump());

    json response;
    response["success"] = result.success;
    response["tool"] = result.tool_name;
    response["exit_code"] = result.exit_code;
    if (result.success) {
        try {
            response["result"] = json::parse(result.result_data);
        } catch (...) {
            response["result"] = result.result_data;
        }
    } else {
        response["error"] = result.error_message;
    }

    std::string payload = response.dump();
    if (payload.size() + 1 > outBufferSize) {
        std::snprintf(outBuffer, outBufferSize,
                      "{\"success\":false,\"error\":\"Response too large\"}");
        return 1;
    }

    std::memcpy(outBuffer, payload.c_str(), payload.size() + 1);
    return result.success ? 0 : 1;
}
