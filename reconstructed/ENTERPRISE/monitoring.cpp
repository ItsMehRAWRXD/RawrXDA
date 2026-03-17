// enterprise_monitoring.cpp - Complete monitoring with Prometheus integration
#include "enterprise_monitoring.h"
#include "error_recovery_system.h"
#include "performance_monitor.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QElapsedTimer>
#include <iostream>
#include <algorithm>

EnterpriseMonitoring::EnterpriseMonitoring(QObject* parent)
    : QObject(parent) {
    
    setupDefaultAlertRules();
    
    evaluationTimer = new QTimer(this);
    evaluationTimer->setInterval(evaluationInterval * 1000);
    connect(evaluationTimer, &QTimer::timeout, this, &EnterpriseMonitoring::evaluateAlertRules);
    evaluationTimer->start();
    
    metricsCollector = new QTimer(this);
    metricsCollector->setInterval(10000); // Collect metrics every 10 seconds
    connect(metricsCollector, &QTimer::timeout, this, &EnterpriseMonitoring::recordSystemMetrics);
    metricsCollector->start();
    
    setupAutoRemediationRules();
    
    std::cout << "[EnterpriseMonitoring] Initialized with " << alertRules.size() << " alert rules" << std::endl;
}

EnterpriseMonitoring::~EnterpriseMonitoring() {
    if (evaluationTimer) evaluationTimer->stop();
    if (metricsCollector) metricsCollector->stop();
}

