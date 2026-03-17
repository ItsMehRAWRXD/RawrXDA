#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <map>

struct PerfRecord {
    std::string quant;
    std::string kernel;
    std::string gpu;
    std::string hardware;
    double tps = 0.0;      // tokens per second
    double ppl = 0.0;      // perplexity
    int64_t timestamp = 0;
};

class MetaLearn {
public:
    explicit MetaLearn();
    
    // Record performance metrics to database
    bool record(const std::string& quant,
                const std::string& kernel,
                const std::string& gpu,
                double tps,
                double ppl);
    
    // Auto-apply the best quantization/kernel for this machine
    bool autoTuneQuant();
    bool autoTuneKernel();
    
    // Suggestions
    std::string suggestQuant() const;
    std::string suggestKernel() const;
    
    // Query
    std::vector<PerfRecord> getHistory(const std::string& quant = "") const;
    
    std::string hardwareKey() const;
    std::string gpuHash() const;

private:
    bool loadDatabase();
    bool saveDatabase() const;
    
    bool computeQuantSuggestion(std::string* bestQuant, double* avgTps, double* avgPpl) const;
    bool computeKernelSuggestion(std::string* bestKernel, double* avgTps) const;
    
    std::string m_dbPath;
    std::vector<PerfRecord> m_records;
    std::string m_lastQuantSuggestion;
    std::string m_lastKernelSuggestion;
};
