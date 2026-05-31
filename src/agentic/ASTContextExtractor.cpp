// ============================================================================
// AST Context Extractor v1 - Implementation
// ============================================================================

#include "ASTContextExtractor.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <unordered_set>

namespace RawrXD::Agentic {

// ============================================================================
// Internal Implementation
// ============================================================================
struct ASTContextExtractor::Impl {
    ModelCapabilityProfile modelProfile;
    ExecutionMode executionMode = ExecutionMode::Safe;
    uint64_t focusLinesBefore = 15;
    uint64_t focusLinesAfter = 15;
    uint64_t maxPromptTokens = 8192;
    bool initialized = false;

    // C++ symbol extraction patterns
    std::regex functionPattern{
        R"((?:^|\n)\s*(?:(?:inline|static|virtual|explicit|constexpr|consteval)\s+)*"
        R"((?:(?:[\w:]+\s*::\s*)*[\w:]+\s+)?)"
        R"((?:[\w:]+\s*::\s*)*~?[\w:]+\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:\s*=\s*0\s*)?(?:\s*;|\s*\{))"
    };

    std::regex classPattern{
        R"((?:^|\n)\s*(?:class|struct)\s+(?:[\w_][\w\d_]*\s*)?(?:[\w_][\w\d_]*)"
        R"(\s*(?::\s*(?:public|protected|private)\s+[\w_][\w\d_]*\s*)?\s*\{)"
    };

    std::regex enumPattern{
        R"((?:^|\n)\s*enum\s+(?:class\s+)?[\w_][\w\d_]*\s*(?::\s*[\w_][\w\d_]*\s*)?\s*\{)"
    };

    std::regex namespacePattern{
        R"((?:^|\n)\s*namespace\s+([\w_][\w\d_]*)\s*\{)"
    };

    std::regex typedefPattern{
        R"((?:^|\n)\s*using\s+([\w_][\w\d_]*)\s*=\s*[^;]+;)"
    };

    std::regex docCommentPattern{
        R"(/\*\*(.*?)\*/|///(.*)$)"
    };
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
ASTContextExtractor::ASTContextExtractor() : m_impl(std::make_unique<Impl>()) {}
ASTContextExtractor::~ASTContextExtractor() = default;

// ============================================================================
// Configuration
// ============================================================================
void ASTContextExtractor::setModelProfile(const ModelCapabilityProfile& profile) {
    m_impl->modelProfile = profile;
    m_impl->initialized = true;

    // Auto-tune focus window based on model size
    if (profile.contextWindowTokens > 0) {
        // Use 25% of context window for code context, rest for response
        m_impl->maxPromptTokens = static_cast<uint64_t>(profile.contextWindowTokens * 0.25);

        // Scale focus window: larger models can handle more context
        if (profile.parameterCount >= 70'000'000'000ULL) {  // 70B+
            m_impl->focusLinesBefore = 50;
            m_impl->focusLinesAfter = 50;
        } else if (profile.parameterCount >= 8'000'000'000ULL) {  // 8B+
            m_impl->focusLinesBefore = 30;
            m_impl->focusLinesAfter = 30;
        } else {
            m_impl->focusLinesBefore = 15;
            m_impl->focusLinesAfter = 15;
        }
    }
}

void ASTContextExtractor::setExecutionMode(ExecutionMode mode) {
    m_impl->executionMode = mode;
}

void ASTContextExtractor::setFocusWindowSize(uint64_t linesBefore, uint64_t linesAfter) {
    m_impl->focusLinesBefore = linesBefore;
    m_impl->focusLinesAfter = linesAfter;
}

void ASTContextExtractor::setMaxPromptTokens(uint64_t maxTokens) {
    m_impl->maxPromptTokens = maxTokens;
}

// ============================================================================
// Symbol Extraction
// ============================================================================
std::vector<Symbol> ASTContextExtractor::extractSymbols(const std::string& code,
                                                            const std::string& language) {
    std::vector<Symbol> symbols;

    if (language == "cpp" || language == "c++" || language == "c") {
        symbols = QuickExtractCppSymbols(code);
    }
    // TODO: Add Python, JavaScript, Rust, Go patterns

    return symbols;
}

Symbol ASTContextExtractor::extractSymbolAtLine(const std::string& code,
                                                const std::string& language,
                                                uint64_t line) {
    auto symbols = extractSymbols(code, language);
    for (const auto& sym : symbols) {
        if (sym.lineStart <= line && sym.lineEnd >= line) {
            return sym;
        }
    }
    return Symbol{};
}

// ============================================================================
// Focus Window Construction
// ============================================================================
FocusWindow ASTContextExtractor::buildFocusWindow(const std::string& code,
                                                    const std::string& language,
                                                    uint64_t cursorLine,
                                                    const std::string& errorContext) {
    FocusWindow window;
    window.cursorLine = cursorLine;
    window.contextLinesBefore = m_impl->focusLinesBefore;
    window.contextLinesAfter = m_impl->focusLinesAfter;
    window.errorContext = errorContext;

    // Split code into lines
    std::vector<std::string> lines;
    std::istringstream stream(code);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) {
        return window;
    }

