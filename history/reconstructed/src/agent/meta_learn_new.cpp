#include "meta_learn.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <chrono>
#include <windows.h>
#include <bcrypt.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string ensureDatabasePath() {
    char* appData = nullptr;
    size_t len = 0;
    _dupenv_s(&appData, &len, "APPDATA");
    
    std::string base;
    if (appData && len > 0) {
        base = std::string(appData) + "\\RawrXD";
        free(appData);
    } else {
        char* home = nullptr;
        _dupenv_s(&home, &len, "USERPROFILE");
        if (home && len > 0) {
            base = std::string(home) + "\\.rawrxd";
            free(home);
        } else {
            base = ".rawrxd";
        }
    }
    
    fs::create_directories(base);
    return base + "\\perf_db.json";
}

std::string defaultGpuLabel() {
    return "unknown-gpu";
}

std::string computeHardwareHash() {
    char hostname[256] = {0};
    DWORD size = sizeof(hostname);
    GetComputerNameA(hostname, &size);
    
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    std::string payload = std::string(hostname) + "|" + 
                         std::to_string(sysInfo.dwNumberOfProcessors);
    
    // Use BCrypt for SHA-256
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbHash = 0;
    DWORD cbData = 0;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) {
        return "unknown-hash";
    }
    
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
    
    std::vector<BYTE> hash(cbHash);
    
    if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0) {
        BCryptHashData(hHash, (PBYTE)payload.data(), (ULONG)payload.size(), 0);
        BCryptFinishHash(hHash, hash.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    // Convert to hex string (first 32 chars)
    std::stringstream ss;
    for (size_t i = 0; i < std::min(size_t(16), hash.size()); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

} // namespace

std::vector<PerfRecord> MetaLearn::loadDB(bool* ok) {
    if (ok) {
        *ok = true;
    }
    
    std::string path = ensureDatabasePath();
    if (!fs::exists(path)) {
        return {};
    }
    
    std::ifstream f(path);
    if (!f) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    
    json j;
    try {
        f >> j;
    } catch (...) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    f.close();
    
    if (!j.is_array()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    
    std::vector<PerfRecord> records;
    for (const auto& item : j) {
        PerfRecord rec;
        
        if (item.contains("quant") && item["quant"].is_string()) {
            std::string quant = item["quant"];
            std::transform(quant.begin(), quant.end(), quant.begin(), ::toupper);
            rec.quant = quant.empty() ? "UNKNOWN" : quant;
        } else {
            rec.quant = "UNKNOWN";
        }
        
        if (item.contains("kernel") && item["kernel"].is_string()) {
            std::string kernel = item["kernel"];
            std::transform(kernel.begin(), kernel.end(), kernel.begin(), ::toupper);
            rec.kernel = kernel.empty() ? "UNKNOWN" : kernel;
        } else {
            rec.kernel = "UNKNOWN";
        }
        
        rec.gpu = item.value("gpu", defaultGpuLabel());
        rec.hardware = item.value("sha256", item.value("hardware", computeHardwareHash()));
        rec.tps = item.value("tps", 0.0);
        rec.ppl = item.value("ppl", 0.0);
        rec.timestamp = item.value("when", std::chrono::system_clock::now().time_since_epoch().count());
        
        records.push_back(rec);
    }
    
    return records;
}

MetaLearn::MetaLearn()
    : m_dbPath(ensureDatabasePath()) {
    loadDatabase();
}

std::string MetaLearn::gpuHash() const {
    return hardwareKey();
}

std::string MetaLearn::hardwareKey() const {
    return computeHardwareHash();
}

std::string MetaLearn::resolveGpuLabel(const std::string& explicitGpu) const {
    // Trim whitespace
    std::string trimmed = explicitGpu;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (!trimmed.empty()) {
        return trimmed;
    }
    return defaultGpuLabel();
}

bool MetaLearn::record(const std::string& quant,
                       const std::string& kernel,
                       const std::string& gpu,
                       double tps,
                       double ppl) {
    PerfRecord rec;
    
    // Convert to uppercase and trim
    std::string quantUpper = quant;
    std::transform(quantUpper.begin(), quantUpper.end(), quantUpper.begin(), ::toupper);
    quantUpper.erase(0, quantUpper.find_first_not_of(" \t\n\r"));
    quantUpper.erase(quantUpper.find_last_not_of(" \t\n\r") + 1);
    rec.quant = quantUpper.empty() ? "UNKNOWN" : quantUpper;
    
    std::string kernelUpper = kernel;
    std::transform(kernelUpper.begin(), kernelUpper.end(), kernelUpper.begin(), ::toupper);
    kernelUpper.erase(0, kernelUpper.find_first_not_of(" \t\n\r"));
    kernelUpper.erase(kernelUpper.find_last_not_of(" \t\n\r") + 1);
    rec.kernel = kernelUpper.empty() ? "UNKNOWN" : kernelUpper;
    
    rec.gpu = resolveGpuLabel(gpu);
    rec.hardware = hardwareKey();
    rec.tps = (std::isfinite(tps) && tps > 0.0) ? tps : 0.0;
    rec.ppl = (std::isfinite(ppl) && ppl > 0.0) ? ppl : 0.0;
    rec.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    m_records.push_back(rec);
    if (onRecordAdded) onRecordAdded(rec);
    
    if (!saveDatabase()) {
        m_records.pop_back();
        return false;
    }
    
    return true;
}

bool MetaLearn::autoTuneQuant() {
    std::string bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        return false;
    }
    
    if (m_lastQuantSuggestion == bestQuant) {
        return true;
    }
    
    m_lastQuantSuggestion = bestQuant;
    if (onSuggestionReady) onSuggestionReady(bestQuant);
    return true;
}

bool MetaLearn::autoTuneKernel() {
    std::string bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        return false;
    }
    
    if (m_lastKernelSuggestion == bestKernel) {
        return true;
    }
    
    m_lastKernelSuggestion = bestKernel;
    if (onKernelSuggestionReady) onKernelSuggestionReady(bestKernel);
    return true;
}

std::string MetaLearn::suggestQuant() const {
    std::string bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        return "Q4_0";
    }
    return bestQuant;
}

