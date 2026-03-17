#include "RoutingAnalytics.h"
#include "ModelRouterExtension.h"
#include <QDebug>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

// ============ Request Metrics Serialization ============

QJsonObject RequestMetrics::toJson() const {
    QJsonObject obj;
    obj["requestId"] = requestId;
    obj["endpointId"] = endpointId;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["responseTime"] = responseTime;
    obj["tokensGenerated"] = tokensGenerated;
    obj["success"] = success;
    obj["errorMessage"] = errorMessage;
    obj["estimatedCost"] = static_cast<qint64>(estimatedCost);
    
    return obj;
}

// ============ Endpoint Metrics Serialization ============

QJsonObject EndpointMetrics::toJson() const {
    QJsonObject obj;
    obj["endpointId"] = endpointId;
    obj["totalRequests"] = static_cast<qint64>(totalRequests);
    obj["successfulRequests"] = static_cast<qint64>(successfulRequests);
    obj["failedRequests"] = static_cast<qint64>(failedRequests);
    obj["averageResponseTime"] = averageResponseTime;
    obj["p50ResponseTime"] = p50ResponseTime;
    obj["p95ResponseTime"] = p95ResponseTime;
    obj["p99ResponseTime"] = p99ResponseTime;
    obj["successRate"] = successRate;
    obj["totalCost"] = static_cast<qint64>(totalCost);
    obj["lastUpdated"] = lastUpdated.toString(Qt::ISODate);
    
    return obj;
}

// ============ AB Test Config Serialization ============

QJsonObject ABTestConfig::toJson() const {
    QJsonObject obj;
    obj["testId"] = testId;
    obj["variantA"] = variantA;
    obj["variantB"] = variantB;
    obj["trafficSplitA"] = trafficSplitA;
    obj["trafficSplitB"] = trafficSplitB;
    obj["startTime"] = startTime.toString(Qt::ISODate);
    obj["endTime"] = endTime.toString(Qt::ISODate);
    obj["active"] = active;
    obj["hypothesis"] = hypothesis;
    
    return obj;
}

// ============ AB Test Results Serialization ============

QJsonObject ABTestResults::toJson() const {
    QJsonObject obj;
    obj["testId"] = testId;
    obj["variantAMetrics"] = variantAMetrics.toJson();
    obj["variantBMetrics"] = variantBMetrics.toJson();
    obj["confidenceLevel"] = confidenceLevel;
    obj["winningVariant"] = winningVariant;
    obj["conclusion"] = conclusion;
    obj["analyzedAt"] = analyzedAt.toString(Qt::ISODate);
    
    return obj;
}

// ============ Cost Analysis Report Serialization ============

QJsonObject CostAnalysisReport::toJson() const {
    QJsonObject obj;
    obj["endpointId"] = endpointId;
    obj["startTime"] = startTime.toString(Qt::ISODate);
    obj["endTime"] = endTime.toString(Qt::ISODate);
    obj["totalRequests"] = static_cast<qint64>(totalRequests);
    obj["totalTokens"] = static_cast<qint64>(totalTokens);
    obj["totalCost"] = static_cast<qint64>(totalCost);
    obj["averageCostPerRequest"] = averageCostPerRequest;
    obj["averageCostPerToken"] = averageCostPerToken;
    
    QJsonObject costByHourObj;
    for (auto it = costByHour.begin(); it != costByHour.end(); ++it) {
        costByHourObj[it.key()] = static_cast<qint64>(it.value());
    }
    obj["costByHour"] = costByHourObj;
    
    return obj;
}

// ============ RoutingAnalytics Implementation ============

RoutingAnalytics::RoutingAnalytics(QObject* parent)
    : QObject(parent)
{
}

RoutingAnalytics::~RoutingAnalytics() {
}

// ============ Request Tracking ============

