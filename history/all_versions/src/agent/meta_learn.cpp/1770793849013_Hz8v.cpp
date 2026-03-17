/**
 * @file meta_learn.cpp
 * @brief Performance database for quantization/kernel auto-tuning (Qt-free)
 *
 * Stores perf records in a JSON file. Uses Win32 / POSIX for system info
 * and CryptoAPI / openssl for SHA-256 hardware hashing.
 */
#include "meta_learn.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <wincrypt.h>
#  pragma comment(lib, "advapi32.lib")
#else
#  include <unistd.h>
#endif

#include <nlohmann/json.hpp>
namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

int64_t currentMsEpoch() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

std::string ensureDatabasePath() {
    std::string base;
#ifdef _WIN32
    const char* ad = std::getenv("LOCALAPPDATA");
    if (ad) base = std::string(ad) + "\\RawrXD";
#else
    const char* home = std::getenv("HOME");
    if (home) base = std::string(home) + "/.local/share/RawrXD";
#endif
    if (base.empty()) base = ".rawrxd";

    fs::create_directories(base);
    return (fs::path(base) / "perf_db.json").string();
}

std::string defaultGpuLabel() {
#ifdef _WIN32
    char buf[256]{};
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) return buf;
#else
    char buf[256]{};
    if (gethostname(buf, sizeof(buf)) == 0) return buf;
#endif
    return "unknown-gpu";
}

std::string sha256Hex(const std::string& input) {
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return {};
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return {};
    }
    CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.data()),
                  static_cast<DWORD>(input.size()), 0);
    DWORD hashLen = 32;
    uint8_t hash[32]{};
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    static const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (DWORD i = 0; i < hashLen; ++i) {
        result += hex[hash[i] >> 4];
        result += hex[hash[i] & 0xF];
    }
    return result.substr(0, 32); // first 16 bytes hex
#else
    // Fallback: trivial FNV-style hash (replace with openssl if available)
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : input) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char buf[17]{};
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
    return buf;
#endif
}

std::string computeHardwareHash() {
    std::string payload;
#ifdef _WIN32
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    payload += "arch=" + std::to_string(si.wProcessorArchitecture) + "|";
    char buf[256]{};
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) payload += std::string("host=") + buf + "|";
#else
    char buf[256]{};
    if (gethostname(buf, sizeof(buf)) == 0) payload += std::string("host=") + buf + "|";
#endif
    payload += "threads=" + std::to_string(std::thread::hardware_concurrency());
    return sha256Hex(payload);
}

} // namespace

// ---------------------------------------------------------------------------
// MetaLearn implementation
// ---------------------------------------------------------------------------
MetaLearn::MetaLearn()
    : m_dbPath(ensureDatabasePath())
{
    loadDatabase();
}

std::string MetaLearn::gpuHash() const { return hardwareKey(); }

std::string MetaLearn::hardwareKey() const { return computeHardwareHash(); }

std::string MetaLearn::resolveGpuLabel(const std::string& explicitGpu) const {
    std::string t = trim(explicitGpu);
    return t.empty() ? defaultGpuLabel() : t;
}

json MetaLearn::loadDB(bool* ok) {
    if (ok) *ok = true;

    std::string path = ensureDatabasePath();
    if (!fs::exists(path)) return json::array();

    std::ifstream f(path);
    if (!f.is_open()) {
        if (ok) *ok = false;
        fprintf(stderr, "[WARN] [MetaLearn] Failed to open %s\n", path.c_str());
        return json::array();
    }

    json doc;
    try {
        doc = json::parse(f);
    } catch (const json::exception&) {
        if (ok) *ok = false;
        fprintf(stderr, "[WARN] [MetaLearn] Invalid JSON in %s\n", path.c_str());
        return json::array();
    }

    return doc.is_array() ? doc : json::array();
}

