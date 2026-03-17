# Q2_K Performance Integration - System Architecture

## How Performance Metrics Integrate with InferenceEngine

### 1. Metrics Collection Pipeline

```
┌─────────────────────────────────────────────────────────────┐
│ User calls: engine.loadModel("q2k-model.gguf")             │
└────────────────────┬────────────────────────────────────────┘
                     │
        ┌────────────▼────────────┐
        │ Measurement Points       │
        ├────────────────────────┤
        │ 1. Start load timer    │──┐
        │ 2. Parse GGUF header   │  │
        │ 3. Detect format       │  │ Collect timing
        │ 4. Load tensors        │  │
        │ 5. Dequantize blocks   │  │
        │ 6. Build transformer   │  │
        │ 7. End load timer      │──┘
        └────────────┬───────────┘
                     │
        ┌────────────▼────────────────────┐
        │ Store Metrics                  │
        ├────────────────────────────────┤
        │ m_loadTimeMs                   │
        │ m_detectionMs                  │
        │ m_dequantTimeMs                │
        │ m_transformerInitMs            │
        │ m_peakMemoryMB                 │
        └────────────┬────────────────────┘
                     │
        ┌────────────▼─────────────────────┐
        │ Emit Signals & Logs             │
        ├─────────────────────────────────┤
        │ emit logMessage(...)            │
        │ emit metricsReady(...)          │
        │ (Qt signals for UI)             │
        └─────────────────────────────────┘
```

---

### 2. Code Integration Points

#### 2.1 InferenceEngine Header (`inference_engine.hpp`)

**Metrics Member Variables**:
```cpp
private:
    // Performance metrics
    qint64 m_loadTimeMs{0};           // Total load time
    qint64 m_detectionTimeMs{0};      // Format detection
    qint64 m_tensorLoadTimeMs{0};     // GGUF reading
    qint64 m_dequantTimeMs{0};        // Q2_K dequantization
    qint64 m_transformerInitTimeMs{0};// Transformer building
    qint64 m_peakMemoryMB{0};         // Peak memory usage
```

**Metrics Signal**:
```cpp
signals:
    void metricsReady(const QJsonObject& metrics);
```

#### 2.2 InferenceEngine Implementation (`inference_engine.cpp`)

**Metric Collection Pattern**:
```cpp
bool InferenceEngine::loadModel(const QString& path)
{
    QMutexLocker lock(&m_mutex);
    QElapsedTimer globalTimer;
    globalTimer.start();
    
    // Measurement 1: Format Detection
    QElapsedTimer formatTimer;
    formatTimer.start();
    QString format = detectQuantizationFormat();
    m_detectionTimeMs = formatTimer.elapsed();
    
    // Measurement 2: Tensor Loading
    QElapsedTimer loadTimer;
    loadTimer.start();
    rebuildTensorCache();
    m_tensorLoadTimeMs = loadTimer.elapsed();
    
    // Measurement 3: Dequantization (in loadQ2kTensors)
    // Measurement 4: Transformer Init
    QElapsedTimer transformerTimer;
    transformerTimer.start();
    buildTransformerFromQ2kCache();
    m_transformerInitTimeMs = transformerTimer.elapsed();
    
    // Total time
    m_loadTimeMs = globalTimer.elapsed();
    
    // Emit metrics
    QJsonObject metrics;
    metrics["load_time_ms"] = static_cast<int>(m_loadTimeMs);
    metrics["detection_ms"] = static_cast<int>(m_detectionTimeMs);
    metrics["tensor_load_ms"] = static_cast<int>(m_tensorLoadTimeMs);
    metrics["dequant_ms"] = static_cast<int>(m_dequantTimeMs);
    metrics["transformer_ms"] = static_cast<int>(m_transformerInitTimeMs);
    metrics["format"] = format;
    emit metricsReady(metrics);
}
```

---

### 3. Real-Time Monitoring

#### Dashboard Display (Qt GUI)

```cpp
// In MainWindow or Dashboard widget
void Dashboard::onMetricsReady(const QJsonObject& metrics) {
    // Update UI with metrics
    ui->loadTimeLabel->setText(
        QString("%1 ms").arg(metrics["load_time_ms"].toInt())
    );
    
    ui->throughputLabel->setText(
        QString("%1 MB/s").arg(
            calculateThroughput(metrics)
        )
    );
    
    // Store in history for trending
    m_metricsHistory.append(metrics);
}

// Trending calculation
double calculateThroughput(const QJsonObject& metrics) {
    int deQuantMs = metrics["dequant_ms"].toInt();
    if (deQuantMs == 0) return 0;
    
    // Assume 7B model = 2.56M elements
    int elements = 2560000;
    double megaElements = elements / 1e6;
    return megaElements / (deQuantMs / 1000.0);  // M elem/sec
}
```

---

### 4. Performance Baseline Reference

