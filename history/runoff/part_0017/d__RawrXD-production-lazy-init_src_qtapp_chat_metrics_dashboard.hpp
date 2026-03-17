#ifndef CHAT_METRICS_DASHBOARD_HPP
#define CHAT_METRICS_DASHBOARD_HPP

/**
 * @file chat_metrics_dashboard.hpp
 * @brief Real-time dashboard for AI Chat metrics and monitoring
 * 
 * Provides:
 * - Real-time metrics visualization
 * - Performance profiling integration
 * - User interaction analytics display
 * - Health status indicators
 */

#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QTableWidget>
#include <QGroupBox>
#include <QPushButton>
#include <QJsonObject>
#include <QDateTime>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QFile>
#include <QJsonDocument>
#include <QIODevice>
#include <memory>

#include "chat_production_infrastructure.hpp"

// Forward declarations (no longer needed but kept for compatibility)
namespace RawrXD {
namespace Chat {
    class ChatProductionInfrastructure;
}
}

namespace RawrXD {
namespace Dashboard {

/**
 * @class MetricCard
 * @brief Individual metric display card
 */
class MetricCard : public QFrame {
    Q_OBJECT
    
public:
    explicit MetricCard(const QString& title, const QString& unit = "", QWidget* parent = nullptr)
        : QFrame(parent), m_unit(unit) {
        setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
        setStyleSheet(R"(
            QFrame { 
                background-color: #2d2d30; 
                border-radius: 8px; 
                padding: 10px;
                border: 1px solid #3e3e42;
            }
        )");
        
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 8, 12, 8);
        
        m_titleLabel = new QLabel(title);
        m_titleLabel->setStyleSheet("color: #858585; font-size: 11px;");
        
        m_valueLabel = new QLabel("--");
        m_valueLabel->setStyleSheet("color: #4ec9b0; font-size: 24px; font-weight: bold;");
        
        m_changeLabel = new QLabel("");
        m_changeLabel->setStyleSheet("color: #858585; font-size: 10px;");
        
        layout->addWidget(m_titleLabel);
        layout->addWidget(m_valueLabel);
        layout->addWidget(m_changeLabel);
    }
    
    void setValue(double value) {
        QString valueStr = QString::number(value, 'f', m_decimals);
        if (!m_unit.isEmpty()) {
            valueStr += " " + m_unit;
        }
        m_valueLabel->setText(valueStr);
        
        // Calculate change
        if (m_previousValue > 0) {
            double change = ((value - m_previousValue) / m_previousValue) * 100;
            QString changeStr = QString("%1%2%")
                .arg(change >= 0 ? "+" : "")
                .arg(change, 0, 'f', 1);
            m_changeLabel->setText(changeStr);
            m_changeLabel->setStyleSheet(
                QString("color: %1; font-size: 10px;")
                    .arg(change >= 0 ? "#22863a" : "#d73a49")
            );
        }
        
        m_previousValue = value;
    }
    
    void setDecimals(int decimals) { m_decimals = decimals; }
    
    void setValueColor(const QString& color) {
        m_valueLabel->setStyleSheet(QString("color: %1; font-size: 24px; font-weight: bold;").arg(color));
    }
    
private:
    QLabel* m_titleLabel;
    QLabel* m_valueLabel;
    QLabel* m_changeLabel;
    QString m_unit;
    double m_previousValue = 0;
    int m_decimals = 0;
};

/**
 * @class HealthIndicator
 * @brief Visual health status indicator
 */
class HealthIndicator : public QWidget {
    Q_OBJECT
    
public:
    enum Status { Healthy, Degraded, Unhealthy, Unknown };
    
    explicit HealthIndicator(const QString& componentName, QWidget* parent = nullptr)
        : QWidget(parent), m_componentName(componentName) {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        
        m_statusDot = new QLabel("●");
        m_statusDot->setStyleSheet("color: #858585; font-size: 12px;");
        
        m_nameLabel = new QLabel(componentName);
        m_nameLabel->setStyleSheet("color: #cccccc; font-size: 11px;");
        
        m_statusLabel = new QLabel("Unknown");
        m_statusLabel->setStyleSheet("color: #858585; font-size: 10px;");
        
        layout->addWidget(m_statusDot);
        layout->addWidget(m_nameLabel);
        layout->addStretch();
        layout->addWidget(m_statusLabel);
    }
    
