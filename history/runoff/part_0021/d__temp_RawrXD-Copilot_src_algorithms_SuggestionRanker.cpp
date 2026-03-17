#include "SuggestionRanker.h"
#include <QDateTime>
#include <QDebug>
#include <QtMath>
#include <QRegularExpression>
#include <algorithm>

SuggestionRanker::SuggestionRanker(QObject *parent)
    : QObject(parent)
{
    resetWeightsToDefault();
}

SuggestionRanker::~SuggestionRanker()
{
}

QList<SuggestionRanker::RankedSuggestion> SuggestionRanker::rankSuggestions(
    const QList<QJsonObject>& suggestions,
    const QString& context,
    const QJsonObject& metadata)
{
    QList<RankedSuggestion> ranked;
    
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    // Calculate individual scores for each suggestion
    for (const QJsonObject& suggestion : suggestions) {
        SuggestionScore score;
        
        score.relevanceScore = calculateRelevanceScore(suggestion, context);
        score.popularityScore = calculatePopularityScore(suggestion);
        score.qualityScore = calculateQualityScore(suggestion);
        score.recencyScore = calculateRecencyScore(suggestion);
        score.diversityScore = calculateDiversityScore(suggestion, suggestions);
        score.performanceScore = calculatePerformanceScore(suggestion);
        score.userPreferenceScore = calculateUserPreferenceScore(suggestion);
        
        // Compute weighted total score
        score.totalScore = 
            score.relevanceScore * m_weights.relevanceWeight +
            score.popularityScore * m_weights.popularityWeight +
            score.qualityScore * m_weights.qualityWeight +
            score.recencyScore * m_weights.recencyWeight +
            score.diversityScore * m_weights.diversityWeight +
            score.performanceScore * m_weights.performanceWeight +
            score.userPreferenceScore * m_weights.userPreferenceWeight;
        
        RankedSuggestion rs;
        rs.suggestion = suggestion;
        rs.score = score;
        rs.rationale = QString("Relevance: %1%, Quality: %2%, Popularity: %3%")
            .arg(qRound(score.relevanceScore * 100))
            .arg(qRound(score.qualityScore * 100))
            .arg(qRound(score.popularityScore * 100));
        
        ranked.append(rs);
    }
    
    // Sort by total score (descending)
    std::sort(ranked.begin(), ranked.end(), 
        [](const RankedSuggestion& a, const RankedSuggestion& b) {
            return a.score.totalScore > b.score.totalScore;
        });
    
    // Assign ranks
    for (int i = 0; i < ranked.size(); ++i) {
        ranked[i].rank = i + 1;
    }
    
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();
    
    // Log performance metrics
    QJsonObject logData;
    logData["operation"] = "rankSuggestions";
    logData["suggestionCount"] = suggestions.size();
    logData["durationMs"] = endTime - startTime;
    logData["topScore"] = ranked.isEmpty() ? 0.0 : ranked.first().score.totalScore;
    
    qDebug() << "[RANKING]" << logData;
    
    emit rankingCompleted(ranked);
    
    return ranked;
}

double SuggestionRanker::calculateRelevanceScore(const QJsonObject& suggestion, const QString& context)
{
    QString suggestionText = suggestion["text"].toString();
    
    if (suggestionText.isEmpty() || context.isEmpty()) {
        return 0.0;
    }
    
    // Extract features from suggestion and context
    QVector<double> suggestionVec = vectorize(suggestionText);
    QVector<double> contextVec = vectorize(context);
    
    // Calculate cosine similarity
    double similarity = computeCosineSimilarity(suggestionVec, contextVec);
    
    // Bonus for exact keyword matches
    QStringList contextKeywords = context.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    int matchCount = 0;
    for (const QString& keyword : contextKeywords) {
        if (keyword.length() > 3 && suggestionText.contains(keyword, Qt::CaseInsensitive)) {
            matchCount++;
        }
    }
    
    double keywordBonus = qMin(0.3, matchCount * 0.05);
    
    return qBound(0.0, similarity + keywordBonus, 1.0);
}

