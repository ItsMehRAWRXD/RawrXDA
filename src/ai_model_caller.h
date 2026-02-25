#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory> 
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace RawrXD {

/**
 * @brief Diagnostic result from AI code analysis
 */
struct Diagnostic {
    std::string message;
    int line = 0;
    int column = 0;
    std::string severity;      // "error", "warning", "info", "hint"
    std::string code_action;   // Suggested fix
    std::string source;        // e.g., "ai-linter"
};

class ModelCaller {
public:
    enum class ModelType {
        GGUF_LOCAL,      // Local GGUF model via GGML
        OLLAMA_REMOTE,   // Remote Ollama endpoint
        OPENAI_API,      // OpenAI API (ChatGPT, GPT-4)
        HUGGINGFACE_API  // HuggingFace Inference API
    };

    struct GenerationParams {
        float temperature = 0.7f;
        int max_tokens = 256;
        float top_p = 0.9f;
        int top_k = 40;
        float repetition_penalty = 1.1f;
        std::string system_prompt;
        int context_window = 2048;
    };

    struct Completion {
        std::string text;
        float score = 0.0f;
        std::string description;
    };

    /**
     * @brief Initialize the model caller with a specific model
     * @param modelPath Path to GGUF file or Ollama model name
     * @return true if initialization succeeded
     */
    static bool Initialize(const std::string& modelPath);
    
    /**
     * @brief Shutdown and release resources
     */
    static void Shutdown();
    
    /**
     * @brief Check if a model is loaded and ready
     */
    static bool IsReady();

    /**
     * @brief Generate completions for code
     */
    static std::vector<Completion> generateCompletion(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::string& context,
        int numCompletions = 3);

    /**
     * @brief Generate code from natural language instruction
     */
    static std::string generateCode(
        const std::string& instruction,
        const std::string& fileType,
        const std::string& context);

    /**
     * @brief Rewrite/refactor code based on instruction
     */
    static std::string generateRewrite(
        const std::string& code,
        const std::string& instruction,
        const std::string& context);

    /**
     * @brief Generate code diagnostics
     */
    static std::vector<Diagnostic> generateDiagnostics(
        const std::string& code,
        const std::string& language);

    /**
     * @brief Core model invocation (sync)
     */
    static std::string callModel(const std::string& prompt, const GenerationParams& params);

    /**
     * @brief Streaming callback type
     * @return false to cancel streaming
     */
    using StreamCallback = std::function<bool(const std::string&)>;
    
    /**
     * @brief Stream model output token by token
     * @param prompt The input prompt
     * @param params Generation parameters
     * @param callback Called for each token chunk
     * @param delay Optional delay between callback invocations
     * @return true if completed successfully
     */
    static bool streamModel(const std::string& prompt, const GenerationParams& params, 
                           StreamCallback callback, 
                           std::chrono::milliseconds delay = std::chrono::milliseconds(0));

    /**
     * @brief Parse JSON response from model
     */
    static nlohmann::json ParseStructuredResponse(const std::string& response);
    
    /**
     * @brief Extract diagnostics from model response text
     */
    static std::vector<Diagnostic> ExtractDiagnostics(const std::string& response);

private:
    static std::string buildCompletionPrompt(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::string& context);
    
    static std::vector<std::string> parseCompletions(const std::string& response);
    static float scoreCompletion(const std::string& completion, const std::string& prefix, const std::string& fileType);
    static std::string getCompletionDescription(const std::string& completion);
};

} // namespace RawrXD