    void setStatus(Status status, const QString& message = "") {
        QString color;
        QString statusText;
        
        switch (status) {
            case Healthy:
                color = "#22863a";
                statusText = "Healthy";
                break;
            case Degraded:
                color = "#f9c513";
                statusText = "Degraded";
                break;
            case Unhealthy:
                color = "#d73a49";
                statusText = "Unhealthy";
                break;
            default:
                color = "#858585";
                statusText = "Unknown";
        }
        
        m_statusDot->setStyleSheet(QString("color: %1; font-size: 12px;").arg(color));
        m_statusLabel->setText(message.isEmpty() ? statusText : message);
    }
    
private:
    QString m_componentName;
    QLabel* m_statusDot;
    QLabel* m_nameLabel;
    QLabel* m_statusLabel;
};

/**
 * @class ChatMetricsDashboard
 * @brief Main dashboard widget for chat metrics
 */
class ChatMetricsDashboard : public QWidget {
    Q_OBJECT
    
public:
    explicit ChatMetricsDashboard(QWidget* parent = nullptr)
        : QWidget(parent) {
        setupUI();
        
        // Start refresh timer
        m_refreshTimer = new QTimer(this);
        connect(m_refreshTimer, &QTimer::timeout, this, &ChatMetricsDashboard::refresh);
        m_refreshTimer->start(2000);  // Refresh every 2 seconds
    }
    
    void setInfrastructure(RawrXD::Chat::ChatProductionInfrastructure* infra) {
        m_infrastructure = infra;
        refresh();
    }
    
    void updateMetrics(const QJsonObject& metrics) {
        if (metrics.isEmpty()) return;
        
        // Update metric cards from JSON
        if (metrics.contains("totalMessages")) {
            m_totalMessagesCard->setValue(metrics["totalMessages"].toInt());
        }
        if (metrics.contains("avgLatency")) {
            m_avgLatencyCard->setValue(metrics["avgLatency"].toDouble());
        }
    }
    
public slots:
    void refresh() {
        if (!m_infrastructure) {
            emit updateRequested();
            return;
        }
        
        updateMetricsInternal();
        updateHealth();
        updateLatencyChart();
        updateAnalytics();
    }
    
private:
    void setupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(16);
        
        // Title
        QLabel* title = new QLabel("AI Chat Metrics Dashboard");
        title->setStyleSheet("color: #4ec9b0; font-size: 18px; font-weight: bold;");
        mainLayout->addWidget(title);
        
        // Metrics cards row
        QHBoxLayout* cardsLayout = new QHBoxLayout();
        cardsLayout->setSpacing(12);
        
        m_totalMessagesCard = new MetricCard("Total Messages");
        m_avgLatencyCard = new MetricCard("Avg Latency", "ms");
        m_avgLatencyCard->setDecimals(1);
        m_cacheHitRateCard = new MetricCard("Cache Hit Rate", "%");
        m_cacheHitRateCard->setDecimals(1);
        m_errorRateCard = new MetricCard("Error Rate", "%");
        m_errorRateCard->setDecimals(2);
        
        cardsLayout->addWidget(m_totalMessagesCard);
        cardsLayout->addWidget(m_avgLatencyCard);
        cardsLayout->addWidget(m_cacheHitRateCard);
        cardsLayout->addWidget(m_errorRateCard);
        
        mainLayout->addLayout(cardsLayout);
        
        // Health status section
        QGroupBox* healthGroup = new QGroupBox("System Health");
        healthGroup->setStyleSheet(R"(
            QGroupBox { 
                color: #cccccc; 
                border: 1px solid #3e3e42; 
                border-radius: 4px; 
                margin-top: 12px;
                padding-top: 8px;
            }
            QGroupBox::title { 
                subcontrol-origin: margin; 
                left: 10px; 
                padding: 0 5px;
            }
        )");
        
        QVBoxLayout* healthLayout = new QVBoxLayout(healthGroup);
        
        m_circuitBreakerHealth = new HealthIndicator("Circuit Breaker");
        m_responseCacheHealth = new HealthIndicator("Response Cache");
        m_inferenceEngineHealth = new HealthIndicator("Inference Engine");
        
        healthLayout->addWidget(m_circuitBreakerHealth);
        healthLayout->addWidget(m_responseCacheHealth);
        healthLayout->addWidget(m_inferenceEngineHealth);
        
        mainLayout->addWidget(healthGroup);
        
        // Latency chart placeholder
        QGroupBox* chartGroup = new QGroupBox("Latency Over Time");
        chartGroup->setStyleSheet(R"(
            QGroupBox { 
                color: #cccccc; 
                border: 1px solid #3e3e42; 
                border-radius: 4px; 
                margin-top: 12px;
                padding-top: 8px;
            }
            QGroupBox::title { 
                subcontrol-origin: margin; 
                left: 10px; 
                padding: 0 5px;
            }
        )");
        