double SuggestionRanker::calculatePopularityScore(const QJsonObject& suggestion)
{
    QString pattern = extractPattern(suggestion);
    
    int acceptances = m_acceptanceCount.value(pattern, 0);
    int rejections = m_rejectionCount.value(pattern, 0);
    int total = acceptances + rejections;
    
    if (total == 0) {
        return 0.5; // Neutral score for new patterns
    }
    
    // Bayesian average to prevent overfitting on small samples
    const int priorCount = 10;
    const double priorAcceptanceRate = 0.5;
    
    double bayesianAverage = 
        (acceptances + priorCount * priorAcceptanceRate) / 
        (total + priorCount);
    
    // Apply logarithmic scaling to reward patterns with more data
    double confidenceFactor = qMin(1.0, qLn(total + 1) / qLn(100));
    
    return bayesianAverage * confidenceFactor + 0.5 * (1.0 - confidenceFactor);
}

double SuggestionRanker::calculateQualityScore(const QJsonObject& suggestion)
{
    QString code = suggestion["text"].toString();
    
    double complexityScore = 1.0 - qBound(0.0, assessCodeComplexity(code) / 20.0, 1.0);
    double namingScore = assessNamingConventions(code);
    double consistencyScore = assessConsistency(code, ""); // TODO: Pass existing code context
    
    // Weighted average
    return complexityScore * 0.4 + namingScore * 0.3 + consistencyScore * 0.3;
}

double SuggestionRanker::calculateRecencyScore(const QJsonObject& suggestion)
{
    QString pattern = extractPattern(suggestion);
    qint64 lastSeen = m_lastSeenTimestamp.value(pattern, 0);
    
    if (lastSeen == 0) {
        return 0.5; // Neutral for new patterns
    }
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 ageMs = currentTime - lastSeen;
    
    // Exponential decay: score = exp(-age / halfLife)
    const qint64 halfLifeMs = 7 * 24 * 60 * 60 * 1000; // 1 week
    double decayFactor = qExp(-static_cast<double>(ageMs) / halfLifeMs);
    
    return decayFactor;
}

double SuggestionRanker::calculateDiversityScore(const QJsonObject& suggestion, const QList<QJsonObject>& others)
{
    if (others.size() <= 1) {
        return 1.0; // Maximum diversity if alone
    }
    
    QVector<double> features = extractFeatures(suggestion);
    double totalDiversity = 0.0;
    int count = 0;
    
    for (const QJsonObject& other : others) {
        if (other == suggestion) continue;
        
        double pairwiseDiversity = calculatePairwiseDiversity(suggestion, other);
        totalDiversity += pairwiseDiversity;
        count++;
    }
    
    return count > 0 ? totalDiversity / count : 1.0;
}

double SuggestionRanker::calculatePerformanceScore(const QJsonObject& suggestion)
{
    QString pattern = extractPattern(suggestion);
    
    if (!m_performanceHistory.contains(pattern)) {
        return 0.5; // Neutral for unknown patterns
    }
    
    PerformanceMetrics metrics = m_performanceHistory[pattern];
    
    // Normalize generation time (lower is better)
    double timeScore = 1.0 - qBound(0.0, metrics.avgGenerationTime / 5000.0, 1.0);
    
    // Normalize token count (prefer concise suggestions)
    double tokenScore = 1.0 - qBound(0.0, metrics.tokenCount / 500.0, 1.0);
    
    // Confidence score (higher is better)
    double confidenceScore = metrics.confidenceScore;
    
    return timeScore * 0.3 + tokenScore * 0.3 + confidenceScore * 0.4;
}

