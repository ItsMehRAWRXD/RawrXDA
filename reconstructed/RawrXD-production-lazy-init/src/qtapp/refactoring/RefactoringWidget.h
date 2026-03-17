#ifndef REFACTORING_WIDGET_H
#define REFACTORING_WIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include "AdvancedRefactoring.h"

/**
 * @brief Qt Widget for code refactoring UI
 * 
 * Provides a visual interface for all refactoring operations.
 * Displays results, progress, and operation history.
 */
class RefactoringWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RefactoringWidget(QWidget* parent = nullptr);
    ~RefactoringWidget();

    void setCurrentFile(const QString& filePath);
    void setSelection(int startLine, int endLine, int startCol, int endCol);

private slots:
    void onRefactoringStarted(const AdvancedRefactoring::RefactoringOperation& op);
    void onRefactoringProgress(int progress, const QString& message);
    void onRefactoringCompleted(const AdvancedRefactoring::RefactoringResult& result);
    void onRefactoringFailed(const QString& error);
    void onExecuteRefactoring();
    void onHistoryItemSelected(int row);
    void onUndo();
    void onRedo();
    void onClearHistory();

private:
    void setupUI();
    void connectSignals();
    void updateRefactoringList();
    void displayResult(const AdvancedRefactoring::RefactoringResult& result);
    void displayOperation(const AdvancedRefactoring::RefactoringOperation& op);

    // UI Components
    QComboBox* m_refactoringType;
    QTextEdit* m_parameters;
    QPushButton* m_executeButton;
    QPushButton* m_undoButton;
    QPushButton* m_redoButton;
    QPushButton* m_clearButton;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTableWidget* m_historyTable;
    QTextEdit* m_resultDisplay;

    // Engine
    AdvancedRefactoring* m_engine;
    QString m_currentFile;
    AdvancedRefactoring::CodeRange m_selection;
};

#endif // REFACTORING_WIDGET_H
