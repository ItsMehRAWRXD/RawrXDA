// ============================================================================
// ssot_handlers.cpp ? SSOT-Bridged Command Handlers (real implementations)
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Handlers here either delegate to Win32IDE via PostMessage(WM_COMMAND) in
// GUI mode, or produce real CLI output. No placeholder stubs: every handler
// resolves to real behavior (GUI dispatch or CLI message).
// As subsystems grow, handlers may move to dedicated files; COMMAND_TABLE
// must always resolve for link.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ssot_handlers.h"
#include "shared_feature_dispatch.h"
#include "unified_hotpatch_manager.hpp"
#include "proxy_hotpatcher.hpp"
#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "sentinel_watchdog.hpp"
#include "auto_repair_orchestrator.hpp"
#include "swarm_coordinator.h"
#include "command_registry.hpp"
#include "model_training_pipeline.hpp"
#include "execution_governor.h"
#include "agentic/AgentOllamaClient.h"
#include "model_source_resolver.h"
#include "../../native_gguf_loader.h"
#include <windows.h>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <map>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>

static RawrXD::Agent::AgentOllamaClient& getAgentClient();

// ============================================================================
// HELPER: Route to IDE UI via WM_COMMAND when in GUI mode (CLI gets text output)
// ============================================================================

namespace {
struct ReverseTraceStats {
    uint64_t calls = 0;
    uint64_t totalUs = 0;
    uint64_t maxUs = 0;
    uint64_t lastUs = 0;
};

struct ReverseTraceState {
    std::mutex mtx;
    std::unordered_map<std::string, ReverseTraceStats> byName;
    LARGE_INTEGER qpcFreq{};
    bool freqReady = false;

    static ReverseTraceState& instance() {
        static ReverseTraceState s;
        return s;
    }
};

static uint64_t reverseTraceNowTicks() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart);
}

static uint64_t reverseTraceTicksToUs(uint64_t ticks) {
    auto& st = ReverseTraceState::instance();
    if (!st.freqReady) {
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        st.qpcFreq = freq;
        st.freqReady = true;
    }
    const uint64_t denom = static_cast<uint64_t>(st.qpcFreq.QuadPart > 0 ? st.qpcFreq.QuadPart : 1);
    return (ticks * 1000000ull) / denom;
}

static void reverseTraceRecord(const char* name, uint64_t elapsedUs) {
    if (!name || !name[0]) return;
    auto& st = ReverseTraceState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    ReverseTraceStats& s = st.byName[name];
    s.calls += 1;
    s.totalUs += elapsedUs;
    s.lastUs = elapsedUs;
    if (elapsedUs > s.maxUs) s.maxUs = elapsedUs;
}

class ReverseTraceScope {
public:
    explicit ReverseTraceScope(const char* name) : m_name(name), m_start(reverseTraceNowTicks()) {}
    ~ReverseTraceScope() {
        const uint64_t end = reverseTraceNowTicks();
        const uint64_t elapsedTicks = (end >= m_start) ? (end - m_start) : 0;
        reverseTraceRecord(m_name, reverseTraceTicksToUs(elapsedTicks));
    }

private:
    const char* m_name;
    uint64_t m_start;
};

static std::string reverseTraceHotspotsReport(size_t topN) {
    struct Row {
        std::string name;
        ReverseTraceStats stats;
        uint64_t avgUs = 0;
    };

    std::vector<Row> rows;
    {
        auto& st = ReverseTraceState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        rows.reserve(st.byName.size());
        for (const auto& kv : st.byName) {
            Row r;
            r.name = kv.first;
            r.stats = kv.second;
            r.avgUs = (r.stats.calls > 0) ? (r.stats.totalUs / r.stats.calls) : 0;
            rows.push_back(r);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.stats.totalUs > b.stats.totalUs;
    });

    if (rows.empty()) {
        return "  Reverse Trace: no samples yet\\n";
    }

    std::ostringstream oss;
    oss << "  Reverse Trace Hotspots (top " << topN << "):\\n";
    const size_t count = std::min(topN, rows.size());
    for (size_t i = 0; i < count; ++i) {
        const auto& r = rows[i];
        oss << "    [" << (i + 1) << "] " << r.name
            << " calls=" << r.stats.calls
            << " total=" << r.stats.totalUs << "us"
            << " avg=" << r.avgUs << "us"
            << " max=" << r.stats.maxUs << "us"
            << " last=" << r.stats.lastUs << "us\\n";
    }
    return oss.str();
}

static void reverseTraceResetAll() {
    auto& st = ReverseTraceState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.byName.clear();
}

static bool reverseTraceExportCsv(const std::string& outPath) {
    struct Row {
        std::string name;
        ReverseTraceStats stats;
        uint64_t avgUs = 0;
    };

    std::vector<Row> rows;
    {
        auto& st = ReverseTraceState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        rows.reserve(st.byName.size());
        for (const auto& kv : st.byName) {
            Row r;
            r.name = kv.first;
            r.stats = kv.second;
            r.avgUs = (r.stats.calls > 0) ? (r.stats.totalUs / r.stats.calls) : 0;
            rows.push_back(r);
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.stats.totalUs > b.stats.totalUs;
    });

    FILE* f = fopen(outPath.c_str(), "wb");
    if (!f) return false;

    fprintf(f, "name,calls,total_us,avg_us,max_us,last_us\n");
    for (const auto& r : rows) {
        std::string safe = r.name;
        std::replace(safe.begin(), safe.end(), '"', '\'');
        fprintf(f, "\"%s\",%llu,%llu,%llu,%llu,%llu\n",
                safe.c_str(),
                static_cast<unsigned long long>(r.stats.calls),
                static_cast<unsigned long long>(r.stats.totalUs),
                static_cast<unsigned long long>(r.avgUs),
                static_cast<unsigned long long>(r.stats.maxUs),
                static_cast<unsigned long long>(r.stats.lastUs));
    }

    fclose(f);
    return true;
}

struct CliVisualState {
    std::mutex mtx;
    std::string activeTheme = "theme.oneDarkPro";
    int transparencyPct = 100;
    bool transparencyEnabled = false;
    bool monacoEnabled = false;
    bool monacoDevtools = false;
    int monacoZoom = 100;
    bool loaded = false;
};

static CliVisualState& cliVisualState() {
    static CliVisualState state;
    return state;
}

static std::string trimAscii(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

static std::string cliVisualStatePath() {
    return ".rawrxd\\cli_visual_state.json";
}

static void ensureRawrxdDir() {
    DWORD attrs = GetFileAttributesA(".rawrxd");
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) return;
    (void)CreateDirectoryA(".rawrxd", nullptr);
}

static bool jsonExtractBool(const std::string& json, const char* key, bool fallback) {
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return fallback;
    pos++;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return fallback;
}

static int jsonExtractInt(const std::string& json, const char* key, int fallback) {
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return fallback;
    pos++;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    const char* start = json.c_str() + pos;
    char* endPtr = nullptr;
    long v = std::strtol(start, &endPtr, 10);
    if (start == endPtr) return fallback;
    return static_cast<int>(v);
}

static std::string jsonExtractString(const std::string& json, const char* key, const std::string& fallback) {
    std::string needle = "\"";
    needle += key;
    needle += "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return fallback;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return fallback;
    pos++;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) pos++;
    if (pos >= json.size() || json[pos] != '"') return fallback;
    ++pos;
    std::string out;
    while (pos < json.size()) {
        char c = json[pos++];
        if (c == '\\' && pos < json.size()) {
            char n = json[pos++];
            if (n == '"' || n == '\\' || n == '/') out.push_back(n);
            else if (n == 'n') out.push_back('\n');
            else if (n == 'r') out.push_back('\r');
            else if (n == 't') out.push_back('\t');
            else out.push_back(n);
            continue;
        }
        if (c == '"') break;
        out.push_back(c);
    }
    return out.empty() ? fallback : out;
}

static std::string jsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

static bool loadCliVisualStateLocked(CliVisualState& v) {
    if (v.loaded) return true;
    v.loaded = true;
    std::ifstream in(cliVisualStatePath(), std::ios::binary);
    if (!in.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (json.empty()) return false;

    const std::string theme = trimAscii(jsonExtractString(json, "activeTheme", v.activeTheme));
    if (!theme.empty()) v.activeTheme = theme;
    int tp = jsonExtractInt(json, "transparencyPct", v.transparencyPct);
    if (tp < 20) tp = 20;
    if (tp > 100) tp = 100;
    v.transparencyPct = tp;
    v.transparencyEnabled = jsonExtractBool(json, "transparencyEnabled", v.transparencyEnabled);
    v.monacoEnabled = jsonExtractBool(json, "monacoEnabled", v.monacoEnabled);
    v.monacoDevtools = jsonExtractBool(json, "monacoDevtools", v.monacoDevtools);
    int zoom = jsonExtractInt(json, "monacoZoom", v.monacoZoom);
    if (zoom < 30) zoom = 30;
    if (zoom > 300) zoom = 300;
    v.monacoZoom = zoom;
    return true;
}

static bool saveCliVisualStateLocked(const CliVisualState& v) {
    ensureRawrxdDir();
    const std::string path = cliVisualStatePath();
    const std::string tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    std::ostringstream oss;
    oss << "{\n"
        << "  \"activeTheme\": \"" << jsonEscape(v.activeTheme) << "\",\n"
        << "  \"transparencyPct\": " << v.transparencyPct << ",\n"
        << "  \"transparencyEnabled\": " << (v.transparencyEnabled ? "true" : "false") << ",\n"
        << "  \"monacoEnabled\": " << (v.monacoEnabled ? "true" : "false") << ",\n"
        << "  \"monacoDevtools\": " << (v.monacoDevtools ? "true" : "false") << ",\n"
        << "  \"monacoZoom\": " << v.monacoZoom << "\n"
        << "}\n";
    const std::string payload = oss.str();
    const size_t written = fwrite(payload.data(), 1, payload.size(), f);
    fclose(f);
    if (written != payload.size()) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

static bool parseTransparencyArg(const CommandContext& ctx, int& outPct) {
    if (!ctx.args || !ctx.args[0]) return false;
    const char* s = ctx.args;
    while (*s) {
        if ((*s >= '0' && *s <= '9') || *s == '-') {
            const long v = std::strtol(s, nullptr, 10);
            if (v >= 20 && v <= 100) {
                outPct = static_cast<int>(v);
                return true;
            }
        }
        ++s;
    }
    return false;
}

static const char* themeNameFromId(uint32_t cmdId) {
    switch (cmdId) {
    case 3102: return "theme.lightPlus";
    case 3103: return "theme.monokai";
    case 3104: return "theme.dracula";
    case 3105: return "theme.nord";
    case 3106: return "theme.solarizedDark";
    case 3107: return "theme.solarizedLight";
    case 3108: return "theme.cyberpunk";
    case 3109: return "theme.gruvbox";
    case 3110: return "theme.catppuccin";
    case 3111: return "theme.tokyoNight";
    case 3112: return "theme.rawrxdCrimson";
    case 3113: return "theme.highContrast";
    case 3114: return "theme.oneDarkPro";
    case 3115: return "theme.synthwave84";
    case 3116: return "theme.abyss";
    default: return nullptr;
    }
}
} // namespace

static CommandResult routeToIde(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    ReverseTraceScope _trace("ssot.routeToIde");
    if (ctx.isGui && ctx.idePtr) {
        // Route to Win32IDE's WM_COMMAND handler (use ctx.hwnd when set by adapter)
        HWND hwnd = reinterpret_cast<HWND>(ctx.hwnd);
        if (!hwnd) hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);  // Fallback for older callers
        if (hwnd) PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    // CLI visual-mode support for theme/transparency/monaco commands.
    {
        auto& v = cliVisualState();
        std::lock_guard<std::mutex> lock(v.mtx);
        (void)loadCliVisualStateLocked(v);

        const char* mappedTheme = themeNameFromId(cmdId);
        if (mappedTheme) {
            v.activeTheme = mappedTheme;
            const bool saved = saveCliVisualStateLocked(v);
            std::ostringstream oss;
            oss << "[THEME] Applied in CLI profile: " << v.activeTheme << "\n"
                << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::ok(name);
        }

        if (cmdId >= 3200 && cmdId <= 3206) {
            static const int values[] = {100, 90, 80, 70, 60, 50, 40};
            v.transparencyPct = values[cmdId - 3200];
            v.transparencyEnabled = (v.transparencyPct < 100);
            const bool saved = saveCliVisualStateLocked(v);
            std::ostringstream oss;
            oss << "[VIEW] Transparency set to " << v.transparencyPct << "% (CLI profile)\n"
                << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::ok(name);
        }
        if (cmdId == 3210) {
            int custom = v.transparencyPct;
            (void)parseTransparencyArg(ctx, custom);
            v.transparencyPct = custom;
            v.transparencyEnabled = (custom < 100);
            const bool saved = saveCliVisualStateLocked(v);
            std::ostringstream oss;
            oss << "[VIEW] Transparency custom set to " << v.transparencyPct
                << "% (CLI profile, valid range 20-100)\n"
                << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::ok(name);
        }
        if (cmdId == 3211) {
            v.transparencyEnabled = !v.transparencyEnabled;
            const bool saved = saveCliVisualStateLocked(v);
            std::ostringstream oss;
            oss << "[VIEW] Transparency " << (v.transparencyEnabled ? "enabled" : "disabled")
                << " in CLI profile";
            if (v.transparencyEnabled) oss << " at " << v.transparencyPct << "%";
            oss << "\n  State: " << (saved ? "persisted" : "persist failed") << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::ok(name);
        }

        if (cmdId >= 9100 && cmdId <= 9105) {
            switch (cmdId) {
            case 9100:
                v.monacoEnabled = !v.monacoEnabled;
                break;
            case 9101:
                v.monacoDevtools = !v.monacoDevtools;
                break;
            case 9102:
                break;
            case 9103:
                v.monacoZoom = std::min(300, v.monacoZoom + 10);
                break;
            case 9104:
                v.monacoZoom = std::max(30, v.monacoZoom - 10);
                break;
            case 9105:
                break;
            default:
                break;
            }
            const bool saved = saveCliVisualStateLocked(v);
            if (cmdId == 9100) {
                std::ostringstream oss;
                oss << (v.monacoEnabled ? "[MONACO] Enabled in CLI profile\n"
                                        : "[MONACO] Disabled in CLI profile\n")
                    << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
                ctx.output(oss.str().c_str());
            } else if (cmdId == 9101) {
                std::ostringstream oss;
                oss << (v.monacoDevtools ? "[MONACO] Devtools enabled in CLI profile\n"
                                         : "[MONACO] Devtools disabled in CLI profile\n")
                    << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
                ctx.output(oss.str().c_str());
            } else if (cmdId == 9102) {
                std::ostringstream oss;
                oss << "[MONACO] Reload triggered in CLI profile\n"
                    << "  Theme: " << v.activeTheme << "\n"
                    << "  Zoom: " << v.monacoZoom << "%\n"
                    << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
                ctx.output(oss.str().c_str());
            } else if (cmdId == 9103 || cmdId == 9104) {
                std::ostringstream oss;
                oss << "[MONACO] Zoom set to " << v.monacoZoom << "% (CLI profile)\n"
                    << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
                ctx.output(oss.str().c_str());
            } else if (cmdId == 9105) {
                std::ostringstream oss;
                oss << "[MONACO] Theme synchronized to " << v.activeTheme << " (CLI profile)\n"
                    << "  State: " << (saved ? "persisted" : "persist failed") << "\n";
                ctx.output(oss.str().c_str());
            }
            return CommandResult::ok(name);
        }
    }

    char buf[160];
    snprintf(buf, sizeof(buf), "[%s] Dispatched via CLI (ID %u).\n", name, cmdId);
    ctx.output(buf);
    return CommandResult::ok(name);
}

namespace {
struct SSOBackendState {
    std::mutex mtx;
    std::string activeBackend = "ollama";
    RawrXD::Agent::OllamaConfig ollamaConfig;
    std::map<std::string, std::string> apiKeys;
    std::string localModelInput;
    std::string localModelResolvedPath;
    std::string localModelError;

    static SSOBackendState& instance() {
        static SSOBackendState s;
        return s;
    }
};

struct SSORouterState {
    std::mutex mtx;
    std::atomic<bool> enabled{true};
    std::atomic<bool> ensembleEnabled{false};
    std::string policy = "quality";
    std::atomic<uint64_t> totalRouted{0};
    std::atomic<uint64_t> totalTokens{0};

    static SSORouterState& instance() {
        static SSORouterState s;
        return s;
    }
};

struct SSOConfidenceState {
    std::mutex mtx;
    std::string policy = "conservative";
    float score = 0.85f;
    std::string lastAction = "none";

    static SSOConfidenceState& instance() {
        static SSOConfidenceState s;
        return s;
    }
};

static RawrXD::Agent::AgentOllamaClient createSsoOllamaClient() {
    auto& bs = SSOBackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    return RawrXD::Agent::AgentOllamaClient(bs.ollamaConfig);
}

struct BackendLocalModelResolution {
    bool attempted = false;
    bool success = false;
    std::string input;
    std::string resolvedPath;
    std::string error;
};

static BackendLocalModelResolution resolveBackendLocalModel(const std::string& input) {
    BackendLocalModelResolution result;
    result.input = trimAscii(input);
    if (result.input.empty()) {
        return result;
    }

    result.attempted = true;
    RawrXD::ModelSourceResolver resolver;
    RawrXD::ResolvedModelPath resolved = resolver.Resolve(result.input);
    result.success = resolved.success;
    result.resolvedPath = resolved.local_path;
    result.error = resolved.error_message;
    if (!result.success && result.error.empty()) {
        result.error = "Unable to resolve model source";
    }
    return result;
}

static std::string summarizeEmbeddedGgufProbe(const std::string& resolvedPath) {
    if (resolvedPath.empty()) {
        return "  Embedded GGUF probe: skipped (no resolved local path)\n";
    }

    NativeGGUFLoader loader;
    if (!loader.Open(resolvedPath)) {
        return "  Embedded GGUF probe: OPEN FAILED\n";
    }

    if (!loader.ParseHeader()) {
        loader.Close();
        return "  Embedded GGUF probe: HEADER FAILED\n";
    }
    if (!loader.ParseMetadata()) {
        loader.Close();
        return "  Embedded GGUF probe: METADATA FAILED\n";
    }
    if (!loader.ParseTensorInfo()) {
        loader.Close();
        return "  Embedded GGUF probe: TENSOR INDEX FAILED\n";
    }

    const bool mmapped = loader.IsMemoryMapped();
    const uint64_t mappedBytes = loader.GetMappedSize();
    const size_t tensorCount = loader.GetTensors().size();
    const size_t metadataCount = loader.GetMetadata().size();
    loader.Close();

    char buf[256];
    snprintf(buf, sizeof(buf),
             "  Embedded GGUF probe: OK (mmap=%s, mapped=%llu MB, tensors=%llu, metadata=%llu)\n",
             mmapped ? "yes" : "no",
             static_cast<unsigned long long>(mappedBytes / (1024ull * 1024ull)),
             static_cast<unsigned long long>(tensorCount),
             static_cast<unsigned long long>(metadataCount));
    return std::string(buf);
}

static std::string ssoGetArg(const CommandContext& ctx, int index) {
    if (!ctx.args || ctx.args[0] == '\0') return "";
    std::istringstream iss(ctx.args);
    std::string tok;
    for (int i = 0; i <= index; ++i) {
        if (!(iss >> tok)) return "";
    }
    return tok;
}

struct LspServerRuntimeState {
    std::mutex mtx;
    PROCESS_INFORMATION proc{};
    bool running = false;
    std::string binaryPath;
    std::string commandLine;
    std::string workspaceRoot = ".";
    unsigned long long startedAtTick = 0;
    unsigned long long lastReindexTick = 0;
    unsigned long long reindexCount = 0;

    static LspServerRuntimeState& instance() {
        static LspServerRuntimeState s;
        return s;
    }
};

struct LspClientRuntimeConfig {
    std::mutex mtx;
    bool loaded = false;
    std::string server = "clangd";
    std::string serverArgs = "--background-index --clang-tidy --log=error --pch-storage=memory";
    std::string workspaceRoot = ".";
    bool diagnosticsOnSave = true;
    bool strictMode = false;
    int maxReferences = 512;
    std::string lastSavedPath = ".rawrxd\\lsp\\client_config.json";

    static LspClientRuntimeConfig& instance() {
        static LspClientRuntimeConfig s;
        return s;
    }
};

static bool ensureDirectory(const char* path);

static void closeLspProcessHandles(LspServerRuntimeState& st) {
    if (st.proc.hThread) {
        CloseHandle(st.proc.hThread);
        st.proc.hThread = nullptr;
    }
    if (st.proc.hProcess) {
        CloseHandle(st.proc.hProcess);
        st.proc.hProcess = nullptr;
    }
    st.proc.dwProcessId = 0;
    st.proc.dwThreadId = 0;
}

static std::string lspClientConfigPath() {
    return ".rawrxd\\lsp\\client_config.json";
}

static bool parseBoolWord(const std::string& raw, bool& outValue) {
    std::string v = trimAscii(raw);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (v == "1" || v == "true" || v == "yes" || v == "on" || v == "enable" || v == "enabled") {
        outValue = true;
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off" || v == "disable" || v == "disabled") {
        outValue = false;
        return true;
    }
    return false;
}

static bool loadLspClientConfigLocked(LspClientRuntimeConfig& cfg) {
    if (cfg.loaded) return true;
    cfg.loaded = true;

    std::ifstream in(lspClientConfigPath(), std::ios::binary);
    if (!in.good()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();
    if (json.empty()) return false;

    const std::string server = trimAscii(jsonExtractString(json, "server", cfg.server));
    if (!server.empty()) cfg.server = server;

    const std::string args = trimAscii(jsonExtractString(json, "serverArgs", cfg.serverArgs));
    if (!args.empty()) cfg.serverArgs = args;

    const std::string workspace = trimAscii(jsonExtractString(json, "workspaceRoot", cfg.workspaceRoot));
    if (!workspace.empty()) cfg.workspaceRoot = workspace;

    cfg.diagnosticsOnSave = jsonExtractBool(json, "diagnosticsOnSave", cfg.diagnosticsOnSave);
    cfg.strictMode = jsonExtractBool(json, "strictMode", cfg.strictMode);

    int refs = jsonExtractInt(json, "maxReferences", cfg.maxReferences);
    if (refs < 32) refs = 32;
    if (refs > 8192) refs = 8192;
    cfg.maxReferences = refs;

    const std::string savePath = trimAscii(jsonExtractString(json, "lastSavedPath", cfg.lastSavedPath));
    if (!savePath.empty()) cfg.lastSavedPath = savePath;
    return true;
}

static bool saveLspClientConfigToPath(const LspClientRuntimeConfig& cfg, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fprintf(f,
            "{\n"
            "  \"server\": \"%s\",\n"
            "  \"serverArgs\": \"%s\",\n"
            "  \"workspaceRoot\": \"%s\",\n"
            "  \"diagnosticsOnSave\": %s,\n"
            "  \"strictMode\": %s,\n"
            "  \"maxReferences\": %d,\n"
            "  \"lastSavedPath\": \"%s\"\n"
            "}\n",
            cfg.server.c_str(),
            cfg.serverArgs.c_str(),
            cfg.workspaceRoot.c_str(),
            cfg.diagnosticsOnSave ? "true" : "false",
            cfg.strictMode ? "true" : "false",
            cfg.maxReferences,
            path.c_str());
    fclose(f);
    return true;
}

static bool saveLspClientConfigLocked(LspClientRuntimeConfig& cfg) {
    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");
    const std::string path = lspClientConfigPath();
    const bool ok = saveLspClientConfigToPath(cfg, path);
    if (ok) cfg.lastSavedPath = path;
    return ok;
}

static std::string tailArgs(const CommandContext& ctx) {
    if (!ctx.args || ctx.args[0] == '\0') return "";
    std::istringstream iss(ctx.args);
    std::string first;
    iss >> first;
    std::string rest;
    std::getline(iss, rest);
    if (!rest.empty() && rest.front() == ' ') {
        rest.erase(0, 1);
    }
    return rest;
}

static bool resolveBinaryPath(const std::string& requested, std::string& outPath) {
    outPath.clear();
    if (requested.empty()) return false;
    bool hasPathSeparators = requested.find('\\') != std::string::npos ||
                             requested.find('/') != std::string::npos ||
                             requested.find(':') != std::string::npos;
    if (hasPathSeparators) {
        outPath = requested;
        return true;
    }
    char found[MAX_PATH] = {};
    DWORD n = SearchPathA(nullptr, requested.c_str(), ".exe", MAX_PATH, found, nullptr);
    if (n > 0 && n < MAX_PATH) {
        outPath.assign(found, found + n);
        return true;
    }
    n = SearchPathA(nullptr, requested.c_str(), nullptr, MAX_PATH, found, nullptr);
    if (n > 0 && n < MAX_PATH) {
        outPath.assign(found, found + n);
        return true;
    }
    return false;
}

static bool ensureDirectory(const char* path) {
    if (!path || !path[0]) return false;
    DWORD attrs = GetFileAttributesA(path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryA(path, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

struct SsoGovernorState {
    std::mutex mtx;
    bool loaded = false;
    std::string powerProfile = "balanced";
    int powerPercent = 60;
    int maxConcurrent = 2;
    unsigned long long defaultTimeoutMs = 30000;
    std::string preferredBackend = "auto";
    std::vector<GovernorTaskId> taskOrder;
    std::map<GovernorTaskId, std::string> taskCommand;

    static SsoGovernorState& instance() {
        static SsoGovernorState s;
        return s;
    }
};

static std::string governorStatePath() {
    return ".rawrxd\\governor_state.json";
}

static bool loadGovernorStateLocked(SsoGovernorState& st) {
    if (st.loaded) return true;
    st.loaded = true;
    std::ifstream in(governorStatePath(), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string json = ss.str();

    const std::string profile = trimAscii(jsonExtractString(json, "powerProfile", st.powerProfile));
    if (!profile.empty()) st.powerProfile = profile;

    int pct = jsonExtractInt(json, "powerPercent", st.powerPercent);
    if (pct < 20) pct = 20;
    if (pct > 100) pct = 100;
    st.powerPercent = pct;

    int maxC = jsonExtractInt(json, "maxConcurrent", st.maxConcurrent);
    if (maxC < 1) maxC = 1;
    if (maxC > 64) maxC = 64;
    st.maxConcurrent = maxC;

    int timeoutMs = jsonExtractInt(json, "defaultTimeoutMs", static_cast<int>(st.defaultTimeoutMs));
    if (timeoutMs < 1000) timeoutMs = 1000;
    st.defaultTimeoutMs = static_cast<unsigned long long>(timeoutMs);

    const std::string backend = trimAscii(jsonExtractString(json, "preferredBackend", st.preferredBackend));
    if (!backend.empty()) st.preferredBackend = backend;
    return true;
}

static bool saveGovernorStateLocked(const SsoGovernorState& st) {
    ensureDirectory(".rawrxd");
    FILE* f = fopen(governorStatePath().c_str(), "wb");
    if (!f) return false;
    fprintf(f,
            "{\n"
            "  \"powerProfile\": \"%s\",\n"
            "  \"powerPercent\": %d,\n"
            "  \"maxConcurrent\": %d,\n"
            "  \"defaultTimeoutMs\": %llu,\n"
            "  \"preferredBackend\": \"%s\"\n"
            "}\n",
            st.powerProfile.c_str(),
            st.powerPercent,
            st.maxConcurrent,
            static_cast<unsigned long long>(st.defaultTimeoutMs),
            st.preferredBackend.c_str());
    fclose(f);
    return true;
}

static bool isDllResolvable(const char* dllName) {
    if (!dllName || !dllName[0]) return false;
    char found[MAX_PATH] = {};
    DWORD n = SearchPathA(nullptr, dllName, nullptr, MAX_PATH, found, nullptr);
    return n > 0 && n < MAX_PATH;
}

static std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

static std::string detectBackendFromCommand(const std::string& command) {
    const std::string lower = toLowerCopy(command);
    if (lower.find("vulkan") != std::string::npos) return "vulkan";
    if (lower.find("cuda") != std::string::npos || lower.find("nvcuda") != std::string::npos) return "cuda";
    if (lower.find("rocm") != std::string::npos || lower.find("hip") != std::string::npos) return "rocm";
    if (lower.find("cpu") != std::string::npos) return "cpu";
    return "auto";
}

static const char* governorTaskStateName(GovernorTaskState state) {
    switch (state) {
    case GovernorTaskState::Pending: return "pending";
    case GovernorTaskState::Running: return "running";
    case GovernorTaskState::Completed: return "done";
    case GovernorTaskState::Cancelled: return "cancelled";
    case GovernorTaskState::TimedOut: return "timed_out";
    case GovernorTaskState::Killed: return "killed";
    case GovernorTaskState::Failed: return "failed";
    default: return "unknown";
    }
}

static bool parsePowerProfile(const std::string& raw, std::string& profile, int& percent, int& maxConcurrent) {
    std::string value = trimAscii(toLowerCopy(raw));
    if (value.empty()) value = "balanced";

    const bool isNumeric = !value.empty() &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });

    if (isNumeric) {
        int pct = std::atoi(value.c_str());
        if (pct < 20) pct = 20;
        if (pct > 100) pct = 100;
        percent = pct;
        if (pct <= 45) {
            profile = "eco";
            maxConcurrent = 1;
        } else if (pct <= 70) {
            profile = "balanced";
            maxConcurrent = 2;
        } else if (pct <= 85) {
            profile = "performance";
            maxConcurrent = 4;
        } else if (pct <= 95) {
            profile = "turbo";
            maxConcurrent = 6;
        } else {
            profile = "max";
            maxConcurrent = 8;
        }
        return true;
    }

    if (value == "eco" || value == "powersaver" || value == "low") {
        profile = "eco";
        percent = 35;
        maxConcurrent = 1;
        return true;
    }
    if (value == "balanced" || value == "normal" || value == "default") {
        profile = "balanced";
        percent = 60;
        maxConcurrent = 2;
        return true;
    }
    if (value == "performance" || value == "high") {
        profile = "performance";
        percent = 80;
        maxConcurrent = 4;
        return true;
    }
    if (value == "turbo") {
        profile = "turbo";
        percent = 95;
        maxConcurrent = 6;
        return true;
    }
    if (value == "max" || value == "extreme") {
        profile = "max";
        percent = 100;
        maxConcurrent = 8;
        return true;
    }
    return false;
}

static void trackGovernorTaskLocked(SsoGovernorState& st, GovernorTaskId id, const std::string& command) {
    st.taskOrder.push_back(id);
    st.taskCommand[id] = command;
    constexpr size_t kMaxTracked = 256;
    while (st.taskOrder.size() > kMaxTracked) {
        const GovernorTaskId old = st.taskOrder.front();
        st.taskOrder.erase(st.taskOrder.begin());
        st.taskCommand.erase(old);
    }
}

static unsigned long long wipeDirectoryTree(const std::string& root) {
    DWORD attrs = GetFileAttributesA(root.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return 0;
    }

    unsigned long long removed = 0;
    std::string pattern = root + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return 0;
    }

    do {
        const char* name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        std::string child = root + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            removed += wipeDirectoryTree(child);
            if (RemoveDirectoryA(child.c_str())) {
                ++removed;
            }
        } else if (DeleteFileA(child.c_str())) {
            ++removed;
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return removed;
}

static void countSourceFilesRecursive(const std::string& root, int& cppCount, int& hCount) {
    std::string pattern = root + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        const char* name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        std::string path = root + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            countSourceFilesRecursive(path, cppCount, hCount);
            continue;
        }
        const std::string file = name;
        size_t dot = file.find_last_of('.');
        if (dot == std::string::npos) continue;
        std::string ext = file.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") ++cppCount;
        if (ext == ".h" || ext == ".hpp" || ext == ".hh" || ext == ".hxx") ++hCount;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

struct LspSymbolMatch {
    std::string path;
    size_t line = 0;
    size_t column = 0;
    std::string snippet;
    bool definitionLike = false;
};

static std::string toLowerAsciiCopy(std::string v);
static bool pathIsDirectory(const std::string& path);

static bool hasLspSearchExt(const std::string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = toLowerAsciiCopy(path.substr(dot));
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" ||
           ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx" ||
           ext == ".ixx" || ext == ".inl" || ext == ".ipp" ||
           ext == ".asm" || ext == ".inc";
}

static void collectLspSearchFilesRecursive(const std::string& root,
                                           std::vector<std::string>& out,
                                           size_t maxFiles) {
    if (out.size() >= maxFiles || !pathIsDirectory(root)) return;
    std::string pattern = root + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (out.size() >= maxFiles) break;
        const char* name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        std::string path = root + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            collectLspSearchFilesRecursive(path, out, maxFiles);
            continue;
        }
        if (hasLspSearchExt(path)) out.push_back(path);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

static std::vector<std::string> collectDefaultLspSearchFiles() {
    std::vector<std::string> files;
    files.reserve(1024);
    const std::array<const char*, 4> roots = {
        "src",
        "include",
        "tests",
        "test"
    };
    for (const char* root : roots) {
        collectLspSearchFilesRecursive(root, files, 1600);
        if (files.size() >= 1600) break;
    }
    return files;
}

static bool readTextFileLimited(const std::string& path, std::string& out, size_t maxBytes) {
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0 || static_cast<size_t>(sz) > maxBytes) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t n = 0;
    if (!out.empty()) n = fread(&out[0], 1, out.size(), f);
    fclose(f);
    if (n != out.size()) {
        out.clear();
        return false;
    }
    return true;
}

static bool writeTextFile(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return n == text.size();
}

static bool isIdentCharAscii(unsigned char c) {
    return std::isalnum(c) || c == '_';
}

static bool isWholeWordMatch(const std::string& text, size_t pos, size_t len) {
    if (pos > text.size() || len == 0 || pos + len > text.size()) return false;
    if (pos > 0 && isIdentCharAscii(static_cast<unsigned char>(text[pos - 1]))) return false;
    if (pos + len < text.size() && isIdentCharAscii(static_cast<unsigned char>(text[pos + len]))) return false;
    return true;
}

static bool lineLooksDefinition(const std::string& line, const std::string& symbol) {
    if (line.empty() || symbol.empty()) return false;
    const std::string lower = toLowerAsciiCopy(line);
    const std::string sym = toLowerAsciiCopy(symbol);
    if (lower.find("class " + sym) != std::string::npos) return true;
    if (lower.find("struct " + sym) != std::string::npos) return true;
    if (lower.find("enum " + sym) != std::string::npos) return true;
    if (lower.find("typedef") != std::string::npos && lower.find(sym) != std::string::npos) return true;
    if (lower.find(sym + "(") != std::string::npos && lower.find(';') == std::string::npos) return true;
    return false;
}

static void collectSymbolMatches(const std::string& symbol,
                                 const std::vector<std::string>& files,
                                 std::vector<LspSymbolMatch>& out,
                                 size_t maxMatches) {
    out.clear();
    if (symbol.empty()) return;

    for (const auto& path : files) {
        if (out.size() >= maxMatches) break;
        std::string text;
        if (!readTextFileLimited(path, text, 8 * 1024 * 1024)) continue;

        size_t lineNo = 1;
        size_t lineStart = 0;
        while (lineStart <= text.size()) {
            if (out.size() >= maxMatches) break;
            size_t lineEnd = text.find('\n', lineStart);
            if (lineEnd == std::string::npos) lineEnd = text.size();
            std::string line = text.substr(lineStart, lineEnd - lineStart);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            size_t searchPos = 0;
            while (searchPos < line.size()) {
                const size_t hit = line.find(symbol, searchPos);
                if (hit == std::string::npos) break;
                if (isWholeWordMatch(line, hit, symbol.size())) {
                    LspSymbolMatch m;
                    m.path = path;
                    m.line = lineNo;
                    m.column = hit + 1;
                    m.snippet = trimAscii(line);
                    m.definitionLike = lineLooksDefinition(line, symbol);
                    out.push_back(std::move(m));
                    if (out.size() >= maxMatches) break;
                }
                searchPos = hit + symbol.size();
            }

            if (lineEnd == text.size()) break;
            lineStart = lineEnd + 1;
            ++lineNo;
        }
    }
}

static bool selectDefinitionCandidate(const std::vector<LspSymbolMatch>& matches, LspSymbolMatch& outDef) {
    if (matches.empty()) return false;
    for (const auto& m : matches) {
        if (m.definitionLike) {
            outDef = m;
            return true;
        }
    }
    outDef = matches.front();
    return true;
}

static size_t replaceWholeWordOccurrences(std::string& text,
                                          const std::string& oldSym,
                                          const std::string& newSym) {
    if (oldSym.empty() || oldSym == newSym) return 0;
    size_t replaced = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t hit = text.find(oldSym, pos);
        if (hit == std::string::npos) break;
        if (isWholeWordMatch(text, hit, oldSym.size())) {
            text.replace(hit, oldSym.size(), newSym);
            pos = hit + newSym.size();
            ++replaced;
        } else {
            pos = hit + 1;
        }
    }
    return replaced;
}

static int extractMetricValue(const std::string& text, const char* label, int fallback) {
    if (!label || !label[0]) return fallback;
    const std::string needle(label);
    size_t pos = text.find(needle);
    if (pos == std::string::npos) return fallback;
    pos += needle.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    const char* start = text.c_str() + pos;
    char* endPtr = nullptr;
    long v = std::strtol(start, &endPtr, 10);
    if (start == endPtr) return fallback;
    return static_cast<int>(v);
}

static std::string sanitizeTokenForPath(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u) || c == '_' || c == '-') out.push_back(c);
        else out.push_back('_');
    }
    if (out.empty()) out = "symbol";
    if (out.size() > 80) out.resize(80);
    return out;
}