void RoutingAnalytics::trackRequest(const RoutingRequest& request, const RoutingDecision& decision) {
    QMutexLocker locker(&m_mutex);
    
    RequestMetrics metrics;
    metrics.requestId = request.requestId;
    metrics.endpointId = decision.endpointId;
    metrics.timestamp = QDateTime::currentDateTime();
    
    m_requestMetrics[request.requestId] = metrics;
}

void RoutingAnalytics::recordRequestCompletion(const QString& requestId, double responseTime,
                                               int tokensGenerated, bool success, const QString& error) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_requestMetrics.contains(requestId)) {
        qWarning() << "Request not tracked:" << requestId;
        return;
    }
    
    RequestMetrics& metrics = m_requestMetrics[requestId];
    metrics.responseTime = responseTime;
    metrics.tokensGenerated = tokensGenerated;
    metrics.success = success;
    metrics.errorMessage = error;
    
    // Update endpoint metrics
    QString endpointId = metrics.endpointId;
    
    if (!m_endpointMetrics.contains(endpointId)) {
        EndpointMetrics epMetrics;
        epMetrics.endpointId = endpointId;
        m_endpointMetrics[endpointId] = epMetrics;
    }
    
    EndpointMetrics& epMetrics = m_endpointMetrics[endpointId];
    epMetrics.totalRequests++;
    
    if (success) {
        epMetrics.successfulRequests++;
    } else {
        epMetrics.failedRequests++;
    }
    
    // Track response times for percentile calculation
    if (!m_responseTimes.contains(endpointId)) {
        m_responseTimes[endpointId] = QList<double>();
    }
    m_responseTimes[endpointId].append(responseTime);
    
    // Update average response time
    double oldAvg = epMetrics.averageResponseTime;
    epMetrics.averageResponseTime = ((oldAvg * (epMetrics.totalRequests - 1)) + responseTime) / epMetrics.totalRequests;
    
    // Update success rate
    epMetrics.successRate = static_cast<double>(epMetrics.successfulRequests) / epMetrics.totalRequests;
    
    epMetrics.lastUpdated = QDateTime::currentDateTime();
    
    // Calculate percentiles
    locker.unlock();
    epMetrics.p50ResponseTime = calculatePercentile(endpointId, 0.50);
    epMetrics.p95ResponseTime = calculatePercentile(endpointId, 0.95);
    epMetrics.p99ResponseTime = calculatePercentile(endpointId, 0.99);
    locker.relock();
}

RequestMetrics RoutingAnalytics::getRequestMetrics(const QString& requestId) const {
    QMutexLocker locker(&m_mutex);
    return m_requestMetrics.value(requestId);
}

QList<RequestMetrics> RoutingAnalytics::getRequestsInTimeRange(const QDateTime& start, const QDateTime& end) const {
    QMutexLocker locker(&m_mutex);
    
    QList<RequestMetrics> result;
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.timestamp >= start && metrics.timestamp <= end) {
            result.append(metrics);
        }
    }
    
    return result;
}

// ============ Performance Metrics ============

EndpointMetrics RoutingAnalytics::getEndpointMetrics(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    return m_endpointMetrics.value(endpointId);
}

QMap<QString, EndpointMetrics> RoutingAnalytics::getAllEndpointMetrics() const {
    QMutexLocker locker(&m_mutex);
    return m_endpointMetrics;
}

double RoutingAnalytics::calculatePercentile(const QString& endpointId, double percentile) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_responseTimes.contains(endpointId)) {
        return 0.0;
    }
    
    QList<double> times = m_responseTimes[endpointId];
    if (times.isEmpty()) {
        return 0.0;
    }
    
    // Sort response times
    QVector<double> sorted = times.toVector();
    std::sort(sorted.begin(), sorted.end());
    
    // Calculate percentile index
    int index = static_cast<int>(std::ceil(percentile * sorted.size())) - 1;
    index = std::max(0, std::min(index, static_cast<int>(sorted.size() - 1)));
    
    return sorted[index];
}

