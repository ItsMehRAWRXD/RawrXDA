// ============================================================================
// prompt_template_engine.cpp — Prompt Template Engine Implementation
// ============================================================================
// Jinja2-style template parsing, variable substitution, conditionals, loops,
// filters, built-in templates, FIM/chat format builders.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/prompt_template_engine.h"

#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>

namespace RawrXD {
namespace Prompt {

// ============================================================================
// Template type names
// ============================================================================

const char* templateTypeName(TemplateType type) {
    switch (type) {
        case TemplateType::FIM_COMPLETION:  return "fim_completion";
        case TemplateType::CHAT:            return "chat";
        case TemplateType::REFACTOR:        return "refactor";
        case TemplateType::EXPLAIN:         return "explain";
        case TemplateType::COMMIT_MSG:      return "commit_msg";
        case TemplateType::CODE_REVIEW:     return "code_review";
        case TemplateType::BUG_FIX:         return "bug_fix";
        case TemplateType::DOCUMENTATION:   return "documentation";
        case TemplateType::TEST_GENERATION: return "test_generation";
        case TemplateType::AGENTIC:         return "agentic";
        case TemplateType::CUSTOM:          return "custom";
        default: return "unknown";
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

PromptTemplateEngine::PromptTemplateEngine() {
    // Register built-in filters
    m_filters["upper"] = {
        [](const std::string& input, void*) -> std::string {
            std::string result = input;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::toupper(c); });
            return result;
        },
        nullptr
    };

    m_filters["lower"] = {
        [](const std::string& input, void*) -> std::string {
            std::string result = input;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return result;
        },
        nullptr
    };

    m_filters["trim"] = {
        [](const std::string& input, void*) -> std::string {
            size_t start = input.find_first_not_of(" \t\r\n");
            size_t end = input.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            return input.substr(start, end - start + 1);
        },
        nullptr
    };

    m_filters["truncate"] = {
        [](const std::string& input, void*) -> std::string {
            const size_t maxLen = 500;
            if (input.size() <= maxLen) return input;
            return input.substr(0, maxLen) + "...";
        },
        nullptr
    };

    m_filters["escape"] = {
        [](const std::string& input, void*) -> std::string {
            std::string result;
            result.reserve(input.size() + 32);
            for (char c : input) {
                switch (c) {
                    case '"':  result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default:   result += c;
                }
            }
            return result;
        },
        nullptr
    };
}

PromptTemplateEngine::~PromptTemplateEngine() = default;

PromptTemplateEngine& PromptTemplateEngine::Global() {
    static PromptTemplateEngine instance;
    return instance;
}

// ============================================================================
// Template Registration
// ============================================================================

void PromptTemplateEngine::registerTemplate(const PromptTemplate& tmpl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_templates[tmpl.id] = tmpl;
}

void PromptTemplateEngine::removeTemplate(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_templates.erase(id);
}

const PromptTemplate* PromptTemplateEngine::getTemplate(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_templates.find(id);
    return (it != m_templates.end()) ? &it->second : nullptr;
}

std::vector<std::string> PromptTemplateEngine::listTemplates() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> ids;
    for (const auto& [id, _] : m_templates) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Built-in Templates
// ============================================================================

void PromptTemplateEngine::registerBuiltins() {
    // FIM Completion (Qwen2.5-Coder format)
    registerTemplate({
        "fim_qwen",
        "FIM Completion (Qwen)",
        TemplateType::FIM_COMPLETION,
        "{% if context %}<|file_sep|>\n{{ context }}\n{% endif %}"
        "<|fim_prefix|>{{ prefix }}<|fim_suffix|>{{ suffix }}<|fim_middle|>",
        "",
        2048,
        0.2f,
        {"prefix", "suffix"},
        {}
    });

    // FIM Completion (CodeLlama format)
    registerTemplate({
        "fim_codellama",
        "FIM Completion (CodeLlama)",
        TemplateType::FIM_COMPLETION,
        "<PRE> {{ prefix }} <SUF>{{ suffix }} <MID>",
        "",
        2048,
        0.2f,
        {"prefix", "suffix"},
        {}
    });

    // FIM Completion (DeepSeek format)
    registerTemplate({
        "fim_deepseek",
        "FIM Completion (DeepSeek)",
        TemplateType::FIM_COMPLETION,
        "<|fim▁begin|>{{ prefix }}<|fim▁hole|>{{ suffix }}<|fim▁end|>",
        "",
        2048,
        0.2f,
        {"prefix", "suffix"},
        {}
    });

    // Chat (ChatML format)
    registerTemplate({
        "chat_chatml",
        "Chat (ChatML)",
        TemplateType::CHAT,
        "{% if system_prompt %}<|im_start|>system\n{{ system_prompt }}<|im_end|>\n{% endif %}"
        "{{ chat_history }}"
        "<|im_start|>user\n{{ user_message }}<|im_end|>\n"
        "<|im_start|>assistant\n",
        "You are a helpful coding assistant.",
        4096,
        0.7f,
        {"user_message"},
        {}
    });

    // Refactor
    registerTemplate({
        "refactor",
        "Code Refactoring",
        TemplateType::REFACTOR,
        "<|im_start|>system\nYou are a code refactoring assistant. "
        "Apply the requested refactoring to the code below. "
        "Return only the modified code without explanation.<|im_end|>\n"
        "<|im_start|>user\n"
        "Refactor the following {{ language }} code:\n"
        "```{{ language }}\n{{ code }}\n```\n\n"
        "Refactoring: {{ instruction }}\n"
        "{% if context %}Additional context:\n{{ context }}\n{% endif %}"
        "<|im_end|>\n<|im_start|>assistant\n",
        "",
        4096,
        0.3f,
        {"code", "language", "instruction"},
        {}
    });

    // Explain
    registerTemplate({
        "explain",
        "Code Explanation",
        TemplateType::EXPLAIN,
        "<|im_start|>system\nExplain the following code clearly and concisely.<|im_end|>\n"
        "<|im_start|>user\nExplain this {{ language }} code:\n"
        "```{{ language }}\n{{ code }}\n```<|im_end|>\n"
        "<|im_start|>assistant\n",
        "",
        4096,
        0.5f,
        {"code", "language"},
        {}
    });

    // Commit message
    registerTemplate({
        "commit_msg",
        "Commit Message",
        TemplateType::COMMIT_MSG,
        "<|im_start|>system\nGenerate a concise git commit message for the following changes. "
        "Use conventional commits format (feat/fix/refactor/docs/test/chore).<|im_end|>\n"
        "<|im_start|>user\n"
        "Branch: {{ branch }}\n"
        "Diff:\n```diff\n{{ diff }}\n```<|im_end|>\n"
        "<|im_start|>assistant\n",
        "",
        2048,
        0.3f,
        {"diff"},
        {}
    });

    // Bug fix
    registerTemplate({
        "bug_fix",
        "Bug Fix",
        TemplateType::BUG_FIX,
        "<|im_start|>system\nYou are an expert debugger. Analyze the error and provide a fix. "
        "Return the corrected code.<|im_end|>\n"
        "<|im_start|>user\n"
        "Error in {{ file }}:\n{{ error_message }}\n\n"
        "Code:\n```{{ language }}\n{{ code }}\n```\n"
        "{% if diagnostics %}Diagnostics:\n{{ diagnostics }}\n{% endif %}"
        "<|im_end|>\n<|im_start|>assistant\n",
        "",
        4096,
        0.2f,
        {"code", "error_message", "language"},
        {}
    });

    // Test generation
    registerTemplate({
        "test_gen",
        "Test Generation",
        TemplateType::TEST_GENERATION,
        "<|im_start|>system\nGenerate comprehensive unit tests for the given code. "
        "Use {{ test_framework }} testing framework.<|im_end|>\n"
        "<|im_start|>user\nGenerate tests for:\n"
        "```{{ language }}\n{{ code }}\n```<|im_end|>\n"
        "<|im_start|>assistant\n",
        "",
        4096,
        0.3f,
        {"code", "language"},
        {}
    });

    // Agentic planning
    registerTemplate({
        "agentic_plan",
        "Agentic Planning",
        TemplateType::AGENTIC,
        "<|im_start|>system\nYou are an autonomous coding agent. "
        "Break down the task into steps. For each step, specify:\n"
        "1. Action (edit_file, create_file, run_command, search)\n"
        "2. Target (file path or command)\n"
        "3. Content or arguments\n"
        "Output as JSON array.<|im_end|>\n"
        "<|im_start|>user\n"
        "Task: {{ task }}\n\n"
        "{% if workspace_context %}Workspace context:\n{{ workspace_context }}\n{% endif %}"
        "{% if errors %}Current errors:\n{{ errors }}\n{% endif %}"
        "{% if recent_edits %}Recent edits:\n{{ recent_edits }}\n{% endif %}"
        "<|im_end|>\n<|im_start|>assistant\n",
        "",
        8192,
        0.5f,
        {"task"},
        {}
    });
}

// ============================================================================
// Template Parsing
// ============================================================================

std::vector<TemplateNode> PromptTemplateEngine::parse(const std::string& templateText) const {
    std::vector<TemplateNode> nodes;
    size_t pos = 0;
    size_t len = templateText.size();

    while (pos < len) {
        // Look for next template tag
        size_t tagStart = std::string::npos;
        size_t nextVar    = templateText.find("{{", pos);
        size_t nextBlock  = templateText.find("{%", pos);
        size_t nextComment = templateText.find("{#", pos);

        tagStart = std::min({nextVar, nextBlock, nextComment});

        if (tagStart == std::string::npos) {
            // Rest is plain text
            if (pos < len) {
                TemplateNode node;
                node.type = NodeType::TEXT;
                node.content = templateText.substr(pos);
                nodes.push_back(std::move(node));
            }
            break;
        }

        // Add text before the tag
        if (tagStart > pos) {
            TemplateNode node;
            node.type = NodeType::TEXT;
            node.content = templateText.substr(pos, tagStart - pos);
            nodes.push_back(std::move(node));
        }

        if (tagStart == nextComment) {
            // Comment: {# ... #}
            size_t endComment = templateText.find("#}", tagStart + 2);
            if (endComment == std::string::npos) {
                pos = tagStart + 2;
                continue;
            }
            // Skip comment entirely
            pos = endComment + 2;
        }
        else if (tagStart == nextVar) {
            // Variable: {{ var }} or {{ var | filter }}
            size_t endVar = templateText.find("}}", tagStart + 2);
            if (endVar == std::string::npos) {
                pos = tagStart + 2;
                continue;
            }

            std::string content = templateText.substr(tagStart + 2, endVar - tagStart - 2);
            // Trim whitespace
            size_t s = content.find_first_not_of(" \t");
            size_t e = content.find_last_not_of(" \t");
            if (s != std::string::npos) content = content.substr(s, e - s + 1);

            // Check for filter: var | filtername
            TemplateNode node;
            size_t pipePos = content.find('|');
            if (pipePos != std::string::npos) {
                node.type = NodeType::FILTER;
                node.content = content.substr(0, pipePos);
                node.filterName = content.substr(pipePos + 1);
                // Trim both
                auto trim = [](std::string& s) {
                    size_t a = s.find_first_not_of(" \t");
                    size_t b = s.find_last_not_of(" \t");
                    if (a != std::string::npos) s = s.substr(a, b - a + 1);
                };
                trim(node.content);
                trim(node.filterName);
            } else {
                node.type = NodeType::VARIABLE;
                node.content = content;
            }

            nodes.push_back(std::move(node));
            pos = endVar + 2;
        }
        else if (tagStart == nextBlock) {
            // Block tag: {% if/else/endif/for/endfor/include %}
            size_t endBlock = templateText.find("%}", tagStart + 2);
            if (endBlock == std::string::npos) {
                pos = tagStart + 2;
                continue;
            }

            std::string content = templateText.substr(tagStart + 2, endBlock - tagStart - 2);
            size_t s = content.find_first_not_of(" \t");
            size_t e = content.find_last_not_of(" \t");
            if (s != std::string::npos) content = content.substr(s, e - s + 1);

            TemplateNode node;

            if (content.substr(0, 3) == "if ") {
                node.type = NodeType::IF;
                node.content = content.substr(3);
            } else if (content == "else") {
                node.type = NodeType::ELSE;
            } else if (content == "endif") {
                node.type = NodeType::ENDIF;
            } else if (content.substr(0, 4) == "for ") {
                node.type = NodeType::FOR;
                node.content = content.substr(4);
            } else if (content == "endfor") {
                node.type = NodeType::ENDFOR;
            } else if (content.substr(0, 8) == "include ") {
                node.type = NodeType::INCLUDE;
                node.content = content.substr(8);
                // Remove quotes
                if (!node.content.empty() && node.content.front() == '"') {
                    node.content = node.content.substr(1);
                }
                if (!node.content.empty() && node.content.back() == '"') {
                    node.content.pop_back();
                }
            } else {
                // Unknown block tag, treat as text
                node.type = NodeType::TEXT;
                node.content = templateText.substr(tagStart, endBlock + 2 - tagStart);
            }

            nodes.push_back(std::move(node));
            pos = endBlock + 2;
        }
    }

    return nodes;
}

// ============================================================================
// Template Evaluation
// ============================================================================

std::string PromptTemplateEngine::evaluate(const std::vector<TemplateNode>& nodes,
                                            const TemplateContext& context) const {
    std::ostringstream oss;
    size_t i = 0;

    while (i < nodes.size()) {
        const auto& node = nodes[i];

        switch (node.type) {
            case NodeType::TEXT:
                oss << node.content;
                i++;
                break;

            case NodeType::VARIABLE:
                oss << resolveVar(node.content, context);
                i++;
                break;

            case NodeType::FILTER: {
                std::string value = resolveVar(node.content, context);
                oss << applyFilter(value, node.filterName);
                i++;
                break;
            }

            case NodeType::IF: {
                bool condition = evaluateCondition(node.content, context);
                i++;

                // Collect nodes until ELSE or ENDIF
                std::vector<TemplateNode> thenBranch;
                std::vector<TemplateNode> elseBranch;
                int depth = 1;
                bool inElse = false;

                while (i < nodes.size() && depth > 0) {
                    if (nodes[i].type == NodeType::IF) depth++;
                    if (nodes[i].type == NodeType::ENDIF) {
                        depth--;
                        if (depth == 0) { i++; break; }
                    }
                    if (nodes[i].type == NodeType::ELSE && depth == 1) {
                        inElse = true;
                        i++;
                        continue;
                    }

                    if (inElse) {
                        elseBranch.push_back(nodes[i]);
                    } else {
                        thenBranch.push_back(nodes[i]);
                    }
                    i++;
                }

                if (condition) {
                    oss << evaluate(thenBranch, context);
                } else {
                    oss << evaluate(elseBranch, context);
                }
                break;
            }

            case NodeType::FOR: {
                // Parse "item in list"
                static const std::regex forRe(R"((\w+)\s+in\s+(\w+))", std::regex::optimize);
                std::smatch match;
                std::string forExpr = node.content;
                i++;

                if (!std::regex_search(forExpr, match, forRe)) {
                    // Skip to endfor
                    int depth = 1;
                    while (i < nodes.size() && depth > 0) {
                        if (nodes[i].type == NodeType::FOR) depth++;
                        if (nodes[i].type == NodeType::ENDFOR) depth--;
                        i++;
                    }
                    break;
                }

                std::string itemVar = match[1].str();
                std::string listVar = match[2].str();

                // Collect body until ENDFOR
                std::vector<TemplateNode> body;
                int depth = 1;
                while (i < nodes.size() && depth > 0) {
                    if (nodes[i].type == NodeType::FOR) depth++;
                    if (nodes[i].type == NodeType::ENDFOR) {
                        depth--;
                        if (depth == 0) { i++; break; }
                    }
                    body.push_back(nodes[i]);
                    i++;
                }

                // Resolve list
                auto it = context.find(listVar);
                if (it != context.end()) {
                    if (auto* list = std::get_if<std::vector<std::string>>(&it->second)) {
                        for (const auto& item : *list) {
                            TemplateContext loopCtx = context;
                            loopCtx[itemVar] = item;
                            oss << evaluate(body, loopCtx);
                        }
                    }
                }
                break;
            }

            case NodeType::INCLUDE: {
                // Resolve included template
                auto it = m_templates.find(node.content);
                if (it != m_templates.end()) {
                    auto includedNodes = parse(it->second.templateText);
                    oss << evaluate(includedNodes, context);
                }
                i++;
                break;
            }

            default:
                i++;
                break;
        }
    }

    return oss.str();
}

// ============================================================================
// Variable Resolution
// ============================================================================

std::string PromptTemplateEngine::resolveVar(const std::string& varName,
                                              const TemplateContext& context) const {
    auto it = context.find(varName);
    if (it == context.end()) return "";

    return std::visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(val);
        } else if constexpr (std::is_same_v<T, float>) {
            return std::to_string(val);
        } else if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::string result;
            for (const auto& s : val) {
                if (!result.empty()) result += ", ";
                result += s;
            }
            return result;
        }
        return "";
    }, it->second);
}