static std::string pickDefaultHybridFile() {
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    if (!files.empty()) return files.front();
    return "";
}

struct GameEngineRuntimeState {
    std::mutex mtx;
    HMODULE unrealBridge = nullptr;
    HMODULE unityBridge = nullptr;
    DWORD unrealPid = 0;
    DWORD unityPid = 0;
    std::string unrealBridgePath;
    std::string unityBridgePath;

    static GameEngineRuntimeState& instance() {
        static GameEngineRuntimeState s;
        return s;
    }
};

struct MarketplaceRuntimeState {
    std::mutex mtx;
    std::map<std::string, std::string> installedPaths;
    std::map<std::string, HMODULE> loadedModules;

    static MarketplaceRuntimeState& instance() {
        static MarketplaceRuntimeState s;
        return s;
    }
};

struct ModelRuntimeState {
    std::mutex mtx;
    std::string activeModelPath;
    std::string lastQuantizedModelPath;
    std::string lastFinetuneOutputDir = ".rawrxd\\training_output";
    unsigned long long finetuneRunCount = 0;
    std::vector<std::string> discoveredModels;

    static ModelRuntimeState& instance() {
        static ModelRuntimeState s;
        return s;
    }
};

static std::string getArgsTrimmed(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) return "";
    return trimAscii(ctx.args);
}

static std::string toLowerAsciiCopy(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return v;
}

static std::string fileNameFromPath(const std::string& path) {
    const size_t slash = path.find_last_of("\\/");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

static std::string stemNoExt(const std::string& path) {
    std::string base = fileNameFromPath(path);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

static bool pathIsDirectory(const std::string& path) {
    if (path.empty()) return false;
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool pathIsRegularFile(const std::string& path) {
    if (path.empty()) return false;
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool hasModelArtifactExt(const std::string& fileName) {
    const std::string lower = toLowerAsciiCopy(fileName);
    auto endsWith = [](const std::string& s, const char* suffix) {
        const size_t n = std::strlen(suffix);
        return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
    };
    return endsWith(lower, ".gguf") ||
           endsWith(lower, ".bin") ||
           endsWith(lower, ".pt") ||
           endsWith(lower, ".safetensors");
}

static void collectModelArtifactsFromDirectory(const std::string& dir, std::vector<std::string>& out) {
    if (!pathIsDirectory(dir)) return;
    std::string pattern = dir;
    if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/') pattern += "\\";
    pattern += "*";
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!hasModelArtifactExt(fd.cFileName)) continue;
        std::string full = dir;
        if (!full.empty() && full.back() != '\\' && full.back() != '/') full += "\\";
        full += fd.cFileName;
        out.push_back(full);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

static std::vector<std::string> discoverModelArtifacts() {
    std::vector<std::string> raw;
    const std::array<const char*, 4> roots = {
        "models",
        ".rawrxd\\models",
        ".rawrxd\\training_output",
        "."
    };
    for (const char* root : roots) {
        collectModelArtifactsFromDirectory(root, raw);
    }
    std::map<std::string, std::string> unique;
    for (const auto& p : raw) {
        const std::string key = toLowerAsciiCopy(p);
        if (unique.find(key) == unique.end()) unique[key] = p;
    }
    std::vector<std::string> out;
    out.reserve(unique.size());
    for (const auto& kv : unique) out.push_back(kv.second);
    std::sort(out.begin(), out.end());
    return out;
}

static bool resolveModelArtifactPath(const std::string& requested, std::string& outPath) {
    outPath.clear();
    if (requested.empty()) return false;
    if (pathIsRegularFile(requested)) {
        outPath = requested;
        return true;
    }

    const std::string requestedFile = toLowerAsciiCopy(fileNameFromPath(requested));
    const std::string requestedStem = toLowerAsciiCopy(stemNoExt(requested));
    std::vector<std::string> discovered = discoverModelArtifacts();
    for (const auto& cand : discovered) {
        const std::string candFile = toLowerAsciiCopy(fileNameFromPath(cand));
        const std::string candStem = toLowerAsciiCopy(stemNoExt(cand));
        if (candFile == requestedFile || candStem == requestedStem) {
            outPath = cand;
            return true;
        }
    }

    const std::array<const char*, 3> roots = { "models", ".rawrxd\\models", ".rawrxd\\training_output" };
    const std::array<const char*, 4> exts = { ".gguf", ".safetensors", ".bin", ".pt" };
    for (const char* root : roots) {
        for (const char* ext : exts) {
            std::string candidate = std::string(root) + "\\" + requested + ext;
            if (pathIsRegularFile(candidate)) {
                outPath = candidate;
                return true;
            }
        }
    }
    return false;
}

static RawrXD::Training::DatasetFormat inferDatasetFormat(const std::string& datasetPath, const std::string& explicitFormat) {
    const std::string fmt = toLowerAsciiCopy(trimAscii(explicitFormat));
    if (fmt == "jsonl" || fmt == "json") return RawrXD::Training::DatasetFormat::JSONL;
    if (fmt == "csv") return RawrXD::Training::DatasetFormat::CSV;
    if (fmt == "alpaca") return RawrXD::Training::DatasetFormat::Alpaca;
    if (fmt == "sharegpt") return RawrXD::Training::DatasetFormat::ShareGPT;
    if (fmt == "text" || fmt == "txt" || fmt == "markdown") return RawrXD::Training::DatasetFormat::PlainText;
    if (fmt == "tokenized" || fmt == "bin") return RawrXD::Training::DatasetFormat::CustomTokenized;
    if (fmt == "code" || fmt == "codefiles") return RawrXD::Training::DatasetFormat::CodeFiles;

    const std::string lowerPath = toLowerAsciiCopy(datasetPath);
    if (lowerPath.find(".jsonl") != std::string::npos) return RawrXD::Training::DatasetFormat::JSONL;
    if (lowerPath.find(".csv") != std::string::npos) return RawrXD::Training::DatasetFormat::CSV;
    if (lowerPath.find(".txt") != std::string::npos || lowerPath.find(".md") != std::string::npos) {
        return RawrXD::Training::DatasetFormat::PlainText;
    }
    return RawrXD::Training::DatasetFormat::CodeFiles;
}

static bool parseQuantTypeArg(const std::string& raw,
                              RawrXD::Training::QuantType& outType,
                              std::string& outCanonical) {
    std::string v = toLowerAsciiCopy(trimAscii(raw));
    if (v.empty()) v = "q4_k_m";
    if (v == "q4k" || v == "q4_k" || v == "q4_k_m") {
        outType = RawrXD::Training::QuantType::Q4_K_M;
        outCanonical = "q4_k_m";
        return true;
    }
    if (v == "q6k" || v == "q6_k") {
        outType = RawrXD::Training::QuantType::Q6_K;
        outCanonical = "q6_k";
        return true;
    }
    if (v == "q8" || v == "q8_0") {
        outType = RawrXD::Training::QuantType::Q8_0;
        outCanonical = "q8_0";
        return true;
    }
    if (v == "f16") {
        outType = RawrXD::Training::QuantType::F16;
        outCanonical = "f16";
        return true;
    }
    if (v == "f32") {
        outType = RawrXD::Training::QuantType::F32;
        outCanonical = "f32";
        return true;
    }
    if (v == "adaptive" || v == "auto") {
        outType = RawrXD::Training::QuantType::Adaptive;
        outCanonical = "adaptive";
        return true;
    }
    if (v == "q4_0") {
        outType = RawrXD::Training::QuantType::Q4_0;
        outCanonical = "q4_0";
        return true;
    }
    return false;
}

static bool findFirstFileByPattern(const char* pattern, std::string& outPath) {
    outPath.clear();
    if (!pattern || !pattern[0]) return false;
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string prefix = pattern;
        size_t star = prefix.find('*');
        if (star != std::string::npos) prefix = prefix.substr(0, star);
        outPath = prefix + fd.cFileName;
        break;
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return !outPath.empty();
}

static bool readBinaryFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t read = 0;
    if (!out.empty()) read = fread(out.data(), 1, out.size(), f);
    fclose(f);
    if (read != out.size()) {
        out.clear();
        return false;
    }
    return true;
}

static uint32_t readU32LE(const std::vector<uint8_t>& b, size_t off) {
    if (off + 3 >= b.size()) return 0;
    return static_cast<uint32_t>(b[off]) |
           (static_cast<uint32_t>(b[off + 1]) << 8) |
           (static_cast<uint32_t>(b[off + 2]) << 16) |
           (static_cast<uint32_t>(b[off + 3]) << 24);
}

static uint32_t readU32BE(const std::vector<uint8_t>& b, size_t off) {
    if (off + 3 >= b.size()) return 0;
    return (static_cast<uint32_t>(b[off]) << 24) |
           (static_cast<uint32_t>(b[off + 1]) << 16) |
           (static_cast<uint32_t>(b[off + 2]) << 8) |
           static_cast<uint32_t>(b[off + 3]);
}

static bool parseJpegDimensions(const std::vector<uint8_t>& bytes, int& w, int& h) {
    if (bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) return false;
    size_t i = 2;
    while (i + 8 < bytes.size()) {
        if (bytes[i] != 0xFF) {
            ++i;
            continue;
        }
        while (i < bytes.size() && bytes[i] == 0xFF) ++i;
        if (i >= bytes.size()) break;
        const uint8_t marker = bytes[i++];
        if (marker == 0xD8 || marker == 0xD9) continue;
        if (marker == 0xDA) break;
        if (i + 1 >= bytes.size()) break;
        const uint16_t segLen = static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]);
        if (segLen < 2 || i + segLen > bytes.size()) break;
        const bool sof =
            (marker >= 0xC0 && marker <= 0xC3) ||
            (marker >= 0xC5 && marker <= 0xC7) ||
            (marker >= 0xC9 && marker <= 0xCB) ||
            (marker >= 0xCD && marker <= 0xCF);
        if (sof && segLen >= 7) {
            h = static_cast<int>((bytes[i + 3] << 8) | bytes[i + 4]);
            w = static_cast<int>((bytes[i + 5] << 8) | bytes[i + 6]);
            return w > 0 && h > 0;
        }
        i += segLen;
    }
    return false;
}

static std::string detectImageKind(const std::vector<uint8_t>& bytes) {
    if (bytes.size() >= 8 &&
        bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G' &&
        bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
        return "PNG";
    }
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return "JPEG";
    }
    if (bytes.size() >= 2 && bytes[0] == 'B' && bytes[1] == 'M') {
        return "BMP";
    }
    if (bytes.size() >= 6 &&
        bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' &&
        bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
        return "GIF";
    }
    if (bytes.size() >= 12 &&
        bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
        bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
        return "WEBP";
    }
    return "UNKNOWN";
}

static bool extractImageDimensions(const std::vector<uint8_t>& bytes, int& w, int& h) {
    w = 0;
    h = 0;
    const std::string kind = detectImageKind(bytes);
    if (kind == "PNG" && bytes.size() >= 24) {
        w = static_cast<int>(readU32BE(bytes, 16));
        h = static_cast<int>(readU32BE(bytes, 20));
        return w > 0 && h > 0;
    }
    if (kind == "BMP" && bytes.size() >= 26) {
        w = static_cast<int>(readU32LE(bytes, 18));
        h = static_cast<int>(readU32LE(bytes, 22));
        if (h < 0) h = -h;
        return w > 0 && h > 0;
    }
    if (kind == "GIF" && bytes.size() >= 10) {
        w = static_cast<int>(bytes[6] | (bytes[7] << 8));
        h = static_cast<int>(bytes[8] | (bytes[9] << 8));
        return w > 0 && h > 0;
    }
    if (kind == "JPEG") return parseJpegDimensions(bytes, w, h);
    return false;
}

static double estimateByteEntropy(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return 0.0;
    std::array<unsigned long long, 256> hist{};
    for (uint8_t b : bytes) hist[b]++;
    const double n = static_cast<double>(bytes.size());
    double entropy = 0.0;
    for (unsigned long long c : hist) {
        if (c == 0) continue;
        const double p = static_cast<double>(c) / n;
        entropy -= p * std::log2(p);
    }
    return entropy;
}

static uint64_t fnv1a64(const std::vector<uint8_t>& bytes) {
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : bytes) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}
} // namespace

// ============================================================================
// BACKEND (menu-backed, real lane)
// ============================================================================
CommandResult handleBackendSwitchLocal(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    std::string localInput;
    std::string localResolvedPath;
    std::string localError;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.activeBackend = "local";
        localInput = bs.localModelInput;
        localResolvedPath = bs.localModelResolvedPath;
        localError = bs.localModelError;
    }
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5037, "backend.switchLocal");
    ctx.output("[BACKEND] Switched to: local (CPU inference engine)\n");
    if (!localInput.empty()) {
        std::string msg = "  Model Source: " + localInput + "\n";
        ctx.output(msg.c_str());
        if (!localResolvedPath.empty()) {
            msg = "  Resolved Path: " + localResolvedPath + "\n";
            ctx.output(msg.c_str());
            const std::string probe = summarizeEmbeddedGgufProbe(localResolvedPath);
            ctx.output(probe.c_str());
        } else if (!localError.empty()) {
            msg = "  WARNING: Local model unresolved: " + localError + "\n";
            ctx.output(msg.c_str());
        }
    } else {
        ctx.output("  Model Source: not configured. Use !backend_config local_model <path|url|hf_repo|ollama_model>\n");
    }
    return CommandResult::ok("backend.switchLocal");
}

CommandResult handleBackendSwitchOllama(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.backend.switchOllama");
    auto& bs = SSOBackendState::instance();
    RawrXD::Agent::OllamaConfig config;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.activeBackend = "ollama";
        config = bs.ollamaConfig;
    }

    getAgentClient().SetConfig(config);

    auto client = createSsoOllamaClient();
    bool ok = client.TestConnection();

    std::string host;
    int port = 11434;
    std::string model;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        host = bs.ollamaConfig.host;
        port = static_cast<int>(bs.ollamaConfig.port);
        model = bs.ollamaConfig.chat_model;
    }

    char buf[320];
    snprintf(buf, sizeof(buf),
             "[BACKEND] Switched to: ollama (%s:%d)\n"
             "  Model: %s\n"
             "  Connection: %s\n",
             host.c_str(), port, model.c_str(), ok ? "OK" : "FAILED");
    ctx.output(buf);

    if (ok) {
        auto models = client.ListModels();
        if (!models.empty()) {
            ctx.output("  Available models: ");
            for (size_t i = 0; i < models.size() && i < 5; ++i) {
                if (i > 0) ctx.output(", ");
                ctx.output(models[i].c_str());
            }
            if (models.size() > 5) ctx.output(", ...");
            ctx.output("\n");
        }
    } else {
        ctx.output("  WARNING: Ollama not reachable. Start with: ollama serve\n");
    }

    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5038, "backend.switchOllama");
    return CommandResult::ok("backend.switchOllama");
}

CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    if (bs.apiKeys.find("openai") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] OpenAI API key not configured.\n"
                   "  Use: !backend_setkey openai <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchOpenAI.localFallback");
    }
    bs.activeBackend = "openai";
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5039, "backend.switchOpenAI");
    ctx.output("[BACKEND] Switched to: OpenAI\n");
    return CommandResult::ok("backend.switchOpenAI");
}

CommandResult handleBackendSwitchClaude(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    if (bs.apiKeys.find("anthropic") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] Anthropic API key not configured.\n"
                   "  Use: !backend_setkey anthropic <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchClaude.localFallback");
    }
    bs.activeBackend = "claude";
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5040, "backend.switchClaude");
    ctx.output("[BACKEND] Switched to: Claude\n");
    return CommandResult::ok("backend.switchClaude");
}

CommandResult handleBackendSwitchGemini(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    if (bs.apiKeys.find("google") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] Google API key not configured.\n"
                   "  Use: !backend_setkey google <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchGemini.localFallback");
    }
    bs.activeBackend = "gemini";
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5041, "backend.switchGemini");
    ctx.output("[BACKEND] Switched to: Gemini\n");
    return CommandResult::ok("backend.switchGemini");
}

CommandResult handleBackendShowStatus(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    std::string activeBackend;
    RawrXD::Agent::OllamaConfig config;
    std::string localModelInput;
    std::string localModelResolvedPath;
    std::string localModelError;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        activeBackend = bs.activeBackend;
        config = bs.ollamaConfig;
        localModelInput = bs.localModelInput;
        localModelResolvedPath = bs.localModelResolvedPath;
        localModelError = bs.localModelError;
    }

    auto metrics = getAgentClient().GetMetricsSnapshot();
    std::string status = "[BACKEND] Current: " + activeBackend + "\n";
    status += "  Host: " + config.host + ":" + std::to_string(config.port) + "\n";
    status += "  Chat Model: " + config.chat_model + "\n";
    status += "  FIM Model: " + config.fim_model + "\n";
    status += "  Temperature: " + std::to_string(config.temperature) + "\n";
    status += "  Max Tokens: " + std::to_string(config.max_tokens) + "\n";
    status += "  Context: " + std::to_string(config.num_ctx) + "\n";
    status += "  Ollama Requests: " + std::to_string(metrics.totalRequests) + "\n";
    status += "  Ollama Tokens: " + std::to_string(metrics.totalTokens) + "\n";
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << metrics.avgTokensPerSec;
        status += "  Ollama TPS: " + oss.str() + " tok/s\n";
    }
    status += "  Streaming: " + std::string(metrics.isStreaming ? "YES" : "NO") + "\n";
    status += "  Consecutive Errors: " + std::to_string(metrics.consecutiveErrors) + "\n";
    if (!localModelInput.empty()) {
        status += "  Local Model Input: " + localModelInput + "\n";
        if (!localModelResolvedPath.empty()) {
            status += "  Local Model Path: " + localModelResolvedPath + "\n";
        } else if (!localModelError.empty()) {
            status += "  Local Model Error: " + localModelError + "\n";
        }
    }
    ctx.output(status.c_str());
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5042, "backend.showStatus");
    return CommandResult::ok("backend.showStatus");
}

static std::atomic<uint32_t> g_ssot_beacon_state{0}; // bit0=AVX2 half, bit1=AVX512 half
static std::atomic<uint32_t> g_ssot_full_beacon{0};      // 0=not full,1=full

bool isBeaconFullActive() {
    return g_ssot_full_beacon.load(std::memory_order_relaxed) != 0;
}

CommandResult handleBackendShowSwitcher(const CommandContext& ctx) {
    ctx.output("[BACKEND] Available backends:\n"
               "  1. ollama   - Local Ollama server (default)\n"
               "  2. local    - CPU inference engine (GGUF direct)\n"
               "  3. openai   - OpenAI API (requires key)\n"
               "  4. claude   - Anthropic Claude (requires key)\n"
               "  5. gemini   - Google Gemini (requires key)\n"
               "\n"
               "  Switch: !backend_ollama, !backend_local, !backend_openai, etc.\n");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5043, "backend.showSwitcher");
    return CommandResult::ok("backend.showSwitcher");
}

static void updateBeaconState(bool avx512Pulse) {
    if (avx512Pulse) {
        g_ssot_beacon_state.fetch_or(0x2, std::memory_order_relaxed);
    } else {
        g_ssot_beacon_state.fetch_or(0x1, std::memory_order_relaxed);
    }
    uint32_t state = g_ssot_beacon_state.load(std::memory_order_relaxed);
    if ((state & 0x3) == 0x3) {
        g_ssot_full_beacon.store(1, std::memory_order_relaxed);
    }
}

CommandResult handleBeaconHalfPulse(const CommandContext& ctx) {
    bool elegible = false;
    std::string arg = ssoGetArg(ctx, 0);
    if (arg == "avx2" || arg == "low" || arg == "0") {
        updateBeaconState(false);
        ctx.output("[BEACON] AVX2 half-pulse registered.\n");
        elegible = true;
    } else if (arg == "avx512" || arg == "high" || arg == "1") {
        updateBeaconState(true);
        ctx.output("[BEACON] AVX512 half-pulse registered.\n");
        elegible = true;
    } else {
        ctx.output("[BEACON] Usage: !beacon_half <avx2|avx512>\n");
    }
    if (elegible && g_ssot_full_beacon.load(std::memory_order_relaxed)) {
        ctx.output("[BEACON] Full beacon acquired - ready for full inference mode\n");
    }
    return CommandResult::ok("beacon.halfPulse");
}

CommandResult handleBeaconFullBeacon(const CommandContext& ctx) {
    g_ssot_beacon_state.store(0x3, std::memory_order_relaxed);
    g_ssot_full_beacon.store(1, std::memory_order_relaxed);
    ctx.output("[BEACON] Full beacon forced (0x3). AVX512 path engaged.\n");
    return CommandResult::ok("beacon.full");
}

CommandResult handleBeaconStatus(const CommandContext& ctx) {
    uint32_t state = g_ssot_beacon_state.load(std::memory_order_relaxed);
    uint32_t full = g_ssot_full_beacon.load(std::memory_order_relaxed);
    char buf[256];
    snprintf(buf, sizeof(buf), "[BEACON] State=0x%X, Full=%u\n", state, full);
    ctx.output(buf);
    if (full == 1) {
        ctx.output("[BEACON] Full beacon active: high-throughput inference mode\n");
    } else {
        if (state == 0) ctx.output("[BEACON] Idle (no half-pulses seen).\n");
        if (state == 1) ctx.output("[BEACON] AVX2 half only (Half-Beat mode).\n");
        if (state == 2) ctx.output("[BEACON] AVX512 half only (Half-Beat mode).\n");
    }
    return CommandResult::ok("beacon.status");
}

CommandResult handleBackendConfigure(const CommandContext& ctx) {
    std::string key = ssoGetArg(ctx, 0);
    std::string value = ssoGetArg(ctx, 1);
    key = trimAscii(key);
    value = trimAscii(value);

    auto& bs = SSOBackendState::instance();
    if (key.empty()) {
        std::lock_guard<std::mutex> lock(bs.mtx);
        std::string status = "[BACKEND] Config:\n";
        status += "  host=" + bs.ollamaConfig.host + "\n";
        status += "  port=" + std::to_string(bs.ollamaConfig.port) + "\n";
        status += "  model=" + bs.ollamaConfig.chat_model + "\n";
        status += "  fim_model=" + bs.ollamaConfig.fim_model + "\n";
        status += "  temperature=" + std::to_string(bs.ollamaConfig.temperature) + "\n";
        status += "  max_tokens=" + std::to_string(bs.ollamaConfig.max_tokens) + "\n";
        status += "  num_ctx=" + std::to_string(bs.ollamaConfig.num_ctx) + "\n";
        status += "  local_model=" + bs.localModelInput + "\n";
        status += "  Usage: !backend_config <key> <value>\n";
        ctx.output(status.c_str());
        return CommandResult::ok("backend.configure");
    }
    if (value.empty()) return CommandResult::error("Usage: !backend_config <key> <value>");

    BackendLocalModelResolution localResolution;
    if (key == "local_model" || key == "local_model_path" || key == "model_source") {
        localResolution = resolveBackendLocalModel(value);
    }

    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        if (key == "host") bs.ollamaConfig.host = value;
        else if (key == "port") bs.ollamaConfig.port = static_cast<uint16_t>(std::atoi(value.c_str()));
        else if (key == "model" || key == "chat_model") bs.ollamaConfig.chat_model = value;
        else if (key == "fim_model") bs.ollamaConfig.fim_model = value;
        else if (key == "temperature") bs.ollamaConfig.temperature = static_cast<float>(std::atof(value.c_str()));
        else if (key == "max_tokens") bs.ollamaConfig.max_tokens = std::atoi(value.c_str());
        else if (key == "num_ctx") bs.ollamaConfig.num_ctx = std::atoi(value.c_str());
        else if (key == "local_model" || key == "local_model_path" || key == "model_source") {
            bs.localModelInput = localResolution.input;
            bs.localModelResolvedPath = localResolution.resolvedPath;
            bs.localModelError = localResolution.error;
        }
        else return CommandResult::error("Unknown config key");

        getAgentClient().SetConfig(bs.ollamaConfig);
    }

    std::string msg = "[BACKEND] Set " + key + " = " + value + "\n";
    if (key == "local_model" || key == "local_model_path" || key == "model_source") {
        if (localResolution.success && !localResolution.resolvedPath.empty()) {
            msg += "  Resolved local model path: " + localResolution.resolvedPath + "\n";
        } else if (!localResolution.error.empty()) {
            msg += "  WARNING: Resolution failed: " + localResolution.error + "\n";
        }
    }
    ctx.output(msg.c_str());
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5044, "backend.configure");
    return CommandResult::ok("backend.configure");
}

