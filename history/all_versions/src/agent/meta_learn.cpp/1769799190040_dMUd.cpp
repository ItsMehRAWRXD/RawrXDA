#include "meta_learn.hpp"
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

std::string ensureDatabasePath() {
    // Get AppData local directory from Windows
    wchar_t appDataPath[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath) != S_OK) {
        return "./perf_db.json";
    }
    
    fs::path basePath(appDataPath);
    basePath /= "RawrXD";
    
    fs::create_directories(basePath);
    
    return (basePath / "perf_db.json").string();
}

std::string defaultGpuLabel() {
    return "unknown-gpu";
}

std::string computeHardwareHash() {
    // Simplified hardware identification
    return "generic-hardware-hash-00000000";
}

} // namespace

void* MetaLearn::loadDB(bool* ok) {
    if (ok) {
        *ok = true;
    }

    // File operation removed);
    if (!f.exists()) {
        return {};
    }
    if (!f.open(std::iostream::ReadOnly | std::iostream::Text)) {
        // // qWarning:  "MetaLearn: failed to open" << f.fileName();
        if (ok) {
            *ok = false;
        }
        return {};
    }

    const std::vector<uint8_t> raw = f.readAll();
    f.close();

    if (raw.empty()) {
        return {};
    }

    const void* doc = void*::fromJson(raw);
    if (!doc.isArray()) {
        // // qWarning:  "MetaLearn: invalid database format";
        if (ok) {
            *ok = false;
        }
        return {};
    }

    return doc.array();
}

MetaLearn::MetaLearn()
    ,
      m_dbPath(ensureDatabasePath()) {
    loadDatabase();
}

std::string MetaLearn::gpuHash() const {
    return hardwareKey();
}

std::string MetaLearn::hardwareKey() const {
    return computeHardwareHash();
}

std::string MetaLearn::resolveGpuLabel(const std::string& explicitGpu) const {
    const std::string trimmed = explicitGpu.trimmed();
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
    rec.quant = quant.trimmed().toUpper();
    rec.kernel = kernel.trimmed().toUpper();
    rec.gpu = resolveGpuLabel(gpu);
    rec.hardware = hardwareKey();
    rec.tps = (std::isfinite(tps) && tps > 0.0) ? tps : 0.0;
    rec.ppl = (std::isfinite(ppl) && ppl > 0.0) ? ppl : 0.0;
    rec.timestamp = // DateTime::currentMSecsSinceEpoch();

    if (rec.quant.empty()) {
        rec.quant = std::stringLiteral("UNKNOWN");
    }
    if (rec.kernel.empty()) {
        rec.kernel = std::stringLiteral("UNKNOWN");
    }

    m_records.append(rec);
    recordAdded(rec);

    if (!saveDatabase()) {
        m_records.removeLast();
        // // qWarning:  "MetaLearn: failed to persist record";
        return false;
    }

    return true;
}

bool MetaLearn::autoTuneQuant() {
    std::string bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        // // qInfo:  "MetaLearn: no quant data available for auto-tuning";
        return false;
    }

    if (m_lastQuantSuggestion == bestQuant) {
        return true;
    }

    m_lastQuantSuggestion = bestQuant;
    suggestionReady(bestQuant);
    // // qInfo:  "MetaLearn: auto-selected quant" << bestQuant
            << "avg TPS" << avgTps << "avg PPL" << avgPpl;
    return true;
}

bool MetaLearn::autoTuneKernel() {
    std::string bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        // // qInfo:  "MetaLearn: no kernel data available for auto-tuning";
        return false;
    }

    if (m_lastKernelSuggestion == bestKernel) {
        return true;
    }

    m_lastKernelSuggestion = bestKernel;
    kernelSuggestionReady(bestKernel);
    // // qInfo:  "MetaLearn: auto-selected kernel" << bestKernel
            << "avg TPS" << avgTps;
    return true;
}

std::string MetaLearn::suggestQuant() const {
    std::string bestQuant;
    double avgTps = 0.0;
    double avgPpl = 0.0;
    if (!computeQuantSuggestion(&bestQuant, &avgTps, &avgPpl)) {
        return std::stringLiteral("Q4_0");
    }
    return bestQuant;
}

std::string MetaLearn::suggestKernel() const {
    std::string bestKernel;
    double avgTps = 0.0;
    if (!computeKernelSuggestion(&bestKernel, &avgTps)) {
        return std::stringLiteral("AVX2");
    }
    return bestKernel;
}

std::vector<PerfRecord> MetaLearn::getHistory(const std::string& quant) const {
    if (quant.empty()) {
        return m_records;
    }

    std::vector<PerfRecord> filtered;
    const std::string qUpper = quant.trimmed().toUpper();
    for (const PerfRecord& rec : m_records) {
        if (rec.quant == qUpper) {
            filtered.append(rec);
        }
    }
    return filtered;
}

