// AgenticMemorySystem Implementation (Core Functions)
#include "agentic_memory_system.h"
#include <QDebug>
#include <QUuid>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <algorithm>

AgenticMemorySystem::AgenticMemorySystem(QObject* parent)
    : QObject(parent),
      m_systemStartTime(QDateTime::currentDateTime())
{
    qDebug() << "[AgenticMemorySystem] Initialized - Ready for memory management";
}

AgenticMemorySystem::~AgenticMemorySystem()
{
    qDebug() << "[AgenticMemorySystem] Destroyed - Cleaned up"
             << m_memories.size() << "memories and"
             << m_experiences.size() << "experiences";
}

// ===== MEMORY STORAGE =====

QString AgenticMemorySystem::storeMemory(
    MemoryType type,
    const QString& content,
    const QJsonObject& metadata)
{
    QString memoryId = QUuid::createUuid().toString();

    auto memory = std::make_unique<MemoryEntry>();
    memory->id = memoryId;
    memory->type = type;
    memory->content = content;
    memory->metadata = metadata;
    memory->timestamp = QDateTime::currentDateTime();
    memory->relevanceScore = 1.0f;
    memory->accessCount = 0;
    memory->isPinned = false;

    m_memories[memoryId.toStdString()] = std::move(memory);
    m_totalStored++;

    emit memoryStored(memoryId);

    return memoryId;
}

void AgenticMemorySystem::updateMemory(const QString& memoryId, const QString& content)
{
    auto it = m_memories.find(memoryId.toStdString());
    if (it != m_memories.end()) {
        it->second->content = content;
    }
}

void AgenticMemorySystem::deleteMemory(const QString& memoryId)
{
    m_memories.erase(memoryId.toStdString());
}

void AgenticMemorySystem::pinMemory(const QString& memoryId, bool pinned)
{
    auto it = m_memories.find(memoryId.toStdString());
    if (it != m_memories.end()) {
        it->second->isPinned = pinned;
    }
}

// ===== MEMORY RETRIEVAL =====

