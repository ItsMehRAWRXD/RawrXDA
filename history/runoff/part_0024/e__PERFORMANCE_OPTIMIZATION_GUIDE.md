# Universal Model Router - Performance Optimization Guide

## Overview

This guide provides benchmarking methodologies, optimization strategies, and performance targets for the Universal Model Router system.

---

## Performance Targets

### Latency Targets

| Model Type | P50 | P95 | P99 | Target |
|-----------|-----|-----|-----|--------|
| **Local GGUF** | 10ms | 25ms | 50ms | <100ms |
| **Ollama Local** | 20ms | 50ms | 100ms | <200ms |
| **Cloud (US)** | 200ms | 500ms | 1000ms | <1.5s |
| **Cloud (EU)** | 250ms | 600ms | 1500ms | <2s |
| **Streaming (first chunk)** | 50ms | 200ms | 500ms | <1s |

### Throughput Targets

| Operation | Target | Notes |
|-----------|--------|-------|
| **Sequential Requests** | 1-2 req/sec | Limited by cloud APIs |
| **Batch Requests** | 10-50 req/min | Combined into single API call |
| **Concurrent Async** | 10+ simultaneous | Qt signal/slot based |
| **Streaming** | 50-200 chunks/sec | Depends on network speed |

### Resource Usage

| Resource | Target | Notes |
|----------|--------|-------|
| **Memory (idle)** | <50MB | ModelInterface + Router + Config |
| **Memory (active)** | <200MB | Per 10 concurrent requests |
| **CPU (idle)** | <1% | Single thread |
| **CPU (generating)** | 10-50% | Depends on task |
| **Network (bandwidth)** | 10-50 Mbps | Typical cloud API usage |

### Accuracy & Success

| Metric | Target | Warning | Critical |
|--------|--------|---------|----------|
| **Success Rate** | >99% | <95% | <90% |
| **API Error Rate** | <1% | >5% | >10% |
| **Timeout Rate** | <0.5% | >2% | >5% |
| **Cost Accuracy** | 100% ± 0.1% | >1% error | >5% error |

---

## Benchmarking Framework

### Setup

```cpp
#include <chrono>
#include <iomanip>
#include "model_interface.h"

struct BenchmarkResult {
    QString model_name;
    int iterations = 0;
    double avg_latency_ms = 0;
    double min_latency_ms = 0;
    double max_latency_ms = 0;
    double p95_latency_ms = 0;
    double p99_latency_ms = 0;
    int success_count = 0;
    double success_rate = 0;
    double total_cost = 0;
    
    void print() {
        cout << "\n=== " << model_name.toStdString() << " ===" << endl;
        cout << "Iterations: " << iterations << endl;
        cout << "Avg: " << fixed << setprecision(1) << avg_latency_ms << "ms" << endl;
        cout << "Min: " << min_latency_ms << "ms" << endl;
        cout << "Max: " << max_latency_ms << "ms" << endl;
        cout << "P95: " << p95_latency_ms << "ms" << endl;
        cout << "P99: " << p99_latency_ms << "ms" << endl;
        cout << "Success: " << fixed << setprecision(1) << success_rate << "%" << endl;
        cout << "Cost: $" << fixed << setprecision(4) << total_cost << endl;
    }
};
```

### Benchmark 1: Latency Benchmarking

```cpp
BenchmarkResult benchmark_latency(
    const QString& model_name,
    const QString& prompt,
    int iterations = 10
) {
    BenchmarkResult result;
    result.model_name = model_name;
    result.iterations = iterations;
    
    QVector<double> latencies;
    
    ModelInterface ai;
    ai.initialize("model_config.json");
    
    cout << "Benchmarking " << model_name.toStdString() << "..." << endl;
    
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto res = ai.generate(prompt, model_name);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        
        latencies.append(duration);
        result.total_cost += res.metadata["cost"].toDouble();
        
        if (res.success) {
            result.success_count++;
        }
        
        cout << "." << flush;
    }
    cout << endl;
    
    // Calculate statistics
    sort(latencies.begin(), latencies.end());
    
    result.min_latency_ms = latencies.first();
    result.max_latency_ms = latencies.last();
    result.success_rate = (double)result.success_count / iterations * 100;
    
    double sum = 0;
    for (auto lat : latencies) {
        sum += lat;
    }
    result.avg_latency_ms = sum / iterations;
    
    // Percentiles
    int p95_idx = (int)(0.95 * iterations);
    int p99_idx = (int)(0.99 * iterations);
    result.p95_latency_ms = latencies[p95_idx];
    result.p99_latency_ms = latencies[p99_idx];
    
    return result;
}
```