// ============================================================================
// Filter Application
// ============================================================================

std::string PromptTemplateEngine::applyFilter(const std::string& value,
                                               const std::string& filterName) const {
    auto it = m_filters.find(filterName);
    if (it == m_filters.end()) return value;
    return it->second.func(value, it->second.userData);
}

// ============================================================================
// Condition Evaluation
// ============================================================================

bool PromptTemplateEngine::evaluateCondition(const std::string& condition,
                                              const TemplateContext& context) const {
    // Simple conditions:
    // "varname" → truthy check (non-empty string, non-zero number, true bool)
    // "not varname" → negation
    // "varname == 'value'" → string equality

    std::string cond = condition;

    // Trim
    size_t s = cond.find_first_not_of(" \t");
    size_t e = cond.find_last_not_of(" \t");
    if (s != std::string::npos) cond = cond.substr(s, e - s + 1);

    // Negation
    bool negate = false;
    if (cond.substr(0, 4) == "not ") {
        negate = true;
        cond = cond.substr(4);
    }

    // Equality check
    size_t eqPos = cond.find("==");
    if (eqPos != std::string::npos) {
        std::string lhs = cond.substr(0, eqPos);
        std::string rhs = cond.substr(eqPos + 2);
        // Trim
        auto trim = [](std::string& str) {
            size_t a = str.find_first_not_of(" \t'\"");
            size_t b = str.find_last_not_of(" \t'\"");
            if (a != std::string::npos) str = str.substr(a, b - a + 1);
        };
        trim(lhs);
        trim(rhs);

        std::string lhsVal = resolveVar(lhs, context);
        bool result = (lhsVal == rhs);
        return negate ? !result : result;
    }

    // Truthy check
    auto it = context.find(cond);
    if (it == context.end()) {
        return negate; // Not found → false, negated → true
    }

    bool truthy = std::visit([](const auto& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return !val.empty();
        } else if constexpr (std::is_same_v<T, int>) {
            return val != 0;
        } else if constexpr (std::is_same_v<T, float>) {
            return val != 0.0f;
        } else if constexpr (std::is_same_v<T, bool>) {
            return val;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            return !val.empty();
        }
        return false;
    }, it->second);

    return negate ? !truthy : truthy;
}

