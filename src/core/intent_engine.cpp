// ============================================================================
// intent_engine.cpp — User Intent Classification & Routing Engine
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Keyword-based intent classification with confidence scoring, context
// boosting, and subsystem routing via registered callbacks.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "intent_engine.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cmath>
#include <sstream>
#include <numeric>

// ============================================================================
// INTENT NAME TABLE
// ============================================================================

struct IntentNameEntry {
    IntentType   type;
    const char*  name;
};

static constexpr IntentNameEntry s_intentNames[] = {
    { IntentType::Unknown,          "unknown" },
    { IntentType::CodeGenerate,     "code.generate" },
    { IntentType::CodeExplain,      "code.explain" },
    { IntentType::CodeRefactor,     "code.refactor" },
    { IntentType::CodeDebug,        "code.debug" },
    { IntentType::CodeOptimize,     "code.optimize" },
    { IntentType::CodeComplete,     "code.complete" },
    { IntentType::CodeReview,       "code.review" },
    { IntentType::CodeFixErrors,    "code.fixErrors" },
    { IntentType::CodeGenerateDocs, "code.generateDocs" },
    { IntentType::CodeGenerateTests,"code.generateTests" },
    { IntentType::FileOpen,         "file.open" },
    { IntentType::FileCreate,       "file.create" },
    { IntentType::FileSave,         "file.save" },
    { IntentType::FileClose,        "file.close" },
    { IntentType::FileSearch,       "file.search" },
    { IntentType::FileNavigate,     "file.navigate" },
    { IntentType::ModelLoad,        "model.load" },
    { IntentType::ModelUnload,      "model.unload" },
    { IntentType::ModelSwitch,      "model.switch" },
    { IntentType::ModelConfigure,   "model.configure" },
    { IntentType::ModelInference,   "model.inference" },
    { IntentType::AgentExecute,     "agent.execute" },
    { IntentType::AgentPlan,        "agent.plan" },
    { IntentType::AgentLoop,        "agent.loop" },
    { IntentType::AgentChain,       "agent.chain" },
    { IntentType::AgentSwarm,       "agent.swarm" },
    { IntentType::AgentMemory,      "agent.memory" },
    { IntentType::Search,           "search" },
    { IntentType::Analyze,          "analyze" },
    { IntentType::Profile,          "profile" },
    { IntentType::Audit,            "audit" },
    { IntentType::Chat,             "chat" },
    { IntentType::Question,         "question" },
    { IntentType::Greeting,         "greeting" },
    { IntentType::SystemSettings,   "system.settings" },
    { IntentType::SystemTheme,      "system.theme" },
    { IntentType::SystemTerminal,   "system.terminal" },
    { IntentType::SystemBuild,      "system.build" },
    { IntentType::SystemDebug,      "system.debug" },
    { IntentType::SystemGit,        "system.git" },
    { IntentType::HotpatchApply,    "hotpatch.apply" },
    { IntentType::HotpatchRevert,   "hotpatch.revert" },
    { IntentType::HotpatchStatus,   "hotpatch.status" },
    { IntentType::ReverseEngineer,  "reverse.engineer" },
    { IntentType::Decompile,        "decompile" },
    { IntentType::Disassemble,      "disassemble" },
    { IntentType::VoiceCommand,     "voice.command" },
    { IntentType::VoiceDictation,   "voice.dictation" },
    { IntentType::Help,             "help" },
    { IntentType::Documentation,    "documentation" },
    { IntentType::MultiIntent,      "multi" },
};

static constexpr size_t s_intentNameCount = sizeof(s_intentNames) / sizeof(s_intentNames[0]);

// ============================================================================
// DEFAULT PATTERN TABLE — Keyword → Intent mappings with weights
// ============================================================================

