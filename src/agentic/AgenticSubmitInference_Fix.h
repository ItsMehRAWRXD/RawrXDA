#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD {
namespace Agentic {

class AgenticInferenceBridge {
public:
    struct RuntimeConfig {
        std::string workingDirectory = ".";
        std::string host = "127.0.0.1";
        uint16_t port = 11435;
        float temperature = 0.1f;
        int maxToolIterations = 8;
    };

    struct InferenceResult {
        std::string response;
        bool usedTools = false;
        bool success = false;
        std::string error;
        int toolIterations = 0;

        struct ToolCallRecord {
            std::string toolName;
            std::string callId;
            std::string output;
            bool success = false;
        };

        std::vector<ToolCallRecord> toolTrace;
    };

    static InferenceResult SubmitInferenceWithTools(
        const std::string& userMessage,
        const std::string& modelName,
        size_t max_tokens = 4096,
        const RuntimeConfig& runtime = RuntimeConfig{});
};

} // namespace Agentic
} // namespace RawrXD
