# Autonomous Features - Quick Reference Guide

## Getting Started

### Access Autonomous Engines in Code
```cpp
// In any object that has access to AgenticIDE
AgenticIDE* ide = AgenticIDE::instance();

// Get engines
AutonomousFeatureEngine* featureEngine = ide->getAutonomousFeatureEngine();
AutonomousModelManager* modelManager = ide->getAutonomousModelManager();
```

### Trigger Model Recommendation
```cpp
// Get best model for your system
ModelRecommendation rec = modelManager->autoDetectBestModel("completion", "cpp");

// Download and setup (handles streaming for >2GB)
modelManager->autoDownloadAndSetup(rec.modelId);

// UI automatically updates with progress
```

### Analyze Code
```cpp
// Start analyzing a file
QString code = readCodeFromFile("main.cpp");
featureEngine->analyzeCode(code, "main.cpp", "cpp");

// Or analyze on timer (already started)
// featureEngine->startBackgroundAnalysis();

// Suggestions appear in AI Suggestions widget automatically
```

### Handle Suggestions in Custom Code
```cpp
// Connect to suggestion signals
connect(featureEngine, &AutonomousFeatureEngine::suggestionGenerated,
        this, [this](const AutonomousSuggestion& suggestion) {
    qDebug() << "Got suggestion:" << suggestion.type;
    qDebug() << "Confidence:" << suggestion.confidence;
    qDebug() << "Suggested code:" << suggestion.suggestedCode;
});

// Handle accept/reject
connect(this, &MyClass::suggestionAccepted, featureEngine, [this](const QString& id) {
    featureEngine->onSuggestionAccepted(id);
});
```

## Key Classes & Methods

### AutonomousModelManager
```cpp
// System Analysis
SystemAnalysis analyzeSystemCapabilities();
ModelRecommendation autoDetectBestModel(const QString& taskType, const QString& language);

// Model Operations
bool autoDownloadAndSetup(const QString& modelId);
bool autoOptimizeModel(const QString& modelId);

// Streaming (for >2GB models)
bool streamModelFromURL(const QString& url, const QString& destination, const QString& modelId);
bool enableStreamingForLargeModels(bool enable);

// Configuration
void setMaxModelSize(qint64 sizeBytes);
qint64 getMaxModelSize() const;
```

### AutonomousFeatureEngine
```cpp
// Analysis
void analyzeCode(const QString& code, const QString& filePath, const QString& language);
void analyzeCodeChange(const QString& oldCode, const QString& newCode, 
                       const QString& filePath, const QString& language);

// Background Analysis
void startBackgroundAnalysis(const QString& projectPath = "");
void stopBackgroundAnalysis();
void enableRealTimeAnalysis(bool enable);
void setAnalysisInterval(int milliseconds);

// Configuration
void setConfidenceThreshold(double threshold);
void enableAutomaticSuggestions(bool enable);
void setMaxConcurrentAnalyses(int max);

// UI Handlers
void onSuggestionAccepted(const QString& suggestionId);
void onSuggestionRejected(const QString& suggestionId);
void markSecurityIssueAsFixed(const QString& issueId);
void markSecurityIssueAsIgnored(const QString& issueId);
void applyOptimization(const QString& optimizationId);
void dismissOptimization(const QString& optimizationId);
```

### StreamingGGUFMemoryManager
```cpp
// Initialize with memory budget
bool initialize(size_t max_memory_bytes);

// Load model (handles streaming for large files)
bool streamModel(const std::string& model_path, const std::string& model_id);

// Configuration
void setPrefetchStrategy(PrefetchStrategy strategy);
void setBlockSize(size_t size);
void setMaxMemoryBudget(size_t bytes);

// Shutdown
void shutdown();
```

## Data Structures

