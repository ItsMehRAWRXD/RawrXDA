// =============================================================================
// knowledge_graph_core.hpp
// RawrXD v14.2.1 Cathedral — Phase C: Long-Term Knowledge Graph
//
// Cross-session persistent learning:
//   - WHY decisions were made (not just WHAT)
//   - User preference learning (coding style, naming conventions)
//   - Codebase archeology (change history with rationale)
//   - Vector-indexed semantic memory
//   - Bayesian preference ranking
//
// Extends embedding_engine.hpp with persistence via SQLite.
// =============================================================================
#pragma once
#ifndef RAWRXD_KNOWLEDGE_GRAPH_CORE_HPP
#define RAWRXD_KNOWLEDGE_GRAPH_CORE_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>

namespace RawrXD {
namespace Knowledge {

// =============================================================================
// Result Types (factory-static pattern)
// =============================================================================

struct KnowledgeResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static KnowledgeResult ok(const char* msg) {
        KnowledgeResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static KnowledgeResult error(const char* msg, int code = -1) {
        KnowledgeResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class DecisionType : uint8_t {
    ArchitecturalChoice,     // "Used singleton because..."
    RefactorReason,          // "Extracted method for testability"
    BugFixRationale,         // "Root cause was race condition in..."
    PerformanceOptimization, // "Switched to SIMD for..."
    SecurityDecision,        // "Added input validation because..."
    UserPreference,          // "User prefers for-loops over ranges"
    ToolChoice,              // "Selected clang-format over..."
    DependencyChoice,        // "Chose library X over Y because..."
    ApiDesign,               // "Made this public because..."
    TestStrategy             // "Mocked X because..."
};

enum class RelationType : uint8_t {
    CallsFunction,
    ImplementsInterface,
    DependsOn,
    TestedBy,
    DocumentedIn,
    ReplacedBy,
    ConflictsWith,
    OptimizedFrom,
    DerivedFrom,
    SimilarTo
};

enum class PreferenceCategory : uint8_t {
    LoopStyle,           // for vs range-for vs while
    NamingConvention,    // camelCase vs snake_case
    ErrorHandling,       // result types vs exceptions
    MemoryManagement,    // smart pointers vs manual
    CodeOrganization,    // headers vs inline
    TestingStyle,        // unit vs integration
    CommentStyle,        // doxygen vs line comments
    IndentationStyle,    // tabs vs spaces, width
    BraceStyle,          // K&R vs Allman
    TemplateUsage        // templates vs virtual dispatch
};

// =============================================================================
// Core Data Types
// =============================================================================

struct DecisionRecord {
    uint64_t     id;
    DecisionType type;
    char         summary[256];        // Short description
    char         rationale[1024];     // WHY this decision was made
    char         alternatives[512];   // What else was considered
    char         file[260];           // Associated file
    int          line;                // Associated line
    char         commitHash[41];      // Git commit (if any)
    uint64_t     timestamp;           // ms since epoch
    float        confidence;          // 0.0 – 1.0: how confident the agent was
    float        embedding[384];      // Vector embedding for semantic search
    int          embeddingDim;        // Actual dimension used
};

struct CodeRelationship {
    uint64_t     id;
    char         sourceSymbol[256];
    char         sourceFile[260];
    int          sourceLine;
    char         targetSymbol[256];
    char         targetFile[260];
    int          targetLine;
    RelationType relation;
    float        strength;            // 0.0 – 1.0
    uint64_t     createdAt;
    uint64_t     lastVerified;        // When was this relation last confirmed?
};

struct UserPreference {
    uint64_t            id;
    PreferenceCategory  category;
    char                key[128];
    char                preferredValue[256];
    char                evidence[512];     // Why we think this is preferred
    int                 observationCount;  // How many times observed
    float               bayesianScore;     // Bayesian confidence (0.0 – 1.0)
    uint64_t            firstObserved;
    uint64_t            lastObserved;
};

struct ChangeArcheology {
    uint64_t    id;
    char        file[260];
    char        functionName[256];
    char        oldBehavior[512];      // "Used to do X"
    char        newBehavior[512];      // "Now does Y"
    char        reason[512];           // "Changed in commit Z for reason..."
    char        commitHash[41];
    char        author[128];
    uint64_t    timestamp;
};

struct SemanticQuery {
    float        queryEmbedding[384];
    int          embeddingDim;
    int          topK;                  // How many results to return
    float        minSimilarity;         // Minimum cosine similarity threshold
    DecisionType filterType;            // Optional type filter (0xFF = all)
};

struct SemanticMatch {
    uint64_t     recordId;
    float        similarity;            // Cosine similarity score
    DecisionType type;
    char         summary[256];
    char         rationale[1024];
};

// =============================================================================
// Callbacks (C function pointers)
// =============================================================================

typedef void (*DecisionRecordedCallback)(const DecisionRecord* record, void* userData);
typedef void (*PreferenceLearntCallback)(const UserPreference* pref, void* userData);
typedef void (*ArcheologyDiscoveredCallback)(const ChangeArcheology* entry, void* userData);

// =============================================================================
// Configuration
// =============================================================================

struct KnowledgeConfig {
    char     dbPath[260];              // SQLite database path
    int      maxDecisions;             // Max stored decisions (default: 100000)
    int      maxRelationships;         // Max stored relationships (default: 500000)
    int      maxPreferences;           // Max preference entries (default: 1000)
    int      embeddingDimension;       // Default: 384
    float    preferenceThreshold;      // Bayesian threshold for "learned" (default: 0.7)
    bool     persistOnEveryWrite;      // Flush to disk on every record (default: false)
    int      flushIntervalSec;         // Periodic flush interval (default: 60)
    bool     enableArcheology;         // Track code history (default: true)
    bool     enablePreferences;        // Track user preferences (default: true)

    static KnowledgeConfig defaults() {
        KnowledgeConfig c;
        std::strncpy(c.dbPath, "rawrxd_knowledge.db", sizeof(c.dbPath) - 1);
        c.maxDecisions         = 100000;
        c.maxRelationships     = 500000;
        c.maxPreferences       = 1000;
        c.embeddingDimension   = 384;
        c.preferenceThreshold  = 0.7f;
        c.persistOnEveryWrite  = false;
        c.flushIntervalSec     = 60;
        c.enableArcheology     = true;
        c.enablePreferences    = true;
        return c;
    }
};

// =============================================================================
// Core Class: KnowledgeGraphCore
// =============================================================================

class KnowledgeGraphCore {
public:
    static KnowledgeGraphCore& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    KnowledgeResult initialize(const KnowledgeConfig& config);
    KnowledgeResult initialize();  // Use defaults
    KnowledgeResult shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ── Decision Recording (the "WHY" table) ───────────────────────────────
    KnowledgeResult recordDecision(const DecisionRecord& record);
    KnowledgeResult recordDecision(DecisionType type,
                                    const char* summary,
                                    const char* rationale,
                                    const char* file = nullptr,
                                    int line = 0);

    std::vector<DecisionRecord> getDecisions(int maxCount = 100) const;
    std::vector<DecisionRecord> getDecisionsByType(DecisionType type, int maxCount = 50) const;
    std::vector<DecisionRecord> getDecisionsForFile(const char* file, int maxCount = 50) const;

    // Semantic search over decisions
    std::vector<SemanticMatch> semanticSearch(const SemanticQuery& query) const;
    std::vector<SemanticMatch> searchByText(const char* query, int topK = 10) const;

    // ── Code Relationships (in-process graph database) ─────────────────────
    KnowledgeResult addRelationship(const CodeRelationship& rel);
    KnowledgeResult addRelationship(const char* source, const char* target,
                                     RelationType type, float strength = 1.0f);
    KnowledgeResult removeRelationship(uint64_t relId);

    std::vector<CodeRelationship> getRelationshipsFrom(const char* symbol) const;
    std::vector<CodeRelationship> getRelationshipsTo(const char* symbol) const;
    std::vector<CodeRelationship> getRelationshipsByType(RelationType type, int maxCount = 100) const;

    // Graph traversal
    std::vector<std::string> getCallChain(const char* fromSymbol, int maxDepth = 10) const;
    std::vector<std::string> getDependents(const char* symbol) const;
    std::vector<std::string> getDependencies(const char* symbol) const;
    bool hasCircularDependency(const char* symbol) const;

    // ── User Preference Learning ───────────────────────────────────────────
    KnowledgeResult observePreference(PreferenceCategory category,
                                       const char* key,
                                       const char* value,
                                       const char* evidence = nullptr);

    UserPreference  getPreference(PreferenceCategory category, const char* key) const;
    std::vector<UserPreference> getAllPreferences() const;
    std::vector<UserPreference> getLearnedPreferences() const; // Above threshold only
    bool isPreferenceLearned(PreferenceCategory category, const char* key) const;
    float getPreferenceConfidence(PreferenceCategory category, const char* key) const;

    // ── Codebase Archeology ────────────────────────────────────────────────
    KnowledgeResult recordChange(const ChangeArcheology& entry);
    KnowledgeResult recordChange(const char* file, const char* function,
                                  const char* oldBehavior, const char* newBehavior,
                                  const char* reason, const char* commitHash = nullptr);

    std::vector<ChangeArcheology> getHistory(const char* file, int maxCount = 50) const;
    std::vector<ChangeArcheology> getFunctionHistory(const char* functionName, int maxCount = 20) const;
    std::vector<ChangeArcheology> searchHistory(const char* query, int maxCount = 20) const;

    // ── Persistence ────────────────────────────────────────────────────────
    KnowledgeResult flush();             // Write all pending to disk
    KnowledgeResult loadFromDisk();      // Reload from SQLite
    KnowledgeResult exportToJson(const char* path) const;
    KnowledgeResult importFromJson(const char* path);

    // ── Embedding Integration ──────────────────────────────────────────────
    typedef void (*EmbeddingGeneratorFn)(const char* text, float* output, int dim, void* ud);
    void setEmbeddingGenerator(EmbeddingGeneratorFn fn, void* ud);

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setDecisionCallback(DecisionRecordedCallback cb, void* ud);
    void setPreferenceCallback(PreferenceLearntCallback cb, void* ud);
    void setArcheologyCallback(ArcheologyDiscoveredCallback cb, void* ud);

    // ── Statistics ──────────────────────────────────────────────────────────
    struct Stats {
        uint64_t totalDecisions;
        uint64_t totalRelationships;
        uint64_t totalPreferences;
        uint64_t totalArcheology;
        uint64_t semanticQueries;
        uint64_t preferenceUpdates;
        double   avgQueryTimeMs;
        uint64_t dbSizeBytes;
    };

    Stats getStats() const;
    std::string statsToJson() const;

    // ── Bayesian Preference Engine ─────────────────────────────────────────
    // Update preference scores using Bayesian inference
    void updateBayesianScores();

private:
    KnowledgeGraphCore();
    ~KnowledgeGraphCore();
    KnowledgeGraphCore(const KnowledgeGraphCore&) = delete;
    KnowledgeGraphCore& operator=(const KnowledgeGraphCore&) = delete;

    // SQLite operations
    KnowledgeResult createTables();
    KnowledgeResult insertDecision(const DecisionRecord& record);
    KnowledgeResult insertRelationship(const CodeRelationship& rel);
    KnowledgeResult insertPreference(const UserPreference& pref);
    KnowledgeResult insertArcheology(const ChangeArcheology& entry);

    // Embedding helpers
    void generateEmbedding(const char* text, float* output);
    float cosineSimilarity(const float* a, const float* b, int dim) const;

    // Graph helpers
    void dfsCallChain(const char* symbol, int depth, int maxDepth,
                      std::vector<std::string>& chain,
                      std::unordered_map<std::string, bool>& visited) const;

    uint64_t nextId();

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    mutable std::mutex m_dbMutex;
    std::atomic<bool>     m_initialized{false};
    std::atomic<uint64_t> m_nextId{1};

    KnowledgeConfig m_config;

    // In-memory stores (flushed to SQLite periodically)
    std::vector<DecisionRecord>     m_decisions;
    std::vector<CodeRelationship>   m_relationships;
    std::vector<UserPreference>     m_preferences;
    std::vector<ChangeArcheology>   m_archeology;

    // Graph adjacency index (symbol → relationship IDs)
    std::unordered_map<std::string, std::vector<uint64_t>> m_outEdges;
    std::unordered_map<std::string, std::vector<uint64_t>> m_inEdges;

    // SQLite handle
    void* m_db;  // sqlite3*

    // Injected subsystems
    EmbeddingGeneratorFn m_embeddingGen;  void* m_embeddingUD;

    // Callbacks
    DecisionRecordedCallback    m_decisionCb;     void* m_decisionCbUD;
    PreferenceLearntCallback    m_preferenceCb;    void* m_preferenceCbUD;
    ArcheologyDiscoveredCallback m_archeologyCb;   void* m_archeologyCbUD;

    // Telemetry
    alignas(64) Stats m_stats;
};

} // namespace Knowledge
} // namespace RawrXD

#endif // RAWRXD_KNOWLEDGE_GRAPH_CORE_HPP
