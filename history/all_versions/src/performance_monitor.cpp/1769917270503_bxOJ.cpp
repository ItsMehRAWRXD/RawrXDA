// performance_monitor.cpp - Real-Time Performance Monitoring & SLA Tracking
#include "performance_monitor.h"


#include <iostream>
#include <algorithm>
#include <numeric>

#include <windows.h>
#include <psapi.h>

PerformanceMonitor::PerformanceMonitor(void* parent)
    : monitoringEnabled(true),
      snapshotIntervalMs(60000),
      metricsRetentionHours(24),
      m_running(false) {
    
    // Convert void* to ignoring it or storing it properly if needed.
    // Assuming void* parent was for Qt QObject parent, we can ignore it.

    setupDefaultSLAs();
    setupDefaultThresholds();
    
    enableMonitoring(true);
}

PerformanceMonitor::~PerformanceMonitor() {
    enableMonitoring(false);
}

void PerformanceMonitor::monitoringLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(snapshotIntervalMs));
        if (!m_running) break;
        
        PerformanceSnapshot snapshot = capturePerformanceSnapshot();
        snapshotCaptured(snapshot);
        
        // Record Heartbeat
        recordMetric("system", "heartbeat", 1.0, "count");
    }
}

void PerformanceMonitor::enableMonitoring(bool enable) {
    monitoringEnabled = enable;
    if (enable && !m_running) {
        m_running = true;
        m_monitorThread = std::thread(&PerformanceMonitor::monitoringLoop, this);
    } else if (!enable && m_running) {
        m_running = false;
        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }
    }
}

void PerformanceMonitor::setSnapshotInterval(int milliseconds) {
    snapshotIntervalMs = milliseconds;
}


void PerformanceMonitor::setupDefaultSLAs() {
    // SLA 1: 99.9% uptime (8.76 hours downtime per year allowed)
    SLADefinition uptimeSLA;
    uptimeSLA.slaId = "uptime_999";
    uptimeSLA.name = "99.9% Uptime";
    uptimeSLA.component = "system";
    uptimeSLA.metric = "availability";
    uptimeSLA.targetValue = 99.9;
    uptimeSLA.acceptableValue = 99.5;
    uptimeSLA.timeWindow = "30d";
    uptimeSLA.isActive = true;
    slaDefinitions["uptime_999"] = uptimeSLA;
    
    // SLA 2: P95 latency < 500ms
    SLADefinition latencySLA;
    latencySLA.slaId = "latency_p95";
    latencySLA.name = "P95 Latency < 500ms";
    latencySLA.component = "ai_execution";
    latencySLA.metric = "latency_p95";
    latencySLA.targetValue = 500.0;
    latencySLA.acceptableValue = 1000.0;
    latencySLA.timeWindow = "24h";
    latencySLA.isActive = true;
    slaDefinitions["latency_p95"] = latencySLA;
    
    // SLA 3: Error rate < 1%
    SLADefinition errorRateSLA;
    errorRateSLA.slaId = "error_rate";
    errorRateSLA.name = "Error Rate < 1%";
    errorRateSLA.component = "system";
    errorRateSLA.metric = "error_rate";
    errorRateSLA.targetValue = 1.0;
    errorRateSLA.acceptableValue = 5.0;
    errorRateSLA.timeWindow = "24h";
    errorRateSLA.isActive = true;
    slaDefinitions["error_rate"] = errorRateSLA;
    
    // SLA 4: Memory usage < 80%
    SLADefinition memorySLA;
    memorySLA.slaId = "memory_usage";
    memorySLA.name = "Memory Usage < 80%";
    memorySLA.component = "system";
    memorySLA.metric = "memory_usage";
    memorySLA.targetValue = 80.0;
    memorySLA.acceptableValue = 90.0;
    memorySLA.timeWindow = "1h";
    memorySLA.isActive = true;
    slaDefinitions["memory_usage"] = memorySLA;


}

