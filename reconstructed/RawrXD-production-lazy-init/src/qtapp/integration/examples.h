#pragma once
/**
 * @file integration_examples.h
 * @brief Example usage patterns for the integration framework.
 * 
 * This header documents practical patterns for using the integration framework.
 * Compile with: RAWRXD_LOGGING_ENABLED=1 environment variable to see output.
 */

#include <QString>
#include <QObject>
#include "ProdIntegration.h"
#include "InitializationTracker.h"
#include "Logger.h"
#include "Diagnostics.h"
#include "ResourceGuards.h"

namespace RawrXD {
namespace Integration {
namespace Examples {

// ============================================================================
// Example 1: Simple Widget Constructor Instrumentation
// ============================================================================
class ExampleWidget1 : public QObject {
    Q_OBJECT
public:
    explicit ExampleWidget1(QObject* parent = nullptr)
        : QObject(parent) {
        ScopedInitTimer init("ExampleWidget1");
        logInfo("ExampleWidget1", "init", "Initializing widget");
        recordMetric("widget_created");
        traceEvent("ExampleWidget1", "created");
    }
};

// ============================================================================
// Example 2: Method Instrumentation with Latency
// ============================================================================
class ExampleWidget2 : public QObject {
    Q_OBJECT
public slots:
    void processData() {
        ScopedTimer timer("ExampleWidget2", "processData", "data_processing");
        
        // Simulate work
        for (int i = 0; i < 100; ++i) {
            // ... work ...
        }
        
        recordMetric("data_processed", 100);
    }
};

// ============================================================================
// Example 3: Using Unified Logger
// ============================================================================
void exampleUnifiedLogging() {
    Logger::info()
        .component("DataModule")
        .event("load_data")
        .message("User requested data load");
    // Logs: [timestamp] [INFO] [DataModule] [load_data] User requested data load
    
    Logger::error()
        .component("DataModule")
        .event("load_failed")
        .message("Network timeout after 5000ms");
}

// ============================================================================
// Example 4: Diagnostic Report Generation
// ============================================================================
void exampleDiagnostics() {
    // Generate JSON report
    auto report = Diagnostics::initializationReport();
    qInfo().noquote() << "Initialization Report:" << report;
    
    // Print human-readable summary
    QString summary = Diagnostics::initializationSummary();
    qInfo().noquote() << summary;
    
    // Dump full report
    Diagnostics::dumpInitializationReport();
}

// ============================================================================
// Example 5: Resource Guards
// ============================================================================
class FileHandle {
public:
    explicit FileHandle(const QString& path) : path(path) {}
    QString path;
};

void exampleResourceGuard() {
    {
        // RAII cleanup: file is automatically closed when guard is destroyed
        ResourceGuard<FileHandle*> file(
            new FileHandle("data.txt"),
            [](FileHandle* f) {
                logInfo("FileGuard", "cleanup", QString("Closing file: %1").arg(f->path));
                delete f;
            }
        );
        
        // Use file
        logInfo("FileGuard", "use", "Reading file");
    }
    // File cleaned up automatically
}

// ============================================================================
// Example 6: Scoped Actions
// ============================================================================
void exampleScopedAction() {
    {
        ScopedAction cleanup([](){ 
            logInfo("Cleanup", "on_exit", "Cleanup code executed");
        });
        
        logInfo("Work", "start", "Starting work");
        // ... work ...
    }
    // cleanup callback executed automatically
}

// ============================================================================
// Example 7: Conditional Instrumentation
// ============================================================================
void exampleConditionalLogging() {
    if (Config::loggingEnabled()) {
        logInfo("Conditional", "check", "Logging is enabled");
    }
    
    if (Config::stubLoggingEnabled()) {
        logInfo("Conditional", "stubs", "Stub logging is enabled");
    }
    
    if (Config::metricsEnabled()) {
        recordMetric("example_metric");
    }
    
    if (Config::tracingEnabled()) {
        traceEvent("Example", "event");
    }
}

// ============================================================================
// Example 8: Complex Flow Instrumentation
// ============================================================================
class ComplexOperation : public QObject {
    Q_OBJECT
public slots:
    void execute() {
        ScopedTimer mainTimer("ComplexOperation", "execute", "full_flow");
        
        step1();
        step2();
        step3();
        
        qInfo().noquote() << "Operation complete";
    }

private:
    void step1() {
        ScopedTimer timer("ComplexOperation", "step1", "phase1");
        logInfo("ComplexOperation", "step1", "Executing phase 1");
        // ... work ...
    }
    
    void step2() {
        ScopedTimer timer("ComplexOperation", "step2", "phase2");
        logInfo("ComplexOperation", "step2", "Executing phase 2");
        // ... work ...
    }
    
    void step3() {
        ScopedTimer timer("ComplexOperation", "step3", "phase3");
        logInfo("ComplexOperation", "step3", "Executing phase 3");
        // ... work ...
    }
};

// ============================================================================
// Example 9: Initialization Tracking in Main Sequence
// ============================================================================
void exampleInitializationSequence() {
    {
        ScopedInitTimer initDB("DatabaseModule");
        logInfo("Bootstrap", "init", "Initializing database");
        // ... db init work ...
    }
    
    {
        ScopedInitTimer initUI("UIModule");
        logInfo("Bootstrap", "init", "Initializing UI");
        // ... ui init work ...
    }
    
    {
        ScopedInitTimer initNetwork("NetworkModule");
        logInfo("Bootstrap", "init", "Initializing network");
        // ... network init work ...
    }
    
    // Print final report
    Diagnostics::dumpInitializationReport();
}

} // namespace Examples
} // namespace Integration
} // namespace RawrXD

// ============================================================================
// Quick reference: How to enable in your application
// ============================================================================
/*
 * PowerShell:
 *   $env:RAWRXD_LOGGING_ENABLED = "1"
 *   $env:RAWRXD_LOG_STUBS = "1"
 *   $env:RAWRXD_ENABLE_METRICS = "1"
 *   $env:RAWRXD_ENABLE_TRACING = "1"
 *   ./MyApp.exe
 *
 * Bash:
 *   export RAWRXD_LOGGING_ENABLED=1
 *   export RAWRXD_LOG_STUBS=1
 *   export RAWRXD_ENABLE_METRICS=1
 *   export RAWRXD_ENABLE_TRACING=1
 *   ./myapp
 */