// ============================================================================
// Rendering
// ============================================================================

TemplateResult PromptTemplateEngine::render(const std::string& templateId,
                                             const TemplateContext& context) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_templates.find(templateId);
    if (it == m_templates.end()) {
        return TemplateResult::error("Template not found");
    }

    // Validate required vars
    for (const auto& req : it->second.requiredVars) {
        if (context.find(req) == context.end()) {
            std::string msg = "Missing required variable: " + req;
            // We can't return a string literal here, so use a static buffer
            static thread_local char errBuf[256];
            snprintf(errBuf, sizeof(errBuf), "Missing required variable: %s", req.c_str());
            return TemplateResult::error(errBuf);
        }
    }

    // Add system prompt to context if not already present
    TemplateContext fullCtx = context;
    if (fullCtx.find("system_prompt") == fullCtx.end() &&
        !it->second.systemPrompt.empty()) {
        fullCtx["system_prompt"] = it->second.systemPrompt;
    }

    auto nodes = parse(it->second.templateText);
    std::string output = evaluate(nodes, fullCtx);
    int tokens = estimateTokens(output);

    return TemplateResult::ok(std::move(output), tokens);
}

TemplateResult PromptTemplateEngine::renderInline(const std::string& templateText,
                                                   const TemplateContext& context) const {
    auto nodes = parse(templateText);
    std::string output = evaluate(nodes, context);
    int tokens = estimateTokens(output);
    return TemplateResult::ok(std::move(output), tokens);
}

