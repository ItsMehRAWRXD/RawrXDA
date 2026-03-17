# Tier 2 Production Systems - Quick Reference

## Quick Start (5 minutes)

### 1. Include Headers
```cpp
#include "tier2_production_integration.h"
```

### 2. Initialize
```cpp
using namespace Tier2Integration;

Tier2Config config;
config.serviceName = "MyService";
config.modelsDirectory = "./models";
config.mlModelsPath = "./ml_models";

ProductionTier2Manager tier2Manager(this);
tier2Manager.initialize(config);
```

### 3. Use Features

#### Error Detection
```cpp
// Report error
QJsonObject context;
context["file"] = __FILE__;
context["line"] = __LINE__;
QString errorId = tier2Manager.reportError("Error message", context);

// Get diagnosis
QJsonObject diagnosis = tier2Manager.diagnoseError(errorId);
QStringList fixes = tier2Manager.suggestFixes(errorId);

// Learn from resolution
tier2Manager.learnFromError("Error message", "Runtime", context, true);
```

#### Model Hotpatching
```cpp
// Register model
tier2Manager.registerModel("/path/to/model.gguf");

// Swap model (zero downtime)
tier2Manager.swapModel("model_id", true);

// Canary deployment
ModelHotpatch::CanaryConfig canary;
canary.testModelId = "new_model";
canary.trafficPercentage = 0.1;  // 10% traffic
canary.minRequests = 100;
canary.successThreshold = 0.95;
tier2Manager.startCanaryDeployment(canary);

// Get canary metrics
QJsonObject metrics = tier2Manager.getCanaryMetrics();
```

#### Performance Monitoring
```cpp
// Record metrics
tier2Manager.recordMetric("inference", "completion", 234.5, "ms");

// Get performance report
QJsonObject report = tier2Manager.getPerformanceReport();
qDebug() << "SLA Compliance:" << report["sla_compliance"];
qDebug() << "Bottlenecks:" << report["bottlenecks"];
```

#### Distributed Tracing
```cpp
// Create trace span
auto span = tier2Manager.startTrace("process_request");

// Add context
span.addTag("user_id", "123");
span.addTag("request_type", "completion");

// Do work...
QString result = processRequest();

// Finish trace
tier2Manager.finishTrace(span);

// Analyze trace
QJsonObject analysis = tier2Manager.analyzeTrace(span.traceId);
QJsonArray bottlenecks = tier2Manager.findTraceBottlenecks(span.traceId);
```

## Advanced Usage

### ML Error Detection (Direct Access)
```cpp
#include "ai/ml_error_detector.h"

using namespace MLErrorDetection;

MLErrorDetector detector;
detector.initialize("./ml_models");

// Detect anomaly
QJsonObject context;
context["severity"] = "high";
auto result = detector.detectAnomaly("Error message", context);

if (result.isAnomaly) {
    qDebug() << "Anomaly Score:" << result.anomalyScore;
    qDebug() << "Method:" << result.detectionMethod;
}

// Predict error category
auto prediction = detector.predictErrorCategory("Error message", context);
qDebug() << "Category:" << prediction.predictedCategory;
qDebug() << "Confidence:" << prediction.confidence;

// Train with batch data
QVector<QPair<ErrorFeatureVector, QString>> trainingData;
// ... populate training data ...
detector.trainModelBatch(trainingData);

// Save model
detector.saveModel("./ml_models/error_model.json");
```

### Model Hotpatching (Direct Access)
```cpp
#include "model_loader/model_hotpatch_manager.h"

using namespace ModelHotpatch;

ModelHotpatchManager hotpatch;
hotpatch.initialize("./models");

// Register multiple models
hotpatch.registerModel("/models/llama-7b.gguf", {{"type", "gguf"}});
hotpatch.registerModel("/models/llama-13b.gguf", {{"type", "gguf"}});

// Atomic swap
auto result = hotpatch.swapModel("llama-13b", true);
if (result.success) {
    qDebug() << "Swapped in" << result.swapDurationMs << "ms";
} else {
    qWarning() << "Swap failed:" << result.errorMessage;
}

// Get performance metrics
auto metrics = hotpatch.getPerformanceMetrics("llama-13b");
qDebug() << "Average Latency:" << metrics.averageLatencyMs;
qDebug() << "P95 Latency:" << metrics.p95LatencyMs;
qDebug() << "Error Rate:" << metrics.errorRate;

// Find best model
QString bestModel = hotpatch.getBestPerformingModel();
qDebug() << "Best performing model:" << bestModel;
```

