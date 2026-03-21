#ifndef IDE_SEMANTIC_ANALYSIS_ENGINE_HPP
#define IDE_SEMANTIC_ANALYSIS_ENGINE_HPP

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>

struct SemanticContext {
    QString filePath;
    int cursorLine;
    int cursorColumn;
    QString selectedText;
    QString lineContent;
    QList<QString> visibleLines;
    QMap<QString, QString> symbolContext;
    QList<QString> recentEdits;
};

struct InlineInsight {
    int line;
    int column;
    QString message;
    QString severity;
    QString actionType;
    QString actionCode;
    double relevanceScore;
};

struct ContextualRecommendation {
    QString type;
    QString title;
    QString description;
    QList<QString> codeSnippets;
    QString rationale;
    double confidence;
    QList<QString> relatedPatterns;
};

class IDESemanticAnalysisEngine {
public:
    static QList<InlineInsight> analyzeAtCursor(const SemanticContext& context);
    static QList<ContextualRecommendation> generateContextualRecommendations(const SemanticContext& context);
    static QJsonObject performRealTimeAnalysis(const QString& code, int cursorPosition);
    static QList<QString> suggestRefactorings(const SemanticContext& context);
    static QJsonArray buildSemanticIndex(const QString& code);
    static QList<QString> findRelatedSymbols(const SemanticContext& context);
    static QJsonObject analyzeCodePattern(const QString& code, int startLine, int endLine);
    
private:
    static QList<InlineInsight> performSyntacticAnalysis(const SemanticContext& context);
    static QList<InlineInsight> performSemanticAnalysis(const SemanticContext& context);
    static QList<InlineInsight> performTypeAnalysis(const SemanticContext& context);
    static QList<InlineInsight> performFlowAnalysis(const SemanticContext& context);
    static QList<ContextualRecommendation> generateRefactoringRecommendations(const SemanticContext& context);
    static QList<ContextualRecommendation> generatePerformanceRecommendations(const SemanticContext& context);
    static QList<ContextualRecommendation> generateSecurityRecommendations(const SemanticContext& context);
    static QList<ContextualRecommendation> generateDesignPatternRecommendations(const SemanticContext& context);
    static QString extractSymbolAtCursor(const SemanticContext& context);
    static QList<QString> findSymbolDefinitions(const QString& symbol, const QString& code);
    static QList<QString> findSymbolUsages(const QString& symbol, const QString& code);
    static QJsonObject buildCallHierarchy(const QString& code);
    static QJsonObject buildInheritanceHierarchy(const QString& code);
    static double calculateRelevanceScore(const InlineInsight& insight, const SemanticContext& context);
};

#endif // IDE_SEMANTIC_ANALYSIS_ENGINE_HPP