### AutonomousSuggestion
```cpp
struct AutonomousSuggestion {
    QString suggestionId;              // Unique ID
    QString type;                      // test_generation, optimization, refactoring, security_fix, documentation
    QString filePath;                  // File path
    int lineNumber;                    // Line number
    QString originalCode;              // Original code snippet
    QString suggestedCode;             // Suggested code
    QString explanation;               // Human readable explanation
    double confidence;                 // 0-1 confidence score
    QStringList benefits;              // List of benefits
    QJsonObject metadata;              // Additional metadata
    QDateTime timestamp;               // When generated
    bool wasAccepted;                  // User acceptance status
};
```

### SecurityIssue
```cpp
struct SecurityIssue {
    QString issueId;                   // Unique ID
    QString severity;                  // critical, high, medium, low
    QString type;                      // sql_injection, xss, buffer_overflow, etc.
    QString filePath;                  // File path
    int lineNumber;                    // Line number
    QString vulnerableCode;            // Vulnerable code
    QString description;               // Description
    QString suggestedFix;              // Suggested fix
    QString cveReference;              // CVE reference if applicable
    QStringList affectedComponents;    // Affected components
    double riskScore;                  // 0-10 risk score
};
```

### PerformanceOptimization
```cpp
struct PerformanceOptimization {
    QString optimizationId;            // Unique ID
    QString type;                      // algorithm, caching, parallelization, memory
    QString filePath;                  // File path
    int lineNumber;                    // Line number
    QString currentImplementation;     // Current code
    QString optimizedImplementation;   // Optimized code
    QString reasoning;                 // Explanation
    double expectedSpeedup;            // Expected speedup multiplier (e.g., 2.0 = 2x faster)
    qint64 expectedMemorySaving;       // Expected memory savings in bytes
    double confidence;                 // Confidence score 0-1
};
```

### SystemAnalysis
```cpp
struct SystemAnalysis {
    qint64 availableRAM;               // Available RAM in bytes
    qint64 availableDiskSpace;         // Available disk space in bytes
    int cpuCores;                      // Number of CPU cores
    bool hasGPU;                       // GPU availability
    QString gpuType;                   // GPU type (e.g., "CUDA", "Metal")
    qint64 gpuMemory;                  // GPU memory in bytes
};
```

## Configuration Files

### Settings Dialog Tabs
1. **Analysis**: Control analysis behavior
2. **Features**: Enable/disable specific features
3. **Memory**: Configure memory usage and prefetching
4. **Models**: Set model size limits and auto-operations

### Recommended Settings
```
// For Development
Analysis Interval: 15 seconds
Confidence Threshold: 0.70
Max Memory: 16 GB
Prefetch Strategy: Adaptive

// For Production
Analysis Interval: 30 seconds
Confidence Threshold: 0.75
Max Memory: 70% of available RAM
Prefetch Strategy: LRU
```

## Common Use Cases

### Use Case 1: Load Large Model Silently
```cpp
void MyApp::loadLargeModel() {
    AutonomousModelManager* mgr = AgenticIDE::instance()->getAutonomousModelManager();
    
    // Hide UI widgets that show progress
    // mgr->autoDownloadAndSetup("meta-llama/Llama-2-7b"); // 13GB model
    // Will automatically stream without blocking
}
```

### Use Case 2: Enable Code Analysis on Project
```cpp
void MyProject::onProjectOpened(const QString& projectPath) {
    AutonomousFeatureEngine* engine = AgenticIDE::instance()->getAutonomousFeatureEngine();
    
    // Start analyzing the project
    engine->startBackgroundAnalysis(projectPath);
    engine->enableRealTimeAnalysis(true);
    
    // Show suggestion widget
    // Suggestions will appear automatically
}
```

### Use Case 3: Custom Security Report
```cpp
void MySecurityAnalyzer::generateReport() {
    AutonomousFeatureEngine* engine = AgenticIDE::instance()->getAutonomousFeatureEngine();
    
    // Get all security issues found
    QString code = readEntireProject();
    engine->analyzeCode(code, "/", "cpp");
    
    // Later, when signals arrive...
    connect(engine, &AutonomousFeatureEngine::securityIssueDetected,
            this, &MySecurityAnalyzer::onIssueDetected);
}

void MySecurityAnalyzer::onIssueDetected(const SecurityIssue& issue) {
    if (issue.severity == "critical") {
        // Alert user
        qWarning() << "CRITICAL:" << issue.description;
    }
}
```