double SuggestionRanker::calculateUserPreferenceScore(const QJsonObject& suggestion)
{
    QString code = suggestion["text"].toString();
    
    double score = 0.0;
    int matchCount = 0;
    
    // Check code style preferences
    QMap<QString, QRegularExpression> stylePatterns = {
        {"camelCase", QRegularExpression("\\b[a-z]+[A-Z][a-zA-Z]*\\b")},
        {"snake_case", QRegularExpression("\\b[a-z]+_[a-z_]+\\b")},
        {"braceNewLine", QRegularExpression("\\n\\s*\\{")},
        {"braceInline", QRegularExpression("[^\\n]\\s*\\{")}
    };
    
    for (auto it = stylePatterns.constBegin(); it != stylePatterns.constEnd(); ++it) {
        if (code.contains(it.value())) {
            double preference = m_stylePreferences.value(it.key(), 0.5);
            score += preference;
            matchCount++;
        }
    }
    
    return matchCount > 0 ? score / matchCount : 0.5;
}

void SuggestionRanker::recordAcceptance(const QJsonObject& suggestion, const QString& context)
{
    QString pattern = extractPattern(suggestion);
    
    m_acceptanceCount[pattern]++;
    m_lastSeenTimestamp[pattern] = QDateTime::currentMSecsSinceEpoch();
    
    // Update context patterns
    if (!m_contextPatterns[context].contains(pattern)) {
        m_contextPatterns[context].append(pattern);
    }
    
    emit feedbackRecorded("acceptance", suggestion);
    
    // Log structured feedback
    QJsonObject logData;
    logData["type"] = "acceptance";
    logData["pattern"] = pattern;
    logData["context"] = context.left(50);
    logData["totalAcceptances"] = m_acceptanceCount[pattern];
    
    qDebug() << "[FEEDBACK]" << logData;
}

void SuggestionRanker::recordRejection(const QJsonObject& suggestion, const QString& context)
{
    QString pattern = extractPattern(suggestion);
    
    m_rejectionCount[pattern]++;
    m_lastSeenTimestamp[pattern] = QDateTime::currentMSecsSinceEpoch();
    
    emit feedbackRecorded("rejection", suggestion);
    
    // Log structured feedback
    QJsonObject logData;
    logData["type"] = "rejection";
    logData["pattern"] = pattern;
    logData["context"] = context.left(50);
    logData["totalRejections"] = m_rejectionCount[pattern];
    
    qDebug() << "[FEEDBACK]" << logData;
}

void SuggestionRanker::updateWeightsBasedOnFeedback()
{
    // Adaptive weight adjustment based on acceptance rates per score component
    if (m_feedbackHistory.empty()) {
        return;
    }
    
    // Calculate acceptance rates for each component
    std::map<std::string, double> componentAcceptanceRates;
    std::map<std::string, int> componentCounts;
    std::map<std::string, int> componentAccepted;
    
    for (const auto& feedback : m_feedbackHistory) {
        for (const auto& [component, score] : feedback) {
            componentCounts[component]++;
            if (score > 0.5) {  // Accepted if score > 0.5
                componentAccepted[component]++;
            }
        }
    }
    
    // Update weights using gradient-based approach
    const double learningRate = 0.05;  // Conservative learning rate
    
    for (const auto& [component, count] : componentCounts) {
        if (count > 0) {
            double acceptanceRate = componentAccepted[component] / static_cast<double>(count);
            
            // Boost weight if component has high acceptance, reduce if low
            if (component == "relevance") {
                m_weights.relevanceWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "popularity") {
                m_weights.popularityWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "quality") {
                m_weights.qualityWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "recency") {
                m_weights.recencyWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "diversity") {
                m_weights.diversityWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "performance") {
                m_weights.performanceWeight += (acceptanceRate - 0.5) * learningRate;
            } else if (component == "userPreference") {
                m_weights.userPreferenceWeight += (acceptanceRate - 0.5) * learningRate;
            }
        }
    }
    
    // Normalize weights to sum to 1.0
    double totalWeight = m_weights.relevanceWeight + m_weights.popularityWeight +
                        m_weights.qualityWeight + m_weights.recencyWeight +
                        m_weights.diversityWeight + m_weights.performanceWeight +
                        m_weights.userPreferenceWeight;
    
    if (totalWeight > 0) {
        m_weights.relevanceWeight /= totalWeight;
        m_weights.popularityWeight /= totalWeight;
        m_weights.qualityWeight /= totalWeight;
        m_weights.recencyWeight /= totalWeight;
        m_weights.diversityWeight /= totalWeight;
        m_weights.performanceWeight /= totalWeight;
        m_weights.userPreferenceWeight /= totalWeight;
    }
    
    // Keep weights in reasonable bounds
    auto clampWeight = [](double& w) {
        w = std::max(0.0, std::min(1.0, w));
    };
    clampWeight(m_weights.relevanceWeight);
    clampWeight(m_weights.popularityWeight);
    clampWeight(m_weights.qualityWeight);
    clampWeight(m_weights.recencyWeight);
    clampWeight(m_weights.diversityWeight);
    clampWeight(m_weights.performanceWeight);
    clampWeight(m_weights.userPreferenceWeight);
    
    emit weightsUpdated(m_weights);
}