QList<QString> RoutingAnalytics::getTopEndpoints(int count) const {
    QMutexLocker locker(&m_mutex);
    
    QList<QPair<QString, double>> scored;
    
    for (auto it = m_endpointMetrics.begin(); it != m_endpointMetrics.end(); ++it) {
        const EndpointMetrics& metrics = it.value();
        
        // Score based on success rate and response time (lower is better)
        double score = metrics.successRate * 1000.0 / (metrics.averageResponseTime + 1.0);
        scored.append({it.key(), score});
    }
    
    // Sort by score (highest first)
    std::sort(scored.begin(), scored.end(), 
             [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                 return a.second > b.second;
             });
    
    QList<QString> result;
    for (int i = 0; i < std::min(static_cast<qsizetype>(count), static_cast<qsizetype>(scored.size())); ++i) {
        result.append(scored[i].first);
    }
    
    return result;
}

QList<QString> RoutingAnalytics::getBottomEndpoints(int count) const {
    QMutexLocker locker(&m_mutex);
    
    QList<QPair<QString, double>> scored;
    
    for (auto it = m_endpointMetrics.begin(); it != m_endpointMetrics.end(); ++it) {
        const EndpointMetrics& metrics = it.value();
        
        double score = metrics.successRate * 1000.0 / (metrics.averageResponseTime + 1.0);
        scored.append({it.key(), score});
    }
    
    // Sort by score (lowest first)
    std::sort(scored.begin(), scored.end(),
             [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                 return a.second < b.second;
             });
    
    QList<QString> result;
    for (int i = 0; i < std::min(static_cast<qsizetype>(count), static_cast<qsizetype>(scored.size())); ++i) {
        result.append(scored[i].first);
    }
    
    return result;
}

// ============ Comparative Analysis ============

QJsonObject RoutingAnalytics::compareEndpoints(const QString& endpointA, const QString& endpointB) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject comparison;
    
    EndpointMetrics metricsA = m_endpointMetrics.value(endpointA);
    EndpointMetrics metricsB = m_endpointMetrics.value(endpointB);
    
    comparison["endpointA"] = endpointA;
    comparison["endpointB"] = endpointB;
    comparison["metricsA"] = metricsA.toJson();
    comparison["metricsB"] = metricsB.toJson();
    
    // Calculate differences
    QJsonObject differences;
    differences["responseTimeDiff"] = metricsA.averageResponseTime - metricsB.averageResponseTime;
    differences["successRateDiff"] = metricsA.successRate - metricsB.successRate;
    differences["requestCountDiff"] = static_cast<qint64>(metricsA.totalRequests - metricsB.totalRequests);
    
    comparison["differences"] = differences;
    
    // Statistical significance
    locker.unlock();
    double significance = calculateStatisticalSignificance(metricsA, metricsB);
    locker.relock();
    comparison["statisticalSignificance"] = significance;
    
    return comparison;
}

QVector<double> RoutingAnalytics::getPerformanceTrend(const QString& endpointId, int hours) const {
    QMutexLocker locker(&m_mutex);
    
    QVector<double> trend(hours, 0.0);
    QVector<int> counts(hours, 0);
    
    QDateTime now = QDateTime::currentDateTime();
    QDateTime startTime = now.addSecs(-hours * 3600);
    
    // Aggregate response times by hour
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.endpointId != endpointId) continue;
        if (metrics.timestamp < startTime) continue;
        
        int hourIndex = startTime.secsTo(metrics.timestamp) / 3600;
        if (hourIndex >= 0 && hourIndex < hours) {
            trend[hourIndex] += metrics.responseTime;
            counts[hourIndex]++;
        }
    }
    
    // Calculate averages
    for (int i = 0; i < hours; ++i) {
        if (counts[i] > 0) {
            trend[i] /= counts[i];
        }
    }
    
    return trend;
}

QVector<qint64> RoutingAnalytics::getThroughputTrend(const QString& endpointId, int hours) const {
    QMutexLocker locker(&m_mutex);
    
    QVector<qint64> trend(hours, 0);
    
    QDateTime now = QDateTime::currentDateTime();
    QDateTime startTime = now.addSecs(-hours * 3600);
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.endpointId != endpointId) continue;
        if (metrics.timestamp < startTime) continue;
        
        int hourIndex = startTime.secsTo(metrics.timestamp) / 3600;
        if (hourIndex >= 0 && hourIndex < hours) {
            trend[hourIndex]++;
        }
    }
    
    return trend;
}

