/**
 * EnhancedMemoryRetrieval - Production Implementation
 * Day 3: Memory retrieval hardening and semantic search
 */

#include "enhanced_memory_retrieval.h"
#include "agentic_memory_system.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>

// ===== RetrievalResult =====

nlohmann::json RetrievalResult::toJson() const
{
    nlohmann::json j;
    j["memoryId"] = memoryId;
    j["content"] = content;
    j["relevanceScore"] = relevanceScore;
    j["accessCount"] = accessCount;
    j["timestamp"] = timestamp;
    j["isEssential"] = isEssential;
    return j;
}

// ===== EnhancedMemoryRetrieval =====

EnhancedMemoryRetrieval::EnhancedMemoryRetrieval(AgenticMemorySystem* memorySystem)
    : m_memorySystem(memorySystem)
{
    std::cout << "[EnhancedMemoryRetrieval] Initialized with hybrid semantic search" << std::endl;
}

EnhancedMemoryRetrieval::~EnhancedMemoryRetrieval()
{
    std::cout << "[EnhancedMemoryRetrieval] Destroyed - Cached "
              << m_resultCache.size() << " queries" << std::endl;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::semanticSearch(
    const std::string& query,
    size_t maxResults)
{
    if (!m_memorySystem) {
        std::cerr << "[EnhancedMemoryRetrieval] Memory system unavailable" << std::endl;
        return {};
    }

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<RetrievalResult> allResults;

    // Search all memory entries
    for (auto memory : m_memorySystem->getMemoriesByType(MemoryType::Fact)) {
        if (!memory) continue;

        RetrievalResult result;
        result.memoryId = memory->id;
        result.content = memory->content;
        result.accessCount = memory->accessCount;
        result.relevanceScore = computeSemanticSimilarity(query, memory->content);
        result.timestamp = "";  // Would extract from memory system
        
        if (result.relevanceScore > 0.1f) {
            allResults.push_back(result);
        }
    }

    // Also search code snippets and procedures
    for (auto memory : m_memorySystem->getMemoriesByType(MemoryType::CodeSnippet)) {
        if (!memory) continue;

        float score = computeSemanticSimilarity(query, memory->content);
        if (score > 0.05f) {
            RetrievalResult result;
            result.memoryId = memory->id;
            result.content = memory->content;
            result.accessCount = memory->accessCount;
            result.relevanceScore = score;
            allResults.push_back(result);
        }
    }

    // Sort by relevance
    std::sort(allResults.begin(), allResults.end(),
        [](const RetrievalResult& a, const RetrievalResult& b) {
            return a.relevanceScore > b.relevanceScore;
        });

    // Truncate to maxResults
    if (allResults.size() > maxResults) {
        allResults.resize(maxResults);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    m_stats.averageRetrievalTimeMs = 
        (m_stats.averageRetrievalTimeMs + duration.count()) / 2.0f;
    m_stats.totalSearches++;

    return allResults;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::keywordSearch(
    const std::string& keyword,
    size_t maxResults)
{
    if (!m_memorySystem) {
        return {};
    }

    std::vector<RetrievalResult> results;
    auto memories = m_memorySystem->getMemoriesByContentSearch(keyword);

    for (auto memory : memories) {
        if (!memory || results.size() >= maxResults) break;

        RetrievalResult result;
        result.memoryId = memory->id;
        result.content = memory->content;
        result.accessCount = memory->accessCount;
        result.relevanceScore = computeKeywordSimilarity(keyword, memory->content);
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(),
        [](const RetrievalResult& a, const RetrievalResult& b) {
            return a.relevanceScore > b.relevanceScore;
        });

    m_stats.totalSearches++;
    return results;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::hybridSearch(
    const std::string& query,
    float keywordWeight,
    size_t maxResults)
{
    // Get both keyword and semantic results
    auto keywordResults = keywordSearch(query, maxResults * 2);
    auto semanticResults = semanticSearch(query, maxResults * 2);

    // Merge with weighted scoring
    std::map<std::string, RetrievalResult> merged;
    
    for (const auto& result : keywordResults) {
        merged[result.memoryId] = result;
        merged[result.memoryId].relevanceScore *= keywordWeight;
    }

    for (const auto& result : semanticResults) {
        if (merged.count(result.memoryId)) {
            merged[result.memoryId].relevanceScore = 
                merged[result.memoryId].relevanceScore * keywordWeight +
                result.relevanceScore * (1.0f - keywordWeight);
        } else {
            merged[result.memoryId] = result;
            merged[result.memoryId].relevanceScore *= (1.0f - keywordWeight);
        }
    }

    // Extract and sort
    std::vector<RetrievalResult> results;
    for (auto& [id, result] : merged) {
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(),
        [](const RetrievalResult& a, const RetrievalResult& b) {
            return a.relevanceScore > b.relevanceScore;
        });

    if (results.size() > maxResults) {
        results.resize(maxResults);
    }

    return results;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::getContextRelevantMemories(
    const std::string& executionGoal,
    size_t maxResults)
{
    // For production: would use more sophisticated context matching
    // For now: use semantic search with execution goal
    return semanticSearch(executionGoal, maxResults);
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::getMemoriesByCategory(
    const std::string& category)
{
    // Map category to memory type
    MemoryType type = MemoryType::Fact;
    if (category == "code") type = MemoryType::CodeSnippet;
    else if (category == "procedure") type = MemoryType::Procedure;
    else if (category == "concept") type = MemoryType::Concept;

    std::vector<RetrievalResult> results;
    auto memories = m_memorySystem->getMemoriesByType(type);

    for (auto memory : memories) {
        if (!memory || results.size() >= 20) break;

        RetrievalResult result;
        result.memoryId = memory->id;
        result.content = memory->content;
        result.accessCount = memory->accessCount;
        result.relevanceScore = 1.0f;  // Category match
        results.push_back(result);
    }

    return results;
}

float EnhancedMemoryRetrieval::scoreRelevance(
    const std::string& memoryContent,
    const std::string& query)
{
    float semanticScore = computeSemanticSimilarity(query, memoryContent);
    float keywordScore = computeKeywordSimilarity(query, memoryContent);
    
    // Weighted combination: 60% semantic, 40% keyword
    return semanticScore * 0.6f + keywordScore * 0.4f;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::getEssentialMemories(size_t count)
{
    std::vector<RetrievalResult> results;
    
    // Get frequently accessed memories
    for (auto memory : m_memorySystem->getMemoriesByType(MemoryType::Fact)) {
        if (!memory || results.size() >= count) break;

        // Weight by access count
        float essentiality = std::min(1.0f, memory->accessCount / 10.0f);
        
        RetrievalResult result;
        result.memoryId = memory->id;
        result.content = memory->content;
        result.accessCount = memory->accessCount;
        result.relevanceScore = essentiality;
        result.isEssential = true;
        results.push_back(result);
    }

    std::sort(results.begin(), results.end(),
        [](const RetrievalResult& a, const RetrievalResult& b) {
            return a.accessCount > b.accessCount;
        });

    return results;
}

std::string EnhancedMemoryRetrieval::summarizeRetrievedMemories(
    const std::vector<RetrievalResult>& results,
    size_t maxLength)
{
    if (results.empty()) {
        return "No relevant memories found.";
    }

    std::ostringstream summary;
    summary << "Key knowledge from " << results.size() << " memories:\n";

    size_t currentLength = summary.str().length();
    
    for (const auto& result : results) {
        if (currentLength >= maxLength) break;

        // Truncate each memory content
        std::string content = result.content;
        if (content.length() > 100) {
            content = content.substr(0, 97) + "...";
        }

        summary << "- " << content << " (" 
                << static_cast<int>(result.relevanceScore * 100) << "% relevant)\n";
        
        currentLength = summary.str().length();
    }

    std::string result = summary.str();
    if (result.length() > maxLength) {
        result = result.substr(0, maxLength);
        result += "...";
    }

    return result;
}

std::vector<std::string> EnhancedMemoryRetrieval::extractKeyFacts(
    const std::vector<RetrievalResult>& results)
{
    std::vector<std::string> facts;

    for (const auto& result : results) {
        if (facts.size() >= 5) break;

        // For simplicity: first sentence of each memory
        std::string content = result.content;
        size_t endPos = content.find('.');
        if (endPos != std::string::npos) {
            facts.push_back(content.substr(0, endPos + 1));
        } else {
            facts.push_back(content.substr(0, std::min(content.length(), size_t(100))));
        }
    }

    return facts;
}

std::string EnhancedMemoryRetrieval::validateMemoryHealth() const
{
    if (!m_memorySystem) {
        return "Memory system not initialized";
    }

    if (m_memorySystem->getMemoryCount() == 0) {
        return "Memory system is empty";
    }

    return "";  // Healthy
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::robustSearch(
    const std::string& query,
    size_t maxResults)
{
    // Attempt search with fallback strategy
    try {
        auto results = hybridSearch(query, 0.5f, maxResults);
        if (!results.empty()) {
            return results;
        }

        // Fallback: keyword only
        return keywordSearch(query, maxResults);
    } catch (const std::exception& e) {
        std::cerr << "[EnhancedMemoryRetrieval] Search failed: " << e.what() << std::endl;
        
        // Last resort: return empty
        return {};
    }
}

void EnhancedMemoryRetrieval::recordMemoryAccess(const std::string& memoryId)
{
    // In production: would update memory system access count
    // This is tracked by the agent during memory usage
}

void EnhancedMemoryRetrieval::updateRelevanceFeedback(
    const std::string& memoryId,
    bool wasUseful)
{
    // In production: would adjust relevance scoring based on feedback
    // Useful memories would be boosted in future searches
}

nlohmann::json EnhancedMemoryRetrieval::getMemoryUsageStats() const
{
    nlohmann::json stats;
    stats["totalSearches"] = m_stats.totalSearches;
    stats["cacheHits"] = m_stats.cacheHits;
    stats["cacheMisses"] = m_stats.cacheMisses;
    stats["averageRetrievalTimeMs"] = m_stats.averageRetrievalTimeMs;
    stats["totalMemoriesIndexed"] = m_stats.totalMemoriesIndexed;
    stats["cacheSize"] = m_resultCache.size();
    return stats;
}

std::string EnhancedMemoryRetrieval::normalizeQuery(const std::string& query)
{
    std::string normalized;
    for (char c : query) {
        normalized += std::tolower(static_cast<unsigned char>(c));
    }
    
    // Remove extra whitespace
    std::istringstream iss(normalized);
    std::string token;
    normalized.clear();
    while (iss >> token) {
        if (!normalized.empty()) normalized += " ";
        normalized += token;
    }

    return normalized;
}

std::vector<RetrievalResult> EnhancedMemoryRetrieval::getCachedResults(
    const std::string& normalizedQuery)
{
    if (m_resultCache.count(normalizedQuery)) {
        m_stats.cacheHits++;
        return m_resultCache.at(normalizedQuery);
    }
    m_stats.cacheMisses++;
    return {};
}

void EnhancedMemoryRetrieval::cacheResults(
    const std::string& normalizedQuery,
    const std::vector<RetrievalResult>& results)
{
    if (m_resultCache.size() >= MAX_CACHE_ENTRIES) {
        pruneCache();
    }
    m_resultCache[normalizedQuery] = results;
}

float EnhancedMemoryRetrieval::computeSemanticSimilarity(
    const std::string& query,
    const std::string& text)
{
    // Simple implementation: word overlap
    auto queryTokens = tokenizeQuery(query);
    auto textTokens = tokenizeQuery(text);

    if (textTokens.empty()) return 0.0f;

    int matches = 0;
    for (const auto& qToken : queryTokens) {
        for (const auto& tToken : textTokens) {
            if (qToken == tToken) {
                matches++;
                break;
            }
        }
    }

    float similarity = static_cast<float>(matches) / textTokens.size();
    return similarity;
}

float EnhancedMemoryRetrieval::computeKeywordSimilarity(
    const std::string& query,
    const std::string& text)
{
    // Count occurrences of query substring in text
    size_t count = 0;
    size_t pos = 0;
    
    std::string lowerText = text;
    std::string lowerQuery = query;
    
    std::transform(lowerText.begin(), lowerText.end(), 
                   lowerText.begin(), ::tolower);
    std::transform(lowerQuery.begin(), lowerQuery.end(),
                   lowerQuery.begin(), ::tolower);

    while ((pos = lowerText.find(lowerQuery, pos)) != std::string::npos) {
        count++;
        pos += lowerQuery.length();
    }

    return std::min(1.0f, count / 10.0f);  // Cap at 1.0
}

std::vector<std::string> EnhancedMemoryRetrieval::tokenizeQuery(
    const std::string& query)
{
    std::vector<std::string> tokens;
    std::istringstream iss(query);
    std::string token;

    while (iss >> token) {
        // Remove punctuation
        token.erase(std::remove_if(token.begin(), token.end(),
            [](unsigned char c) { return std::ispunct(c); }),
            token.end());
        
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    return tokens;
}

void EnhancedMemoryRetrieval::pruneCache()
{
    // Simple strategy: remove oldest half
    size_t toRemove = m_resultCache.size() / 2;
    auto it = m_resultCache.begin();
    
    for (size_t i = 0; i < toRemove && it != m_resultCache.end(); ++i) {
        it = m_resultCache.erase(it);
    }
}
