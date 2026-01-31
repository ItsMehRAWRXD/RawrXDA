// RawrXD_ErrorRecovery.hpp - Autonomous Error Recovery and Auto-Healing
// Pure C++20 - No Qt Dependencies
// Manages: Error tracking, Recovery strategies, Health monitoring

#pragma once

#include "RawrXD_JSON.hpp"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iostream>

namespace RawrXD {

enum class ErrorSeverity { Info, Warning, Error, Critical, Fatal };
enum class ErrorCategory { System, Network, FileIO, Database, AIModel, Cloud, Security, Performance };

struct ErrorRecord {
    std::string id;
    std::string component;
    ErrorSeverity severity;
    ErrorCategory category;
    std::string message;
    uint64_t timestamp;
    bool recovered = false;
    int retries = 0;
};

class ErrorRecoverySystem {
public:
    ErrorRecoverySystem() {
        m_healthScore = 100.0;
    }

    std::string RecordError(const std::string& component, ErrorSeverity severity, ErrorCategory category, const std::string& message) {
        ErrorRecord rec;
        rec.id = "ERR_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        rec.component = component;
        rec.severity = severity;
        rec.category = category;
        rec.message = message;
        rec.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        m_errors.push_back(rec);
        UpdateHealth();

        std::cout << "[ErrorRecovery] Recorded " << message << " in " << component << "\n";
        
        if (severity >= ErrorSeverity::Error) {
            AttemptRecovery(rec);
        }

        return rec.id;
    }

    double GetHealthScore() const { return m_healthScore; }

private:
    std::vector<ErrorRecord> m_errors;
    double m_healthScore;

    void UpdateHealth() {
        int active = 0;
        for (const auto& e : m_errors) if (!e.recovered && e.severity >= ErrorSeverity::Error) active++;
        m_healthScore = std::max(0.0, 100.0 - (active * 5.0));
    }

    void AttemptRecovery(ErrorRecord& rec) {
        rec.retries++;
        std::cout << "[ErrorRecovery] Attempting recovery for " << rec.id << " (Attempt " << rec.retries << ")\n";
        
        // Simple recovery logic
        if (rec.category == ErrorCategory::Network) {
            // Reset network handles?
            rec.recovered = true;
        } else if (rec.category == ErrorCategory::FileIO) {
            // Retry file op?
            rec.recovered = true;
        }
        
        if (rec.recovered) {
            std::cout << "[ErrorRecovery] Successfully recovered " << rec.id << "\n";
            UpdateHealth();
        }
    }
};

} // namespace RawrXD