// ============ A/B Testing ============

QString RoutingAnalytics::startABTest(const ABTestConfig& config) {
    QMutexLocker locker(&m_mutex);
    
    ABTestConfig test = config;
    test.active = true;
    test.startTime = QDateTime::currentDateTime();
    
    m_abTests[test.testId] = test;
    
    qDebug() << "Started A/B test:" << test.testId;
    
    return test.testId;
}

void RoutingAnalytics::stopABTest(const QString& testId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_abTests.contains(testId)) {
        m_abTests[testId].active = false;
        m_abTests[testId].endTime = QDateTime::currentDateTime();
        
        locker.unlock();
        ABTestResults results = analyzeABTest(testId);
        emit abTestCompleted(testId, results);
        
        qDebug() << "Stopped A/B test:" << testId;
    }
}

ABTestConfig RoutingAnalytics::getABTestConfig(const QString& testId) const {
    QMutexLocker locker(&m_mutex);
    return m_abTests.value(testId);
}

ABTestResults RoutingAnalytics::analyzeABTest(const QString& testId) const {
    QMutexLocker locker(&m_mutex);
    
    ABTestResults results;
    results.testId = testId;
    
    if (!m_abTests.contains(testId)) {
        results.conclusion = "Test not found";
        return results;
    }
    
    const ABTestConfig& config = m_abTests[testId];
    
    // Get metrics for both variants
    results.variantAMetrics = m_endpointMetrics.value(config.variantA);
    results.variantBMetrics = m_endpointMetrics.value(config.variantB);
    
    // Calculate statistical significance
    locker.unlock();
    results.confidenceLevel = calculateStatisticalSignificance(
        results.variantAMetrics, results.variantBMetrics);
    
    results.winningVariant = determineWinner(
        results.variantAMetrics, results.variantBMetrics, results.confidenceLevel);
    locker.relock();
    
    // Generate conclusion
    if (results.winningVariant == "A") {
        results.conclusion = QString("Variant A (%1) performs significantly better").arg(config.variantA);
    } else if (results.winningVariant == "B") {
        results.conclusion = QString("Variant B (%1) performs significantly better").arg(config.variantB);
    } else {
        results.conclusion = "No statistically significant difference detected";
    }
    
    results.analyzedAt = QDateTime::currentDateTime();
    
    return results;
}

QList<ABTestConfig> RoutingAnalytics::getActiveABTests() const {
    QMutexLocker locker(&m_mutex);
    
    QList<ABTestConfig> active;
    
    for (const ABTestConfig& test : m_abTests) {
        if (test.active) {
            active.append(test);
        }
    }
    
    return active;
}

QString RoutingAnalytics::routeForABTest(const QString& testId, const RoutingRequest& request) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_abTests.contains(testId)) {
        return QString();
    }
    
    const ABTestConfig& test = m_abTests[testId];
    
    if (!test.active) {
        return QString();
    }
    
    // Random routing based on traffic split
    double random = QRandomGenerator::global()->generateDouble();
    
    if (random < test.trafficSplitA) {
        return test.variantA;
    } else {
        return test.variantB;
    }
}

// ============ Cost Analysis ============

void RoutingAnalytics::trackRequestCost(const QString& requestId, qint64 cost) {
    QMutexLocker locker(&m_mutex);
    
    m_requestCosts[requestId] = cost;
    
    if (m_requestMetrics.contains(requestId)) {
        m_requestMetrics[requestId].estimatedCost = cost;
        
        QString endpointId = m_requestMetrics[requestId].endpointId;
        if (m_endpointMetrics.contains(endpointId)) {
            m_endpointMetrics[endpointId].totalCost += cost;
        }
    }
}

