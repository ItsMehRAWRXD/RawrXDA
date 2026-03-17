// =============================================================================
// knowledge_graph_core.cpp
// RawrXD v14.2.1 Cathedral — Phase C: Long-Term Knowledge Graph
//
// Persistent cross-session learning with:
//   - SQLite-backed decision store (the WHY table)
//   - In-memory graph of code relationships
//   - Bayesian preference ranking
//   - Vector-indexed semantic search
//   - Codebase change archeology
// =============================================================================

#include "knowledge_graph_core.hpp"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>

// Forward declare sqlite3 if available
struct sqlite3;
struct sqlite3_stmt;
extern "C" {
    int sqlite3_open(const char*, sqlite3**);
    int sqlite3_close(sqlite3*);
    int sqlite3_exec(sqlite3*, const char*, int(*)(void*,int,char**,char**), void*, char**);
    int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
    int sqlite3_step(sqlite3_stmt*);
    int sqlite3_finalize(sqlite3_stmt*);
    int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
    int sqlite3_bind_int64(sqlite3_stmt*, int, long long);
    int sqlite3_bind_double(sqlite3_stmt*, int, double);
    int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int, void(*)(void*));
    const char* sqlite3_column_text(sqlite3_stmt*, int);
    long long sqlite3_column_int64(sqlite3_stmt*, int);
    double sqlite3_column_double(sqlite3_stmt*, int);
    const void* sqlite3_column_blob(sqlite3_stmt*, int);
    int sqlite3_column_bytes(sqlite3_stmt*, int);
    const char* sqlite3_errmsg(sqlite3*);
}

#define SQLITE_OK       0
#define SQLITE_ROW      100
#define SQLITE_DONE     101
#define SQLITE_TRANSIENT ((void(*)(void*))-1)

namespace RawrXD {
namespace Knowledge {

// =============================================================================
// Singleton
// =============================================================================

KnowledgeGraphCore& KnowledgeGraphCore::instance() {
    static KnowledgeGraphCore s_instance;
    return s_instance;
}

KnowledgeGraphCore::KnowledgeGraphCore()
    : m_db(nullptr)
    , m_embeddingGen(nullptr), m_embeddingUD(nullptr)
    , m_decisionCb(nullptr),   m_decisionCbUD(nullptr)
    , m_preferenceCb(nullptr), m_preferenceCbUD(nullptr)
    , m_archeologyCb(nullptr), m_archeologyCbUD(nullptr)
{
    m_config = KnowledgeConfig::defaults();
    std::memset(&m_stats, 0, sizeof(m_stats));
}

KnowledgeGraphCore::~KnowledgeGraphCore() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

KnowledgeResult KnowledgeGraphCore::initialize() {
    return initialize(KnowledgeConfig::defaults());
}

KnowledgeResult KnowledgeGraphCore::initialize(const KnowledgeConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;

    // Open SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open(config.dbPath, &db);
    if (rc != SQLITE_OK) {
        return KnowledgeResult::error("Failed to open knowledge database", rc);
    }
    m_db = db;

    // Create tables
    auto result = createTables();
    if (!result.success) return result;

    // Load existing data from disk
    loadFromDisk();

    m_initialized.store(true);
    return KnowledgeResult::ok("Knowledge graph initialized");
}

KnowledgeResult KnowledgeGraphCore::shutdown() {
    if (!m_initialized.load()) {
        return KnowledgeResult::ok("Not initialized");
    }

    // Flush pending data
    flush();

    // Close database
    if (m_db) {
        sqlite3_close(static_cast<sqlite3*>(m_db));
        m_db = nullptr;
    }

    m_initialized.store(false);
    return KnowledgeResult::ok("Knowledge graph shut down");
}

KnowledgeResult KnowledgeGraphCore::createTables() {
    sqlite3* db = static_cast<sqlite3*>(m_db);
    char* errmsg = nullptr;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS decisions (
            id INTEGER PRIMARY KEY,
            type INTEGER,
            summary TEXT,
            rationale TEXT,
            alternatives TEXT,
            file TEXT,
            line INTEGER,
            commit_hash TEXT,
            timestamp INTEGER,
            confidence REAL,
            embedding BLOB
        );

        CREATE TABLE IF NOT EXISTS relationships (
            id INTEGER PRIMARY KEY,
            source_symbol TEXT,
            source_file TEXT,
            source_line INTEGER,
            target_symbol TEXT,
            target_file TEXT,
            target_line INTEGER,
            relation_type INTEGER,
            strength REAL,
            created_at INTEGER,
            last_verified INTEGER
        );

        CREATE TABLE IF NOT EXISTS preferences (
            id INTEGER PRIMARY KEY,
            category INTEGER,
            key TEXT,
            preferred_value TEXT,
            evidence TEXT,
            observation_count INTEGER,
            bayesian_score REAL,
            first_observed INTEGER,
            last_observed INTEGER
        );

        CREATE TABLE IF NOT EXISTS archeology (
            id INTEGER PRIMARY KEY,
            file TEXT,
            function_name TEXT,
            old_behavior TEXT,
            new_behavior TEXT,
            reason TEXT,
            commit_hash TEXT,
            author TEXT,
            timestamp INTEGER
        );

        CREATE INDEX IF NOT EXISTS idx_decisions_type ON decisions(type);
        CREATE INDEX IF NOT EXISTS idx_decisions_file ON decisions(file);
        CREATE INDEX IF NOT EXISTS idx_relationships_source ON relationships(source_symbol);
        CREATE INDEX IF NOT EXISTS idx_relationships_target ON relationships(target_symbol);
        CREATE INDEX IF NOT EXISTS idx_preferences_category ON preferences(category, key);
        CREATE INDEX IF NOT EXISTS idx_archeology_file ON archeology(file);
        CREATE INDEX IF NOT EXISTS idx_archeology_function ON archeology(function_name);
    )";

    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        return KnowledgeResult::error("Failed to create tables", rc);
    }

