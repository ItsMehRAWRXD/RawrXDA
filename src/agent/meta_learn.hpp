#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>

struct PerfRecord {
    std::string quant;
    std::string kernel;
    std::string gpu;
    std::string hardware;
    double tps;      // tokens per second
    double ppl;      // perplexity
    int64_t timestamp;
};

class MetaLearn {
public:
    explicit MetaLearn();
    
    virtual ~MetaLearn() = default;

    // Record performance metrics to database
    bool record(const std::string& quant,
                const std::string& kernel,
                const std::string& gpu,
                double tps,
                double ppl);
    
    // Auto-apply the best quantization/kernel for this machine
    bool autoTuneQuant();
    bool autoTuneKernel();
    
    // Suggestions without side effects
    std::string suggestQuant() const;
    std::string suggestKernel() const;
    
    // Get performance history
    std::vector<PerfRecord> getHistory(const std::string& quant = "") const;
    
    // Load database from disk
    bool loadDatabase();
    
    // Save database to disk
    bool saveDatabase() const;

    // Hardware fingerprint helper
    std::string gpuHash() const;

    // Lightweight static helper for callers needing raw records
    static nlohmann::json loadDB(bool* ok = nullptr);
    
    // Callbacks (replacing signals)
    std::function<void(const PerfRecord&)> onRecordAdded;
    std::function<void(const std::string&)> onSuggestionReady;
    std::function<void(const std::string&)> onKernelSuggestionReady;
    
private:
    std::string resolveGpuLabel(const std::string& explicitGpu) const;
    bool computeQuantSuggestion(std::string* bestQuant,
                                double* avgTps,
                                double* avgPpl) const;
    bool computeKernelSuggestion(std::string* bestKernel,
                                 double* avgTps) const;
    std::string hardwareKey() const;
    std::vector<PerfRecord> m_records;
    std::string m_dbPath;
    std::string m_lastQuantSuggestion;
    std::string m_lastKernelSuggestion;
};
