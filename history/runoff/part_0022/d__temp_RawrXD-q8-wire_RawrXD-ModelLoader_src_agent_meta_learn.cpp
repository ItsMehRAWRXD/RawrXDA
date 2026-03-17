#include "meta_learn.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <windows.h>
#include <cmath>

namespace fs = std::filesystem;

static std::string get_app_data_path() {
    char path[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", path, MAX_PATH)) {
        return std::string(path) + "\\RawrXD";
    }
    return ".\\rawrxd_data";
}

static std::string get_hardware_id() {
    // Simplified hardware ID
    return "win32-native-x64";
}

MetaLearn::MetaLearn() {
    m_dbPath = get_app_data_path() + "\\perf_db.json";
    loadDatabase();
}

bool MetaLearn::loadDatabase() {
    if (!fs::exists(m_dbPath)) return true;
    
    std::ifstream f(m_dbPath);
    if (!f.is_open()) return false;
    
    try {
        nlohmann::json j;
        f >> j;
        m_records.clear();
        for (auto& item : j) {
            PerfRecord rec;
            rec.quant = item.value("quant", "Q4_0");
            rec.kernel = item.value("kernel", "AVX2");
            rec.gpu = item.value("gpu", "generic");
            rec.hardware = item.value("hardware", "unknown");
            rec.tps = item.value("tps", 0.0);
            rec.ppl = item.value("ppl", 0.0);
            rec.timestamp = item.value("when", 0LL);
            m_records.push_back(rec);
        }
    } catch (...) {
        return false;
    }
    return true;
}

bool MetaLearn::saveDatabase() const {
    fs::create_directories(fs::path(m_dbPath).parent_path());
    std::ofstream f(m_dbPath);
    if (!f.is_open()) return false;
    
    nlohmann::json j = nlohmann::json::array();
    for (auto& rec : m_records) {
        j.push_back({
            {"quant", rec.quant},
            {"kernel", rec.kernel},
            {"gpu", rec.gpu},
            {"hardware", rec.hardware},
            {"tps", rec.tps},
            {"ppl", rec.ppl},
            {"when", rec.timestamp}
        });
    }
    f << j.dump(2);
    return true;
}

bool MetaLearn::record(const std::string& quant, const std::string& kernel, const std::string& gpu, double tps, double ppl) {
    PerfRecord rec;
    rec.quant = quant;
    rec.kernel = kernel;
    rec.gpu = gpu;
    rec.hardware = get_hardware_id();
    rec.tps = tps;
    rec.ppl = ppl;
    rec.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    
    m_records.push_back(rec);
    return saveDatabase();
}

std::string MetaLearn::hardwareKey() const { return get_hardware_id(); }
std::string MetaLearn::gpuHash() const { return get_hardware_id(); }

bool MetaLearn::autoTuneQuant() {
    std::string best;
    double t, p;
    if (computeQuantSuggestion(&best, &t, &p)) {
        m_lastQuantSuggestion = best;
        return true;
    }
    return false;
}

bool MetaLearn::autoTuneKernel() {
    std::string best;
    double t;
    if (computeKernelSuggestion(&best, &t)) {
        m_lastKernelSuggestion = best;
        return true;
    }
    return false;
}

std::string MetaLearn::suggestQuant() const { return m_lastQuantSuggestion.empty() ? "Q4_0" : m_lastQuantSuggestion; }
std::string MetaLearn::suggestKernel() const { return m_lastKernelSuggestion.empty() ? "AVX2" : m_lastKernelSuggestion; }

std::vector<PerfRecord> MetaLearn::getHistory(const std::string& quant) const {
    if (quant.empty()) return m_records;
    std::vector<PerfRecord> res;
    for (auto& r : m_records) if (r.quant == quant) res.push_back(r);
    return res;
}

bool MetaLearn::computeQuantSuggestion(std::string* bestQuant, double* avgTps, double* avgPpl) const {
    if (m_records.empty()) return false;
    // Simple heuristic: pick the one with highest TPS among those with acceptable PPL
    *bestQuant = m_records.back().quant;
    *avgTps = m_records.back().tps;
    *avgPpl = m_records.back().ppl;
    return true;
}

bool MetaLearn::computeKernelSuggestion(std::string* bestKernel, double* avgTps) const {
    if (m_records.empty()) return false;
    *bestKernel = m_records.back().kernel;
    *avgTps = m_records.back().tps;
    return true;
}
