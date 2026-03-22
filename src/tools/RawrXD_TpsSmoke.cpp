// RawrXD-TpsSmoke — single console harness: runtime_surface (unity) + model gate + GGUFRunner + TPS vs 239 ref.
// Usage: RawrXD-TpsSmoke.exe [path.gguf] [max_tokens]
//
// Canonical run for comparing to RAWRXD_TPS_REF (default 239): small CPU-friendly GGUF (e.g. 7B Q4),
// default max_tokens=8 — same as scripts/Benchmark-TpsSmoke-Models.ps1. Pass a higher second arg for
// longer runs; huge checkpoints need small max_tokens or they take forever.
// Env: RAWRXD_TPS_REF=239  RAWRXD_TPS_REQUIRE_BEAT=1 (exit 4 if below ref)
//      RAWRXD_TPS_MACHINE_JSON=1 — emit one stderr line RAWRXD_TPS_JSON={...} for batch drivers (see
//      scripts/Benchmark-TpsSmoke-Models.ps1)
//
// Each decode step runs a full multi-layer forward (attention + MLP) in reference C++ — not a tiny micro-bench.
// Huge checkpoints (10–80+ GB) can take a very long time per step or OOM; use a small GGUF for quick TPS checks.
// GGUFRunner: RAWRXD_GGUF_MAX_LAYER_FLOAT_RAM_GB=<n> skips per-layer float dequant when the estimated mirror
// would exceed n GiB (avoids bad_alloc); unset = attempt full load (may still OOM on RAM).

#include "../llm_adapter/GGUFRunner.h"
#include "../logging/Logger.h"
#include "rawrxd/runtime/GenerationStopwatch.hpp"
#include "rawrxd/runtime/RuntimeSurfaceBootstrap.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <cstring>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstring>
#include <windows.h>
#endif

namespace
{

double envDouble(const char* name, double fallback)
{
    const char* v = std::getenv(name);
    if (!v || !v[0])
    {
        return fallback;
    }
    return std::strtod(v, nullptr);
}

bool envTruthy(const char* name)
{
    const char* v = std::getenv(name);
    if (!v || !v[0])
    {
        return false;
    }
    return v[0] == '1' || v[0] == 'y' || v[0] == 'Y' || v[0] == 't' || v[0] == 'T';
}

/** One-line JSON for batch drivers (PowerShell). Emitted to stderr; line prefix RAWRXD_TPS_JSON= */
struct TpsMachineRecord
{
    int exitCode = 0;
    std::string phase = "unknown";
    std::string path;
    int maxTokens = 0;
    unsigned long long fileBytes = 0;
    std::string arch;
    int layers = 0;
    int embed = 0;
    std::uint64_t vocab = 0;
    int steps = 0;
    double wallS = 0.0;
    double tps = 0.0;
    double refTps = 239.0;
    std::uint64_t genMs = 0;
    bool beatRef = false;
    std::string detail;
};

std::string jsonEscapeString(const std::string& s)
{
    std::string o;
    o.reserve(s.size() + 8);
    o.push_back('"');
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\b':
                o += "\\b";
                break;
            case '\f':
                o += "\\f";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\r':
                o += "\\r";
                break;
            case '\t':
                o += "\\t";
                break;
            default:
                if (c < 0x20U)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    o += buf;
                }
                else
                {
                    o.push_back(static_cast<char>(c));
                }
        }
    }
    o.push_back('"');
    return o;
}

void emitTpsMachineJsonIfEnabled(const TpsMachineRecord& r)
{
    if (!envTruthy("RAWRXD_TPS_MACHINE_JSON"))
    {
        return;
    }
    std::ostringstream j;
    j << "RAWRXD_TPS_JSON=";
    j << '{';
    j << "\"exit\":" << r.exitCode << ',';
    j << "\"phase\":" << jsonEscapeString(r.phase) << ',';
    j << "\"path\":" << jsonEscapeString(r.path) << ',';
    j << "\"max_tokens\":" << r.maxTokens << ',';
    j << "\"file_bytes\":" << r.fileBytes << ',';
    j << "\"arch\":" << jsonEscapeString(r.arch) << ',';
    j << "\"layers\":" << r.layers << ',';
    j << "\"embed\":" << r.embed << ',';
    j << "\"vocab\":" << r.vocab << ',';
    j << "\"steps\":" << r.steps << ',';
    j << "\"wall_s\":" << std::to_string(r.wallS) << ',';
    j << "\"tps\":" << std::to_string(r.tps) << ',';
    j << "\"ref_tps\":" << std::to_string(r.refTps) << ',';
    j << "\"gen_ms\":" << r.genMs << ',';
    j << "\"beat_ref\":" << (r.beatRef ? "true" : "false") << ',';
    j << "\"detail\":" << jsonEscapeString(r.detail);
    j << '}';
    std::cerr << j.str() << '\n';
    std::cerr.flush();
}