void PerformanceMonitor::setupDefaultThresholds() {
    // CPU threshold
    PerformanceThreshold cpuThreshold;
    cpuThreshold.component = "system";
    cpuThreshold.metric = "cpu_usage";
    cpuThreshold.warningThreshold = 70.0;
    cpuThreshold.criticalThreshold = 90.0;
    cpuThreshold.isEnabled = true;
    thresholds["system_cpu"] = cpuThreshold;
    
    // Memory threshold
    PerformanceThreshold memThreshold;
    memThreshold.component = "system";
    memThreshold.metric = "memory_usage";
    memThreshold.warningThreshold = 80.0;
    memThreshold.criticalThreshold = 95.0;
    memThreshold.isEnabled = true;
    thresholds["system_memory"] = memThreshold;
    
    // Latency threshold
    PerformanceThreshold latencyThreshold;
    latencyThreshold.component = "ai_execution";
    latencyThreshold.metric = "latency";
    latencyThreshold.warningThreshold = 1000.0;
    latencyThreshold.criticalThreshold = 5000.0;
    latencyThreshold.isEnabled = true;
    thresholds["ai_latency"] = latencyThreshold;


}

void PerformanceMonitor::recordMetric(const std::string& component, const std::string& operation, 
                                     double value, const std::string& unit) {
    MetricData metric;
    metric.metricId = std::string("%1_%2_%3"));
    metric.component = component;
    metric.operation = operation;
    metric.value = value;
    metric.unit = unit;
    metric.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    
    // Store in time-series
    std::string metricKey = std::string("%1_%2");
    metricHistory[metricKey].append(metric);
    
    // Prune old metrics (keep only retention period)
    pruneOldMetrics(metricKey);
    
    // Check thresholds
    checkThreshold(metric);
    
    metricRecorded(metric);
}

void PerformanceMonitor::recordMetricWithTags(const std::string& component, const std::string& operation,
                                             double value, const std::string& unit, const void*& tags) {
    MetricData metric;
    metric.metricId = std::string("%1_%2_%3"));
    metric.component = component;
    metric.operation = operation;
    metric.value = value;
    metric.unit = unit;
    metric.tags = tags;
    metric.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    
    std::string metricKey = std::string("%1_%2");
    metricHistory[metricKey].append(metric);
    
    pruneOldMetrics(metricKey);
    checkThreshold(metric);
    
    metricRecorded(metric);
}

void PerformanceMonitor::startTimer(const std::string& timerId, const std::string& component, 
                                   const std::string& operation) {
    std::chrono::steady_clock timer;
    timer.start();
    
    activeTimers[timerId] = timer;
    timerContext[timerId] = qMakePair(component, operation);
}

double PerformanceMonitor::stopTimer(const std::string& timerId) {
    if (!activeTimers.contains(timerId)) {
        return -1.0;
    }
    
    std::chrono::steady_clock timer = activeTimers[timerId];
    double elapsedMs = timer.elapsed();
    
    // Record metric automatically
    if (timerContext.contains(timerId)) {
        std::pair<std::string, std::string> context = timerContext[timerId];
        recordMetric(context.first, context.second, elapsedMs, "ms");
    }
    
    activeTimers.remove(timerId);
    timerContext.remove(timerId);
    
    return elapsedMs;
}

ScopedTimer PerformanceMonitor::createScopedTimer(const std::string& component, const std::string& operation) {
    return ScopedTimer(this, component, operation);
}

void PerformanceMonitor::checkThreshold(const MetricData& metric) {
    std::string thresholdKey = std::string("%1_%2");
    
    if (!thresholds.contains(thresholdKey)) {
        return;
    }
    
    PerformanceThreshold threshold = thresholds[thresholdKey];
    
    if (!threshold.isEnabled) {
        return;
    }
    
    if (metric.value >= threshold.criticalThreshold) {


        thresholdViolation(metric, "critical");
    } else if (metric.value >= threshold.warningThreshold) {


        thresholdViolation(metric, "warning");
    }
}

void PerformanceMonitor::pruneOldMetrics(const std::string& metricKey) {
    if (!metricHistory.contains(metricKey)) {
        return;
    }
    
    std::chrono::system_clock::time_point cutoff = std::chrono::system_clock::time_point::currentDateTime().addSecs(-metricsRetentionHours * 3600);
    
    std::vector<MetricData>& metrics = metricHistory[metricKey];
    metrics.erase(
        std::remove_if(metrics.begin(), metrics.end(),
                      [&cutoff](const MetricData& m) { return m.timestamp < cutoff; }),
        metrics.end()
    );
}