    return KnowledgeResult::ok("Tables created");
}

// =============================================================================
// Decision Recording
// =============================================================================

KnowledgeResult KnowledgeGraphCore::recordDecision(const DecisionRecord& record) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DecisionRecord r = record;
    if (r.id == 0) r.id = nextId();
    if (r.timestamp == 0) r.timestamp = static_cast<uint64_t>(std::time(nullptr)) * 1000;

    // Generate embedding if generator is set
    if (m_embeddingGen && r.embeddingDim == 0) {
        std::string text = std::string(r.summary) + " " + std::string(r.rationale);
        generateEmbedding(text.c_str(), r.embedding);
        r.embeddingDim = m_config.embeddingDimension;
    }

    m_decisions.push_back(r);
    m_stats.totalDecisions++;

    if (m_decisionCb) {
        m_decisionCb(&r, m_decisionCbUD);
    }

    // Persist if configured
    if (m_config.persistOnEveryWrite) {
        insertDecision(r);
    }

    return KnowledgeResult::ok("Decision recorded");
}

KnowledgeResult KnowledgeGraphCore::recordDecision(
    DecisionType type,
    const char* summary,
    const char* rationale,
    const char* file,
    int line)
{
    DecisionRecord r;
    std::memset(&r, 0, sizeof(r));
    r.id = nextId();
    r.type = type;
    r.line = line;
    r.confidence = 1.0f;
    r.timestamp = static_cast<uint64_t>(std::time(nullptr)) * 1000;

    std::strncpy(r.summary, summary, sizeof(r.summary) - 1);
    std::strncpy(r.rationale, rationale, sizeof(r.rationale) - 1);
    if (file) std::strncpy(r.file, file, sizeof(r.file) - 1);

    return recordDecision(r);
}

std::vector<DecisionRecord> KnowledgeGraphCore::getDecisions(int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = std::min(maxCount, static_cast<int>(m_decisions.size()));
    return std::vector<DecisionRecord>(
        m_decisions.end() - count,
        m_decisions.end()
    );
}

std::vector<DecisionRecord> KnowledgeGraphCore::getDecisionsByType(DecisionType type, int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DecisionRecord> result;
    for (auto it = m_decisions.rbegin(); it != m_decisions.rend() && static_cast<int>(result.size()) < maxCount; ++it) {
        if (it->type == type) {
            result.push_back(*it);
        }
    }
    return result;
}