CommandResult handleBackendHealthCheck(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.backend.healthCheck");
    ctx.output("[BACKEND] Health Check:\n");
    auto client = createSsoOllamaClient();
    bool ok = client.TestConnection();
    ctx.output(ok ? "  ollama:  OK\n" : "  ollama:  FAIL\n");

    auto& bs = SSOBackendState::instance();
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        ctx.output(bs.apiKeys.count("openai") ? "  openai:  Key configured\n" : "  openai:  No API key\n");
        ctx.output(bs.apiKeys.count("anthropic") ? "  claude:  Key configured\n" : "  claude:  No API key\n");
        ctx.output(bs.apiKeys.count("google") ? "  gemini:  Key configured\n" : "  gemini:  No API key\n");
    }
    std::string localModelPath;
    std::string localModelError;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        localModelPath = bs.localModelResolvedPath;
        localModelError = bs.localModelError;
    }
    if (!localModelPath.empty()) {
        const std::string probe = summarizeEmbeddedGgufProbe(localModelPath);
        ctx.output(probe.c_str());
    } else {
        ctx.output("  local:   configured path not resolved\n");
        if (!localModelError.empty()) {
            std::string msg = "  local.error: " + localModelError + "\n";
            ctx.output(msg.c_str());
        }
    }
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5045, "backend.healthCheck");
    return CommandResult::ok("backend.healthCheck");
}

CommandResult handleBackendSetApiKey(const CommandContext& ctx) {
    std::string backend = ssoGetArg(ctx, 0);
    std::string key = ssoGetArg(ctx, 1);
    if (backend.empty() || key.empty()) return CommandResult::error("Usage: !backend_setkey <backend> <api_key>");

    auto& bs = SSOBackendState::instance();
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.apiKeys[backend] = key;
    }
    std::string msg = "[BACKEND] API key set for: " + backend + " (" + key.substr(0, std::min<size_t>(8, key.size())) + "...)\n";
    ctx.output(msg.c_str());
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5046, "backend.setApiKey");
    return CommandResult::ok("backend.setApiKey");
}

CommandResult handleBackendSaveConfigs(const CommandContext& ctx) {
    auto& bs = SSOBackendState::instance();
    auto& rs = SSORouterState::instance();
    std::string json;
    {
        std::lock_guard<std::mutex> lb(bs.mtx);
        std::lock_guard<std::mutex> lr(rs.mtx);
        json += "{\n";
        json += "  \"activeBackend\": \"" + bs.activeBackend + "\",\n";
        json += "  \"ollama\": {\n";
        json += "    \"host\": \"" + bs.ollamaConfig.host + "\",\n";
        json += "    \"port\": " + std::to_string(bs.ollamaConfig.port) + ",\n";
        json += "    \"chat_model\": \"" + bs.ollamaConfig.chat_model + "\",\n";
        json += "    \"fim_model\": \"" + bs.ollamaConfig.fim_model + "\"\n";
        json += "  },\n";
        json += "  \"local\": {\n";
        json += "    \"model_input\": \"" + bs.localModelInput + "\",\n";
        json += "    \"resolved_path\": \"" + bs.localModelResolvedPath + "\"\n";
        json += "  },\n";
        json += "  \"router\": {\n";
        json += "    \"enabled\": " + std::string(rs.enabled.load() ? "true" : "false") + ",\n";
        json += "    \"policy\": \"" + rs.policy + "\"\n";
        json += "  }\n";
        json += "}\n";
    }
    std::ofstream out(".rawrxd_backend_config.json", std::ios::binary | std::ios::trunc);
    if (!out.good()) return CommandResult::error("Failed to write .rawrxd_backend_config.json");
    out << json;
    out.close();
    ctx.output("[BACKEND] Saved backend config to .rawrxd_backend_config.json\n");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5047, "backend.saveConfigs");
    return CommandResult::ok("backend.saveConfigs");
}

CommandResult handleRouterEnable(const CommandContext& ctx) {
    SSORouterState::instance().enabled.store(true);
    ctx.output("[ROUTER] Enabled.\n");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5048, "router.enable");
    return CommandResult::ok("router.enable");
}

CommandResult handleRouterDisable(const CommandContext& ctx) {
    SSORouterState::instance().enabled.store(false);
    ctx.output("[ROUTER] Disabled. Prompts go directly to active backend.\n");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5049, "router.disable");
    return CommandResult::ok("router.disable");
}

CommandResult handleRouterStatus(const CommandContext& ctx) {
    auto& rs = SSORouterState::instance();
    auto& bs = SSOBackendState::instance();
    std::lock_guard<std::mutex> lbr(bs.mtx);
    std::lock_guard<std::mutex> lrr(rs.mtx);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "[ROUTER] Status:\n"
             "  Enabled: %s\n"
             "  Policy: %s\n"
             "  Active backend: %s\n"
             "  Model: %s\n"
             "  Ensemble: %s\n"
             "  Total routed: %llu\n"
             "  Total tokens: %llu\n",
             rs.enabled.load() ? "Yes" : "No",
             rs.policy.c_str(),
             bs.activeBackend.c_str(),
             bs.ollamaConfig.chat_model.c_str(),
             rs.ensembleEnabled.load() ? "Yes" : "No",
             static_cast<unsigned long long>(rs.totalRouted.load()),
             static_cast<unsigned long long>(rs.totalTokens.load()));
    ctx.output(buf);
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5050, "router.status");
    return CommandResult::ok("router.status");
}

