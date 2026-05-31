#ifndef AI_DEBUGGER_H
#define AI_DEBUGGER_H

// AI-Assisted Debug Agent — NativeDebuggerEngine Backend
// Production debug agent using DbgEng COM for AI-assisted debugging.
// Integrates with SovereignInferenceClient for LLM-powered analysis.
// NO SOURCE FILE IS TO BE SIMPLIFIED

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace RawrXD {
namespace Agent {
    class SovereignInferenceClient;
}
namespace Debugger {
    enum class BreakpointType : uint32_t;
}
namespace Debug {

struct ExceptionInfo {
    uint32_t    code;
    const char* name;
    const char* description;
    const char* commonCause;
};

class AIDebugAgent {
public:
    static AIDebugAgent& Instance();

    // ---- Sovereign Inference Integration ----
    void SetInferenceClient(std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> client);
    bool HasInferenceClient() const;

    // ---- Session Management ----
    struct LaunchResult {
        bool success;
        std::string detail;
        uint32_t pid;
    };

    LaunchResult LaunchTarget(const std::string& exePath,
                               const std::string& args = "",
                               const std::string& workDir = "");
    LaunchResult AttachToProcess(uint32_t pid);

    // ---- Exception Analysis ----
    struct ExceptionAnalysis {
        uint32_t exceptionCode;
        std::string exceptionName;
        std::string description;
        std::string commonCause;
        uint64_t faultAddress;
        std::string faultSymbol;
        std::string faultModule;
        std::string stackSummary;
        std::string registerSummary;
        std::string hypothesis;
        std::vector<std::string> suggestedActions;
    };

    ExceptionAnalysis AnalyzeLastException();
    std::string FormatAnalysisForLLM(const ExceptionAnalysis& analysis);

    // ---- Breakpoint Suggestions ----
    struct BreakpointSuggestion {
        std::string symbol;
        std::string reason;
        Debugger::BreakpointType type;
    };

    std::vector<BreakpointSuggestion> SuggestBreakpoints(const std::string& context);

    // ---- Memory Analysis ----
    struct MemoryAnalysis {
        uint64_t address;
        std::string hexDump;
        std::string interpretation;
        bool isCodeRegion;
        bool isStackRegion;
        bool isHeapRegion;
        std::string nearestSymbol;
    };

    MemoryAnalysis AnalyzeMemoryRegion(uint64_t address, uint32_t size = 256);
    std::string DisassembleWithAnnotations(uint64_t address, uint32_t lineCount = 20);
    std::string CaptureDebugSnapshot();

    // ---- AI-Powered Analysis ----
    std::string GenerateAIHypothesis(const ExceptionAnalysis& analysis);
    std::vector<std::string> GenerateAISuggestedActions(const ExceptionAnalysis& analysis);
    std::vector<BreakpointSuggestion> GenerateAIBreakpointSuggestions(const std::string& context);

private:
    AIDebugAgent() = default;
    AIDebugAgent(const AIDebugAgent&) = delete;
    AIDebugAgent& operator=(const AIDebugAgent&) = delete;

    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> m_inferenceClient;
};

} // namespace Debug
} // namespace RawrXD

#endif // AI_DEBUGGER_H
