#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory> 

namespace RawrXD {

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

    static std::vector<Completion> generateCompletion(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::string& context,
        int numCompletions = 3);

    static std::string generateCode(
        const std::string& instruction,
        const std::string& fileType,
        const std::string& context);

    static std::string generateRewrite(
        const std::string& code,
        const std::string& instruction,
        const std::string& context);

    // Internal Implementation helpers (exposed for simplicity in this refactor step, but usually private)
    static std::string callModel(const std::string& prompt, const GenerationParams& params);

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