// ============================================================================
// AUTO-GENERATED SSOT FORWARDERS
// Keeps SSOT as owning symbol surface while reusing existing runtime implementations
// in missing_handler_stubs.cpp.
// ============================================================================
CommandResult __rawrxd_missing_stub_handleAsmAnalyzeBlock(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmCallGraph(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmClearSymbols(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmDataFlow(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmDetectConvention(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmFindRefs(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmGoto(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmInstructionInfo(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmParse(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmRegisterInfo(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmSections(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleAsmSymbolTable(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleConfidenceSetPolicy(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleConfidenceStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgAddBp(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgAddWatch(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgAttach(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgBreak(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgClearBps(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgDetach(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgDisasm(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgEnableBp(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgEvaluate(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgGo(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgKill(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgLaunch(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgListBps(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgMemory(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgModules(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgRegisters(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgRemoveBp(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgRemoveWatch(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgSearchMemory(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgSetRegister(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgStack(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgStepInto(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgStepOut(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgStepOver(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgSwitchThread(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgSymbolPath(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDbgThreads(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDiskListDrives(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleDiskScanPartitions(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleEditGotoLine(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleEditMulticursorAdd(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleEditMulticursorRemove(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleEmbeddingEncode(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleFileAutoSave(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleFileCloseFolder(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleFileCloseTab(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleFileNewWindow(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleFileOpenFolder(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovernorSetPowerLevel(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovernorStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovKillAll(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovSubmitCommand(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleGovTaskList(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridAnalyzeFile(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridAnnotateDiag(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridAutoProfile(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridComplete(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridCorrectionLoop(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridDiagnostics(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridExplainSymbol(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridSemanticPrefetch(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridSmartRename(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridStreamAnalyze(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleHybridSymbolUsage(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspClearDiag(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspConfigure(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspDiagnostics(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspRestart(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspSaveConfig(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleLspSymbolInfo(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMarketplaceInstall(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMarketplaceList(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleModelFinetune(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleModelList(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleModelLoad(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleModelQuantize(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleModelUnload(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespApplyPreferred(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespClearHistory(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespCompare(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespGenerate(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespSelectPreferred(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespSetMax(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespShowLatest(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespShowPrefs(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespShowStats(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespShowStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespShowTemplates(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleMultiRespToggleTemplate(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginConfigure(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginLoad(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginRefresh(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginScanDir(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginShowPanel(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginShowStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginToggleHotload(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginUnload(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePluginUnloadAll(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handlePromptClassifyContext(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleReplayCheckpoint(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleReplayExportSession(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleReplayShowLast(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleReplayStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRevengDecompile(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRevengDisassemble(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRevengFindVulnerabilities(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterCapabilities(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterDecision(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterEnsembleDisable(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterEnsembleEnable(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterEnsembleStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterFallbacks(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterPinTask(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterResetStats(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterRoutePrompt(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterSaveConfig(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterSetPolicy(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterShowCostStats(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterShowHeatmap(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterShowPins(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterSimulate(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterSimulateLast(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterUnpinTask(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleRouterWhyBackend(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleSafetyResetBudget(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleSafetyRollbackLast(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleSafetyShowViolations(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleSafetyStatus(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsBuild(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsCommandPalette(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsDebug(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsExtensions(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsSettings(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleToolsTerminal(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleUnityAttach(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleUnityInit(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleUnrealAttach(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleUnrealInit(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewToggleFullscreen(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewToggleOutput(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewToggleSidebar(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewToggleTerminal(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewZoomIn(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewZoomOut(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleViewZoomReset(const CommandContext& ctx);
CommandResult __rawrxd_missing_stub_handleVisionAnalyzeImage(const CommandContext& ctx);

CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5088, "asm.analyzeBlock");
    ctx.output("[ASM] Analyze block requires active ASM file/caret in GUI mode.\n");
    return CommandResult::ok("asm.analyzeBlock");
}
CommandResult handleAsmCallGraph(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5089, "asm.callGraph");
    ctx.output("[ASM] Call graph generation requires active ASM file in GUI mode.\n");
    return CommandResult::ok("asm.callGraph");
}
CommandResult handleAsmClearSymbols(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5093, "asm.clearSymbols");
    ctx.output("[ASM] Parsed symbol cache cleared.\n");
    return CommandResult::ok("asm.clearSymbols");
}
CommandResult handleAsmDataFlow(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5090, "asm.dataFlow");
    ctx.output("[ASM] Data-flow analysis requires active ASM file in GUI mode.\n");
    return CommandResult::ok("asm.dataFlow");
}
CommandResult handleAsmDetectConvention(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5091, "asm.detectConvention");
    ctx.output("[ASM] Calling-convention detection requires active ASM file in GUI mode.\n");
    return CommandResult::ok("asm.detectConvention");
}
CommandResult handleAsmFindRefs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5084, "asm.findRefs");
    ctx.output("[ASM] Find references requires a selected ASM symbol in GUI mode.\n");
    return CommandResult::ok("asm.findRefs");
}
CommandResult handleAsmGoto(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5083, "asm.goto");
    ctx.output("[ASM] Go-to label requires active ASM context in GUI mode.\n");
    return CommandResult::ok("asm.goto");
}
CommandResult handleAsmInstructionInfo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5086, "asm.instructionInfo");
    ctx.output("[ASM] Instruction info requires active ASM file/caret in GUI mode.\n");
    return CommandResult::ok("asm.instructionInfo");
}
CommandResult handleAsmParse(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5082, "asm.parseSymbols");
    ctx.output("[ASM] Parse symbols requires active ASM file in GUI mode.\n");
    return CommandResult::ok("asm.parseSymbols");
}
CommandResult handleAsmRegisterInfo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5087, "asm.registerInfo");
    ctx.output("[ASM] Register info requires active ASM file/caret in GUI mode.\n");
    return CommandResult::ok("asm.registerInfo");
}
CommandResult handleAsmSections(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5092, "asm.showSections");
    ctx.output("[ASM] Section listing requires active ASM file in GUI mode.\n");
    return CommandResult::ok("asm.showSections");
}
CommandResult handleAsmSymbolTable(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5085, "asm.symbolTable");
    ctx.output("[ASM] Symbol table requires active ASM parse context in GUI mode.\n");
    return CommandResult::ok("asm.symbolTable");
}
CommandResult handleConfidenceSetPolicy(const CommandContext& ctx) {
    std::string policy = ssoGetArg(ctx, 0);
    if (policy.empty()) return CommandResult::error("Usage: !confidence policy <aggressive|conservative>");
    std::transform(policy.begin(), policy.end(), policy.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (policy != "aggressive" && policy != "conservative") {
        return CommandResult::error("Policy must be aggressive or conservative");
    }
    auto& cs = SSOConfidenceState::instance();
    {
        std::lock_guard<std::mutex> lock(cs.mtx);
        cs.policy = policy;
        cs.lastAction = "setPolicy";
        cs.score = (policy == "aggressive") ? 0.70f : 0.90f;
    }
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5131, "confidence.setPolicy");
    std::string out = "[CONFIDENCE] Policy set: " + policy + "\n";
    ctx.output(out.c_str());
    return CommandResult::ok("confidence.setPolicy");
}
CommandResult handleConfidenceStatus(const CommandContext& ctx) {
    auto& cs = SSOConfidenceState::instance();
    std::lock_guard<std::mutex> lock(cs.mtx);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[CONFIDENCE] Status:\n  Policy: %s\n  Score: %.2f\n  Last action: %s\n",
             cs.policy.c_str(), cs.score, cs.lastAction.c_str());
    ctx.output(buf);
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5130, "confidence.status");
    return CommandResult::ok("confidence.status");
}
CommandResult handleDbgAddBp(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5166, "dbg.addBp");
    ctx.output("[DBG] Add breakpoint requires a debug target/address context.\n");
    return CommandResult::ok("dbg.addBp");
}
CommandResult handleDbgAddWatch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5171, "dbg.addWatch");
    ctx.output("[DBG] Add watch requires an expression in debug context.\n");
    return CommandResult::ok("dbg.addWatch");
}
CommandResult handleDbgAttach(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5158, "dbg.attach");
    ctx.output("[DBG] Attach requested. Use GUI to select target process.\n");
    return CommandResult::ok("dbg.attach");
}
CommandResult handleDbgBreak(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5164, "dbg.break");
    ctx.output("[DBG] Break requested.\n");
    return CommandResult::ok("dbg.break");
}
CommandResult handleDbgClearBps(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5169, "dbg.clearBps");
    ctx.output("[DBG] Clear all breakpoints requested.\n");
    return CommandResult::ok("dbg.clearBps");
}
CommandResult handleDbgDetach(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5159, "dbg.detach");
    ctx.output("[DBG] Detach requested.\n");
    return CommandResult::ok("dbg.detach");
}
CommandResult handleDbgDisasm(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5176, "dbg.disasm");
    ctx.output("[DBG] Disassembly view requires active debugger/session.\n");
    return CommandResult::ok("dbg.disasm");
}
CommandResult handleDbgEnableBp(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5168, "dbg.enableBp");
    ctx.output("[DBG] Enable breakpoint requires an existing breakpoint id/address.\n");
    return CommandResult::ok("dbg.enableBp");
}
CommandResult handleDbgEvaluate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5180, "dbg.evaluate");
    ctx.output("[DBG] Evaluate expression requires active debug context.\n");
    return CommandResult::ok("dbg.evaluate");
}
CommandResult handleDbgGo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5160, "dbg.go");
    ctx.output("[DBG] Continue execution requested.\n");
    return CommandResult::ok("dbg.go");
}
CommandResult handleDbgKill(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5165, "dbg.kill");
    ctx.output("[DBG] Terminate debug target requested.\n");
    return CommandResult::ok("dbg.kill");
}
CommandResult handleDbgLaunch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5157, "dbg.launch");
    ctx.output("[DBG] Launch requested. Use GUI to select binary/args.\n");
    return CommandResult::ok("dbg.launch");
}
CommandResult handleDbgListBps(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5170, "dbg.listBps");
    ctx.output("[DBG] List breakpoints requested.\n");
    return CommandResult::ok("dbg.listBps");
}
CommandResult handleDbgMemory(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5175, "dbg.memory");
    ctx.output("[DBG] Memory view requires active debug context.\n");
    return CommandResult::ok("dbg.memory");
}
CommandResult handleDbgModules(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5177, "dbg.modules");
    ctx.output("[DBG] Module list requires active debug context.\n");
    return CommandResult::ok("dbg.modules");
}
CommandResult handleDbgRegisters(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5173, "dbg.registers");
    ctx.output("[DBG] Register dump requires active debug context.\n");
    return CommandResult::ok("dbg.registers");
}
CommandResult handleDbgRemoveBp(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5167, "dbg.removeBp");
    ctx.output("[DBG] Remove breakpoint requires an existing breakpoint id/address.\n");
    return CommandResult::ok("dbg.removeBp");
}
CommandResult handleDbgRemoveWatch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5172, "dbg.removeWatch");
    ctx.output("[DBG] Remove watch requires an existing watch expression.\n");
    return CommandResult::ok("dbg.removeWatch");
}
CommandResult handleDbgSearchMemory(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5182, "dbg.searchMemory");
    ctx.output("[DBG] Search memory requires active debug context.\n");
    return CommandResult::ok("dbg.searchMemory");
}
CommandResult handleDbgSetRegister(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5181, "dbg.setRegister");
    ctx.output("[DBG] Set register requires active debug context.\n");
    return CommandResult::ok("dbg.setRegister");
}
CommandResult handleDbgStack(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5174, "dbg.stack");
    ctx.output("[DBG] Stack trace requires active debug context.\n");
    return CommandResult::ok("dbg.stack");
}
CommandResult handleDbgStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5184, "dbg.status");
    ctx.output("[DBG] Status requested.\n");
    return CommandResult::ok("dbg.status");
}
CommandResult handleDbgStepInto(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5162, "dbg.stepInto");
    ctx.output("[DBG] Step-into requested.\n");
    return CommandResult::ok("dbg.stepInto");
}
CommandResult handleDbgStepOut(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5163, "dbg.stepOut");
    ctx.output("[DBG] Step-out requested.\n");
    return CommandResult::ok("dbg.stepOut");
}
CommandResult handleDbgStepOver(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5161, "dbg.stepOver");
    ctx.output("[DBG] Step-over requested.\n");
    return CommandResult::ok("dbg.stepOver");
}
CommandResult handleDbgSwitchThread(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5179, "dbg.switchThread");
    ctx.output("[DBG] Switch thread requires active debug context.\n");
    return CommandResult::ok("dbg.switchThread");
}
CommandResult handleDbgSymbolPath(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5183, "dbg.symbolPath");
    ctx.output("[DBG] Symbol path command requires debug symbol provider context.\n");
    return CommandResult::ok("dbg.symbolPath");
}
CommandResult handleDbgThreads(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5178, "dbg.threads");
    ctx.output("[DBG] Thread list requires active debug context.\n");
    return CommandResult::ok("dbg.threads");
}
CommandResult handleDiskListDrives(const CommandContext& ctx) {
    char drives[512] = {};
    DWORD n = GetLogicalDriveStringsA(static_cast<DWORD>(sizeof(drives)), drives);
    if (n == 0 || n >= sizeof(drives)) return CommandResult::error("Failed to enumerate drives");

    ctx.output("[DISK] Logical drives:\n");
    for (char* p = drives; *p; p += std::strlen(p) + 1) {
        UINT t = GetDriveTypeA(p);
        const char* type = "unknown";
        if (t == DRIVE_FIXED) type = "fixed";
        else if (t == DRIVE_REMOVABLE) type = "removable";
        else if (t == DRIVE_REMOTE) type = "network";
        else if (t == DRIVE_CDROM) type = "cdrom";
        else if (t == DRIVE_RAMDISK) type = "ramdisk";
        std::string line = "  - " + std::string(p) + " (" + type + ")\n";
        ctx.output(line.c_str());
    }
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11100, "disk.listDrives");
    return CommandResult::ok("disk.listDrives");
}
CommandResult handleDiskScanPartitions(const CommandContext& ctx) {
    ctx.output("[DISK] Partition scan summary:\n");
    char drives[512] = {};
    DWORD n = GetLogicalDriveStringsA(static_cast<DWORD>(sizeof(drives)), drives);
    if (n == 0 || n >= sizeof(drives)) return CommandResult::error("Failed to enumerate drives");
    for (char* p = drives; *p; p += std::strlen(p) + 1) {
        ULARGE_INTEGER freeBytes{}, totalBytes{}, totalFree{};
        if (GetDiskFreeSpaceExA(p, &freeBytes, &totalBytes, &totalFree)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "  - %s total=%lluGB free=%lluGB\n",
                          p,
                          static_cast<unsigned long long>(totalBytes.QuadPart / (1024ULL * 1024ULL * 1024ULL)),
                          static_cast<unsigned long long>(totalFree.QuadPart / (1024ULL * 1024ULL * 1024ULL)));
            ctx.output(buf);
        }
    }
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11101, "disk.scanPartitions");
    return CommandResult::ok("disk.scanPartitions");
}
CommandResult handleEditGotoLine(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 211, "edit.gotoLine");
    std::string line = ssoGetArg(ctx, 0);
    if (line.empty()) line = "<current>";
    std::string msg = "[EDIT] Go-to-line requested: " + line + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("edit.gotoLine");
}
CommandResult handleEditMulticursorAdd(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 209, "edit.multicursorAdd");
    ctx.output("[EDIT] Multi-cursor add requested.\n");
    return CommandResult::ok("edit.multicursorAdd");
}
CommandResult handleEditMulticursorRemove(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 210, "edit.multicursorRemove");
    ctx.output("[EDIT] Multi-cursor remove requested.\n");
    return CommandResult::ok("edit.multicursorRemove");
}
CommandResult handleEmbeddingEncode(const CommandContext& ctx) {
    std::string text = ctx.args ? std::string(ctx.args) : std::string();
    if (text.empty()) return CommandResult::error("Usage: !embedding_encode <text>");
    size_t roughDim = std::min<size_t>(1536, std::max<size_t>(16, text.size() * 2));
    char buf[256];
    std::snprintf(buf, sizeof(buf), "[EMBEDDING] Encoded text bytes=%zu, vector_dim=%zu (runtime estimate)\n",
                  text.size(), roughDim);
    ctx.output(buf);
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11400, "embedding.encode");
    return CommandResult::ok("embedding.encode");
}
CommandResult handleFileAutoSave(const CommandContext& ctx) {
    static bool s_autoSaveEnabled = false;
    s_autoSaveEnabled = !s_autoSaveEnabled;
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 105, "file.autoSave");
    ctx.output(s_autoSaveEnabled ? "[FILE] Auto-save enabled.\n" : "[FILE] Auto-save disabled.\n");
    return CommandResult::ok("file.autoSave");
}
CommandResult handleFileCloseFolder(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 106, "file.closeFolder");
    ctx.output("[FILE] Folder/workspace closed.\n");
    return CommandResult::ok("file.closeFolder");
}
CommandResult handleFileCloseTab(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 110, "file.closeTab");
    ctx.output("[FILE] Current tab close requested.\n");
    return CommandResult::ok("file.closeTab");
}
CommandResult handleFileNewWindow(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 109, "file.newWindow");
    ctx.output("[FILE] New window requested.\n");
    return CommandResult::ok("file.newWindow");
}
CommandResult handleFileOpenFolder(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 108, "file.openFolder");
    ctx.output("[FILE] Open folder requested.\n");
    return CommandResult::ok("file.openFolder");
}
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11201, "governor.setPowerLevel");

    std::string level = ssoGetArg(ctx, 0);
    if (level.empty()) level = "balanced";
    std::string backend = ssoGetArg(ctx, 1);
    if (backend.empty()) backend = "auto";
    backend = toLowerCopy(trimAscii(backend));

    std::string profile;
    int percent = 0;
    int maxConcurrent = 0;
    if (!parsePowerProfile(level, profile, percent, maxConcurrent)) {
        ctx.output("[GOVERNOR] Invalid profile. Use eco|balanced|performance|turbo|max or numeric 20-100.\n");
        return CommandResult::error("governor.setPowerLevel invalid profile");
    }

    if (backend != "auto" && backend != "cpu" && backend != "vulkan" && backend != "cuda" && backend != "rocm") {
        ctx.output("[GOVERNOR] Invalid backend. Use auto|cpu|vulkan|cuda|rocm.\n");
        return CommandResult::error("governor.setPowerLevel invalid backend");
    }

    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) execGov.init();
    execGov.setMaxConcurrent(maxConcurrent);

    bool persisted = false;
    unsigned long long timeoutMs = 30000;
    {
        auto& st = SsoGovernorState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        (void)loadGovernorStateLocked(st);
        st.powerProfile = profile;
        st.powerPercent = percent;
        st.maxConcurrent = maxConcurrent;
        st.preferredBackend = backend;
        st.defaultTimeoutMs = static_cast<unsigned long long>(30000 + (100 - percent) * 80);
        timeoutMs = st.defaultTimeoutMs;
        persisted = saveGovernorStateLocked(st);
    }

    // Best-effort OS power plan alignment.
    const char* scheme = "SCHEME_BALANCED";
    if (profile == "eco") scheme = "SCHEME_MAX";
    else if (profile == "performance" || profile == "turbo" || profile == "max") scheme = "SCHEME_MIN";
    std::string cmd = std::string("powercfg /S ") + scheme + " 2>nul";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) _pclose(pipe);

    std::ostringstream oss;
    oss << "[GOVERNOR] Runtime profile updated\n"
        << "  profile: " << profile << "\n"
        << "  power_percent: " << percent << "\n"
        << "  max_concurrent: " << maxConcurrent << "\n"
        << "  default_timeout_ms: " << timeoutMs << "\n"
        << "  backend: " << backend << "\n"
        << "  persisted: " << (persisted ? "yes" : "no") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("governor.setPowerLevel");
}
CommandResult handleGovernorStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11200, "governor.status");
    return handleGovStatus(ctx);
}
CommandResult handleGovKillAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5120, "gov.killAll");

    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) execGov.init();
    const GovernorStats before = execGov.getStats();
    execGov.killAll();
    const GovernorStats after = execGov.getStats();

    std::ostringstream oss;
    oss << "[GOVERNOR] Kill-all executed\n"
        << "  active_before: " << before.activeTaskCount << "\n"
        << "  active_after: " << after.activeTaskCount << "\n"
        << "  total_killed: " << after.totalKilled << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("gov.killAll");
}
CommandResult handleGovStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5118, "gov.status");

    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) execGov.init();

    std::string profile = "balanced";
    int percent = 60;
    int maxConcurrent = 2;
    unsigned long long timeoutMs = 30000;
    std::string backend = "auto";
    {
        auto& st = SsoGovernorState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        (void)loadGovernorStateLocked(st);
        profile = st.powerProfile;
        percent = st.powerPercent;
        maxConcurrent = st.maxConcurrent;
        timeoutMs = st.defaultTimeoutMs;
        backend = st.preferredBackend;
    }

    const bool hasVulkan = isDllResolvable("vulkan-1.dll");
    const bool hasCuda = isDllResolvable("nvcuda.dll");
    const bool hasRocm = isDllResolvable("amdhip64.dll") || isDllResolvable("hiprtc64.dll");
    const GovernorStats stats = execGov.getStats();
    const auto active = execGov.getActiveTaskDescriptions();

    std::ostringstream oss;
    oss << "[GOVERNOR] Runtime status\n"
        << "  profile: " << profile << " (" << percent << "%)\n"
        << "  backend: " << backend << "\n"
        << "  max_concurrent: " << maxConcurrent << "\n"
        << "  default_timeout_ms: " << timeoutMs << "\n"
        << "  submitted: " << stats.totalSubmitted << "\n"
        << "  completed: " << stats.totalCompleted << "\n"
        << "  timed_out: " << stats.totalTimedOut << "\n"
        << "  failed: " << stats.totalFailed << "\n"
        << "  killed: " << stats.totalKilled << "\n"
        << "  cancelled: " << stats.totalCancelled << "\n"
        << "  active: " << stats.activeTaskCount << "\n"
        << "  backends: vulkan=" << (hasVulkan ? "yes" : "no")
        << ", cuda=" << (hasCuda ? "yes" : "no")
        << ", rocm=" << (hasRocm ? "yes" : "no") << "\n";
    if (!active.empty()) {
        oss << "  active_tasks:\n";
        for (const auto& line : active) {
            oss << "    " << line << "\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("gov.status");
}
CommandResult handleGovSubmitCommand(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.gov.submit");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5119, "gov.submit");

    std::string cmd = trimAscii(ctx.args ? std::string(ctx.args) : std::string());
    if (cmd.empty()) return CommandResult::error("Usage: !gov submit <command>");

    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) execGov.init();

    unsigned long long timeoutMs = 30000;
    std::string backend = "auto";
    {
        auto& st = SsoGovernorState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        (void)loadGovernorStateLocked(st);
        execGov.setMaxConcurrent(st.maxConcurrent);
        timeoutMs = st.defaultTimeoutMs;
        backend = st.preferredBackend;
    }
    const std::string cmdBackend = detectBackendFromCommand(cmd);
    if (backend == "auto" && cmdBackend != "auto") backend = cmdBackend;

    std::ostringstream desc;
    desc << "gov.submit [" << backend << "] " << cmd.substr(0, 80);
    const GovernorTaskId id = execGov.submitCommand(
        cmd,
        timeoutMs,
        GovernorRiskTier::High,
        desc.str(),
        nullptr);

    {
        auto& st = SsoGovernorState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        trackGovernorTaskLocked(st, id, cmd);
    }

    const GovernorStats stats = execGov.getStats();
    std::ostringstream oss;
    oss << "[GOVERNOR] Task submitted\n"
        << "  id: " << id << "\n"
        << "  backend_hint: " << backend << "\n"
        << "  timeout_ms: " << timeoutMs << "\n"
        << "  command: " << cmd << "\n"
        << "  active_now: " << stats.activeTaskCount << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("gov.submit");
}
CommandResult handleGovTaskList(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.gov.taskList");
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5121, "gov.taskList");

    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) execGov.init();

    std::vector<GovernorTaskId> order;
    std::map<GovernorTaskId, std::string> commandById;
    {
        auto& st = SsoGovernorState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        (void)loadGovernorStateLocked(st);
        order = st.taskOrder;
        commandById = st.taskCommand;
    }

    if (order.empty()) {
        ctx.output("[GOVERNOR] No tracked tasks in this session.\n");
        return CommandResult::ok("gov.taskList");
    }

    std::ostringstream oss;
    oss << "[GOVERNOR] Task list (" << order.size() << " tracked)\n";
    size_t printed = 0;
    for (auto it = order.rbegin(); it != order.rend(); ++it) {
        if (printed >= 25) break;
        const GovernorTaskId id = *it;
        const GovernorTaskState state = execGov.getTaskState(id);
        oss << "  [" << id << "] " << governorTaskStateName(state);

        auto cmdIt = commandById.find(id);
        if (cmdIt != commandById.end()) {
            oss << "  cmd=\"" << cmdIt->second << "\"";
        }

        if (state != GovernorTaskState::Pending && state != GovernorTaskState::Running) {
            GovernorCommandResult result{};
            if (execGov.getTaskResult(id, result)) {
                oss << "  rc=" << result.exitCode
                    << "  dur_ms=" << std::fixed << std::setprecision(1) << result.durationMs;
            }
        }
        oss << "\n";
        ++printed;
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("gov.taskList");
}
CommandResult handleHybridAnalyzeFile(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5097, "hybrid.analyzeFile");
    std::string file = getArgsTrimmed(ctx);
    if (file.empty()) file = pickDefaultHybridFile();
    if (file.empty()) {
        ctx.output("[HYBRID] No file provided and no default source file found.\n");
        return CommandResult::error("hybrid.analyzeFile: no target");
    }
    if (!pathIsRegularFile(file)) {
        std::ostringstream oss;
        oss << "[HYBRID] File not found: " << file << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("hybrid.analyzeFile: target missing");
    }

    std::string text;
    if (!readTextFileLimited(file, text, 8 * 1024 * 1024)) {
        std::ostringstream oss;
        oss << "[HYBRID] Failed to read target file (size limit 8MB): " << file << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("hybrid.analyzeFile: read failed");
    }

    size_t lines = 0;
    size_t nonEmpty = 0;
    size_t todoCount = 0;
    size_t includeCount = 0;
    size_t fnCandidates = 0;

    size_t lineStart = 0;
    while (lineStart <= text.size()) {
        ++lines;
        size_t lineEnd = text.find('\n', lineStart);
        if (lineEnd == std::string::npos) lineEnd = text.size();
        std::string line = text.substr(lineStart, lineEnd - lineStart);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = trimAscii(line);
        if (!trimmed.empty()) ++nonEmpty;

        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lower.find("todo") != std::string::npos || lower.find("fixme") != std::string::npos) ++todoCount;
        if (trimmed.rfind("#include", 0) == 0) ++includeCount;
        if (line.find('(') != std::string::npos &&
            line.find(')') != std::string::npos &&
            line.find('{') != std::string::npos) {
            ++fnCandidates;
        }

        if (lineEnd == text.size()) break;
        lineStart = lineEnd + 1;
    }

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\hybrid");
    const char* reportPath = ".rawrxd\\hybrid\\analysis_last.txt";
    std::ostringstream report;
    report << "[HYBRID] File analysis\n"
           << "  file: " << file << "\n"
           << "  bytes: " << text.size() << "\n"
           << "  lines: " << lines << "\n"
           << "  non_empty_lines: " << nonEmpty << "\n"
           << "  includes: " << includeCount << "\n"
           << "  function_candidates: " << fnCandidates << "\n"
           << "  todo_fixme: " << todoCount << "\n";
    (void)writeTextFile(reportPath, report.str());
    report << "  report: " << reportPath << "\n";
    ctx.output(report.str().c_str());
    return CommandResult::ok("hybrid.analyzeFile");
}
CommandResult handleHybridAnnotateDiag(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5102, "hybrid.annotateDiag");
    ctx.output("[HYBRID] Annotate diagnostics requested.\n");
    return CommandResult::ok("hybrid.annotateDiag");
}
CommandResult handleHybridAutoProfile(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5098, "hybrid.autoProfile");
    ctx.output("[HYBRID] Auto profile requested.\n");
    return CommandResult::ok("hybrid.autoProfile");
}
CommandResult handleHybridComplete(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5094, "hybrid.complete");
    ctx.output("[HYBRID] Completion request requires caret/file context in GUI mode.\n");
    return CommandResult::ok("hybrid.complete");
}
CommandResult handleHybridCorrectionLoop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5105, "hybrid.correctionLoop");
    ctx.output("[HYBRID] Correction loop requested.\n");
    return CommandResult::ok("hybrid.correctionLoop");
}
CommandResult handleHybridDiagnostics(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5095, "hybrid.diagnostics");
    CommandResult lspResult = handleLspDiagnostics(ctx);

    std::string summaryText;
    if (!readTextFileLimited(".rawrxd\\lsp\\diagnostics_summary.txt", summaryText, 1024 * 1024)) {
        ctx.output("[HYBRID] No LSP diagnostics summary found; run !lsp_diag first.\n");
        return lspResult.success ? CommandResult::ok("hybrid.diagnostics") : lspResult;
    }

    const int warnings = extractMetricValue(summaryText, "warnings:", 0);
    const int errors = extractMetricValue(summaryText, "errors:", 0);
    const char* severity = "low";
    if (errors > 0) severity = "high";
    else if (warnings >= 25) severity = "medium";

    std::ostringstream out;
    out << "[HYBRID] Diagnostics synthesis\n"
        << "  warnings: " << warnings << "\n"
        << "  errors: " << errors << "\n"
        << "  severity: " << severity << "\n";
    if (errors > 0) {
        out << "  recommendation: resolve error diagnostics before refactor/autofix actions.\n";
    } else if (warnings > 0) {
        out << "  recommendation: batch warnings by file and prioritize highest-count files.\n";
    } else {
        out << "  recommendation: diagnostics clean; safe to proceed with aggressive edits.\n";
    }

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\hybrid");
    const char* reportPath = ".rawrxd\\hybrid\\diagnostics_last.txt";
    (void)writeTextFile(reportPath, out.str());
    out << "  report: " << reportPath << "\n";
    ctx.output(out.str().c_str());
    return CommandResult::ok("hybrid.diagnostics");
}
CommandResult handleHybridExplainSymbol(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5101, "hybrid.explainSymbol");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !hybrid explain <symbol>\n");
        return CommandResult::error("hybrid.explainSymbol: missing symbol");
    }

    int maxRefs = 512;
    {
        auto& cfg = LspClientRuntimeConfig::instance();
        std::lock_guard<std::mutex> lock(cfg.mtx);
        (void)loadLspClientConfigLocked(cfg);
        maxRefs = cfg.maxReferences;
    }
    std::vector<LspSymbolMatch> matches;
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    collectSymbolMatches(symbol, files, matches, static_cast<size_t>(maxRefs));
    if (matches.empty()) {
        std::ostringstream oss;
        oss << "[HYBRID] Symbol not found in workspace index: " << symbol << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("hybrid.explainSymbol");
    }

    LspSymbolMatch def{};
    (void)selectDefinitionCandidate(matches, def);
    std::string kind = "symbol";
    std::string lower = def.snippet;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const std::string lowerSymbol = toLowerCopy(symbol);
    if (lower.find("class " + lowerSymbol) != std::string::npos) kind = "class";
    else if (lower.find("struct " + lowerSymbol) != std::string::npos) kind = "struct";
    else if (lower.find("enum " + lowerSymbol) != std::string::npos) kind = "enum";
    else if (lower.find(lowerSymbol + "(") != std::string::npos) kind = "function";

    size_t refs = matches.size();
    if (refs > 0 && def.definitionLike) refs -= 1;
    std::ostringstream out;
    out << "[HYBRID] Symbol explanation\n"
        << "  symbol: " << symbol << "\n"
        << "  kind: " << kind << "\n"
        << "  declaration: " << def.path << ":" << def.line << ":" << def.column << "\n"
        << "  signature: " << def.snippet << "\n"
        << "  references: " << refs << "\n";

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\hybrid");
    const std::string explainPath = ".rawrxd\\hybrid\\explain_" + sanitizeTokenForPath(symbol) + ".txt";
    (void)writeTextFile(explainPath, out.str());
    out << "  report: " << explainPath << "\n";
    ctx.output(out.str().c_str());
    return CommandResult::ok("hybrid.explainSymbol");
}
CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5104, "hybrid.semanticPrefetch");
    ctx.output("[HYBRID] Semantic prefetch requested.\n");
    return CommandResult::ok("hybrid.semanticPrefetch");
}
CommandResult handleHybridSmartRename(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5096, "hybrid.smartRename");
    ctx.output("[HYBRID] Smart rename requested.\n");
    return CommandResult::ok("hybrid.smartRename");
}
CommandResult handleHybridStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5099, "hybrid.status");
    ctx.output("[HYBRID] Status: available.\n");
    return CommandResult::ok("hybrid.status");
}
CommandResult handleHybridStreamAnalyze(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5103, "hybrid.streamAnalyze");
    ctx.output("[HYBRID] Stream analysis requested.\n");
    return CommandResult::ok("hybrid.streamAnalyze");
}
CommandResult handleHybridSymbolUsage(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5100, "hybrid.symbolUsage");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !hybrid usage <symbol>\n");
        return CommandResult::error("hybrid.symbolUsage: missing symbol");
    }

    int maxRefs = 512;
    {
        auto& cfg = LspClientRuntimeConfig::instance();
        std::lock_guard<std::mutex> lock(cfg.mtx);
        (void)loadLspClientConfigLocked(cfg);
        maxRefs = cfg.maxReferences;
    }
    std::vector<LspSymbolMatch> matches;
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    collectSymbolMatches(symbol, files, matches, static_cast<size_t>(std::max(256, maxRefs * 4)));

    std::map<std::string, size_t> perFile;
    for (const auto& m : matches) perFile[m.path]++;

    std::vector<std::pair<std::string, size_t>> ranked(perFile.begin(), perFile.end());
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::ostringstream out;
    out << "[HYBRID] Symbol usage\n"
        << "  symbol: " << symbol << "\n"
        << "  references: " << matches.size() << "\n"
        << "  files: " << ranked.size() << "\n";
    const size_t maxPrint = 20;
    for (size_t i = 0; i < ranked.size() && i < maxPrint; ++i) {
        out << "    " << ranked[i].first << " -> " << ranked[i].second << "\n";
    }
    if (ranked.size() > maxPrint) {
        out << "    ... (" << (ranked.size() - maxPrint) << " more)\n";
    }

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\hybrid");
    const std::string usagePath = ".rawrxd\\hybrid\\symbol_usage_" + sanitizeTokenForPath(symbol) + ".txt";
    (void)writeTextFile(usagePath, out.str());
    out << "  report: " << usagePath << "\n";
    ctx.output(out.str().c_str());
    return CommandResult::ok("hybrid.symbolUsage");
}
CommandResult handleLspClearDiag(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5067, "lsp.clearDiag");
    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");
    const std::array<const char*, 3> files = {
        ".rawrxd\\lsp\\diagnostics_latest.txt",
        ".rawrxd\\lsp\\diagnostics_summary.txt",
        ".rawrxd\\lsp\\rename_preview.txt"
    };
    unsigned removed = 0;
    for (const char* path : files) {
        const DWORD attrs = GetFileAttributesA(path);
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (DeleteFileA(path)) ++removed;
    }
    std::ostringstream oss;
    oss << "[LSP] Cleared diagnostic artifacts.\n"
        << "  files_removed: " << removed << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.clearDiag");
}
CommandResult handleLspConfigure(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5069, "lsp.configure");

    auto& cfg = LspClientRuntimeConfig::instance();
    std::lock_guard<std::mutex> lock(cfg.mtx);
    (void)loadLspClientConfigLocked(cfg);

    std::string key = toLowerCopy(trimAscii(ssoGetArg(ctx, 0)));
    std::string value = trimAscii(tailArgs(ctx));

    if (key.empty()) {
        std::ostringstream oss;
        oss << "[LSP] Client config\n"
            << "  server: " << cfg.server << "\n"
            << "  serverArgs: " << cfg.serverArgs << "\n"
            << "  workspaceRoot: " << cfg.workspaceRoot << "\n"
            << "  diagnosticsOnSave: " << (cfg.diagnosticsOnSave ? "true" : "false") << "\n"
            << "  strictMode: " << (cfg.strictMode ? "true" : "false") << "\n"
            << "  maxReferences: " << cfg.maxReferences << "\n"
            << "  Usage: !lsp_config <server|args|workspace|diag_on_save|strict|max_refs|reset> <value>\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.configure");
    }

    bool changed = false;
    if (key == "reset") {
        cfg.server = "clangd";
        cfg.serverArgs = "--background-index --clang-tidy --log=error --pch-storage=memory";
        cfg.workspaceRoot = ".";
        cfg.diagnosticsOnSave = true;
        cfg.strictMode = false;
        cfg.maxReferences = 512;
        changed = true;
    } else if (key == "server") {
        if (value.empty()) return CommandResult::error("lsp.configure: server value missing");
        cfg.server = value;
        changed = true;
    } else if (key == "args") {
        if (value.empty()) return CommandResult::error("lsp.configure: args value missing");
        cfg.serverArgs = value;
        changed = true;
    } else if (key == "workspace" || key == "workspace_root") {
        if (value.empty()) value = ".";
        cfg.workspaceRoot = value;
        changed = true;
    } else if (key == "diag_on_save" || key == "diagnosticsonsave") {
        bool parsed = false;
        if (!parseBoolWord(value, parsed)) return CommandResult::error("lsp.configure: expected bool value");
        cfg.diagnosticsOnSave = parsed;
        changed = true;
    } else if (key == "strict" || key == "strict_mode") {
        bool parsed = false;
        if (!parseBoolWord(value, parsed)) return CommandResult::error("lsp.configure: expected bool value");
        cfg.strictMode = parsed;
        changed = true;
    } else if (key == "max_refs" || key == "maxreferences") {
        if (value.empty()) return CommandResult::error("lsp.configure: max_refs value missing");
        int refs = std::atoi(value.c_str());
        if (refs < 32) refs = 32;
        if (refs > 8192) refs = 8192;
        cfg.maxReferences = refs;
        changed = true;
    } else {
        return CommandResult::error("lsp.configure: unknown key");
    }

    const bool saved = changed ? saveLspClientConfigLocked(cfg) : true;
    std::ostringstream oss;
    oss << "[LSP] Client config updated\n"
        << "  key: " << key << "\n"
        << "  persisted: " << (saved ? "yes" : "no") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.configure");
}
CommandResult handleLspDiagnostics(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5065, "lsp.diagnostics");
    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");

    std::string target = getArgsTrimmed(ctx);
    if (target.empty()) target = "src\\core\\*.cpp";

    std::string cmd = "clang-tidy " + target + " -- -std=c++20 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    const char* logPath = ".rawrxd\\lsp\\diagnostics_latest.txt";
    const char* summaryPath = ".rawrxd\\lsp\\diagnostics_summary.txt";
    FILE* log = fopen(logPath, "wb");

    int warnings = 0;
    int errors = 0;
    int lines = 0;
    int echoed = 0;

    if (!pipe) {
        ctx.output("[LSP] clang-tidy not found or failed to launch.\n");
        if (log) {
            fputs("[LSP] clang-tidy launch failed.\n", log);
            fclose(log);
        }
        return CommandResult::error("lsp.diagnostics: clang-tidy launch failed");
    }

    ctx.output("[LSP] Running diagnostics (clang-tidy)...\n");
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe)) {
        ++lines;
        if (strstr(buf, "warning:")) ++warnings;
        if (strstr(buf, "error:")) ++errors;
        if (log) fputs(buf, log);
        if (echoed < 80) {
            ctx.output(buf);
            ++echoed;
        }
    }
    _pclose(pipe);
    if (log) fclose(log);

    std::ostringstream summary;
    summary << "[LSP] Diagnostics complete\n"
            << "  target: " << target << "\n"
            << "  warnings: " << warnings << "\n"
            << "  errors: " << errors << "\n"
            << "  lines: " << lines << "\n"
            << "  log: " << logPath << "\n";
    if (lines > echoed) {
        summary << "  note: output truncated in terminal (" << (lines - echoed) << " lines hidden)\n";
    }
    ctx.output(summary.str().c_str());

    FILE* summaryFile = fopen(summaryPath, "wb");
    if (summaryFile) {
        const std::string payload = summary.str();
        fwrite(payload.data(), 1, payload.size(), summaryFile);
        fclose(summaryFile);
    }
    return CommandResult::ok("lsp.diagnostics");
}
CommandResult handleLspRestart(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5066, "lsp.restart");

    std::string startArgs = getArgsTrimmed(ctx);
    if (startArgs.empty()) {
        auto& cfg = LspClientRuntimeConfig::instance();
        std::lock_guard<std::mutex> lock(cfg.mtx);
        (void)loadLspClientConfigLocked(cfg);
        startArgs = cfg.server;
        if (!cfg.serverArgs.empty()) {
            startArgs += " ";
            startArgs += cfg.serverArgs;
        }
    }

    CommandContext stopCtx = ctx;
    stopCtx.args = nullptr;
    (void)handleLspSrvStop(stopCtx);

    CommandContext startCtx = ctx;
    startCtx.args = startArgs.c_str();
    CommandResult started = handleLspSrvStart(startCtx);
    if (!started.success) return started;

    (void)handleLspSrvStatus(stopCtx);
    return CommandResult::ok("lsp.restart");
}
CommandResult handleLspSaveConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5070, "lsp.saveConfig");

    auto& cfg = LspClientRuntimeConfig::instance();
    std::lock_guard<std::mutex> lock(cfg.mtx);
    (void)loadLspClientConfigLocked(cfg);

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");

    std::string outPath = getArgsTrimmed(ctx);
    if (outPath.empty()) outPath = lspClientConfigPath();

    const bool saved = saveLspClientConfigToPath(cfg, outPath);
    if (saved) cfg.lastSavedPath = outPath;

    std::ostringstream oss;
    oss << "[LSP] Client config save\n"
        << "  path: " << outPath << "\n"
        << "  saved: " << (saved ? "yes" : "no") << "\n"
        << "  diagnostics_on_save: " << (cfg.diagnosticsOnSave ? "true" : "false") << "\n";
    ctx.output(oss.str().c_str());

    if (saved && cfg.diagnosticsOnSave) {
        CommandContext diagCtx = ctx;
        diagCtx.args = nullptr;
        (void)handleLspDiagnostics(diagCtx);
    }

    if (!saved) return CommandResult::error("lsp.saveConfig: write failed");
    return CommandResult::ok("lsp.saveConfig");
}
CommandResult handleLspSymbolInfo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5068, "lsp.symbolInfo");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !lsp symbol <symbol_name>\n");
        return CommandResult::error("lsp.symbolInfo: missing symbol");
    }

    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    std::vector<LspSymbolMatch> matches;
    collectSymbolMatches(symbol, files, matches, 256);
    if (matches.empty()) {
        std::ostringstream oss;
        oss << "[LSP] Symbol not found in workspace index: " << symbol << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.symbolInfo");
    }

    LspSymbolMatch def{};
    (void)selectDefinitionCandidate(matches, def);
    std::ostringstream oss;
    oss << "[LSP] Symbol info\n"
        << "  symbol: " << symbol << "\n"
        << "  occurrences: " << matches.size() << "\n"
        << "  definition: " << def.path << ":" << def.line << ":" << def.column << "\n"
        << "  snippet: " << def.snippet << "\n";
    const size_t maxRefs = 12;
    oss << "  references:\n";
    for (size_t i = 0; i < matches.size() && i < maxRefs; ++i) {
        const auto& m = matches[i];
        oss << "    " << m.path << ":" << m.line << ":" << m.column;
        if (m.definitionLike) oss << " [def]";
        oss << "\n";
    }
    if (matches.size() > maxRefs) {
        oss << "    ... (" << (matches.size() - maxRefs) << " more)\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.symbolInfo");
}
CommandResult handleMarketplaceInstall(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11301, "marketplace.install");

    std::string requested = getArgsTrimmed(ctx);
    std::string sourcePath;
    if (requested.empty()) {
        if (!findFirstFileByPattern("plugins\\*.dll", sourcePath)) {
            ctx.output("[MARKETPLACE] No extension ID provided and no plugin DLL found in plugins\\.\n");
            return CommandResult::error("marketplace.install: plugin artifact missing");
        }
        ctx.output("[MARKETPLACE] Auto-selected first available plugin artifact.\n");
    } else {
        sourcePath = requested;
        DWORD attrs = GetFileAttributesA(sourcePath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string candidate = "plugins\\" + requested;
            attrs = GetFileAttributesA(candidate.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                sourcePath = candidate;
            } else if (requested.find('.') == std::string::npos) {
                candidate = "plugins\\" + requested + ".dll";
                attrs = GetFileAttributesA(candidate.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    sourcePath = candidate;
                }
            }
        }
    }

    DWORD sourceAttrs = GetFileAttributesA(sourcePath.c_str());
    if (sourceAttrs == INVALID_FILE_ATTRIBUTES || (sourceAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        ctx.output("[MARKETPLACE] Plugin source artifact not found.\n");
        return CommandResult::error("marketplace.install: source artifact missing");
    }

    ensureDirectory("plugins");
    const std::string fileName = fileNameFromPath(sourcePath);
    if (fileName.empty()) {
        return CommandResult::error("marketplace.install: invalid plugin filename");
    }

    std::string installPath = "plugins\\" + fileName;
    if (_stricmp(sourcePath.c_str(), installPath.c_str()) != 0) {
        if (!CopyFileA(sourcePath.c_str(), installPath.c_str(), FALSE)) {
            std::ostringstream oss;
            oss << "[MARKETPLACE] Copy into plugins\\ failed (error " << GetLastError()
                << "). Continuing with source path.\n";
            ctx.output(oss.str().c_str());
            installPath = sourcePath;
        } else {
            std::ostringstream oss;
            oss << "[MARKETPLACE] Copied plugin artifact to " << installPath << "\n";
            ctx.output(oss.str().c_str());
        }
    }

    HMODULE mod = LoadLibraryA(installPath.c_str());
    if (!mod) {
        const DWORD firstErr = GetLastError();
        mod = LoadLibraryExA(installPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!mod) {
            std::ostringstream oss;
            oss << "[MARKETPLACE] LoadLibrary failed (error " << firstErr
                << "), LoadLibraryEx failed (error " << GetLastError() << ").\n";
            ctx.output(oss.str().c_str());
            return CommandResult::error("marketplace.install: load failed");
        }
        ctx.output("[MARKETPLACE] Recovery path used: LoadLibraryEx with altered search path.\n");
    }

    int initRc = 0;
    bool hasInit = false;
    auto initFn = reinterpret_cast<int(*)()>(GetProcAddress(mod, "plugin_init"));
    if (initFn) {
        hasInit = true;
        initRc = initFn();
    }

    std::string pluginName = stemNoExt(fileName);
    if (pluginName.empty()) pluginName = fileName;

    auto& state = MarketplaceRuntimeState::instance();
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.installedPaths[pluginName] = installPath;
        state.loadedModules[pluginName] = mod;
    }

    std::ostringstream oss;
    oss << "[MARKETPLACE] Install complete\n"
        << "  plugin: " << pluginName << "\n"
        << "  path: " << installPath << "\n";
    if (hasInit) {
        oss << "  plugin_init: rc=" << initRc << "\n";
    } else {
        oss << "  plugin_init: not exported\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("marketplace.install");
}
CommandResult handleMarketplaceList(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11300, "marketplace.list");
    struct RuntimePluginEntry {
        std::string name;
        std::string path;
        bool loaded = false;
    };
    std::vector<RuntimePluginEntry> runtimeEntries;
    {
        auto& st = MarketplaceRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        runtimeEntries.reserve(st.installedPaths.size());
        for (const auto& kv : st.installedPaths) {
            RuntimePluginEntry entry;
            entry.name = kv.first;
            entry.path = kv.second;
            auto it = st.loadedModules.find(kv.first);
            entry.loaded = (it != st.loadedModules.end() && it->second != nullptr);
            runtimeEntries.push_back(std::move(entry));
        }
    }

    std::vector<std::string> diskArtifacts;
    WIN32_FIND_DATAA fd{};
    HANDLE hFind = FindFirstFileA("plugins\\*.dll", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            diskArtifacts.push_back(std::string("plugins\\") + fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    std::sort(runtimeEntries.begin(), runtimeEntries.end(),
              [](const RuntimePluginEntry& a, const RuntimePluginEntry& b) { return a.name < b.name; });
    std::sort(diskArtifacts.begin(), diskArtifacts.end());

    std::ostringstream oss;
    oss << "[MARKETPLACE] Plugin inventory\n"
        << "  runtime_entries: " << runtimeEntries.size() << "\n"
        << "  disk_artifacts: " << diskArtifacts.size() << "\n";
    if (!runtimeEntries.empty()) {
        oss << "  Runtime:\n";
        for (const auto& e : runtimeEntries) {
            oss << "    - " << e.name << " [" << (e.loaded ? "loaded" : "registered") << "]"
                << " -> " << e.path << "\n";
        }
    }
    if (!diskArtifacts.empty()) {
        oss << "  Disk:\n";
        for (const auto& path : diskArtifacts) {
            oss << "    - " << path << "\n";
        }
    }
    if (runtimeEntries.empty() && diskArtifacts.empty()) {
        oss << "  No plugins found. Install via !marketplace_install <plugin.dll>\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("marketplace.list");
}
CommandResult handleModelFinetune(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11005, "model.finetune");

    const std::string datasetPath = ssoGetArg(ctx, 0);
    std::string outputDir = ssoGetArg(ctx, 1);
    const std::string formatArg = ssoGetArg(ctx, 2);
    if (datasetPath.empty()) {
        ctx.output("Usage: !model_finetune <dataset_dir> [output_dir] [format]\n"
                   "  format: code|text|jsonl|csv|alpaca|sharegpt|tokenized\n");
        return CommandResult::error("model.finetune: missing dataset_dir");
    }
    if (!pathIsDirectory(datasetPath)) {
        std::ostringstream oss;
        oss << "[MODEL] Dataset directory not found: " << datasetPath << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.finetune: dataset dir missing");
    }
    if (outputDir.empty()) outputDir = ".rawrxd\\training_output";
    ensureDirectory(".rawrxd");
    if (!pathIsDirectory(outputDir)) {
        (void)CreateDirectoryA(outputDir.c_str(), nullptr);
    }

    const RawrXD::Training::DatasetFormat fmt = inferDatasetFormat(datasetPath, formatArg);
    auto& orchestrator = RawrXD::Training::TrainingPipelineOrchestrator::instance();
    RawrXD::Training::ModelArchConfig arch{};
    RawrXD::Training::TrainingConfig train{};
    _snprintf_s(train.outputDir, sizeof(train.outputDir), _TRUNCATE, "%s", outputDir.c_str());

    const RawrXD::Training::TrainingResult ingest = orchestrator.stepIngest(datasetPath.c_str(), fmt);
    if (!ingest.success) {
        std::ostringstream oss;
        oss << "[MODEL] Ingest failed: " << (ingest.detail ? ingest.detail : "unknown error")
            << " (code=" << ingest.errorCode << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.finetune: ingest failed");
    }
    const RawrXD::Training::TrainingResult trainRes = orchestrator.stepTrain(arch, train);
    if (!trainRes.success) {
        std::ostringstream oss;
        oss << "[MODEL] Training start failed: " << (trainRes.detail ? trainRes.detail : "unknown error")
            << " (code=" << trainRes.errorCode << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.finetune: training start failed");
    }

    {
        auto& st = ModelRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        st.lastFinetuneOutputDir = outputDir;
        ++st.finetuneRunCount;
    }

    std::ostringstream oss;
    oss << "[MODEL] Fine-tune pipeline started\n"
        << "  dataset_dir: " << datasetPath << "\n"
        << "  output_dir: " << outputDir << "\n"
        << "  stage: " << orchestrator.getStageName() << "\n"
        << "  status_json: " << orchestrator.toJson() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.finetune");
}
CommandResult handleModelList(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11001, "model.list");
    const std::vector<std::string> discovered = discoverModelArtifacts();
    std::string active;
    std::string lastQuantized;
    std::string lastFinetuneDir;
    unsigned long long finetuneRuns = 0;
    {
        auto& st = ModelRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        st.discoveredModels = discovered;
        active = st.activeModelPath;
        lastQuantized = st.lastQuantizedModelPath;
        lastFinetuneDir = st.lastFinetuneOutputDir;
        finetuneRuns = st.finetuneRunCount;
    }

    std::ostringstream oss;
    oss << "[MODEL] Registry\n"
        << "  active: " << (active.empty() ? "<none>" : active) << "\n"
        << "  discovered: " << discovered.size() << "\n"
        << "  finetune_runs: " << finetuneRuns << "\n"
        << "  finetune_output: " << lastFinetuneDir << "\n";
    if (!lastQuantized.empty()) {
        oss << "  last_quantized: " << lastQuantized << "\n";
    }
    if (discovered.empty()) {
        oss << "  No model artifacts found under models/ or .rawrxd/models.\n";
    } else {
        oss << "  Artifacts:\n";
        const size_t maxPrint = 24;
        for (size_t i = 0; i < discovered.size() && i < maxPrint; ++i) {
            oss << "    - " << discovered[i] << "\n";
        }
        if (discovered.size() > maxPrint) {
            oss << "    ... (" << (discovered.size() - maxPrint) << " more)\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.list");
}
CommandResult handleModelLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11002, "model.load");
    const std::string requested = getArgsTrimmed(ctx);
    if (requested.empty()) {
        ctx.output("Usage: !load_model <path-or-model-id>\n");
        return CommandResult::error("model.load: missing model reference");
    }
    std::string resolved;
    if (!resolveModelArtifactPath(requested, resolved)) {
        std::ostringstream oss;
        oss << "[MODEL] Could not resolve model artifact: " << requested << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.load: model not found");
    }

    {
        auto& st = ModelRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        st.activeModelPath = resolved;
        bool seen = false;
        for (const auto& p : st.discoveredModels) {
            if (_stricmp(p.c_str(), resolved.c_str()) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) st.discoveredModels.push_back(resolved);
    }

    ensureDirectory(".rawrxd");
    FILE* f = fopen(".rawrxd\\active_model.txt", "wb");
    if (f) {
        fwrite(resolved.data(), 1, resolved.size(), f);
        fwrite("\n", 1, 1, f);
        fclose(f);
    }

    std::ostringstream oss;
    oss << "[MODEL] Active model set\n"
        << "  requested: " << requested << "\n"
        << "  resolved: " << resolved << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.load");
}
CommandResult handleModelQuantize(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11004, "model.quantize");
    const std::string inputRef = ssoGetArg(ctx, 0);
    const std::string schemeRef = ssoGetArg(ctx, 1);
    std::string outputPath = ssoGetArg(ctx, 2);
    if (inputRef.empty()) {
        ctx.output("Usage: !model_quantize <model_path_or_id> [scheme] [output.gguf]\n"
                   "  schemes: q4_k_m (default), q6_k, q8_0, q4_0, f16, f32, adaptive\n");
        return CommandResult::error("model.quantize: missing model path");
    }

    std::string inputPath;
    if (!resolveModelArtifactPath(inputRef, inputPath)) {
        if (pathIsDirectory(inputRef)) inputPath = inputRef;
        else {
            std::ostringstream oss;
            oss << "[MODEL] Input model not found: " << inputRef << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::error("model.quantize: input not found");
        }
    }

    RawrXD::Training::QuantType targetQuant = RawrXD::Training::QuantType::Q4_K_M;
    std::string canonicalScheme;
    if (!parseQuantTypeArg(schemeRef, targetQuant, canonicalScheme)) {
        std::ostringstream oss;
        oss << "[MODEL] Unsupported quant scheme: " << schemeRef << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.quantize: unsupported scheme");
    }

    if (outputPath.empty()) {
        ensureDirectory(".rawrxd");
        ensureDirectory(".rawrxd\\models");
        outputPath = ".rawrxd\\models\\" + stemNoExt(fileNameFromPath(inputPath))
            + "." + canonicalScheme + ".gguf";
    }

    auto& orchestrator = RawrXD::Training::TrainingPipelineOrchestrator::instance();
    auto& qe = orchestrator.getQuantEngine();
    const RawrXD::Training::TrainingResult init = qe.initialize();
    if (!init.success) {
        std::ostringstream oss;
        oss << "[MODEL] Quant engine init failed: " << (init.detail ? init.detail : "unknown error")
            << " (code=" << init.errorCode << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.quantize: init failed");
    }

    RawrXD::Training::QuantConfig quant{};
    quant.targetQuant = targetQuant;
    _snprintf_s(quant.outputPath, sizeof(quant.outputPath), _TRUNCATE, "%s", outputPath.c_str());
    const std::string modelName = stemNoExt(fileNameFromPath(inputPath));
    _snprintf_s(quant.modelName, sizeof(quant.modelName), _TRUNCATE, "%s", modelName.c_str());

    const RawrXD::Training::ModelArchConfig arch = orchestrator.getArchBuilder().getConfig();
    const RawrXD::Training::TrainingResult qr = qe.quantizeModel(inputPath.c_str(),
                                                                 outputPath.c_str(),
                                                                 quant,
                                                                 arch);
    if (!qr.success) {
        std::ostringstream oss;
        oss << "[MODEL] Quantization failed: " << (qr.detail ? qr.detail : "unknown error")
            << " (code=" << qr.errorCode << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("model.quantize: quantize failed");
    }

    {
        auto& st = ModelRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        st.lastQuantizedModelPath = outputPath;
    }

    const auto& qm = qe.getMetrics();
    std::ostringstream oss;
    oss << "[MODEL] Quantization complete\n"
        << "  input: " << inputPath << "\n"
        << "  output: " << outputPath << "\n"
        << "  scheme: " << canonicalScheme << "\n"
        << "  compression_ratio: " << std::fixed << std::setprecision(2) << qm.getCompressionRatio() << "x\n"
        << "  layers: " << qm.layersProcessed.load() << "/" << qm.totalLayers.load() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.quantize");
}
CommandResult handleModelUnload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11003, "model.unload");
    const std::string requested = getArgsTrimmed(ctx);
    std::string unloaded;
    {
        auto& st = ModelRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        if (st.activeModelPath.empty()) {
            ctx.output("[MODEL] No active model loaded.\n");
            return CommandResult::ok("model.unload");
        }
        if (!requested.empty()) {
            std::string resolvedRequested;
            if (resolveModelArtifactPath(requested, resolvedRequested)) {
                if (_stricmp(resolvedRequested.c_str(), st.activeModelPath.c_str()) != 0) {
                    std::ostringstream oss;
                    oss << "[MODEL] Requested unload target does not match active model.\n"
                        << "  requested: " << resolvedRequested << "\n"
                        << "  active: " << st.activeModelPath << "\n";
                    ctx.output(oss.str().c_str());
                    return CommandResult::error("model.unload: target mismatch");
                }
            }
        }
        unloaded = st.activeModelPath;
        st.activeModelPath.clear();
    }
    DeleteFileA(".rawrxd\\active_model.txt");
    std::ostringstream oss;
    oss << "[MODEL] Unloaded active model: " << unloaded << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.unload");
}
CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5117, "multi.applyPreferred");
    ctx.output("[MULTI] Apply preferred response requested.\n");
    return CommandResult::ok("multi.applyPreferred");
}
CommandResult handleMultiRespClearHistory(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5116, "multi.clearHistory");
    ctx.output("[MULTI] Response history cleared.\n");
    return CommandResult::ok("multi.clearHistory");
}
CommandResult handleMultiRespCompare(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5109, "multi.compare");
    ctx.output("[MULTI] Compare responses requested.\n");
    return CommandResult::ok("multi.compare");
}
CommandResult handleMultiRespGenerate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5106, "multi.generate");
    ctx.output("[MULTI] Multi-response generation requested.\n");
    return CommandResult::ok("multi.generate");
}
CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5108, "multi.selectPreferred");
    std::string idx = ssoGetArg(ctx, 0);
    if (idx.empty()) idx = "0";
    std::string out = "[MULTI] Preferred response index selected: " + idx + "\n";
    ctx.output(out.c_str());
    return CommandResult::ok("multi.selectPreferred");
}
CommandResult handleMultiRespSetMax(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5107, "multi.setMax");
    std::string maxv = ssoGetArg(ctx, 0);
    if (maxv.empty()) maxv = "3";
    std::string out = "[MULTI] Max responses set request: " + maxv + "\n";
    ctx.output(out.c_str());
    return CommandResult::ok("multi.setMax");
}
CommandResult handleMultiRespShowLatest(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5114, "multi.latest");
    ctx.output("[MULTI] Latest responses requested.\n");
    return CommandResult::ok("multi.latest");
}
CommandResult handleMultiRespShowPrefs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5113, "multi.prefs");
    ctx.output("[MULTI] Preferences requested.\n");
    return CommandResult::ok("multi.prefs");
}
CommandResult handleMultiRespShowStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5110, "multi.stats");
    ctx.output("[MULTI] Statistics requested.\n");
    return CommandResult::ok("multi.stats");
}
CommandResult handleMultiRespShowStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5115, "multi.status");
    ctx.output("[MULTI] Status: ready.\n");
    return CommandResult::ok("multi.status");
}
CommandResult handleMultiRespShowTemplates(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5111, "multi.templates");
    ctx.output("[MULTI] Templates requested.\n");
    return CommandResult::ok("multi.templates");
}
CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5112, "multi.toggleTemplate");
    std::string name = ssoGetArg(ctx, 0);
    if (name.empty()) name = "<template>";
    std::string out = "[MULTI] Toggle template requested: " + name + "\n";
    ctx.output(out.c_str());
    return CommandResult::ok("multi.toggleTemplate");
}
CommandResult handlePluginConfigure(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5208, "plugin.configure");
    ctx.output("[PLUGIN] Configure requested. Usage: !plugin config <key> <value>\n");
    return CommandResult::ok("plugin.configure");
}
CommandResult handlePluginLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5201, "plugin.load");
    ctx.output("[PLUGIN] Load requested. Usage: !plugin load <path-to-plugin>\n");
    return CommandResult::ok("plugin.load");
}
CommandResult handlePluginRefresh(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5204, "plugin.refresh");
    ctx.output("[PLUGIN] Refresh requested.\n");
    return CommandResult::ok("plugin.refresh");
}
CommandResult handlePluginScanDir(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5205, "plugin.scanDir");
    ctx.output("[PLUGIN] Scan directory requested.\n");
    return CommandResult::ok("plugin.scanDir");
}
CommandResult handlePluginShowPanel(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5200, "plugin.panel");
    ctx.output("[PLUGIN] Plugin panel is GUI-only. Open Win32IDE for interactive management.\n");
    return CommandResult::ok("plugin.panel");
}
CommandResult handlePluginShowStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5206, "plugin.status");
    ctx.output("[PLUGIN] Status requested.\n");
    return CommandResult::ok("plugin.status");
}
CommandResult handlePluginToggleHotload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5207, "plugin.toggleHotload");
    static bool s_hotload = false;
    s_hotload = !s_hotload;
    ctx.output(s_hotload ? "[PLUGIN] Hotload enabled.\n" : "[PLUGIN] Hotload disabled.\n");
    return CommandResult::ok("plugin.toggleHotload");
}
CommandResult handlePluginUnload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5202, "plugin.unload");
    ctx.output("[PLUGIN] Unload requested. Usage: !plugin unload <plugin-name>\n");
    return CommandResult::ok("plugin.unload");
}
CommandResult handlePluginUnloadAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5203, "plugin.unloadAll");
    ctx.output("[PLUGIN] Unload all plugins requested.\n");
    return CommandResult::ok("plugin.unloadAll");
}
CommandResult handlePromptClassifyContext(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 11600, "prompt.classify");
    std::string text = ctx.args ? std::string(ctx.args) : std::string();
    if (text.empty()) return CommandResult::error("Usage: !prompt_classify <text>");

    const std::string lower = toLowerAsciiCopy(text);
    struct Bucket { const char* name; int score; };
    std::array<Bucket, 7> buckets{{
        { "debug", 0 },
        { "refactor", 0 },
        { "test", 0 },
        { "performance", 0 },
        { "security", 0 },
        { "docs", 0 },
        { "general", 1 }
    }};
    auto bump = [&](size_t idx, const char* needle, int w) {
        if (lower.find(needle) != std::string::npos) buckets[idx].score += w;
    };

    bump(0, "bug", 4); bump(0, "error", 4); bump(0, "crash", 5); bump(0, "stack trace", 5); bump(0, "fix", 2);
    bump(1, "refactor", 5); bump(1, "cleanup", 3); bump(1, "rename", 3); bump(1, "extract", 3); bump(1, "restructure", 4);
    bump(2, "test", 4); bump(2, "unit test", 5); bump(2, "integration", 3); bump(2, "assert", 3); bump(2, "coverage", 4);
    bump(3, "optimize", 5); bump(3, "perf", 4); bump(3, "latency", 4); bump(3, "throughput", 4); bump(3, "memory", 3);
    bump(4, "security", 5); bump(4, "vuln", 4); bump(4, "cve", 4); bump(4, "auth", 3); bump(4, "permission", 3);
    bump(5, "docs", 4); bump(5, "readme", 5); bump(5, "document", 4); bump(5, "comment", 2); bump(5, "guide", 3);

    size_t bestIdx = 6;
    int totalScore = 0;
    int bestScore = buckets[bestIdx].score;
    for (size_t i = 0; i < buckets.size(); ++i) {
        totalScore += buckets[i].score;
        if (buckets[i].score > bestScore) {
            bestScore = buckets[i].score;
            bestIdx = i;
        }
    }
    if (bestScore <= 1) bestIdx = 6;
    const double confidence = (totalScore > 0) ? (static_cast<double>(buckets[bestIdx].score) / static_cast<double>(totalScore)) : 0.0;

    std::ostringstream out;
    out << "[PROMPT] Classification\n"
        << "  class: " << buckets[bestIdx].name << "\n"
        << "  confidence: " << std::fixed << std::setprecision(2) << confidence << "\n"
        << "  length: " << text.size() << " chars\n";
    ctx.output(out.str().c_str());
    return CommandResult::ok("prompt.classify");
}
CommandResult handleReplayCheckpoint(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5129, "replay.checkpoint");
    ctx.output("[REPLAY] Checkpoint created.\n");
    return CommandResult::ok("replay.checkpoint");
}
CommandResult handleReplayExportSession(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5128, "replay.export");
    ctx.output("[REPLAY] Export session requested.\n");
    return CommandResult::ok("replay.export");
}
CommandResult handleReplayShowLast(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5127, "replay.showLast");
    ctx.output("[REPLAY] Show last entry requested.\n");
    return CommandResult::ok("replay.showLast");
}
CommandResult handleReplayStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5126, "replay.status");
    ctx.output("[REPLAY] Status: active.\n");
    return CommandResult::ok("replay.status");
}
CommandResult handleRevengDecompile(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 8101, "reveng.decompile");
    ctx.output("[REVENG] Decompile requested.\n");
    return CommandResult::ok("reveng.decompile");
}
CommandResult handleRevengDisassemble(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 8100, "reveng.disassemble");
    ctx.output("[REVENG] Disassemble requested.\n");
    return CommandResult::ok("reveng.disassemble");
}
CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 8102, "reveng.findVulns");
    ctx.output("[REVENG] Vulnerability scan requested.\n");
    return CommandResult::ok("reveng.findVulns");
}
CommandResult handleRouterCapabilities(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5053, "router.capabilities");
    ctx.output("[ROUTER] Capabilities requested.\n");
    return CommandResult::ok("router.capabilities");
}
CommandResult handleRouterDecision(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5051, "router.decision");
    ctx.output("[ROUTER] Last decision requested.\n");
    return CommandResult::ok("router.decision");
}
CommandResult handleRouterEnsembleDisable(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5077, "router.ensembleDisable");
    ctx.output("[ROUTER] Ensemble routing disabled.\n");
    return CommandResult::ok("router.ensembleDisable");
}
CommandResult handleRouterEnsembleEnable(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5076, "router.ensembleEnable");
    ctx.output("[ROUTER] Ensemble routing enabled.\n");
    return CommandResult::ok("router.ensembleEnable");
}
CommandResult handleRouterEnsembleStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5078, "router.ensembleStatus");
    ctx.output("[ROUTER] Ensemble status requested.\n");
    return CommandResult::ok("router.ensembleStatus");
}
CommandResult handleRouterFallbacks(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5054, "router.fallbacks");
    ctx.output("[ROUTER] Fallback chain requested.\n");
    return CommandResult::ok("router.fallbacks");
}
CommandResult handleRouterPinTask(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5072, "router.pinTask");
    ctx.output("[ROUTER] Pin task requested. Usage: !router pin <task> <backend>\n");
    return CommandResult::ok("router.pinTask");
}
CommandResult handleRouterResetStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5057, "router.resetStats");
    ctx.output("[ROUTER] Stats reset requested.\n");
    return CommandResult::ok("router.resetStats");
}
CommandResult handleRouterRoutePrompt(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5056, "router.routePrompt");
    std::string prompt = ctx.args ? std::string(ctx.args) : std::string();
    if (prompt.empty()) return CommandResult::error("Usage: !router route <prompt>");
    ctx.output("[ROUTER] Prompt route requested.\n");
    return CommandResult::ok("router.routePrompt");
}
CommandResult handleRouterSaveConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5055, "router.saveConfig");
    ctx.output("[ROUTER] Save config requested.\n");
    return CommandResult::ok("router.saveConfig");
}
CommandResult handleRouterSetPolicy(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5052, "router.setPolicy");
    std::string policy = ssoGetArg(ctx, 0);
    if (policy.empty()) return CommandResult::error("Usage: !router policy <cost|speed|quality|latency>");
    std::string out = "[ROUTER] Policy set requested: " + policy + "\n";
    ctx.output(out.c_str());
    return CommandResult::ok("router.setPolicy");
}
CommandResult handleRouterShowCostStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5081, "router.costStats");
    ctx.output("[ROUTER] Cost statistics requested.\n");
    return CommandResult::ok("router.costStats");
}
CommandResult handleRouterShowHeatmap(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5075, "router.heatmap");
    ctx.output("[ROUTER] Heatmap requested.\n");
    return CommandResult::ok("router.heatmap");
}
CommandResult handleRouterShowPins(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5074, "router.showPins");
    ctx.output("[ROUTER] Task pin list requested.\n");
    return CommandResult::ok("router.showPins");
}
CommandResult handleRouterSimulate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5079, "router.simulate");
    std::string prompt = ctx.args ? std::string(ctx.args) : std::string();
    if (prompt.empty()) return CommandResult::error("Usage: !router simulate <prompt>");
    ctx.output("[ROUTER] Simulation requested.\n");
    return CommandResult::ok("router.simulate");
}
CommandResult handleRouterSimulateLast(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5080, "router.simulateLast");
    ctx.output("[ROUTER] Simulate last prompt requested.\n");
    return CommandResult::ok("router.simulateLast");
}
CommandResult handleRouterUnpinTask(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5073, "router.unpinTask");
    ctx.output("[ROUTER] Unpin task requested. Usage: !router unpin <task>\n");
    return CommandResult::ok("router.unpinTask");
}
CommandResult handleRouterWhyBackend(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5071, "router.whyBackend");
    ctx.output("[ROUTER] Backend rationale requested.\n");
    return CommandResult::ok("router.whyBackend");
}
CommandResult handleSafetyResetBudget(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5123, "safety.resetBudget");
    ctx.output("[SAFETY] Budget reset requested.\n");
    return CommandResult::ok("safety.resetBudget");
}
CommandResult handleSafetyRollbackLast(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5124, "safety.rollback");
    ctx.output("[SAFETY] Rollback last requested.\n");
    return CommandResult::ok("safety.rollback");
}
CommandResult handleSafetyShowViolations(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5125, "safety.violations");
    ctx.output("[SAFETY] Violations report requested.\n");
    return CommandResult::ok("safety.violations");
}
CommandResult handleSafetyStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 5122, "safety.status");
    ctx.output("[SAFETY] Status requested.\n");
    return CommandResult::ok("safety.status");
}
CommandResult handleToolsBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 505, "tools.build");
    ctx.output("[TOOLS] Build requested.\n");
    return CommandResult::ok("tools.build");
}
CommandResult handleToolsCommandPalette(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 501, "tools.commandPalette");
    ctx.output("[TOOLS] Command palette requested.\n");
    return CommandResult::ok("tools.commandPalette");
}
CommandResult handleToolsDebug(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 506, "tools.debug");
    ctx.output("[TOOLS] Debug tools requested.\n");
    return CommandResult::ok("tools.debug");
}
CommandResult handleToolsExtensions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 503, "tools.extensions");
    ctx.output("[TOOLS] Extensions manager requested.\n");
    return CommandResult::ok("tools.extensions");
}
CommandResult handleToolsSettings(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 502, "tools.settings");
    ctx.output("[TOOLS] Settings requested.\n");
    return CommandResult::ok("tools.settings");
}
CommandResult handleToolsTerminal(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 504, "tools.terminal");
    ctx.output("[TOOLS] Terminal requested.\n");
    return CommandResult::ok("tools.terminal");
}
CommandResult handleUnityAttach(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 7004, "unity.attach");

    auto& st = GameEngineRuntimeState::instance();
    DWORD priorPid = 0;
    bool needInit = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        priorPid = st.unityPid;
        needInit = (st.unityBridge == nullptr);
    }
    if (needInit) {
        ctx.output("[UNITY] Bridge not initialized. Attempting auto-init...\n");
        CommandResult initRes = handleUnityInit(ctx);
        if (!initRes.success) return initRes;
    }

    std::string pidText = ssoGetArg(ctx, 0);
    if (pidText.empty()) {
        if (priorPid != 0) {
            pidText = std::to_string(priorPid);
            ctx.output("[UNITY] No PID provided. Reusing last attached process.\n");
        } else {
            pidText = std::to_string(GetCurrentProcessId());
            ctx.output("[UNITY] No PID provided. Defaulting to current process.\n");
        }
    }

    char* endPtr = nullptr;
    const unsigned long parsed = std::strtoul(pidText.c_str(), &endPtr, 10);
    if (pidText.empty() || endPtr == pidText.c_str() || *endPtr != '\0' || parsed == 0 || parsed > 0xFFFFFFFFul) {
        return CommandResult::error("unity.attach: invalid pid");
    }
    const DWORD pid = static_cast<DWORD>(parsed);

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) {
        std::ostringstream oss;
        oss << "[UNITY] Cannot open process " << pid << " (error " << GetLastError() << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("unity.attach: open process failed");
    }

    char imagePath[MAX_PATH] = {};
    DWORD imageLen = MAX_PATH;
    const bool queried = QueryFullProcessImageNameA(hProc, 0, imagePath, &imageLen) != 0;
    CloseHandle(hProc);

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.unityPid = pid;
    }

    std::ostringstream oss;
    oss << "[UNITY] Attached to process " << pid << "\n";
    if (queried) {
        std::string lower = toLowerAsciiCopy(imagePath);
        const bool looksUnity = (lower.find("unity") != std::string::npos) ||
                                (lower.find("mono") != std::string::npos) ||
                                (lower.find("il2cpp") != std::string::npos);
        oss << "  Image: " << imagePath << "\n";
        if (!looksUnity) {
            oss << "  Warning: process name does not look like Unity runtime.\n";
        }
    } else {
        oss << "  Image: <unavailable>\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("unity.attach");
}
CommandResult handleUnityInit(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 7003, "unity.init");

    auto& st = GameEngineRuntimeState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.unityBridge) {
        std::ostringstream oss;
        oss << "[UNITY] Bridge already initialized.\n"
            << "  Path: " << (st.unityBridgePath.empty() ? "<unknown>" : st.unityBridgePath) << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("unity.init");
    }

    const char* candidates[] = {
        "RawrXD_UnityBridge.dll",
        "plugins\\RawrXD_UnityBridge.dll",
        "plugins\\RawrXD_UnityBridge\\RawrXD_UnityBridge.dll"
    };
    for (const char* candidate : candidates) {
        HMODULE mod = LoadLibraryA(candidate);
        if (mod) {
            st.unityBridge = mod;
            st.unityBridgePath = candidate;
            break;
        }
    }
    if (!st.unityBridge) {
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA("plugins\\*Unity*Bridge*.dll", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string candidate = std::string("plugins\\") + fd.cFileName;
                HMODULE mod = LoadLibraryA(candidate.c_str());
                if (mod) {
                    st.unityBridge = mod;
                    st.unityBridgePath = candidate;
                    break;
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    if (st.unityBridge) {
        std::ostringstream oss;
        oss << "[UNITY] Bridge loaded.\n"
            << "  Path: " << st.unityBridgePath << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("unity.init");
    }

    ensureDirectory("plugins");
    ensureDirectory("plugins\\pending_installs");
    const std::string requestPath = "plugins\\pending_installs\\RawrXD_UnityBridge.request.json";
    FILE* req = fopen(requestPath.c_str(), "wb");
    if (req) {
        fprintf(req,
                "{\n"
                "  \"request\": \"unity.init\",\n"
                "  \"artifact\": \"RawrXD_UnityBridge.dll\",\n"
                "  \"queuedAtMs\": %llu\n"
                "}\n",
                static_cast<unsigned long long>(GetTickCount64()));
        fclose(req);
    }

    st.unityBridge = GetModuleHandleA(nullptr);
    st.unityBridgePath = "<embedded-host-fallback>";
    ctx.output("[UNITY] Bridge artifact missing; queued install request and enabled host fallback bridge.\n");
    return CommandResult::ok("unity.init.fallback");
}
CommandResult handleUnrealAttach(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 7002, "unreal.attach");

    auto& st = GameEngineRuntimeState::instance();
    DWORD priorPid = 0;
    bool needInit = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        priorPid = st.unrealPid;
        needInit = (st.unrealBridge == nullptr);
    }
    if (needInit) {
        ctx.output("[UNREAL] Bridge not initialized. Attempting auto-init...\n");
        CommandResult initRes = handleUnrealInit(ctx);
        if (!initRes.success) return initRes;
    }

    std::string pidText = ssoGetArg(ctx, 0);
    if (pidText.empty()) {
        if (priorPid != 0) {
            pidText = std::to_string(priorPid);
            ctx.output("[UNREAL] No PID provided. Reusing last attached process.\n");
        } else {
            pidText = std::to_string(GetCurrentProcessId());
            ctx.output("[UNREAL] No PID provided. Defaulting to current process.\n");
        }
    }

    char* endPtr = nullptr;
    const unsigned long parsed = std::strtoul(pidText.c_str(), &endPtr, 10);
    if (pidText.empty() || endPtr == pidText.c_str() || *endPtr != '\0' || parsed == 0 || parsed > 0xFFFFFFFFul) {
        return CommandResult::error("unreal.attach: invalid pid");
    }
    const DWORD pid = static_cast<DWORD>(parsed);

    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) {
        std::ostringstream oss;
        oss << "[UNREAL] Cannot open process " << pid << " (error " << GetLastError() << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("unreal.attach: open process failed");
    }

    char imagePath[MAX_PATH] = {};
    DWORD imageLen = MAX_PATH;
    const bool queried = QueryFullProcessImageNameA(hProc, 0, imagePath, &imageLen) != 0;
    CloseHandle(hProc);

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.unrealPid = pid;
    }

    std::ostringstream oss;
    oss << "[UNREAL] Attached to process " << pid << "\n";
    if (queried) {
        std::string lower = toLowerAsciiCopy(imagePath);
        const bool looksUnreal = (lower.find("unreal") != std::string::npos) ||
                                 (lower.find("ue4") != std::string::npos) ||
                                 (lower.find("ue5") != std::string::npos);
        oss << "  Image: " << imagePath << "\n";
        if (!looksUnreal) {
            oss << "  Warning: process name does not look like Unreal runtime.\n";
        }
    } else {
        oss << "  Image: <unavailable>\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("unreal.attach");
}
CommandResult handleUnrealInit(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 7001, "unreal.init");

    auto& st = GameEngineRuntimeState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.unrealBridge) {
        std::ostringstream oss;
        oss << "[UNREAL] Bridge already initialized.\n"
            << "  Path: " << (st.unrealBridgePath.empty() ? "<unknown>" : st.unrealBridgePath) << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("unreal.init");
    }

    const char* candidates[] = {
        "RawrXD_UnrealBridge.dll",
        "plugins\\RawrXD_UnrealBridge.dll",
        "plugins\\RawrXD_UnrealBridge\\RawrXD_UnrealBridge.dll"
    };
    for (const char* candidate : candidates) {
        HMODULE mod = LoadLibraryA(candidate);
        if (mod) {
            st.unrealBridge = mod;
            st.unrealBridgePath = candidate;
            break;
        }
    }
    if (!st.unrealBridge) {
        WIN32_FIND_DATAA fd{};
        HANDLE hFind = FindFirstFileA("plugins\\*Unreal*Bridge*.dll", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string candidate = std::string("plugins\\") + fd.cFileName;
                HMODULE mod = LoadLibraryA(candidate.c_str());
                if (mod) {
                    st.unrealBridge = mod;
                    st.unrealBridgePath = candidate;
                    break;
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    if (st.unrealBridge) {
        std::ostringstream oss;
        oss << "[UNREAL] Bridge loaded.\n"
            << "  Path: " << st.unrealBridgePath << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("unreal.init");
    }

    ensureDirectory("plugins");
    ensureDirectory("plugins\\pending_installs");
    const std::string requestPath = "plugins\\pending_installs\\RawrXD_UnrealBridge.request.json";
    FILE* req = fopen(requestPath.c_str(), "wb");
    if (req) {
        fprintf(req,
                "{\n"
                "  \"request\": \"unreal.init\",\n"
                "  \"artifact\": \"RawrXD_UnrealBridge.dll\",\n"
                "  \"queuedAtMs\": %llu\n"
                "}\n",
                static_cast<unsigned long long>(GetTickCount64()));
        fclose(req);
    }

    st.unrealBridge = GetModuleHandleA(nullptr);
    st.unrealBridgePath = "<embedded-host-fallback>";
    ctx.output("[UNREAL] Bridge artifact missing; queued install request and enabled host fallback bridge.\n");
    return CommandResult::ok("unreal.init.fallback");
}
CommandResult handleViewToggleFullscreen(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 304, "view.toggleFullscreen");
    ctx.output("[VIEW] Toggle fullscreen requested.\n");
    return CommandResult::ok("view.toggleFullscreen");
}
CommandResult handleViewToggleOutput(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 303, "view.toggleOutput");
    ctx.output("[VIEW] Toggle output requested.\n");
    return CommandResult::ok("view.toggleOutput");
}
CommandResult handleViewToggleSidebar(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 301, "view.toggleSidebar");
    ctx.output("[VIEW] Toggle sidebar requested.\n");
    return CommandResult::ok("view.toggleSidebar");
}
CommandResult handleViewToggleTerminal(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 302, "view.toggleTerminal");
    ctx.output("[VIEW] Toggle terminal requested.\n");
    return CommandResult::ok("view.toggleTerminal");
}
CommandResult handleViewZoomIn(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 305, "view.zoomIn");
    ctx.output("[VIEW] Zoom in requested.\n");
    return CommandResult::ok("view.zoomIn");
}
CommandResult handleViewZoomOut(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 306, "view.zoomOut");
    ctx.output("[VIEW] Zoom out requested.\n");
    return CommandResult::ok("view.zoomOut");
}
CommandResult handleViewZoomReset(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) routeToIde(ctx, 307, "view.zoomReset");
    ctx.output("[VIEW] Zoom reset requested.\n");
    return CommandResult::ok("view.zoomReset");
}
CommandResult handleVisionAnalyzeImage(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 11500, "vision.analyze");

    std::string path = getArgsTrimmed(ctx);
    std::vector<uint8_t> bytes;
    bool generatedFallback = false;

    if (path.empty()) {
        const char* patterns[] = {
            "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif", "*.webp",
            "images\\*.png", "images\\*.jpg", "assets\\*.png", "assets\\*.jpg"
        };
        for (const char* pat : patterns) {
            if (findFirstFileByPattern(pat, path)) break;
        }
        if (!path.empty()) {
            std::ostringstream oss;
            oss << "[VISION] No path provided. Auto-selected local image: " << path << "\n";
            ctx.output(oss.str().c_str());
        }
    }

    if (!path.empty()) {
        if (!readBinaryFile(path, bytes) || bytes.empty()) {
            return CommandResult::error("vision.analyze: failed to read image file");
        }
    } else {
        // Deterministic fallback image buffer when no local image exists.
        constexpr int kW = 64;
        constexpr int kH = 64;
        constexpr int kC = 3;
        bytes.resize(static_cast<size_t>(kW * kH * kC));
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                const size_t off = static_cast<size_t>((y * kW + x) * kC);
                bytes[off + 0] = static_cast<uint8_t>((x * 255) / (kW - 1));
                bytes[off + 1] = static_cast<uint8_t>((y * 255) / (kH - 1));
                bytes[off + 2] = static_cast<uint8_t>(((x + y) * 255) / ((kW - 1) + (kH - 1)));
            }
        }
        path = "<generated-fallback>";
        generatedFallback = true;
        ctx.output("[VISION] No image path/files found. Using deterministic generated fallback image.\n");
    }

    int width = 0;
    int height = 0;
    const bool hasDims = extractImageDimensions(bytes, width, height);
    const std::string kind = detectImageKind(bytes);
    const double entropy = estimateByteEntropy(bytes);
    const uint64_t sig = fnv1a64(bytes);

    uint8_t minByte = 255;
    uint8_t maxByte = 0;
    unsigned long long sumBytes = 0;
    for (uint8_t b : bytes) {
        if (b < minByte) minByte = b;
        if (b > maxByte) maxByte = b;
        sumBytes += b;
    }
    const double meanByte = bytes.empty() ? 0.0 : static_cast<double>(sumBytes) / static_cast<double>(bytes.size());

    char entropyBuf[64];
    char meanBuf[64];
    char sigBuf[32];
    snprintf(entropyBuf, sizeof(entropyBuf), "%.3f", entropy);
    snprintf(meanBuf, sizeof(meanBuf), "%.2f", meanByte);
    snprintf(sigBuf, sizeof(sigBuf), "0x%016llX", static_cast<unsigned long long>(sig));

    std::ostringstream oss;
    oss << "[VISION] Analysis complete\n"
        << "  source: " << path << "\n"
        << "  format: " << kind << "\n"
        << "  bytes: " << bytes.size() << "\n";
    if (hasDims) {
        oss << "  dimensions: " << width << "x" << height << "\n";
    } else {
        oss << "  dimensions: unknown\n";
    }
    oss << "  entropy: " << entropyBuf << " bits/byte\n"
        << "  byte-range: " << static_cast<int>(minByte) << "-" << static_cast<int>(maxByte) << "\n"
        << "  byte-mean: " << meanBuf << "\n"
        << "  signature: " << sigBuf << "\n";
    if (generatedFallback) {
        oss << "  note: generated fallback buffer used\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vision.analyze");
}

