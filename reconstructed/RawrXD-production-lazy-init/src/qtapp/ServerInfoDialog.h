#pragma once

#include <QDialog>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <memory>
#include <cstdint>

// Forward declarations
class APIServer;
struct ServerMetrics;

/**
 * @brief ServerInfoDialog - Display real-time API server metrics and status
 * 
 * Shows:
 * - Server status (Running/Stopped)
 * - Listen port and instance info
 * - Total requests, successful, failed, active connections
 * - Success rate percentage
 * - Auto-refreshes every 2 seconds
 */
class ServerInfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit ServerInfoDialog(APIServer* apiServer = nullptr, QWidget* parent = nullptr);
    ~ServerInfoDialog();

private slots:
    void updateMetrics();
    void onRefreshTimer();

private:
    void setupUI();
    void setupConnections();
    void refreshMetricsDisplay();

    APIServer* m_apiServer;
    QTimer* m_refreshTimer;

    // UI Components
    QLabel* m_statusLabel;
    QLabel* m_portLabel;
    QLabel* m_totalRequestsLabel;
    QLabel* m_successfulLabel;
    QLabel* m_failedLabel;
    QLabel* m_activeConnectionsLabel;
    QLabel* m_successRateLabel;
    QLabel* m_uptimeLabel;

    // Cache for comparison (use void* to avoid including api_server.h)
    void* m_lastMetricsData;
};
