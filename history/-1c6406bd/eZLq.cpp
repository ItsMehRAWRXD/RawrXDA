#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QChartView>
#include <QSplineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>

#include "enterprise/EnterpriseAgentBridge.hpp"
#include "enterprise/EnterpriseMetrics.hpp"
#include "enterprise/AICodeIntelligence.hpp"
#include "enterprise/MLWorkflowOptimizer.hpp"
#include "enterprise/EnterpriseSecurity.hpp"
#include "enterprise/ToolScheduler.hpp"

class EnterpriseDashboard : public QMainWindow {
    Q_OBJECT
    
public:
    explicit EnterpriseDashboard(QWidget *parent = nullptr);
    ~EnterpriseDashboard();
    
private slots:
    void onMissionStarted(const QString& missionId);
    void onMissionCompleted(const QString& missionId, const QVariantMap& results);
    void onMissionFailed(const QString& missionId, const QString& error);
    void onMetricsUpdated(const QJsonObject& metrics);
    void onSecurityAlert(const QString& alertType, const QJsonObject& details);
    void onAnalysisComplete(const QString& filePath, const QList<CodeInsight>& insights);
    
private:
    void setupUI();
    void setupConnections();
    void initializeEnterpriseComponents();
    void updateRealTimeMetrics();
    void updateMissionStatus();
    void updateSecurityStatus();
    void updatePerformanceCharts();
    
    // UI Components
    QTabWidget* m_tabWidget;
    
    // Dashboard tabs
    QWidget* m_dashboardTab;
    QWidget* m_missionsTab;
    QWidget* m_metricsTab;
    QWidget* m_securityTab;
    QWidget* m_analysisTab;
    QWidget* m_optimizationTab;
    
    // Enterprise components
    EnterpriseAgentBridge* m_agentBridge;
    EnterpriseMetricsCollector* m_metricsCollector;
    AICodeIntelligence* m_aiIntelligence;
    MLWorkflowOptimizer* m_workflowOptimizer;
    EnterpriseSecurity* m_security;
    ToolScheduler* m_scheduler;
    
    // Real-time data
    QTimer* m_metricsTimer;
    QTimer* m_statusTimer;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("AgenticToolExecutor Enterprise");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("RawrXD Corporation");
    
    // Create and show enterprise dashboard
    EnterpriseDashboard dashboard;
    dashboard.show();
    
    return app.exec();
}