#### Expected Values by Model Size

```
Model    Format   File Size   Load Time   Peak RAM   TPS
3B       Q2_K     1.0 GB      380 ms      8 GB       45
         Q4_K     1.5 GB      300 ms      8 GB       48
         
7B       Q2_K     2.6 GB      900 ms      31 GB      48
         Q4_K     3.9 GB      716 ms      32 GB      53
         
13B      Q2_K     5.2 GB      1,850 ms    56 GB      42
         Q4_K     7.8 GB      1,480 ms    58 GB      47
         
70B      Q2_K     26 GB       9,200 ms    280 GB     30
         Q4_K     39 GB       7,360 ms    290 GB     35
```

#### Regression Detection

```cpp
// Check if performance degraded
bool checkPerformanceRegression(const QJsonObject& current) {
    QJsonObject baseline = loadBaseline(current["format"].toString());
    
    int currentLoad = current["load_time_ms"].toInt();
    int baselineLoad = baseline["load_time_ms"].toInt();
    
    // Flag if > 20% slower
    if (currentLoad > baselineLoad * 1.2) {
        qWarning() << "Performance regression detected:"
                   << (currentLoad - baselineLoad) << "ms slower";
        return true;
    }
    
    return false;
}
```

---

### 5. Inference-Time Metrics

#### Token Generation Performance

```cpp
std::vector<int32_t> InferenceEngine::generate(...)
{
    QElapsedTimer inferenceTimer;
    inferenceTimer.start();
    
    std::vector<int32_t> result = inputTokens;
    int tokensGenerated = 0;
    
    for (int step = 0; step < maxTokens; ++step) {
        // Actual inference
        std::vector<float> logits = m_transformer.forward(result);
        
        // Sampling
        int32_t nextToken = sampleToken(logits);
        result.push_back(nextToken);
        tokensGenerated++;
    }
    
    qint64 elapsed = inferenceTimer.elapsed();
    m_tokensPerSecond = (tokensGenerated * 1000.0) / elapsed;
    
    // Emit inference metrics
    emit inferenceMetrics({
        {"tokens_generated", tokensGenerated},
        {"elapsed_ms", static_cast<int>(elapsed)},
        {"tps", QString::number(m_tokensPerSecond, 'f', 1)},
        {"format", m_detectedQuantFormat}
    });
    
    return result;
}
```

---

### 6. Performance Profiling Integration

#### Detailed Breakdown (Verbose Mode)

```cpp
void InferenceEngine::profileLoadModel(const QString& path)
{
    QJsonObject profile;
    profile["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    profile["model_path"] = path;
    
    // Phase 1: GGUF Parsing
    {
        QElapsedTimer timer;
        timer.start();
        
        GGUFParser parser(path);
        
        profile["phase_parse"] = QJsonObject{
            {"time_ms", static_cast<int>(timer.elapsed())},
            {"tensor_count", static_cast<int>(parser.tensors().size())},
            {"metadata_entries", static_cast<int>(parser.metadata().size())}
        };
    }
    
    // Phase 2: Format Detection
    {
        QElapsedTimer timer;
        timer.start();
        
        QString format = detectQuantizationFormat();
        
        profile["phase_detect"] = QJsonObject{
            {"time_ms", static_cast<int>(timer.elapsed())},
            {"format", format}
        };
    }
    
    // Phase 3: Tensor Loading
    {
        QElapsedTimer timer;
        timer.start();
        
        rebuildTensorCache();
        
        profile["phase_load"] = QJsonObject{
            {"time_ms", static_cast<int>(timer.elapsed())},
            {"tensor_count", m_tensorCache.size()}
        };
    }
    
    // Phase 4: Dequantization
    {
        QElapsedTimer timer;
        timer.start();
        
        loadQ2kTensors();
        
        profile["phase_dequant"] = QJsonObject{
            {"time_ms", static_cast<int>(timer.elapsed())},
            {"throughput_m_elem_s", calculateThroughput()}
        };
    }
    
    // Save profile
    QJsonDocument doc(profile);
    saveProfile(doc);
    emit profileReady(profile);
}
```

---

### 7. Comparative Benchmarking

#### A/B Testing Q2_K vs Q4_K