CostAnalysisReport RoutingAnalytics::getCostAnalysis(const QString& endpointId,
                                                     const QDateTime& start, const QDateTime& end) const {
    QMutexLocker locker(&m_mutex);
    
    CostAnalysisReport report;
    report.endpointId = endpointId;
    report.startTime = start;
    report.endTime = end;
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.endpointId != endpointId) continue;
        if (metrics.timestamp < start || metrics.timestamp > end) continue;
        
        report.totalRequests++;
        report.totalTokens += metrics.tokensGenerated;
        report.totalCost += metrics.estimatedCost;
        
        // Track cost by hour
        QString hourKey = metrics.timestamp.toString("yyyy-MM-dd hh");
        report.costByHour[hourKey] += metrics.estimatedCost;
    }
    
    if (report.totalRequests > 0) {
        report.averageCostPerRequest = static_cast<double>(report.totalCost) / report.totalRequests;
    }
    
    if (report.totalTokens > 0) {
        report.averageCostPerToken = static_cast<double>(report.totalCost) / report.totalTokens;
    }
    
    return report;
}

qint64 RoutingAnalytics::getTotalCost(const QDateTime& start, const QDateTime& end) const {
    QMutexLocker locker(&m_mutex);
    
    qint64 total = 0;
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.timestamp >= start && metrics.timestamp <= end) {
            total += metrics.estimatedCost;
        }
    }
    
    return total;
}

QMap<QString, qint64> RoutingAnalytics::getCostByEndpoint(const QDateTime& start, const QDateTime& end) const {
    QMutexLocker locker(&m_mutex);
    
    QMap<QString, qint64> costs;
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.timestamp >= start && metrics.timestamp <= end) {
            costs[metrics.endpointId] += metrics.estimatedCost;
        }
    }
    
    return costs;
}

qint64 RoutingAnalytics::forecastCost(const QString& endpointId, int hours) const {
    QMutexLocker locker(&m_mutex);
    
    // Simple forecast based on recent average
    QDateTime now = QDateTime::currentDateTime();
    QDateTime start = now.addSecs(-hours * 3600);
    
    qint64 recentCost = 0;
    int count = 0;
    
    for (const RequestMetrics& metrics : m_requestMetrics) {
        if (metrics.endpointId != endpointId) continue;
        if (metrics.timestamp < start) continue;
        
        recentCost += metrics.estimatedCost;
        count++;
    }
    
    if (count == 0) return 0;
    
    double averageCostPerRequest = static_cast<double>(recentCost) / count;
    double requestsPerHour = static_cast<double>(count) / hours;
    
    return static_cast<qint64>(averageCostPerRequest * requestsPerHour * hours);
}

// ============ Reporting ============

QJsonObject RoutingAnalytics::generatePerformanceReport() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject report;
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray endpointsArray;
    for (const EndpointMetrics& metrics : m_endpointMetrics) {
        endpointsArray.append(metrics.toJson());
    }
    report["endpoints"] = endpointsArray;
    
    locker.unlock();
    QList<QString> topEndpoints = getTopEndpoints(5);
    locker.relock();
    
    QJsonArray topArray;
    for (const QString& id : topEndpoints) {
        topArray.append(id);
    }
    report["topPerformers"] = topArray;
    
    return report;
}

QJsonObject RoutingAnalytics::generateCostReport(const QDateTime& start, const QDateTime& end) const {
    QJsonObject report;
    report["startTime"] = start.toString(Qt::ISODate);
    report["endTime"] = end.toString(Qt::ISODate);
    report["totalCost"] = static_cast<qint64>(getTotalCost(start, end));
    
    QMap<QString, qint64> costsByEndpoint = getCostByEndpoint(start, end);
    QJsonObject costsObj;
    for (auto it = costsByEndpoint.begin(); it != costsByEndpoint.end(); ++it) {
        costsObj[it.key()] = static_cast<qint64>(it.value());
    }
    report["costByEndpoint"] = costsObj;
    
    return report;
}