static const IntentPattern s_defaultPatterns[] = {
    // --- Code generation ---
    { "generate",        IntentType::CodeGenerate,      0.85f, true,  false },
    { "create function", IntentType::CodeGenerate,      0.90f, true,  false },
    { "write code",      IntentType::CodeGenerate,      0.90f, false, false },
    { "implement",       IntentType::CodeGenerate,      0.80f, true,  false },
    { "scaffold",        IntentType::CodeGenerate,      0.85f, true,  false },
    { "boilerplate",     IntentType::CodeGenerate,      0.80f, false, false },
    { "stub",            IntentType::CodeGenerate,      0.70f, true,  false },
    { "code for",        IntentType::CodeGenerate,      0.75f, true,  false },
    { "make a",          IntentType::CodeGenerate,      0.60f, true,  false },
    { "create class",    IntentType::CodeGenerate,      0.90f, true,  false },
    { "add method",      IntentType::CodeGenerate,      0.85f, true,  false },

    // --- Code explanation ---
    { "explain",         IntentType::CodeExplain,       0.90f, true,  false },
    { "what does",       IntentType::CodeExplain,       0.85f, true,  false },
    { "how does",        IntentType::CodeExplain,       0.85f, true,  false },
    { "describe",        IntentType::CodeExplain,       0.80f, true,  false },
    { "walk me through", IntentType::CodeExplain,       0.90f, true,  false },
    { "break down",      IntentType::CodeExplain,       0.80f, true,  false },
    { "understand",      IntentType::CodeExplain,       0.70f, true,  false },

    // --- Code refactoring ---
    { "refactor",        IntentType::CodeRefactor,      0.95f, true,  false },
    { "restructure",     IntentType::CodeRefactor,      0.90f, true,  false },
    { "reorganize",      IntentType::CodeRefactor,      0.85f, true,  false },
    { "clean up",        IntentType::CodeRefactor,      0.80f, true,  false },
    { "simplify",        IntentType::CodeRefactor,      0.75f, true,  false },
    { "extract method",  IntentType::CodeRefactor,      0.90f, true,  false },
    { "rename",          IntentType::CodeRefactor,      0.80f, true,  false },

    // --- Code debugging ---
    { "debug",           IntentType::CodeDebug,         0.90f, false, false },
    { "breakpoint",      IntentType::CodeDebug,         0.85f, false, false },
    { "trace",           IntentType::CodeDebug,         0.70f, false, false },
    { "step through",    IntentType::CodeDebug,         0.90f, false, false },
    { "why is",          IntentType::CodeDebug,         0.65f, true,  false },
    { "crash",           IntentType::CodeDebug,         0.75f, false, false },
    { "bug",             IntentType::CodeDebug,         0.70f, false, false },
    { "error",           IntentType::CodeDebug,         0.65f, false, false },

    // --- Code optimization ---
    { "optimize",        IntentType::CodeOptimize,      0.90f, true,  false },
    { "speed up",        IntentType::CodeOptimize,      0.85f, true,  false },
    { "faster",          IntentType::CodeOptimize,      0.70f, false, false },
    { "performance",     IntentType::CodeOptimize,      0.75f, false, false },
    { "reduce memory",   IntentType::CodeOptimize,      0.85f, false, false },
    { "cache",           IntentType::CodeOptimize,      0.60f, false, false },

    // --- Code completion ---
    { "complete",        IntentType::CodeComplete,      0.80f, true,  false },
    { "autocomplete",    IntentType::CodeComplete,      0.90f, false, false },
    { "finish this",     IntentType::CodeComplete,      0.85f, false, false },
    { "continue code",   IntentType::CodeComplete,      0.85f, false, false },
    { "fill in",         IntentType::CodeComplete,      0.75f, true,  false },

    // --- Code review ---
    { "review",          IntentType::CodeReview,        0.85f, true,  false },
    { "code review",     IntentType::CodeReview,        0.95f, false, false },
    { "check code",      IntentType::CodeReview,        0.80f, false, false },
    { "audit code",      IntentType::CodeReview,        0.80f, false, false },

    // --- Fix errors ---
    { "fix",             IntentType::CodeFixErrors,     0.80f, true,  false },
    { "fix error",       IntentType::CodeFixErrors,     0.95f, false, false },
    { "fix bug",         IntentType::CodeFixErrors,     0.95f, false, false },
    { "resolve",         IntentType::CodeFixErrors,     0.75f, true,  false },
    { "patch",           IntentType::CodeFixErrors,     0.70f, true,  false },

    // --- Generate docs ---
    { "document",        IntentType::CodeGenerateDocs,  0.85f, true,  false },
    { "add comments",    IntentType::CodeGenerateDocs,  0.90f, false, false },
    { "docstring",       IntentType::CodeGenerateDocs,  0.90f, false, false },
    { "jsdoc",           IntentType::CodeGenerateDocs,  0.85f, false, false },
    { "doxygen",         IntentType::CodeGenerateDocs,  0.85f, false, false },

    // --- Generate tests ---
    { "test",            IntentType::CodeGenerateTests, 0.70f, false, false },
    { "unit test",       IntentType::CodeGenerateTests, 0.95f, false, false },
    { "write test",      IntentType::CodeGenerateTests, 0.90f, true,  false },
    { "test case",       IntentType::CodeGenerateTests, 0.90f, false, false },
    { "integration test",IntentType::CodeGenerateTests, 0.90f, false, false },

    // --- File operations ---
    { "open file",       IntentType::FileOpen,          0.90f, true,  false },
    { "open",            IntentType::FileOpen,          0.60f, true,  false },
    { "create file",     IntentType::FileCreate,        0.90f, true,  false },
    { "new file",        IntentType::FileCreate,        0.85f, false, false },
    { "save",            IntentType::FileSave,          0.80f, false, false },
    { "close",           IntentType::FileClose,         0.75f, false, false },
    { "find file",       IntentType::FileSearch,        0.85f, true,  false },
    { "search file",     IntentType::FileSearch,        0.85f, true,  false },
    { "goto",            IntentType::FileNavigate,      0.80f, true,  false },
    { "navigate",        IntentType::FileNavigate,      0.80f, true,  false },
    { "jump to",         IntentType::FileNavigate,      0.85f, true,  false },

    // --- Model operations ---
    { "load model",      IntentType::ModelLoad,         0.95f, true,  false },
    { "model load",      IntentType::ModelLoad,         0.90f, true,  false },
    { "gguf",            IntentType::ModelLoad,         0.75f, false, false },
    { "unload model",    IntentType::ModelUnload,       0.95f, false, false },
    { "switch model",    IntentType::ModelSwitch,       0.90f, true,  false },
    { "configure model", IntentType::ModelConfigure,    0.90f, false, false },
    { "inference",       IntentType::ModelInference,    0.85f, false, false },
    { "run model",       IntentType::ModelInference,    0.80f, false, false },
    { "predict",         IntentType::ModelInference,    0.75f, false, false },

    // --- Agent tasks ---
    { "agent",           IntentType::AgentExecute,      0.70f, false, false },
    { "execute agent",   IntentType::AgentExecute,      0.90f, false, false },
    { "plan",            IntentType::AgentPlan,         0.75f, true,  false },
    { "make plan",       IntentType::AgentPlan,         0.85f, true,  false },
    { "agent loop",      IntentType::AgentLoop,         0.90f, false, false },
    { "loop",            IntentType::AgentLoop,         0.60f, false, false },
    { "chain",           IntentType::AgentChain,        0.75f, true,  false },
    { "swarm",           IntentType::AgentSwarm,        0.80f, false, false },
    { "memory",          IntentType::AgentMemory,       0.60f, false, false },

    // --- Search & analysis ---
    { "search",          IntentType::Search,            0.80f, true,  false },
    { "grep",            IntentType::Search,            0.85f, true,  false },
    { "find",            IntentType::Search,            0.65f, true,  false },
    { "analyze",         IntentType::Analyze,           0.85f, true,  false },
    { "analysis",        IntentType::Analyze,           0.80f, false, false },
    { "profile",         IntentType::Profile,           0.85f, false, false },
    { "benchmark",       IntentType::Profile,           0.80f, false, false },
    { "audit",           IntentType::Audit,             0.85f, false, false },
    { "lint",            IntentType::Audit,             0.75f, false, false },

    // --- Chat & interactive ---
    { "chat",            IntentType::Chat,              0.80f, false, false },
    { "tell me",         IntentType::Chat,              0.65f, true,  false },
    { "hi",              IntentType::Greeting,          0.90f, false, false },
    { "hello",           IntentType::Greeting,          0.90f, false, false },
    { "hey",             IntentType::Greeting,          0.85f, false, false },
    { "?",               IntentType::Question,          0.50f, false, false },
    { "what is",         IntentType::Question,          0.75f, true,  false },
    { "how to",          IntentType::Question,          0.80f, true,  false },
    { "can you",         IntentType::Question,          0.60f, true,  false },

    // --- System / IDE ---
    { "settings",        IntentType::SystemSettings,    0.85f, false, false },
    { "preferences",     IntentType::SystemSettings,    0.80f, false, false },
    { "config",          IntentType::SystemSettings,    0.70f, false, false },
    { "theme",           IntentType::SystemTheme,       0.90f, false, false },
    { "dark mode",       IntentType::SystemTheme,       0.85f, false, false },
    { "light mode",      IntentType::SystemTheme,       0.85f, false, false },
    { "terminal",        IntentType::SystemTerminal,    0.80f, false, false },
    { "shell",           IntentType::SystemTerminal,    0.70f, false, false },
    { "build",           IntentType::SystemBuild,       0.80f, false, false },
    { "compile",         IntentType::SystemBuild,       0.85f, false, false },
    { "make",            IntentType::SystemBuild,       0.55f, false, false },
    { "cmake",           IntentType::SystemBuild,       0.90f, false, false },
    { "git",             IntentType::SystemGit,         0.85f, false, false },
    { "commit",          IntentType::SystemGit,         0.80f, false, false },
    { "push",            IntentType::SystemGit,         0.70f, false, false },
    { "pull",            IntentType::SystemGit,         0.70f, false, false },
    { "merge",           IntentType::SystemGit,         0.75f, false, false },

    // --- Hotpatch ---
    { "hotpatch",        IntentType::HotpatchApply,     0.85f, false, false },
    { "apply patch",     IntentType::HotpatchApply,     0.90f, true,  false },
    { "revert patch",    IntentType::HotpatchRevert,    0.90f, true,  false },
    { "patch status",    IntentType::HotpatchStatus,    0.85f, false, false },

    // --- Reverse engineering ---
    { "reverse",         IntentType::ReverseEngineer,   0.75f, false, false },
    { "reverse engineer",IntentType::ReverseEngineer,   0.95f, false, false },
    { "decompile",       IntentType::Decompile,         0.95f, true,  false },
    { "disassemble",     IntentType::Disassemble,       0.95f, true,  false },
    { "disasm",          IntentType::Disassemble,       0.90f, true,  false },
    { "dumpbin",         IntentType::Disassemble,       0.85f, true,  false },

    // --- Voice ---
    { "voice",           IntentType::VoiceCommand,      0.80f, false, false },
    { "speak",           IntentType::VoiceCommand,      0.75f, false, false },
    { "dictate",         IntentType::VoiceDictation,    0.90f, false, false },
    { "transcribe",      IntentType::VoiceDictation,    0.90f, false, false },

    // --- Help ---
    { "help",            IntentType::Help,              0.90f, false, false },
    { "docs",            IntentType::Documentation,     0.85f, false, false },
    { "documentation",   IntentType::Documentation,     0.90f, false, false },
    { "manual",          IntentType::Documentation,     0.80f, false, false },
    { "readme",          IntentType::Documentation,     0.75f, false, false },
};