std::vector<DecisionRecord> KnowledgeGraphCore::getDecisionsForFile(const char* file, int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DecisionRecord> result;
    for (auto it = m_decisions.rbegin(); it != m_decisions.rend() && static_cast<int>(result.size()) < maxCount; ++it) {
        if (std::strcmp(it->file, file) == 0) {
            result.push_back(*it);
        }
    }
    return result;
}

// =============================================================================
// Semantic Search
// =============================================================================

std::vector<SemanticMatch> KnowledgeGraphCore::semanticSearch(const SemanticQuery& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.semanticQueries++;

    std::vector<SemanticMatch> results;

    for (const auto& d : m_decisions) {
        if (d.embeddingDim == 0) continue;

        // Type filter
        if (static_cast<uint8_t>(query.filterType) != 0xFF &&
            d.type != query.filterType) continue;

        float sim = cosineSimilarity(query.queryEmbedding, d.embedding,
                                     std::min(query.embeddingDim, d.embeddingDim));

        if (sim >= query.minSimilarity) {
            SemanticMatch m;
            m.recordId   = d.id;
            m.similarity = sim;
            m.type       = d.type;
            std::strncpy(m.summary, d.summary, sizeof(m.summary) - 1);
            std::strncpy(m.rationale, d.rationale, sizeof(m.rationale) - 1);
            results.push_back(m);
        }
    }

    // Sort by similarity descending
    std::sort(results.begin(), results.end(),
        [](const SemanticMatch& a, const SemanticMatch& b) {
            return a.similarity > b.similarity;
        });

    // Limit to topK
    if (static_cast<int>(results.size()) > query.topK) {
        results.resize(static_cast<size_t>(query.topK));
    }

    return results;
}

std::vector<SemanticMatch> KnowledgeGraphCore::searchByText(const char* query, int topK) const {
    SemanticQuery sq;
    std::memset(&sq, 0, sizeof(sq));
    sq.topK          = topK;
    sq.minSimilarity = 0.3f;
    sq.embeddingDim  = m_config.embeddingDimension;
    sq.filterType    = static_cast<DecisionType>(0xFF); // All types

    // Generate embedding for query text
    if (m_embeddingGen) {
        m_embeddingGen(query, sq.queryEmbedding, sq.embeddingDim,
                       const_cast<void*>(m_embeddingUD));
    }

    return semanticSearch(sq);
}

// =============================================================================
// Code Relationships
// =============================================================================

KnowledgeResult KnowledgeGraphCore::addRelationship(const CodeRelationship& rel) {
    std::lock_guard<std::mutex> lock(m_mutex);

    CodeRelationship r = rel;
    if (r.id == 0) r.id = nextId();
    if (r.createdAt == 0) r.createdAt = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    r.lastVerified = r.createdAt;

    m_relationships.push_back(r);
    m_stats.totalRelationships++;

    // Update adjacency index
    m_outEdges[std::string(r.sourceSymbol)].push_back(r.id);
    m_inEdges[std::string(r.targetSymbol)].push_back(r.id);

    return KnowledgeResult::ok("Relationship added");
}

KnowledgeResult KnowledgeGraphCore::addRelationship(
    const char* source, const char* target,
    RelationType type, float strength)
{
    CodeRelationship r;
    std::memset(&r, 0, sizeof(r));
    r.id = nextId();
    r.relation = type;
    r.strength = strength;
    r.createdAt = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    r.lastVerified = r.createdAt;
    std::strncpy(r.sourceSymbol, source, sizeof(r.sourceSymbol) - 1);
    std::strncpy(r.targetSymbol, target, sizeof(r.targetSymbol) - 1);

    return addRelationship(r);
}

std::vector<CodeRelationship> KnowledgeGraphCore::getRelationshipsFrom(const char* symbol) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeRelationship> result;

    auto it = m_outEdges.find(std::string(symbol));
    if (it == m_outEdges.end()) return result;

    for (uint64_t id : it->second) {
        for (const auto& r : m_relationships) {
            if (r.id == id) {
                result.push_back(r);
                break;
            }
        }
    }
    return result;
}

std::vector<CodeRelationship> KnowledgeGraphCore::getRelationshipsTo(const char* symbol) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeRelationship> result;

    auto it = m_inEdges.find(std::string(symbol));
    if (it == m_inEdges.end()) return result;

    for (uint64_t id : it->second) {
        for (const auto& r : m_relationships) {
            if (r.id == id) {
                result.push_back(r);
                break;
            }
        }
    }
    return result;
}

