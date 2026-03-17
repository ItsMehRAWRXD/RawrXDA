#ifndef ENTERPRISE_TELEMETRY_HPP
#define ENTERPRISE_TELEMETRY_HPP

#include <string>
#include <vector>
#include <memory>

class EnterpriseTelemetryPrivate;

class EnterpriseTelemetry {
public:
    static EnterpriseTelemetry* instance();
    
    // OpenTelemetry integration
    void initializeOpenTelemetry();
    void recordMissionSpan(const std::string& missionId, 
                          const std::string& missionType,
                          double durationMs,
                          bool success,
                          const std::string& errorMessage = "");
    void recordToolExecutionSpan(const std::string& toolName,
                                const std::vector<std::string>& parameters,
                                double durationMs,
                                bool success,
                                int exitCode);
    
    // Prometheus metrics export
    void exportToPrometheus();
    
    // Enterprise telemetry configuration
    void setTelemetryEnabled(bool enabled);
    bool isTelemetryEnabled() const;
    
    // Performance metrics
    double getMissionSuccessRate();
    double getToolExecutionSuccessRate();
    
private:
    explicit EnterpriseTelemetry();
    ~EnterpriseTelemetry();
    
    // Helper functions
    std::string generateTraceId();
    std::string generateSpanId();
    
    std::unique_ptr<EnterpriseTelemetryPrivate> d_ptr;
    
    // Disable copying
    EnterpriseTelemetry(const EnterpriseTelemetry&) = delete;
    EnterpriseTelemetry& operator=(const EnterpriseTelemetry&) = delete;
};

#endif // ENTERPRISE_TELEMETRY_HPP