static constexpr size_t s_defaultPatternCount = sizeof(s_defaultPatterns) / sizeof(s_defaultPatterns[0]);

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

IntentEngine::IntentEngine() {
    m_patterns.reserve(256);
    loadDefaultPatterns();
    fprintf(stderr, "[INFO] [IntentEngine] Initialized with %zu default patterns\n",
            m_patterns.size());
}

IntentEngine::~IntentEngine() = default;

// ============================================================================
// CLASSIFICATION
// ============================================================================

IntentClassification IntentEngine::classify(const std::string& userInput) const {
    return classifyWithContext(userInput, {}, {});
}

IntentClassification IntentEngine::classifyWithContext(
    const std::string& userInput,
    const std::string& conversationHistory,
    const std::string& currentFile) const
{
    m_totalClassifications.fetch_add(1, std::memory_order_relaxed);

    if (userInput.empty()) {
        return IntentClassification::unknown();
    }

    // Score all intents against the input
    auto scores = scoreAllIntents(userInput);

    // Apply context-based boosts
    if (!conversationHistory.empty() || !currentFile.empty()) {
        applyContextBoosts(scores, conversationHistory, currentFile);
    }

    // Sort by confidence descending
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    IntentClassification result;
    result.flags = 0;

    if (scores.empty() || scores[0].second < 0.1f) {
        // Nothing matched — default to Chat
        result.primaryIntent     = IntentType::Chat;
        result.primaryConfidence = 0.3f;
        result.secondaryIntent   = IntentType::Unknown;
        result.secondaryConfidence = 0.0f;
        result.isAmbiguous       = false;
        result.requiresContext   = true;
    } else {
        result.primaryIntent     = scores[0].first;
        result.primaryConfidence = scores[0].second;

        if (scores.size() > 1) {
            result.secondaryIntent     = scores[1].first;
            result.secondaryConfidence = scores[1].second;
            // Ambiguous if top-2 are within 0.15 of each other
            result.isAmbiguous = (scores[0].second - scores[1].second) < 0.15f;
        } else {
            result.secondaryIntent     = IntentType::Unknown;
            result.secondaryConfidence = 0.0f;
            result.isAmbiguous         = false;
        }

        result.requiresContext = (result.primaryConfidence < 0.5f);
    }

    // Extract subject — the first noun-like token after the action keyword
    // Store in scratch buffers (thread-safe via mutex)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scratchSubject.clear();
        m_scratchAction.clear();

        auto tokens = tokenize(userInput);
        if (!tokens.empty()) {
            m_scratchAction = tokens[0];
            // Subject is everything after the first recognized keyword
            bool foundAction = false;
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (foundAction) {
                    if (!m_scratchSubject.empty()) m_scratchSubject += " ";
                    m_scratchSubject += tokens[i];
                }
                // Check if this token matches any pattern keyword
                for (const auto& pat : m_patterns) {
                    if (containsWord(tokens[i], pat.keyword)) {
                        foundAction = true;
                        m_scratchAction = pat.keyword;
                        break;
                    }
                }
            }
            if (m_scratchSubject.empty() && tokens.size() > 1) {
                // Fallback: subject is everything after first token
                for (size_t i = 1; i < tokens.size(); ++i) {
                    if (!m_scratchSubject.empty()) m_scratchSubject += " ";
                    m_scratchSubject += tokens[i];
                }
            }
        }
        result.extractedSubject = m_scratchSubject.c_str();
        result.extractedAction  = m_scratchAction.c_str();
    }

    // Check for multi-intent (multiple strong signals)
    if (scores.size() >= 2 &&
        scores[0].second > 0.6f && scores[1].second > 0.6f &&
        scores[0].first != scores[1].first)
    {
        uint32_t cat0 = static_cast<uint32_t>(scores[0].first) >> 8;
        uint32_t cat1 = static_cast<uint32_t>(scores[1].first) >> 8;
        if (cat0 != cat1) {
            result.flags |= 0x01; // MULTI_INTENT_FLAG
        }
    }

    fprintf(stderr, "[DEBUG] [IntentEngine] '%s' → %s (%.2f) / %s (%.2f)%s\n",
            userInput.substr(0, 80).c_str(),
            intentName(result.primaryIntent), result.primaryConfidence,
            intentName(result.secondaryIntent), result.secondaryConfidence,
            result.isAmbiguous ? " [AMBIGUOUS]" : "");

    return result;
}