std::vector<MetricData> PerformanceMonitor::getMetrics(const std::string& component, const std::string& operation,
                                                   const std::chrono::system_clock::time_point& startTime, const std::chrono::system_clock::time_point& endTime) const {
    std::string metricKey = std::string("%1_%2");
    
    if (!metricHistory.contains(metricKey)) {
        return std::vector<MetricData>();
    }
    
    std::vector<MetricData> filtered;
    const std::vector<MetricData>& metrics = metricHistory[metricKey];
    
    for (const MetricData& metric : metrics) {
        if (metric.timestamp >= startTime && metric.timestamp <= endTime) {
            filtered.append(metric);
        }
    }
    
    return filtered;
}

double PerformanceMonitor::getAverageMetric(const std::string& component, const std::string& operation,
                                           const std::chrono::system_clock::time_point& startTime, const std::chrono::system_clock::time_point& endTime) const {
    std::vector<MetricData> metrics = getMetrics(component, operation, startTime, endTime);
    
    if (metrics.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (const MetricData& metric : metrics) {
        sum += metric.value;
    }
    
    return sum / metrics.size();
}

double PerformanceMonitor::getP95Latency(const std::string& component, const std::string& operation) const {
    return getPercentile(component, operation, 95.0);
}

double PerformanceMonitor::getP99Latency(const std::string& component, const std::string& operation) const {
    return getPercentile(component, operation, 99.0);
}

double PerformanceMonitor::getPercentile(const std::string& component, const std::string& operation, 
                                        double percentile) const {
    std::string metricKey = std::string("%1_%2");
    
    if (!metricHistory.contains(metricKey)) {
        return 0.0;
    }
    
    const std::vector<MetricData>& metrics = metricHistory[metricKey];
    
    if (metrics.empty()) {
        return 0.0;
    }
    
    // Extract values and sort
    std::vector<double> values;
    for (const MetricData& metric : metrics) {
        values.append(metric.value);
    }
    
    std::sort(values.begin(), values.end());
    
    // Calculate percentile index
    int index = static_cast<int>((percentile / 100.0) * values.size());
    index = std::min(index, static_cast<int>(values.size() - 1));
    
    return values[index];
}

SLACompliance PerformanceMonitor::evaluateSLA(const std::string& slaId) const {
    SLACompliance compliance;
    compliance.slaId = slaId;
    compliance.isCompliant = false;
    compliance.compliancePercentage = 0.0;
    
    if (!slaDefinitions.contains(slaId)) {
        return compliance;
    }
    
    SLADefinition sla = slaDefinitions[slaId];
    
    if (!sla.isActive) {
        return compliance;
    }
    
    // Get time window
    std::chrono::system_clock::time_point endTime = std::chrono::system_clock::time_point::currentDateTime();
    std::chrono::system_clock::time_point startTime = calculateTimeWindow(endTime, sla.timeWindow);
    
    // Get metrics for this SLA
    std::vector<MetricData> metrics = getMetrics(sla.component, sla.metric, startTime, endTime);
    
    if (metrics.empty()) {
        compliance.currentValue = 0.0;
        compliance.targetValue = sla.targetValue;
        return compliance;
    }
    
    // Calculate compliance based on metric type
    if (sla.metric == "availability") {
        // For availability, calculate uptime percentage
        compliance.currentValue = calculateUptimePercentage(startTime, endTime);
    } else if (sla.metric == "latency_p95") {
        compliance.currentValue = getP95Latency(sla.component, "latency");
    } else if (sla.metric == "error_rate") {
        compliance.currentValue = calculateErrorRate(startTime, endTime);
    } else {
        // For other metrics, use average
        compliance.currentValue = getAverageMetric(sla.component, sla.metric, startTime, endTime);
    }
    
    compliance.targetValue = sla.targetValue;
    
    // Determine compliance (different logic for different metrics)
    if (sla.metric == "availability") {
        // Higher is better
        compliance.isCompliant = (compliance.currentValue >= sla.targetValue);
        compliance.compliancePercentage = (compliance.currentValue / sla.targetValue) * 100.0;
    } else {
        // Lower is better (latency, error rate, memory usage)
        compliance.isCompliant = (compliance.currentValue <= sla.targetValue);
        if (compliance.currentValue > 0) {
            compliance.compliancePercentage = (sla.targetValue / compliance.currentValue) * 100.0;
        } else {
            compliance.compliancePercentage = 100.0;
        }
    }
    
    compliance.compliancePercentage = std::min(100.0, std::max(0.0, compliance.compliancePercentage));
    
    // Count violations
    compliance.violationCount = 0;
    for (const MetricData& metric : metrics) {
        if ((sla.metric == "availability" && metric.value < sla.targetValue) ||
            (sla.metric != "availability" && metric.value > sla.targetValue)) {
            compliance.violationCount++;
            compliance.lastViolation = metric.timestamp;
        }
    }
    
    return compliance;
}

std::vector<SLACompliance> PerformanceMonitor::evaluateAllSLAs() const {
    std::vector<SLACompliance> allCompliance;
    
    for (const std::string& slaId : slaDefinitions.keys()) {
        SLACompliance compliance = evaluateSLA(slaId);
        allCompliance.append(compliance);
    }
    
    return allCompliance;
}

double PerformanceMonitor::calculateUptimePercentage(const std::chrono::system_clock::time_point& startTime, 
                                                     const std::chrono::system_clock::time_point& endTime) const {
    // Calculate total minutes in window
    int64_t totalMinutes = startTime.secsTo(endTime) / 60;
    
    if (totalMinutes <= 0) {
        return 100.0;
    }
    
    // Count downtime minutes (when system had critical errors or was unavailable)
    int64_t downtimeMinutes = 0;
    
    // This would integrate with ErrorRecoverySystem to get actual downtime
    // For now, simulate with performance data
    
    return ((totalMinutes - downtimeMinutes) * 100.0) / totalMinutes;
}

double PerformanceMonitor::calculateErrorRate(const std::chrono::system_clock::time_point& startTime, 
                                              const std::chrono::system_clock::time_point& endTime) const {
    // Get total operations
    std::vector<MetricData> successMetrics = getMetrics("system", "success", startTime, endTime);
    std::vector<MetricData> errorMetrics = getMetrics("system", "error", startTime, endTime);
    
    int totalOps = successMetrics.size() + errorMetrics.size();
    
    if (totalOps == 0) {
        return 0.0;
    }
    
    return (errorMetrics.size() * 100.0) / totalOps;
}

std::chrono::system_clock::time_point PerformanceMonitor::calculateTimeWindow(const std::chrono::system_clock::time_point& endTime, const std::string& window) const {
    if (window == "1h") {
        return endTime.addSecs(-3600);
    } else if (window == "24h") {
        return endTime.addDays(-1);
    } else if (window == "7d") {
        return endTime.addDays(-7);
    } else if (window == "30d") {
        return endTime.addDays(-30);
    }
    
    return endTime.addDays(-1); // Default to 24h
}

PerformanceSnapshot PerformanceMonitor::capturePerformanceSnapshot() {
    PerformanceSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    
    // Capture system metrics (platform-specific)
#ifdef 
    snapshot.cpuUsage = getCPUUsageWindows();
    snapshot.memoryUsage = getMemoryUsageWindows();
#elif defined()
    snapshot.cpuUsage = getCPUUsageLinux();
    snapshot.memoryUsage = getMemoryUsageLinux();
#elif defined()
    snapshot.cpuUsage = getCPUUsageMac();
    snapshot.memoryUsage = getMemoryUsageMac();
#else
    snapshot.cpuUsage = 0.0;
    snapshot.memoryUsage = 0.0;
#endif
    
    snapshot.diskUsage = 0.0; // Would implement disk monitoring
    snapshot.networkUsage = 0.0; // Would implement network monitoring
    snapshot.activeConnections = activeTimers.size();
    
    // Calculate request metrics
    std::string metricKey = "ai_execution_latency";
    if (metricHistory.contains(metricKey)) {
        const std::vector<MetricData>& metrics = metricHistory[metricKey];
        if (!metrics.empty()) {
            double sum = 0.0;
            for (const MetricData& m : metrics) {
                sum += m.value;
            }
            snapshot.averageLatency = sum / metrics.size();
        }
    }
    
    // Calculate requests per second (last minute)
    std::chrono::system_clock::time_point oneMinuteAgo = std::chrono::system_clock::time_point::currentDateTime().addSecs(-60);
    std::vector<MetricData> recentMetrics = getMetrics("system", "request", oneMinuteAgo, snapshot.timestamp);
    snapshot.requestsPerSecond = recentMetrics.size() / 60.0;
    
    // Record snapshot
    performanceHistory.append(snapshot);
    
    // Keep only last 1440 snapshots (24 hours at 1-minute intervals)
    if (performanceHistory.size() > 1440) {
        performanceHistory.removeFirst();
    }
    
    snapshotCaptured(snapshot);
    
    return snapshot;
}

double PerformanceMonitor::getCPUUsageWindows() const {
    // Simplified - would use Windows Performance Counters
    return 0.0;
}

double PerformanceMonitor::getMemoryUsageWindows() const {
    // Simplified - would use GlobalMemoryStatusEx
    return 0.0;
}

double PerformanceMonitor::getCPUUsageLinux() const {
    // Simplified - would parse /proc/stat
    return 0.0;
}

double PerformanceMonitor::getMemoryUsageLinux() const {
    // Simplified - would parse /proc/meminfo
    return 0.0;
}

double PerformanceMonitor::getCPUUsageMac() const {
    // Simplified - would use host_statistics
    return 0.0;
}

double PerformanceMonitor::getMemoryUsageMac() const {
    // Simplified - would use host_statistics
    return 0.0;
}

std::vector<PerformanceSnapshot> PerformanceMonitor::getPerformanceHistory(int minutes) const {
    if (minutes <= 0 || performanceHistory.empty()) {
        return performanceHistory;
    }
    
    std::chrono::system_clock::time_point cutoff = std::chrono::system_clock::time_point::currentDateTime().addSecs(-minutes * 60);
    
    std::vector<PerformanceSnapshot> filtered;
    for (const PerformanceSnapshot& snapshot : performanceHistory) {
        if (snapshot.timestamp >= cutoff) {
            filtered.append(snapshot);
        }
    }
    
    return filtered;
}

std::vector<Bottleneck> PerformanceMonitor::detectBottlenecks() const {
    std::vector<Bottleneck> bottlenecks;
    
    // Check CPU bottleneck
    if (!performanceHistory.empty()) {
        double avgCPU = 0.0;
        for (const PerformanceSnapshot& snapshot : performanceHistory) {
            avgCPU += snapshot.cpuUsage;
        }
        avgCPU /= performanceHistory.size();
        
        if (avgCPU > 80.0) {
            Bottleneck cpuBottleneck;
            cpuBottleneck.bottleneckId = "cpu_bottleneck";
            cpuBottleneck.component = "system";
            cpuBottleneck.type = "cpu";
            cpuBottleneck.severity = (avgCPU > 95.0) ? 10.0 : (avgCPU / 10.0);
            cpuBottleneck.description = std::string("High CPU usage detected: %1%");
            cpuBottleneck.detectedAt = std::chrono::system_clock::time_point::currentDateTime();
            cpuBottleneck.recommendations << "Optimize compute-intensive operations"
                                         << "Consider parallelization"
                                         << "Upgrade CPU or add more cores";
            bottlenecks.append(cpuBottleneck);
        }
        
        // Check memory bottleneck
        double avgMemory = 0.0;
        for (const PerformanceSnapshot& snapshot : performanceHistory) {
            avgMemory += snapshot.memoryUsage;
        }
        avgMemory /= performanceHistory.size();
        
        if (avgMemory > 85.0) {
            Bottleneck memBottleneck;
            memBottleneck.bottleneckId = "memory_bottleneck";
            memBottleneck.component = "system";
            memBottleneck.type = "memory";
            memBottleneck.severity = (avgMemory > 95.0) ? 10.0 : (avgMemory / 10.0);
            memBottleneck.description = std::string("High memory usage detected: %1%");
            memBottleneck.detectedAt = std::chrono::system_clock::time_point::currentDateTime();
            memBottleneck.recommendations << "Implement memory pooling"
                                         << "Reduce cache size"
                                         << "Add more RAM";
            bottlenecks.append(memBottleneck);
        }
    }
    
    // Check latency bottleneck
    double p95Latency = getP95Latency("ai_execution", "latency");
    if (p95Latency > 2000.0) {
        Bottleneck latencyBottleneck;
        latencyBottleneck.bottleneckId = "latency_bottleneck";
        latencyBottleneck.component = "ai_execution";
        latencyBottleneck.type = "io";
        latencyBottleneck.severity = (p95Latency > 5000.0) ? 10.0 : (p95Latency / 500.0);
        latencyBottleneck.description = std::string("High P95 latency detected: %1ms");
        latencyBottleneck.detectedAt = std::chrono::system_clock::time_point::currentDateTime();
        latencyBottleneck.recommendations << "Use local models instead of cloud"
                                         << "Implement request caching"
                                         << "Optimize network connectivity";
        bottlenecks.append(latencyBottleneck);
    }
    
    return bottlenecks;
}

TrendAnalysis PerformanceMonitor::analyzeTrend(const std::string& component, const std::string& operation,
                                              int windowMinutes) const {
    TrendAnalysis analysis;
    analysis.component = component;
    analysis.operation = operation;
    analysis.trend = "stable";
    analysis.changePercentage = 0.0;
    
    std::chrono::system_clock::time_point endTime = std::chrono::system_clock::time_point::currentDateTime();
    std::chrono::system_clock::time_point startTime = endTime.addSecs(-windowMinutes * 60);
    
    std::vector<MetricData> metrics = getMetrics(component, operation, startTime, endTime);
    
    if (metrics.size() < 2) {
        return analysis;
    }
    
    // Calculate average for first and second half
    int midpoint = metrics.size() / 2;
    
    double firstHalfAvg = 0.0;
    for (int i = 0; i < midpoint; ++i) {
        firstHalfAvg += metrics[i].value;
    }
    firstHalfAvg /= midpoint;
    
    double secondHalfAvg = 0.0;
    for (int i = midpoint; i < metrics.size(); ++i) {
        secondHalfAvg += metrics[i].value;
    }
    secondHalfAvg /= (metrics.size() - midpoint);
    
    // Calculate change percentage
    if (firstHalfAvg > 0) {
        analysis.changePercentage = ((secondHalfAvg - firstHalfAvg) / firstHalfAvg) * 100.0;
    }
    
    // Determine trend
    if (analysis.changePercentage > 10.0) {
        analysis.trend = "increasing";
    } else if (analysis.changePercentage < -10.0) {
        analysis.trend = "decreasing";
    } else {
        analysis.trend = "stable";
    }
    
    // Forecast next value (simple linear extrapolation)
    analysis.forecast = secondHalfAvg + (secondHalfAvg - firstHalfAvg);
    analysis.confidence = 0.75; // Moderate confidence
    
    return analysis;
}

bool PerformanceMonitor::exportToPrometheus(const std::string& outputPath) const {
    std::fstream file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        
        return false;
    }
    
    QTextStream out(&file);
    
    // Export all metrics in Prometheus format
    for (auto it = metricHistory.begin(); it != metricHistory.end(); ++it) {
        std::string metricKey = it.key();
        const std::vector<MetricData>& metrics = it.value();
        
        if (metrics.empty()) {
            continue;
        }
        
        // Use most recent metric
        const MetricData& metric = metrics.last();
        
        // Prometheus format: metric_name{labels} value timestamp
        std::string prometheusName = std::string("%1_%2")
                                .replace(" ", "_").replace("-", "_").toLower();
        
        out << prometheusName << "{";
        
        // Add tags as labels
        if (!metric.tags.empty()) {
            std::vector<std::string> labels;
            for (auto tagIt = metric.tags.begin(); tagIt != metric.tags.end(); ++tagIt) {
                labels << std::string("%1=\"%2\"")).toString());
            }
            out << labels.join(",");
        }
        
        out << "} " << metric.value << " " << metric.timestamp.toMSecsSinceEpoch() << "\n";
    }
    
    file.close();


    return true;
}

void PerformanceMonitor::enableMonitoring(bool enable) {
    monitoringEnabled = enable;
    if (enable) {
        snapshotTimer->start();
    } else {
        snapshotTimer->stop();
    }
    
}

void PerformanceMonitor::setSnapshotInterval(int milliseconds) {
    snapshotIntervalMs = milliseconds;
    snapshotTimer->setInterval(milliseconds);
}

void PerformanceMonitor::setMetricsRetention(int hours) {
    metricsRetentionHours = hours;
}

void PerformanceMonitor::clearMetrics() {
    metricHistory.clear();
    performanceHistory.clear();
    
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(PerformanceMonitor* monitor, const std::string& component, const std::string& operation)
    : performanceMonitor(monitor), component(component), operation(operation) {
    timerId = std::string("%1_%2_%3"));
    performanceMonitor->startTimer(timerId, component, operation);
}

ScopedTimer::~ScopedTimer() {
    performanceMonitor->stopTimer(timerId);
}