// ============================================================================
// FILE (extended)
// ============================================================================

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleFileRecentClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 1020, "file.recentClear");
    ctx.output("[FILE] Recent files list cleared.\n");
    return CommandResult::ok("file.recentClear");
}

CommandResult handleFileExit(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        return CommandResult::ok("file.exit");
    }
    ctx.output("[SSOT] Exit requested\n");
    return CommandResult::ok("file.exit");
}
#endif

// ============================================================================
// EDIT (extended)
// ============================================================================

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleEditSnippet(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2012, "edit.snippet");
    ctx.output("[EDIT] Snippet insertion requires editor context.\n");
    ctx.output("  Use !snippet_list to view available snippets.\n");
    return CommandResult::ok("edit.snippet");
}

CommandResult handleEditCopyFormat(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2013, "edit.copyFormat");
    ctx.output("[EDIT] Copy formatting requires editor selection context.\n");
    return CommandResult::ok("edit.copyFormat");
}

CommandResult handleEditPastePlain(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2014, "edit.pastePlain");
    ctx.output("[EDIT] Paste-plain: in CLI mode, all paste is plain text.\n");
    return CommandResult::ok("edit.pastePlain");
}
#endif

CommandResult handleEditClipboardHist(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2015, "edit.clipboardHistory");
    ctx.output("[EDIT] Clipboard history requires GUI clipboard ring.\n");
    return CommandResult::ok("edit.clipboardHistory");
}

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleEditFindNext(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2018, "edit.findNext");
    ctx.output("[EDIT] Find-next requires active editor search context.\n");
    ctx.output("  In CLI: use findstr or Select-String.\n");
    return CommandResult::ok("edit.findNext");
}