#if defined(_WIN32)
/** If the process dies from 0xC0000005 etc., normal C++ paths never run — emit machine JSON for batch scripts. */
LONG WINAPI rawrxdTpsUnhandledExceptionFilter(_EXCEPTION_POINTERS* ep)
{
    if (!envTruthy("RAWRXD_TPS_MACHINE_JSON"))
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionCode : 0;
    const int exitAsInt = static_cast<int>(static_cast<int32_t>(code));
    char buf[2048];
    const int n =
        std::snprintf(buf, sizeof(buf),
                      "RAWRXD_TPS_JSON={\"exit\":%d,\"phase\":\"native_crash\",\"path\":\"\",\"max_tokens\":0,"
                      "\"file_bytes\":0,\"arch\":\"\",\"layers\":0,\"embed\":0,\"vocab\":0,\"steps\":0,\"wall_s\":0,"
                      "\"tps\":0,\"ref_tps\":239,\"gen_ms\":0,\"beat_ref\":false,"
                      "\"detail\":\"NTSTATUS=0x%08lX (access violation if 0xC0000005)\"}\n",
                      exitAsInt, static_cast<unsigned long>(code));
    if (n > 0 && static_cast<size_t>(n) < sizeof(buf))
    {
        (void)fwrite(buf, 1, static_cast<size_t>(n), stderr);
        (void)fflush(stderr);
    }
    ExitProcess(static_cast<UINT>(3));
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

}  // namespace