std::vector<std::string> KnowledgeGraphCore::getCallChain(const char* fromSymbol, int maxDepth) const {
    std::vector<std::string> chain;
    std::unordered_map<std::string, bool> visited;
    dfsCallChain(fromSymbol, 0, maxDepth, chain, visited);
    return chain;
}

void KnowledgeGraphCore::dfsCallChain(
    const char* symbol, int depth, int maxDepth,
    std::vector<std::string>& chain,
    std::unordered_map<std::string, bool>& visited) const
{
    if (depth >= maxDepth) return;

    std::string sym(symbol);
    if (visited.count(sym)) return;
    visited[sym] = true;
    chain.push_back(sym);

    auto it = m_outEdges.find(sym);
    if (it == m_outEdges.end()) return;

    for (uint64_t id : it->second) {
        for (const auto& r : m_relationships) {
            if (r.id == id && r.relation == RelationType::CallsFunction) {
                dfsCallChain(r.targetSymbol, depth + 1, maxDepth, chain, visited);
            }
        }
    }
}

std::vector<std::string> KnowledgeGraphCore::getDependents(const char* symbol) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    auto it = m_inEdges.find(std::string(symbol));
    if (it == m_inEdges.end()) return result;

    for (uint64_t id : it->second) {
        for (const auto& r : m_relationships) {
            if (r.id == id && r.relation == RelationType::DependsOn) {
                result.push_back(std::string(r.sourceSymbol));
            }
        }
    }
    return result;
}

std::vector<std::string> KnowledgeGraphCore::getDependencies(const char* symbol) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    auto it = m_outEdges.find(std::string(symbol));
    if (it == m_outEdges.end()) return result;

    for (uint64_t id : it->second) {
        for (const auto& r : m_relationships) {
            if (r.id == id && r.relation == RelationType::DependsOn) {
                result.push_back(std::string(r.targetSymbol));
            }
        }
    }
    return result;
}

bool KnowledgeGraphCore::hasCircularDependency(const char* symbol) const {
    std::unordered_map<std::string, bool> visited;
    std::vector<std::string> chain;
    dfsCallChain(symbol, 0, 100, chain, visited);

    // If we visit the starting symbol again, circular
    int count = 0;
    for (const auto& s : chain) {
        if (s == std::string(symbol)) count++;
    }
    return count > 1;
}

// =============================================================================
// User Preference Learning
// =============================================================================

KnowledgeResult KnowledgeGraphCore::observePreference(
    PreferenceCategory category,
    const char* key,
    const char* value,
    const char* evidence)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Look for existing preference
    for (auto& p : m_preferences) {
        if (p.category == category && std::strcmp(p.key, key) == 0) {
            // Update existing
            if (std::strcmp(p.preferredValue, value) == 0) {
                // Same value observed again — strengthen belief
                p.observationCount++;
                p.bayesianScore = std::min(1.0f,
                    p.bayesianScore + (1.0f - p.bayesianScore) * 0.15f);
            } else {
                // Different value — weaken belief
                p.bayesianScore *= 0.85f;
                if (p.bayesianScore < 0.5f) {
                    // Preference may have changed
                    std::strncpy(p.preferredValue, value, sizeof(p.preferredValue) - 1);
                    p.bayesianScore = 0.5f;
                }
            }
            p.lastObserved = static_cast<uint64_t>(std::time(nullptr)) * 1000;
            if (evidence) std::strncpy(p.evidence, evidence, sizeof(p.evidence) - 1);

            m_stats.preferenceUpdates++;

            // Notify if threshold crossed
            if (p.bayesianScore >= m_config.preferenceThreshold && m_preferenceCb) {
                m_preferenceCb(&p, m_preferenceCbUD);
            }

            return KnowledgeResult::ok("Preference updated");
        }
    }

    // New preference
    UserPreference p;
    std::memset(&p, 0, sizeof(p));
    p.id = nextId();
    p.category = category;
    std::strncpy(p.key, key, sizeof(p.key) - 1);
    std::strncpy(p.preferredValue, value, sizeof(p.preferredValue) - 1);
    if (evidence) std::strncpy(p.evidence, evidence, sizeof(p.evidence) - 1);
    p.observationCount = 1;
    p.bayesianScore    = 0.5f;  // Prior: 50/50
    p.firstObserved    = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    p.lastObserved     = p.firstObserved;

    m_preferences.push_back(p);
    m_stats.totalPreferences++;

    return KnowledgeResult::ok("New preference recorded");
}