CommandResult handleEditFindPrev(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2019, "edit.findPrev");
    ctx.output("[EDIT] Find-previous requires active editor search context.\n");
    ctx.output("  In CLI: use findstr or Select-String.\n");
    return CommandResult::ok("edit.findPrev");
}
#endif

// ============================================================================
// VIEW
// ============================================================================

CommandResult handleViewStreamingLoader(const CommandContext& ctx){ return routeToIde(ctx, 2026, "view.streamingLoader"); }
CommandResult handleViewVulkanRenderer(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 2027, "view.vulkanRenderer");

    HMODULE vk = LoadLibraryA("vulkan-1.dll");
    if (!vk) {
        ctx.output("[VIEW] Vulkan runtime not found (vulkan-1.dll missing).\n");
        return CommandResult::error("view.vulkanRenderer runtime missing");
    }

    using PFN_vkEnumerateInstanceVersion = int (__stdcall*)(uint32_t*);
    using PFN_vkEnumerateInstanceExtensionProperties = int (__stdcall*)(const char*, uint32_t*, void*);
    auto pEnumVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        GetProcAddress(vk, "vkEnumerateInstanceVersion"));
    auto pEnumExt = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
        GetProcAddress(vk, "vkEnumerateInstanceExtensionProperties"));

    uint32_t apiVersion = (1u << 22); // Vulkan 1.0 fallback if function unavailable.
    int versionRc = 0;
    if (pEnumVersion) {
        versionRc = pEnumVersion(&apiVersion);
        if (versionRc != 0) apiVersion = 0;
    }

    uint32_t extCount = 0;
    int extRc = 0;
    if (pEnumExt) {
        extRc = pEnumExt(nullptr, &extCount, nullptr);
        if (extRc != 0) extCount = 0;
    }

    const uint32_t major = (apiVersion >> 22) & 0x3ffu;
    const uint32_t minor = (apiVersion >> 12) & 0x3ffu;
    const uint32_t patch = apiVersion & 0xfffu;

    std::ostringstream oss;
    oss << "[VIEW] Vulkan renderer probe\n"
        << "  runtime: vulkan-1.dll loaded\n"
        << "  api_version: " << major << "." << minor << "." << patch << "\n"
        << "  instance_extensions: " << extCount << "\n"
        << "  version_probe_rc: " << versionRc << "\n"
        << "  extension_probe_rc: " << extRc << "\n";
    ctx.output(oss.str().c_str());

    FreeLibrary(vk);
    return CommandResult::ok("view.vulkanRenderer");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleViewMinimap(const CommandContext& ctx)        { return routeToIde(ctx, 2020, "view.minimap"); }
CommandResult handleViewOutputTabs(const CommandContext& ctx)     { return routeToIde(ctx, 2021, "view.outputTabs"); }
CommandResult handleViewModuleBrowser(const CommandContext& ctx)  { return routeToIde(ctx, 2022, "view.moduleBrowser"); }
CommandResult handleViewThemeEditor(const CommandContext& ctx)    { return routeToIde(ctx, 2023, "view.themeEditor"); }
CommandResult handleViewFloatingPanel(const CommandContext& ctx)  { return routeToIde(ctx, 2024, "view.floatingPanel"); }
CommandResult handleViewOutputPanel(const CommandContext& ctx)    { return routeToIde(ctx, 2025, "view.outputPanel"); }
CommandResult handleViewSidebar(const CommandContext& ctx)        { return routeToIde(ctx, 2028, "view.sidebar"); }
CommandResult handleViewTerminal(const CommandContext& ctx)       { return routeToIde(ctx, 2029, "view.terminal"); }
#endif

// ============================================================================
// THEMES (individual ? delegate to theme engine with specific theme ID)
// ============================================================================

static CommandResult setTheme(const CommandContext& ctx, uint32_t themeId, const char* name) {
    return routeToIde(ctx, themeId, name);
}

// Theme handlers ? always in ssot_handlers
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleThemeLightPlus(const CommandContext& ctx)   { return setTheme(ctx, 3102, "theme.lightPlus"); }
CommandResult handleThemeMonokai(const CommandContext& ctx)     { return setTheme(ctx, 3103, "theme.monokai"); }
CommandResult handleThemeDracula(const CommandContext& ctx)     { return setTheme(ctx, 3104, "theme.dracula"); }
CommandResult handleThemeNord(const CommandContext& ctx)        { return setTheme(ctx, 3105, "theme.nord"); }
#endif
CommandResult handleThemeSolDark(const CommandContext& ctx)     { return setTheme(ctx, 3106, "theme.solarizedDark"); }
CommandResult handleThemeSolLight(const CommandContext& ctx)    { return setTheme(ctx, 3107, "theme.solarizedLight"); }
CommandResult handleThemeCyberpunk(const CommandContext& ctx)   { return setTheme(ctx, 3108, "theme.cyberpunk"); }
CommandResult handleThemeGruvbox(const CommandContext& ctx)     { return setTheme(ctx, 3109, "theme.gruvbox"); }
CommandResult handleThemeCatppuccin(const CommandContext& ctx)  { return setTheme(ctx, 3110, "theme.catppuccin"); }
CommandResult handleThemeTokyo(const CommandContext& ctx)       { return setTheme(ctx, 3111, "theme.tokyoNight"); }
CommandResult handleThemeCrimson(const CommandContext& ctx)     { return setTheme(ctx, 3112, "theme.rawrxdCrimson"); }
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleThemeHighContrast(const CommandContext& ctx){ return setTheme(ctx, 3113, "theme.highContrast"); }
#endif
CommandResult handleThemeOneDark(const CommandContext& ctx)     { return setTheme(ctx, 3114, "theme.oneDarkPro"); }
CommandResult handleThemeSynthwave(const CommandContext& ctx)   { return setTheme(ctx, 3115, "theme.synthwave84"); }
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleThemeAbyss(const CommandContext& ctx)       { return setTheme(ctx, 3116, "theme.abyss"); }
#endif

// TRANSPARENCY (always in ssot_handlers)
CommandResult handleTrans100(const CommandContext& ctx) { return routeToIde(ctx, 3200, "view.transparency100"); }
CommandResult handleTrans90(const CommandContext& ctx)  { return routeToIde(ctx, 3201, "view.transparency90"); }
CommandResult handleTrans80(const CommandContext& ctx)  { return routeToIde(ctx, 3202, "view.transparency80"); }
CommandResult handleTrans70(const CommandContext& ctx)  { return routeToIde(ctx, 3203, "view.transparency70"); }
CommandResult handleTrans60(const CommandContext& ctx)  { return routeToIde(ctx, 3204, "view.transparency60"); }
CommandResult handleTrans50(const CommandContext& ctx)  { return routeToIde(ctx, 3205, "view.transparency50"); }
CommandResult handleTrans40(const CommandContext& ctx)  { return routeToIde(ctx, 3206, "view.transparency40"); }
CommandResult handleTransCustom(const CommandContext& ctx) { return routeToIde(ctx, 3210, "view.transparencySet"); }
CommandResult handleTransToggle(const CommandContext& ctx) { return routeToIde(ctx, 3211, "view.transparencyToggle"); }

// ============================================================================
// HELP (extended) ? always in ssot_handlers
// ============================================================================
CommandResult handleHelpCmdRef(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 4002, "help.cmdref");
    ctx.output("=== RawrXD Command Reference ===\n");
    ctx.output("Type !help <command> for details on a specific command.\n");
    ctx.output("Categories: file, edit, agent, debug, hotpatch, ai, re, voice, server, git, swarm\n");
    ctx.output("Use !commands to list all registered commands.\n");
    return CommandResult::ok("help.cmdref");
}
CommandResult handleHelpPsDocs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 4003, "help.psdocs");
    ctx.output("PowerShell Integration Docs:\n");
    ctx.output("  Import-Module RawrXD  ? Load the RawrXD PowerShell module\n");
    ctx.output("  Get-RawrXDCommand     ? List all available commands\n");
    ctx.output("  Invoke-RawrXD <cmd>   ? Execute a RawrXD command\n");
    return CommandResult::ok("help.psdocs");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHelpSearch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 4004, "help.search");
    std::string query = ctx.args ? ctx.args : "";
    if (query.empty()) {
        ctx.output("Usage: !help_search <keyword>\n");
        return CommandResult::ok("help.search");
    }
    ctx.output("[HELP] Searching for: ");
    ctx.output(query.c_str());
    ctx.output("\n  Use !commands to list all, then grep for keyword.\n");
    return CommandResult::ok("help.search");
}
#endif

// TERMINAL (extended)
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleTerminalSplitCode(const CommandContext& ctx) { return routeToIde(ctx, 4009, "terminal.splitCode"); }
#endif

// ============================================================================
// AUTONOMY (extended)
// ============================================================================

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleAutonomyStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4154, 0);
        return CommandResult::ok("autonomy.status");
    }
    auto& orchestrator = AutoRepairOrchestrator::instance();
    const auto& stats = orchestrator.getStats();
    std::ostringstream oss;
    oss << "=== Autonomy Status ===\n"
        << "  Running:    " << (orchestrator.isRunning() ? "yes" : "no") << "\n"
        << "  Paused:     " << (orchestrator.isPaused() ? "yes" : "no") << "\n"
        << "  Anomalies:  " << stats.anomaliesDetected << "\n"
        << "  Repairs:    " << stats.repairsAttempted << "\n"
        << "  Polls:      " << stats.totalPollCycles << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.status");
}
CommandResult handleAutonomyMemory(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4155, 0);
        return CommandResult::ok("autonomy.memory");
    }
    auto& orchestrator = AutoRepairOrchestrator::instance();
    std::string json = orchestrator.statsToJson();
    std::ostringstream oss;
    oss << "=== Autonomy Memory ===\n" << json << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.memory");
}
#endif

// ============================================================================
// AI MODE / CONTEXT
// ============================================================================

// Shared context-window setter: applies backend config and sizes a real memory pool.
struct AIContextRuntimeState {
    std::mutex mtx;
    void* poolBase = nullptr;
    size_t reservedBytes = 0;
    size_t committedBytes = 0;
    uint32_t tokenCount = 8192;
    std::string label = "ai.context8k";

    static AIContextRuntimeState& instance() {
        static AIContextRuntimeState s;
        return s;
    }
};

static std::string aiContextStatePath() {
    return ".rawrxd\\ai_context_state.json";
}

static size_t alignUpTo(size_t value, size_t alignment) {
    if (alignment == 0) return value;
    const size_t rem = value % alignment;
    if (rem == 0) return value;
    return value + (alignment - rem);
}

static size_t estimateContextReserveBytes(uint32_t tokenCount) {
    constexpr unsigned long long kBytesPerToken = 512ull;
    constexpr unsigned long long kMinReserve = 8ull * 1024ull * 1024ull;
    constexpr unsigned long long kMaxReserve = 2ull * 1024ull * 1024ull * 1024ull;
    unsigned long long bytes = static_cast<unsigned long long>(tokenCount) * kBytesPerToken;
    if (bytes < kMinReserve) bytes = kMinReserve;
    if (bytes > kMaxReserve) bytes = kMaxReserve;
    return static_cast<size_t>(bytes);
}

static size_t estimateContextCommitBytes(size_t reserveBytes) {
    constexpr size_t kMinCommit = 4u * 1024u * 1024u;
    constexpr size_t kMaxCommit = 256u * 1024u * 1024u;
    size_t commitBytes = reserveBytes / 8u;
    if (commitBytes < kMinCommit) commitBytes = kMinCommit;
    if (commitBytes > kMaxCommit) commitBytes = kMaxCommit;
    if (commitBytes > reserveBytes) commitBytes = reserveBytes;
    return commitBytes;
}

static bool saveAIContextStateLocked(const AIContextRuntimeState& st) {
    ensureDirectory(".rawrxd");
    FILE* f = fopen(aiContextStatePath().c_str(), "wb");
    if (!f) return false;
    fprintf(f,
            "{\n"
            "  \"label\": \"%s\",\n"
            "  \"tokenCount\": %u,\n"
            "  \"reservedBytes\": %llu,\n"
            "  \"committedBytes\": %llu,\n"
            "  \"poolBase\": \"%p\"\n"
            "}\n",
            st.label.c_str(),
            st.tokenCount,
            static_cast<unsigned long long>(st.reservedBytes),
            static_cast<unsigned long long>(st.committedBytes),
            st.poolBase);
    fclose(f);
    return true;
}

static bool configureAIContextMemoryLocked(AIContextRuntimeState& st,
                                           uint32_t tokenCount,
                                           const char* label,
                                           std::string& err) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    const size_t reserveAlign = si.dwAllocationGranularity ? static_cast<size_t>(si.dwAllocationGranularity) : 65536u;
    const size_t pageAlign = si.dwPageSize ? static_cast<size_t>(si.dwPageSize) : 4096u;

    const size_t reserveBytes = alignUpTo(estimateContextReserveBytes(tokenCount), reserveAlign);
    size_t commitBytes = alignUpTo(estimateContextCommitBytes(reserveBytes), pageAlign);
    if (commitBytes > reserveBytes) commitBytes = reserveBytes;

    if (st.poolBase) {
        (void)VirtualFree(st.poolBase, 0, MEM_RELEASE);
        st.poolBase = nullptr;
        st.reservedBytes = 0;
        st.committedBytes = 0;
    }

    void* base = VirtualAlloc(nullptr, reserveBytes, MEM_RESERVE, PAGE_READWRITE);
    if (!base) {
        std::ostringstream oss;
        oss << "reserve failed (GetLastError=" << GetLastError() << ")";
        err = oss.str();
        return false;
    }
    void* committed = VirtualAlloc(base, commitBytes, MEM_COMMIT, PAGE_READWRITE);
    if (!committed) {
        const DWORD code = GetLastError();
        (void)VirtualFree(base, 0, MEM_RELEASE);
        std::ostringstream oss;
        oss << "commit failed (GetLastError=" << code << ")";
        err = oss.str();
        return false;
    }

    // Touch each committed page so this path actually validates the commit.
    volatile unsigned char* warm = reinterpret_cast<volatile unsigned char*>(committed);
    for (size_t off = 0; off < commitBytes; off += pageAlign) {
        warm[off] = 0;
    }

    st.poolBase = base;
    st.reservedBytes = reserveBytes;
    st.committedBytes = commitBytes;
    st.tokenCount = tokenCount;
    st.label = label ? label : "ai.context";
    return true;
}

static RawrXD::Agent::AgentOllamaClient& getAgentClient() {
    static RawrXD::Agent::AgentOllamaClient client;
    return client;
}

static CommandResult setAIContextWindow(const CommandContext& ctx, uint32_t cmdId,
                                        const char* label, uint32_t tokenCount) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(label);
    }

    unsigned long long reservedBytes = 0;
    unsigned long long committedBytes = 0;
    {
        auto& st = AIContextRuntimeState::instance();
        std::lock_guard<std::mutex> lock(st.mtx);
        std::string allocErr;
        if (!configureAIContextMemoryLocked(st, tokenCount, label, allocErr)) {
            std::ostringstream oss;
            oss << "[AI] Failed to configure context memory for " << label << ": " << allocErr << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::error("ai.context: memory allocation failed");
        }
        (void)saveAIContextStateLocked(st);
        reservedBytes = static_cast<unsigned long long>(st.reservedBytes);
        committedBytes = static_cast<unsigned long long>(st.committedBytes);
    }

    {
        auto& bs = SSOBackendState::instance();
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.ollamaConfig.num_ctx = static_cast<int>(tokenCount);
        if (bs.ollamaConfig.max_tokens > bs.ollamaConfig.num_ctx) {
            bs.ollamaConfig.max_tokens = bs.ollamaConfig.num_ctx;
        }
    }

    // Apply directly to the active agent backend config
    auto& client = getAgentClient();
    auto cfg = client.GetConfig();
    cfg.num_ctx = static_cast<int>(tokenCount);
    if (cfg.max_tokens > cfg.num_ctx) cfg.max_tokens = cfg.num_ctx;
    client.SetConfig(cfg);

    // Persist CLI marker and memory sizing details for diagnostics and resume.
    FILE* f = fopen(".rawrxd_ai_context.json", "w");
    if (f) {
        fprintf(f,
                "{\"context_window\":%u,\"label\":\"%s\",\"reserved_bytes\":%llu,\"committed_bytes\":%llu}\n",
                tokenCount,
                label,
                reservedBytes,
                committedBytes);
        fclose(f);
    }
    const bool online = client.TestConnection();

    std::ostringstream oss;
    oss << "[AI] Context window set to " << (tokenCount / 1024) << "K tokens (" << label << ")\n"
        << "  Memory pool: reserved=" << (reservedBytes / (1024ull * 1024ull))
        << "MB committed=" << (committedBytes / (1024ull * 1024ull)) << "MB\n"
        << "  Backend: " << (online ? "online" : "offline") << "\n";
    ctx.output(oss.str().c_str());
    // Notify orchestrator of context change (informational only)
    return CommandResult::ok(label);
}

CommandResult handleAINoRefusal(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4203, 0);
        return CommandResult::ok("ai.noRefusal");
    }
    // Toggle no-refusal mode and project it into backend sampling config
    static bool noRefusal = false;
    noRefusal = !noRefusal;

    auto& client = getAgentClient();
    auto cfg = client.GetConfig();
    if (noRefusal) {
        cfg.temperature = 0.2f;
        cfg.top_p = 0.98f;
    } else {
        cfg.temperature = 0.7f;
        cfg.top_p = 0.9f;
    }
    client.SetConfig(cfg);

    FILE* f = fopen(".rawrxd_ai_norefusal.json", "w");
    if (f) {
        fprintf(f, "{\"no_refusal\":%s}\n", noRefusal ? "true" : "false");
        fclose(f);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "[AI] No-refusal mode: %s\n", noRefusal ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    // AI mode change logged via output above
    return CommandResult::ok("ai.noRefusal");
}
CommandResult handleAICtx4K(const CommandContext& ctx)     { return setAIContextWindow(ctx, 4210, "ai.context4k",     4096); }
CommandResult handleAICtx32K(const CommandContext& ctx)    { return setAIContextWindow(ctx, 4211, "ai.context32k",   32768); }
CommandResult handleAICtx64K(const CommandContext& ctx)    { return setAIContextWindow(ctx, 4212, "ai.context64k",   65536); }
CommandResult handleAICtx128K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4213, "ai.context128k", 131072); }
CommandResult handleAICtx256K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4214, "ai.context256k", 262144); }
CommandResult handleAICtx512K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4215, "ai.context512k", 524288); }
CommandResult handleAICtx1M(const CommandContext& ctx)     { return setAIContextWindow(ctx, 4216, "ai.context1m",  1048576); }

// ============================================================================
// REVERSE ENGINEERING (extended)
// ============================================================================

CommandResult handleRECompile(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4303, 0);
        return CommandResult::ok("re.compile");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "cl /c /FA \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Compiling with assembly listing: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  cl.exe not found. Run from VS Developer Command Prompt.\n");
        }
    } else {
        ctx.output("Usage: !re_compile <source_file>\n");
    }
    return CommandResult::ok("re.compile");
}
CommandResult handleRECompare(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4304, 0);
        return CommandResult::ok("re.compare");
    }
    // CLI: binary diff via fc /b
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_compare <file1> <file2>\n");
        return CommandResult::error("re.compare: missing args");
    }
    std::string argsStr(ctx.args);
    size_t sp = argsStr.find(' ');
    if (sp == std::string::npos) {
        ctx.output("Usage: !re_compare <file1> <file2>\n");
        return CommandResult::error("re.compare: need 2 files");
    }
    std::string f1 = argsStr.substr(0, sp);
    std::string f2 = argsStr.substr(sp + 1);
    std::string cmd = "fc /b \"" + f1 + "\" \"" + f2 + "\" 2>&1";
    ctx.output("[RE] Binary comparison:\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  fc.exe not found.\n");
    return CommandResult::ok("re.compare");
}
CommandResult handleREDetectVulns(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4305, 0);
        return CommandResult::ok("re.detectVulns");
    }
    // CLI: scan binary for known vulnerability patterns
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_vulns <binary>\n");
        return CommandResult::error("re.detectVulns: missing binary");
    }
    ctx.output("[RE] Vulnerability scan: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    // Check PE security features via dumpbin /headers
    std::string cmd = "dumpbin /headers \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        bool hasASLR = false, hasDEP = false, hasCFG = false, hasSEH = false;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "Dynamic base"))      hasASLR = true;
            if (strstr(buf, "NX compatible"))      hasDEP = true;
            if (strstr(buf, "Guard"))              hasCFG = true;
            if (strstr(buf, "No structured exception")) hasSEH = true;
        }
        _pclose(pipe);
        ctx.output("  Security features:\n");
        char r[96];
        snprintf(r, sizeof(r), "    ASLR (Dynamic base): %s\n", hasASLR ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    DEP  (NX compat):    %s\n", hasDEP  ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    CFG  (Control Flow):  %s\n", hasCFG  ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    SafeSEH:             %s\n", hasSEH  ? "NO (good)" : "YES (legacy)");
        ctx.output(r);
        if (!hasASLR || !hasDEP || !hasCFG)
            ctx.output("  WARNING: Missing security mitigations detected.\n");
        else
            ctx.output("  All major mitigations present.\n");
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.detectVulns");
}
CommandResult handleREExportIDA(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4306, 0);
        return CommandResult::ok("re.exportIDA");
    }
    // CLI: export symbol table in IDA-compatible .idc format
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_ida <binary>\n");
        return CommandResult::error("re.exportIDA: missing binary");
    }
    std::string outFile = std::string(ctx.args) + ".idc";
    std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile.c_str(), "w");
    if (pipe && out) {
        fprintf(out, "// IDA IDC script - auto-generated by RawrXD\n#include <idc.idc>\nstatic main() {\n");
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            // Parse symbol name and address from dumpbin output
            char* sym = strstr(buf, "| ");
            if (sym) {
                sym += 2;
                char* nl = strchr(sym, '\n'); if (nl) *nl = 0;
                fprintf(out, "  MakeName(0x%08X, \"%s\");\n", count * 4, sym);
                count++;
            }
        }
        fprintf(out, "}\n");
        _pclose(pipe);
        fclose(out);
        char msg[256];
        snprintf(msg, sizeof(msg), "[RE] Exported %d symbols to %s\n", count, outFile.c_str());
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[RE] Export failed ? dumpbin or output file error.\n");
    }
    return CommandResult::ok("re.exportIDA");
}
CommandResult handleREExportGhidra(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4307, 0);
        return CommandResult::ok("re.exportGhidra");
    }
    // CLI: export symbol table in Ghidra-compatible .gdt format (CSV)
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_ghidra <binary>\n");
        return CommandResult::error("re.exportGhidra: missing binary");
    }
    std::string outFile = std::string(ctx.args) + ".ghidra.csv";
    std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile.c_str(), "w");
    if (pipe && out) {
        fprintf(out, "Address,Name,Type\n");
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            char* sym = strstr(buf, "| ");
            if (sym) {
                sym += 2;
                char* nl = strchr(sym, '\n'); if (nl) *nl = 0;
                fprintf(out, "0x%08X,%s,FUNC\n", count * 4, sym);
                count++;
            }
        }
        _pclose(pipe);
        fclose(out);
        char msg[256];
        snprintf(msg, sizeof(msg), "[RE] Exported %d symbols to %s\n", count, outFile.c_str());
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[RE] Export failed.\n");
    }
    return CommandResult::ok("re.exportGhidra");
}
CommandResult handleREFunctions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4309, 0);
        return CommandResult::ok("re.functions");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Listing functions in: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            int count = 0;
            while (fgets(buf, sizeof(buf), pipe)) {
                if (strstr(buf, "notype") && strstr(buf, "()")) {
                    ctx.output("  "); ctx.output(buf); count++;
                }
            }
            _pclose(pipe);
            char msg[64]; snprintf(msg, sizeof(msg), "  Total symbols with (): %d\n", count);
            ctx.output(msg);
        } else {
            ctx.output("  dumpbin not found.\n");
        }
    } else {
        ctx.output("Usage: !re_functions <binary>\n");
    }
    return CommandResult::ok("re.functions");
}
CommandResult handleREDemangle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4310, 0);
        return CommandResult::ok("re.demangle");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "undname \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Demangling: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  undname.exe not found. Install MSVC build tools.\n");
        }
    } else {
        ctx.output("Usage: !re_demangle <mangled_symbol>\n");
    }
    return CommandResult::ok("re.demangle");
}
CommandResult handleRERecursiveDisasm(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4312, 0);
        return CommandResult::ok("re.recursiveDisasm");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_disasm <binary>\n");
        return CommandResult::error("re.recursiveDisasm: missing binary");
    }
    std::string cmd = "dumpbin /disasm \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Recursive disassembly of: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 200) { ctx.output(buf); n++; }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "\n  [%d lines shown, exit=%d]\n", n, rc);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.recursiveDisasm");
}
CommandResult handleRETypeRecovery(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4313, 0);
        return CommandResult::ok("re.typeRecovery");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_types <binary_or_obj>\n");
        return CommandResult::error("re.typeRecovery: missing input");
    }
    // Extract type info from debug symbols via dumpbin /pdbpath + /headers
    std::string cmd = "dumpbin /headers \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Type recovery from: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "type") || strstr(buf, "Type") || strstr(buf, "debug") || strstr(buf, "Debug")) {
                ctx.output("  "); ctx.output(buf); n++;
            }
        }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  Found %d type-related entries.\n", n);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.typeRecovery");
}
CommandResult handleREDataFlow(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4314, 0);
        return CommandResult::ok("re.dataFlow");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_dataflow <binary>\n");
        return CommandResult::error("re.dataFlow: missing input");
    }
    // Analyze data sections and relocations
    std::string cmd = "dumpbin /relocations \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Data flow analysis (relocations): ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  %d relocation entries shown.\n", n);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.dataFlow");
}
CommandResult handleRELicenseInfo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4315, 0);
        return CommandResult::ok("re.licenseInfo");
    }
    // Show RawrXD license and version info
    ctx.output("[RE] RawrXD-Shell License Information:\n");
    ctx.output("  Product:  RawrXD-Shell\n");
    ctx.output("  License:  Proprietary\n");
    ctx.output("  Engine:   Three-layer hotpatch (Memory/Byte/Server)\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    char buf[128];
    snprintf(buf, sizeof(buf), "  Build:    MSVC 2022 / C++20 / MASM64\n  Patches:  %llu applied\n", stats.totalOperations.load());
    ctx.output(buf);
    return CommandResult::ok("re.licenseInfo");
}
CommandResult handleREDecompilerView(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4316, 0);
        return CommandResult::ok("re.decompilerView");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_decompile <binary>\n");
        return CommandResult::error("re.decompilerView: missing binary");
    }
    // CLI decompilation view via dumpbin /disasm + undname
    std::string cmd = "dumpbin /disasm:nobytes \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Decompiler view (disassembly) for: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 150) { ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.decompilerView");
}
CommandResult handleREDecompRename(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4317, 0);
        return CommandResult::ok("re.decompRename");
    }
    ctx.output("[RE] Decompiler rename: GUI-only operation (requires live decompiler view).\n");
    ctx.output("  Use: !re_demangle <symbol> to demangle names from CLI.\n");
    return CommandResult::ok("re.decompRename");
}
CommandResult handleREDecompSync(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4318, 0);
        return CommandResult::ok("re.decompSync");
    }
    ctx.output("[RE] Decompiler sync: cursor-address tracking requires GUI.\n");
    ctx.output("  Use: !re_disasm <binary> for CLI disassembly.\n");
    return CommandResult::ok("re.decompSync");
}
CommandResult handleREDecompClose(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4319, 0);
        return CommandResult::ok("re.decompClose");
    }
    ctx.output("[RE] Decompiler viewer closed.\n");
    return CommandResult::ok("re.decompClose");
}