// ============================================================================
// ROUTING
// ============================================================================

bool IntentEngine::route(const IntentClassification& classification,
                          const std::string& rawInput) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t key = static_cast<uint32_t>(classification.primaryIntent);
    auto it = m_routes.find(key);
    if (it != m_routes.end()) {
        if (classification.primaryConfidence >= it->second.minConfidence) {
            m_totalRouted.fetch_add(1, std::memory_order_relaxed);
            fprintf(stderr, "[INFO] [IntentEngine] Routing to '%s' (confidence: %.2f)\n",
                    it->second.subsystemName, classification.primaryConfidence);
            it->second.handler(classification, rawInput);
            return true;
        }
    }

    // Try secondary intent
    if (classification.secondaryIntent != IntentType::Unknown) {
        key = static_cast<uint32_t>(classification.secondaryIntent);
        it = m_routes.find(key);
        if (it != m_routes.end()) {
            if (classification.secondaryConfidence >= it->second.minConfidence) {
                m_totalRouted.fetch_add(1, std::memory_order_relaxed);
                fprintf(stderr, "[INFO] [IntentEngine] Routing (fallback) to '%s' (confidence: %.2f)\n",
                        it->second.subsystemName, classification.secondaryConfidence);
                it->second.handler(classification, rawInput);
                return true;
            }
        }
    }

    // No route matched
    fprintf(stderr, "[WARN] [IntentEngine] No route for intent '%s'\n",
            intentName(classification.primaryIntent));
    return false;
}