TemplateResult PromptTemplateEngine::renderForModel(const std::string& templateId,
                                                     const std::string& modelId,
                                                     const TemplateContext& context) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_templates.find(templateId);
    if (it == m_templates.end()) {
        return TemplateResult::error("Template not found");
    }

    // Check for model-specific override
    auto overIt = it->second.modelOverrides.find(modelId);
    if (overIt != it->second.modelOverrides.end()) {
        auto nodes = parse(overIt->second);
        std::string output = evaluate(nodes, context);
        return TemplateResult::ok(std::move(output), estimateTokens(output));
    }

    // Fall back to default template
    auto nodes = parse(it->second.templateText);
    std::string output = evaluate(nodes, context);
    return TemplateResult::ok(std::move(output), estimateTokens(output));
}

// ============================================================================
// Validation
// ============================================================================

TemplateResult PromptTemplateEngine::validate(const std::string& templateId,
                                               const TemplateContext& context) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_templates.find(templateId);
    if (it == m_templates.end()) {
        return TemplateResult::error("Template not found");
    }

    for (const auto& req : it->second.requiredVars) {
        if (context.find(req) == context.end()) {
            static thread_local char errBuf[256];
            snprintf(errBuf, sizeof(errBuf), "Missing: %s", req.c_str());
            return TemplateResult::error(errBuf);
        }
    }

    return TemplateResult::ok("Validation passed");
}

