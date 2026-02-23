#include "ide_diagnostic_system.h"
#include <algorithm>
#include <sstream>
#include <numeric>
#include <fstream>

using namespace RawrXD;

void IDEDiagnosticSystem::RegisterDiagnosticListener(DiagnosticCallback callback) {
    listeners_.push_back(callback);
}

void IDEDiagnosticSystem::EmitDiagnostic(const DiagnosticEvent& event) {
    diagnostics_.push_back(event);
    
    // Notify all listeners
    for (auto& listener : listeners_) {
        listener(event);
    }
}

void IDEDiagnosticSystem::StartMonitoring() {
    is_monitoring_ = true;
}

void IDEDiagnosticSystem::StopMonitoring() {
    is_monitoring_ = false;
}

std::vector<DiagnosticEvent> IDEDiagnosticSystem::GetDiagnostics(size_t limit) const {
    if (diagnostics_.size() <= limit) {
        return diagnostics_;
    }
    
    return std::vector<DiagnosticEvent>(
        diagnostics_.end() - limit,
        diagnostics_.end()
    );
}

std::vector<DiagnosticEvent> IDEDiagnosticSystem::GetDiagnosticsForFile(const std::string& file) const {
    std::vector<DiagnosticEvent> result;
    
    std::copy_if(diagnostics_.begin(), diagnostics_.end(), std::back_inserter(result),
        [&file](const DiagnosticEvent& e) { return e.source_file == file; });
    
    return result;
}

void IDEDiagnosticSystem::ClearDiagnostics() {
    diagnostics_.clear();
}

int IDEDiagnosticSystem::CountErrors() const {
    return std::count_if(diagnostics_.begin(), diagnostics_.end(),
        [](const DiagnosticEvent& e) { return e.severity == "error" || e.severity == "critical"; });
}

int IDEDiagnosticSystem::CountWarnings() const {
    return std::count_if(diagnostics_.begin(), diagnostics_.end(),
        [](const DiagnosticEvent& e) { return e.severity == "warning"; });
}

float IDEDiagnosticSystem::GetHealthScore() const {
    if (diagnostics_.empty()) return 100.0f;
    
    int errors = CountErrors();
    int warnings = CountWarnings();
    
    // Health scoring: -10 per error, -2 per warning
    float health = 100.0f - (errors * 10.0f) - (warnings * 2.0f);
    
    return std::max(0.0f, std::min(100.0f, health));
}

void IDEDiagnosticSystem::SaveSession(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) return;
    
    file << "=== IDE Diagnostic Session ===\n";
    file << "Total Events: " << diagnostics_.size() << "\n";
    file << "Errors: " << CountErrors() << "\n";
    file << "Warnings: " << CountWarnings() << "\n";
    file << "Health Score: " << GetHealthScore() << "%\n\n";
    
    for (const auto& diag : diagnostics_) {
        file << "["  << diag.severity << "] " << diag.source_file 
             << ":" << diag.line_number << " - " << diag.message << "\n";
    }
}

bool IDEDiagnosticSystem::LoadSession(const std::string& filepath) {
    std::ifstream file(filepath);
    return file.is_open();
}

void IDEDiagnosticSystem::RecordCompileTime(const std::string& file, long long ms) {
    compile_times_.push_back({file, ms});
}

void IDEDiagnosticSystem::RecordInferenceTime(long long ms) {
    inference_times_.push_back(ms);
}

std::string IDEDiagnosticSystem::GetPerformanceReport() const {
    std::ostringstream oss;
    
    oss << "=== Performance Report ===\n";
    
    // Compile times
    if (!compile_times_.empty()) {
        long long total_compile = std::accumulate(compile_times_.begin(), compile_times_.end(), 0LL,
            [](long long sum, const auto& p) { return sum + p.second; });
        long long avg_compile = total_compile / compile_times_.size();
        
        oss << "Compilation:\n";
        oss << "  Total Files: " << compile_times_.size() << "\n";
        oss << "  Average Time: " << avg_compile << "ms\n";
        oss << "  Total Time: " << total_compile << "ms\n";
    }
    
    // Inference times
    if (!inference_times_.empty()) {
        long long total_inference = std::accumulate(inference_times_.begin(), inference_times_.end(), 0LL);
        long long avg_inference = total_inference / inference_times_.size();
        long long min_inference = *std::min_element(inference_times_.begin(), inference_times_.end());
        long long max_inference = *std::max_element(inference_times_.begin(), inference_times_.end());
        
        oss << "Inference:\n";
        oss << "  Invocations: " << inference_times_.size() << "\n";
        oss << "  Average Time: " << avg_inference << "ms\n";
        oss << "  Min Time: " << min_inference << "ms\n";
        oss << "  Max Time: " << max_inference << "ms\n";
        oss << "  Total Time: " << total_inference << "ms\n";
    }
    
    return oss.str();
}
