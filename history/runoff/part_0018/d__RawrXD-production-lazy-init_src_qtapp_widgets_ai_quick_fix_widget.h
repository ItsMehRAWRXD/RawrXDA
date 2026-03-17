#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QListWidget>
#include <QJsonObject>
#include <QJsonArray>

class ModelRouterAdapter;

class AIQuickFixWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AIQuickFixWidget(QWidget* parent = nullptr);
    ~AIQuickFixWidget();

private slots:
    void applyFix();
    void analyzeCode();
    void onGenerationComplete(const QString& result, int tokens_used, double latency_ms);
    void onGenerationError(const QString& error);

public slots:
    void setDiagnostics(const QJsonArray& diagnostics);
    void setCurrentCode(const QString& code, const QString& filePath);

private:
    void setupModelRouter();
    void extractIssuesFromDiagnostics();
    
    QListWidget* m_issueList;
    QTextEdit* m_solutionText;
    QPushButton* m_applyButton;
    QPushButton* m_analyzeButton;
    
    // AI Integration
    ModelRouterAdapter* m_modelRouter;
    bool m_isGenerating;
    QString m_currentCode;
    QString m_currentFilePath;
    QJsonArray m_currentDiagnostics;
    QString m_generatedFix;
};