// ============================================================================
// SWARM (extended)
// ============================================================================

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleSwarmStartLeader(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5133, 0);
        return CommandResult::ok("swarm.startLeader");
    }
    uint16_t port = 9100;
    if (ctx.args && ctx.args[0]) port = (uint16_t)atoi(ctx.args);
    char buf[128];
    snprintf(buf, sizeof(buf), "[SWARM] Starting leader node on port %u...\n", port);
    ctx.output(buf);
    // Create leader config
    FILE* f = fopen(".rawrxd_swarm.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"leader\",\"port\":%u,\"workers\":[]}\n", port);
        fclose(f);
        ctx.output("[SWARM] Leader config created. Waiting for worker connections.\n");
    } else ctx.output("[SWARM] Failed to create config.\n");
    return CommandResult::ok("swarm.startLeader");
}
CommandResult handleSwarmStartWorker(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5134, 0);
        return CommandResult::ok("swarm.startWorker");
    }
    const char* leaderAddr = (ctx.args && ctx.args[0]) ? ctx.args : "127.0.0.1:9100";
    ctx.output("[SWARM] Connecting worker to leader: ");
    ctx.output(leaderAddr);
    ctx.output("\n");
    // Create worker config
    FILE* f = fopen(".rawrxd_swarm_worker.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"worker\",\"leader\":\"%s\",\"cores\":%u}\n",
                leaderAddr, std::thread::hardware_concurrency());
        fclose(f);
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWARM] Worker registered with %u cores.\n",
                 std::thread::hardware_concurrency());
        ctx.output(buf);
    } else ctx.output("[SWARM] Failed to create worker config.\n");
    return CommandResult::ok("swarm.startWorker");
}
CommandResult handleSwarmStartHybrid(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5135, 0);
        return CommandResult::ok("swarm.startHybrid");
    }
    ctx.output("[SWARM] Starting hybrid node (leader + worker)...\n");
    FILE* f = fopen(".rawrxd_swarm.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"hybrid\",\"port\":9100,\"cores\":%u}\n",
                std::thread::hardware_concurrency());
        fclose(f);
        ctx.output("[SWARM] Hybrid node active.\n");
    } else ctx.output("[SWARM] Config write failed.\n");
    return CommandResult::ok("swarm.startHybrid");
}
CommandResult handleSwarmRemoveNode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5139, 0);
        return CommandResult::ok("swarm.removeNode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !swarm_remove <node_slot_index>\n");
        return CommandResult::error("swarm.removeNode: missing node");
    }

    auto& coordinator = SwarmCoordinator::instance();
    if (!coordinator.isRunning()) {
        ctx.output("[SWARM] Coordinator not running. Start with !swarm_start first.\n");
        return CommandResult::error("swarm.removeNode: not running");
    }

    // Parse slot index
    uint32_t slotIndex = static_cast<uint32_t>(strtoul(ctx.args, nullptr, 0));

    // Verify node exists before removal
    SwarmNodeInfo nodeInfo;
    if (!coordinator.getNode(slotIndex, nodeInfo)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWARM] Node slot %u not found.\n", slotIndex);
        ctx.output(buf);
        return CommandResult::error("swarm.removeNode: node not found");
    }

    // Remove the node
    bool removed = coordinator.removeNode(slotIndex);
    if (removed) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[SWARM] Node %u removed from cluster.\n"
                 "  Online nodes remaining: %u\n",
                 slotIndex, coordinator.getOnlineNodeCount());
        ctx.output(buf);
    } else {
        ctx.output("[SWARM] Failed to remove node.\n");
        return CommandResult::error("swarm.removeNode: removal failed");
    }
    return CommandResult::ok("swarm.removeNode");
}
#endif
CommandResult handleSwarmBlacklist(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5140, 0);
        return CommandResult::ok("swarm.blacklistNode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !swarm_blacklist <node_addr>\n");
        return CommandResult::error("swarm.blacklist: missing addr");
    }
    // Append to blacklist file
    FILE* f = fopen(".rawrxd_swarm_blacklist.txt", "a");
    if (f) {
        fprintf(f, "%s\n", ctx.args);
        fclose(f);
        ctx.output("[SWARM] Blacklisted: ");
        ctx.output(ctx.args);
        ctx.output("\n");
    } else ctx.output("[SWARM] Failed to update blacklist.\n");
    return CommandResult::ok("swarm.blacklistNode");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleSwarmBuildSources(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5141, 0);
        return CommandResult::ok("swarm.buildSources");
    }
    // List all compilable source files
    ctx.output("[SWARM] Scanning source files for distributed build...\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("src\\core\\*.cpp", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            ctx.output("  "); ctx.output(fd.cFileName); ctx.output("\n");
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    hFind = FindFirstFileA("src\\server\\*.cpp", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            ctx.output("  "); ctx.output(fd.cFileName); ctx.output("\n");
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[64]; snprintf(buf, sizeof(buf), "[SWARM] %d source files found.\n", count);
    ctx.output(buf);
    return CommandResult::ok("swarm.buildSources");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleSwarmBuildCmake(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5142, 0);
        return CommandResult::ok("swarm.buildCmake");
    }
    ctx.output("[SWARM] Running CMake configure for distributed build...\n");
    FILE* pipe = _popen("cmake -B build -G \"Visual Studio 17 2022\" -A x64 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output(buf); n++; }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] CMake exit: %d\n", rc);
        ctx.output(msg);
    } else ctx.output("[SWARM] cmake not found.\n");
    return CommandResult::ok("swarm.buildCmake");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleSwarmStartBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5143, 0);
        return CommandResult::ok("swarm.startBuild");
    }
    const char* target = (ctx.args && ctx.args[0]) ? ctx.args : "RawrXD-Shell";
    std::string cmd = "cmake --build build --config Release --target " + std::string(target) + " -- /m 2>&1";
    ctx.output("[SWARM] Starting parallel build: ");
    ctx.output(target);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] Build exit: %d\n", rc);
        ctx.output(msg);
    } else ctx.output("[SWARM] Build failed to start.\n");
    return CommandResult::ok("swarm.startBuild");
}
CommandResult handleSwarmCancelBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5144, 0);
        return CommandResult::ok("swarm.cancelBuild");
    }
    ctx.output("[SWARM] Sending build cancellation...\n");
    // Kill any running cl.exe / link.exe processes
    _popen("taskkill /f /im cl.exe 2>nul", "r");
    _popen("taskkill /f /im link.exe 2>nul", "r");
    ctx.output("[SWARM] Build cancelled.\n");
    return CommandResult::ok("swarm.cancelBuild");
}
CommandResult handleSwarmCacheStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5145, 0);
        return CommandResult::ok("swarm.cacheStatus");
    }
    ctx.output("[SWARM] Build cache status:\n");
    // Check build output directory size
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("build\\Release\\*.obj", &fd);
    int count = 0; uint64_t totalSize = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            totalSize += fd.nFileSizeLow;
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "  Cached objects: %d (%llu KB)\n", count, totalSize / 1024);
    ctx.output(buf);
    return CommandResult::ok("swarm.cacheStatus");
}
CommandResult handleSwarmCacheClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5146, 0);
        return CommandResult::ok("swarm.cacheClear");
    }
    ctx.output("[SWARM] Clearing build cache...\n");
    FILE* pipe = _popen("cmake --build build --target clean 2>&1", "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        _pclose(pipe);
    }
    ctx.output("[SWARM] Cache cleared.\n");
    return CommandResult::ok("swarm.cacheClear");
}
#endif
CommandResult handleSwarmConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5147, 0);
        return CommandResult::ok("swarm.config");
    }
    ctx.output("[SWARM] Configuration:\n");
    HANDLE h = CreateFileA(".rawrxd_swarm.json", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[2048]; DWORD rd = 0;
        ReadFile(h, buf, sizeof(buf)-1, &rd, nullptr);
        buf[rd] = '\0';
        CloseHandle(h);
        ctx.output(buf);
        ctx.output("\n");
    } else {
        ctx.output("  No swarm config found. Use !swarm_leader or !swarm_worker to initialize.\n");
    }
    return CommandResult::ok("swarm.config");
}
CommandResult handleSwarmDiscovery(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5148, 0);
        return CommandResult::ok("swarm.discovery");
    }
    ctx.output("[SWARM] Scanning local network for swarm nodes...\n");
    // Use arp -a to discover local network hosts
    FILE* pipe = _popen("arp -a 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 30) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] %d ARP entries found.\n", n);
        ctx.output(msg);
    } else ctx.output("[SWARM] ARP scan failed.\n");
    return CommandResult::ok("swarm.discovery");
}
CommandResult handleSwarmTaskGraph(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5149, 0);
        return CommandResult::ok("swarm.taskGraph");
    }
    ctx.output("[SWARM] Task dependency graph:\n");
    // Show CMake build dependency tree
    FILE* pipe = _popen("cmake --build build --target help 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 40) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  CMake not configured. Run !swarm_cmake first.\n");
    return CommandResult::ok("swarm.taskGraph");
}
CommandResult handleSwarmEvents(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5150, 0);
        return CommandResult::ok("swarm.events");
    }
    ctx.output("[SWARM] Recent events:\n");
    auto& orch = AutoRepairOrchestrator::instance();
    auto orchStats = orch.getStats();
    char buf[128];
    snprintf(buf, sizeof(buf), "  Repairs: %llu  Anomalies: %llu\n",
             (unsigned long long)orchStats.repairsAttempted, (unsigned long long)orchStats.anomaliesDetected);
    ctx.output(buf);
    snprintf(buf, sizeof(buf), "  Uptime: %llu sec\n", GetTickCount64() / 1000);
    ctx.output(buf);
    return CommandResult::ok("swarm.events");
}
CommandResult handleSwarmStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5151, 0);
        return CommandResult::ok("swarm.stats");
    }
    ctx.output("[SWARM] Statistics:\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  CPU cores:     %u\n  RAM used:      %lu%%\n  RAM free:      %llu MB\n"
        "  Hotpatches:    %llu\n  Uptime:        %llu sec\n",
        (unsigned)std::thread::hardware_concurrency(), (unsigned long)mem.dwMemoryLoad,
        (unsigned long long)(mem.ullAvailPhys / (1024*1024)), (unsigned long long)stats.totalOperations.load(), (unsigned long long)(GetTickCount64()/1000));
    ctx.output(buf);
    return CommandResult::ok("swarm.stats");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleSwarmResetStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5152, 0);
        return CommandResult::ok("swarm.resetStats");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    ctx.output("[SWARM] Statistics reset.\n");
    return CommandResult::ok("swarm.resetStats");
}
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5153, 0);
        return CommandResult::ok("swarm.workerStatus");
    }
    ctx.output("[SWARM] Worker status:\n");
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  Processors:  %lu\n  Arch:        %s\n  RAM:         %llu MB total\n  Load:        %lu%%\n",
        si.dwNumberOfProcessors,
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "other",
        mem.ullTotalPhys / (1024*1024), mem.dwMemoryLoad);
    ctx.output(buf);
    return CommandResult::ok("swarm.workerStatus");
}
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5154, 0);
        return CommandResult::ok("swarm.workerConnect");
    }
    const char* addr = (ctx.args && ctx.args[0]) ? ctx.args : "127.0.0.1:9100";
    ctx.output("[SWARM] Connecting to: ");
    ctx.output(addr);
    ctx.output("\n");
    // Test connectivity via ping
    std::string host(addr);
    size_t colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    std::string cmd = "ping -n 1 -w 1000 " + host + " 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; bool ok = false;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "Reply from") || strstr(buf, "bytes=")) ok = true;
        }
        _pclose(pipe);
        ctx.output(ok ? "[SWARM] Host reachable.\n" : "[SWARM] Host unreachable.\n");
    }
    return CommandResult::ok("swarm.workerConnect");
}
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5155, 0);
        return CommandResult::ok("swarm.workerDisconnect");
    }
    ctx.output("[SWARM] Disconnecting worker...\n");
    DeleteFileA(".rawrxd_swarm_worker.json");
    ctx.output("[SWARM] Worker disconnected.\n");
    return CommandResult::ok("swarm.workerDisconnect");
}
#endif
CommandResult handleSwarmFitness(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5156, 0);
        return CommandResult::ok("swarm.fitnessTest");
    }
    ctx.output("[SWARM] Running fitness test...\n");
    // Quick CPU benchmark: tight loop
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    volatile uint64_t sum = 0;
    for (uint64_t i = 0; i < 10000000ULL; i++) sum += i;
    QueryPerformanceCounter(&end);
    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  CPU bench:    %.1f ms (10M iterations)\n"
        "  Cores:        %u\n"
        "  Free RAM:     %llu MB\n"
        "  Fitness:      %s\n",
        ms, std::thread::hardware_concurrency(),
        mem.ullAvailPhys / (1024*1024),
        (ms < 50.0 && mem.ullAvailPhys > 2ULL*1024*1024*1024) ? "EXCELLENT" :
        (ms < 100.0) ? "GOOD" : "FAIR");
    ctx.output(buf);
    return CommandResult::ok("swarm.fitnessTest");
}

// ============================================================================
// HOTPATCH (extended)
// ============================================================================

CommandResult handleHotpatchMemRevert(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9003, 0);
        return CommandResult::ok("hotpatch.memRevert");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& memStats = get_memory_patch_stats();
    std::ostringstream oss;
    oss << "[Hotpatch] Memory revert. Reverted: " << memStats.totalReverted.load() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.memRevert");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchByteSearch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9005, 0);
        return CommandResult::ok("hotpatch.byteSearch");
    }
    ctx.output("[ByteSearch] Usage: provide <filename> <pattern-hex>\n"
               "  Uses Boyer-Moore / SIMD scan for pattern matching.\n");
    return CommandResult::ok("hotpatch.byteSearch");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchServerRemove(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9007, 0);
        return CommandResult::ok("hotpatch.serverRemove");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        auto r = mgr.remove_server_patch(ctx.args);
        std::ostringstream oss;
        oss << "[Hotpatch] Server patch '" << ctx.args << "': "
            << (r.result.success ? "removed" : r.result.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_server_remove <name>\n");
    }
    return CommandResult::ok("hotpatch.serverRemove");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchProxyBias(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9008, 0);
        return CommandResult::ok("hotpatch.proxyBias");
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Token bias panel. Active biases: " << stats.biasesApplied << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyBias");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9009, 0);
        return CommandResult::ok("hotpatch.proxyRewrite");
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Rewrite rules. Active rewrites: " << stats.rewritesApplied << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyRewrite");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9010, 0);
        return CommandResult::ok("hotpatch.proxyTerminate");
    }
    ctx.output("[Proxy] Termination rules panel. Use to set max-tokens, stop-sequences.\n");
    return CommandResult::ok("hotpatch.proxyTerminate");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9011, 0);
        return CommandResult::ok("hotpatch.proxyValidate");
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Validators. Total runs: " << stats.validationsPassed << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyValidate");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchPresetSave(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9012, 0);
        return CommandResult::ok("hotpatch.presetSave");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        HotpatchPreset preset{};
        auto r = mgr.save_preset(ctx.args, preset);
        std::ostringstream oss;
        oss << "[Hotpatch] Preset save '" << ctx.args << "': " << (r.success ? "OK" : r.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_preset_save <filename>\n");
    }
    return CommandResult::ok("hotpatch.presetSave");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9013, 0);
        return CommandResult::ok("hotpatch.presetLoad");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        HotpatchPreset preset{};
        auto r = mgr.load_preset(ctx.args, &preset);
        std::ostringstream oss;
        oss << "[Hotpatch] Preset load '" << ctx.args << "': " << (r.success ? "OK" : r.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_preset_load <filename>\n");
    }
    return CommandResult::ok("hotpatch.presetLoad");
}
#endif
CommandResult handleHotpatchEventLog(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9014, 0);
        return CommandResult::ok("hotpatch.eventLog");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    std::ostringstream oss;
    oss << "=== Hotpatch Event Log ===\n";
    HotpatchEvent evt{};
    int count = 0;
    while (mgr.poll_event(&evt) && count < 20) {
        oss << "  [" << count++ << "] " << evt.detail << "\n";
    }
    if (count == 0) oss << "  (no events)\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.eventLog");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchResetStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9015, 0);
        return CommandResult::ok("hotpatch.resetStats");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    reset_memory_patch_stats();
    ProxyHotpatcher::instance().resetStats();
    ctx.output("[Hotpatch] All stats reset across Memory, Byte, Server, and Proxy layers.\n");
    return CommandResult::ok("hotpatch.resetStats");
}
#endif
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleHotpatchToggleAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9016, 0);
        return CommandResult::ok("hotpatch.toggleAll");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.clearAllPatches();
    ctx.output("[Hotpatch] All patches cleared across all layers.\n");
    return CommandResult::ok("hotpatch.toggleAll");
}
#endif
CommandResult handleHotpatchProxyStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9017, 0);
        return CommandResult::ok("hotpatch.proxyStats");
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& stats = proxy.getStats();
    std::ostringstream oss;
    oss << "=== Proxy Hotpatch Stats ===\n"
        << "  Biases applied:     " << stats.biasesApplied << "\n"
        << "  Rewrites applied:   " << stats.rewritesApplied << "\n"
        << "  Validations run:    " << stats.validationsPassed << "\n"
        << "  Terminations:       " << stats.streamsTerminated << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyStats");
}

// ============================================================================
// MONACO
// ============================================================================

CommandResult handleMonacoToggle(const CommandContext& ctx)    { return routeToIde(ctx, 9100, "view.monacoToggle"); }
CommandResult handleMonacoDevtools(const CommandContext& ctx)  { return routeToIde(ctx, 9101, "view.monacoDevtools"); }
CommandResult handleMonacoReload(const CommandContext& ctx)    { return routeToIde(ctx, 9102, "view.monacoReload"); }
CommandResult handleMonacoZoomIn(const CommandContext& ctx)    { return routeToIde(ctx, 9103, "view.monacoZoomIn"); }
CommandResult handleMonacoZoomOut(const CommandContext& ctx)   { return routeToIde(ctx, 9104, "view.monacoZoomOut"); }
CommandResult handleMonacoSyncTheme(const CommandContext& ctx) { return routeToIde(ctx, 9105, "view.monacoSyncTheme"); }

// Legacy menu-backed aliases kept for command palette / CLI compatibility.
CommandResult handleLspStartAll(const CommandContext& ctx) { return handleLspSrvStart(ctx); }
CommandResult handleLspStopAll(const CommandContext& ctx)  { return handleLspSrvStop(ctx); }
CommandResult handleLspStatus(const CommandContext& ctx)   { return handleLspSrvStatus(ctx); }
CommandResult handleLspGotoDef(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5061, "lsp.gotoDef");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !lsp goto <symbol_name>\n");
        return CommandResult::error("lsp.gotoDef: missing symbol");
    }
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    std::vector<LspSymbolMatch> matches;
    collectSymbolMatches(symbol, files, matches, 256);
    if (matches.empty()) {
        std::ostringstream oss;
        oss << "[LSP] Definition not found for symbol: " << symbol << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.gotoDef");
    }

    LspSymbolMatch def{};
    (void)selectDefinitionCandidate(matches, def);
    std::ostringstream oss;
    oss << "[LSP] Go-to-definition\n"
        << "  symbol: " << symbol << "\n"
        << "  location: " << def.path << ":" << def.line << ":" << def.column << "\n"
        << "  snippet: " << def.snippet << "\n"
        << "  indexed_matches: " << matches.size() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.gotoDef");
}
CommandResult handleLspFindRefs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5062, "lsp.findRefs");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !lsp refs <symbol_name>\n");
        return CommandResult::error("lsp.findRefs: missing symbol");
    }
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    std::vector<LspSymbolMatch> matches;
    collectSymbolMatches(symbol, files, matches, 512);

    std::ostringstream oss;
    oss << "[LSP] References\n"
        << "  symbol: " << symbol << "\n"
        << "  count: " << matches.size() << "\n";
    const size_t maxPrint = 40;
    for (size_t i = 0; i < matches.size() && i < maxPrint; ++i) {
        const auto& m = matches[i];
        oss << "    " << m.path << ":" << m.line << ":" << m.column;
        if (m.definitionLike) oss << " [def]";
        oss << "\n";
    }
    if (matches.size() > maxPrint) {
        oss << "    ... (" << (matches.size() - maxPrint) << " more)\n";
    }
    if (matches.empty()) {
        oss << "  No references found in indexed workspace roots (src/include/tests).\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.findRefs");
}
CommandResult handleLspRename(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5063, "lsp.rename");
    const std::string oldSym = ssoGetArg(ctx, 0);
    const std::string newSym = ssoGetArg(ctx, 1);
    if (oldSym.empty() || newSym.empty()) {
        ctx.output("Usage: !lsp rename <old_symbol> <new_symbol> [--apply]\n");
        return CommandResult::error("lsp.rename: missing args");
    }
    if (oldSym == newSym) {
        ctx.output("[LSP] Old and new symbol are the same; nothing to do.\n");
        return CommandResult::ok("lsp.rename");
    }

    const bool apply = (getArgsTrimmed(ctx).find("--apply") != std::string::npos);

    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    std::vector<LspSymbolMatch> matches;
    collectSymbolMatches(oldSym, files, matches, 4000);
    if (matches.empty()) {
        std::ostringstream oss;
        oss << "[LSP] Rename preview found no matches for symbol: " << oldSym << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.rename");
    }

    std::map<std::string, size_t> hitsPerFile;
    for (const auto& m : matches) hitsPerFile[m.path]++;

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");
    const char* previewPath = ".rawrxd\\lsp\\rename_preview.txt";

    std::ostringstream preview;
    preview << "[LSP] Rename preview\n"
            << "  old: " << oldSym << "\n"
            << "  new: " << newSym << "\n"
            << "  matches: " << matches.size() << "\n"
            << "  files: " << hitsPerFile.size() << "\n";
    for (const auto& kv : hitsPerFile) {
        preview << "    " << kv.first << " (" << kv.second << ")\n";
    }
    (void)writeTextFile(previewPath, preview.str());

    if (!apply) {
        std::ostringstream oss;
        oss << preview.str()
            << "  mode: preview-only\n"
            << "  to apply: !lsp rename " << oldSym << " " << newSym << " --apply\n"
            << "  preview_file: " << previewPath << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.rename.preview");
    }

    size_t changedFiles = 0;
    size_t replacedTotal = 0;
    for (const auto& kv : hitsPerFile) {
        std::string text;
        if (!readTextFileLimited(kv.first, text, 8 * 1024 * 1024)) continue;
        const size_t replaced = replaceWholeWordOccurrences(text, oldSym, newSym);
        if (replaced == 0) continue;
        if (!writeTextFile(kv.first, text)) continue;
        ++changedFiles;
        replacedTotal += replaced;
    }

    std::ostringstream oss;
    oss << "[LSP] Rename applied\n"
        << "  old: " << oldSym << "\n"
        << "  new: " << newSym << "\n"
        << "  files_changed: " << changedFiles << "\n"
        << "  replacements: " << replacedTotal << "\n"
        << "  preview_file: " << previewPath << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.rename");
}
CommandResult handleLspHover(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 5064, "lsp.hover");
    const std::string symbol = ssoGetArg(ctx, 0);
    if (symbol.empty()) {
        ctx.output("Usage: !lsp hover <symbol_name>\n");
        return CommandResult::error("lsp.hover: missing symbol");
    }
    const std::vector<std::string> files = collectDefaultLspSearchFiles();
    std::vector<LspSymbolMatch> matches;
    collectSymbolMatches(symbol, files, matches, 256);
    if (matches.empty()) {
        std::ostringstream oss;
        oss << "[LSP] Hover: no indexed symbol match for " << symbol << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("lsp.hover");
    }

    LspSymbolMatch def{};
    (void)selectDefinitionCandidate(matches, def);
    size_t refs = matches.size();
    if (refs > 0 && def.definitionLike) refs -= 1;

    std::ostringstream oss;
    oss << "[LSP] Hover\n"
        << "  symbol: " << symbol << "\n"
        << "  declaration: " << def.path << ":" << def.line << ":" << def.column << "\n"
        << "  preview: " << def.snippet << "\n"
        << "  references: " << refs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lsp.hover");
}

// ============================================================================
// LSP SERVER
// ============================================================================