// ============================================================================
// Custom Filters
// ============================================================================

void PromptTemplateEngine::registerFilter(const std::string& name,
                                           FilterFunc func, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filters[name] = {func, userData};
}

// ============================================================================
// FIM Builder
// ============================================================================

TemplateResult PromptTemplateEngine::buildFIM(const std::string& prefix,
                                               const std::string& suffix,
                                               const std::string& context,
                                               const std::string& modelFormat) const {
    TemplateContext ctx;
    ctx["prefix"]  = prefix;
    ctx["suffix"]  = suffix;
    ctx["context"] = context;

    std::string templateId;
    if (modelFormat == "qwen" || modelFormat == "qwen2.5") {
        templateId = "fim_qwen";
    } else if (modelFormat == "codellama") {
        templateId = "fim_codellama";
    } else if (modelFormat == "deepseek") {
        templateId = "fim_deepseek";
    } else {
        templateId = "fim_qwen"; // Default
    }

    return render(templateId, ctx);
}

// ============================================================================
// Chat Builder
// ============================================================================

TemplateResult PromptTemplateEngine::buildChat(const std::vector<ChatMessage>& messages,
                                                const std::string& modelFormat) const {
    std::ostringstream oss;

    if (modelFormat == "chatml" || modelFormat == "qwen") {
        for (const auto& msg : messages) {
            oss << "<|im_start|>" << msg.role << "\n"
                << msg.content << "<|im_end|>\n";
        }
        oss << "<|im_start|>assistant\n";
    } else if (modelFormat == "llama2") {
        bool firstUser = true;
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                oss << "<<SYS>>\n" << msg.content << "\n<</SYS>>\n\n";
            } else if (msg.role == "user") {
                if (firstUser) {
                    oss << "[INST] " << msg.content << " [/INST]";
                    firstUser = false;
                } else {
                    oss << "\n[INST] " << msg.content << " [/INST]";
                }
            } else if (msg.role == "assistant") {
                oss << " " << msg.content << " ";
            }
        }
    } else if (modelFormat == "llama3") {
        for (const auto& msg : messages) {
            oss << "<|start_header_id|>" << msg.role << "<|end_header_id|>\n\n"
                << msg.content << "<|eot_id|>\n";
        }
        oss << "<|start_header_id|>assistant<|end_header_id|>\n\n";
    } else {
        // Generic: just concatenate
        for (const auto& msg : messages) {
            oss << msg.role << ": " << msg.content << "\n";
        }
        oss << "assistant: ";
    }

    std::string output = oss.str();
    return TemplateResult::ok(std::move(output), estimateTokens(output));
}

// ============================================================================
// Token Estimation
// ============================================================================

int PromptTemplateEngine::estimateTokens(const std::string& text) const {
    if (text.empty()) return 0;
    int count = 0;
    bool inWord = false;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            if (!inWord) { ++count; inWord = true; }
        } else {
            inWord = false;
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') ++count;
        }
    }
    return static_cast<int>(count * 1.3f);
}

} // namespace Prompt
} // namespace RawrXD