### Use Case 4: Custom Optimization Applier
```cpp
void MyOptimizer::applyOptimizations() {
    AutonomousFeatureEngine* engine = AgenticIDE::instance()->getAutonomousFeatureEngine();
    
    QString code = editor->text();
    engine->analyzeCode(code, "main.cpp", "cpp");
    
    // When optimizations are found...
    connect(engine, &AutonomousFeatureEngine::optimizationFound,
            this, &MyOptimizer::onOptimizationFound);
}

void MyOptimizer::onOptimizationFound(const PerformanceOptimization& opt) {
    if (opt.expectedSpeedup > 2.0) {  // Only care about 2x+ speedup
        // Apply optimization
        applyCodeChange(opt.currentImplementation, opt.optimizedImplementation);
        
        // Notify
        engine->applyOptimization(opt.optimizationId);
    }
}
```

## Debugging

### Enable Debug Output
```cpp
// In your code
AutonomousFeatureEngine* engine = AgenticIDE::instance()->getAutonomousFeatureEngine();

// All internal debug logs will print to qDebug()
// Check Debug Output panel in IDE
```

### Monitor Model Loading
```cpp
AutonomousModelManager* mgr = AgenticIDE::instance()->getAutonomousModelManager();

// Connect to progress
connect(mgr, QOverload<const QString&, int, qint64, qint64>::of(&AutonomousModelManager::downloadProgress),
        [](const QString& modelId, int pct, qint64 speed, qint64 eta) {
    qDebug() << "Model:" << modelId << "Progress:" << pct << "% Speed:" << (speed / 1024 / 1024) << "MB/s ETA:" << (eta / 60) << "min";
});
```

### Check System Analysis
```cpp
AutonomousModelManager* mgr = AgenticIDE::instance()->getAutonomousModelManager();
SystemAnalysis sys = mgr->analyzeSystemCapabilities();

qDebug() << "RAM:" << (sys.availableRAM / 1024 / 1024 / 1024) << "GB";
qDebug() << "Disk:" << (sys.availableDiskSpace / 1024 / 1024 / 1024) << "GB";
qDebug() << "CPU:" << sys.cpuCores;
qDebug() << "GPU:" << (sys.hasGPU ? sys.gpuType : "None");
```

## Performance Tips

1. **Adjust Analysis Interval**: Longer intervals = less CPU usage
2. **Increase Confidence Threshold**: Fewer suggestions but higher quality
3. **Limit Concurrent Analyses**: Reduce from 4 to 1-2 on slow machines
4. **Use Prefetch Strategy**: Adaptive learns access patterns, Aggressive prefetches everything

## Troubleshooting

### No Suggestions Appearing
- Check if real-time analysis is enabled: `enableRealTimeAnalysis(true)`
- Check confidence threshold isn't too high
- Check analysis interval isn't too long
- Monitor debug output for analysis errors

### Models Not Recommended
- Verify `analyzeSystemCapabilities()` detects system correctly
- Check minimum suitability score (default 0.75)
- Review available models in catalog

### Slow Analysis
- Increase analysis interval to reduce CPU
- Reduce max concurrent analyses
- Check for large files being analyzed

### Out of Memory
- Reduce max memory budget in streaming manager
- Increase prefetch block size (uses less total memory)
- Disable streaming for models <2GB

## More Information

See detailed documentation:
- `AUTONOMOUS_FEATURES_COMPLETE.md` - Complete feature overview
- `AUTONOMOUS_FEATURES_IMPLEMENTATION.md` - Implementation details
- `WIRING_VERIFICATION.md` - Signal/slot wiring verification