void IntentEngine::registerRoute(IntentType intent, const char* subsystem,
                                  std::function<void(const IntentClassification&, const std::string&)> handler,
                                  float minConfidence) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t key = static_cast<uint32_t>(intent);
    m_routes[key] = IntentRoute{intent, subsystem, std::move(handler), minConfidence};
    fprintf(stderr, "[INFO] [IntentEngine] Registered route: %s → %s (min: %.2f)\n",
            intentName(intent), subsystem, minConfidence);
}

void IntentEngine::removeRoute(IntentType intent) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_routes.erase(static_cast<uint32_t>(intent));
}

// ============================================================================
// PATTERN MANAGEMENT
// ============================================================================

void IntentEngine::addPattern(const IntentPattern& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.push_back(pattern);
}

void IntentEngine::clearPatterns() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.clear();
}

void IntentEngine::loadDefaultPatterns() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.clear();
    m_patterns.reserve(s_defaultPatternCount + 32);
    for (size_t i = 0; i < s_defaultPatternCount; ++i) {
        m_patterns.push_back(s_defaultPatterns[i]);
    }
}

size_t IntentEngine::patternCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_patterns.size();
}

size_t IntentEngine::routeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_routes.size();
}

// ============================================================================
// SCORING
// ============================================================================

float IntentEngine::matchPatterns(const std::string& input, IntentType intent) const {
    // Already locked by caller
    float bestScore = 0.0f;
    std::string lower = toLower(input);

    for (const auto& pat : m_patterns) {
        if (pat.intent != intent) continue;

        std::string keyword = pat.caseSensitive ? pat.keyword : toLower(pat.keyword);
        const std::string& haystack = pat.caseSensitive ? input : lower;

        // Full phrase match (higher confidence)
        size_t pos = haystack.find(keyword);
        if (pos != std::string::npos) {
            float score = pat.baseWeight;

            // Boost for match at start of input
            if (pos == 0) score += 0.05f;

            // Boost for exact word boundary
            bool leftBound  = (pos == 0 || !std::isalnum(static_cast<unsigned char>(haystack[pos - 1])));
            bool rightBound = (pos + keyword.size() >= haystack.size() ||
                               !std::isalnum(static_cast<unsigned char>(haystack[pos + keyword.size()])));
            if (leftBound && rightBound) score += 0.05f;

            // Penalty for very short input with a generic keyword
            if (input.size() < 5 && keyword.size() < 4) score -= 0.10f;

            // Boost for longer keyword matches (more specific)
            if (keyword.size() > 8) score += 0.05f;

            bestScore = std::max(bestScore, std::min(score, 1.0f));
        }
    }

    return bestScore;
}