bool MetaLearn::record(const std::string& quant,
                       const std::string& kernel,
                       const std::string& gpu,
                       double tps, double ppl) {
    PerfRecord rec;
    rec.quant    = toUpper(trim(quant));
    rec.kernel   = toUpper(trim(kernel));
    rec.gpu      = resolveGpuLabel(gpu);
    rec.hardware = hardwareKey();
    rec.tps      = (std::isfinite(tps) && tps > 0.0) ? tps : 0.0;
    rec.ppl      = (std::isfinite(ppl) && ppl > 0.0) ? ppl : 0.0;
    rec.timestamp = currentMsEpoch();

    if (rec.quant.empty())  rec.quant  = "UNKNOWN";
    if (rec.kernel.empty()) rec.kernel = "UNKNOWN";

    m_records.push_back(rec);
    if (onRecordAdded) onRecordAdded(rec);

    if (!saveDatabase()) {
        m_records.pop_back();
        fprintf(stderr, "[WARN] [MetaLearn] Failed to persist record\n");
        return false;
    }
    return true;
}

bool MetaLearn::autoTuneQuant() {
    std::string best;
    double avgTps = 0, avgPpl = 0;
    if (!computeQuantSuggestion(&best, &avgTps, &avgPpl)) return false;
    if (m_lastQuantSuggestion == best) return true;
    m_lastQuantSuggestion = best;
    if (onSuggestionReady) onSuggestionReady(best);
    fprintf(stderr, "[INFO] [MetaLearn] Auto-selected quant: %s (TPS: %.1f, PPL: %.2f)\n",
            best.c_str(), avgTps, avgPpl);
    return true;
}

bool MetaLearn::autoTuneKernel() {
    std::string best;
    double avgTps = 0;
    if (!computeKernelSuggestion(&best, &avgTps)) return false;
    if (m_lastKernelSuggestion == best) return true;
    m_lastKernelSuggestion = best;
    if (onKernelSuggestionReady) onKernelSuggestionReady(best);
    fprintf(stderr, "[INFO] [MetaLearn] Auto-selected kernel: %s (TPS: %.1f)\n",
            best.c_str(), avgTps);
    return true;
}

std::string MetaLearn::suggestQuant() const {
    std::string best; double t, p;
    return computeQuantSuggestion(&best, &t, &p) ? best : "Q4_0";
}

std::string MetaLearn::suggestKernel() const {
    std::string best; double t;
    return computeKernelSuggestion(&best, &t) ? best : "AVX2";
}

std::vector<PerfRecord> MetaLearn::getHistory(const std::string& quant) const {
    if (quant.empty()) return m_records;
    std::string qUp = toUpper(trim(quant));
    std::vector<PerfRecord> out;
    for (const auto& r : m_records)
        if (r.quant == qUp) out.push_back(r);
    return out;
}

bool MetaLearn::loadDatabase() {
    m_records.clear();
    bool ok = false;
    json arr = MetaLearn::loadDB(&ok);
    if (!ok) return false;
    if (arr.empty()) return true;

    m_records.reserve(arr.size());
    for (const auto& obj : arr) {
        PerfRecord rec;
        rec.quant    = toUpper(trim(obj.value("quant", "")));
        rec.kernel   = toUpper(trim(obj.value("kernel", "")));
        rec.gpu      = obj.value("gpu", "");
        rec.hardware = obj.value("sha256", obj.value("hardware", ""));
        rec.tps      = obj.value("tps", 0.0);
        rec.ppl      = obj.value("ppl", 0.0);
        rec.timestamp = obj.value("when", static_cast<int64_t>(0));

        if (rec.quant.empty())    rec.quant    = "UNKNOWN";
        if (rec.kernel.empty())   rec.kernel   = "UNKNOWN";
        if (rec.gpu.empty())      rec.gpu      = defaultGpuLabel();
        if (rec.hardware.empty()) rec.hardware = hardwareKey();
        if (rec.timestamp <= 0)   rec.timestamp = currentMsEpoch();

        m_records.push_back(rec);
    }

    fprintf(stderr, "[INFO] [MetaLearn] Loaded %zu records\n", m_records.size());
    return true;
}