### Benchmark 2: Throughput Benchmarking

```cpp
struct ThroughputResult {
    QString model_name;
    int total_requests = 0;
    double duration_seconds = 0;
    double requests_per_second = 0;
    double total_cost = 0;
    int success_count = 0;
    
    void print() {
        cout << "\n=== Throughput: " << model_name.toStdString() << " ===" << endl;
        cout << "Total requests: " << total_requests << endl;
        cout << "Duration: " << fixed << setprecision(1) << duration_seconds << "s" << endl;
        cout << "Throughput: " << requests_per_second << " req/s" << endl;
        cout << "Cost: $" << total_cost << endl;
    }
};

ThroughputResult benchmark_throughput(
    const QString& model_name,
    const QString& prompt,
    int duration_seconds = 30
) {
    ThroughputResult result;
    result.model_name = model_name;
    
    ModelInterface ai;
    ai.initialize("model_config.json");
    
    auto start = chrono::high_resolution_clock::now();
    auto end_time = start + chrono::seconds(duration_seconds);
    
    while (chrono::high_resolution_clock::now() < end_time) {
        auto res = ai.generate(prompt, model_name);
        
        result.total_requests++;
        result.total_cost += res.metadata["cost"].toDouble();
        if (res.success) result.success_count++;
        
        cout << ".";
        cout.flush();
    }
    
    auto elapsed = chrono::high_resolution_clock::now() - start;
    result.duration_seconds = chrono::duration_cast<chrono::milliseconds>(elapsed).count() / 1000.0;
    result.requests_per_second = result.total_requests / result.duration_seconds;
    
    return result;
}
```

### Benchmark 3: Memory Benchmarking

```cpp
#include <QProcess>

struct MemoryResult {
    QString model_name;
    long memory_initial_kb = 0;
    long memory_peak_kb = 0;
    long memory_delta_kb = 0;
    
    void print() {
        cout << "\n=== Memory: " << model_name.toStdString() << " ===" << endl;
        cout << "Initial: " << memory_initial_kb << " KB" << endl;
        cout << "Peak: " << memory_peak_kb << " KB" << endl;
        cout << "Delta: " << memory_delta_kb << " KB" << endl;
    }
};

long get_memory_usage() {
    // Platform-specific implementation
    #ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.WorkingSetSize / 1024;  // In KB
    #else
        // Linux: read /proc/self/status
        // macOS: use task_info
    #endif
    return 0;
}

MemoryResult benchmark_memory(
    const QString& model_name,
    int iterations = 100
) {
    MemoryResult result;
    result.model_name = model_name;
    result.memory_initial_kb = get_memory_usage();
    result.memory_peak_kb = result.memory_initial_kb;
    
    ModelInterface ai;
    ai.initialize("model_config.json");
    
    for (int i = 0; i < iterations; ++i) {
        auto res = ai.generate("test", model_name);
        
        long current = get_memory_usage();
        result.memory_peak_kb = max(result.memory_peak_kb, current);
        
        cout << ".";
        cout.flush();
    }
    
    result.memory_delta_kb = result.memory_peak_kb - result.memory_initial_kb;
    
    return result;
}
```

### Benchmark 4: Cost Accuracy

