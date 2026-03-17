#pragma once

#include <QString>
#include <QObject>
#include <QVector>
#include <functional>
#include <memory>
#include <QObject>
#include <QMap>

// Forward declarations
class AIIntegrationHub;
class AgenticExecutor;
class MultiModalModelRouter;

namespace RawrXD {

/**
 * @brief Cursor-style inline completion and workflow engine
 * 
 * Provides Cursor IDE-like features:
 * - Sub-50ms inline completions
 * - Smart refactoring (Cmd+K style)
 * - Comment→Code generation
 * - Context-aware suggestions
 * - Multi-cursor editing support
 */
class CursorWorkflowEngine : public QObject {
    Q_OBJECT

public:
    struct CursorContext {
        QString currentFile;
        QString selectedText;
        QString linePrefix;         // Text before cursor
        QString lineSuffix;         // Text after cursor
        int cursorLine{0};
        int cursorColumn{0};
        QVector<QString> recentEdits;
        QString clipboardContent;
        QMap<QString, QString> openFiles;
        QString recentCommands;
        QString projectRoot;
        QString language;
    };

    struct CodeCompletion {
        QString text;
        QString description;
        double confidence{0.0};
        int priority{0};
    };

    struct CodeChange {
        QString filePath;
        int startLine{0};
        int endLine{0};
        QString originalCode;
        QString newCode;
        QString reasoning;
    };

    explicit CursorWorkflowEngine(QObject* parent = nullptr);
    ~CursorWorkflowEngine();

    // Initialization
    void initialize(AIIntegrationHub* hub, AgenticExecutor* executor);
    bool isReady() const;

    // Cursor-style inline completions
    void triggerInlineCompletion(const CursorContext& ctx);
    void acceptInlineCompletion();
    void rejectInlineCompletion();
    QString getCurrentCompletion() const;

    // Multi-cursor editing
    void applyMultiCursorEdits(const QVector<CursorContext>& cursors);

    // Smart refactoring (Cmd+K style)
    void smartRefactor(const QString& instruction, const CursorContext& ctx);

    // Code generation from comments
    void generateFromComment(const QString& comment, const CursorContext& ctx);

    // Context-aware suggestions
    QVector<QString> getContextualSuggestions(const CursorContext& ctx);

    // Configuration
    void setLatencyTarget(int milliseconds);
    void setConfidenceThreshold(double threshold);

signals:
    void completionReady(const CodeCompletion& completion);
    void partialCompletion(const QString& token);
    void refactoringComplete(const QVector<CodeChange>& changes);
    void codeGenerated(const QString& code);
    void suggestionsReady(const QVector<QString>& suggestions);
    void errorOccurred(const QString& error);

private:
    QString buildCursorPrompt(const CursorContext& ctx);
    QVector<CodeChange> parseCursorResponse(const QString& response);
    QString selectOptimalModel(const CursorContext& ctx);
    int calculateComplexity(const CursorContext& ctx);
    int estimateContext(const CursorContext& ctx);

    AIIntegrationHub* m_hub{nullptr};
    AgenticExecutor* m_executor{nullptr};
    MultiModalModelRouter* m_router{nullptr};

    QString m_currentCompletion;
    bool m_completionActive{false};
    int m_latencyTarget{50};  // milliseconds
    double m_confidenceThreshold{0.7};
};

} // namespace RawrXD
