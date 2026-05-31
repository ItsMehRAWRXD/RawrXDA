#pragma once
/**
 * EnhancedMemoryRetrieval - Day 3: Memory retrieval hardening
 * 
 * Production-grade memory retrieval with semantic search capabilities,
 * relevance scoring, and intelligent indexing for agent decision-making.
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

class AgenticMemorySystem;

/**
 * @struct RetrievalResult
 * Represents a single memory retrieval result with scoring
 */
struct RetrievalResult {
    std::string memoryId;
    std::string content;
    float relevanceScore = 0.0f;
    int accessCount = 0;
    std::string timestamp;
    bool isEssential = false;
    
    nlohmann::json toJson() const;
};

/**
 * @class EnhancedMemoryRetrieval
 * @brief Production memory retrieval system with semantic capabilities
 * 
 * Provides:
 * - Keyword-based and semantic search
 * - Relevance scoring using multiple signals
 * - Auto-summarization of retrieved knowledge  
 * - Safe failure handling (gradual degradation on errors)
 * - Query optimization and caching
 * - Memory lifecycle tracking
 */
class EnhancedMemoryRetrieval {
public:
    explicit EnhancedMemoryRetrieval(AgenticMemorySystem* memorySystem);
    ~EnhancedMemoryRetrieval();

    // ===== Semantic Search =====
    
    /**
     * Search memory using semantic similarity
     * Finds conceptually similar memories even with different keywords
     * @param query Search query
     * @param maxResults Maximum results to return
     * @return sorted results by relevance
     */
    std::vector<RetrievalResult> semanticSearch(
        const std::string& query,
        size_t maxResults = 10);

    /**
     * Search memory using keyword matching
     * Fast, exact pattern matching suitable for code snippets
     * @param keyword Search pattern
     * @param maxResults Maximum results to return
     * @return results matched by keyword
     */
    std::vector<RetrievalResult> keywordSearch(
        const std::string& keyword,
        size_t maxResults = 10);

    /**
     * Hybrid search combining keyword and semantic
     * @param query Search query
     * @param keywordWeight Weight for keyword matching (0-1)
     * @param maxResults Maximum results
     */
    std::vector<RetrievalResult> hybridSearch(
        const std::string& query,
        float keywordWeight = 0.4f,
        size_t maxResults = 10);

    // ===== Context-Aware Retrieval =====

    /**
     * Retrieve memories related to execution context
     * @param executionGoal Current goal being executed
     * @param maxResults Max results
     * @return memories relevant to this goal
     */
    std::vector<RetrievalResult> getContextRelevantMemories(
        const std::string& executionGoal,
        size_t maxResults = 5);

    /**
     * Find memories by semantic type/category
     * @param category Semantic category (e.g., "debugging", "patterns", "errors")
     * @return memories in this category
     */
    std::vector<RetrievalResult> getMemoriesByCategory(const std::string& category);

    // ===== Relevance Scoring =====

    /**
     * Score memory relevance to a query (0-1)
     * Considers: keyword match, semantic match, recency, access frequency, quality
     */
    float scoreRelevance(
        const std::string& memoryContent,
        const std::string& query);

    /**
     * Get top-K most high-value memories  
     * (frequently accessed, recent, high quality)
     */
    std::vector<RetrievalResult> getEssentialMemories(size_t count = 5);

    // ===== Auto-Summarization =====

    /**
     * Summarize retrieved memories into actionable knowledge
     * @param results Retrieved memories to summarize
     * @param maxLength Maximum length of summary
     * @return summarized knowledge suitable for agent decision-making
     */
    std::string summarizeRetrievedMemories(
        const std::vector<RetrievalResult>& results,
        size_t maxLength = 500);

    /**
     * Extract key facts from retrieved memories
     * @param results Retrieved memories
     * @return bullet-point key facts
     */
    std::vector<std::string> extractKeyFacts(
        const std::vector<RetrievalResult>& results);

    // ===== Safe Failure Handling =====

    /**
     * Check if memory system is healthy
     * @return error message if unhealthy, empty string if healthy
     */
    std::string validateMemoryHealth() const;

    /**
     * Attempt graceful degradation on memory errors
     * Falls back to less precise search methods
     */
    std::vector<RetrievalResult> robustSearch(
        const std::string& query,
        size_t maxResults = 10);

    // ===== Memory Lifecycle =====

    /**
     * Mark memory as accessed (updates relevance scoring)
     */
    void recordMemoryAccess(const std::string& memoryId);

    /**
     * Update memory relevance based on usefulness feedback
     * @param memoryId Memory to update
     * @param wasUseful True if memory was helpful to decision-making
     */
    void updateRelevanceFeedback(const std::string& memoryId, bool wasUseful);

    /**
     * Get memory access history and usage patterns
     */
    nlohmann::json getMemoryUsageStats() const;

    // ===== Query Optimization =====

    /**
     * Normalize query for better matching
     * (lowercasing, tokenization, etc.)
     */
    static std::string normalizeQuery(const std::string& query);

    /**
     * Check if query is already cached
     * @return cached results if available, empty if not cached
     */
    std::vector<RetrievalResult> getCachedResults(const std::string& normalizedQuery);

    /**
     * Cache query results
     */
    void cacheResults(
        const std::string& normalizedQuery,
        const std::vector<RetrievalResult>& results);

    // ===== Statistics =====

    struct RetrievalStats {
        size_t totalSearches = 0;
        size_t cacheHits = 0;
        size_t cacheMisses = 0;
        float averageRetrievalTimeMs = 0.0f;
        size_t totalMemoriesIndexed = 0;
    };

    RetrievalStats getStatistics() const { return m_stats; }

private:
    AgenticMemorySystem* m_memorySystem;
    RetrievalStats m_stats;
    
    // Query result caching
    std::map<std::string, std::vector<RetrievalResult>> m_resultCache;
    static constexpr size_t MAX_CACHE_ENTRIES = 100;

    // Helpers
    float computeSemanticSimilarity(
        const std::string& query,
        const std::string& text);
    
    float computeKeywordSimilarity(
        const std::string& query,
        const std::string& text);
    
    std::vector<std::string> tokenizeQuery(const std::string& query);
    
    void pruneCache();
};