std::string MetaLearn::suggestKernel() const {
    std::string bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        return "AVX2";
    }
    return bestKernel;
}

std::vector<PerfRecord> MetaLearn::getHistory(const std::string& quant) const {
    if (quant.empty()) {
        return m_records;
    }
    
    std::vector<PerfRecord> filtered;
    std::string qUpper = quant;
    std::transform(qUpper.begin(), qUpper.end(), qUpper.begin(), ::toupper);
    qUpper.erase(0, qUpper.find_first_not_of(" \t\n\r"));
    qUpper.erase(qUpper.find_last_not_of(" \t\n\r") + 1);
    
    for (const PerfRecord& rec : m_records) {
        if (rec.quant == qUpper) {
            filtered.push_back(rec);
        }
    }
    return filtered;
}

bool MetaLearn::loadDatabase() {
    m_records.clear();
    
    bool ok = false;
    m_records = MetaLearn::loadDB(&ok);
    return ok || m_records.empty();
}

bool MetaLearn::saveDatabase() const {
    json arr = json::array();
    
    for (const PerfRecord& rec : m_records) {
        json obj;
        obj["quant"] = rec.quant;
        obj["kernel"] = rec.kernel;
        obj["gpu"] = rec.gpu;
        obj["sha256"] = rec.hardware;
        obj["tps"] = rec.tps;
        obj["ppl"] = rec.ppl;
        obj["when"] = rec.timestamp;
        arr.push_back(obj);
    }
    
    fs::path dbPath(m_dbPath);
    fs::create_directories(dbPath.parent_path());
    
    std::ofstream f(m_dbPath);
    if (!f) {
        return false;
    }
    
    f << arr.dump(2);
    f.close();
    
    return true;
}

