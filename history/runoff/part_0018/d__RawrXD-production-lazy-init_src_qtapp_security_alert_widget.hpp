#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QString>

/**
 * @brief SecurityAlertWidget - Qt/C++ reference implementation
 * 
 * Displays security issues detected by autonomous systems.
 * Features:
 * - Color-coded severity levels (Critical=Red, High=Orange, Medium=Yellow, Low=Green)
 * - ListView with columns: Issue, Severity, Type, Location
 * - Fix/Ignore/Details buttons
 * - Integration with SecurityIssue MASM structures
 * 
 * This is the Qt reference version—will be ported to MASM Win32 later.
 */
class SecurityAlertWidget : public QWidget {
    Q_OBJECT

public:
    // Security issue severity levels
    enum Severity {
        SeverityCritical = 0,  // Exploitable vulns, auth bypass
        SeverityHigh = 1,      // Major issues, DoS risk
        SeverityMedium = 2,    // Moderate issues, data leaks
        SeverityLow = 3        // Minor warnings, best practices
    };

    // Issue categories
    enum IssueType {
        TypeInputValidation = 0,
        TypeAuthorizationBypass = 1,
        TypeSQLInjection = 2,
        TypeXSS = 3,
        TypeCryptoWeakness = 4,
        TypeDependencyVulnerability = 5,
        TypeMemorySafety = 6,
        TypeOther = 7
    };

    struct SecurityIssue {
        QString id;                    // Unique identifier
        QString title;                 // e.g., "SQL Injection in user_query()"
        QString description;           // Detailed explanation
        Severity severity;             // Critical/High/Medium/Low
        IssueType type;               // Category
        QString location;              // File:Line where issue found
        QString fixSuggestion;        // How to fix it
        bool isFixed = false;
    };

    explicit SecurityAlertWidget(QWidget* parent = nullptr);
    ~SecurityAlertWidget() override;

    // Issue management
    void addIssue(const SecurityIssue& issue);
    void removeIssue(const QString& issueId);
    void clearAllIssues();
    void markAsFixed(const QString& issueId);

    // Query
    int issueCount() const;
    int criticalCount() const;
    int highCount() const;
    QList<SecurityIssue> getAllIssues() const;

    // Severity color mapping
    static QColor getSeverityColor(Severity severity);
    static QString getSeverityString(Severity severity);
    static QString getTypeString(IssueType type);

signals:
    void issueSelected(const QString& issueId);
    void fixRequested(const QString& issueId);
    void ignoreRequested(const QString& issueId);
    void issueFixed(const QString& issueId);
    void issuesCleared();

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onFixClicked();
    void onIgnoreClicked();
    void onDetailsClicked();
    void updateMetrics();

private:
    void setupUI();
    void refreshIssueList();
    QString formatIssueDisplay(const SecurityIssue& issue) const;

    // UI Components
    QListWidget* m_issueList;
    QTextEdit* m_statsLabel;              // "5 issues: 1 Critical, 2 High, 2 Medium"
    QProgressBar* m_severityBar;       // Visual severity indicator
    QPushButton* m_fixButton;
    QPushButton* m_ignoreButton;
    QPushButton* m_detailsButton;
    QPushButton* m_clearAllButton;
    QTextEdit* m_detailsText;           // Details panel

    // Data
    QList<SecurityIssue> m_issues;
    QString m_selectedIssueId;
};
