#pragma once
/**
 * @file ai_code_generator.h
 * @brief AI-powered code generation from natural language
 * Batch 5 - Item 72: AI code generator
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::AI {

enum class GenerationType {
    Function,
    Class,
    Test,
    Documentation,
    Regex,
    Sql,
    Config,
    Custom
};

struct GenerationRequest {
    GenerationType type;
    std::string description;
    std::string language;
    std::string context;
    std::vector<std::string> requirements;
    int maxTokens;
    float temperature;
};

struct GenerationResult {
    std::string code;
    std::string explanation;
    std::vector<std::string> suggestions;
    bool isComplete;
    std::string error;
};

struct Template {
    std::string name;
    std::string description;
    GenerationType type;
    std::string prompt;
    std::vector<std::string> placeholders;
};

class AICodeGenerator {
public:
    AICodeGenerator();
    ~AICodeGenerator();

    // Initialization
    bool initialize();
    void shutdown();

    // Generation
    GenerationResult generate(const GenerationRequest& request);
    std::future<GenerationResult> generateAsync(const GenerationRequest& request);

    // Quick generators
    GenerationResult generateFunction(const std::string& description, const std::string& language);
    GenerationResult generateClass(const std::string& description, const std::string& language);
    GenerationResult generateTests(const std::string& code, const std::string& language);
    GenerationResult generateDocumentation(const std::string& code, const std::string& language);
    GenerationResult generateRegex(const std::string& description);
    GenerationResult generateSql(const std::string& description);

    // Templates
    void registerTemplate(const Template& template_);
    void unregisterTemplate(const std::string& name);
    std::vector<Template> getTemplates() const;
    std::optional<Template> getTemplate(const std::string& name) const;
    GenerationResult generateFromTemplate(const std::string& templateName,
                                         const std::map<std::string, std::string>& values);

    // Refinement
    GenerationResult refine(const std::string& code, const std::string& instruction);
    GenerationResult addFeature(const std::string& code, const std::string& feature);
    GenerationResult optimize(const std::string& code);
    GenerationResult convertLanguage(const std::string& code, const std::string& targetLanguage);

    // Configuration
    void setModel(const std::string& model);
    void setDefaultLanguage(const std::string& language);
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);

    // History
    std::vector<GenerationResult> getHistory() const;
    void clearHistory();

    // Events
    using GenerationCallback = std::function<void(const GenerationResult&)>;
    void onGenerationComplete(GenerationCallback callback);

private:
    std::string m_model;
    std::string m_defaultLanguage{"cpp"};
    int m_maxTokens{2000};
    float m_temperature{0.7f};
    std::vector<Template> m_templates;
    std::vector<GenerationResult> m_history;

    GenerationCallback m_generationCallback;

    GenerationResult performGeneration(const GenerationRequest& request);
    std::string buildPrompt(const GenerationRequest& request);
    GenerationResult parseResponse(const std::string& response);
    void notifyGenerationComplete(const GenerationResult& result);
};

// Global instance
AICodeGenerator& getAICodeGenerator();

} // namespace RawrXD::AI
