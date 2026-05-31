// ============================================================================
// smart_completion.h — Smart Code Completion Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// Reverse-engineered from GitHub Copilot, Tabnine, and Codeium with:
// - Context-aware completion suggestions
// - Multi-line code generation
// - Pattern-based completion
// - Language-specific rules
// - Fuzzy matching and ranking
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <chrono>
#include <regex>

namespace RawrXD {
namespace Completion {

// ============================================================================
// Core Types
// ============================================================================

struct Position {
    uint32_t line;
    uint32_t column;
    
    bool operator==(const Position& other) const {
        return line == other.line && column == other.column;
    }
    
    bool operator<(const Position& other) const {
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

struct Range {
    Position start;
    Position end;
    
    bool contains(const Position& pos) const {
        return start <= pos && pos <= end;
    }
};

struct CompletionContext {
    std::string uri;
    std::string language;
    std::string textBeforeCursor;
    std::string textAfterCursor;
    Position position;
    std::vector<std::string> recentLines;
    std::vector<std::string> importedSymbols;
    std::unordered_map<std::string, std::string> definedSymbols;
    std::vector<std::string> openFiles;
    std::chrono::system_clock::time_point timestamp;
};

struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    std::string documentation;
    std::string sortText;
    std::string filterText;
    
    enum class Kind {
        Text,
        Method,
        Function,
        Constructor,
        Field,
        Variable,
        Class,
        Interface,
        Module,
        Property,
        Unit,
        Value,
        Enum,
        Keyword,
        Snippet,
        Color,
        File,
        Reference,
        Folder,
        EnumMember,
        Constant,
        Struct,
        Event,
        Operator,
        TypeParameter
    } kind;
    
    enum class InsertTextFormat {
        PlainText,
        Snippet
    } insertTextFormat = InsertTextFormat::PlainText;
    
    // Additional metadata
    float score = 0.0f;
    bool isRecommended = false;
    bool isIncomplete = false;
    std::vector<std::string> commitCharacters;
    std::string data; // Custom data for resolution
    
    // Multi-line support
    bool isMultiLine = false;
    std::vector<std::string> additionalLines;
};

struct CompletionList {
    bool isIncomplete;
    std::vector<CompletionItem> items;
    std::chrono::system_clock::time_point timestamp;
};

struct CompletionSettings {
    bool enableMultiLine = true;
    bool enableSnippets = true;
    bool enablePatternMatching = true;
    bool enableFuzzyMatching = true;
    uint32_t maxSuggestions = 50;
    uint32_t maxMultiLineLength = 500;
    float minScore = 0.1f;
    std::vector<std::string> triggerCharacters;
    bool autoTrigger = true;
    uint32_t triggerDelay = 300; // milliseconds
};

// ============================================================================
// Pattern Types
// ============================================================================

struct CompletionPattern {
    std::string id;
    std::string name;
    std::regex pattern;
    std::string template_;
    std::vector<std::string> placeholders;
    std::string language;
    float priority = 1.0f;
    bool isMultiLine = false;
};

struct SnippetTemplate {
    std::string name;
    std::string prefix;
    std::string body;
    std::string description;
    std::string language;
    std::vector<std::string> placeholders;
};

// ============================================================================
// Language-Specific Rules
// ============================================================================

struct LanguageRules {
    std::string language;
    std::vector<std::string> keywords;
    std::vector<std::string> builtins;
    std::vector<std::string> types;
    std::vector<std::string> operators;
    std::vector<std::string> commentPatterns;
    std::vector<std::string> stringPatterns;
    std::vector<std::string> blockStart;
    std::vector<std::string> blockEnd;
    std::string functionPattern;
    std::string classPattern;
    std::string variablePattern;
    std::string importPattern;
    std::string exportPattern;
};

// ============================================================================
// Ranking and Scoring
// ============================================================================

struct ScoringFactors {
    float contextMatch = 0.0f;      // How well it matches surrounding context
    float frequency = 0.0f;         // How often this completion is used
    float recency = 0.0f;           // How recently it was used
    float typeMatch = 0.0f;         // Type compatibility
    float scopeMatch = 0.0f;        // Scope visibility
    float semanticMatch = 0.0f;    // Semantic relevance
    float patternMatch = 0.0f;      // Pattern-based match
    float fuzzyMatch = 0.0f;       // Fuzzy string match
};

struct CompletionScore {
    float total = 0.0f;
    ScoringFactors factors;
    std::string explanation;
};

// ============================================================================
// Context Analysis
// ============================================================================

struct ContextAnalysis {
    std::string currentScope;
    std::string currentFunction;
    std::string currentClass;
    std::vector<std::string> visibleVariables;
    std::vector<std::string> visibleFunctions;
    std::vector<std::string> visibleClasses;
    std::vector<std::string> expectedTypes;
    std::vector<std::string> recentEdits;
    std::string indentation;
    bool isInFunction = false;
    bool isInClass = false;
    bool isInLoop = false;
    bool isInCondition = false;
    bool isAfterDot = false;
    bool isAfterNew = false;
    bool isAfterReturn = false;
};

// ============================================================================
// Completion Provider Interface
// ============================================================================

class ICompletionProvider {
public:
    virtual ~ICompletionProvider() = default;
    