        QVBoxLayout* chartLayout = new QVBoxLayout(chartGroup);
        m_latencyChartLabel = new QLabel("Latency chart will be displayed here");
        m_latencyChartLabel->setStyleSheet("color: #858585;");
        m_latencyChartLabel->setAlignment(Qt::AlignCenter);
        m_latencyChartLabel->setMinimumHeight(150);
        chartLayout->addWidget(m_latencyChartLabel);
        
        mainLayout->addWidget(chartGroup);
        
        // Analytics section
        QGroupBox* analyticsGroup = new QGroupBox("Session Analytics");
        analyticsGroup->setStyleSheet(R"(
            QGroupBox { 
                color: #cccccc; 
                border: 1px solid #3e3e42; 
                border-radius: 4px; 
                margin-top: 12px;
                padding-top: 8px;
            }
            QGroupBox::title { 
                subcontrol-origin: margin; 
                left: 10px; 
                padding: 0 5px;
            }
        )");
        
        QGridLayout* analyticsLayout = new QGridLayout(analyticsGroup);
        
        m_sessionDurationLabel = new QLabel("Duration: --");
        m_sessionDurationLabel->setStyleSheet("color: #cccccc;");
        m_codeApprovalsLabel = new QLabel("Code Approvals: --");
        m_codeApprovalsLabel->setStyleSheet("color: #22863a;");
        m_codeRejectionsLabel = new QLabel("Code Rejections: --");
        m_codeRejectionsLabel->setStyleSheet("color: #d73a49;");
        m_modelsUsedLabel = new QLabel("Models Used: --");
        m_modelsUsedLabel->setStyleSheet("color: #cccccc;");
        
        analyticsLayout->addWidget(m_sessionDurationLabel, 0, 0);
        analyticsLayout->addWidget(m_codeApprovalsLabel, 0, 1);
        analyticsLayout->addWidget(m_codeRejectionsLabel, 1, 0);
        analyticsLayout->addWidget(m_modelsUsedLabel, 1, 1);
        
        mainLayout->addWidget(analyticsGroup);
        
        // Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        
        QPushButton* refreshBtn = new QPushButton("Refresh Now");
        refreshBtn->setStyleSheet(R"(
            QPushButton { 
                background-color: #0e639c; 
                color: white; 
                border: none; 
                padding: 8px 16px; 
                border-radius: 4px;
            }
            QPushButton:hover { background-color: #1177bb; }
        )");
        connect(refreshBtn, &QPushButton::clicked, this, &ChatMetricsDashboard::refresh);
        
        QPushButton* exportBtn = new QPushButton("Export Report");
        exportBtn->setStyleSheet(R"(
            QPushButton { 
                background-color: #3e3e42; 
                color: white; 
                border: none; 
                padding: 8px 16px; 
                border-radius: 4px;
            }
            QPushButton:hover { background-color: #4e4e52; }
        )");
        connect(exportBtn, &QPushButton::clicked, this, &ChatMetricsDashboard::exportReport);
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(refreshBtn);
        buttonLayout->addWidget(exportBtn);
        
        mainLayout->addLayout(buttonLayout);
        mainLayout->addStretch();
        
        setStyleSheet("background-color: #1e1e1e;");
    }
    
    void updateMetrics() {
        if (!m_infrastructure) return;
        
        QJsonObject metrics = m_infrastructure->metrics()->getFullMetricsReport();
        QJsonObject messages = metrics["messages"].toObject();
        QJsonObject latency = metrics["latency"].toObject();
        
        m_totalMessagesCard->setValue(messages["total"].toDouble());
        m_avgLatencyCard->setValue(latency["avgMs"].toDouble());
        
        // Calculate error rate
        double total = messages["total"].toDouble();
        double errors = messages["errors"].toDouble();
        double errorRate = total > 0 ? (errors / total) * 100 : 0;
        m_errorRateCard->setValue(errorRate);
        
        // Color error rate based on severity
        if (errorRate > 10) {
            m_errorRateCard->setValueColor("#d73a49");
        } else if (errorRate > 5) {
            m_errorRateCard->setValueColor("#f9c513");
        } else {
            m_errorRateCard->setValueColor("#22863a");
        }
        
        // Cache stats
        QJsonObject cacheStats = m_infrastructure->responseCache()->getStats();
        int hits = cacheStats["totalHits"].toInt();
        int entries = cacheStats["entries"].toInt();
        double hitRate = entries > 0 ? (static_cast<double>(hits) / (hits + entries)) * 100 : 0;
        m_cacheHitRateCard->setValue(hitRate);
    }
    
    void updateHealth() {
        if (!m_infrastructure) return;
        
        QJsonObject health = m_infrastructure->healthMonitor()->getFullHealthReport();
        QJsonObject components = health["components"].toObject();
        
        auto getStatus = [](const QString& statusStr) -> HealthIndicator::Status {
            if (statusStr == "healthy") return HealthIndicator::Healthy;
            if (statusStr == "degraded") return HealthIndicator::Degraded;
            if (statusStr == "unhealthy") return HealthIndicator::Unhealthy;
            return HealthIndicator::Unknown;
        };
        
        if (components.contains("CircuitBreaker")) {
            QJsonObject cb = components["CircuitBreaker"].toObject();
            m_circuitBreakerHealth->setStatus(
                getStatus(cb["status"].toString()),
                cb["message"].toString()
            );
        }
        
        if (components.contains("ResponseCache")) {
            QJsonObject rc = components["ResponseCache"].toObject();
            m_responseCacheHealth->setStatus(
                getStatus(rc["status"].toString()),
                rc["message"].toString()
            );
        }
        
        // Inference engine health based on circuit breaker state
        auto cbState = m_infrastructure->circuitBreaker()->state();
        HealthIndicator::Status ieStatus = HealthIndicator::Healthy;
        QString ieMessage = "Operational";
        
        if (cbState == RawrXD::Chat::CircuitBreaker::OPEN) {
            ieStatus = HealthIndicator::Unhealthy;
            ieMessage = "Circuit open - service unavailable";
        } else if (cbState == RawrXD::Chat::CircuitBreaker::HALF_OPEN) {
            ieStatus = HealthIndicator::Degraded;
            ieMessage = "Recovering...";
        }
        m_inferenceEngineHealth->setStatus(ieStatus, ieMessage);
    }
    
    void updateLatencyChart() {
        if (!m_infrastructure) return;
        
        QJsonObject metrics = m_infrastructure->metrics()->getFullMetricsReport();
        QJsonObject latency = metrics["latency"].toObject();
        
        QString chartText = QString(
            "P50: %1ms | P95: %2ms | P99: %3ms | Max: %4ms"
        ).arg(latency["p50Ms"].toInt())
         .arg(latency["p95Ms"].toInt())
         .arg(latency["p99Ms"].toInt())
         .arg(latency["maxMs"].toInt());
        
        m_latencyChartLabel->setText(chartText);
    }
    
    void updateAnalytics() {
        if (!m_infrastructure) return;
        
        QJsonObject analytics = m_infrastructure->analytics()->getSessionReport();
        
        double duration = analytics["durationMinutes"].toDouble();
        m_sessionDurationLabel->setText(QString("Duration: %1 min").arg(duration, 0, 'f', 1));
        
        m_codeApprovalsLabel->setText(
            QString("Code Approvals: %1").arg(analytics["codeApprovals"].toInt()));
        m_codeRejectionsLabel->setText(
            QString("Code Rejections: %1").arg(analytics["codeRejections"].toInt()));
        
        QJsonArray models = analytics["queriedModels"].toArray();
        m_modelsUsedLabel->setText(
            QString("Models Used: %1").arg(models.size()));
    }
    
signals:
    void reportExported(const QString& path);
    void updateRequested();
    
private slots:
    void exportReport() {
        if (!m_infrastructure) return;
        
        QJsonObject fullReport = m_infrastructure->getFullStatusReport();
        
        QString fileName = QString("chat_metrics_%1.json")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(fullReport).toJson(QJsonDocument::Indented));
            file.close();
            emit reportExported(fileName);
        }
    }
    
    void updateMetricsInternal() {
        updateMetrics();
    }
    
private:
    RawrXD::Chat::ChatProductionInfrastructure* m_infrastructure = nullptr;
    QTimer* m_refreshTimer;
    
    // Metric cards
    MetricCard* m_totalMessagesCard;
    MetricCard* m_avgLatencyCard;
    MetricCard* m_cacheHitRateCard;
    MetricCard* m_errorRateCard;
    
    // Health indicators
    HealthIndicator* m_circuitBreakerHealth;
    HealthIndicator* m_responseCacheHealth;
    HealthIndicator* m_inferenceEngineHealth;
    
    // Chart
    QLabel* m_latencyChartLabel;
    
    // Analytics labels
    QLabel* m_sessionDurationLabel;
    QLabel* m_codeApprovalsLabel;
    QLabel* m_codeRejectionsLabel;
    QLabel* m_modelsUsedLabel;
};

} // namespace Dashboard
} // namespace RawrXD

#endif // CHAT_METRICS_DASHBOARD_HPP
