// ============================================================================
// prompt_template_engine.h — Prompt Template Engine
// ============================================================================
// Jinja2-style variable substitution, conditionals, loops, and JSON schema
// for prompt templates. Supports FIM, chat, refactor, and agentic prompts.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <mutex>
#include <functional>

namespace RawrXD {
namespace Prompt {

// ============================================================================
// Template Variable Value
// ============================================================================

using TemplateValue = std::variant<
    std::string,
    int,
    float,
    bool,
    std::vector<std::string>
>;

// Variable context (key → value map)
using TemplateContext = std::unordered_map<std::string, TemplateValue>;

// ============================================================================
// Template Result
// ============================================================================

struct TemplateResult {
    bool        success;
    const char* detail;
    int         errorCode;
    std::string output;
    int         estimatedTokens;

    static TemplateResult ok(std::string out, int tokens = 0) {
        TemplateResult r;
        r.success         = true;
        r.detail          = "Rendered";
        r.errorCode       = 0;
        r.output          = std::move(out);
        r.estimatedTokens = tokens;
        return r;
    }

    static TemplateResult error(const char* msg, int code = -1) {
        TemplateResult r;
        r.success         = false;
        r.detail          = msg;
        r.errorCode       = code;
        return r;
    }
};

// ============================================================================
// Template Type
// ============================================================================

enum class TemplateType : uint8_t {
    FIM_COMPLETION  = 0,    // Fill-in-the-middle
    CHAT            = 1,    // Multi-turn chat
    REFACTOR        = 2,    // Code refactoring
    EXPLAIN         = 3,    // Code explanation
    COMMIT_MSG      = 4,    // Git commit message
    CODE_REVIEW     = 5,    // Code review
    BUG_FIX         = 6,    // Bug diagnosis and fix
    DOCUMENTATION   = 7,    // Doc generation
    TEST_GENERATION = 8,    // Test case generation
    AGENTIC         = 9,    // Multi-step agentic plan
    CUSTOM          = 10    // User-defined
};

const char* templateTypeName(TemplateType type);

// ============================================================================
// Prompt Template — stored definition
// ============================================================================

struct PromptTemplate {
    std::string   id;           // Unique template ID
    std::string   name;         // Human-readable name
    TemplateType  type;
    std::string   templateText; // Template body with {{variables}}
    std::string   systemPrompt; // Optional system prompt
    int           maxTokens;    // Token limit for this template
    float         temperature;  // Default temperature

    // Required variables (for validation)
    std::vector<std::string> requiredVars;

    // Optional: model-specific overrides
    std::unordered_map<std::string, std::string> modelOverrides;
};

// ============================================================================
// AST Node for template parsing
// ============================================================================

enum class NodeType : uint8_t {
    TEXT,           // Raw text
    VARIABLE,       // {{ var }}
    IF,             // {% if cond %} ... {% endif %}
    ELSE,           // {% else %}
    FOR,            // {% for item in list %} ... {% endfor %}
    ENDIF,
    ENDFOR,
    FILTER,         // {{ var | filter }}
    COMMENT,        // {# comment #}
    INCLUDE         // {% include "template_id" %}
};

struct TemplateNode {
    NodeType    type;
    std::string content;         // Raw text, variable name, or condition
    std::string filterName;      // For FILTER nodes
    std::vector<TemplateNode> children;
};

// ============================================================================
// Prompt Template Engine
// ============================================================================

class PromptTemplateEngine {
public:
    PromptTemplateEngine();
    ~PromptTemplateEngine();

    // Singleton
    static PromptTemplateEngine& Global();

    // ---- Template Registration ----

    void registerTemplate(const PromptTemplate& tmpl);
    void removeTemplate(const std::string& id);
    const PromptTemplate* getTemplate(const std::string& id) const;
    std::vector<std::string> listTemplates() const;

    // Register built-in templates (FIM, chat, refactor, etc.)
    void registerBuiltins();

    // ---- Rendering ----

    // Render a registered template by ID
    TemplateResult render(const std::string& templateId,
                           const TemplateContext& context) const;

    // Render an inline template string
    TemplateResult renderInline(const std::string& templateText,
                                const TemplateContext& context) const;

    // Render with model-specific overrides
    TemplateResult renderForModel(const std::string& templateId,
                                   const std::string& modelId,
                                   const TemplateContext& context) const;

    // ---- Validation ----

    // Check that all required variables are present in context
    TemplateResult validate(const std::string& templateId,
                             const TemplateContext& context) const;

    // ---- Filters ----

    // Register a custom filter: {{ var | myfilter }}
    using FilterFunc = std::string(*)(const std::string& input, void* userData);
    void registerFilter(const std::string& name, FilterFunc func, void* userData);

    // ---- FIM Helpers ----

    // Build FIM prompt for a specific model format
    TemplateResult buildFIM(const std::string& prefix,
                             const std::string& suffix,
                             const std::string& context,
                             const std::string& modelFormat = "qwen") const;

    // ---- Chat Helpers ----

    // Build chat prompt from message list
    struct ChatMessage {
        std::string role;     // "system", "user", "assistant"
        std::string content;
    };

    TemplateResult buildChat(const std::vector<ChatMessage>& messages,
                              const std::string& modelFormat = "chatml") const;

private:
    // Parse template string into AST
    std::vector<TemplateNode> parse(const std::string& templateText) const;

    // Evaluate parsed AST with context
    std::string evaluate(const std::vector<TemplateNode>& nodes,
                          const TemplateContext& context) const;

    // Resolve variable from context
    std::string resolveVar(const std::string& varName,
                            const TemplateContext& context) const;

    // Apply filter to value
    std::string applyFilter(const std::string& value,
                             const std::string& filterName) const;

    // Evaluate condition for if blocks
    bool evaluateCondition(const std::string& condition,
                            const TemplateContext& context) const;

    // Estimate tokens
    int estimateTokens(const std::string& text) const;

    // Templates by ID
    std::unordered_map<std::string, PromptTemplate> m_templates;

    // Custom filters
    struct FilterEntry {
        FilterFunc func;
        void*      userData;
    };
    std::unordered_map<std::string, FilterEntry> m_filters;

    mutable std::mutex m_mutex;
};

} // namespace Prompt
} // namespace RawrXD