UserPreference KnowledgeGraphCore::getPreference(PreferenceCategory category, const char* key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_preferences) {
        if (p.category == category && std::strcmp(p.key, key) == 0) {
            return p;
        }
    }
    UserPreference empty;
    std::memset(&empty, 0, sizeof(empty));
    return empty;
}

std::vector<UserPreference> KnowledgeGraphCore::getAllPreferences() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_preferences;
}

std::vector<UserPreference> KnowledgeGraphCore::getLearnedPreferences() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<UserPreference> result;
    for (const auto& p : m_preferences) {
        if (p.bayesianScore >= m_config.preferenceThreshold) {
            result.push_back(p);
        }
    }
    return result;
}

bool KnowledgeGraphCore::isPreferenceLearned(PreferenceCategory category, const char* key) const {
    auto p = getPreference(category, key);
    return p.bayesianScore >= m_config.preferenceThreshold;
}

float KnowledgeGraphCore::getPreferenceConfidence(PreferenceCategory category, const char* key) const {
    auto p = getPreference(category, key);
    return p.bayesianScore;
}

void KnowledgeGraphCore::updateBayesianScores() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Time decay: reduce confidence for stale preferences
    uint64_t now = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    for (auto& p : m_preferences) {
        uint64_t age = now - p.lastObserved;
        // Decay by 5% per week of inactivity
        if (age > 604800000ULL) { // > 1 week
            float weeks = static_cast<float>(age) / 604800000.0f;
            float decay = std::pow(0.95f, weeks);
            p.bayesianScore *= decay;
        }
    }
}

// =============================================================================
// Codebase Archeology
// =============================================================================

KnowledgeResult KnowledgeGraphCore::recordChange(const ChangeArcheology& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ChangeArcheology e = entry;
    if (e.id == 0) e.id = nextId();
    if (e.timestamp == 0) e.timestamp = static_cast<uint64_t>(std::time(nullptr)) * 1000;

    m_archeology.push_back(e);
    m_stats.totalArcheology++;

    if (m_archeologyCb) {
        m_archeologyCb(&e, m_archeologyCbUD);
    }

    return KnowledgeResult::ok("Change recorded");
}

KnowledgeResult KnowledgeGraphCore::recordChange(
    const char* file, const char* function,
    const char* oldBehavior, const char* newBehavior,
    const char* reason, const char* commitHash)
{
    ChangeArcheology e;
    std::memset(&e, 0, sizeof(e));
    e.id = nextId();
    e.timestamp = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    std::strncpy(e.file, file, sizeof(e.file) - 1);
    std::strncpy(e.functionName, function, sizeof(e.functionName) - 1);
    std::strncpy(e.oldBehavior, oldBehavior, sizeof(e.oldBehavior) - 1);
    std::strncpy(e.newBehavior, newBehavior, sizeof(e.newBehavior) - 1);
    std::strncpy(e.reason, reason, sizeof(e.reason) - 1);
    if (commitHash) std::strncpy(e.commitHash, commitHash, sizeof(e.commitHash) - 1);

    return recordChange(e);
}

std::vector<ChangeArcheology> KnowledgeGraphCore::getHistory(const char* file, int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ChangeArcheology> result;
    for (auto it = m_archeology.rbegin(); it != m_archeology.rend() && static_cast<int>(result.size()) < maxCount; ++it) {
        if (std::strcmp(it->file, file) == 0) {
            result.push_back(*it);
        }
    }
    return result;
}

std::vector<ChangeArcheology> KnowledgeGraphCore::getFunctionHistory(const char* functionName, int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ChangeArcheology> result;
    for (auto it = m_archeology.rbegin(); it != m_archeology.rend() && static_cast<int>(result.size()) < maxCount; ++it) {
        if (std::strcmp(it->functionName, functionName) == 0) {
            result.push_back(*it);
        }
    }
    return result;
}

