#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>

namespace RawrXD {

struct DiagnosticEvent {
    enum Type { CompileWarning, RuntimeError, MemoryLeak, SecurityIssue, PerformanceDegradation };
    
    Type type;
    std::string message;
    std::string source_file;
    int line_number = 0;
    std::chrono::system_clock::time_point timestamp;
    std::string severity; // "info", "warning", "error", "critical"
};

class IDEDiagnosticSystem {
public:
    using DiagnosticCallback = std::function<void(const DiagnosticEvent&)>;
    
    IDEDiagnosticSystem() = default;
    virtual ~IDEDiagnosticSystem() = default;
    
    // Core operations
    void RegisterDiagnosticListener(DiagnosticCallback callback);
    void EmitDiagnostic(const DiagnosticEvent& event);
    
    // Real-time monitoring
    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const { return is_monitoring_; }
    
    // Diagnostic collection
    std::vector<DiagnosticEvent> GetDiagnostics(size_t limit = 100) const;
    std::vector<DiagnosticEvent> GetDiagnosticsForFile(const std::string& file) const;
    void ClearDiagnostics();
    
    // Analysis
    int CountErrors() const;
    int CountWarnings() const;
    float GetHealthScore() const; // 0-100 health metric
    
    // Session management
    void SaveSession(const std::string& filepath);
    bool LoadSession(const std::string& filepath);
    
    // Performance profiling hooks
    void RecordCompileTime(const std::string& file, long long ms);
    void RecordInferenceTime(long long ms);
    std::string GetPerformanceReport() const;

private:
    std::vector<DiagnosticEvent> diagnostics_;
    std::vector<DiagnosticCallback> listeners_;
    bool is_monitoring_ = false;
    
    std::vector<std::pair<std::string, long long>> compile_times_;
    std::vector<long long> inference_times_;
};

}