```cpp
struct CostResult {
    QString model_name;
    int total_requests = 0;
    double total_reported_cost = 0;
    double total_expected_cost = 0;
    double accuracy_percent = 0;
    
    void print() {
        cout << "\n=== Cost Accuracy: " << model_name.toStdString() << " ===" << endl;
        cout << "Requests: " << total_requests << endl;
        cout << "Reported: $" << fixed << setprecision(4) << total_reported_cost << endl;
        cout << "Expected: $" << total_expected_cost << endl;
        cout << "Accuracy: " << accuracy_percent << "%" << endl;
    }
};

CostResult benchmark_cost(
    const QString& model_name,
    int iterations = 50
) {
    CostResult result;
    result.model_name = model_name;
    result.total_requests = iterations;
    
    ModelInterface ai;
    ai.initialize("model_config.json");
    
    // Provider pricing (as of 2024)
    map<QString, pair<double, double>> pricing = {
        {"gpt-4", {0.00003, 0.00006}},  // Input, Output per token
        {"gpt-3.5-turbo", {0.0000005, 0.0000015}},
        {"claude-3-opus", {0.000015, 0.000075}},
        // ... etc
    };
    
    for (int i = 0; i < iterations; ++i) {
        auto res = ai.generate("Test prompt", model_name);
        
        result.total_reported_cost += res.metadata["cost"].toDouble();
        
        // Calculate expected cost based on token count and pricing
        int tokens = res.metadata["tokens_used"].toInt();
        if (pricing.count(model_name)) {
            double cost_per_token = pricing[model_name].second;
            result.total_expected_cost += tokens * cost_per_token;
        }
        
        cout << ".";
        cout.flush();
    }
    
    if (result.total_expected_cost > 0) {
        result.accuracy_percent = 
            (1.0 - fabs(result.total_reported_cost - result.total_expected_cost) 
                / result.total_expected_cost) * 100;
    }
    
    return result;
}
```

---

## Run Complete Benchmark Suite

```cpp
void run_all_benchmarks() {
    cout << "\n╔════════════════════════════════════╗\n";
    cout << "║  UNIVERSAL MODEL ROUTER BENCHMARK  ║\n";
    cout << "║  Comprehensive Performance Test    ║\n";
    cout << "╚════════════════════════════════════╝\n" << endl;
    
    QStringList models_to_test = {
        "quantumide-q4km",      // Local GGUF
        "gpt-4",                // OpenAI
        "gpt-3.5-turbo",        // Cheaper alternative
        "claude-3-opus",        // Anthropic
        "gemini-pro",           // Google
    };
    
    cout << "\n1️⃣  LATENCY BENCHMARKS" << endl;
    cout << "════════════════════════════════════\n";
    
    for (const auto& model : models_to_test) {
        auto result = benchmark_latency(model, "Hello, world!", 20);
        result.print();
    }
    
    cout << "\n2️⃣  THROUGHPUT BENCHMARKS" << endl;
    cout << "════════════════════════════════════\n";
    
    for (const auto& model : models_to_test) {
        auto result = benchmark_throughput(model, "Test", 10);  // 10 seconds each
        result.print();
    }
    
    cout << "\n3️⃣  MEMORY BENCHMARKS" << endl;
    cout << "════════════════════════════════════\n";
    
    for (const auto& model : models_to_test) {
        auto result = benchmark_memory(model, 50);
        result.print();
    }
    
    cout << "\n4️⃣  COST ACCURACY BENCHMARKS" << endl;
    cout << "════════════════════════════════════\n";
    
    for (const auto& model : models_to_test) {
        auto result = benchmark_cost(model, 50);
        result.print();
    }
    
    cout << "\n✅  BENCHMARKING COMPLETE!" << endl;
}
```

---

## Optimization Strategies

### Optimization 1: Request Batching

**Problem:** Multiple API calls = high latency

**Solution:** Batch requests into single API call

```cpp
// ❌ SLOW: 100 separate API calls
for (const auto& prompt : prompts) {
    auto result = ai->generate(prompt, "gpt-4");
    process(result);
}
// Total latency: 100 × 500ms = 50 seconds

// ✅ FAST: 1 API call with 100 prompts
auto results = ai->generateBatch(prompts, "gpt-4");
for (const auto& result : results) {
    process(result);
}
// Total latency: 1 × 2000ms = 2 seconds
// 25x FASTER!
```

### Optimization 2: Streaming for Large Responses

**Problem:** Waiting for entire response = perceived slow

**Solution:** Stream chunks in real-time

