#ifndef SUGGESTIONRANKER_H
#define SUGGESTIONRANKER_H

#include <QObject>
#include <QJsonObject>
#include <QVector>
#include <QPair>
#include <QMap>
#include <functional>

/**
 * @brief Hybrid ranking system combining collaborative and content-based filtering
 * 
 * This implements a sophisticated multi-factor ranking algorithm that considers:
 * - Contextual relevance (content-based filtering)
 * - Historical acceptance patterns (collaborative filtering)
 * - Code quality metrics
 * - User preferences
 * - Performance characteristics
 */
class SuggestionRanker : public QObject
{
    Q_OBJECT

public:
    struct SuggestionScore {
        double relevanceScore = 0.0;      // Content-based: How well it fits the context
        double popularityScore = 0.0;     // Collaborative: How often it's accepted
        double qualityScore = 0.0;        // Code quality metrics
        double recencyScore = 0.0;        // How recent the pattern is
        double diversityScore = 0.0;      // Diversity from other suggestions
        double performanceScore = 0.0;    // Performance characteristics
        double userPreferenceScore = 0.0; // Alignment with user coding style
        
        double totalScore = 0.0;          // Weighted sum of all scores
        
        QJsonObject toJson() const;
        static SuggestionScore fromJson(const QJsonObject& obj);
    };

    struct RankingWeights {
        double relevanceWeight = 0.35;
        double popularityWeight = 0.15;
        double qualityWeight = 0.25;
        double recencyWeight = 0.10;
        double diversityWeight = 0.05;
        double performanceWeight = 0.05;
        double userPreferenceWeight = 0.05;
        
        void normalize(); // Ensure weights sum to 1.0
        QJsonObject toJson() const;
        static RankingWeights fromJson(const QJsonObject& obj);
    };

    struct RankedSuggestion {
        QJsonObject suggestion;
        SuggestionScore score;
        int rank;
        QString rationale; // Human-readable explanation of ranking
    };

    explicit SuggestionRanker(QObject *parent = nullptr);
    ~SuggestionRanker();

    // Core ranking methods
    QList<RankedSuggestion> rankSuggestions(
        const QList<QJsonObject>& suggestions,
        const QString& context,
        const QJsonObject& metadata = QJsonObject()
    );

    // Individual scoring methods
    double calculateRelevanceScore(const QJsonObject& suggestion, const QString& context);
    double calculatePopularityScore(const QJsonObject& suggestion);
    double calculateQualityScore(const QJsonObject& suggestion);
    double calculateRecencyScore(const QJsonObject& suggestion);
    double calculateDiversityScore(const QJsonObject& suggestion, const QList<QJsonObject>& others);
    double calculatePerformanceScore(const QJsonObject& suggestion);
    double calculateUserPreferenceScore(const QJsonObject& suggestion);

    // Weight management
    void setWeights(const RankingWeights& weights);
    RankingWeights getWeights() const;
    void resetWeightsToDefault();

    // Learning and adaptation
    void recordAcceptance(const QJsonObject& suggestion, const QString& context);
    void recordRejection(const QJsonObject& suggestion, const QString& context);
    void updateWeightsBasedOnFeedback(); // Adaptive learning

    // Filtering
    QList<QJsonObject> filterSuggestions(
        const QList<QJsonObject>& suggestions,
        const std::function<bool(const QJsonObject&)>& filter
    );

    // Statistics and insights
    QJsonObject getRankingStatistics() const;
    void clearHistory();

signals:
    void rankingCompleted(const QList<RankedSuggestion>& rankedSuggestions);
    void weightsUpdated(const RankingWeights& newWeights);
    void feedbackRecorded(const QString& type, const QJsonObject& suggestion);

private:
    RankingWeights m_weights;
    
    // Historical data for collaborative filtering
    QMap<QString, int> m_acceptanceCount;     // Pattern -> acceptance count
    QMap<QString, int> m_rejectionCount;      // Pattern -> rejection count
    QMap<QString, qint64> m_lastSeenTimestamp; // Pattern -> timestamp
    QMap<QString, QList<QString>> m_contextPatterns; // Context -> patterns
    
    // User preference learning
    QMap<QString, double> m_stylePreferences; // Style attribute -> preference score
    
    // Performance tracking
    struct PerformanceMetrics {
        qint64 avgGenerationTime = 0;
        int tokenCount = 0;
        double confidenceScore = 0.0;
    };
    QMap<QString, PerformanceMetrics> m_performanceHistory;
    
    // Internal helper methods
    QString extractPattern(const QJsonObject& suggestion);
    double computeTFIDF(const QString& term, const QString& document, const QStringList& corpus);
    double computeCosineSimilarity(const QVector<double>& vec1, const QVector<double>& vec2);
    QVector<double> vectorize(const QString& text);
    double calculateEditDistance(const QString& str1, const QString& str2);
    
    // Collaborative filtering helpers
    double computeCollaborativeScore(const QString& pattern);
    QList<QString> findSimilarPatterns(const QString& pattern, int topK = 5);
    
    // Quality assessment
    double assessCodeComplexity(const QString& code);
    double assessNamingConventions(const QString& code);
    double assessConsistency(const QString& code, const QString& existingCode);
    
    // Diversity calculation
    QVector<double> extractFeatures(const QJsonObject& suggestion);
    double calculatePairwiseDiversity(const QJsonObject& s1, const QJsonObject& s2);
    
    // Structured logging
    void logRanking(const QList<RankedSuggestion>& ranked);
};

#endif // SUGGESTIONRANKER_H