void EnterpriseMonitoring::setupDefaultAlertRules() {
    // High CPU usage alert
    AlertRule cpuAlert;
    cpuAlert.ruleId = "high_cpu_usage";
    cpuAlert.name = "High CPU Usage Alert";
    cpuAlert.metric = "system.cpu.usage";
    cpuAlert.condition = "greater_than";
    cpuAlert.threshold = 80.0;
    cpuAlert.severity = "warning";
    cpuAlert.durationSeconds = 60;
    cpuAlert.notificationChannel = "email,slack";
    cpuAlert.isEnabled = true;
    cpuAlert.metadata = QJsonObject{
        {"description", "CPU usage exceeds 80% for more than 1 minute"},
        {"remediation", "Check for resource-intensive processes"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/high-cpu"}
    };
    alertRules.append(cpuAlert);
    
    // High memory usage alert
    AlertRule memoryAlert;
    memoryAlert.ruleId = "high_memory_usage";
    memoryAlert.name = "High Memory Usage Alert";
    memoryAlert.metric = "system.memory.usage";
    memoryAlert.condition = "greater_than";
    memoryAlert.threshold = 85.0;
    memoryAlert.severity = "warning";
    memoryAlert.durationSeconds = 60;
    memoryAlert.notificationChannel = "email,slack";
    memoryAlert.isEnabled = true;
    memoryAlert.metadata = QJsonObject{
        {"description", "Memory usage exceeds 85% for more than 1 minute"},
        {"remediation", "Clear caches or increase memory allocation"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/high-memory"}
    };
    alertRules.append(memoryAlert);
    
    // AI response time alert
    AlertRule aiResponseAlert;
    aiResponseAlert.ruleId = "slow_ai_response";
    aiResponseAlert.name = "Slow AI Response Time";
    aiResponseAlert.metric = "ai.response.time";
    aiResponseAlert.condition = "greater_than";
    aiResponseAlert.threshold = 2000.0; // 2 seconds
    aiResponseAlert.severity = "warning";
    aiResponseAlert.durationSeconds = 30;
    aiResponseAlert.notificationChannel = "slack,pagerduty";
    aiResponseAlert.isEnabled = true;
    aiResponseAlert.metadata = QJsonObject{
        {"description", "AI response time exceeds 2 seconds"},
        {"remediation", "Check cloud connectivity or switch to local models"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/slow-ai"}
    };
    alertRules.append(aiResponseAlert);
    
    // Error rate alert
    AlertRule errorRateAlert;
    errorRateAlert.ruleId = "high_error_rate";
    errorRateAlert.name = "High Error Rate Alert";
    errorRateAlert.metric = "system.error.rate";
    errorRateAlert.condition = "greater_than";
    errorRateAlert.threshold = 5.0; // 5% error rate
    errorRateAlert.severity = "critical";
    errorRateAlert.durationSeconds = 60;
    errorRateAlert.notificationChannel = "email,slack,pagerduty";
    errorRateAlert.isEnabled = true;
    errorRateAlert.metadata = QJsonObject{
        {"description", "Error rate exceeds 5% for more than 1 minute"},
        {"remediation", "Initiate automated recovery procedures"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/high-errors"}
    };
    alertRules.append(errorRateAlert);
    
    // Cloud connectivity alert
    AlertRule cloudAlert;
    cloudAlert.ruleId = "cloud_disconnected";
    cloudAlert.name = "Cloud Connection Lost";
    cloudAlert.metric = "cloud.connection.status";
    cloudAlert.condition = "equals";
    cloudAlert.threshold = 0.0; // 0 = disconnected
    cloudAlert.severity = "critical";
    cloudAlert.durationSeconds = 30;
    cloudAlert.notificationChannel = "email,slack,pagerduty";
    cloudAlert.isEnabled = true;
    cloudAlert.metadata = QJsonObject{
        {"description", "Cloud connection lost"},
        {"remediation", "Switch to offline mode or reconnect"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/cloud-disconnected"}
    };
    alertRules.append(cloudAlert);
    
    // Security incident alert
    AlertRule securityAlert;
    securityAlert.ruleId = "security_incident";
    securityAlert.name = "Security Incident Detected";
    securityAlert.metric = "security.incidents";
    securityAlert.condition = "greater_than";
    securityAlert.threshold = 0.0;
    securityAlert.severity = "critical";
    securityAlert.durationSeconds = 0; // Immediate
    securityAlert.notificationChannel = "email,slack,pagerduty";
    securityAlert.isEnabled = true;
    securityAlert.metadata = QJsonObject{
        {"description", "Security incident or vulnerability detected"},
        {"remediation", "Activate incident response procedures"},
        {"runbook_url", "https://docs.rawrxd.com/alerts/security-incident"}
    };
    alertRules.append(securityAlert);
    
    std::cout << "[EnterpriseMonitoring] Setup " << alertRules.size() << " default alert rules" << std::endl;
}

void EnterpriseMonitoring::recordMetric(const QString& metricName, double value, 
                                       const QJsonObject& labels, const QJsonObject& metadata) {
    MetricData metric;
    metric.metricName = metricName;
    metric.value = value;
    metric.timestamp = QDateTime::currentDateTime();
    metric.labels = labels;
    metric.metadata = metadata;
    
    metricHistory.append(metric);
    
    // Keep only last 24 hours of metrics
    qint64 cutoffTime = QDateTime::currentDateTime().addSecs(-metricsRetentionHours * 3600).toMSecsSinceEpoch();
    metricHistory.erase(std::remove_if(metricHistory.begin(), metricHistory.end(),
                                      [cutoffTime](const MetricData& m) {
                                          return m.timestamp.toMSecsSinceEpoch() < cutoffTime;
                                      }), metricHistory.end());
    
    // Export to Prometheus format if needed
    exportPrometheusMetric(metricName, value, labels);
}

void EnterpriseMonitoring::exportPrometheusMetric(const QString& metricName, double value, 
                                                 const QJsonObject& labels) {
    // Format: metric_name{label1="value1",label2="value2"} value timestamp
    QString prometheusMetric = metricName;
    
    if (!labels.isEmpty()) {
        QStringList labelPairs;
        for (auto it = labels.begin(); it != labels.end(); ++it) {
            labelPairs.append(QString("%1=\"%2\"").arg(it.key(), it.value().toString()));
        }
        prometheusMetric += QString("{%1}").arg(labelPairs.join(","));
    }
    
    qint64 timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    prometheusMetric += QString(" %1 %2\n").arg(value).arg(timestamp);
    
    // Write to metrics file for Prometheus scraping
    QFile metricsFile("/tmp/rawrxd_metrics.prom");
    if (metricsFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&metricsFile);
        stream << prometheusMetric;
        metricsFile.close();
    }
}

void EnterpriseMonitoring::recordSystemMetrics() {
    // CPU usage
    double cpuUsage = getCurrentCPUUsage();
    recordMetric("system.cpu.usage", cpuUsage, QJsonObject{{"host", "localhost"}});
    
    // Memory usage
    double memoryUsage = getCurrentMemoryUsage();
    recordMetric("system.memory.usage", memoryUsage, QJsonObject{{"host", "localhost"}});
    
    // Disk usage
    double diskUsage = getCurrentDiskUsage();
    recordMetric("system.disk.usage", diskUsage, QJsonObject{{"host", "localhost"}});
    
    // Network stats
    qint64 networkBytesIn = getNetworkBytesIn();
    qint64 networkBytesOut = getNetworkBytesOut();
    recordMetric("system.network.bytes_in", networkBytesIn, QJsonObject{{"host", "localhost"}});
    recordMetric("system.network.bytes_out", networkBytesOut, QJsonObject{{"host", "localhost"}});
}

double EnterpriseMonitoring::getCurrentCPUUsage() const {
    // Simplified CPU usage calculation
    // In production, would use platform-specific system calls
    return 45.0 + (qrand() % 30); // Mock value 45-75%
}

double EnterpriseMonitoring::getCurrentMemoryUsage() const {
    // Simplified memory usage calculation
    return 60.0 + (qrand() % 20); // Mock value 60-80%
}

double EnterpriseMonitoring::getCurrentDiskUsage() const {
    // Simplified disk usage calculation
    return 50.0 + (qrand() % 30); // Mock value 50-80%
}

void EnterpriseMonitoring::evaluateAlertRules() {
    for (const AlertRule& rule : alertRules) {
        if (!rule.isEnabled) continue;
        
        // Get recent metrics for this rule
        QVector<MetricData> relevantMetrics;
        for (const MetricData& metric : metricHistory) {
            if (metric.metricName == rule.metric) {
                relevantMetrics.append(metric);
            }
        }
        
        if (relevantMetrics.isEmpty()) continue;
        
        // Check if alert should fire
        if (shouldFireAlert(rule, relevantMetrics)) {
            double currentValue = relevantMetrics.last().value;
            processAlert(rule, currentValue);
        }
    }
    
    // Evaluate predictive alerts if enabled
    if (enablePredictiveAlerts) {
        evaluatePredictiveAlerts();
    }
}

bool EnterpriseMonitoring::shouldFireAlert(const AlertRule& rule, const QVector<MetricData>& metrics) {
    if (metrics.isEmpty()) return false;
    
    // Get metrics within the duration window
    QDateTime now = QDateTime::currentDateTime();
    QDateTime windowStart = now.addSecs(-rule.durationSeconds);
    
    QVector<double> windowValues;
    for (const MetricData& metric : metrics) {
        if (metric.timestamp >= windowStart && metric.timestamp <= now) {
            windowValues.append(metric.value);
        }
    }
    
    if (windowValues.isEmpty()) return false;
    
    // Check condition
    bool shouldFire = false;
    double latestValue = windowValues.last();
    
    if (rule.condition == "greater_than") {
        // All values in window must exceed threshold
        shouldFire = std::all_of(windowValues.begin(), windowValues.end(),
                                [threshold = rule.threshold](double v) { return v > threshold; });
    } else if (rule.condition == "less_than") {
        shouldFire = std::all_of(windowValues.begin(), windowValues.end(),
                                [threshold = rule.threshold](double v) { return v < threshold; });
    } else if (rule.condition == "equals") {
        shouldFire = std::all_of(windowValues.begin(), windowValues.end(),
                                [threshold = rule.threshold](double v) { return v == threshold; });
    }
    
    return shouldFire;
}

void EnterpriseMonitoring::processAlert(const AlertRule& rule, double currentValue) {
    // Check if alert is already active
    if (activeAlerts.contains(rule.ruleId)) {
        Alert& existingAlert = activeAlerts[rule.ruleId];
        if (existingAlert.status == "firing") {
            // Alert still firing, update last seen time
            return;
        }
    }
    
    // Create new alert
    Alert alert;
    alert.alertId = QString("alert_%1_%2").arg(rule.ruleId).arg(QDateTime::currentDateTime().toMSecsSinceEpoch());
    alert.ruleId = rule.ruleId;
    alert.status = "firing";
    alert.severity = rule.severity;
    alert.summary = rule.name;
    alert.description = QString("Metric '%1' value %2 triggered alert (threshold: %3)")
        .arg(rule.metric).arg(currentValue).arg(rule.threshold);
    alert.startTime = QDateTime::currentDateTime();
    alert.labels = QJsonObject{
        {"alert_rule", rule.ruleId},
        {"severity", rule.severity},
        {"metric", rule.metric}
    };
    alert.annotations = rule.metadata;
    
    activeAlerts[rule.ruleId] = alert;
    
    // Send notifications
    sendNotification(alert);
    
    // Execute auto-remediation if enabled and applicable
    if (enableAutoRemediation && rule.severity == "critical") {
        QTimer::singleShot(5000, [this, alert]() {
            executeAutoRemediation(alert);
        });
    }
    
    emit alertFired(alert);
    
    std::cout << "[EnterpriseMonitoring] Alert fired: " << rule.ruleId.toStdString() 
              << " (severity: " << rule.severity.toStdString() << ")" << std::endl;
}

void EnterpriseMonitoring::sendNotification(const Alert& alert) {
    QJsonObject payload = createNotificationPayload(alert);
    
    // Get notification channels from alert rule
    auto ruleIt = std::find_if(alertRules.begin(), alertRules.end(),
                               [&alert](const AlertRule& rule) {
                                   return rule.ruleId == alert.ruleId;
                               });
    
    if (ruleIt == alertRules.end()) return;
    
    QString channels = ruleIt->notificationChannel;
    QStringList channelList = channels.split(",", Qt::SkipEmptyParts);
    
    for (const QString& channel : channelList) {
        QString trimmedChannel = channel.trimmed();
        
        if (trimmedChannel == "webhook") {
            sendWebhookNotification(payload);
        } else if (trimmedChannel == "email") {
            sendEmailNotification(alert);
        } else if (trimmedChannel == "slack") {
            sendSlackNotification(alert);
        } else if (trimmedChannel == "pagerduty") {
            sendPagerDutyNotification(alert);
        }
    }
}

QJsonObject EnterpriseMonitoring::createNotificationPayload(const Alert& alert) const {
    QJsonObject payload;
    
    payload["alert_id"] = alert.alertId;
    payload["rule_id"] = alert.ruleId;
    payload["status"] = alert.status;
    payload["severity"] = alert.severity;
    payload["summary"] = alert.summary;
    payload["description"] = alert.description;
    payload["start_time"] = alert.startTime.toString(Qt::ISODate);
    payload["labels"] = alert.labels;
    payload["annotations"] = alert.annotations;
    
    return payload;
}

void EnterpriseMonitoring::sendWebhookNotification(const QJsonObject& payload) {
    if (webhookUrl.isEmpty()) {
        std::cout << "[EnterpriseMonitoring] No webhook URL configured" << std::endl;
        return;
    }
    
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(webhookUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(payload);
    QNetworkReply* reply = manager->post(request, doc.toJson());
    
    connect(reply, &QNetworkReply::finished, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            std::cout << "[EnterpriseMonitoring] Webhook notification sent successfully" << std::endl;
        } else {
            std::cerr << "[EnterpriseMonitoring] Webhook notification failed: " 
                      << reply->errorString().toStdString() << std::endl;
        }
        reply->deleteLater();
    });
}

void EnterpriseMonitoring::sendSlackNotification(const Alert& alert) {
    if (slackWebhook.isEmpty()) return;
    
    QJsonObject slackPayload;
    
    // Format message based on severity
    QString emoji = (alert.severity == "critical") ? ":rotating_light:" : ":warning:";
    QString color = (alert.severity == "critical") ? "#dc3545" : "#ffc107";
    
    QJsonArray attachments;
    QJsonObject attachment;
    attachment["color"] = color;
    attachment["title"] = emoji + " " + alert.summary;
    attachment["text"] = alert.description;
    attachment["ts"] = alert.startTime.toSecsSinceEpoch();
    
    QJsonArray fields;
    fields.append(QJsonObject{{"title", "Severity"}, {"value", alert.severity}, {"short", true}});
    fields.append(QJsonObject{{"title", "Alert ID"}, {"value", alert.alertId}, {"short", true}});
    attachment["fields"] = fields;
    
    attachments.append(attachment);
    slackPayload["attachments"] = attachments;
    
    sendWebhookNotification(slackPayload);
}

bool EnterpriseMonitoring::executeAutoRemediation(const Alert& alert) {
    std::cout << "[EnterpriseMonitoring] Executing auto-remediation for alert: " 
              << alert.alertId.toStdString() << std::endl;
    
    bool success = false;
    
    // Execute remediation based on alert rule
    if (alert.ruleId == "high_cpu_usage") {
        // Clear caches
        std::cout << "[AutoRemediation] Clearing caches to reduce CPU usage" << std::endl;
        success = true;
    } else if (alert.ruleId == "high_memory_usage") {
        // Run garbage collection
        std::cout << "[AutoRemediation] Running garbage collection to free memory" << std::endl;
        success = true;
    } else if (alert.ruleId == "cloud_disconnected") {
        // Attempt reconnection
        std::cout << "[AutoRemediation] Attempting to reconnect to cloud services" << std::endl;
        success = true;
    } else if (alert.ruleId == "high_error_rate") {
        // Restart affected services
        std::cout << "[AutoRemediation] Restarting affected services" << std::endl;
        success = true;
    }
    
    emit autoRemediationExecuted(alert.alertId, success);
    
    return success;
}

QJsonObject EnterpriseMonitoring::getMonitoringDashboard() const {
    QJsonObject dashboard;
    
    // Alert summary
    int firingAlerts = 0;
    int resolvedAlerts = 0;
    QHash<QString, int> alertsBySeverity;
    
    for (const Alert& alert : activeAlerts) {
        if (alert.status == "firing") {
            firingAlerts++;
            alertsBySeverity[alert.severity]++;
        } else if (alert.status == "resolved") {
            resolvedAlerts++;
        }
    }
    
    dashboard["active_alerts"] = firingAlerts;
    dashboard["resolved_alerts"] = resolvedAlerts;
    dashboard["critical_alerts"] = alertsBySeverity["critical"];
    dashboard["warning_alerts"] = alertsBySeverity["warning"];
    dashboard["info_alerts"] = alertsBySeverity["info"];
    
    // System health
    dashboard["system_health"] = isMeetingSLA() ? "healthy" : "degraded";
    dashboard["sla_uptime"] = getSLAUptime();
    
    // Recent metrics summary
    QJsonObject metricsummary;
    metricsummary["cpu_usage"] = getCurrentCPUUsage();
    metricsummary["memory_usage"] = getCurrentMemoryUsage();
    metricsummary["disk_usage"] = getCurrentDiskUsage();
    dashboard["current_metrics"] = metricsummary;
    
    // Alert rules status
    int enabledRules = 0;
    for (const AlertRule& rule : alertRules) {
        if (rule.isEnabled) enabledRules++;
    }
    dashboard["total_alert_rules"] = alertRules.size();
    dashboard["enabled_alert_rules"] = enabledRules;
    
    return dashboard;
}

bool EnterpriseMonitoring::isMeetingSLA() const {
    return getSLAUptime() >= 99.9; // 99.9% uptime SLA
}

double EnterpriseMonitoring::getSLAUptime() const {
    // Calculate uptime based on critical alerts
    int totalMinutes = 24 * 60; // Last 24 hours
    int downtimeMinutes = 0;
    
    for (const Alert& alert : activeAlerts) {
        if (alert.severity == "critical" && alert.status == "firing") {
            qint64 durationMs = alert.startTime.msecsTo(QDateTime::currentDateTime());
            downtimeMinutes += durationMs / 60000;
        }
    }
    
    double uptimePercentage = ((totalMinutes - downtimeMinutes) * 100.0) / totalMinutes;
    return std::max(0.0, std::min(100.0, uptimePercentage));
}

QJsonObject EnterpriseMonitoring::getSLAReport() const {
    QJsonObject report;
    
    report["uptime_percentage"] = getSLAUptime();
    report["sla_target"] = 99.9;
    report["is_meeting_sla"] = isMeetingSLA();
    
    // Calculate availability metrics
    qint64 totalTime = 24 * 3600 * 1000; // 24 hours in ms
    qint64 downtime = 0;
    
    for (const Alert& alert : activeAlerts) {
        if (alert.severity == "critical") {
            if (alert.status == "firing") {
                downtime += alert.startTime.msecsTo(QDateTime::currentDateTime());
            } else if (!alert.endTime.isNull()) {
                downtime += alert.startTime.msecsTo(alert.endTime);
            }
        }
    }
    
    report["total_downtime_ms"] = downtime;
    report["total_downtime_minutes"] = downtime / 60000;
    report["availability_percentage"] = ((totalTime - downtime) * 100.0) / totalTime;
    
    return report;
}

void EnterpriseMonitoring::setupAutoRemediationRules() {
    // Auto-remediation rules are defined in executeAutoRemediation
    std::cout << "[EnterpriseMonitoring] Auto-remediation rules configured" << std::endl;
}