QList<QJsonObject> SuggestionRanker::filterSuggestions(
    const QList<QJsonObject>& suggestions,
    const std::function<bool(const QJsonObject&)>& filter)
{
    QList<QJsonObject> filtered;
    
    for (const QJsonObject& suggestion : suggestions) {
        if (filter(suggestion)) {
            filtered.append(suggestion);
        }
    }
    
    return filtered;
}

void SuggestionRanker::setWeights(const RankingWeights& weights)
{
    RankingWeights normalized = weights;
    normalized.normalize();
    m_weights = normalized;
    
    emit weightsUpdated(m_weights);
}

SuggestionRanker::RankingWeights SuggestionRanker::getWeights() const
{
    return m_weights;
}

void SuggestionRanker::resetWeightsToDefault()
{
    m_weights = RankingWeights();
    m_weights.normalize();
}

QJsonObject SuggestionRanker::getRankingStatistics() const
{
    QJsonObject stats;
    
    stats["totalPatterns"] = m_acceptanceCount.size() + m_rejectionCount.size();
    stats["totalAcceptances"] = std::accumulate(
        m_acceptanceCount.begin(), m_acceptanceCount.end(), 0);
    stats["totalRejections"] = std::accumulate(
        m_rejectionCount.begin(), m_rejectionCount.end(), 0);
    
    // Calculate overall acceptance rate
    int totalAcceptances = stats["totalAcceptances"].toInt();
    int totalRejections = stats["totalRejections"].toInt();
    double acceptanceRate = (totalAcceptances + totalRejections) > 0
        ? static_cast<double>(totalAcceptances) / (totalAcceptances + totalRejections)
        : 0.0;
    
    stats["acceptanceRate"] = acceptanceRate;
    stats["currentWeights"] = m_weights.toJson();
    
    return stats;
}

void SuggestionRanker::clearHistory()
{
    m_acceptanceCount.clear();
    m_rejectionCount.clear();
    m_lastSeenTimestamp.clear();
    m_contextPatterns.clear();
    m_performanceHistory.clear();
}

// ============================================================================
// Helper Methods
// ============================================================================

QString SuggestionRanker::extractPattern(const QJsonObject& suggestion)
{
    QString text = suggestion["text"].toString();
    
    // Create a simplified pattern by removing literals and identifiers
    QString pattern = text;
    
    // Remove string literals
    pattern.replace(QRegularExpression("\"[^\"]*\""), "\"\"");
    pattern.replace(QRegularExpression("'[^']*'"), "''");
    
    // Remove numbers
    pattern.replace(QRegularExpression("\\d+"), "0");
    
    // Normalize whitespace
    pattern = pattern.simplified();
    
    return pattern.left(200); // Limit pattern length
}

double SuggestionRanker::computeCosineSimilarity(const QVector<double>& vec1, const QVector<double>& vec2)
{
    if (vec1.size() != vec2.size() || vec1.isEmpty()) {
        return 0.0;
    }
    
    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;
    
    for (int i = 0; i < vec1.size(); ++i) {
        dotProduct += vec1[i] * vec2[i];
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
    }
    
    double denominator = qSqrt(norm1) * qSqrt(norm2);
    
    return denominator > 0.0 ? dotProduct / denominator : 0.0;
}

