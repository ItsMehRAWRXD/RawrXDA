/**
 * @file problems_panel.hpp
 * @brief Problems panel widget for IDE diagnostics and build errors
 * @author RawrXD Team
 * @date 2026-01-08
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMetaType>

/**
 * @struct DiagnosticIssue
 * @brief Single diagnostic issue (error, warning, info)
 */
struct DiagnosticIssue {
    enum Severity { Error, Warning, Info } severity;
    QString file;
    int line = -1;
    int column = -1;
    QString message;
    QString source;  // "MASM", "LSP", "Build", etc.
    QString code;    // e.g., "ML2005" for MASM
    QString fix;     // Suggested fix text
    
    bool operator==(const DiagnosticIssue& other) const {
        return file == other.file && line == other.line && 
               column == other.column && message == other.message;
    }
};

Q_DECLARE_METATYPE(DiagnosticIssue)

/**
 * @class ProblemsPanel
 * @brief IDE Problems panel for displaying and filtering diagnostics
 * 
 * Displays compilation/MASM build errors, LSP diagnostics, and runtime issues
 * in a filterable, sortable tree view. Supports click-to-fix integration with AI.
 */
class ProblemsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget* parent = nullptr);
    ~ProblemsPanel();

    /**
     * @brief Add a diagnostic issue to the panel
     */
    void addIssue(const DiagnosticIssue& issue);

    /**
     * @brief Clear all issues
     */
    void clearIssues();

    /**
     * @brief Set issues from JSON array (LSP/diagnostic format)
     */
    void setIssuesFromJSON(const QString& filePath, const QJsonArray& diagnostics);

    /**
     * @brief Parse MASM build output and extract errors
     * @param output ML64.exe or ml.exe output text
     * @return Vector of parsed issues
     */
    static QVector<DiagnosticIssue> parseMASMOutput(const QString& output);

    /**
     * @brief Get count of issues by severity
     */
    int errorCount() const { return m_errorCount; }
    int warningCount() const { return m_warningCount; }
    int infoCount() const { return m_infoCount; }

signals:
    /**
     * @brief Emitted when user clicks on an issue
     */
    void issueSelected(const DiagnosticIssue& issue);

    /**
     * @brief Emitted when user requests a fix via AI
     */
    void fixRequested(const DiagnosticIssue& issue);

    /**
     * @brief Emitted when user wants to navigate to issue in editor
     */
    void navigateToIssue(const QString& file, int line, int column);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterTextChanged(const QString& text);
    void onSeverityFilterChanged(int index);
    void onSourceFilterChanged(int index);

private:
    void setupUI();
    void updateTree();
    void filterAndSort();
    QString severityIcon(DiagnosticIssue::Severity sev) const;
    QString severityColor(DiagnosticIssue::Severity sev) const;

    // UI Components
    QLineEdit* m_filterInput;
    QComboBox* m_severityFilter;
    QComboBox* m_sourceFilter;
    QLabel* m_summaryLabel;
    QTreeWidget* m_issueTree;

    // Data
    QVector<DiagnosticIssue> m_issues;
    int m_errorCount = 0;
    int m_warningCount = 0;
    int m_infoCount = 0;
};