std::vector<std::pair<IntentType, float>> IntentEngine::scoreAllIntents(const std::string& input) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Collect all unique intent types from patterns
    std::unordered_map<uint32_t, IntentType> intentSet;
    for (const auto& pat : m_patterns) {
        intentSet[static_cast<uint32_t>(pat.intent)] = pat.intent;
    }

    std::vector<std::pair<IntentType, float>> scores;
    scores.reserve(intentSet.size());

    for (const auto& [key, itype] : intentSet) {
        float score = matchPatterns(input, itype);
        if (score > 0.0f) {
            scores.push_back({itype, score});
        }
    }

    return scores;
}

void IntentEngine::applyContextBoosts(
    std::vector<std::pair<IntentType, float>>& scores,
    const std::string& conversationHistory,
    const std::string& currentFile) const
{
    // Boost code-related intents when a code file is open
    if (!currentFile.empty()) {
        std::string ext;
        auto dotPos = currentFile.rfind('.');
        if (dotPos != std::string::npos) {
            ext = toLower(currentFile.substr(dotPos));
        }

        bool isCodeFile = (ext == ".cpp" || ext == ".c" || ext == ".h" || ext == ".hpp" ||
                           ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".rs" ||
                           ext == ".go" || ext == ".java" || ext == ".cs" || ext == ".asm" ||
                           ext == ".masm");

        if (isCodeFile) {
            for (auto& [intent, score] : scores) {
                uint32_t cat = static_cast<uint32_t>(intent) >> 8;
                if (cat == 0x01) { // Code* intents
                    score = std::min(score + 0.10f, 1.0f);
                }
            }
        }

        // Boost ASM/disasm intents for .asm files
        if (ext == ".asm" || ext == ".masm") {
            for (auto& [intent, score] : scores) {
                if (intent == IntentType::Disassemble || intent == IntentType::ReverseEngineer) {
                    score = std::min(score + 0.10f, 1.0f);
                }
            }
        }

        // Boost model intents for .gguf files
        if (ext == ".gguf") {
            for (auto& [intent, score] : scores) {
                uint32_t cat = static_cast<uint32_t>(intent) >> 8;
                if (cat == 0x03) { // Model* intents
                    score = std::min(score + 0.15f, 1.0f);
                }
            }
        }
    }

    // Boost chat/question if conversation history shows chat pattern
    if (!conversationHistory.empty()) {
        std::string lower = toLower(conversationHistory);
        if (lower.find("chat") != std::string::npos || lower.find("conversation") != std::string::npos) {
            for (auto& [intent, score] : scores) {
                if (intent == IntentType::Chat || intent == IntentType::Question) {
                    score = std::min(score + 0.05f, 1.0f);
                }
            }
        }

        // If last messages were about debugging, boost debug intent
        // Look at last 200 chars of history
        std::string recent = conversationHistory.size() > 200
            ? conversationHistory.substr(conversationHistory.size() - 200)
            : conversationHistory;
        std::string recentLower = toLower(recent);
        if (recentLower.find("debug") != std::string::npos ||
            recentLower.find("error") != std::string::npos ||
            recentLower.find("crash") != std::string::npos) {
            for (auto& [intent, score] : scores) {
                if (intent == IntentType::CodeDebug || intent == IntentType::CodeFixErrors) {
                    score = std::min(score + 0.08f, 1.0f);
                }
            }
        }
    }
}