// =============================================================================
// Persistence
// =============================================================================

KnowledgeResult KnowledgeGraphCore::flush() {
    if (!m_db) return KnowledgeResult::error("Database not open");

    std::lock_guard<std::mutex> lock(m_dbMutex);

    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

    for (const auto& d : m_decisions) insertDecision(d);
    for (const auto& r : m_relationships) insertRelationship(r);
    for (const auto& p : m_preferences) insertPreference(p);
    for (const auto& a : m_archeology) insertArcheology(a);

    sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);

    return KnowledgeResult::ok("Flushed to disk");
}

KnowledgeResult KnowledgeGraphCore::loadFromDisk() {
    if (!m_db) return KnowledgeResult::error("Database not open");

    // Load decisions
    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db,
        "SELECT id,type,summary,rationale,alternatives,file,line,commit_hash,timestamp,confidence,embedding FROM decisions",
        -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DecisionRecord d;
            std::memset(&d, 0, sizeof(d));
            d.id = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
            d.type = static_cast<DecisionType>(sqlite3_column_int64(stmt, 1));
            const char* s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            if (s) std::strncpy(d.summary, s, sizeof(d.summary) - 1);
            s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            if (s) std::strncpy(d.rationale, s, sizeof(d.rationale) - 1);
            s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            if (s) std::strncpy(d.alternatives, s, sizeof(d.alternatives) - 1);
            s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            if (s) std::strncpy(d.file, s, sizeof(d.file) - 1);
            d.line = static_cast<int>(sqlite3_column_int64(stmt, 6));
            s = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            if (s) std::strncpy(d.commitHash, s, sizeof(d.commitHash) - 1);
            d.timestamp = static_cast<uint64_t>(sqlite3_column_int64(stmt, 8));
            d.confidence = static_cast<float>(sqlite3_column_double(stmt, 9));

            const void* blob = sqlite3_column_blob(stmt, 10);
            int blobSize = sqlite3_column_bytes(stmt, 10);
            if (blob && blobSize > 0) {
                int dim = blobSize / static_cast<int>(sizeof(float));
                dim = std::min(dim, 384);
                std::memcpy(d.embedding, blob, static_cast<size_t>(dim) * sizeof(float));
                d.embeddingDim = dim;
            }

            m_decisions.push_back(d);
            if (d.id >= m_nextId.load()) m_nextId.store(d.id + 1);
        }
        sqlite3_finalize(stmt);
    }

    m_stats.totalDecisions = m_decisions.size();
    return KnowledgeResult::ok("Loaded from disk");
}