bool MetaLearn::saveDatabase() const {
    json arr = json::array();
    for (const auto& rec : m_records) {
        arr.push_back({
            {"quant",   rec.quant},
            {"kernel",  rec.kernel},
            {"gpu",     rec.gpu},
            {"sha256",  rec.hardware},
            {"tps",     rec.tps},
            {"ppl",     rec.ppl},
            {"when",    rec.timestamp}
        });
    }

    fs::create_directories(fs::path(m_dbPath).parent_path());
    std::ofstream f(m_dbPath, std::ios::trunc);
    if (!f.is_open()) {
        fprintf(stderr, "[WARN] [MetaLearn] Cannot write %s\n", m_dbPath.c_str());
        return false;
    }
    f << arr.dump();
    return f.good();
}

bool MetaLearn::computeQuantSuggestion(std::string* bestQuant,
                                       double* avgTps,
                                       double* avgPpl) const {
    struct S { double sumTps = 0, sumPpl = 0; int count = 0; };
    std::string key = hardwareKey();
    std::unordered_map<std::string, S> stats;

    for (const auto& r : m_records) {
        if (!r.hardware.empty() && r.hardware != key) continue;
        if (r.quant.empty()) continue;
        if (!std::isfinite(r.tps) || r.tps <= 0) continue;
        if (!std::isfinite(r.ppl) || r.ppl <= 0) continue;
        auto& e = stats[r.quant];
        e.sumTps += r.tps; e.sumPpl += r.ppl; e.count++;
    }

    if (stats.empty()) {
        if (bestQuant) bestQuant->clear();
        if (avgTps) *avgTps = 0; if (avgPpl) *avgPpl = 0;
        return false;
    }

    double bestPplVal = std::numeric_limits<double>::max();
    for (const auto& [_, v] : stats) {
        if (!v.count) continue;
        bestPplVal = std::min(bestPplVal, v.sumPpl / v.count);
    }

    double pplLimit = bestPplVal * 1.05;
    std::string chosen; double cTps = 0, cPpl = 0; bool found = false;
    for (const auto& [name, v] : stats) {
        if (!v.count) continue;
        double at = v.sumTps / v.count, ap = v.sumPpl / v.count;
        if (ap <= pplLimit && (!found || at > cTps)) {
            found = true; chosen = name; cTps = at; cPpl = ap;
        }
    }

    if (!found) {
        for (const auto& [name, v] : stats) {
            if (!v.count) continue;
            double at = v.sumTps / v.count, ap = v.sumPpl / v.count;
            if (!found || ap < cPpl || (std::abs(ap - cPpl) < 1e-6 && at > cTps)) {
                found = true; chosen = name; cTps = at; cPpl = ap;
            }
        }
    }

    if (!found) {
        if (bestQuant) bestQuant->clear();
        if (avgTps) *avgTps = 0; if (avgPpl) *avgPpl = 0;
        return false;
    }

    if (bestQuant) *bestQuant = chosen;
    if (avgTps) *avgTps = cTps;
    if (avgPpl) *avgPpl = cPpl;
    return true;
}

bool MetaLearn::computeKernelSuggestion(std::string* bestKernel,
                                        double* avgTps) const {
    struct K { double sumTps = 0; int count = 0; };
    std::string key = hardwareKey();
    std::unordered_map<std::string, K> stats;

    for (const auto& r : m_records) {
        if (r.kernel.empty()) continue;
        if (!r.hardware.empty() && r.hardware != key) continue;
        if (!std::isfinite(r.tps) || r.tps <= 0) continue;
        auto& e = stats[r.kernel];
        e.sumTps += r.tps; e.count++;
    }

    if (stats.empty()) {
        if (bestKernel) bestKernel->clear();
        if (avgTps) *avgTps = 0;
        return false;
    }

    std::string chosen; double cTps = 0; bool found = false;
    for (const auto& [name, v] : stats) {
        if (!v.count) continue;
        double at = v.sumTps / v.count;
        if (!found || at > cTps) { found = true; chosen = name; cTps = at; }
    }

    if (!found) {
        if (bestKernel) bestKernel->clear();
        if (avgTps) *avgTps = 0;
        return false;
    }

    if (bestKernel) *bestKernel = chosen;
    if (avgTps) *avgTps = cTps;
    return true;
}
