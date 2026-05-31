// inference_parity_trace.h — Structured JSON trace for CLI/UI parity testing
//
// Both `rawrxd run` (CLI) and `runLocalInferencePipeline` (Win32IDE) can emit
// the same envelope describing a single inference call so the streams can be
// diffed structurally, not just by total stdout bytes.
//
// Envelope shape (one JSON object per file):
//   {
//     "source":  "cli" | "ui-pipeline",
//     "model":   "...",
//     "prompt":  "...",
//     "started_ms":   <epoch ms>,
//     "first_token_ms": <delta from started_ms to first non-empty token>,
//     "completed_ms":   <delta from started_ms to done=true>,
//     "tokens":  ["t0", "t1", ...],
//     "token_count": N,
//     "token_us":  [<int64 microseconds since started>, ...]   // parallel to tokens[]
//     "error":   "..." | ""
//   }
//
// Time fields use std::chrono::steady_clock for elapsed deltas (monotonic,
// immune to wall-clock jumps); started_ms is the only system_clock value and
// is kept only for cross-host correlation.
//
// Strings are escaped per RFC 8259 (no external JSON dep needed).
#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::ParityTrace {

struct Recorder {
    std::string source;
    std::string model;
    std::string prompt;
    // Immutable provenance for reproducibility and cross-host parity triage.
    std::string runId;
    std::string buildCommit = "unknown";
    std::string buildConfig = "unknown";
    // Backend identity at the moment of inference. Populated by setBackend()
    // before/after the inference call. Distinguishes traces produced by:
    //   "cpu-fallback"  — RawrXD::ParityFallback::run (deterministic oracle)
    //   "plugin"        — InferencePlugin::generate via DLL bridge
    //   "vulkan" / "cuda" / "hip" — real GPU backend identified at runtime
    //   "unknown"       — capture path did not identify backend
    std::string backend = "unknown";
    // Optional human-readable device description (e.g. "AMD Radeon RX 7900 XTX").
    std::string device;
    // Optional backend/runtime fingerprint details.
    std::string backendVersion;
    std::string driverVersion;
    std::string deviceId;
    // Inference config snapshot used for this generation.
    int maxTokens = -1;
    float temperature = -1.0f;
    int topK = -1;
    float topP = -1.0f;
    int seed = -1;
    std::vector<std::string> tokens;
    // Per-token monotonically-increasing sequence numbers stamped at the
    // recorder boundary. Any async reordering on the UI callback path
    // surfaces as a non-monotonic seq in the trace JSON instead of as a
    // silent token-array divergence.
    std::vector<long long> seq;
    // Per-token elapsed microseconds since start(), captured at the moment
    // the recorder receives the token. Parallel to tokens[]. Computed from
    // steady_clock so the deltas are immune to wall-clock skew.
    std::vector<long long> tokenUs;
    // Per-token inter-arrival delta in microseconds. Parallel to tokens[].
    std::vector<long long> tokenDtUs;
    long long nextSeq = 0;
    long long startedMs = 0;
    long long firstTokenMs = -1;
    long long completedMs = -1;
    std::chrono::steady_clock::time_point startedSteady{};
    std::string error;
    std::mutex mu;

    static long long nextRunSeq() {
        static std::atomic<long long> g_seq{0};
        return g_seq.fetch_add(1, std::memory_order_relaxed);
    }

    static std::string makeRunId(long long startedMs) {
        return std::to_string(startedMs) + "-" + std::to_string(nextRunSeq());
    }

    void setBuildInfo(std::string commit, std::string config) {
        std::lock_guard<std::mutex> lk(mu);
        buildCommit = std::move(commit);
        buildConfig = std::move(config);
    }

    void setBackend(std::string backendName, std::string deviceName = {}) {
        setBackendDetails(std::move(backendName), std::move(deviceName), {}, {}, {});
    }

    void setBackendDetails(std::string backendName,
                           std::string deviceName = {},
                           std::string backendVer = {},
                           std::string driverVer = {},
                           std::string devId = {}) {
        std::lock_guard<std::mutex> lk(mu);
        backend = std::move(backendName);
        device = std::move(deviceName);
        backendVersion = std::move(backendVer);
        driverVersion = std::move(driverVer);
        deviceId = std::move(devId);
    }

    void setInferenceConfig(int maxTok, float temp, int topk = -1, float topp = -1.0f, int seedVal = -1) {
        std::lock_guard<std::mutex> lk(mu);
        maxTokens = maxTok;
        temperature = temp;
        topK = topk;
        topP = topp;
        seed = seedVal;
    }