// ============================================================================
// TOKENIZATION HELPERS
// ============================================================================

std::vector<std::string> IntentEngine::tokenize(const std::string& input) const {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        // Strip leading/trailing punctuation
        while (!token.empty() && std::ispunct(static_cast<unsigned char>(token.front())) &&
               token.front() != '!' && token.front() != '/' && token.front() != '?') {
            token.erase(token.begin());
        }
        while (!token.empty() && std::ispunct(static_cast<unsigned char>(token.back())) &&
               token.back() != '?' && token.back() != '!') {
            token.pop_back();
        }
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::string IntentEngine::toLower(const std::string& s) const {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool IntentEngine::containsWord(const std::string& haystack, const char* needle) const {
    if (!needle || !needle[0]) return false;
    std::string h = toLower(haystack);
    std::string n = toLower(needle);
    return h.find(n) != std::string::npos;
}

// ============================================================================
// DIAGNOSTICS
// ============================================================================

std::string IntentEngine::diagnosticReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    char buf[2048];
    snprintf(buf, sizeof(buf),
        "=== Intent Engine Diagnostic Report ===\n"
        "Patterns loaded:        %zu\n"
        "Routes registered:      %zu\n"
        "Total classifications:  %llu\n"
        "Total routed:           %llu\n"
        "Route coverage:         %.1f%%\n"
        "========================================\n",
        m_patterns.size(),
        m_routes.size(),
        static_cast<unsigned long long>(m_totalClassifications.load()),
        static_cast<unsigned long long>(m_totalRouted.load()),
        m_totalClassifications.load() > 0
            ? (100.0 * m_totalRouted.load() / m_totalClassifications.load())
            : 0.0);

    std::string report(buf);

    // List routes
    report += "\nRegistered Routes:\n";
    for (const auto& [key, route] : m_routes) {
        snprintf(buf, sizeof(buf), "  %-25s → %-20s (min: %.2f)\n",
                 intentName(route.intent), route.subsystemName, route.minConfidence);
        report += buf;
    }

    return report;
}

// ============================================================================
// STATIC NAME LOOKUP
// ============================================================================

const char* IntentEngine::intentName(IntentType type) {
    for (size_t i = 0; i < s_intentNameCount; ++i) {
        if (s_intentNames[i].type == type) return s_intentNames[i].name;
    }
    return "unknown";
}

IntentType IntentEngine::intentFromName(const char* name) {
    if (!name) return IntentType::Unknown;
    for (size_t i = 0; i < s_intentNameCount; ++i) {
        if (strcmp(s_intentNames[i].name, name) == 0) return s_intentNames[i].type;
    }
    return IntentType::Unknown;
}

// ============================================================================
// GLOBAL SINGLETON
// ============================================================================

IntentEngine& getIntentEngine() {
    static IntentEngine instance;
    return instance;
}