AgenticMemorySystem::MemoryEntry* AgenticMemorySystem::getMemory(const QString& memoryId)
{
    auto it = m_memories.find(memoryId.toStdString());
    if (it != m_memories.end()) {
        it->second->accessCount++;
        m_totalRetrieved++;
        emit memoryRetrieved(memoryId);
        return it->second.get();
    }
    return nullptr;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::getMemoriesByType(
    MemoryType type)
{
    std::vector<MemoryEntry*> results;

    for (auto& pair : m_memories) {
        if (pair.second->type == type) {
            results.push_back(pair.second.get());
        }
    }

    return results;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::getMemoriesByTag(
    const QString& tag)
{
    std::vector<MemoryEntry*> results;

    for (auto& pair : m_memories) {
        if (pair.second->tags.contains(tag)) {
            results.push_back(pair.second.get());
        }
    }

    return results;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::searchMemories(
    const QString& query,
    int limit)
{
    std::vector<MemoryEntry*> results;

    for (auto& pair : m_memories) {
        if (pair.second->content.contains(query, Qt::CaseInsensitive)) {
            results.push_back(pair.second.get());
        }
    }

    // Sort by relevance score
    std::sort(results.begin(), results.end(),
              [](MemoryEntry* a, MemoryEntry* b) {
                  return a->relevanceScore > b->relevanceScore;
              });

    if (limit > 0 && results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

std::vector<AgenticMemorySystem::MemoryEntry*> AgenticMemorySystem::getRelevantMemories(
    const QString& context,
    int limit)
{
    std::vector<MemoryEntry*> results;

    for (auto& pair : m_memories) {
        float similarity = calculateSimilarity(pair.second->content, context);
        pair.second->relevanceScore = similarity;

        if (similarity > 0.5f) {
            results.push_back(pair.second.get());
        }
    }

    // Sort by relevance
    std::sort(results.begin(), results.end(),
              [](MemoryEntry* a, MemoryEntry* b) {
                  return a->relevanceScore > b->relevanceScore;
              });

    if (limit > 0 && results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

// ===== EXPERIENCE MANAGEMENT =====

QString AgenticMemorySystem::recordExperience(
    const QString& taskDescription,
    const QJsonObject& goalState,
    const QJsonObject& resultState,
    bool successful,
    const QStringList& strategiesUsed)
{
    QString experienceId = QUuid::createUuid().toString();

    auto experience = std::make_unique<Experience>();
    experience->experienceId = experienceId;
    experience->taskDescription = taskDescription;
    experience->goalState = goalState;
    experience->resultState = resultState;
    experience->successful = successful;
    experience->successRate = successful ? 1.0f : 0.0f;
    experience->strategiesUsed = strategiesUsed;
    experience->timestamp = QDateTime::currentDateTime();
    experience->usageCount = 0;

    m_experiences[experienceId.toStdString()] = std::move(experience);

    emit experienceRecorded(experienceId);

    return experienceId;
}

std::vector<AgenticMemorySystem::Experience> AgenticMemorySystem::findSimilarExperiences(
    const QString& currentTask,
    float minSimilarity)
{
    std::vector<Experience> similar;

    for (auto& pair : m_experiences) {
        float similarity = calculateSimilarity(pair.second->taskDescription, currentTask);

        if (similarity >= minSimilarity) {
            pair.second->similarityToCurrentTask = similarity;
            similar.push_back(*pair.second);
        }
    }

    // Sort by similarity
    std::sort(similar.begin(), similar.end(),
              [](const Experience& a, const Experience& b) {
                  return a.similarityToCurrentTask > b.similarityToCurrentTask;
              });

    return similar;
}

AgenticMemorySystem::Experience* AgenticMemorySystem::getExperience(const QString& experienceId)
{
    auto it = m_experiences.find(experienceId.toStdString());
    if (it != m_experiences.end()) {
        return it->second.get();
    }
    return nullptr;
}

void AgenticMemorySystem::recordExperienceUsage(const QString& experienceId)
{
    auto it = m_experiences.find(experienceId.toStdString());
    if (it != m_experiences.end()) {
        it->second->usageCount++;
    }
}

std::vector<AgenticMemorySystem::Experience> AgenticMemorySystem::getMostSuccessfulExperiences(
    int limit)
{
    std::vector<Experience> successful;

    for (auto& pair : m_experiences) {
        if (pair.second->successful) {
            successful.push_back(*pair.second);
        }
    }

    // Sort by success rate
    std::sort(successful.begin(), successful.end(),
              [](const Experience& a, const Experience& b) {
                  return a.successRate > b.successRate;
              });

    if (limit > 0 && successful.size() > limit) {
        successful.resize(limit);
    }

    return successful;
}

// ===== PATTERN RECOGNITION =====

QString AgenticMemorySystem::recordPattern(
    const QString& description,
    const QString& category,
    const QJsonArray& examples)
{
    QString patternId = QUuid::createUuid().toString();

    auto pattern = std::make_unique<Pattern>();
    pattern->patternId = patternId;
    pattern->description = description;
    pattern->category = category;
    pattern->examples = examples;
    pattern->confidence = 0.5f;
    pattern->occurrences = 1;
    pattern->firstObserved = QDateTime::currentDateTime();
    pattern->lastObserved = QDateTime::currentDateTime();

    m_patterns[patternId.toStdString()] = std::move(pattern);

    emit patternDetected(patternId);

    return patternId;
}

std::vector<AgenticMemorySystem::Pattern> AgenticMemorySystem::detectPatterns(
    const QString& context)
{
    std::vector<Pattern> detected;

    for (auto& pair : m_patterns) {
        // Simple pattern matching on description
        if (pair.second->description.contains(context, Qt::CaseInsensitive)) {
            detected.push_back(*pair.second);
        }
    }

    return detected;
}

std::vector<AgenticMemorySystem::Pattern> AgenticMemorySystem::getPatternsForCategory(
    const QString& category)
{
    std::vector<Pattern> patterns;

    for (auto& pair : m_patterns) {
        if (pair.second->category == category) {
            patterns.push_back(*pair.second);
        }
    }

    return patterns;
}

AgenticMemorySystem::Pattern* AgenticMemorySystem::getPattern(const QString& patternId)
{
    auto it = m_patterns.find(patternId.toStdString());
    if (it != m_patterns.end()) {
        return it->second.get();
    }
    return nullptr;
}

float AgenticMemorySystem::getPatternConfidence(const QString& patternId)
{
    auto it = m_patterns.find(patternId.toStdString());
    if (it != m_patterns.end()) {
        return it->second->confidence;
    }
    return 0.0f;
}

// ===== LEARNING =====

void AgenticMemorySystem::recordSuccess(
    const QString& taskDescription,
    const QString& strategy,
    float effectiveness)
{
    // Record successful experience
    QJsonObject goal;
    goal["task"] = taskDescription;
    goal["strategy"] = strategy;

    recordExperience(taskDescription, goal, goal, true, {strategy});
}

void AgenticMemorySystem::recordFailure(
    const QString& taskDescription,
    const QString& strategy,
    const QString& failureReason)
{
    QJsonObject goal;
    goal["task"] = taskDescription;
    goal["strategy"] = strategy;
    goal["failure_reason"] = failureReason;

    recordExperience(taskDescription, goal, goal, false, {strategy});
}

float AgenticMemorySystem::getStrategyEffectiveness(const QString& strategy) const
{
    int successCount = 0;
    int totalCount = 0;

    for (const auto& pair : m_experiences) {
        for (const auto& usedStrategy : pair.second->strategiesUsed) {
            if (usedStrategy == strategy) {
                totalCount++;
                if (pair.second->successful) {
                    successCount++;
                }
            }
        }
    }

    if (totalCount == 0) return 0.0f;

    return (successCount * 100.0f) / totalCount;
}

QStringList AgenticMemorySystem::getRankedStrategies(const QString& taskType)
{
    std::vector<std::pair<QString, float>> strategies;

    std::unordered_map<std::string, float> strategyScores;
    std::unordered_map<std::string, int> strategyCounts;

    for (const auto& pair : m_experiences) {
        for (const auto& strategy : pair.second->strategiesUsed) {
            strategyCounts[strategy.toStdString()]++;
            if (pair.second->successful) {
                strategyScores[strategy.toStdString()] += 1.0f;
            }
        }
    }

    for (const auto& pair : strategyScores) {
        float effectiveness = pair.second / strategyCounts[pair.first];
        strategies.push_back({QString::fromStdString(pair.first), effectiveness});
    }

    std::sort(strategies.begin(), strategies.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    QStringList ranked;
    for (const auto& pair : strategies) {
        ranked.append(pair.first);
    }

    return ranked;
}

// ===== CONSOLIDATION =====

void AgenticMemorySystem::consolidateMemories()
{
    updateMemoryDecay();

    emit memoriesConsolidated();
}

void AgenticMemorySystem::pruneOldMemories(int ageInDays)
{
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-ageInDays);
    int prunedCount = 0;

    for (auto it = m_memories.begin(); it != m_memories.end(); ) {
        if (it->second->timestamp < cutoff && !it->second->isPinned) {
            it = m_memories.erase(it);
            prunedCount++;
        } else {
            ++it;
        }
    }

    emit memoryPruned(prunedCount);
}

void AgenticMemorySystem::forgetMemory(const QString& memoryId)
{
    deleteMemory(memoryId);
}

float AgenticMemorySystem::getMemoryDecayFactor(const MemoryEntry& entry) const
{
    int daysSinceAccess = entry.timestamp.daysTo(QDateTime::currentDateTime());
    return std::pow(m_decayRate, daysSinceAccess);
}

// ===== STATISTICS =====

QJsonObject AgenticMemorySystem::getMemoryStatistics() const
{
    QJsonObject stats;

    stats["total_memories"] = static_cast<int>(m_memories.size());
    stats["total_experiences"] = static_cast<int>(m_experiences.size());
    stats["total_patterns"] = static_cast<int>(m_patterns.size());
    stats["total_stored"] = m_totalStored;
    stats["total_retrieved"] = m_totalRetrieved;
    stats["average_success_rate"] = getAverageSuccessRate();

    return stats;
}

float AgenticMemorySystem::getAverageSuccessRate() const
{
    if (m_experiences.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& pair : m_experiences) {
        total += pair.second->successRate;
    }

    return total / m_experiences.size();
}

QString AgenticMemorySystem::getMostCommonSuccessStrategy() const
{
    std::unordered_map<std::string, int> strategyCount;

    for (const auto& pair : m_experiences) {
        if (pair.second->successful) {
            for (const auto& strategy : pair.second->strategiesUsed) {
                strategyCount[strategy.toStdString()]++;
            }
        }
    }

    if (strategyCount.empty()) return "";

    auto maxIt = std::max_element(strategyCount.begin(), strategyCount.end(),
                                  [](const auto& a, const auto& b) {
                                      return a.second < b.second;
                                  });

    return QString::fromStdString(maxIt->first);
}

// ===== EXPORT/IMPORT =====

QString AgenticMemorySystem::exportMemories() const
{
    QJsonArray memories;

    for (const auto& pair : m_memories) {
        QJsonObject memObj;
        memObj["id"] = pair.second->id;
        memObj["type"] = static_cast<int>(pair.second->type);
        memObj["content"] = pair.second->content;
        memObj["metadata"] = pair.second->metadata;
        memObj["timestamp"] = pair.second->timestamp.toString(Qt::ISODate);

        memories.append(memObj);
    }

    return QString::fromUtf8(QJsonDocument(memories).toJson());
}

bool AgenticMemorySystem::importMemories(const QString& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isArray()) return false;

    for (const auto& item : doc.array()) {
        QJsonObject obj = item.toObject();
        storeMemory(
            static_cast<MemoryType>(obj["type"].toInt()),
            obj["content"].toString(),
            obj["metadata"].toObject()
        );
    }

    return true;
}

QString AgenticMemorySystem::exportExperiences() const
{
    QJsonArray experiences;

    for (const auto& pair : m_experiences) {
        QJsonObject expObj;
        expObj["id"] = pair.second->experienceId;
        expObj["task"] = pair.second->taskDescription;
        expObj["successful"] = pair.second->successful;
        expObj["timestamp"] = pair.second->timestamp.toString(Qt::ISODate);

        experiences.append(expObj);
    }

    return QString::fromUtf8(QJsonDocument(experiences).toJson());
}

bool AgenticMemorySystem::importExperiences(const QString& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isArray()) return false;

    // Process experiences as needed
    return true;
}

// ===== CONFIGURATION =====

void AgenticMemorySystem::setRetentionPolicy(int maxMemories)
{
    m_maxMemories = maxMemories;
}

void AgenticMemorySystem::setDecayRate(float rate)
{
    m_decayRate = rate;
}

void AgenticMemorySystem::setAccessWeightFactor(float factor)
{
    m_accessWeightFactor = factor;
}

// ===== PRIVATE HELPERS =====

float AgenticMemorySystem::calculateSimilarity(const QString& str1, const QString& str2)
{
    // Simple word-based similarity
    QStringList words1 = str1.split(QRegularExpression("\\s+"));
    QStringList words2 = str2.split(QRegularExpression("\\s+"));

    int matches = 0;
    for (const auto& w1 : words1) {
        if (words2.contains(w1)) matches++;
    }

    int total = std::max(words1.size(), words2.size());
    return total > 0 ? (matches * 1.0f) / total : 0.0f;
}

void AgenticMemorySystem::updateMemoryDecay()
{
    for (auto& pair : m_memories) {
        float decayFactor = getMemoryDecayFactor(*pair.second);
        pair.second->relevanceScore *= decayFactor;
    }
}

float AgenticMemorySystem::calculateRelevanceScore(
    const MemoryEntry& entry,
    const QString& context)
{
    float similarity = calculateSimilarity(entry.content, context);
    float recency = getMemoryDecayFactor(entry);
    float accessBoost = std::log(entry.accessCount + 1) * m_accessWeightFactor;

    return (similarity * 0.5f + recency * 0.3f + accessBoost * 0.2f);
}

QString AgenticMemorySystem::hashContent(const QString& content)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(content.toUtf8());
    return QString(hash.result().toHex());
}

void AgenticMemorySystem::extractPatterns()
{
    // Pattern extraction logic
}