    static long long systemNowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    long long elapsedMsLocked() const {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - startedSteady).count();
    }

    long long elapsedUsLocked() const {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now() - startedSteady).count();
    }

    void start(std::string src, std::string mdl, std::string p) {
        std::lock_guard<std::mutex> lk(mu);
        source = std::move(src);
        model = std::move(mdl);
        prompt = std::move(p);
        startedMs = systemNowMs();
        runId = makeRunId(startedMs);
        startedSteady = std::chrono::steady_clock::now();
        firstTokenMs = -1;
        completedMs = -1;
        tokens.clear();
        seq.clear();
        tokenUs.clear();
        tokenDtUs.clear();
        nextSeq = 0;
        error.clear();
    }

    void onToken(const std::string& tok, bool /*done*/) {
        std::lock_guard<std::mutex> lk(mu);
        if (!tok.empty()) {
            const long long us = elapsedUsLocked();
            if (firstTokenMs < 0) firstTokenMs = us / 1000;
            tokens.push_back(tok);
            seq.push_back(nextSeq++);
            const long long prevUs = tokenUs.empty() ? 0 : tokenUs.back();
            tokenUs.push_back(us);
            tokenDtUs.push_back(tokenUs.size() == 1 ? us : (us - prevUs));
        }
    }

    void onComplete() {
        std::lock_guard<std::mutex> lk(mu);
        if (completedMs < 0) completedMs = elapsedMsLocked();
    }

    void onError(const std::string& msg) {
        std::lock_guard<std::mutex> lk(mu);
        error = msg;
        if (completedMs < 0) completedMs = elapsedMsLocked();
    }
};

inline void appendEscaped(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

// Returns true if the trace was written successfully.
inline bool writeJson(const Recorder& r, const std::string& path) {
    std::string buf;
    buf.reserve(4096 + r.tokens.size() * 8);
    buf += "{\n  \"source\": ";    appendEscaped(buf, r.source);
    buf += ",\n  \"model\": ";     appendEscaped(buf, r.model);
    buf += ",\n  \"prompt\": ";    appendEscaped(buf, r.prompt);
    buf += ",\n  \"run_id\": ";    appendEscaped(buf, r.runId);
    buf += ",\n  \"build_commit\": "; appendEscaped(buf, r.buildCommit);
    buf += ",\n  \"build_config\": "; appendEscaped(buf, r.buildConfig);
    buf += ",\n  \"backend\": ";   appendEscaped(buf, r.backend);
    buf += ",\n  \"device\": ";    appendEscaped(buf, r.device);
    buf += ",\n  \"backend_version\": "; appendEscaped(buf, r.backendVersion);
    buf += ",\n  \"driver_version\": "; appendEscaped(buf, r.driverVersion);
    buf += ",\n  \"device_id\": "; appendEscaped(buf, r.deviceId);
    buf += ",\n  \"started_ms\": " + std::to_string(r.startedMs);
    buf += ",\n  \"first_token_ms\": " + std::to_string(r.firstTokenMs);
    buf += ",\n  \"completed_ms\": " + std::to_string(r.completedMs);
    buf += ",\n  \"token_count\": " + std::to_string(r.tokens.size());
    buf += ",\n  \"max_tokens\": " + std::to_string(r.maxTokens);
    buf += ",\n  \"temperature\": " + std::to_string(r.temperature);
    buf += ",\n  \"top_k\": " + std::to_string(r.topK);
    buf += ",\n  \"top_p\": " + std::to_string(r.topP);
    buf += ",\n  \"seed\": " + std::to_string(r.seed);
    buf += ",\n  \"error\": ";     appendEscaped(buf, r.error);
    buf += ",\n  \"tokens\": [";
    for (size_t i = 0; i < r.tokens.size(); ++i) {
        if (i) buf += ", ";
        appendEscaped(buf, r.tokens[i]);
    }
    buf += "],\n  \"seq\": [";
    for (size_t i = 0; i < r.seq.size(); ++i) {
        if (i) buf += ", ";
        buf += std::to_string(r.seq[i]);
    }
    buf += "],\n  \"token_us\": [";
    for (size_t i = 0; i < r.tokenUs.size(); ++i) {
        if (i) buf += ", ";
        buf += std::to_string(r.tokenUs[i]);
    }
    buf += "],\n  \"token_dt_us\": [";
    for (size_t i = 0; i < r.tokenDtUs.size(); ++i) {
        if (i) buf += ", ";
        buf += std::to_string(r.tokenDtUs[i]);
    }
    // seq_monotonic: true iff seq[i] == i for all i (no async reordering).
    bool monotonic = (r.seq.size() == r.tokens.size());
    for (size_t i = 0; monotonic && i < r.seq.size(); ++i)
        if (r.seq[i] != static_cast<long long>(i)) monotonic = false;
    buf += "],\n  \"seq_monotonic\": ";
    buf += (monotonic ? "true" : "false");
    buf += "\n}\n";

    FILE* f = nullptr;
#if defined(_WIN32)
    fopen_s(&f, path.c_str(), "wb");
#else
    f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    std::fwrite(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return true;
}

} // namespace RawrXD::ParityTrace