QJsonObject RoutingAnalytics::generateComparativeReport(const QStringList& endpointIds) const {
    QJsonObject report;
    report["generatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray endpointsArray;
    for (const QString& id : endpointIds) {
        EndpointMetrics metrics = getEndpointMetrics(id);
        endpointsArray.append(metrics.toJson());
    }
    report["endpoints"] = endpointsArray;
    
    return report;
}

QJsonObject RoutingAnalytics::exportMetrics() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject data;
    
    QJsonArray requestsArray;
    for (const RequestMetrics& metrics : m_requestMetrics) {
        requestsArray.append(metrics.toJson());
    }
    data["requests"] = requestsArray;
    
    QJsonArray endpointsArray;
    for (const EndpointMetrics& metrics : m_endpointMetrics) {
        endpointsArray.append(metrics.toJson());
    }
    data["endpoints"] = endpointsArray;
    
    return data;
}

// ============ Data Management ============

void RoutingAnalytics::clearOldData(int daysToKeep) {
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-daysToKeep);
    
    // Remove old requests
    QList<QString> toRemove;
    for (auto it = m_requestMetrics.begin(); it != m_requestMetrics.end(); ++it) {
        if (it.value().timestamp < cutoff) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& id : toRemove) {
        m_requestMetrics.remove(id);
        m_requestCosts.remove(id);
    }
    
    qDebug() << "Cleared" << toRemove.size() << "old requests";
}

void RoutingAnalytics::resetMetrics() {
    QMutexLocker locker(&m_mutex);
    
    m_requestMetrics.clear();
    m_endpointMetrics.clear();
    m_responseTimes.clear();
    m_requestCosts.clear();
    
    qDebug() << "All metrics reset";
}

qint64 RoutingAnalytics::getDataSize() const {
    QMutexLocker locker(&m_mutex);
    
    // Rough estimate
    qint64 size = 0;
    size += m_requestMetrics.size() * 200;  // ~200 bytes per request
    size += m_endpointMetrics.size() * 150;  // ~150 bytes per endpoint
    
    for (const auto& list : m_responseTimes) {
        size += list.size() * sizeof(double);
    }
    
    return size;
}

// ============ Private Helper Functions ============

void RoutingAnalytics::updateEndpointMetrics(const QString& endpointId) {
    // Already handled in recordRequestCompletion
}

double RoutingAnalytics::calculateStatisticalSignificance(const EndpointMetrics& a, const EndpointMetrics& b) const {
    // Simplified statistical test (t-test approximation)
    if (a.totalRequests < 30 || b.totalRequests < 30) {
        return 0.0;  // Insufficient data
    }
    
    double meanDiff = std::abs(a.averageResponseTime - b.averageResponseTime);
    double pooledStdDev = std::sqrt(
        (a.p95ResponseTime - a.averageResponseTime) * (a.p95ResponseTime - a.averageResponseTime) +
        (b.p95ResponseTime - b.averageResponseTime) * (b.p95ResponseTime - b.averageResponseTime)
    ) / 2.0;
    
    if (pooledStdDev < 1.0) return 1.0;  // Very low variance
    
    double tStat = meanDiff / (pooledStdDev / std::sqrt((a.totalRequests + b.totalRequests) / 2.0));
    
    // Map t-statistic to confidence (simplified)
    double confidence = std::min(1.0, tStat / 3.0);
    
    return confidence;
}

QString RoutingAnalytics::determineWinner(const EndpointMetrics& a, const EndpointMetrics& b, double confidence) const {
    if (confidence < 0.8) {
        return "inconclusive";
    }
    
    // Compare based on response time and success rate
    double scoreA = a.successRate * 1000.0 / (a.averageResponseTime + 1.0);
    double scoreB = b.successRate * 1000.0 / (b.averageResponseTime + 1.0);
    
    if (scoreA > scoreB * 1.1) {  // 10% margin
        return "A";
    } else if (scoreB > scoreA * 1.1) {
        return "B";
    }
    
    return "inconclusive";
}
