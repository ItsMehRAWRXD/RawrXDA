#pragma once
#include "prometheus_engine.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Prometheus {

// =============================================================================
// AGENTIC LOOP
// =============================================================================

class AgenticLoop {
public:
    struct Config {
        int maxIterations = 10;
        bool parallelTools = true;
        bool showReasoning = false;
        bool stopOnFirstError = false;
        std::chrono::milliseconds toolTimeout{30000};
        std::chrono::milliseconds iterationTimeout{120000};
    };

    AgenticLoop(PrometheusEngine& engine, Config config = {});

    GenerationResult execute(
        const std::vector<Message>& messages,
        const std::vector<ToolDefinition>& tools
    );

    void setToolExecutor(std::function<std::string(const ToolCall&)> executor);

    using ProgressCallback = std::function<void(int iteration, const std::string& status)>;
    void setProgressCallback(ProgressCallback cb);

private:
    PrometheusEngine& engine_;
    Config config_;
    std::function<std::string(const ToolCall&)> toolExecutor_;
    ProgressCallback progressCallback_;

    std::vector<ToolCall> executeTools(std::vector<ToolCall>& calls);
};

// =============================================================================
// CODE SANDBOX
// =============================================================================

class CodeSandbox {
public:
    struct Config {
        std::string runtime = "python";
        uint64_t memoryLimitMB = 256;
        std::chrono::milliseconds timeout{30000};
        bool allowNetwork = false;
        bool allowFilesystem = false;
        std::string workDir;
    };

    struct ExecutionResult {
        bool success = false;
        std::string stdout_output;
        std::string stderr_output;
        int exitCode = -1;
        std::chrono::milliseconds duration{0};
        std::vector<std::string> generatedFiles;
        std::string error;
    };

    explicit CodeSandbox(Config config = {});
    ~CodeSandbox();

    ExecutionResult execute(const std::string& code, const std::string& language = "python");
    ExecutionResult executeWithInput(
        const std::string& code,
        const std::string& input,
        const std::string& language = "python"
    );

    class Session {
    public:
        explicit Session(CodeSandbox& sandbox);
        ~Session();
        ExecutionResult execute(const std::string& code);
        ExecutionResult executeWithInput(const std::string& code, const std::string& input);
        bool writeFile(const std::string& path, const std::string& content);
        std::string readFile(const std::string& path);
        std::vector<std::string> listFiles();
    private:
        CodeSandbox& sandbox_;
    };

    Session createSession();

private:
    Config config_;
    std::string sandboxDir_;
    ExecutionResult executePython(const std::string& code, const std::string& input = "");
    ExecutionResult executeNode(const std::string& code, const std::string& input = "");
};

// =============================================================================
// ARTIFACT MANAGER
// =============================================================================

class ArtifactManager {
public:
    struct Artifact {
        std::string id;
        std::string type;
        std::string language;
        std::string title;
        std::string content;
        std::chrono::system_clock::time_point createdAt;
        bool hasBeenExecuted = false;
        std::string lastOutput;
    };

    std::vector<Artifact> parseArtifacts(const std::string& response);
    std::string registerArtifact(const Artifact& artifact);
    std::optional<Artifact> getArtifact(const std::string& id) const;
    CodeSandbox::ExecutionResult executeArtifact(const std::string& id, CodeSandbox& sandbox);
    std::vector<Artifact> listArtifacts() const;
    void clear();

private:
    std::unordered_map<std::string, Artifact> artifacts_;
    uint64_t nextId_ = 1;
};

// =============================================================================
// INLINE IMPLEMENTATION
// =============================================================================

inline AgenticLoop::AgenticLoop(PrometheusEngine& engine, Config config)
    : engine_(engine), config_(std::move(config)) {}

inline void AgenticLoop::setToolExecutor(std::function<std::string(const ToolCall&)> executor) {
    toolExecutor_ = std::move(executor);
}

inline void AgenticLoop::setProgressCallback(ProgressCallback cb) {
    progressCallback_ = std::move(cb);
}

inline GenerationResult AgenticLoop::execute(
    const std::vector<Message>& messages,
    const std::vector<ToolDefinition>& tools
) {
    GenerationConfig genConfig;
    genConfig.tools = tools;
    genConfig.enableToolCalls = true;
    genConfig.parallelToolCalls = config_.parallelTools;
    genConfig.maxTokens = 4096;

    std::vector<Message> currentMessages = messages;
    GenerationResult result;

    for (int i = 0; i < config_.maxIterations; ++i) {
        if (progressCallback_) progressCallback_(i, "Generating...");
        result = engine_.generate(currentMessages, genConfig);
        if (result.toolCalls.empty()) {
            result.finishReason = "stop";
            break;
        }
        if (progressCallback_) {
            progressCallback_(i, "Executing " + std::to_string(result.toolCalls.size()) + " tools...");
        }
        auto executedCalls = executeTools(result.toolCalls);
        if (config_.stopOnFirstError) {
            for (const auto& call : executedCalls) {
                if (call.state == ToolCall::State::Failed) {
                    result.finishReason = "tool_error";
                    return result;
                }
            }
        }
        Message assistantMsg;
        assistantMsg.role = "assistant";
        ContentPart assistantContent;
        assistantContent.type = ContentPart::Type::Text;
        assistantContent.text = result.text;
        assistantMsg.content.push_back(assistantContent);
        for (const auto& call : executedCalls) {
            ContentPart toolContent;
            toolContent.type = ContentPart::Type::ToolCall;
            toolContent.toolCall = call;
            assistantMsg.content.push_back(toolContent);
        }
        currentMessages.push_back(assistantMsg);
        for (const auto& call : executedCalls) {
            Message toolMsg;
            toolMsg.role = "tool";
            toolMsg.toolCallId = call.id;
            toolMsg.name = call.name;
            ContentPart resultContent;
            resultContent.type = ContentPart::Type::ToolResult;
            resultContent.toolResult = call.result;
            if (!call.error.empty()) resultContent.toolResult = "Error: " + call.error;
            toolMsg.content.push_back(resultContent);
            currentMessages.push_back(toolMsg);
        }
    }
    if (result.toolCalls.empty()) result.finishReason = "max_iterations";
    return result;
}

