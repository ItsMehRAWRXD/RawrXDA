#include "ServerInfoDialog.h"
#include "settings.h"
#include "../../include/api_server.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDateTime>
#include <QScreen>
#include <QApplication>
#include <iomanip>
#include <sstream>

ServerInfoDialog::ServerInfoDialog(APIServer* apiServer, QWidget* parent)
    : QDialog(parent), m_apiServer(apiServer), m_refreshTimer(nullptr), m_lastMetricsData(nullptr) {
    setWindowTitle("API Server Information");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
    setMinimumWidth(500);
    setMinimumHeight(350);

    setupUI();
    setupConnections();

    // Start auto-refresh timer (2 seconds)
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &ServerInfoDialog::onRefreshTimer);
    m_refreshTimer->start(2000);

    // Initial update
    updateMetrics();

    // Center on screen (Qt6 compatible)
    if (QScreen* screen = QApplication::primaryScreen()) {
        move(screen->geometry().center() - frameGeometry().center());
    }
}

ServerInfoDialog::~ServerInfoDialog() {
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
    if (m_lastMetricsData) {
        delete reinterpret_cast<ServerMetrics*>(m_lastMetricsData);
    }
}

void ServerInfoDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Server Status Group
    QGroupBox* statusGroup = new QGroupBox("Server Status", this);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    m_statusLabel = new QLabel("Status: Checking...", this);
    m_portLabel = new QLabel("Port: N/A", this);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addWidget(m_portLabel);
    mainLayout->addWidget(statusGroup);

    // Request Statistics Group
    QGroupBox* statsGroup = new QGroupBox("Request Statistics", this);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsGroup);

    m_totalRequestsLabel = new QLabel("Total Requests: 0", this);
    m_successfulLabel = new QLabel("Successful: 0", this);
    m_failedLabel = new QLabel("Failed: 0", this);
    m_activeConnectionsLabel = new QLabel("Active Connections: 0", this);
    m_successRateLabel = new QLabel("Success Rate: 0.0%", this);

    statsLayout->addWidget(m_totalRequestsLabel);
    statsLayout->addWidget(m_successfulLabel);
    statsLayout->addWidget(m_failedLabel);
    statsLayout->addWidget(m_activeConnectionsLabel);
    statsLayout->addWidget(m_successRateLabel);
    mainLayout->addWidget(statsGroup);

    // Uptime Group
    QGroupBox* uptimeGroup = new QGroupBox("Server Uptime", this);
    QVBoxLayout* uptimeLayout = new QVBoxLayout(uptimeGroup);
    m_uptimeLabel = new QLabel("Uptime: N/A", this);
    uptimeLayout->addWidget(m_uptimeLabel);
    mainLayout->addWidget(uptimeGroup);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* refreshButton = new QPushButton("Refresh Now", this);
    connect(refreshButton, &QPushButton::clicked, this, &ServerInfoDialog::updateMetrics);
    buttonLayout->addStretch();
    buttonLayout->addWidget(refreshButton);
    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();
    setLayout(mainLayout);
}

void ServerInfoDialog::setupConnections() {
    // Connections handled in constructor via Timer
}

void ServerInfoDialog::onRefreshTimer() {
    updateMetrics();
}

void ServerInfoDialog::updateMetrics() {
    if (!m_apiServer) {
        m_statusLabel->setText("Status: <span style='color: red;'>Not Initialized</span>");
        m_portLabel->setText("Port: N/A");
        return;
    }

    // Update status
    bool isRunning = m_apiServer->IsRunning();
    QString statusColor = isRunning ? "green" : "red";
    QString statusText = isRunning ? "Running" : "Stopped";
    m_statusLabel->setText(QString("Status: <span style='color: %1;'>%2</span>").arg(statusColor, statusText));

    // Update port
    uint16_t port = m_apiServer->GetPort();
    m_portLabel->setText(QString("Port: %1").arg(port));

    // Get and display metrics
    auto metrics = m_apiServer->GetMetrics();
    m_totalRequestsLabel->setText(QString("Total Requests: %1").arg(metrics.total_requests));
    m_successfulLabel->setText(QString("Successful: %1").arg(metrics.successful_requests));
    m_failedLabel->setText(QString("Failed: %1").arg(metrics.failed_requests));
    m_activeConnectionsLabel->setText(QString("Active Connections: %1").arg(metrics.active_connections));

    // Calculate and display success rate
    if (metrics.total_requests > 0) {
        double successRate = (100.0 * metrics.successful_requests) / metrics.total_requests;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << successRate;
        m_successRateLabel->setText(QString("Success Rate: %1%").arg(QString::fromStdString(ss.str())));
    } else {
        m_successRateLabel->setText("Success Rate: N/A");
    }

    m_uptimeLabel->setText("Uptime: (auto-tracked by server)");
    // Store metrics pointer (convert to opaque void* to avoid circular includes)
    m_lastMetricsData = reinterpret_cast<void*>(new ServerMetrics(metrics));
}

void ServerInfoDialog::refreshMetricsDisplay() {
    updateMetrics();
}