    virtual std::string name() const = 0;
    virtual std::vector<std::string> languages() const = 0;
    virtual bool canProvide(const CompletionContext& context) const = 0;
    virtual CompletionList provideCompletions(const CompletionContext& context) = 0;
    virtual std::optional<CompletionItem> resolveCompletion(const CompletionItem& item) = 0;
};

// ============================================================================
// Smart Completion Engine Interface
// ============================================================================

class ISmartCompletionEngine {
public:
    virtual ~ISmartCompletionEngine() = default;
    
    // Configuration
    virtual void setSettings(const CompletionSettings& settings) = 0;
    virtual CompletionSettings getSettings() const = 0;
    
    // Main completion API
    virtual CompletionList getCompletions(const CompletionContext& context) = 0;
    virtual std::optional<CompletionItem> resolveCompletion(const CompletionItem& item) = 0;
    
    // Provider management
    virtual void registerProvider(std::unique_ptr<ICompletionProvider> provider) = 0;
    virtual void unregisterProvider(const std::string& name) = 0;
    virtual std::vector<std::string> getProviders() const = 0;
    
    // Pattern management
    virtual void addPattern(const CompletionPattern& pattern) = 0;
    virtual void removePattern(const std::string& patternId) = 0;
    virtual std::vector<CompletionPattern> getPatterns(const std::string& language) const = 0;
    
    // Snippet management
    virtual void addSnippet(const SnippetTemplate& snippet) = 0;
    virtual void removeSnippet(const std::string& name) = 0;
    virtual std::vector<SnippetTemplate> getSnippets(const std::string& language) const = 0;
    
    // Language rules
    virtual void setLanguageRules(const std::string& language, const LanguageRules& rules) = 0;
    virtual std::optional<LanguageRules> getLanguageRules(const std::string& language) const = 0;
    
    // Context analysis
    virtual ContextAnalysis analyzeContext(const CompletionContext& context) const = 0;
    
    // Scoring
    virtual CompletionScore scoreCompletion(const CompletionItem& item,
                                          const CompletionContext& context) const = 0;
    virtual std::vector<CompletionItem> rankCompletions(
        std::vector<CompletionItem> items,
        const CompletionContext& context) const = 0;
    
    // Statistics
    virtual void recordUsage(const std::string& completionId) = 0;
    virtual std::unordered_map<std::string, uint32_t> getUsageStatistics() const = 0;
    
    // Caching
    virtual void clearCache() = 0;
    virtual void invalidateFile(const std::string& uri) = 0;
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<ISmartCompletionEngine> createSmartCompletionEngine();

} // namespace Completion
} // namespace RawrXD
