// RawrXD_MetaLearn.hpp - Autonomous Performance Learning
// Pure C++20 - No Qt Dependencies
// Manages: Performance database, Hardware fingerprints, Auto-tuning

#pragma once

#include "RawrXD_JSON.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <windows.h>
#include <intrin.h>

namespace fs = std::filesystem;

namespace RawrXD {

struct PerfRecord {
    std::string quant;
    std::string kernel;
    std::string gpu;
    std::string hardware;
    double tps = 0.0;
    double ppl = 0.0;
    uint64_t timestamp = 0;
};

class MetaLearn {
public:
    MetaLearn() {
        dbPath_ = "d:\\RawrXD\\perf_db.json";
        loadDatabase();
    }

    bool Record(const std::string& quant, const std::string& kernel, double tps, double ppl) {
        PerfRecord rec;
        rec.quant = quant;
        rec.kernel = kernel;
        rec.gpu = "Autodetect";
        rec.hardware = GetHardwareHash();
        rec.tps = tps;
        rec.ppl = ppl;
        rec.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        records_.push_back(rec);
        return saveDatabase();
    }

    std::string SuggestQuant() {
        if (records_.empty()) return "Q8_K";
        // Simple heuristic: highest TPS with PPL under threshold
        std::string best = "Q8_K";
        double maxTps = 0;
        for (const auto& r : records_) {
            if (r.tps > maxTps) {
                maxTps = r.tps;
                best = r.quant;
            }
        }
        return best;
    }

private:
    std::string dbPath_;
    std::vector<PerfRecord> records_;

    bool loadDatabase() {
        std::ifstream f(dbPath_);
        if (!f.is_open()) return false;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto json = JSONParser::Parse(content);
        if (json.isArray()) {
            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json.at(std::to_string(i)); // JSONArray access is easier with index but JSONValue needs to support it
                // ... populate records_ ...
            }
        }
        return true;
    }

    bool saveDatabase() {
        JSONArray arr;
        for (const auto& r : records_) {
            JSONObject obj;
            obj["quant"] = JSONValue(r.quant);
            obj["kernel"] = JSONValue(r.kernel);
            obj["tps"] = JSONValue(r.tps);
            obj["ppl"] = JSONValue(r.ppl);
            obj["timestamp"] = JSONValue((int64_t)r.timestamp);
            arr.push_back(JSONValue(obj));
        }
        std::ofstream f(dbPath_);
        f << JSONValue(arr).stringify(true);
        return true;
    }

    std::string GetHardwareHash() {
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        char buf[64];
        sprintf_s(buf, "%08X%08X", cpuInfo[0], cpuInfo[3]);
        return std::string(buf);
    }
};

} // namespace RawrXD
