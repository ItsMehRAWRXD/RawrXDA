// ═══════════════════════════════════════════════════════════════════════════════
// REVERSE LINE DIFFERENCE INTEGRATION
// Integrates line difference analysis with reverse feature engine
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef REVERSE_LINE_DIFFERENCE_H
#define REVERSE_LINE_DIFFERENCE_H

#include "reverse_feature_engine.h"
#include "line_difference_generator.h"
#include <QVector>
#include <QHash>

// Enhanced reverse feature with line difference analysis
struct ReverseFeatureWithDifferences : public ReverseFeature {
    QVector<LineDifference> lineDifferences;
    QVector<CharacterPosition> allCharacterPositions;
    double averageEntropy;
    double maxEntropy;
    double minEntropy;
    int totalCharacters;
    int changedCharacters;
    
    QJsonObject toJson() const {
        QJsonObject obj = ReverseFeature::toJson();
        obj["averageEntropy"] = averageEntropy;
        obj["maxEntropy"] = maxEntropy;
        obj["minEntropy"] = minEntropy;
        obj["totalCharacters"] = totalCharacters;
        obj["changedCharacters"] = changedCharacters;
        
        QJsonArray diffs;
        for (const auto& diff : lineDifferences) {
            diffs.append(diff.toJson());
        }
        obj["lineDifferences"] = diffs;
        
        return obj;
    }
};

class ReverseLineDifferenceEngine : public ReverseFeatureEngine {
    Q_OBJECT

public:
    explicit ReverseLineDifferenceEngine(QObject* parent = nullptr);
    
    // Enhanced deconstruction with line difference analysis
    bool deconstructFeatureWithDifferences(int reverseId);
    
    // Generate comprehensive difference report
    QString generateDifferenceReport(int reverseId) const;
    
    // Analyze character positions across all features
    void analyzeAllCharacterPositions();
    
    // Get character statistics
    QHash<QChar, int> getCharacterFrequency() const;
    double getOverallEntropy() const;
    
    // Configuration
    void setDifferenceGenerator(LineDifferenceGenerator* generator);

private:
    LineDifferenceGenerator* m_diffGenerator;
    QHash<int, ReverseFeatureWithDifferences> m_enhancedFeatures;
    
    // Helper methods
    void calculateFeatureStatistics(ReverseFeatureWithDifferences& feature);
    QString generateCharacterAnalysis(const ReverseFeatureWithDifferences& feature);
};

#endif // REVERSE_LINE_DIFFERENCE_H
