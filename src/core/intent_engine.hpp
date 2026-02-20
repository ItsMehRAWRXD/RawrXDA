// ============================================================================
// intent_engine.hpp — User Intent Classification & Routing Engine
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Classifies user input into actionable intents and routes to the appropriate
// subsystem (AI inference, file ops, agent tasks, debugging, etc.)
// Uses keyword pattern matching + confidence scoring + fallback chain.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_INTENT_ENGINE_HPP
#define RAWRXD_INTENT_ENGINE_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>

// ============================================================================
// INTENT TYPES — All recognized user intent categories
// ============================================================================

enum class IntentType : uint32_t {
    Unknown          = 0,

    // Code operations
    CodeGenerate     = 0x0100,
    CodeExplain      = 0x0101,
    CodeRefactor     = 0x0102,
    CodeDebug        = 0x0103,
    CodeOptimize     = 0x0104,
    CodeComplete     = 0x0105,
    CodeReview       = 0x0106,
    CodeFixErrors    = 0x0107,
    CodeGenerateDocs = 0x0108,
    CodeGenerateTests= 0x0109,

    // File operations
    FileOpen         = 0x0200,
    FileCreate       = 0x0201,
    FileSave         = 0x0202,
    FileClose        = 0x0203,
    FileSearch       = 0x0204,
    FileNavigate     = 0x0205,

    // Model operations
    ModelLoad        = 0x0300,
    ModelUnload      = 0x0301,
    ModelSwitch      = 0x0302,
    ModelConfigure   = 0x0303,
    ModelInference   = 0x0304,

    // Agent tasks
    AgentExecute     = 0x0400,
    AgentPlan        = 0x0401,
    AgentLoop        = 0x0402,
    AgentChain       = 0x0403,
    AgentSwarm       = 0x0404,
    AgentMemory      = 0x0405,

    // Search & analysis
    Search           = 0x0500,
    Analyze          = 0x0501,
    Profile          = 0x0502,
    Audit            = 0x0503,

    // Chat & interactive
    Chat             = 0x0600,
    Question         = 0x0601,
    Greeting         = 0x0602,

    // System / IDE
    SystemSettings   = 0x0700,
    SystemTheme      = 0x0701,
    SystemTerminal   = 0x0702,
    SystemBuild      = 0x0703,
    SystemDebug      = 0x0704,
    SystemGit        = 0x0705,

    // Hotpatch
    HotpatchApply    = 0x0800,
    HotpatchRevert   = 0x0801,
    HotpatchStatus   = 0x0802,

    // Reverse engineering
    ReverseEngineer  = 0x0900,
    Decompile        = 0x0901,
    Disassemble      = 0x0902,

    // Voice
    VoiceCommand     = 0x0A00,
    VoiceDictation   = 0x0A01,

    // Help
    Help             = 0x0B00,
    Documentation    = 0x0B01,

    // Multi-intent (compound)
    MultiIntent      = 0xFF00,
};

// ============================================================================
// INTENT CLASSIFICATION — Result of intent analysis
// ============================================================================

struct IntentClassification {
    IntentType   primaryIntent;           // Highest-confidence intent
    float        primaryConfidence;       // 0.0 - 1.0
    IntentType   secondaryIntent;         // Fallback intent
    float        secondaryConfidence;     // 0.0 - 1.0
    const char*  extractedSubject;        // Extracted subject/target (e.g., filename, code snippet ref)
    const char*  extractedAction;         // Extracted action verb
    uint32_t     flags;                   // Intent-specific flags
    bool         isAmbiguous;             // True if top-2 intents are close in confidence
    bool         requiresContext;         // True if intent needs more info to resolve

    static IntentClassification unknown() {
        return {IntentType::Unknown, 0.0f, IntentType::Unknown, 0.0f,
                "", "", 0, false, false};
    }
};

// ============================================================================
// INTENT PATTERN — Keyword/regex pattern mapped to an intent
// ============================================================================

struct IntentPattern {
    const char*  keyword;                 // Trigger keyword/phrase
    IntentType   intent;                  // Mapped intent
    float        baseWeight;              // Base confidence weight (0.0 - 1.0)
    bool         requiresArgument;        // Needs following argument
    bool         caseSensitive;           // Case-sensitive match
};

// ============================================================================
// INTENT ROUTING — Callback registered per intent type
// ============================================================================

struct IntentRoute {
    IntentType   intent;
    const char*  subsystemName;           // "ai_inference", "file_ops", "agent", etc.
    std::function<void(const IntentClassification&, const std::string& rawInput)> handler;
    float        minConfidence;           // Minimum confidence to trigger this route
};

// ============================================================================
// INTENT ENGINE — Main classification + routing engine
// ============================================================================

class IntentEngine {
public:
    IntentEngine();
    ~IntentEngine();

    // ---- Classification API ----
    IntentClassification classify(const std::string& userInput) const;
    IntentClassification classifyWithContext(const std::string& userInput,
                                             const std::string& conversationHistory,
                                             const std::string& currentFile) const;

    // ---- Routing API ----
    bool route(const IntentClassification& classification,
               const std::string& rawInput) const;

    void registerRoute(IntentType intent, const char* subsystem,
                       std::function<void(const IntentClassification&, const std::string&)> handler,
                       float minConfidence = 0.3f);

    void removeRoute(IntentType intent);

    // ---- Pattern Management ----
    void addPattern(const IntentPattern& pattern);
    void clearPatterns();
    void loadDefaultPatterns();

    // ---- Diagnostics ----
    size_t patternCount() const;
    size_t routeCount() const;
    uint64_t totalClassifications() const { return m_totalClassifications.load(); }
    uint64_t totalRouted() const { return m_totalRouted.load(); }
    std::string diagnosticReport() const;

    // ---- Intent name lookup ----
    static const char* intentName(IntentType type);
    static IntentType intentFromName(const char* name);

private:
    // Pattern matching
    float matchPatterns(const std::string& input, IntentType intent) const;
    std::vector<std::pair<IntentType, float>> scoreAllIntents(const std::string& input) const;
    void applyContextBoosts(std::vector<std::pair<IntentType, float>>& scores,
                            const std::string& conversationHistory,
                            const std::string& currentFile) const;

    // Tokenization
    std::vector<std::string> tokenize(const std::string& input) const;
    std::string toLower(const std::string& s) const;
    bool containsWord(const std::string& haystack, const char* needle) const;

    // Data
    std::vector<IntentPattern>                              m_patterns;
    std::unordered_map<uint32_t, IntentRoute>               m_routes;     // keyed by IntentType
    mutable std::mutex                                      m_mutex;
    mutable std::atomic<uint64_t>                           m_totalClassifications{0};
    mutable std::atomic<uint64_t>                           m_totalRouted{0};

    // Scratch buffers (reused to avoid allocation)
    mutable std::string                                     m_scratchLower;
    mutable std::string                                     m_scratchSubject;
    mutable std::string                                     m_scratchAction;
};

// ============================================================================
// GLOBAL SINGLETON
// ============================================================================

IntentEngine& getIntentEngine();

#endif // RAWRXD_INTENT_ENGINE_HPP