```cpp
struct BenchmarkResult {
    QString format;
    int loadTimeMs;
    double throughputMElemS;
    int peakMemoryMB;
    double tokenPerSec;
};

QVector<BenchmarkResult> compareFormats(const QString& modelPath) {
    QVector<BenchmarkResult> results;
    
    // Test Q2_K
    {
        InferenceEngine engine;
        engine.setQuantMode("Q2_K");
        BenchmarkResult q2k = runBenchmark(engine, modelPath);
        q2k.format = "Q2_K";
        results.append(q2k);
    }
    
    // Test Q4_K
    {
        InferenceEngine engine;
        engine.setQuantMode("Q4_K");
        BenchmarkResult q4k = runBenchmark(engine, modelPath);
        q4k.format = "Q4_K";
        results.append(q4k);
    }
    
    return results;
}

void reportComparison(const QVector<BenchmarkResult>& results) {
    qInfo() << "\n=== Format Comparison ===";
    
    auto q2k = results[0];
    auto q4k = results[1];
    
    qInfo() << "Load Time:"
            << "Q2_K=" << q2k.loadTimeMs << "ms"
            << "Q4_K=" << q4k.loadTimeMs << "ms"
            << "Δ=" << ((double)q2k.loadTimeMs / q4k.loadTimeMs - 1.0) * 100 << "%";
    
    qInfo() << "Throughput:"
            << "Q2_K=" << q2k.throughputMElemS << "M/s"
            << "Q4_K=" << q4k.throughputMElemS << "M/s"
            << "Δ=" << ((q4k.throughputMElemS / q2k.throughputMElemS - 1.0) * 100) << "%";
}
```

---

### 8. Logging Format Reference

#### Structured Metrics Logging

```
time=2025-12-04T12:00:00.000-05:00 level=INFO source=inference_engine.cpp:loadModel msg="model load complete" \
  load_time_ms=902 detection_ms=2 tensor_load_ms=152 dequant_ms=620 transformer_ms=128 \
  format="Q2_K" model_path="/models/llama-7b-q2k.gguf" tensor_count=500 \
  memory_peak_mb=31240 throughput_m_elem_s=432
```

#### Metrics Available in Logs

| Field | Type | Example |
|-------|------|---------|
| `load_time_ms` | int | 902 |
| `detection_ms` | int | 2 |
| `tensor_load_ms` | int | 152 |
| `dequant_ms` | int | 620 |
| `transformer_ms` | int | 128 |
| `format` | string | "Q2_K" |
| `tensor_count` | int | 500 |
| `memory_peak_mb` | int | 31240 |
| `throughput_m_elem_s` | int | 432 |

---

### 9. Performance Regression Testing

#### CI/CD Integration

```powershell
# PowerShell script for CI
param(
    [string]$modelPath = "models/llama-7b-q2k.gguf",
    [string]$threshold = 1.2  # 20% regression threshold
)

# Run benchmark
$result = & .\bench_q2k_vs_q4k_e2e.exe $modelPath
$metrics = $result | ConvertFrom-Json

# Compare with baseline
$baseline = Get-Content baseline.json | ConvertFrom-Json
$loadTime = $metrics.load_time_ms
$baselineTime = $baseline.load_time_ms

# Check regression
if ($loadTime -gt $baselineTime * $threshold) {
    Write-Error "Performance regression: $loadTime ms vs baseline $baselineTime ms"
    exit 1
}

Write-Output "✓ Performance OK: $loadTime ms (baseline: $baselineTime ms)"
exit 0
```

---

### 10. Metrics Export

#### JSON Export for Analysis

```json
{
  "timestamp": "2025-12-04T12:00:00Z",
  "system": {
    "processor": "Intel i7-13700K",
    "ram_gb": 64,
    "os": "Windows 11"
  },
  "model": {
    "path": "/models/llama-7b-q2k.gguf",
    "format": "Q2_K",
    "parameters": 7000000000,
    "file_size_gb": 2.6
  },
  "metrics": {
    "load": {
      "total_ms": 902,
      "phases": {
        "detection_ms": 2,
        "tensor_load_ms": 152,
        "dequant_ms": 620,
        "transformer_ms": 128
      },
      "throughput_m_elem_s": 432
    },
    "inference": {
      "tokens_per_second": 48,
      "latency_ms_per_token": 20.8,
      "temperature": 0.8
    },
    "memory": {
      "peak_mb": 31240,
      "steady_state_mb": 28000
    }
  },
  "comparison": {
    "q2k_vs_q4k": {
      "load_time_delta_percent": -26,
      "throughput_delta_percent": -19,
      "file_size_delta_percent": -33
    }
  }
}
```

---

## Summary

**Key Integration Points**:
1. ✅ Metrics collection in `loadModel()` and `generate()`
2. ✅ Structured logging with timestamps and tags
3. ✅ Qt signals for UI updates
4. ✅ JSON export for trending analysis
5. ✅ Regression detection in CI/CD
6. ✅ Real-time dashboard visualization

**Metrics to Track**:
- Load time (detection, loading, dequant, transformer init)
- Throughput (M elements/sec)
- Peak memory
- Inference speed (tokens/sec)
- Performance regressions (>20% slower)

**Files Modified**:
- `src/qtapp/inference_engine.hpp` - Metrics members + signals
- `src/qtapp/inference_engine.cpp` - Metrics collection code
- CI/CD scripts - Regression testing

---

*For detailed metrics analysis, see `Q2_K-PERFORMANCE-METRICS-REPORT.md`*