### Performance Monitoring (Direct Access)
```cpp
#include "performance_monitor.h"

PerformanceMonitor monitor;

// Scoped timing (RAII)
{
    ScopedTimer timer = monitor.createScopedTimer("inference", "completion");
    // ... do work ...
    // Timer automatically recorded on scope exit
}

// Manual timing
QString timerId = "my_operation";
monitor.startTimer(timerId, "service", "operation");
// ... do work ...
double elapsed = monitor.stopTimer(timerId);

// Get metrics
QDateTime start = QDateTime::currentDateTime().addSecs(-3600);
auto metrics = monitor.getMetrics("service", "operation", start);

// Calculate statistics
double avg = monitor.getAverageMetric("service", "operation");
double p95 = monitor.getP95Latency("service", "operation");
double p99 = monitor.getP99Latency("service", "operation");

// SLA evaluation
auto compliance = monitor.evaluateSLA("uptime_999");
if (!compliance.isCompliant) {
    qWarning() << "SLA violation!" << compliance.slaId;
    qWarning() << "Current:" << compliance.currentValue;
    qWarning() << "Target:" << compliance.targetValue;
}

// Export to Prometheus
monitor.exportToPrometheus("./metrics/prometheus.txt");
```

### Distributed Tracing (Direct Access)
```cpp
#include "monitoring/distributed_tracing_system.h"

using namespace Tracing;

DistributedTracingSystem tracer;
tracer.initialize("MyService", "1.0.0");
tracer.setSamplingRate(0.1);  // 10% sampling

// Create span with builder
SpanBuilder builder = tracer.buildSpan("database_query");
builder.addTag("query_type", "SELECT");
builder.addTag("table", "users");
builder.addBaggage("request_id", "req-123");
Span span = builder.start();

// ... do work ...

tracer.finishSpan(span);

// Retrieve trace
Trace trace = tracer.getTrace(span.traceId);
qDebug() << "Total duration:" << trace.totalDurationMicros;
qDebug() << "Span count:" << trace.spanCount();

// Critical path analysis
auto criticalPath = tracer.getCriticalPath(span.traceId);
double criticalDuration = tracer.calculateCriticalPathDuration(span.traceId);

// Find bottlenecks
QJsonArray bottlenecks = tracer.findBottlenecks(span.traceId);

// Export traces
tracer.exportToJaeger("./traces/jaeger.json");
tracer.exportToZipkin("./traces/zipkin.json");
```

## Configuration Reference

### Tier2Config Complete Options
```cpp
Tier2Config config;

// Service identification
config.serviceName = "MyService";
config.serviceVersion = "1.0.0";

// Feature flags
config.enableMLErrorDetection = true;
config.enableErrorAnalysis = true;
config.enableModelHotpatching = true;
config.enablePerformanceMonitoring = true;
config.enableDistributedTracing = true;

// Paths
config.mlModelsPath = "./ml_models";
config.modelsDirectory = "./models";
config.metricsExportPath = "./metrics";

// ML Error Detection settings
config.errorDetectionSamplingRate = 1.0;  // 100% sampling

// Model Hotpatching settings
config.maxConcurrentModelLoads = 3;
config.modelValidationTimeoutSec = 60;

// Performance Monitoring settings
config.performanceSnapshotIntervalMs = 60000;  // 1 minute
config.metricsRetentionHours = 24;

// Distributed Tracing settings
config.tracingSamplingRate = 0.1;  // 10% sampling

// General settings
config.healthCheckIntervalSec = 30;
config.exportMetricsOnShutdown = true;
```

## Signal/Slot Connections