    // Calculate line range
    uint64_t startLine = (cursorLine > m_impl->focusLinesBefore) ?
                         (cursorLine - m_impl->focusLinesBefore) : 0;
    uint64_t endLine = std::min(cursorLine + m_impl->focusLinesAfter,
                                static_cast<uint64_t>(lines.size()));

    // Extract surrounding code
    std::ostringstream surrounding;
    for (uint64_t i = startLine; i < endLine && i < lines.size(); ++i) {
        surrounding << lines[i] << "\n";
    }
    window.surroundingCode = surrounding.str();

    // Extract symbols in focus area
    auto allSymbols = extractSymbols(code, language);
    for (const auto& sym : allSymbols) {
        // Include symbol if it overlaps with focus window
        if ((sym.lineStart >= startLine && sym.lineStart <= endLine) ||
            (sym.lineEnd >= startLine && sym.lineEnd <= endLine) ||
            (sym.lineStart <= startLine && sym.lineEnd >= endLine)) {
            window.relevantSymbols.push_back(sym);
        }
    }

    return window;
}

// ============================================================================
// Prompt Assembly
// ============================================================================
std::string ASTContextExtractor::assemblePrompt(const std::string& task,
                                                const FocusWindow& focus,
                                                const std::vector<Symbol>& globalSymbols,
                                                const std::string& additionalContext) {
    std::ostringstream prompt;

    // System instruction
    prompt << "You are a Sovereign Systems Engineer. Analyze the provided code context and "
              << task << ".\n\n";

    // Execution mode notice
    switch (m_impl->executionMode) {
        case ExecutionMode::Safe:
            prompt << "[MODE: SAFE - Propose changes only, do not apply]\n";
            break;
        case ExecutionMode::Normal:
            prompt << "[MODE: NORMAL - Apply after confirmation]\n";
            break;
        case ExecutionMode::Unsafe:
            prompt << "[MODE: UNSAFE - Direct apply, no confirmation]\n";
            break;
        case ExecutionMode::Kernel:
            prompt << "[MODE: KERNEL - System-level access]\n";
            break;
    }

    // Error context if present
    if (!focus.errorContext.empty()) {
        prompt << "[ERROR CONTEXT]\n" << focus.errorContext << "\n\n";
    }

    // Focus window
    prompt << "[FOCUS WINDOW - Lines " << (focus.cursorLine - focus.contextLinesBefore)
              << "-" << (focus.cursorLine + focus.contextLinesAfter) << "]\n";
    prompt << "```\n" << focus.surroundingCode << "```\n\n";

    // Relevant symbols
    if (!focus.relevantSymbols.empty()) {
        prompt << "[RELEVANT SYMBOLS]\n";
        for (const auto& sym : focus.relevantSymbols) {
            prompt << "- " << sym.name;
            if (!sym.signature.empty()) {
                prompt << " : " << sym.signature;
            }
            prompt << " (lines " << sym.lineStart << "-" << sym.lineEnd << ")\n";
        }
        prompt << "\n";
    }

    // Global symbol references
    if (!globalSymbols.empty()) {
        prompt << "[GLOBAL CONTEXT]\n";
        for (const auto& sym : globalSymbols) {
            if (sym.tokenCount < 200) {  // Only include small symbols
                prompt << "// " << sym.name << ":\n";
                prompt << "```\n" << sym.signature << "\n```\n";
            }
        }
        prompt << "\n";
    }

    // Additional context
    if (!additionalContext.empty()) {
        prompt << "[ADDITIONAL CONTEXT]\n" << additionalContext << "\n\n";
    }

    // Task instruction
    prompt << "[TASK] " << task << "\n\n";
    prompt << "Return only the result. No explanations unless requested.\n";

    return prompt.str();
}

// ============================================================================
// Context Compaction
// ============================================================================
std::string ASTContextExtractor::compactContext(const std::string& context, uint64_t targetTokens) {
    uint64_t currentTokens = estimateTokens(context);
    if (currentTokens <= targetTokens) {
        return context;
    }

    // Strategy: Remove doc comments first, then body content, keep signatures
    std::string compacted = context;

    // Remove doc comments
    std::regex docComment{R"(/\*\*.*?\*/|///.*?$)"};
    compacted = std::regex_replace(compacted, docComment, "// ...\n");

    currentTokens = estimateTokens(compacted);
    if (currentTokens <= targetTokens) {
        return compacted;
    }

    // Truncate function bodies to signatures only
    std::regex bodyPattern{R"((\{)[^}]*?(\}))"};
    compacted = std::regex_replace(compacted, bodyPattern, "{ ... }");

    currentTokens = estimateTokens(compacted);
    if (currentTokens <= targetTokens) {
        return compacted;
    }

    // Final fallback: hard truncate with ellipsis
    uint64_t charLimit = static_cast<uint64_t>(targetTokens * 3.5);  // ~3.5 chars/token avg
    if (compacted.length() > charLimit) {
        compacted = compacted.substr(0, static_cast<size_t>(charLimit)) + "\n... [truncated]\n";
    }

    return compacted;
}

// ============================================================================
// Execution Mode Checks
// ============================================================================
bool ASTContextExtractor::canAutoApply() const {
    return m_impl->executionMode == ExecutionMode::Unsafe ||
           m_impl->executionMode == ExecutionMode::Kernel;
}

bool ASTContextExtractor::requiresConfirmation() const {
    return m_impl->executionMode == ExecutionMode::Normal;
}

bool ASTContextExtractor::isKernelMode() const {
    return m_impl->executionMode == ExecutionMode::Kernel;
}

// ============================================================================
// Model-Aware Sizing
// ============================================================================
uint64_t ASTContextExtractor::getOptimalContextLines() const {
    return m_impl->focusLinesBefore + m_impl->focusLinesAfter;
}

uint64_t ASTContextExtractor::getMaxPromptTokens() const {
    return m_impl->maxPromptTokens;
}

uint64_t ASTContextExtractor::estimateTokens(const std::string& text) const {
    return EstimateTokenCount(text, m_impl->modelProfile.parameterCount);
}

// ============================================================================
// Unbounded Model Support
// ============================================================================
bool ASTContextExtractor::canHandleModelSize(uint64_t parameterCount) const {
    // Structural support: no hard limit, but warn if > 10T
    // 1.8T = 1,800,000,000,000 parameters
    return true;  // Architecture supports any size
}

uint64_t ASTContextExtractor::getRecommendedBatchSize(uint64_t parameterCount) const {
    // Scale batch size inversely with model size
    if (parameterCount >= 1'000'000'000'000ULL) {  // 1T+
        return 1;  // Single token at a time for massive models
    } else if (parameterCount >= 100'000'000'000ULL) {  // 100B+
        return 4;
    } else if (parameterCount >= 10'000'000'000ULL) {  // 10B+
        return 16;
    } else {
        return 64;  // Small models can batch heavily
    }
}

uint64_t ASTContextExtractor::getRecommendedContextWindow(uint64_t parameterCount) const {
    // Larger models typically have larger context windows
    if (parameterCount >= 500'000'000'000ULL) {  // 500B+
        return 256'000;  // 256k tokens
    } else if (parameterCount >= 100'000'000'000ULL) {  // 100B+
        return 128'000;  // 128k tokens
    } else if (parameterCount >= 10'000'000'000ULL) {  // 10B+
        return 32'768;   // 32k tokens
    } else {
        return 8'192;    // 8k tokens for small models
    }
}

// ============================================================================
// Global Instance
// ============================================================================
ASTContextExtractor& GetASTContextExtractor() {
    static ASTContextExtractor instance;
    return instance;
}

// ============================================================================
// Quick C++ Symbol Extraction
// ============================================================================
std::vector<Symbol> QuickExtractCppSymbols(const std::string& code) {
    std::vector<Symbol> symbols;
    std::istringstream stream(code);
    std::string line;
    uint64_t lineNum = 0;

    // Simple state machine for C++ parsing
    enum class ParseState {
        TopLevel,
        InFunction,
        InClass,
        InComment,
        InString
    };

    ParseState state = ParseState::TopLevel;
    Symbol currentSymbol;
    int braceDepth = 0;
    std::ostringstream currentBody;
    uint64_t symbolStartLine = 0;

    auto flushSymbol = [&]() {
        if (!currentSymbol.name.empty()) {
            currentSymbol.lineEnd = lineNum;
            currentSymbol.body = currentBody.str();
            currentSymbol.tokenCount = EstimateTokenCount(currentSymbol.body);
            symbols.push_back(currentSymbol);
        }
        currentSymbol = Symbol{};
        currentBody.str("");
        currentBody.clear();
    };

    while (std::getline(stream, line)) {
        ++lineNum;

        // Track comments
        size_t commentPos = line.find("//");
        size_t blockCommentStart = line.find("/*");
        size_t blockCommentEnd = line.find("*/");

        std::string codePart = line;
        if (commentPos != std::string::npos) {
            codePart = line.substr(0, commentPos);
        }

        // Check for function/class/struct definitions
        if (state == ParseState::TopLevel) {
            std::regex funcRegex{
                R"(\s*(?:(?:inline|static|virtual|explicit|constexpr|consteval|constinit)\s+)*"
                R"((?:[\w:]+\s+)?(?:[\w:]+\s*::\s*)*~?[\w:]+\s*\([^)]*\)\s*(?:const\s*)?"
                R"((?:noexcept\s*)?(?:override\s*)?(?:final\s*)?(?:\s*=\s*0\s*)?(?:\s*;|\s*\{))"
            };

            std::regex classRegex{
                R"(\s*(?:class|struct)\s+(?:[\w_][\w\d_]*\s*)?(?:[\w_][\w\d_]*)"
                R"(\s*(?::\s*(?:public|protected|private)\s+[\w_][\w\d_]*\s*)?\s*\{)"
            };

            std::smatch match;
            if (std::regex_search(codePart, match, funcRegex)) {
                currentSymbol.type = SymbolType::Function;
                currentSymbol.name = "function_" + std::to_string(lineNum);
                currentSymbol.signature = match[0];
                currentSymbol.lineStart = lineNum;
                symbolStartLine = lineNum;
                state = ParseState::InFunction;
                braceDepth = 1;
                currentBody << line << "\n";
            } else if (std::regex_search(codePart, match, classRegex)) {
                currentSymbol.type = SymbolType::Class;
                currentSymbol.name = "class_" + std::to_string(lineNum);
                currentSymbol.signature = match[0];
                currentSymbol.lineStart = lineNum;
                symbolStartLine = lineNum;
                state = ParseState::InClass;
                braceDepth = 1;
                currentBody << line << "\n";
            }
        } else if (state == ParseState::InFunction || state == ParseState::InClass) {
            currentBody << line << "\n";

            // Track brace depth
            for (char c : codePart) {
                if (c == '{') ++braceDepth;
                else if (c == '}') --braceDepth;
            }

            if (braceDepth == 0) {
                flushSymbol();
                state = ParseState::TopLevel;
            }
        }
    }

    // Flush any remaining symbol
    flushSymbol();

    return symbols;
}

// ============================================================================
// Token Estimation
// ============================================================================
uint64_t EstimateTokenCount(const std::string& text, uint64_t modelParameterCount) {
    // Model-aware token estimation
    // Larger models often use different tokenizers with varying efficiency

    double charsPerToken = 3.5;  // Default heuristic

    if (modelParameterCount >= 1'000'000'000'000ULL) {  // 1T+
        charsPerToken = 4.2;  // Advanced tokenizers (e.g., GPT-4 style)
    } else if (modelParameterCount >= 100'000'000'000ULL) {  // 100B+
        charsPerToken = 3.8;
    } else if (modelParameterCount >= 10'000'000'000ULL) {  // 10B+
        charsPerToken = 3.5;
    } else {
        charsPerToken = 3.0;  // Smaller models, simpler tokenizers
    }

    // Adjust for code density
    bool isCode = (text.find("{") != std::string::npos ||
                   text.find(";") != std::string::npos ||
                   text.find("//") != std::string::npos);
    if (isCode) {
        charsPerToken *= 0.85;  // Code has more tokens per char
    }

    return static_cast<uint64_t>(text.length() / charsPerToken) + 1;
}

} // namespace RawrXD::Agentic