```cpp
// ❌ SLOW: Wait for entire response
auto result = ai->generate(long_prompt, "gpt-4");
display(result.content);  // 5 seconds to display

// ✅ FAST: Display chunks immediately
ai->generateStream(long_prompt, "gpt-4",
    [](const QString& chunk) {
        display(chunk);  // Immediate feedback
    }
);
// Perception: Fast (first chunk in 200ms)
```

### Optimization 3: Async for Non-blocking UI

**Problem:** Synchronous calls freeze UI

**Solution:** Use async + callbacks

```cpp
// ❌ SLOW: Blocks UI for 2 seconds
auto result = ai->generate(prompt, "gpt-4");
ui->update_result(result);  // UI frozen during generate

// ✅ FAST: Non-blocking
ai->generateAsync(prompt, "gpt-4",
    [this](const GenerationResult& result) {
        ui->update_result(result);  // Called when ready
    }
);
// UI stays responsive
```

### Optimization 4: Caching Responses

**Problem:** Same prompts generate multiple times

**Solution:** Cache results

```cpp
QMap<QString, GenerationResult> response_cache;

GenerationResult get_or_generate(const QString& prompt, const QString& model) {
    QString key = prompt + "|" + model;
    
    // Return cached if available
    if (response_cache.contains(key)) {
        qDebug() << "Cache hit!";
        return response_cache[key];
    }
    
    // Generate and cache
    qDebug() << "Cache miss, generating...";
    auto result = ai->generate(prompt, model);
    response_cache[key] = result;
    
    return result;
}
```

**Benchmark:**
```
Without cache: 100 requests × 500ms = 50s
With cache: 1 generation + 99 lookups = 0.5s + 99×1μs ≈ 0.5s
100x FASTER!
```

### Optimization 5: Local Models for Interactive Features

**Problem:** Cloud models slow for real-time interaction

**Solution:** Use local models for speed

```cpp
// ❌ SLOW: Cloud API (500ms)
auto suggestion = ai->generate(code, "gpt-4");

// ✅ FAST: Local GGUF (50ms)
auto suggestion = ai->generate(code, "quantumide-q4km");

// Per-feature optimization:
if (is_real_time_feature) {
    model = "quantumide-q4km";  // <100ms
} else if (is_background_task) {
    model = "gpt-3.5-turbo";    // 200-500ms
} else {
    model = "gpt-4";            // Best quality
}
```

### Optimization 6: Smart Model Selection

**Problem:** Wrong model selection = slow or expensive

**Solution:** Auto-select optimal model

```cpp
// Select fastest model for task
QString model = ai->selectFastestModel("chat");

// Select cheapest model under budget
QString model = ai->selectCostOptimalModel(prompt, 0.01);  // Max $0.01

// Select best model for task
QString model = ai->selectBestModel("code_generation", "python", false);
```

### Optimization 7: Connection Pooling

**Problem:** Creating new connection per request

**Solution:** Reuse connections (handled internally)

```cpp
// CloudApiClient reuses QNetworkAccessManager
// Multiple requests share same connection pool
// Automatic HTTP Keep-Alive

// Result: 30% faster for sequential requests
```

### Optimization 8: Concurrent Requests

**Problem:** Sequential requests = n×latency

**Solution:** Send multiple async requests

```cpp
QVector<GenerationResult> results;
int completed = 0;
int target = 10;

for (int i = 0; i < target; ++i) {
    ai->generateAsync(prompts[i], "gpt-4",
        [&results, &completed, target](const GenerationResult& result) {
            results.append(result);
            completed++;
            if (completed == target) {
                process_all_results(results);
            }
        }
    );
}

// Sequential: 10 × 500ms = 5s
// Concurrent: 1 × 500ms = 0.5s (if API allows)
// 10x FASTER!
```

---

## Performance Monitoring Dashboard

