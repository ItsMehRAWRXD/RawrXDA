# Phase 5 Model Router Extension - QUICK REFERENCE

## 📋 Component Overview

| Component | LOC | Purpose |
|-----------|-----|---------|
| **ModelRouterExtension** | 1,520+ | Intelligent routing with circuit breakers |
| **LoadBalancingStrategies** | 1,075+ | 12 algorithms including ML-based |
| **ModelPoolManager** | 930+ | Instance pooling with auto-scaling |
| **RoutingAnalytics** | 1,070+ | Analytics, A/B testing, cost tracking |
| **TOTAL** | **4,395+** | Complete model routing infrastructure |

**Status**: ✅ Fully Complete | ✅ CMake Integrated | ✅ Zero Stubs

---

## 🚀 Quick Start

### 1. Basic Setup
```cpp
#include "ModelRouterExtension.h"

// Create router
ModelRouterExtension* router = new ModelRouterExtension(parent);

// Register endpoint
ModelEndpoint ep;
ep.id = "gpt4";
ep.name = "GPT-4 Turbo";
ep.type = "cloud";
ep.weight = 70;
ep.maxConcurrentRequests = 100;
router->registerEndpoint(ep);

// Set strategy
router->setRoutingStrategy(RoutingStrategy::Adaptive);

// Enable health monitoring (every 60s)
router->enableHealthMonitoring(60000);
```

### 2. Route a Request
```cpp
RoutingRequest req;
req.requestId = QUuid::createUuid().toString();
req.prompt = "Your prompt here";
req.maxTokens = 500;

RoutingDecision decision = router->routeRequest(req);
qDebug() << "Using endpoint:" << decision.endpointId;
```

### 3. Track Analytics
```cpp
#include "RoutingAnalytics.h"

RoutingAnalytics* analytics = new RoutingAnalytics(parent);

// Track request
analytics->trackRequest(req, decision);

// After completion
analytics->recordRequestCompletion(req.requestId, 
    1.23,    // Response time (s)
    487,     // Tokens generated
    true);   // Success

// Track cost (in microdollars)
analytics->trackRequestCost(req.requestId, 2000);
```

---

## 🎯 Routing Strategies

| Strategy | Use Case | Key Benefit |
|----------|----------|-------------|
| **RoundRobin** | Equal distribution | Simple, fair |
| **WeightedRandom** | Capacity-aware | Honors endpoint weights |
| **LeastConnections** | Uneven load | Routes to least busy |
| **ResponseTimeBased** | Latency-sensitive | Routes to fastest |
| **Adaptive** | Production | ML scoring (health+load+cost) |
| **PriorityBased** | Tiered endpoints | Honors priority levels |
| **CostOptimized** | Budget-constrained | Minimizes cost |
| **Custom** | Special needs | User-defined logic |

### Setting Strategy
```cpp
router->setRoutingStrategy(RoutingStrategy::Adaptive);
```

---

## ⚖️ Load Balancing Algorithms

| Algorithm | Complexity | Best For |
|-----------|-----------|----------|
| **RoundRobin** | O(1) | Simple distribution |
| **WeightedRoundRobin** | O(1) | Capacity-aware (nginx algorithm) |
| **Random** | O(1) | Stateless routing |
| **WeightedRandom** | O(n) | Weighted stateless |
| **LeastConnections** | O(n) | Uneven request durations |
| **LeastResponseTime** | O(n) | Latency optimization |
| **PowerOfTwoChoices** | O(1) | O(1) with good balance |
| **ConsistentHashing** | O(log n) | Request affinity |
| **IpHash** | O(1) | Client-based routing |
| **LeastLoad** | O(n) | Load-aware |
| **AdaptiveMLBased** | O(h×i) | ML-optimized |

### Setting Algorithm
```cpp
#include "LoadBalancingStrategies.h"

LoadBalancingStrategies* balancer = new LoadBalancingStrategies(parent);
balancer->setAlgorithm(LoadBalancingAlgorithm::PowerOfTwoChoices);
```

### ML-Based Routing
```cpp
// Initialize ML model
balancer->initializeMLModel(7);  // 7 input features

// Train after request completion
QVector<double> features = {
    requestSize, latency, load, 
    timeOfDay, dayOfWeek, queueDepth, errorRate
};
double reward = success ? 1.0 : 0.0;
balancer->trainMLModel(endpointId, features, reward);
```

---

## 🏊 Instance Pooling

### Basic Usage
```cpp
#include "ModelPoolManager.h"

ModelPoolManager* pool = new ModelPoolManager(parent);

// Create instance
QString instanceId = pool->createInstance("endpoint1", "/path/to/model.gguf");

// Initialize (load model)
pool->initializeInstance(instanceId);

// Acquire for request (blocks until available)
QString acquiredId = pool->acquireInstance("endpoint1", 5000);  // 5s timeout
if (!acquiredId.isEmpty()) {
    // Use instance...
    
    // Release back to pool
    pool->releaseInstance(acquiredId);
}
```