int main(int argc, char** argv)
{
#if defined(_WIN32)
    SetUnhandledExceptionFilter(rawrxdTpsUnhandledExceptionFilter);
#endif
    TpsMachineRecord rec;
    rec.refTps = envDouble("RAWRXD_TPS_REF", 239.0);

    try
    {
        RawrXD::Runtime::bootstrapRuntimeSurface();

        const std::string modelPath =
            (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : std::string("model/llama-7b-q4_0.gguf");
        // Default 8 matches Benchmark-TpsSmoke-Models.ps1 and the historical RAWRXD_TPS_REF=239 regime.
        const int maxTok = (argc > 2 && argv[2] && argv[2][0]) ? std::atoi(argv[2]) : 8;

        rec.path = modelPath;
        rec.maxTokens = maxTok;

        const bool requireBeat = envTruthy("RAWRXD_TPS_REQUIRE_BEAT");

        std::error_code ec;
        const std::filesystem::path fsPath(modelPath);
        if (!std::filesystem::exists(fsPath, ec))
        {
            rec.phase = "path_missing";
            rec.detail = "file does not exist";
            rec.exitCode = 1;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error(
                "TpsSmoke: model file does not exist (use a real .gguf path, not a doc placeholder): " + modelPath,
                "TpsSmoke");
            return 1;
        }
        if (!std::filesystem::is_regular_file(fsPath, ec))
        {
            rec.phase = "path_not_file";
            rec.detail = "not a regular file";
            rec.exitCode = 1;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error("TpsSmoke: model path is not a regular file: " + modelPath,
                                                      "TpsSmoke");
            return 1;
        }
        const auto byteSize = std::filesystem::file_size(fsPath, ec);
        rec.fileBytes = static_cast<unsigned long long>(byteSize);
        if (ec || byteSize < 32)
        {
            rec.phase = "path_invalid_size";
            rec.detail = "empty or too small";
            rec.exitCode = 1;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error(
                "TpsSmoke: model file is missing, empty, or too small to be GGUF (< 32 bytes): " + modelPath,
                "TpsSmoke");
            return 1;
        }

        GGUFRunner runner;
        // KV cache is allocated inside loadGGUFModel using maxTokens — set before load so sizing matches decode.
        runner.setMaxTokens(maxTok);
        if (!runner.loadModel(modelPath))
        {
            rec.phase = "load_failed";
            rec.detail = "loadModel returned false";
            rec.exitCode = 1;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error(
                "TpsSmoke: loadModel failed (not valid GGUF or unsupported layout): " + modelPath, "TpsSmoke");
            return 1;
        }

        runner.setTemperature(0.0f);
        runner.setTopP(1.0f);

        if (!runner.inferenceWeightsReady())
        {
            rec.arch = runner.architecture();
            rec.layers = static_cast<int>(runner.layerCount());
            rec.embed = static_cast<int>(runner.embeddingDim());
            rec.vocab = static_cast<std::uint64_t>(runner.vocabularySize());
            rec.phase = "weights_incomplete";
            rec.detail = "GGUFRunner has no per-layer blk.* weights loaded (would crash with 0xC0000005). "
                         "Implement layer tensor load + quant support for this checkpoint.";
            rec.exitCode = 2;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error("TpsSmoke: " + rec.detail + " Model: " + modelPath, "TpsSmoke");
            return 2;
        }

        rec.arch = runner.architecture();
        rec.layers = static_cast<int>(runner.layerCount());
        rec.embed = static_cast<int>(runner.embeddingDim());
        rec.vocab = static_cast<std::uint64_t>(runner.vocabularySize());

        RawrXD::Logging::Logger::instance().info(
            std::string("[TpsSmoke] loaded arch=") + rec.arch + " layers=" + std::to_string(rec.layers) +
                " embed=" + std::to_string(rec.embed) + " vocab=" + std::to_string(rec.vocab) +
                " file_bytes=" + std::to_string(rec.fileBytes),
            "TpsSmoke");
        RawrXD::Logging::Logger::instance().info(
            std::string("[TpsSmoke] runInference starting (max_tokens=") + std::to_string(maxTok) +
                ") — full forward per step; large models may sit here a long time (not a hang). "
                "Set RAWRXD_GGUF_DECODE_PROGRESS=1 for per-token logs.",
            "TpsSmoke");

        std::vector<float> logits(std::max<size_t>(1, runner.vocabularySize()));

        const auto wall0 = std::chrono::steady_clock::now();
        if (!runner.runInference("TPS smoke — single-module forward/token benchmark.", logits.data()))
        {
            rec.phase = "inference_failed";
            rec.detail = "runInference returned false";
            rec.exitCode = 2;
            emitTpsMachineJsonIfEnabled(rec);
            RawrXD::Logging::Logger::instance().error("TpsSmoke: runInference failed", "TpsSmoke");
            return 2;
        }
        const auto wall1 = std::chrono::steady_clock::now();

        const double wallSec = std::chrono::duration<double>(wall1 - wall0).count();
        const int steps = runner.lastDecodeSteps();
        const double tps = (wallSec > 1e-9) ? (static_cast<double>(steps) / wallSec) : 0.0;
        const std::uint64_t genMs = RawrXD::Runtime::GenerationStopwatch::instance().totalGenerationMs();

        rec.phase = "ok";
        rec.steps = steps;
        rec.wallS = wallSec;
        rec.tps = tps;
        rec.genMs = genMs;
        rec.beatRef = tps >= rec.refTps;

        std::string summary = "[TpsSmoke] decode_steps=" + std::to_string(steps) +
                              " wall_s=" + std::to_string(wallSec) + " tps=" + std::to_string(tps) +
                              " ref_tps=" + std::to_string(rec.refTps) +
                              " gen_stopwatch_total_ms=" + std::to_string(genMs) +
                              (rec.beatRef ? " RESULT=BEAT_REF" : " RESULT=BELOW_REF");
        RawrXD::Logging::Logger::instance().info(summary, "TpsSmoke");

        if (requireBeat && tps < rec.refTps)
        {
            rec.exitCode = 4;
            rec.detail = "RAWRXD_TPS_REQUIRE_BEAT and below ref";
            emitTpsMachineJsonIfEnabled(rec);
            return 4;
        }
        rec.exitCode = 0;
        emitTpsMachineJsonIfEnabled(rec);
        return 0;
    }
    catch (const std::bad_alloc& e)
    {
        rec.phase = "bad_alloc";
        rec.detail = e.what();
        rec.exitCode = 3;
        emitTpsMachineJsonIfEnabled(rec);
        std::cerr << "[TpsSmoke] std::bad_alloc (often model too large for RAM): " << e.what() << "\n";
        std::cerr.flush();
        RawrXD::Logging::Logger::instance().error(std::string("TpsSmoke: std::bad_alloc — ") + e.what(), "TpsSmoke");
        return 3;
    }
    catch (const std::exception& e)
    {
        rec.phase = "exception";
        rec.detail = e.what();
        rec.exitCode = 3;
        emitTpsMachineJsonIfEnabled(rec);
        std::cerr << "[TpsSmoke] unhandled exception: " << e.what() << "\n";
        std::cerr.flush();
        RawrXD::Logging::Logger::instance().error(std::string("TpsSmoke: exception — ") + e.what(), "TpsSmoke");
        return 3;
    }
}