QVector<double> SuggestionRanker::vectorize(const QString& text)
{
    // Simple bag-of-words vectorization
    // Production version would use proper tokenization and TF-IDF
    
    QStringList words = text.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts);
    QMap<QString, int> wordCounts;
    
    for (const QString& word : words) {
        QString normalized = word.toLower();
        wordCounts[normalized]++;
    }
    
    // Create fixed-size vector (using hash for dimensionality reduction)
    const int vectorSize = 100;
    QVector<double> vector(vectorSize, 0.0);
    
    for (auto it = wordCounts.constBegin(); it != wordCounts.constEnd(); ++it) {
        uint hash = qHash(it.key()) % vectorSize;
        vector[hash] += it.value();
    }
    
    return vector;
}

double SuggestionRanker::assessCodeComplexity(const QString& code)
{
    // Cyclomatic complexity approximation
    int complexity = 1; // Base complexity
    
    QStringList complexityKeywords = {
        "if", "else", "for", "while", "switch", "case", 
        "catch", "&&", "||", "?", "break", "continue"
    };
    
    for (const QString& keyword : complexityKeywords) {
        complexity += code.count(QRegularExpression("\\b" + keyword + "\\b"));
    }
    
    // Add function count
    complexity += code.count(QRegularExpression("\\bfunction\\b"));
    complexity += code.count(QRegularExpression("\\w+\\s*\\([^)]*\\)\\s*\\{"));
    
    return qMin(complexity, 50); // Cap at 50
}

double SuggestionRanker::assessNamingConventions(const QString& code)
{
    // Check for consistent naming conventions
    int camelCaseCount = code.count(QRegularExpression("\\b[a-z]+[A-Z][a-zA-Z]*\\b"));
    int snakeCaseCount = code.count(QRegularExpression("\\b[a-z]+_[a-z_]+\\b"));
    int totalIdentifiers = camelCaseCount + snakeCaseCount;
    
    if (totalIdentifiers == 0) {
        return 0.5; // Neutral
    }
    
    // Prefer consistency (either all camelCase or all snake_case)
    int maxConsistentCount = qMax(camelCaseCount, snakeCaseCount);
    double consistencyRatio = static_cast<double>(maxConsistentCount) / totalIdentifiers;
    
    return consistencyRatio;
}

double SuggestionRanker::assessConsistency(const QString& code, const QString& existingCode)
{
    if (existingCode.isEmpty()) {
        return 0.5; // Neutral if no existing code to compare
    }
    
    // Compare indentation style
    bool codeUsesSpaces = code.contains(QRegularExpression("^    ", QRegularExpression::MultilineOption));
    bool existingUsesSpaces = existingCode.contains(QRegularExpression("^    ", QRegularExpression::MultilineOption));
    
    double indentationMatch = (codeUsesSpaces == existingUsesSpaces) ? 1.0 : 0.0;
    
    // Compare brace style
    bool codeNewlineBrace = code.contains(QRegularExpression("\\n\\s*\\{"));
    bool existingNewlineBrace = existingCode.contains(QRegularExpression("\\n\\s*\\{"));
    
    double braceMatch = (codeNewlineBrace == existingNewlineBrace) ? 1.0 : 0.0;
    
    return (indentationMatch + braceMatch) / 2.0;
}

QVector<double> SuggestionRanker::extractFeatures(const QJsonObject& suggestion)
{
    QString code = suggestion["text"].toString();
    
    QVector<double> features;
    features.append(code.length());
    features.append(code.count('\n'));
    features.append(code.count(';'));
    features.append(code.count('{'));
    features.append(assessCodeComplexity(code));
    
    return features;
}