### Auto-Scaling
```cpp
// Enable auto-scaling
pool->enableAutoScaling(true);

// Set thresholds (scale up > 75%, down < 25%)
pool->setScaleThresholds(0.75, 0.25);

// Set limits
pool->setMinMaxInstances(2, 10);

// Auto-scaling happens automatically via QTimer
```

### Pre-Warming
```cpp
// Pre-warm instances on startup
pool->preWarmInstances("endpoint1", 3);  // Warm 3 instances

// Schedule warm-up
pool->scheduleWarmUp("endpoint1", 5000);  // Warm in 5 seconds
```

---

## 📊 Analytics & Reporting

### Performance Metrics
```cpp
// Get endpoint metrics
EndpointMetrics metrics = analytics->getEndpointMetrics("gpt4");
qDebug() << "Total requests:" << metrics.totalRequests;
qDebug() << "Success rate:" << metrics.successRate;
qDebug() << "Avg response time:" << metrics.averageResponseTime;
qDebug() << "p95 latency:" << metrics.p95ResponseTime;

// Top performers
QList<QString> topEndpoints = analytics->getTopEndpoints(5);
```

### Trend Analysis
```cpp
// Performance trend (last 24 hours)
QVector<double> perfTrend = analytics->getPerformanceTrend("gpt4", 24);

// Throughput trend (requests per hour)
QVector<qint64> throughput = analytics->getThroughputTrend("gpt4", 24);
```

### A/B Testing
```cpp
// Start A/B test
ABTestConfig test;
test.testId = QUuid::createUuid().toString();
test.variantA = "gpt4-endpoint";
test.variantB = "claude-endpoint";
test.trafficSplitA = 0.5;
test.trafficSplitB = 0.5;
test.hypothesis = "Testing latency difference";

QString testId = analytics->startABTest(test);

// Route using A/B test
QString endpoint = analytics->routeForABTest(testId, request);

// Analyze results
ABTestResults results = analytics->analyzeABTest(testId);
qDebug() << "Winner:" << results.winningVariant;
qDebug() << "Confidence:" << results.confidenceLevel;
qDebug() << "Conclusion:" << results.conclusion;

// Stop test
analytics->stopABTest(testId);
```

### Cost Analysis
```cpp
// Get cost report
QDateTime start = QDateTime::currentDateTime().addDays(-7);
QDateTime end = QDateTime::currentDateTime();

CostAnalysisReport report = analytics->getCostAnalysis("gpt4", start, end);
qDebug() << "Total cost:" << report.totalCost;
qDebug() << "Avg cost/request:" << report.averageCostPerRequest;

// Forecast cost (next 7 days)
qint64 forecast = analytics->forecastCost("gpt4", 168);

// Cost by endpoint
QMap<QString, qint64> costs = analytics->getCostByEndpoint(start, end);
```

### Generate Reports
```cpp
// Performance report
QJsonObject perfReport = analytics->generatePerformanceReport();

// Cost report
QJsonObject costReport = analytics->generateCostReport(start, end);

// Comparative report
QJsonObject comparison = analytics->generateComparativeReport(
    {"gpt4-endpoint", "claude-endpoint", "local-llama"}
);
```

---

## 🛡️ Circuit Breakers

Circuit breakers prevent cascading failures by blocking requests to unhealthy endpoints.

### States
- **Closed**: Normal operation (requests allowed)
- **Open**: Too many failures (requests blocked, cooldown active)
- **HalfOpen**: Testing recovery (limited requests)

### Configuration
```cpp
// Circuit breaker triggers on 5 failures, cooldown 30s, 3 half-open attempts
CircuitBreaker breaker(5, 30000, 3);

// Check if request allowed
if (breaker.allowRequest()) {
    // Execute request
    if (success) {
        breaker.recordSuccess();
    } else {
        breaker.recordFailure();
    }
}

// Manual reset
breaker.reset();
```

---

## 🏥 Health Monitoring

### Automatic Health Checks
```cpp
// Enable automatic health monitoring (every 60 seconds)
router->enableHealthMonitoring(60000);

// Disable
router->disableHealthMonitoring();
```

### Manual Health Checks
```cpp
// Check single endpoint
router->checkEndpointHealth("gpt4");

// Check all endpoints
router->checkAllEndpointsHealth();

// Get endpoint health
ModelEndpoint ep = router->getEndpoint("gpt4");
qDebug() << "Health score:" << ep.healthScore;  // 0.0 - 1.0
```

---

## 🔧 Configuration Management