```cpp
struct PerformanceMonitor {
    Q_OBJECT
    
public:
    PerformanceMonitor(ModelInterface* ai) : ai_(ai) {
        connect(&monitor_timer, &QTimer::timeout, this, &PerformanceMonitor::collect_metrics);
        monitor_timer.start(5000);  // Every 5 seconds
    }
    
    void collect_metrics() {
        metrics_snapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();
        snapshot.avg_latency = ai_->getAverageLatency();
        snapshot.success_rate = ai_->getSuccessRate();
        snapshot.total_cost = ai_->getTotalCost();
        snapshot.memory_usage = get_memory_usage();
        
        metrics_history.append(snapshot);
        
        // Alert on issues
        if (snapshot.avg_latency > 1000) {
            emit performance_degradation("Latency > 1s");
        }
        if (snapshot.success_rate < 95) {
            emit performance_degradation("Success rate < 95%");
        }
        if (snapshot.total_cost > daily_budget) {
            emit cost_exceeded();
        }
    }
    
    void print_dashboard() {
        auto latest = metrics_history.last();
        
        cout << "\n╔════════════════════════════╗\n";
        cout << "║  PERFORMANCE DASHBOARD     ║\n";
        cout << "║  " << latest.timestamp.toString().toStdString() << "  ║\n";
        cout << "╚════════════════════════════╝\n";
        cout << "Avg Latency: " << fixed << setprecision(0) << latest.avg_latency << "ms\n";
        cout << "Success Rate: " << latest.success_rate << "%\n";
        cout << "Total Cost: $" << fixed << setprecision(2) << latest.total_cost << "\n";
        cout << "Memory: " << latest.memory_usage << " MB\n";
    }
    
private:
    ModelInterface* ai_;
    QTimer monitor_timer;
    
    struct metrics_snapshot {
        QDateTime timestamp;
        double avg_latency = 0;
        int success_rate = 0;
        double total_cost = 0;
        int memory_usage = 0;
    };
    
    QVector<metrics_snapshot> metrics_history;
};
```

---

## Optimization Checklist

### Before Production
- [ ] Run full benchmark suite
- [ ] Verify latency targets met
- [ ] Verify memory usage acceptable
- [ ] Verify cost tracking accuracy
- [ ] Enable request caching
- [ ] Configure batch processing
- [ ] Set up monitoring
- [ ] Configure alerting
- [ ] Document performance baselines

### During Operation
- [ ] Monitor metrics daily
- [ ] Review cost trends weekly
- [ ] Analyze latency patterns
- [ ] Track success rates
- [ ] Watch for regressions
- [ ] Optimize high-cost features
- [ ] Cache frequently used prompts
- [ ] Route to fast models when possible

### Regular Maintenance
- [ ] Monthly performance review
- [ ] Quarterly benchmarking
- [ ] Update optimization strategies
- [ ] Audit cache hit rates
- [ ] Review API tier usage
- [ ] Plan for growth

---

## Common Performance Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| High latency | Using cloud models | Switch to local, use async |
| Memory leak | Unclosed connections | Use smart pointers, Qt lifecycle |
| High cost | Too many API calls | Batch requests, cache, use cheaper models |
| Slow startup | Loading large config | Cache config in memory |
| API rate limits | Too many simultaneous | Queue requests, increase delays |
| Network timeouts | Slow internet | Increase timeout, use streaming |

---

## Performance Scaling

### For 10 Users
- Use single ModelInterface instance (thread-safe)
- Enable request queuing
- Monitor costs daily

### For 100 Users
- Use request pooling
- Cache responses in Redis
- Monitor costs hourly
- Use cheaper models

### For 1000 Users
- Distribute load across instances
- Implement circuit breakers
- Use fast local models with cloud fallback
- Monitor cost per user

### For 10,000+ Users
- Enterprise API tier with higher limits
- Edge caching for common responses
- Model-specific rate limiting
- Cost budgeting per user/feature

---

## Success Metrics

✅ Local models: <100ms latency
✅ Cloud models: <2000ms latency  
✅ Success rate: >95%
✅ Cost accuracy: ±0.1%
✅ Memory usage: <200MB (active)
✅ API error rate: <1%
✅ Cache hit rate: >50%

---

## Conclusion

The Universal Model Router is optimized for:
- **Speed**: Local models <100ms, streaming for responsiveness
- **Cost**: Intelligent model selection, caching, batching
- **Reliability**: Automatic retry, fallback chains, monitoring
- **Scalability**: Async operations, connection pooling, load distribution

Follow the benchmarking and optimization strategies above to achieve maximum performance for your use case.