KnowledgeResult KnowledgeGraphCore::insertDecision(const DecisionRecord& d) {
    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO decisions (id,type,summary,rationale,alternatives,file,line,commit_hash,timestamp,confidence,embedding) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return KnowledgeResult::error("Prepare failed", rc);

    sqlite3_bind_int64(stmt, 1, static_cast<long long>(d.id));
    sqlite3_bind_int64(stmt, 2, static_cast<int>(d.type));
    sqlite3_bind_text(stmt, 3, d.summary, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, d.rationale, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, d.alternatives, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, d.file, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, d.line);
    sqlite3_bind_text(stmt, 8, d.commitHash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, static_cast<long long>(d.timestamp));
    sqlite3_bind_double(stmt, 10, d.confidence);
    if (d.embeddingDim > 0) {
        sqlite3_bind_blob(stmt, 11, d.embedding,
            d.embeddingDim * static_cast<int>(sizeof(float)), SQLITE_TRANSIENT);
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return KnowledgeResult::ok("Inserted");
}

KnowledgeResult KnowledgeGraphCore::insertRelationship(const CodeRelationship& r) {
    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO relationships (id,source_symbol,source_file,source_line,target_symbol,target_file,target_line,relation_type,strength,created_at,last_verified) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return KnowledgeResult::error("Prepare failed", rc);

    sqlite3_bind_int64(stmt, 1, static_cast<long long>(r.id));
    sqlite3_bind_text(stmt, 2, r.sourceSymbol, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, r.sourceFile, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, r.sourceLine);
    sqlite3_bind_text(stmt, 5, r.targetSymbol, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, r.targetFile, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, r.targetLine);
    sqlite3_bind_int64(stmt, 8, static_cast<int>(r.relation));
    sqlite3_bind_double(stmt, 9, r.strength);
    sqlite3_bind_int64(stmt, 10, static_cast<long long>(r.createdAt));
    sqlite3_bind_int64(stmt, 11, static_cast<long long>(r.lastVerified));

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return KnowledgeResult::ok("Inserted");
}

KnowledgeResult KnowledgeGraphCore::insertPreference(const UserPreference& p) {
    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO preferences (id,category,key,preferred_value,evidence,observation_count,bayesian_score,first_observed,last_observed) "
        "VALUES (?,?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return KnowledgeResult::error("Prepare failed", rc);

    sqlite3_bind_int64(stmt, 1, static_cast<long long>(p.id));
    sqlite3_bind_int64(stmt, 2, static_cast<int>(p.category));
    sqlite3_bind_text(stmt, 3, p.key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, p.preferredValue, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, p.evidence, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, p.observationCount);
    sqlite3_bind_double(stmt, 7, p.bayesianScore);
    sqlite3_bind_int64(stmt, 8, static_cast<long long>(p.firstObserved));
    sqlite3_bind_int64(stmt, 9, static_cast<long long>(p.lastObserved));

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return KnowledgeResult::ok("Inserted");
}

KnowledgeResult KnowledgeGraphCore::insertArcheology(const ChangeArcheology& a) {
    sqlite3* db = static_cast<sqlite3*>(m_db);
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO archeology (id,file,function_name,old_behavior,new_behavior,reason,commit_hash,author,timestamp) "
        "VALUES (?,?,?,?,?,?,?,?,?)",
        -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return KnowledgeResult::error("Prepare failed", rc);

    sqlite3_bind_int64(stmt, 1, static_cast<long long>(a.id));
    sqlite3_bind_text(stmt, 2, a.file, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, a.functionName, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, a.oldBehavior, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, a.newBehavior, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, a.reason, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, a.commitHash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, a.author, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 9, static_cast<long long>(a.timestamp));

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return KnowledgeResult::ok("Inserted");
}

// =============================================================================
// Embedding Helpers
// =============================================================================

void KnowledgeGraphCore::generateEmbedding(const char* text, float* output) {
    if (m_embeddingGen) {
        m_embeddingGen(text, output, m_config.embeddingDimension, m_embeddingUD);
    }
}

float KnowledgeGraphCore::cosineSimilarity(const float* a, const float* b, int dim) const {
    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot   += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    float denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom < 1e-8f) return 0.0f;
    return dot / denom;
}

uint64_t KnowledgeGraphCore::nextId() {
    return m_nextId.fetch_add(1);
}

// =============================================================================
// Setters
// =============================================================================

void KnowledgeGraphCore::setEmbeddingGenerator(EmbeddingGeneratorFn fn, void* ud) { m_embeddingGen = fn; m_embeddingUD = ud; }
void KnowledgeGraphCore::setDecisionCallback(DecisionRecordedCallback cb, void* ud) { m_decisionCb = cb; m_decisionCbUD = ud; }
void KnowledgeGraphCore::setPreferenceCallback(PreferenceLearntCallback cb, void* ud) { m_preferenceCb = cb; m_preferenceCbUD = ud; }
void KnowledgeGraphCore::setArcheologyCallback(ArcheologyDiscoveredCallback cb, void* ud) { m_archeologyCb = cb; m_archeologyCbUD = ud; }

// =============================================================================
// Statistics
// =============================================================================

KnowledgeGraphCore::Stats KnowledgeGraphCore::getStats() const {
    return m_stats;
}

std::string KnowledgeGraphCore::statsToJson() const {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        R"({"totalDecisions":%llu,"totalRelationships":%llu,"totalPreferences":%llu,)"
        R"("totalArcheology":%llu,"semanticQueries":%llu,"preferenceUpdates":%llu})",
        (unsigned long long)m_stats.totalDecisions,
        (unsigned long long)m_stats.totalRelationships,
        (unsigned long long)m_stats.totalPreferences,
        (unsigned long long)m_stats.totalArcheology,
        (unsigned long long)m_stats.semanticQueries,
        (unsigned long long)m_stats.preferenceUpdates
    );
    return std::string(buf);
}

} // namespace Knowledge
} // namespace RawrXD