### Save/Load Configuration
```cpp
// Save configuration
QJsonObject config = router->saveConfiguration();
QFile file("router_config.json");
file.open(QIODevice::WriteOnly);
file.write(QJsonDocument(config).toJson());
file.close();

// Load configuration
QFile loadFile("router_config.json");
loadFile.open(QIODevice::ReadOnly);
QJsonDocument doc = QJsonDocument::fromJson(loadFile.readAll());
router->loadConfiguration(doc.object());
```

---

## 🧹 Data Management

### Clear Old Data
```cpp
// Keep last 30 days
analytics->clearOldData(30);

// Reset all metrics
analytics->resetMetrics();

// Check data size
qint64 sizeBytes = analytics->getDataSize();
```

---

## 🐛 Common Patterns

### Pattern 1: Production Routing Pipeline
```cpp
// Setup
ModelRouterExtension* router = new ModelRouterExtension(parent);
LoadBalancingStrategies* balancer = new LoadBalancingStrategies(parent);
ModelPoolManager* pool = new ModelPoolManager(parent);
RoutingAnalytics* analytics = new RoutingAnalytics(parent);

router->setRoutingStrategy(RoutingStrategy::Adaptive);
balancer->setAlgorithm(LoadBalancingAlgorithm::AdaptiveMLBased);
pool->enableAutoScaling(true);
router->enableHealthMonitoring(60000);

// Request
RoutingRequest req;
req.requestId = QUuid::createUuid().toString();
req.prompt = userPrompt;

RoutingDecision decision = router->routeRequest(req);
QString instanceId = pool->acquireInstance(decision.endpointId, 5000);

analytics->trackRequest(req, decision);

// Execute inference
double responseTime = executeInference(instanceId, req.prompt);

pool->releaseInstance(instanceId);
analytics->recordRequestCompletion(req.requestId, responseTime, tokens, true);
analytics->trackRequestCost(req.requestId, calculateCost(tokens));
```

### Pattern 2: Cost-Aware Routing
```cpp
// Check recent costs
QDateTime start = QDateTime::currentDateTime().addDays(-1);
qint64 dailyCost = analytics->getTotalCost(start, QDateTime::currentDateTime());

if (dailyCost > budgetThreshold) {
    // Switch to cost-optimized routing
    router->setRoutingStrategy(RoutingStrategy::CostOptimized);
    qDebug() << "Switched to cost-optimized routing (budget exceeded)";
}
```

### Pattern 3: Fallback Chains
```cpp
// Automatic fallback
RoutingDecision decision = router->routeWithFallback(req);

if (decision.alternativeEndpoints.isEmpty()) {
    qWarning() << "No alternatives available!";
} else {
    qDebug() << "Alternatives:" << decision.alternativeEndpoints;
}
```

---

## 📁 File Locations

```
src/qtapp/model_router/
├── ModelRouterExtension.h       (420 LOC)
├── ModelRouterExtension.cpp     (1,100 LOC)
├── LoadBalancingStrategies.h    (325 LOC)
├── LoadBalancingStrategies.cpp  (750 LOC)
├── ModelPoolManager.h           (280 LOC)
├── ModelPoolManager.cpp         (650 LOC)
├── RoutingAnalytics.h           (220 LOC)
└── RoutingAnalytics.cpp         (850 LOC)
```

---

## 🔗 Dependencies

- Qt6 Core (QObject, QMutex, QTimer, QDateTime, QUuid)
- Qt6 Network (for endpoint communication)
- InferenceEngine (existing RawrXD component)

---

## 🚨 Troubleshooting

| Issue | Solution |
|-------|----------|
| Circuit breaker stuck Open | Increase cooldown or manually reset: `breaker.reset()` |
| Auto-scaling thrashing | Increase cooldown periods in `PoolConfiguration` |
| ML routing not learning | Ensure `trainMLModel()` called after each request |
| High memory usage | Call `analytics->clearOldData(7)` periodically |
| Endpoints always unhealthy | Check health scoring thresholds and network connectivity |

---

## ✅ Build Integration

CMake automatically detects and includes Phase 5 Extension:
```cmake
if(EXISTS "${CMAKE_SOURCE_DIR}/src/qtapp/model_router/ModelRouterExtension.cpp")
    list(APPEND AGENTICIDE_SOURCES
        src/qtapp/model_router/ModelRouterExtension.cpp
        src/qtapp/model_router/LoadBalancingStrategies.cpp
        src/qtapp/model_router/ModelPoolManager.cpp
        src/qtapp/model_router/RoutingAnalytics.cpp)
endif()
```

Verify with: `cmake ..` in build directory.

---

## 🎓 Key Takeaways

1. **Adaptive strategy** + **ML-based balancing** = production-ready routing
2. **Circuit breakers** prevent cascading failures
3. **Auto-scaling pools** handle variable load
4. **A/B testing** enables data-driven decisions
5. **Cost tracking** ensures budget compliance
6. All components are **thread-safe** and **Qt-integrated**

---

**Phase 5 Extension: Enterprise-grade model routing for RawrXD-AgenticIDE** 🚀