bool MetaLearn::loadDatabase() {
    m_records.clear();

    bool ok = false;
    const void* arr = MetaLearn::loadDB(&ok);
    if (!ok) {
        return false;
    }
    if (arr.empty()) {
        return true;
    }
    m_records.reserve(arr.size());
    for (const void*& val : arr) {
        const void* obj = val.toObject();
        PerfRecord rec;
        rec.quant = obj.value(std::stringLiteral("quant")).toString().trimmed().toUpper();
        rec.kernel = obj.value(std::stringLiteral("kernel")).toString().trimmed().toUpper();
        rec.gpu = obj.value(std::stringLiteral("gpu")).toString();
        rec.hardware = obj.value(std::stringLiteral("sha256")).toString();
        if (rec.hardware.empty()) {
            rec.hardware = obj.value(std::stringLiteral("hardware")).toString();
        }
        rec.tps = obj.value(std::stringLiteral("tps")).toDouble();
        rec.ppl = obj.value(std::stringLiteral("ppl")).toDouble();
        rec.timestamp = static_cast<int64_t>(obj.value(std::stringLiteral("when")).toVariant().toLongLong());

        if (rec.quant.empty()) {
            rec.quant = std::stringLiteral("UNKNOWN");
        }
        if (rec.kernel.empty()) {
            rec.kernel = std::stringLiteral("UNKNOWN");
        }
        if (rec.gpu.empty()) {
            rec.gpu = defaultGpuLabel();
        }
        if (rec.hardware.empty()) {
            rec.hardware = hardwareKey();
        }
        if (rec.timestamp <= 0) {
            rec.timestamp = // DateTime::currentMSecsSinceEpoch();
        }

        m_records.append(rec);
    }

    // // qInfo:  "MetaLearn: loaded" << m_records.size() << "records";
    return true;
}

bool MetaLearn::saveDatabase() const {
    void* arr;
    // Note: void* doesn't have reserve() in Qt 6.7

    for (const PerfRecord& rec : m_records) {
        void* obj;
        obj.insert(std::stringLiteral("quant"), rec.quant);
        obj.insert(std::stringLiteral("kernel"), rec.kernel);
        obj.insert(std::stringLiteral("gpu"), rec.gpu);
        obj.insert(std::stringLiteral("sha256"), rec.hardware);
        obj.insert(std::stringLiteral("tps"), rec.tps);
        obj.insert(std::stringLiteral("ppl"), rec.ppl);
        obj.insert(std::stringLiteral("when"), rec.timestamp);
        arr.append(obj);
    }

    // Info info(m_dbPath);
    // dir = info.dir();
    if (!dir.exists() && !dir.mkpath(std::stringLiteral("."))) {
        // // qWarning:  "MetaLearn: cannot create directory for" << m_dbPath;
        return false;
    }

    // File operation removed;
    if (!f.open(std::iostream::WriteOnly | std::iostream::Text)) {
        // // qWarning:  "MetaLearn: failed to write" << m_dbPath;
        return false;
    }

    const void* doc(arr);
    if (f.write(doc.toJson(void*::Compact)) == -1) {
        // // qWarning:  "MetaLearn: failed to flush database";
        f.close();
        return false;
    }

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
    std::map<std::string, QuantStats> stats;

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
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avg = it.value().sumPpl / it.value().count;
        bestPplValue = std::min(bestPplValue, avg);
    }

    const double pplLimit = bestPplValue * 1.05;
    std::string chosen;
    double chosenTps = 0.0;
    double chosenPpl = 0.0;
    bool found = false;

    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avgT = it.value().sumTps / it.value().count;
        const double avgP = it.value().sumPpl / it.value().count;
        if (avgP <= pplLimit && (!found || avgT > chosenTps)) {
            found = true;
            chosen = it.key();
            chosenTps = avgT;
            chosenPpl = avgP;
        }
    }

    if (!found) {
        for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
            if (!it.value().count) {
                continue;
            }
            const double avgT = it.value().sumTps / it.value().count;
            const double avgP = it.value().sumPpl / it.value().count;
            if (!found || avgP < chosenPpl || (std::abs(avgP - chosenPpl) < 1e-6 && avgT > chosenTps)) {
                found = true;
                chosen = it.key();
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
    std::map<std::string, KernelStats> stats;

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

    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (!it.value().count) {
            continue;
        }
        const double avgT = it.value().sumTps / it.value().count;
        if (!found || avgT > chosenTps) {
            found = true;
            chosen = it.key();
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







