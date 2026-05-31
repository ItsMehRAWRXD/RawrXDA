// live_parameter_tuning.h - Live parameter tuning interface for real-time adjustment
// Allows changing scheduler, arbitration, and heat map parameters while running
//
// This is what makes the system engineerable instead of experimental.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "final_production_pipeline.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Parameter type
enum class ParameterType : uint8_t {
    FLOAT = 0,
    INT = 1,
    BOOL = 2,
    STRING = 3,
    ENUM = 4,
};

// Parameter range
struct ParameterRange {
    float min_float = 0.0f;
    float max_float = 1.0f;
    int min_int = 0;
    int max_int = 100;
    std::vector<std::string> enum_values;
};

// Parameter definition
struct ParameterDef {
    std::string name;
    std::string description;
    std::string category;  // "scheduler", "arbitration", "kv", etc.
    ParameterType type;
    ParameterRange range;
    bool requires_restart;
    bool is_hot_swappable;
};

// Parameter value
struct ParameterValue {
    std::string name;
    ParameterType type;
    float float_value;
    int int_value;
    bool bool_value;
    std::string string_value;
    std::chrono::steady_clock::time_point changed_at;
};

// Parameter change callback
using ParameterChangeCallback = std::function<void(const std::string& name, const ParameterValue& value)>;

// Live parameter tuner
class LiveParameterTuner {
public:
    LiveParameterTuner(FinalProductionPipeline* pipeline);
    ~LiveParameterTuner();
    
    // Register parameter
    void RegisterParameter(const ParameterDef& def);
    
    // Get parameter value
    ParameterValue GetParameter(const std::string& name) const;
    
    // Set parameter value (hot-swap if possible)
    bool SetParameter(const std::string& name, const ParameterValue& value);
    
    // Set parameter by string (parses based on type)
    bool SetParameterString(const std::string& name, const std::string& value);
    
    // Get all parameter definitions
    std::vector<ParameterDef> GetAllParameters() const;
    
    // Get parameters by category
    std::vector<ParameterDef> GetParametersByCategory(const std::string& category) const;
    
    // Register change callback
    void RegisterCallback(const std::string& name, ParameterChangeCallback callback);
    
    // Unregister callback
    void UnregisterCallback(const std::string& name);
    
    // Get parameter change history
    std::vector<ParameterValue> GetChangeHistory(const std::string& name, int count) const;
    
    // Export all parameters to JSON
    std::string ExportParametersJSON() const;
    
    // Import parameters from JSON
    bool ImportParametersJSON(const std::string& json);
    
    // Reset parameter to default
    bool ResetParameter(const std::string& name);
    
    // Reset all parameters
    void ResetAllParameters();
    
    // Validate parameter value
    bool ValidateParameter(const std::string& name, const ParameterValue& value) const;
    
private:
    // Apply parameter to pipeline
    bool ApplyParameter(const std::string& name, const ParameterValue& value);
    
    // Members
    FinalProductionPipeline* pipeline_;
    
    mutable std::mutex params_mutex_;
    std::unordered_map<std::string, ParameterDef> definitions_;
    std::unordered_map<std::string, ParameterValue> values_;
    std::unordered_map<std::string, ParameterValue> defaults_;
    std::unordered_map<std::string, std::vector<ParameterValue>> history_;
    std::unordered_map<std::string, ParameterChangeCallback> callbacks_;
};

} // namespace RawrXD