### Error Detection Signals
```cpp
connect(&tier2Manager, &ProductionTier2Manager::anomalyDetected,
        this, [](double score, QString method) {
    qWarning() << "Anomaly!" << method << score;
});

connect(&tier2Manager, &ProductionTier2Manager::criticalErrorDetected,
        this, [](QString errorId, QString message) {
    qCritical() << "CRITICAL:" << message;
});
```

### Model Hotpatching Signals
```cpp
connect(&tier2Manager, &ProductionTier2Manager::modelSwapped,
        this, [](QString newId, QString oldId) {
    qInfo() << "Model swapped:" << oldId << "->" << newId;
});

connect(&tier2Manager, &ProductionTier2Manager::canaryPromoted,
        this, [](QString modelId) {
    qInfo() << "Canary promoted:" << modelId;
});
```

### Performance Signals
```cpp
connect(&tier2Manager, &ProductionTier2Manager::performanceThresholdViolated,
        this, [](QString component, QString metric, double value) {
    qWarning() << "Threshold violated:" << component << metric << value;
});

connect(&tier2Manager, &ProductionTier2Manager::bottleneckDetected,
        this, [](QString component, double latency) {
    qWarning() << "Bottleneck:" << component << latency << "ms";
});
```

## Best Practices

### 1. Always Initialize Before Use
```cpp
Tier2Config config;
ProductionTier2Manager tier2;
if (!tier2.initialize(config)) {
    qCritical() << "Failed to initialize Tier 2 systems";
    return;
}
```

### 2. Use RAII for Tracing
```cpp
void myFunction() {
    auto span = tier2Manager.startTrace("my_function");
    
    // Work happens here
    
    tier2Manager.finishTrace(span);  // Automatic in destructor
}
```

### 3. Record All Errors
```cpp
try {
    riskyOperation();
} catch (const std::exception& e) {
    QJsonObject context;
    context["exception_type"] = typeid(e).name();
    tier2Manager.reportError(e.what(), context);
    throw;  // Re-throw after reporting
}
```

### 4. Validate Before Swapping
```cpp
// Always validate production models
bool success = tier2Manager.swapModel("new_model", true);  // true = validate
```

### 5. Export Metrics Regularly
```cpp
// In shutdown or on signal
tier2Manager.exportAllMetrics("./backup/metrics");
```

## Troubleshooting

### Issue: Model swap fails
```cpp
// Check model is registered
auto models = tier2Manager.listModels();
for (const auto& model : models) {
    qDebug() << model.modelId << model.modelName;
}

// Check current model
QString activeModel = tier2Manager.getActiveModelId();
qDebug() << "Active model:" << activeModel;
```

### Issue: High error rate
```cpp
// Get error statistics
QJsonObject errorStats = tier2Manager.getErrorStatistics();
qDebug() << "Total errors:" << errorStats["total_errors"];
qDebug() << "Error rate:" << errorStats["error_rate"];

// Get ML detector stats
auto mlStats = // from direct access
qDebug() << "Anomaly detection rate:" << mlStats["anomaly_rate"];
```

### Issue: Performance degradation
```cpp
// Get performance report
QJsonObject perfReport = tier2Manager.getPerformanceReport();

// Check SLA compliance
QJsonArray slas = perfReport["sla_compliance"].toArray();
for (const QJsonValue& sla : slas) {
    QJsonObject slaObj = sla.toObject();
    if (!slaObj["compliant"].toBool()) {
        qWarning() << "SLA violation:" << slaObj["sla_id"];
    }
}

// Find bottlenecks
QJsonArray bottlenecks = perfReport["bottlenecks"].toArray();
for (const QJsonValue& bottleneck : bottlenecks) {
    qWarning() << "Bottleneck:" << bottleneck.toObject();
}
```

## Performance Tips

1. **Use sampling for high-volume tracing** (10% is good default)
2. **Batch model training** instead of per-error learning
3. **Set appropriate metrics retention** (24h usually sufficient)
4. **Enable canary deployments** for safe model updates
5. **Monitor SLA compliance** to catch degradation early

## Support

For issues or questions:
- Check logs in structured format
- Export all metrics for analysis
- Run health checks: `tier2Manager.runHealthChecks()`
- Get system status: `tier2Manager.getSystemStatus()`