inline std::vector<ToolCall> AgenticLoop::executeTools(std::vector<ToolCall>& calls) {
    if (!toolExecutor_) {
        for (auto& call : calls) {
            call.state = ToolCall::State::Failed;
            call.error = "No tool executor configured";
        }
        return calls;
    }
    for (auto& call : calls) {
        call.state = ToolCall::State::Running;
        auto start = std::chrono::steady_clock::now();
        try {
            call.result = toolExecutor_(call);
            call.state = ToolCall::State::Completed;
        } catch (const std::exception& e) {
            call.error = e.what();
            call.state = ToolCall::State::Failed;
        }
        auto end = std::chrono::steady_clock::now();
        call.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }
    return calls;
}

inline CodeSandbox::CodeSandbox(Config config) : config_(std::move(config)) {}
inline CodeSandbox::~CodeSandbox() = default;

inline CodeSandbox::ExecutionResult CodeSandbox::execute(
    const std::string& code, const std::string& language
) {
    if (language == "python") return executePython(code, "");
    if (language == "node" || language == "javascript") return executeNode(code, "");
    ExecutionResult r;
    r.error = "Unsupported language: " + language;
    return r;
}

inline CodeSandbox::ExecutionResult CodeSandbox::executeWithInput(
    const std::string& code, const std::string& input, const std::string& language
) {
    if (language == "python") return executePython(code, input);
    if (language == "node" || language == "javascript") return executeNode(code, input);
    ExecutionResult r;
    r.error = "Unsupported language: " + language;
    return r;
}

inline CodeSandbox::ExecutionResult CodeSandbox::executePython(
    const std::string& code, const std::string& /*input*/
) {
    ExecutionResult r;
    r.error = "Python execution not implemented in this build";
    r.exitCode = -1;
    return r;
}

inline CodeSandbox::ExecutionResult CodeSandbox::executeNode(
    const std::string& code, const std::string& /*input*/
) {
    ExecutionResult r;
    r.error = "Node execution not implemented in this build";
    r.exitCode = -1;
    return r;
}

inline CodeSandbox::Session CodeSandbox::createSession() {
    return Session(*this);
}

inline CodeSandbox::Session::Session(CodeSandbox& sandbox) : sandbox_(sandbox) {}
inline CodeSandbox::Session::~Session() = default;
inline CodeSandbox::ExecutionResult CodeSandbox::Session::execute(const std::string& code) {
    ExecutionResult r;
    r.error = "Session execution not implemented";
    return r;
}
inline CodeSandbox::ExecutionResult CodeSandbox::Session::executeWithInput(
    const std::string& code, const std::string& input
) {
    ExecutionResult r;
    r.error = "Session execution not implemented";
    return r;
}
inline bool CodeSandbox::Session::writeFile(const std::string& path, const std::string& content) {
    (void)path; (void)content;
    return false;
}
inline std::string CodeSandbox::Session::readFile(const std::string& path) {
    (void)path;
    return "";
}
inline std::vector<std::string> CodeSandbox::Session::listFiles() {
    return {};
}

inline std::vector<ArtifactManager::Artifact> ArtifactManager::parseArtifacts(const std::string& response) {
    std::vector<Artifact> result;
    size_t pos = 0;
    while (true) {
        size_t start = response.find(SpecialTokens::ARTIFACT_START, pos);
        if (start == std::string::npos) break;
        start += std::strlen(SpecialTokens::ARTIFACT_START);
        size_t end = response.find(SpecialTokens::ARTIFACT_END, start);
        if (end == std::string::npos) break;
        Artifact a;
        a.content = response.substr(start, end - start);
        a.id = "art_" + std::to_string(nextId_++);
        a.createdAt = std::chrono::system_clock::now();
        result.push_back(a);
        pos = end + std::strlen(SpecialTokens::ARTIFACT_END);
    }
    return result;
}

inline std::string ArtifactManager::registerArtifact(const Artifact& artifact) {
    std::string id = artifact.id.empty() ? "art_" + std::to_string(nextId_++) : artifact.id;
    artifacts_[id] = artifact;
    artifacts_[id].id = id;
    return id;
}

inline std::optional<ArtifactManager::Artifact> ArtifactManager::getArtifact(const std::string& id) const {
    auto it = artifacts_.find(id);
    if (it != artifacts_.end()) return it->second;
    return std::nullopt;
}

inline CodeSandbox::ExecutionResult ArtifactManager::executeArtifact(
    const std::string& id, CodeSandbox& sandbox
) {
    auto art = getArtifact(id);
    if (!art) {
        CodeSandbox::ExecutionResult r;
        r.error = "Artifact not found: " + id;
        return r;
    }
    auto result = sandbox.execute(art->content, art->language.empty() ? "python" : art->language);
    if (result.success) {
        artifacts_[id].hasBeenExecuted = true;
        artifacts_[id].lastOutput = result.stdout_output;
    }
    return result;
}

inline std::vector<ArtifactManager::Artifact> ArtifactManager::listArtifacts() const {
    std::vector<Artifact> result;
    for (const auto& [id, art] : artifacts_) {
        result.push_back(art);
    }
    return result;
}

inline void ArtifactManager::clear() {
    artifacts_.clear();
}

} // namespace Prometheus
