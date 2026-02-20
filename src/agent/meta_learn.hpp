#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct PerfRecord {
    std::string quant;
    std::string kernel;
    std::string gpu;
    std::string hardware;
    double tps = 0.0;
    double ppl = 0.0;
    int64_t timestamp = 0;
};

/// Callback typedefs (function pointers, not std::function)
typedef void (*OnRecordAddedCb)(const PerfRecord& rec);
typedef void (*OnSuggestionCb)(const char* suggestion);

class MetaLearn {
public:
    MetaLearn();
    bool record(const std::string& quant, const std::string& kernel, const std::string& gpu, double tps, double ppl);
    bool autoTuneQuant();
    bool autoTuneKernel();
    std::string suggestQuant() const;
    std::string suggestKernel() const;
    std::vector<PerfRecord> getHistory(const std::string& quant = "") const;
    bool loadDatabase();
    bool saveDatabase() const;
    std::string gpuHash() const;

    /// Load perf_db.json and return parsed records (no nlohmann dependency)
    static std::vector<PerfRecord> loadDB(bool* ok = nullptr);

    // Callbacks (function pointers per project convention)
    OnRecordAddedCb onRecordAdded = nullptr;
    OnSuggestionCb onSuggestionReady = nullptr;
    OnSuggestionCb onKernelSuggestionReady = nullptr;

private:
    std::string resolveGpuLabel(const std::string& explicitGpu) const;
    bool computeQuantSuggestion(std::string* bestQuant, double* avgTps, double* avgPpl) const;
    bool computeKernelSuggestion(std::string* bestKernel, double* avgTps) const;
    std::string hardwareKey() const;
    std::vector<PerfRecord> m_records;
    std::string m_dbPath;
    std::string m_lastQuantSuggestion;
    std::string m_lastKernelSuggestion;
};