double SuggestionRanker::calculatePairwiseDiversity(const QJsonObject& s1, const QJsonObject& s2)
{
    QString text1 = s1["text"].toString();
    QString text2 = s2["text"].toString();
    
    // Calculate edit distance
    double editDist = calculateEditDistance(text1.left(100), text2.left(100));
    int maxLen = qMax(text1.length(), text2.length());
    
    // Normalize to [0, 1] where 1 = completely different
    return maxLen > 0 ? qMin(1.0, editDist / maxLen) : 0.0;
}

double SuggestionRanker::calculateEditDistance(const QString& str1, const QString& str2)
{
    // Levenshtein distance
    int len1 = str1.length();
    int len2 = str2.length();
    
    QVector<QVector<int>> dp(len1 + 1, QVector<int>(len2 + 1));
    
    for (int i = 0; i <= len1; ++i) dp[i][0] = i;
    for (int j = 0; j <= len2; ++j) dp[0][j] = j;
    
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (str1[i - 1] == str2[j - 1]) ? 0 : 1;
            dp[i][j] = qMin({
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + cost
            });
        }
    }
    
    return dp[len1][len2];
}

// ============================================================================
// Serialization Methods
// ============================================================================

QJsonObject SuggestionRanker::SuggestionScore::toJson() const
{
    QJsonObject obj;
    obj["relevanceScore"] = relevanceScore;
    obj["popularityScore"] = popularityScore;
    obj["qualityScore"] = qualityScore;
    obj["recencyScore"] = recencyScore;
    obj["diversityScore"] = diversityScore;
    obj["performanceScore"] = performanceScore;
    obj["userPreferenceScore"] = userPreferenceScore;
    obj["totalScore"] = totalScore;
    return obj;
}

SuggestionRanker::SuggestionScore SuggestionRanker::SuggestionScore::fromJson(const QJsonObject& obj)
{
    SuggestionScore score;
    score.relevanceScore = obj["relevanceScore"].toDouble();
    score.popularityScore = obj["popularityScore"].toDouble();
    score.qualityScore = obj["qualityScore"].toDouble();
    score.recencyScore = obj["recencyScore"].toDouble();
    score.diversityScore = obj["diversityScore"].toDouble();
    score.performanceScore = obj["performanceScore"].toDouble();
    score.userPreferenceScore = obj["userPreferenceScore"].toDouble();
    score.totalScore = obj["totalScore"].toDouble();
    return score;
}

void SuggestionRanker::RankingWeights::normalize()
{
    double sum = relevanceWeight + popularityWeight + qualityWeight + 
                 recencyWeight + diversityWeight + performanceWeight + 
                 userPreferenceWeight;
    
    if (sum > 0.0) {
        relevanceWeight /= sum;
        popularityWeight /= sum;
        qualityWeight /= sum;
        recencyWeight /= sum;
        diversityWeight /= sum;
        performanceWeight /= sum;
        userPreferenceWeight /= sum;
    }
}

QJsonObject SuggestionRanker::RankingWeights::toJson() const
{
    QJsonObject obj;
    obj["relevanceWeight"] = relevanceWeight;
    obj["popularityWeight"] = popularityWeight;
    obj["qualityWeight"] = qualityWeight;
    obj["recencyWeight"] = recencyWeight;
    obj["diversityWeight"] = diversityWeight;
    obj["performanceWeight"] = performanceWeight;
    obj["userPreferenceWeight"] = userPreferenceWeight;
    return obj;
}

SuggestionRanker::RankingWeights SuggestionRanker::RankingWeights::fromJson(const QJsonObject& obj)
{
    RankingWeights weights;
    weights.relevanceWeight = obj["relevanceWeight"].toDouble();
    weights.popularityWeight = obj["popularityWeight"].toDouble();
    weights.qualityWeight = obj["qualityWeight"].toDouble();
    weights.recencyWeight = obj["recencyWeight"].toDouble();
    weights.diversityWeight = obj["diversityWeight"].toDouble();
    weights.performanceWeight = obj["performanceWeight"].toDouble();
    weights.userPreferenceWeight = obj["userPreferenceWeight"].toDouble();
    return weights;
}
