/**
 * @file realtime_refactoring_widget.h
 * @brief Real-time Code Refactoring Widget with Live Suggestions
 * 
 * Provides real-time code improvement with:
 * - Live refactoring suggestions as you type
 * - One-click apply for safe transformations
 * - Preview diff before applying
 * - Undo/redo support
 * - Batch refactoring for multiple files
 * - AI-powered suggestion engine
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include "refactoring_engine.h"

class QListWidget;
class QTextEdit;
class QPushButton;
class QLabel;
class QSplitter;
class QCheckBox;
class CodeAnalyzer;
class AutomaticRefactoringEngine;

/**
 * @class RealtimeRefactoringWidget
 * @brief Live refactoring suggestions with one-click apply
 */
class RealtimeRefactoringWidget : public QWidget {
    Q_OBJECT

public:
    explicit RealtimeRefactoringWidget(QWidget* parent = nullptr);
    ~RealtimeRefactoringWidget() override;

    void initialize();
    void analyzeCode(const QString& code, const QString& filePath);

public slots:
    void onCodeChanged(const QString& code);
    void onSuggestionSelected(int index);
    void applySelectedSuggestion();
    void applyAllSuggestions();
    void previewSuggestion();
    void undoLastRefactoring();
    void configureRules();

signals:
    void suggestionApplied(const RefactoringProposal& proposal);
    void codeRefactored(const QString& newCode);
    void refactoringError(const QString& error);

private:
    void setupUI();
    void updateSuggestionList(const QVector<RefactoringProposal>& proposals);
    void showPreview(const RefactoringProposal& proposal);
    
    CodeAnalyzer* m_analyzer = nullptr;
    AutomaticRefactoringEngine* m_engine = nullptr;
    
    QListWidget* m_suggestionList = nullptr;
    QTextEdit* m_previewPane = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_applyAllBtn = nullptr;
    QPushButton* m_undoBtn = nullptr;
    QLabel* m_statsLabel = nullptr;
    QCheckBox* m_autoApplySafe = nullptr;
    
    QVector<RefactoringProposal> m_proposals;
    QVector<QString> m_undoStack;
    QString m_currentFilePath;
};