bool MetaLearn::computeQuantSuggestion(std::string* bestQuant,
                                       double* avgTps,
                                       double* avgPpl) const {
    struct QuantStats {
        double sumTps = 0.0;
        double sumPpl = 0.0;
        int count = 0;
    };
    
    const std::string key = hardwareKey();
    std::unordered_map<std::string, QuantStats> stats;
    
    for (const PerfRecord& rec : m_records) {
        if (!rec.hardware.empty() && rec.hardware != key) {
            continue;
        }
        if (rec.quant.empty()) {
            continue;
        }
        if (!std::isfinite(rec.tps) || rec.tps <= 0.0) {
            continue;
        }
        if (!std::isfinite(rec.ppl) || rec.ppl <= 0.0) {
            continue;
        }
        
        QuantStats& entry = stats[rec.quant];
        entry.sumTps += rec.tps;
        entry.sumPpl += rec.ppl;
        entry.count += 1;
    }
    
    if (stats.empty()) {
        if (bestQuant) {
            bestQuant->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        if (avgPpl) {
            *avgPpl = 0.0;
        }
        return false;
    }
    
    double bestPplValue = std::numeric_limits<double>::max();
    for (const auto& [k, v] : stats) {
        if (!v.count) {
            continue;
        }
        const double avg = v.sumPpl / v.count;
        bestPplValue = std::min(bestPplValue, avg);
    }
    
    const double pplLimit = bestPplValue * 1.05;
    std::string chosen;
    double chosenTps = 0.0;
    double chosenPpl = 0.0;
    bool found = false;
    
    for (const auto& [k, v] : stats) {
        if (!v.count) {
            continue;
        }
        const double avgT = v.sumTps / v.count;
        const double avgP = v.sumPpl / v.count;
        if (avgP <= pplLimit && (!found || avgT > chosenTps)) {
            found = true;
            chosen = k;
            chosenTps = avgT;
            chosenPpl = avgP;
        }
    }
    
    if (!found) {
        for (const auto& [k, v] : stats) {
            if (!v.count) {
                continue;
            }
            const double avgT = v.sumTps / v.count;
            const double avgP = v.sumPpl / v.count;
            if (!found || avgP < chosenPpl || (std::abs(avgP - chosenPpl) < 1e-6 && avgT > chosenTps)) {
                found = true;
                chosen = k;
                chosenTps = avgT;
                chosenPpl = avgP;
            }
        }
    }
    
    if (!found) {
        if (bestQuant) {
            bestQuant->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        if (avgPpl) {
            *avgPpl = 0.0;
        }
        return false;
    }
    
    if (bestQuant) {
        *bestQuant = chosen;
    }
    if (avgTps) {
        *avgTps = chosenTps;
    }
    if (avgPpl) {
        *avgPpl = chosenPpl;
    }
    return true;
}

bool MetaLearn::computeKernelSuggestion(std::string* bestKernel,
                                        double* avgTps) const {
    struct KernelStats {
        double sumTps = 0.0;
        int count = 0;
    };
    
    const std::string key = hardwareKey();
    std::unordered_map<std::string, KernelStats> stats;
    
    for (const PerfRecord& rec : m_records) {
        if (rec.kernel.empty()) {
            continue;
        }
        if (!rec.hardware.empty() && rec.hardware != key) {
            continue;
        }
        if (!std::isfinite(rec.tps) || rec.tps <= 0.0) {
            continue;
        }
        
        KernelStats& entry = stats[rec.kernel];
        entry.sumTps += rec.tps;
        entry.count += 1;
    }
    
    if (stats.empty()) {
        if (bestKernel) {
            bestKernel->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        return false;
    }
    
    std::string chosen;
    double chosenTps = 0.0;
    bool found = false;
    
    for (const auto& [k, v] : stats) {
        if (!v.count) {
            continue;
        }
        const double avgT = v.sumTps / v.count;
        if (!found || avgT > chosenTps) {
            found = true;
            chosen = k;
            chosenTps = avgT;
        }
    }
    
    if (!found) {
        if (bestKernel) {
            bestKernel->clear();
        }
        if (avgTps) {
            *avgTps = 0.0;
        }
        return false;
    }
    
    if (bestKernel) {
        *bestKernel = chosen;
    }
    if (avgTps) {
        *avgTps = chosenTps;
    }
    return true;
}