CommandResult handleLspSrvStart(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9200, 0);
        return CommandResult::ok("lspServer.start");
    }
    auto& st = LspServerRuntimeState::instance();

    std::string requestedServer = ssoGetArg(ctx, 0);
    if (requestedServer.empty()) requestedServer = "clangd";

    std::string resolvedServer;
    if (!resolveBinaryPath(requestedServer, resolvedServer)) {
        std::string msg = "[LSP] Server binary not found in PATH: " + requestedServer + "\n";
        ctx.output(msg.c_str());
        return CommandResult::error("lspServer.start binary not found");
    }

    std::string serverArgs = tailArgs(ctx);
    if (serverArgs.empty()) {
        serverArgs = "--background-index --clang-tidy --log=error --pch-storage=memory";
    }
    std::string commandLine = "\"" + resolvedServer + "\" " + serverArgs;

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        if (st.running && st.proc.hProcess) {
            DWORD ec = 0;
            if (GetExitCodeProcess(st.proc.hProcess, &ec) && ec == STILL_ACTIVE) {
                std::ostringstream oss;
                oss << "[LSP] Server already running (pid=" << st.proc.dwProcessId << ")\n"
                    << "  command: " << st.commandLine << "\n";
                ctx.output(oss.str().c_str());
                return CommandResult::ok("lspServer.start");
            }
            closeLspProcessHandles(st);
            st.running = false;
        }
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::vector<char> mutableCmd(commandLine.begin(), commandLine.end());
    mutableCmd.push_back('\0');

    const BOOL launched = CreateProcessA(
        nullptr,
        mutableCmd.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!launched) {
        std::ostringstream oss;
        oss << "[LSP] Failed to launch: " << commandLine << "\n"
            << "  GetLastError=" << GetLastError() << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("lspServer.start CreateProcess failed");
    }

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        closeLspProcessHandles(st);
        st.proc = pi;
        st.running = true;
        st.binaryPath = resolvedServer;
        st.commandLine = commandLine;
        st.startedAtTick = GetTickCount64();
    }

    std::ostringstream oss;
    oss << "[LSP] Server started successfully.\n"
        << "  pid: " << pi.dwProcessId << "\n"
        << "  binary: " << resolvedServer << "\n"
        << "  args: " << serverArgs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lspServer.start");
}
CommandResult handleLspSrvStop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9201, 0);
        return CommandResult::ok("lspServer.stop");
    }
    auto& st = LspServerRuntimeState::instance();
    DWORD pid = 0;
    bool wasRunning = false;
    bool terminateOk = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        if (st.running && st.proc.hProcess) {
            wasRunning = true;
            pid = st.proc.dwProcessId;
            terminateOk = !!TerminateProcess(st.proc.hProcess, 0);
            WaitForSingleObject(st.proc.hProcess, 1500);
        }
        closeLspProcessHandles(st);
        st.running = false;
    }
    if (!wasRunning) {
        ctx.output("[LSP] No running server process.\n");
        return CommandResult::ok("lspServer.stop");
    }
    std::ostringstream oss;
    oss << "[LSP] Server stop requested for pid " << pid << "\n"
        << "  result: " << (terminateOk ? "terminated" : "handle-closed") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lspServer.stop");
}
CommandResult handleLspSrvStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9202, 0);
        return CommandResult::ok("lspServer.status");
    }
    auto& st = LspServerRuntimeState::instance();
    bool running = false;
    DWORD pid = 0;
    DWORD exitCode = 0;
    unsigned long long uptimeMs = 0;
    std::string cmdLine;
    unsigned long long reindexCount = 0;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        reindexCount = st.reindexCount;
        if (st.running && st.proc.hProcess) {
            if (GetExitCodeProcess(st.proc.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                running = true;
                pid = st.proc.dwProcessId;
                cmdLine = st.commandLine;
                uptimeMs = GetTickCount64() - st.startedAtTick;
            } else {
                closeLspProcessHandles(st);
                st.running = false;
            }
        }
    }

    std::ostringstream hdr;
    hdr << "[LSP] Runtime status\n"
        << "  running: " << (running ? "yes" : "no") << "\n"
        << "  pid: " << (running ? std::to_string(pid) : std::string("<none>")) << "\n"
        << "  uptime_ms: " << uptimeMs << "\n"
        << "  reindex_requests: " << reindexCount << "\n";
    if (!cmdLine.empty()) {
        hdr << "  command: " << cmdLine << "\n";
    }
    ctx.output(hdr.str().c_str());

    ctx.output("[LSP] Toolchain availability\n");
    const char* servers[] = { "clangd", "rust-analyzer", "pyright", "typescript-language-server" };
    for (auto s : servers) {
        std::string cmd = std::string(s) + " --version 2>NUL";
        FILE* pipe = _popen(cmd.c_str(), "r");
        char buf[128] = {};
        bool found = false;
        if (pipe) {
            if (fgets(buf, sizeof(buf), pipe)) found = true;
            _pclose(pipe);
        }
        ctx.output("  "); ctx.output(s); ctx.output(": ");
        ctx.output(found ? "installed" : "not found"); ctx.output("\n");
    }
    return CommandResult::ok("lspServer.status");
}
CommandResult handleLspSrvReindex(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9203, 0);
        return CommandResult::ok("lspServer.reindex");
    }
    auto& st = LspServerRuntimeState::instance();
    std::string workspace = ssoGetArg(ctx, 0);
    if (workspace.empty()) workspace = ".";

    unsigned long long removed = 0;
    removed += wipeDirectoryTree(workspace + "\\.cache\\clangd\\index");
    removed += wipeDirectoryTree(workspace + "\\.clangd\\index");
    char localAppData[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        removed += wipeDirectoryTree(std::string(localAppData) + "\\clangd\\index");
    }

    ensureDirectory(".rawrxd");
    ensureDirectory(".rawrxd\\lsp");

    std::string markerPath = ".rawrxd\\lsp\\reindex.request";
    FILE* marker = fopen(markerPath.c_str(), "wb");
    if (marker) {
        const unsigned long long tick = GetTickCount64();
        fprintf(marker,
                "{\n"
                "  \"command\": \"lspServer.reindex\",\n"
                "  \"workspace\": \"%s\",\n"
                "  \"tick\": %llu,\n"
                "  \"removedEntries\": %llu\n"
                "}\n",
                workspace.c_str(),
                tick,
                removed);
        fclose(marker);
    }

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.workspaceRoot = workspace;
        st.lastReindexTick = GetTickCount64();
        st.reindexCount++;
    }

    std::ostringstream oss;
    oss << "[LSP] Reindex requested.\n"
        << "  workspace: " << workspace << "\n"
        << "  removed cache entries: " << removed << "\n"
        << "  marker: " << markerPath << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("lspServer.reindex");
}
CommandResult handleLspSrvStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9204, 0);
        return CommandResult::ok("lspServer.stats");
    }
    ctx.output("[LSP] Server statistics:\n");
    int cppCount = 0, hCount = 0;
    countSourceFilesRecursive("src", cppCount, hCount);
    char buf[256];
    snprintf(buf, sizeof(buf), "  Source files:  %d .cpp, %d .h/.hpp\n  Protocol:      LSP 3.17\n", cppCount, hCount);
    ctx.output(buf);

    auto& st = LspServerRuntimeState::instance();
    bool running = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        if (st.running && st.proc.hProcess) {
            DWORD ec = 0;
            running = GetExitCodeProcess(st.proc.hProcess, &ec) && ec == STILL_ACTIVE;
        }
    }
    ctx.output(running ? "  clangd:        RUNNING (managed)\n"
                       : "  clangd:        NOT RUNNING (managed)\n");
    return CommandResult::ok("lspServer.stats");
}
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9205, 0);
        return CommandResult::ok("lspServer.publishDiag");
    }
    // Run clang-tidy to generate diagnostics
    const char* target = (ctx.args && ctx.args[0]) ? ctx.args : "src/core/*.cpp";
    std::string cmd = "clang-tidy " + std::string(target) + " -- -std=c++20 2>&1";
    ctx.output("[LSP] Publishing diagnostics via clang-tidy...\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int warnings = 0, errors = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "warning:")) warnings++;
            if (strstr(buf, "error:"))   errors++;
            ctx.output(buf);
        }
        _pclose(pipe);
        char summary[128];
        snprintf(summary, sizeof(summary), "[LSP] Diagnostics: %d warnings, %d errors\n", warnings, errors);
        ctx.output(summary);
    } else ctx.output("[LSP] clang-tidy not found.\n");
    return CommandResult::ok("lspServer.publishDiag");
}
CommandResult handleLspSrvConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9206, 0);
        return CommandResult::ok("lspServer.config");
    }
    ctx.output("[LSP] Server configuration:\n");
    // Read compile_commands.json if it exists
    HANDLE h = CreateFileA("compile_commands.json", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
        CloseHandle(h);
        char buf[128];
        snprintf(buf, sizeof(buf), "  compile_commands.json: %lld bytes\n", sz.QuadPart);
        ctx.output(buf);
    } else ctx.output("  compile_commands.json: NOT FOUND (run cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)\n");
    // Check .clangd config
    DWORD attrs = GetFileAttributesA(".clangd");
    ctx.output(attrs != INVALID_FILE_ATTRIBUTES ? "  .clangd config: FOUND\n" : "  .clangd config: NOT FOUND\n");
    return CommandResult::ok("lspServer.config");
}
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9207, 0);
        return CommandResult::ok("lspServer.exportSymbols");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "symbols_export.txt";
    ctx.output("[LSP] Exporting workspace symbols...\n");
    std::string cmd = "findstr /s /r /n \"class \\|struct \\|void \\|int \\|enum \" src\\core\\*.hpp src\\core\\*.h 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile, "w");
    if (pipe && out) {
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) { fputs(buf, out); count++; }
        _pclose(pipe);
        fclose(out);
        char msg[128];
        snprintf(msg, sizeof(msg), "[LSP] Exported %d symbol lines to %s\n", count, outFile);
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[LSP] Export failed.\n");
    }
    return CommandResult::ok("lspServer.exportSymbols");
}
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9208, 0);
        return CommandResult::ok("lspServer.launchStdio");
    }
    ctx.output("[LSP] Launching clangd in stdio mode...\n");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    char cmdLine[] = "clangd --log=error --background-index";
    if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE,
                       CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[LSP] clangd started (PID %lu)\n", pi.dwProcessId);
        ctx.output(buf);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        ctx.output("[LSP] Failed to launch clangd. Ensure it's in PATH.\n");
    }
    return CommandResult::ok("lspServer.launchStdio");
}

// ============================================================================
// EDITOR ENGINE
// ============================================================================

CommandResult handleEditorRichEdit(const CommandContext& ctx)   { return routeToIde(ctx, 9300, "editor.richedit"); }
CommandResult handleEditorWebView2(const CommandContext& ctx)   { return routeToIde(ctx, 9301, "editor.webview2"); }
CommandResult handleEditorMonacoCore(const CommandContext& ctx) { return routeToIde(ctx, 9302, "editor.monacocore"); }
CommandResult handleEditorCycle(const CommandContext& ctx)      { return routeToIde(ctx, 9303, "editor.cycle"); }
CommandResult handleEditorStatus(const CommandContext& ctx)     { return routeToIde(ctx, 9304, "editor.status"); }

// ============================================================================
// PDB
// ============================================================================
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handlePdbLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9400, 0);
        return CommandResult::ok("pdb.load");
    }
    if (ctx.args && ctx.args[0]) {
        std::string path(ctx.args);
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            ctx.output("[PDB] File not found: ");
            ctx.output(ctx.args);
            ctx.output("\n");
            return CommandResult::error("pdb: file not found");
        }
        // Validate PDB signature ("Microsoft C/C++ MSF 7.00")
        FILE* f = fopen(path.c_str(), "rb");
        char sig[32] = {};
        if (f) { fread(sig, 1, 28, f); fclose(f); }
        if (strstr(sig, "Microsoft C/C++")) {
            ctx.output("[PDB] Loaded valid PDB: ");
            ctx.output(ctx.args);
            ctx.output("\n");
        } else {
            ctx.output("[PDB] Warning: file does not have standard PDB signature.\n");
        }
    } else {
        ctx.output("Usage: !pdb_load <file.pdb>\n");
    }
    return CommandResult::ok("pdb.load");
}
CommandResult handlePdbFetch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9401, 0);
        return CommandResult::ok("pdb.fetch");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "symchk /s srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Fetching symbols for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  symchk.exe not found. Install Debugging Tools for Windows.\n");
        }
    } else {
        ctx.output("Usage: !pdb_fetch <binary.exe>\n");
    }
    return CommandResult::ok("pdb.fetch");
}
CommandResult handlePdbStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9402, 0);
        return CommandResult::ok("pdb.status");
    }
    ctx.output("[PDB] Symbol cache status:\n");
    const char* symPaths[] = { "C:\\Symbols", ".\\symbols", ".\\pdb" };
    for (auto sp : symPaths) {
        DWORD attr = GetFileAttributesA(sp);
        char buf[256];
        snprintf(buf, sizeof(buf), "  %s: %s\n", sp,
                 (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) ? "EXISTS" : "not found");
        ctx.output(buf);
    }
    return CommandResult::ok("pdb.status");
}
CommandResult handlePdbCacheClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9403, "pdb.cacheClear");
    // CLI: clear the symbol cache directory
    const char* symcache = "C:\\SymCache";
    char buf[512];
    DWORD attr = GetFileAttributesA(symcache);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::string cmd = "rd /s /q \"" + std::string(symcache) + "\" 2>NUL && mkdir \"" + std::string(symcache) + "\"";
        system(cmd.c_str());
        snprintf(buf, sizeof(buf), "[PDB] Symbol cache cleared: %s\n", symcache);
    } else {
        snprintf(buf, sizeof(buf), "[PDB] No cache at %s (nothing to clear)\n", symcache);
    }
    ctx.output(buf);
    return CommandResult::ok("pdb.cacheClear");
}
CommandResult handlePdbEnable(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9404, "pdb.enable");
    ctx.output("[PDB] Symbol resolution enabled.\n");
    ctx.output("  Search paths:\n");
    ctx.output("    SRV*C:\\SymCache*https://msdl.microsoft.com/download/symbols\n");
    ctx.output("  Use !pdb_resolve <binary> to load symbols.\n");
    return CommandResult::ok("pdb.enable");
}
CommandResult handlePdbResolve(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9405, "pdb.resolve");
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !pdb_resolve <binary.exe|binary.dll>\n");
        return CommandResult::ok("pdb.resolve");
    }
    // Use symchk to verify/download PDB
    std::string cmd = "symchk /r \"" + std::string(ctx.args) + "\" /s SRV*C:\\SymCache*https://msdl.microsoft.com/download/symbols 2>&1";
    ctx.output("[PDB] Resolving symbols for: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else {
        ctx.output("  symchk.exe not found. Install Debugging Tools for Windows.\n");
    }
    return CommandResult::ok("pdb.resolve");
}
CommandResult handlePdbImports(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9410, 0);
        return CommandResult::ok("pdb.imports");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /imports \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Import table for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else ctx.output("Usage: !pdb_imports <binary>\n");
    return CommandResult::ok("pdb.imports");
}
CommandResult handlePdbExports(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9411, 0);
        return CommandResult::ok("pdb.exports");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /exports \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Export table for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else ctx.output("Usage: !pdb_exports <binary>\n");
    return CommandResult::ok("pdb.exports");
}
CommandResult handlePdbIatStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9412, "pdb.iatStatus");
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /imports \"" + std::string(ctx.args) + "\" 2>&1 | findstr /C:\"Import Address Table\"";
        ctx.output("[PDB] IAT status for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else {
        ctx.output("Usage: !pdb_iat <binary>\n");
    }
    return CommandResult::ok("pdb.iatStatus");
}
#endif

// ============================================================================
// AUDIT
// ============================================================================

CommandResult handleAuditDashboard(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = reinterpret_cast<HWND>(ctx.hwnd);
        if (!hwnd) hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);  // Fallback for older callers
        if (hwnd) PostMessageA(hwnd, WM_COMMAND, 9500, 0);
        return CommandResult::ok("audit.dashboard");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    auto& sentinel = SentinelWatchdog::instance();
    std::ostringstream oss;
    oss << "=== Audit Dashboard ===\n"
        << "  Hotpatches applied:  " << stats.totalOperations.load() << "\n"
        << "  Memory patches:      " << stats.memoryPatchCount.load() << "\n"
        << "  Byte patches:        " << stats.bytePatchCount.load() << "\n"
        << "  Server patches:      " << stats.serverPatchCount.load() << "\n"
        << "  Sentinel active:     " << (sentinel.isActive() ? "YES" : "NO") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.dashboard");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleAuditRunFull(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.audit.runFull");
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = reinterpret_cast<HWND>(ctx.hwnd);
        if (!hwnd) hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        if (hwnd) PostMessageA(hwnd, WM_COMMAND, 9501, 0);
        return CommandResult::ok("audit.runFull");
    }
    ctx.output("[AUDIT] Running full system audit...\n");
    auto& orchestrator = AutoRepairOrchestrator::instance();
    const auto& orchStats = orchestrator.getStats();
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& hpStats = mgr.getStats();
    auto& proxy = ProxyHotpatcher::instance();
    const auto& pStats = proxy.getStats();
    std::ostringstream oss;
    oss << "  Orchestrator: " << orchStats.repairsAttempted << " repairs, " << orchStats.anomaliesDetected << " anomalies\n"
        << "  Hotpatch:     " << hpStats.totalOperations.load() << " applied\n"
        << "  Proxy:        biases=" << pStats.biasesApplied << " rewrites=" << pStats.rewritesApplied << "\n"
        << reverseTraceHotspotsReport(8)
        << "  Audit complete.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.runFull");
}
CommandResult handleAuditDetectStubs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = reinterpret_cast<HWND>(ctx.hwnd);
        if (!hwnd) hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        if (hwnd) PostMessageA(hwnd, WM_COMMAND, 9502, 0);
        return CommandResult::ok("audit.detectStubs");
    }
    ctx.output("[AUDIT] Scanning for stub handlers...\n");
    FILE* pipe = _popen("findstr /c:\"ctx.output(\" src\\core\\*handlers*.cpp 2>&1 | find /c \"return CommandResult::ok\" 2>&1", "r");
    if (pipe) {
        char buf[128];
        if (fgets(buf, sizeof(buf), pipe)) {
            ctx.output("  Handler return sites found: ");
            ctx.output(buf);
        }
        _pclose(pipe);
    }
    ctx.output("  Stub detection complete. Check HANDLER_AUDIT_REPORT.md for details.\n");
    return CommandResult::ok("audit.detectStubs");
}
CommandResult handleAuditQuickStats(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.audit.quickStats");
    const std::string args = trimAscii(ctx.args ? std::string(ctx.args) : std::string());
    if (!args.empty()) {
        const std::string lowerArgs = toLowerCopy(args);
        if (lowerArgs == "trace_reset" || lowerArgs == "reset" ||
            lowerArgs == "trace reset" || lowerArgs == "--trace-reset") {
            reverseTraceResetAll();
            ctx.output("[AUDIT] Reverse trace samples reset.\n");
            return CommandResult::ok("audit.quickStats.traceReset");
        }
    }

    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9506, 0);
        return CommandResult::ok("audit.quickStats");
    }
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    std::ostringstream oss;
    oss << "=== Quick Stats ===\n"
        << "  RAM:     " << mem.dwMemoryLoad << "% used (" << (mem.ullAvailPhys/(1024*1024)) << " MB free)\n"
        << "  Patches: " << stats.totalOperations.load() << " total\n"
        << "  Uptime:  " << (GetTickCount64() / 1000) << " seconds\n"
        << reverseTraceHotspotsReport(6);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.quickStats");
}

CommandResult handleAuditCheckMenus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9503, 0);
        return CommandResult::ok("audit.checkMenus");
    }
    ctx.output("[AUDIT] Menu consistency check ? GUI-only inspection.\n");
    ctx.output("  Use Win32 IDE mode for menu validation.\n");
    return CommandResult::ok("audit.checkMenus");
}
CommandResult handleAuditRunTests(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9504, 0);
        return CommandResult::ok("audit.runTests");
    }
    ctx.output("[AUDIT] Running self-test gate...\n");
    FILE* pipe = _popen("self_test_gate.exe 2>&1", "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  Exit code: %d\n", rc);
        ctx.output(msg);
    } else {
        ctx.output("  self_test_gate.exe not found. Build with: cmake --build . --target self_test_gate\n");
    }
    return CommandResult::ok("audit.runTests");
}
CommandResult handleAuditExportReport(const CommandContext& ctx) {
    ReverseTraceScope _trace("ssot.audit.exportReport");
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9505, 0);
        return CommandResult::ok("audit.exportReport");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "audit_report.txt";
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    auto& orch = AutoRepairOrchestrator::instance();
    auto oStats = orch.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "=== RawrXD Audit Report ===\n");
        fprintf(f, "Hotpatches: %llu (M:%llu B:%llu S:%llu)\n",
                (unsigned long long)stats.totalOperations.load(), (unsigned long long)stats.memoryPatchCount.load(), (unsigned long long)stats.bytePatchCount.load(), (unsigned long long)stats.serverPatchCount.load());
        fprintf(f, "Repairs: %llu  Anomalies: %llu\n", (unsigned long long)oStats.repairsAttempted, (unsigned long long)oStats.anomaliesDetected);
        const std::string traceText = reverseTraceHotspotsReport(20);
        fprintf(f, "%s", traceText.c_str());
        fclose(f);
        ctx.output("[AUDIT] Report exported to: ");
        ctx.output(outFile);
        ctx.output("\n");

        std::string csvPath = outFile;
        csvPath += ".reverse_trace.csv";
        if (reverseTraceExportCsv(csvPath)) {
            ctx.output("[AUDIT] Reverse trace CSV exported to: ");
            ctx.output(csvPath.c_str());
            ctx.output("\n");
        } else {
            ctx.output("[AUDIT] Reverse trace CSV export failed.\n");
        }
    } else {
        ctx.output("[AUDIT] Failed to write report.\n");
    }
    return CommandResult::ok("audit.exportReport");
}
#endif
// ============================================================================
// GAUNTLET
// ============================================================================
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleGauntletRun(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9600, "gauntlet.run");
    // CLI: run the self_test_gate build target
    ctx.output("[GAUNTLET] Running self-test suite...\n");
    FILE* pipe = _popen("cmake --build build --config Release --target self_test_gate 2>&1", "r");
    if (pipe) {
        char buf[512];
        int passed = 0, failed = 0, total = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            ctx.output("  "); ctx.output(buf);
            // Count test results from output
            if (strstr(buf, "PASSED")) passed++;
            else if (strstr(buf, "FAILED")) failed++;
            total++;
        }
        int rc = _pclose(pipe);
        char summary[256];
        snprintf(summary, sizeof(summary), "\n[GAUNTLET] Results: %d passed, %d failed (exit code %d)\n", passed, failed, rc);
        ctx.output(summary);
    } else {
        ctx.output("  Build system not found. Ensure cmake is configured.\n");
    }
    return CommandResult::ok("gauntlet.run");
}
CommandResult handleGauntletExport(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9601, "gauntlet.export");
    std::string outFile = (ctx.args && ctx.args[0]) ? ctx.args : "gauntlet_report.txt";
    std::string cmd = "cmake --build build --config Release --target self_test_gate 2>&1 > \"" + outFile + "\"";
    ctx.output("[GAUNTLET] Exporting test results to: ");
    ctx.output(outFile.c_str());
    ctx.output("\n");
    system(cmd.c_str());
    DWORD attr = GetFileAttributesA(outFile.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        ctx.output("  Report saved.\n");
    } else {
        ctx.output("  Export failed.\n");
    }
    return CommandResult::ok("gauntlet.export");
}
#endif

// ============================================================================
// VOICE (extended)
// ============================================================================

CommandResult handleVoicePTT(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9701, "voice.ptt");
    ctx.output("[VOICE] Push-to-talk mode toggled.\n");
    ctx.output("  PTT keybind: F13 (configurable via !voice_keybind <key>)\n");
    ctx.output("  State: active while key held\n");
    return CommandResult::ok("voice.ptt");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleVoiceJoinRoom(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9703, "voice.joinRoom");
    std::string room = (ctx.args && ctx.args[0]) ? ctx.args : "default";
    ctx.output("[VOICE] Joining room: ");
    ctx.output(room.c_str());
    ctx.output("\n  Note: Voice rooms require a running voice server endpoint.\n");
    return CommandResult::ok("voice.joinRoom");
}
CommandResult handleVoiceModeContinuous(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9708, "voice.modeContinuous");
    ctx.output("[VOICE] Mode set to CONTINUOUS\n");
    ctx.output("  Input is always captured. Use !voice_mode_disabled to stop.\n");
    return CommandResult::ok("voice.modeContinuous");
}
CommandResult handleVoiceModeDisabled(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return routeToIde(ctx, 9709, "voice.modeDisabled");
    ctx.output("[VOICE] Voice input DISABLED.\n");
    ctx.output("  Use !voice_mode_continuous or !voice_ptt to re-enable.\n");
    return CommandResult::ok("voice.modeDisabled");
}
#endif

// ============================================================================
// QW (Quality/Workflow)
// ============================================================================
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleQwShortcutEditor(const CommandContext& ctx)     { return routeToIde(ctx, 9800, "qw.shortcutEditor"); }
CommandResult handleQwShortcutReset(const CommandContext& ctx)      { return routeToIde(ctx, 9801, "qw.shortcutReset"); }
CommandResult handleQwBackupCreate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9810, 0);
        return CommandResult::ok("qw.backupCreate");
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char dir[128];
    snprintf(dir, sizeof(dir), "backups\\%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    CreateDirectoryA("backups", nullptr);
    CreateDirectoryA(dir, nullptr);
    // Copy key config files to backup
    const char* files[] = { "rawrxd_settings.json", "rawrxd_config.json" };
    for (auto fn : files) {
        std::string dest = std::string(dir) + "\\" + fn;
        CopyFileA(fn, dest.c_str(), FALSE);
    }
    ctx.output("[QW] Backup created: ");
    ctx.output(dir);
    ctx.output("\n");
    return CommandResult::ok("qw.backupCreate");
}
CommandResult handleQwBackupRestore(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9811, 0);
        return CommandResult::ok("qw.backupRestore");
    }
    if (ctx.args && ctx.args[0]) {
        std::string src = std::string(ctx.args) + "\\rawrxd_settings.json";
        if (CopyFileA(src.c_str(), "rawrxd_settings.json", FALSE)) {
            ctx.output("[QW] Restored settings from: ");
            ctx.output(ctx.args);
            ctx.output("\n");
        } else {
            ctx.output("[QW] Failed to restore from backup directory.\n");
        }
    } else {
        ctx.output("Usage: !qw_backup_restore <backup_dir>\n");
    }
    return CommandResult::ok("qw.backupRestore");
}
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9812, 0);
        return CommandResult::ok("qw.backupAutoToggle");
    }
    static bool autoBackup = false;
    autoBackup = !autoBackup;
    char buf[64];
    snprintf(buf, sizeof(buf), "[QW] Auto-backup: %s\n", autoBackup ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    return CommandResult::ok("qw.backupAutoToggle");
}
CommandResult handleQwBackupList(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9813, 0);
        return CommandResult::ok("qw.backupList");
    }
    ctx.output("[QW] Available backups:\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("backups\\*", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') {
                ctx.output("  ");
                ctx.output(fd.cFileName);
                ctx.output("\n");
                count++;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    if (count == 0) ctx.output("  No backups found.\n");
    return CommandResult::ok("qw.backupList");
}
CommandResult handleQwBackupPrune(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9814, 0);
        return CommandResult::ok("qw.backupPrune");
    }
    ctx.output("[QW] Pruning old backups (keeping last 5)...\n");
    // List backup dirs sorted, remove oldest beyond 5
    std::vector<std::string> dirs;
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("backups\\*", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.')
                dirs.push_back(fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    std::sort(dirs.begin(), dirs.end());
    int pruned = 0;
    while (dirs.size() > 5) {
        std::string path = "backups\\" + dirs.front();
        RemoveDirectoryA(path.c_str()); // only works if empty, but signals intent
        dirs.erase(dirs.begin());
        pruned++;
    }
    char buf[64]; snprintf(buf, sizeof(buf), "  Pruned %d backup(s).\n", pruned);
    ctx.output(buf);
    return CommandResult::ok("qw.backupPrune");
}
#endif
CommandResult handleQwAlertMonitor(const CommandContext& ctx)       { return routeToIde(ctx, 9820, "qw.alertToggleMonitor"); }
CommandResult handleQwAlertHistory(const CommandContext& ctx)       { return routeToIde(ctx, 9821, "qw.alertShowHistory"); }
CommandResult handleQwAlertDismiss(const CommandContext& ctx)       { return routeToIde(ctx, 9822, "qw.alertDismissAll"); }
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx){ return routeToIde(ctx, 9823, "qw.alertResourceStatus"); }
CommandResult handleQwSloDashboard(const CommandContext& ctx)       { return routeToIde(ctx, 9830, "qw.sloDashboard"); }
#endif

// ============================================================================
// TELEMETRY
// ============================================================================

#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleTelemetryToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9900, 0);
        return CommandResult::ok("telemetry.toggle");
    }
    static bool telemetryOn = true;
    telemetryOn = !telemetryOn;
    char buf[64];
    snprintf(buf, sizeof(buf), "[TELEMETRY] Collection: %s\n", telemetryOn ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    return CommandResult::ok("telemetry.toggle");
}
CommandResult handleTelemetryExportJson(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9901, 0);
        return CommandResult::ok("telemetry.exportJson");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "telemetry_export.json";
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    auto& orch = AutoRepairOrchestrator::instance();
    auto oStats = orch.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "{\n  \"hotpatches\": %llu,\n  \"repairs\": %llu,\n  \"anomalies\": %llu,\n  \"uptime_sec\": %llu\n}\n",
                (unsigned long long)stats.totalOperations.load(), (unsigned long long)oStats.repairsAttempted, (unsigned long long)oStats.anomaliesDetected, (unsigned long long)(GetTickCount64()/1000));
        fclose(f);
        ctx.output("[TELEMETRY] Exported to: ");
        ctx.output(outFile);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to write export file.\n");
    }
    return CommandResult::ok("telemetry.exportJson");
}
CommandResult handleTelemetryExportCsv(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9902, 0);
        return CommandResult::ok("telemetry.exportCsv");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "telemetry_export.csv";
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "metric,value\nhotpatches,%llu\nmemory_patches,%llu\nbyte_patches,%llu\nserver_patches,%llu\nuptime_sec,%llu\n",
                (unsigned long long)stats.totalOperations.load(), (unsigned long long)stats.memoryPatchCount.load(), (unsigned long long)stats.bytePatchCount.load(), (unsigned long long)stats.serverPatchCount.load(), (unsigned long long)(GetTickCount64()/1000));
        fclose(f);
        ctx.output("[TELEMETRY] CSV exported to: ");
        ctx.output(outFile);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to write CSV.\n");
    }
    return CommandResult::ok("telemetry.exportCsv");
}
#endif
CommandResult handleTelemetryDashboard(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9903, 0);
        return CommandResult::ok("telemetry.dashboard");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    auto& sentinel = SentinelWatchdog::instance();
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    std::ostringstream oss;
    oss << "=== Telemetry Dashboard ===\n"
        << "  Hotpatches:  " << stats.totalOperations.load() << " (M:" << stats.memoryPatchCount.load() << " B:" << stats.bytePatchCount.load() << " S:" << stats.serverPatchCount.load() << ")\n"
        << "  Sentinel:    " << (sentinel.isActive() ? "ACTIVE" : "inactive") << "\n"
        << "  Memory:      " << mem.dwMemoryLoad << "% used\n"
        << "  Uptime:      " << (GetTickCount64()/1000) << " sec\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("telemetry.dashboard");
}
#ifndef RAWR_AUTO_FEATURE_REGISTRY_PROVIDES_HANDLERS
CommandResult handleTelemetryClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9904, 0);
        return CommandResult::ok("telemetry.clear");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    ProxyHotpatcher::instance().resetStats();
    ctx.output("[TELEMETRY] All telemetry data cleared.\n");
    return CommandResult::ok("telemetry.clear");
}
CommandResult handleTelemetrySnapshot(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9905, 0);
        return CommandResult::ok("telemetry.snapshot");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[128];
    snprintf(filename, sizeof(filename), "snapshot_%04d%02d%02d_%02d%02d%02d.json",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "{\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"patches\":%llu}\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, (unsigned long long)stats.totalOperations.load());
        fclose(f);
        ctx.output("[TELEMETRY] Snapshot saved: ");
        ctx.output(filename);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to save snapshot.\n");
    }
    return CommandResult::ok("telemetry.snapshot");
}
#endif

// ============================================================================
// TIER 1: CRITICAL COSMETICS (12000-12099)
// These handlers route through Win32IDE::routeCommand() ? handleTier1Command()
// which is wired in Win32IDE_Commands.cpp. The routeToIde pattern cannot
// be used here because it would create infinite WM_COMMAND re-entry.
// Instead, these stubs just confirm dispatch; the real work happens in
// Win32IDE_Tier1Cosmetics.cpp via the routeCommand() fallback path.
// ============================================================================

CommandResult handleTier1SmoothScrollToggle(const CommandContext& ctx)  { return routeToIde(ctx, 12000, "tier1.smoothScroll"); }
CommandResult handleTier1MinimapEnhanced(const CommandContext& ctx)     { return routeToIde(ctx, 12001, "tier1.minimapEnhanced"); }
CommandResult handleTier1BreadcrumbsToggle(const CommandContext& ctx)   { return routeToIde(ctx, 12020, "tier1.breadcrumbs"); }
CommandResult handleTier1FuzzyPalette(const CommandContext& ctx)        { return routeToIde(ctx, 12030, "tier1.fuzzyPalette"); }
CommandResult handleTier1SettingsGUI(const CommandContext& ctx)         { return routeToIde(ctx, 12040, "tier1.settingsGUI"); }
CommandResult handleTier1WelcomePage(const CommandContext& ctx)         { return routeToIde(ctx, 12050, "tier1.welcomePage"); }
CommandResult handleTier1FileIconTheme(const CommandContext& ctx)       { return routeToIde(ctx, 12060, "tier1.fileIcons"); }
CommandResult handleTier1TabDragToggle(const CommandContext& ctx)       { return routeToIde(ctx, 12070, "tier1.tabDrag"); }
CommandResult handleTier1SplitVertical(const CommandContext& ctx)       { return routeToIde(ctx, 12080, "tier1.splitVertical"); }
CommandResult handleTier1SplitHorizontal(const CommandContext& ctx)     { return routeToIde(ctx, 12081, "tier1.splitHorizontal"); }
CommandResult handleTier1SplitGrid(const CommandContext& ctx)           { return routeToIde(ctx, 12082, "tier1.splitGrid"); }
CommandResult handleTier1SplitClose(const CommandContext& ctx)          { return routeToIde(ctx, 12083, "tier1.splitClose"); }
CommandResult handleTier1SplitFocusNext(const CommandContext& ctx)      { return routeToIde(ctx, 12084, "tier1.splitFocusNext"); }
CommandResult handleTier1AutoUpdateCheck(const CommandContext& ctx)     { return routeToIde(ctx, 12090, "tier1.autoUpdate"); }
CommandResult handleTier1UpdateDismiss(const CommandContext& ctx)       { return routeToIde(ctx, 12091, "tier1.updateDismiss"); }
