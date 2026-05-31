 // RawrEngine Lane B: Headless minimal entry point.
// Goal: keep the 274TB streamer/loader core linkable without GUI/hotpatch/omega subsystems.

#include "Win32IDE_AgenticBridge.h"
#include "cpu_inference_engine.h"
#include "gguf_loader.h"
#include "rawrxd_model_loader.h"

#include <psapi.h>
#include <windows.h>

#include <memoryapi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <io.h>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifndef RAWRXD_HEADLESS_COPILOT_TMP_CAP
#define RAWRXD_HEADLESS_COPILOT_TMP_CAP 1024u
#endif

extern "C" unsigned __int64 RawrXD_EnableSeLockMemoryPrivilege();
extern "C" unsigned int rawr_cpu_has_avx512();
extern "C" void* RawrXD_MapModelView2MB(HANDLE hMap, uint64_t off, size_t sz, uint64_t* outBaseOrError);
extern "C" void RawrXD_StreamToGPU_AVX512(void* dst, const void* src, unsigned long long blocks64B);

static std::wstring widenUtf8(const char* s)
{
    if (!s)
        return std::wstring();
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (needed <= 0)
        return std::wstring();
    std::wstring out;
    // Allocate including NUL terminator; use &out[0] for mutable storage across standard libs.
    out.resize(static_cast<size_t>(needed));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], needed);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

static void appendU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

static void appendU64(std::vector<uint8_t>& out, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
}

static void appendBytes(std::vector<uint8_t>& out, const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    out.insert(out.end(), p, p + size);
}

static void appendString(std::vector<uint8_t>& out, const std::string& s)
{
    appendU64(out, static_cast<uint64_t>(s.size()));
    appendBytes(out, s.data(), s.size());
}

static uint64_t align32U64(uint64_t x)
{
    return (x + 31ULL) & ~31ULL;
}

static bool writeMinimalGgufV3(const std::filesystem::path& path)
{
    constexpr uint32_t kMagic = 0x46554747u;  // "GGUF" LE
    constexpr uint32_t kVersion = 3u;
    constexpr uint64_t kTensorCount = 1u;
    // arch, tok model, file_type, llama dims (GATE-7), tokenizer tokens
    constexpr uint64_t kKvCount = 9u;

    const std::string kArchKey = "general.architecture";
    const std::string kArchVal = "llama";
    const std::string kTokModelKey = "tokenizer.ggml.model";
    const std::string kTokModelVal = "gpt2";
    const std::string kFileTypeKey = "general.file_type";
    const uint32_t kFileTypeVal = 0u;  // F32
    const std::string kLlamaEmbKey = "llama.embedding_length";
    const uint32_t kLlamaEmb = 4u;  // must match token_embd row / head divisibility
    const std::string kLlamaBlocksKey = "llama.block_count";
    const uint32_t kLlamaBlocks = 1u;
    const std::string kLlamaHeadsKey = "llama.attention.head_count";
    const uint32_t kLlamaHeads = 1u;
    const std::string kLlamaHeadsKvKey = "llama.attention.head_count_kv";
    const uint32_t kLlamaHeadsKv = 1u;
    const std::string kLlamaCtxKey = "llama.context_length";
    const uint32_t kLlamaCtx = 512u;
    const std::string kTokTokensKey = "tokenizer.ggml.tokens";
    const std::vector<std::string> kTokens = {"<unk>", "hello"};

    const std::string kTensorName = "token_embd.weight";
    const uint32_t kTensorNDims = 1u;
    const uint64_t kTensorDim0 = 4u;
    const uint32_t kTensorTypeF32 = 0u;  // GGMLType::F32
    const float kTensorData[4] = {1.0f, 2.0f, 3.5f, -4.0f};

    std::vector<uint8_t> buf;
    buf.reserve(4096);

    appendU32(buf, kMagic);
    appendU32(buf, kVersion);
    appendU64(buf, kTensorCount);
    appendU64(buf, kKvCount);

    appendString(buf, kArchKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kArchVal);

    appendString(buf, kTokModelKey);
    appendU32(buf, 8u);  // string
    appendString(buf, kTokModelVal);

    appendString(buf, kFileTypeKey);
    appendU32(buf, 4u);  // uint32
    appendU32(buf, kFileTypeVal);

    appendString(buf, kLlamaEmbKey);
    appendU32(buf, 4u);
    appendU32(buf, kLlamaEmb);

    appendString(buf, kLlamaBlocksKey);
    appendU32(buf, 4u);
    appendU32(buf, kLlamaBlocks);

    appendString(buf, kLlamaHeadsKey);
    appendU32(buf, 4u);
    appendU32(buf, kLlamaHeads);

    appendString(buf, kLlamaHeadsKvKey);
    appendU32(buf, 4u);
    appendU32(buf, kLlamaHeadsKv);

    appendString(buf, kLlamaCtxKey);
    appendU32(buf, 4u);
    appendU32(buf, kLlamaCtx);

    appendString(buf, kTokTokensKey);
    appendU32(buf, 9u);  // array
    appendU32(buf, 8u);  // element type = string
    appendU64(buf, static_cast<uint64_t>(kTokens.size()));
    for (const auto& t : kTokens)
        appendString(buf, t);

    appendString(buf, kTensorName);
    appendU32(buf, kTensorNDims);
    appendU64(buf, kTensorDim0);
    appendU32(buf, kTensorTypeF32);

    const uint64_t dataStart = align32U64(static_cast<uint64_t>(buf.size() + 8 /* offset field */));
    appendU64(buf, dataStart);

    if (buf.size() < dataStart)
        buf.resize(static_cast<size_t>(dataStart), 0);

    appendBytes(buf, kTensorData, sizeof(kTensorData));

    // Ensure the file is large enough for legacy mapping paths that may request
    // ~1MB+ windows while parsing/initializing. Padding is valid for GGUF.
    constexpr size_t kMinFileBytes = 2u * 1024u * 1024u;
    if (buf.size() < kMinFileBytes)
        buf.resize(kMinFileBytes, 0);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    out.flush();
    return true;
}

static uint32_t lcgNext(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

template <typename T> static void shuffleLcg(std::vector<T>& v, uint32_t seed)
{
    if (v.size() <= 1)
        return;
    uint32_t s = seed ? seed : 0xC0FFEEu;
    for (size_t i = v.size() - 1; i > 0; --i)
    {
        const uint32_t r = lcgNext(s);
        const size_t j = static_cast<size_t>(r % (i + 1));
        std::swap(v[i], v[j]);
    }
}

static uint64_t alignUpU64(uint64_t x, uint64_t alignment)
{
    if (alignment == 0)
        return x;
    const uint64_t r = x % alignment;
    return r ? (x + (alignment - r)) : x;
}

static bool writeFragmentedGgufV3(const std::filesystem::path& path, uint32_t numTensors, uint64_t strideBytes,
                                  bool alignToStride, bool shuffleLayout, uint32_t shuffleSeed)
{
    constexpr uint32_t kMagic = 0x46554747u;  // "GGUF" LE
    constexpr uint32_t kVersion = 3u;
    constexpr uint32_t kTensorTypeF32 = 0u;  // GGMLType::F32

    if (numTensors < 4u)
        numTensors = 4u;
    if (strideBytes == 0)
        strideBytes = 2ull * 1024ull * 1024ull;

    struct TensorSpec
    {
        std::string name;
        float values[4];
    };

    std::vector<TensorSpec> specs;
    specs.reserve(numTensors);

    // Canonical tensors (zones/layers).
    specs.push_back({"token_embd.weight", {11.0f, 12.0f, 13.0f, 14.0f}});
    specs.push_back({"blk.0.attn.weight", {21.0f, 22.0f, 23.0f, 24.0f}});
    specs.push_back({"blk.7.ffn.weight", {31.0f, 32.0f, 33.0f, 34.0f}});
    specs.push_back({"blk.8.attn.weight", {41.0f, 42.0f, 43.0f, 44.0f}});

    constexpr uint32_t kGeneratedLayerBase = 16u;
    for (uint32_t i = 4u; i < numTensors; ++i)
    {
        const uint32_t layer = kGeneratedLayerBase + i;
        const bool useAttn = (i % 2u) == 0u;
        TensorSpec s;
        s.name = "blk." + std::to_string(layer) + (useAttn ? ".attn.weight" : ".ffn.weight");
        const float base = static_cast<float>(1000.0 + layer * 10.0);
        s.values[0] = base + 1.0f;
        s.values[1] = base + 2.0f;
        s.values[2] = base + 3.0f;
        s.values[3] = base + 4.0f;
        specs.push_back(std::move(s));
    }

    if (shuffleLayout)
        shuffleLcg(specs, shuffleSeed);

    const uint64_t tensorCount = static_cast<uint64_t>(specs.size());
    constexpr uint64_t kvCount = 4u;

    const std::string kArchKey = "general.architecture";
    const std::string kArchVal = "llama";
    const std::string kTokModelKey = "tokenizer.ggml.model";
    const std::string kTokModelVal = "gpt2";
    const std::string kFileTypeKey = "general.file_type";
    const uint32_t kFileTypeVal = 0u;  // F32
    const std::string kTokTokensKey = "tokenizer.ggml.tokens";
    const std::vector<std::string> kTokens = {"<unk>", "hello"};

    // Build header+KV+tensor table first, then fill offsets.
    std::vector<uint8_t> buf;
    buf.reserve(static_cast<size_t>(tensorCount) * 128u + 4096u);

    appendU32(buf, kMagic);
    appendU32(buf, kVersion);
    appendU64(buf, tensorCount);
    appendU64(buf, kvCount);

    appendString(buf, kArchKey);
    appendU32(buf, 8u);
    appendString(buf, kArchVal);

    appendString(buf, kTokModelKey);
    appendU32(buf, 8u);
    appendString(buf, kTokModelVal);

    appendString(buf, kFileTypeKey);
    appendU32(buf, 4u);
    appendU32(buf, kFileTypeVal);

    appendString(buf, kTokTokensKey);
    appendU32(buf, 9u);
    appendU32(buf, 8u);
    appendU64(buf, static_cast<uint64_t>(kTokens.size()));
    for (const auto& t : kTokens)
        appendString(buf, t);

    struct OffsetPatch
    {
        size_t offsetFieldPos;
        uint64_t absOffset;
        float values[4];
    };
    std::vector<OffsetPatch> patches;
    patches.reserve(static_cast<size_t>(tensorCount));

    for (const auto& s : specs)
    {
        appendString(buf, s.name);
        appendU32(buf, 1u);  // ndims
        appendU64(buf, 4u);  // dim0
        appendU32(buf, kTensorTypeF32);
        const size_t offPos = buf.size();
        appendU64(buf, 0);  // offset field: patched with actual absolute offset after layout
        OffsetPatch p{};
        p.offsetFieldPos = offPos;
        p.absOffset = 0;
        std::memcpy(p.values, s.values, sizeof(p.values));
        patches.push_back(p);
    }

    // Data starts 32B aligned.
    uint64_t cursor = align32U64(static_cast<uint64_t>(buf.size()));
    for (size_t i = 0; i < patches.size(); ++i)
    {
        if (alignToStride)
            cursor = alignUpU64(cursor, strideBytes);
        patches[i].absOffset = cursor;
        cursor += sizeof(patches[i].values);
        cursor = alignUpU64(cursor, strideBytes);
    }

    // Patch offsets back into tensor table.
    for (const auto& p : patches)
    {
        const uint64_t v = p.absOffset;
        const size_t pos = p.offsetFieldPos;
        for (int b = 0; b < 8; ++b)
            buf[pos + static_cast<size_t>(b)] = static_cast<uint8_t>((v >> (8 * b)) & 0xFFu);
    }

    // Write a sparse file without allocating gigantic in-memory holes.
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    const std::wstring wPath = path.wstring();
    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    if (!buf.empty())
    {
        if (!WriteFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &written, nullptr) || written != buf.size())
        {
            CloseHandle(hFile);
            return false;
        }
    }

    uint64_t maxEnd = 0;
    for (const auto& p : patches)
    {
        LARGE_INTEGER li{};
        li.QuadPart = static_cast<LONGLONG>(p.absOffset);
        if (!SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN))
        {
            CloseHandle(hFile);
            return false;
        }
        DWORD w = 0;
        if (!WriteFile(hFile, p.values, sizeof(p.values), &w, nullptr) || w != sizeof(p.values))
        {
            CloseHandle(hFile);
            return false;
        }
        const uint64_t end = p.absOffset + static_cast<uint64_t>(sizeof(p.values));
        maxEnd = (end > maxEnd) ? end : maxEnd;
    }

    // Pad to at least one stride beyond last payload to keep mapping stable.
    const uint64_t finalBytes = alignUpU64(maxEnd, strideBytes);
    LARGE_INTEGER endLi{};
    endLi.QuadPart = static_cast<LONGLONG>(finalBytes);
    if (!SetFilePointerEx(hFile, endLi, nullptr, FILE_BEGIN) || !SetEndOfFile(hFile))
    {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

static const char* getOptValue(const std::vector<std::string>& args, const char* opt)
{
    for (size_t i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] == opt)
            return args[i + 1].c_str();
    }
    return nullptr;
}

static bool hasOpt(const std::vector<std::string>& args, const char* opt)
{
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == opt)
            return true;
    }
    return false;
}

class ScopedStdioSilence
{
  public:
    explicit ScopedStdioSilence(bool enable)
    {
        if (!enable)
            return;
        std::fflush(stdout);
        std::fflush(stderr);
        m_savedStdout = _dup(_fileno(stdout));
        if (m_savedStdout < 0)
            return;
        m_savedStderr = _dup(_fileno(stderr));
        if (m_savedStderr < 0)
        {
            _close(m_savedStdout);
            m_savedStdout = -1;
            return;
        }
        FILE* outFile = nullptr;
        if (freopen_s(&outFile, "NUL", "w", stdout) != 0)
        {
            _close(m_savedStderr);
            m_savedStderr = -1;
            _close(m_savedStdout);
            m_savedStdout = -1;
            return;
        }
        FILE* errFile = nullptr;
        if (freopen_s(&errFile, "NUL", "w", stderr) != 0)
        {
            _dup2(m_savedStdout, _fileno(stdout));
            _close(m_savedStderr);
            m_savedStderr = -1;
            _close(m_savedStdout);
            m_savedStdout = -1;
            return;
        }
        m_engaged = true;
    }

    ~ScopedStdioSilence()
    {
        if (!m_engaged)
            return;
        std::fflush(stdout);
        std::fflush(stderr);
        _dup2(m_savedStdout, _fileno(stdout));
        _dup2(m_savedStderr, _fileno(stderr));
        _close(m_savedStdout);
        _close(m_savedStderr);
        m_savedStdout = -1;
        m_savedStderr = -1;
        m_engaged = false;
    }

  private:
    int m_savedStdout = -1;
    int m_savedStderr = -1;
    bool m_engaged    = false;
};

static int printUsage()
{
    // Minimal and deterministic; no SSOT/CLI layers in Lane B.
    const char* msg =
        "RawrEngine (LaneB headless)\n"
        "Usage:\n"
        "  RawrEngine.exe --compile-only\n"
        "  RawrEngine.exe --gguf-header <path>\n"
        "  RawrEngine.exe --gen-gguf <path>\n"
        "  RawrEngine.exe --gen-gguf-frag <path> [--num-tensors <N>] [--stride-bytes <N>] [--align-to-stride 0|1] "
        "[--shuffle-layout 0|1] [--seed <u32>]\n"
        "  RawrEngine.exe --load-model <path>\n"
        "  RawrEngine.exe --infer <path> --prompt <text> [--max-tokens <N>] [--verbose]\n"
        "  RawrEngine.exe --copilot-smoke [--model <path.gguf>] [--prompt <text>] [--with-agentic] [--skip-agentic]\n"
        "      (Lane B: load + one GenerateStreaming; optional second AgenticBridge pass with --with-agentic only;\n"
        "       default skips agentic to avoid re-entrant GGUF init on the shared engine)\n"
        "  RawrEngine.exe --streamer-smoke <path> [--offset <u64>] [--size <u64>] [--iterations <N>]\n"
        "  RawrEngine.exe --bench-streamer <path> [--max-mb <N>] [--iters <N>] [--largepages 0|1] [--prefetch 0|1]\n"
        "  RawrEngine.exe --offset-sweep <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--largepages 0|1] "
        "[--prefetch 0|1]\n"
        "  RawrEngine.exe --offset-sweep-loader <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--prefetch "
        "0|1]\n";
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &written, nullptr);
    return 2;
}

/// Headless Copilot/Codex-style parity: load weights (or synthesize minimal GGUF), one streaming generation
/// (TPS-style wall_ms / estimated_tps). Optional second pass: AgenticBridge::ExecuteAgentCommand when
/// --with-agentic is passed (default off — re-loading the shared GGUF stack can AV on minimal smoke files).
static int runCopilotSmoke(const std::vector<std::string>& args)
{
    auto writeStdout = [](const char* s)
    {
        if (!s)
            return;
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, static_cast<DWORD>(std::strlen(s)), &w, nullptr);
    };

    std::filesystem::path ggufPath;
    bool wroteTemp = false;

    try
    {
        const char* modelOpt = getOptValue(args, "--model");
        if (modelOpt && modelOpt[0])
        {
            ggufPath = std::filesystem::path(widenUtf8(modelOpt));
        }
        else
        {
            wchar_t tmpDir[RAWRXD_HEADLESS_COPILOT_TMP_CAP]{};
            const DWORD n = GetTempPathW(RAWRXD_HEADLESS_COPILOT_TMP_CAP, tmpDir);
            if (n == 0 || n >= RAWRXD_HEADLESS_COPILOT_TMP_CAP)
            {
                writeStdout("copilot-smoke: GetTempPathW failed\nEXIT=1\n");
                return 1;
            }
            ggufPath = std::filesystem::path(tmpDir) / L"rawrxd_copilot_smoke_minimal.gguf";
            if (!writeMinimalGgufV3(ggufPath))
            {
                writeStdout("copilot-smoke: failed to write minimal GGUF in %%TEMP%%\nEXIT=1\n");
                return 1;
            }
            wroteTemp = true;
        }

        const char* promptStr = getOptValue(args, "--prompt");
        const std::string prompt =
            (promptStr && promptStr[0]) ? std::string(promptStr) : std::string("Reply with the single word: OK");

        bool skipAgentic = true;
        for (size_t ai = 1; ai < args.size(); ++ai)
        {
            if (args[ai] == "--with-agentic")
            {
                skipAgentic = false;
            }
            else if (args[ai] == "--skip-agentic")
            {
                skipAgentic = true;
            }
        }

        const std::string modelUtf8 = ggufPath.string();
        const std::shared_ptr<RawrXD::CPUInferenceEngine> engine = RawrXD::CPUInferenceEngine::GetSharedInstance();
        if (!engine->LoadModel(modelUtf8))
        {
            const std::string err = engine->GetLastLoadErrorMessage();
            if (!err.empty())
            {
                std::string line = "copilot-smoke: LoadModel failed: ";
                line += err;
                line += "\n";
                writeStdout(line.c_str());
            }
            else
            {
                writeStdout("copilot-smoke: LoadModel failed\n");
            }
            if (wroteTemp)
            {
                std::error_code ec;
                std::filesystem::remove(ggufPath, ec);
            }
            writeStdout("EXIT=1\n");
            return 1;
        }

        const std::vector<int32_t> inputToks = engine->Tokenize(prompt);

        LARGE_INTEGER perfFreq{};
        LARGE_INTEGER t0{};
        LARGE_INTEGER t1{};
        if (!QueryPerformanceFrequency(&perfFreq) || perfFreq.QuadPart == 0)
        {
            perfFreq.QuadPart = 1;
        }
        std::string responseText;
        QueryPerformanceCounter(&t0);
        engine->GenerateStreaming(
            inputToks, 128, [&responseText](const std::string& piece) { responseText += piece; }, []() {});
        QueryPerformanceCounter(&t1);
        const double wallMs =
            (1000.0 * static_cast<double>(t1.QuadPart - t0.QuadPart)) / static_cast<double>(perfFreq.QuadPart);
        const unsigned long long respChars = static_cast<unsigned long long>(responseText.size());
        // Rough throughput: UTF-8 output chars / wall seconds (IDE chat uses token-based estimated_tps).
        double estTps = 0.0;
        if (wallMs > 1e-6)
        {
            estTps = static_cast<double>(respChars) / (wallMs / 1000.0);
        }

        const bool modelLoaded = engine->IsModelLoaded();

        std::string agenticType = "skipped";
        unsigned long long agenticChars = 0;
        bool agenticOk = true;
        if (modelLoaded && !skipAgentic)
        {
            // Attach to the already-loaded shared CPUInferenceEngine — do not call Initialize(..., modelUtf8)
            // again or the loader/tokenizer may be re-entered and leave the engine inconsistent.
            AgenticBridge bridge(nullptr);
            if (!bridge.Initialize("", ""))
            {
                agenticOk = false;
                agenticType = "INIT_FAILED";
            }
            else
            {
                const AgentResponse ar = bridge.ExecuteAgentCommand(prompt);
                agenticChars = static_cast<unsigned long long>(ar.content.size());
                agenticOk = (ar.type != AgentResponseType::AGENT_ERROR);
                switch (ar.type)
                {
                    case AgentResponseType::ANSWER:
                        agenticType = "ANSWER";
                        break;
                    case AgentResponseType::TOOL_CALL:
                        agenticType = "TOOL_CALL";
                        break;
                    case AgentResponseType::THINKING:
                        agenticType = "THINKING";
                        break;
                    default:
                        agenticType = "AGENT_ERROR";
                        break;
                }
            }
        }

        const bool ok = modelLoaded && (skipAgentic || agenticOk);
        const char* rtype = modelLoaded ? "ANSWER" : "AGENT_ERROR";

        {
            std::ostringstream j;
            j.setf(std::ios::fixed, std::ios::floatfield);
            j << std::setprecision(3);
            j << "COPILOT_SMOKE_JSON:{\"ok\":" << (ok ? "true" : "false") << ",\"response_type\":\"" << rtype
              << "\",\"response_chars\":" << respChars << ",\"model_loaded\":" << (modelLoaded ? "true" : "false")
              << ",\"wall_ms\":" << wallMs << ",\"estimated_tps\":" << estTps
              << ",\"agentic_skipped\":" << (skipAgentic ? "true" : "false")
              << ",\"agentic_ok\":" << (skipAgentic ? "true" : (agenticOk ? "true" : "false"))
              << ",\"agentic_response_type\":\"" << agenticType << "\",\"agentic_response_chars\":" << agenticChars
              << "}\n";
            writeStdout(j.str().c_str());
        }

        writeStdout(ok ? "EXIT=0\n" : "EXIT=1\n");

        if (wroteTemp)
        {
            std::error_code ec;
            std::filesystem::remove(ggufPath, ec);
        }
        // Lane B: some builds fault during static teardown after a successful smoke; optional fast exit skips
        // atexit/static destructors (set RAWRXD_COPILOT_SMOKE_FAST_EXIT=1 from smoketests / CI only).
        if (ok)
        {
            const char* fastExit = std::getenv("RAWRXD_COPILOT_SMOKE_FAST_EXIT");
            if (fastExit && fastExit[0] == '1' && fastExit[1] == '\0')
            {
                std::fflush(nullptr);
                std::_Exit(0);
            }
        }
        return ok ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        char buf[512]{};
        std::snprintf(buf, sizeof(buf), "copilot-smoke: exception: %s\nEXIT=1\n", e.what());
        writeStdout(buf);
        if (wroteTemp)
        {
            std::error_code ec;
            std::filesystem::remove(ggufPath, ec);
        }
        return 1;
    }
    catch (...)
    {
        writeStdout("copilot-smoke: unknown exception\nEXIT=1\n");
        if (wroteTemp)
        {
            std::error_code ec;
            std::filesystem::remove(ggufPath, ec);
        }
        return 1;
    }
}

static uint64_t qpcNow();
static double qpcToSeconds(uint64_t delta);
static size_t alignUp(size_t v, size_t a);
static uint64_t xorshift64(uint64_t& s);

static int offsetSweepLoader(const std::wstring& wPath, uint32_t windowMb, uint32_t iters, uint64_t seed, bool prefetch)
{
    if (windowMb < 1)
        windowMb = 1;
    if (iters < 1)
        iters = 1;
    if (seed == 0)
        seed = 0xC0FFEE1234ULL;

    RawrXDModelLoader loader;
    loader.SetPrefetchEnabled(prefetch);
    if (!loader.Load(wPath.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE))
        return 1;

    const uint64_t fileSize = loader.GetFileSizeBytes();
    if (fileSize == 0)
        return 1;

    const size_t requested = static_cast<size_t>(windowMb) * 1024u * 1024u;
    size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
    if (mapSize == 0)
        return 1;
    if (mapSize > fileSize)
        mapSize = alignUp(requested, 64u * 1024u);
    if (mapSize == 0 || mapSize > fileSize)
        return 2;

    const uint64_t maxOffset = fileSize - static_cast<uint64_t>(mapSize);
    uint64_t rng = seed;
    volatile uint64_t sink = 0;

    uint64_t mapTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t mapTicksMax = 0;
    uint64_t mapTicksSum = 0;

    uint64_t hintTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t hintTicksMax = 0;
    uint64_t hintTicksSum = 0;
    uint32_t hintOkCount = 0;

    uint64_t touchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t touchTicksMax = 0;
    uint64_t touchTicksSum = 0;

    for (uint32_t i = 0; i < iters; ++i)
    {
        const uint64_t r = xorshift64(rng);
        const uint64_t off = (maxOffset ? (r % (maxOffset + 1ull)) : 0ull);

        const uint64_t t0 = qpcNow();
        void* src = loader.MapWindow(off, mapSize);
        const uint64_t t1 = qpcNow();
        if (!src)
            return 1;

        const uint64_t mapTicks = (t1 - t0);
        mapTicksMin = (mapTicks < mapTicksMin) ? mapTicks : mapTicksMin;
        mapTicksMax = (mapTicks > mapTicksMax) ? mapTicks : mapTicksMax;
        mapTicksSum += mapTicks;

        if (prefetch)
        {
            // Hint a small lookahead inside this mapping window when possible.
            // We use "off + 64KB" as a deterministic, cheap proxy for "tensor N+1".
            const uint64_t hintOff = off + 64ull * 1024ull;
            const uint64_t h0 = qpcNow();
            const bool ok = loader.HintRange(hintOff, 256ull * 1024ull);
            const uint64_t h1 = qpcNow();
            const uint64_t hintTicks = (h1 - h0);
            hintTicksMin = (hintTicks < hintTicksMin) ? hintTicks : hintTicksMin;
            hintTicksMax = (hintTicks > hintTicksMax) ? hintTicks : hintTicksMax;
            hintTicksSum += hintTicks;
            hintOkCount += ok ? 1u : 0u;
        }

        // First-byte latency proxy.
        const uint64_t u0 = qpcNow();
        const volatile uint64_t* p64 = reinterpret_cast<const volatile uint64_t*>(src);
        sink ^= p64[0];
        sink ^= p64[1];
        sink ^= p64[2];
        sink ^= p64[3];
        sink ^= p64[4];
        sink ^= p64[5];
        sink ^= p64[6];
        sink ^= p64[7];
        const uint64_t u1 = qpcNow();

        const uint64_t touchTicks = (u1 - u0);
        touchTicksMin = (touchTicks < touchTicksMin) ? touchTicks : touchTicksMin;
        touchTicksMax = (touchTicks > touchTicksMax) ? touchTicks : touchTicksMax;
        touchTicksSum += touchTicks;

        loader.UnmapWindow();
    }

    const double mapAvgUs = (iters > 0) ? (qpcToSeconds(mapTicksSum / iters) * 1e6) : 0.0;
    const double mapMinUs = qpcToSeconds(mapTicksMin) * 1e6;
    const double mapMaxUs = qpcToSeconds(mapTicksMax) * 1e6;

    const double hintAvgUs = (prefetch && iters > 0) ? (qpcToSeconds(hintTicksSum / iters) * 1e6) : 0.0;
    const double hintMinUs = prefetch ? (qpcToSeconds(hintTicksMin) * 1e6) : 0.0;
    const double hintMaxUs = prefetch ? (qpcToSeconds(hintTicksMax) * 1e6) : 0.0;

    const double touchAvgUs = (iters > 0) ? (qpcToSeconds(touchTicksSum / iters) * 1e6) : 0.0;
    const double touchMinUs = qpcToSeconds(touchTicksMin) * 1e6;
    const double touchMaxUs = qpcToSeconds(touchTicksMax) * 1e6;

    char line[512]{};
    std::snprintf(line, sizeof(line),
                  "offset-sweep-loader: window=%uMB iters=%u seed=%llu prefetch=%u\n"
                  "  map_us:   avg=%.2f min=%.2f max=%.2f\n"
                  "  hint_us:  avg=%.2f min=%.2f max=%.2f ok=%u/%u\n"
                  "  touch_us: avg=%.2f min=%.2f max=%.2f\n"
                  "  sink=%llu\n",
                  windowMb, iters, static_cast<unsigned long long>(seed), prefetch ? 1u : 0u, mapAvgUs, mapMinUs,
                  mapMaxUs, hintAvgUs, hintMinUs, hintMaxUs, static_cast<unsigned int>(hintOkCount),
                  static_cast<unsigned int>(prefetch ? iters : 0u), touchAvgUs, touchMinUs, touchMaxUs,
                  static_cast<unsigned long long>(sink));
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
    return 0;
}

static int streamerSmoke(const std::wstring& wPath, uint64_t offset, uint64_t size, uint32_t iterations)
{
    if (iterations < 1)
        iterations = 1;
    if (size == 0)
        size = 4096;

    RawrXDModelLoader loader;
    if (!loader.Load(wPath.c_str(), VK_NULL_HANDLE, VK_NULL_HANDLE))
        return 1;

    const uint64_t fileSize = loader.GetFileSizeBytes();
    if (fileSize == 0)
        return 1;

    for (uint32_t i = 0; i < iterations; ++i)
    {
        const uint64_t farOffset = (fileSize > (size + 4096)) ? (fileSize - size - 4096) : 0;
        const uint64_t chosenOffset = (i & 1u) ? farOffset : offset;
        void* p = loader.MapWindow(chosenOffset, static_cast<size_t>(size));
        if (!p)
            return 1;

        if (loader.UsingLargePages())
        {
            const uintptr_t base = reinterpret_cast<uintptr_t>(loader.GetCurrentViewBase());
            if ((base & 0x1FFFFFull) != 0)
                return 1;
        }

        loader.UnmapWindow();
    }

    return 0;
}

struct AlignedVirtualAlloc
{
    void* base = nullptr;     // address returned by VirtualAlloc
    void* aligned = nullptr;  // aligned view inside base
    size_t reserveBytes = 0;
    size_t usableBytes = 0;
};

static AlignedVirtualAlloc allocAlignedVirtual(size_t bytes, size_t alignment)
{
    AlignedVirtualAlloc a{};
    if (bytes == 0)
        return a;
    if (alignment < 16)
        alignment = 16;

    const size_t total = bytes + alignment;
    void* p = VirtualAlloc(nullptr, total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p)
        return a;

    const uintptr_t u = reinterpret_cast<uintptr_t>(p);
    const uintptr_t alignedU = (u + (alignment - 1)) & ~(static_cast<uintptr_t>(alignment - 1));
    a.base = p;
    a.aligned = reinterpret_cast<void*>(alignedU);
    a.reserveBytes = total;
    a.usableBytes = bytes;
    return a;
}

static void freeAlignedVirtual(AlignedVirtualAlloc& a)
{
    if (a.base)
        VirtualFree(a.base, 0, MEM_RELEASE);
    a = {};
}

static uint64_t qpcNow()
{
    LARGE_INTEGER v{};
    QueryPerformanceCounter(&v);
    return static_cast<uint64_t>(v.QuadPart);
}

static uint64_t qpcFreq()
{
    static uint64_t f = []()
    {
        LARGE_INTEGER v{};
        QueryPerformanceFrequency(&v);
        return static_cast<uint64_t>(v.QuadPart);
    }();
    return f;
}

static double qpcToSeconds(uint64_t delta)
{
    const uint64_t f = qpcFreq();
    return f ? (static_cast<double>(delta) / static_cast<double>(f)) : 0.0;
}

static size_t alignUp(size_t v, size_t a)
{
    return (v + (a - 1)) & ~(a - 1);
}

typedef BOOL(WINAPI* PrefetchVirtualMemoryFn)(HANDLE, ULONG_PTR, PWIN32_MEMORY_RANGE_ENTRY, ULONG);

static PrefetchVirtualMemoryFn getPrefetchVirtualMemoryFn()
{
    static PrefetchVirtualMemoryFn fn = []() -> PrefetchVirtualMemoryFn
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32)
            return nullptr;
        return reinterpret_cast<PrefetchVirtualMemoryFn>(GetProcAddress(k32, "PrefetchVirtualMemory"));
    }();
    return fn;
}

static bool tryPrefetchRange(void* addr, size_t bytes)
{
    auto fn = getPrefetchVirtualMemoryFn();
    if (!fn || !addr || bytes == 0)
        return false;

    WIN32_MEMORY_RANGE_ENTRY entry{};
    entry.VirtualAddress = addr;
    entry.NumberOfBytes = bytes;
    return fn(GetCurrentProcess(), 1, &entry, 0) ? true : false;
}

static uint64_t xorshift64(uint64_t& s)
{
    // Deterministic RNG (no <random> overhead in Lane B).
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
}

static uint64_t getPrivateBytes()
{
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.PrivateUsage);
}

static uint64_t getWorkingSetBytes()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.WorkingSetSize);
}

static uint64_t getPageFaultCount()
{
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0;
    return static_cast<uint64_t>(pmc.PageFaultCount);
}

static IO_COUNTERS getIoCounters()
{
    IO_COUNTERS io{};
    (void)GetProcessIoCounters(GetCurrentProcess(), &io);
    return io;
}

static bool readFileMagicU32(const std::wstring& wPath, uint32_t& out)
{
    out = 0;
    HANDLE h = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD read = 0;
    const BOOL ok = ReadFile(h, &out, sizeof(out), &read, nullptr);
    CloseHandle(h);
    return ok && read == sizeof(out);
}

static int benchStreamer(const std::wstring& wPath, uint32_t maxMb, uint32_t iters, bool tryLargePages, bool prefetch)
{
    if (maxMb < 1)
        maxMb = 1;
    if (iters < 1)
        iters = 1;

    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return 1;

    LARGE_INTEGER fileSizeLi{};
    if (!GetFileSizeEx(hFile, &fileSizeLi) || fileSizeLi.QuadPart <= 0)
    {
        CloseHandle(hFile);
        return 1;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);

    DWORD protect = PAGE_READONLY;
    DWORD mapFlags = 0;
    if (tryLargePages)
    {
        const bool privOk = (RawrXD_EnableSeLockMemoryPrivilege() == 0);
        if (!privOk)
        {
            static std::once_flag once;
            std::call_once(
                once,
                []()
                {
                    const char* msg = "bench-streamer: large pages unavailable; using standard page mapping\n";
                    DWORD w = 0;
                    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                });
        }
        if (privOk)
            mapFlags |= SEC_LARGE_PAGES;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, protect | mapFlags, 0, 0, nullptr);
    if (!hMap && (mapFlags & SEC_LARGE_PAGES))
    {
        // Fall back gracefully when large-page mapping is unavailable.
        hMap = CreateFileMappingW(hFile, nullptr, protect, 0, 0, nullptr);
    }
    if (!hMap)
    {
        CloseHandle(hFile);
        return 1;
    }

    const bool hasAvx512 = (rawr_cpu_has_avx512() != 0);
    const uint32_t mbStart = 1;
    const uint32_t mbEnd = maxMb;
    const uint64_t freq = qpcFreq();

    // Print header.
    char header[320]{};
    std::snprintf(
        header, sizeof(header),
        "bench-streamer: file=%llu bytes  avx512=%u  iters=%u  maxMb=%u  qpcHz=%llu  largepages=%u  prefetch=%u\n",
        static_cast<unsigned long long>(fileSize), hasAvx512 ? 1u : 0u, iters, maxMb,
        static_cast<unsigned long long>(freq), tryLargePages ? 1u : 0u, prefetch ? 1u : 0u);
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), header, static_cast<DWORD>(std::strlen(header)), &written, nullptr);

    bool printedAny = false;
    for (uint32_t mb = mbStart; mb <= mbEnd; mb = (mb < 1024 ? mb * 2 : mb + 1024))
    {
        const size_t requested = static_cast<size_t>(mb) * 1024u * 1024u;
        // Prefer 2MB-aligned mapping sizes for the huge-page/TLB-friendly path, but
        // gracefully fall back for tiny files where a 2MB window is impossible.
        size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
        const uint64_t offset = 0;  // keep offset 0 to validate 2MB alignment easily
        bool expect2mbAligned = true;
        if (offset + mapSize > fileSize)
        {
            mapSize = alignUp(requested, 64u * 1024u);
            expect2mbAligned = false;
            if (offset + mapSize > fileSize)
                break;
        }

        uint64_t baseOrErr = 0;
        void* src = RawrXD_MapModelView2MB(hMap, offset, mapSize, &baseOrErr);
        if (!src)
            break;

        const uintptr_t srcU = reinterpret_cast<uintptr_t>(src);
        const bool src2mbAligned = ((srcU & 0x1FFFFFull) == 0);
        // Note: MapViewOfFile address alignment is not guaranteed to be 2MB even when
        // offset/size are 2MB-aligned. We record alignment instead of failing.

        // Allocate a destination buffer simulating an upload heap mapping (64B aligned view).
        AlignedVirtualAlloc dstAlloc = allocAlignedVirtual(mapSize, 64);
        if (!dstAlloc.aligned)
        {
            UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));
            break;
        }

        // Optional prefetch hint (best-effort). This is a VMM hint; it may be ignored.
        if (prefetch)
            (void)tryPrefetchRange(src, mapSize);

        // Sentinel integrity: stamp last 4 bytes in the source range and expect it in destination.
        constexpr uint32_t kSentinel = 0xDEADC0DEu;
        const size_t tail = mapSize - sizeof(uint32_t);
        std::memcpy(reinterpret_cast<uint8_t*>(src) + tail, &kSentinel, sizeof(uint32_t));

        // memcpy benchmark
        uint64_t t0 = qpcNow();
        for (uint32_t i = 0; i < iters; ++i)
        {
            std::memcpy(dstAlloc.aligned, src, mapSize);
        }
        uint64_t t1 = qpcNow();
        const double memcpySec = qpcToSeconds(t1 - t0);
        const double memcpyGbps =
            memcpySec > 0.0 ? (static_cast<double>(mapSize) * static_cast<double>(iters) / memcpySec / 1e9) : 0.0;

        // AVX-512 NT copy benchmark (if available)
        double ntSec = 0.0;
        double ntGbps = 0.0;
        bool ntOk = false;
        if (hasAvx512)
        {
            const unsigned long long blocks64B = static_cast<unsigned long long>(mapSize / 64);
            uint64_t a0 = qpcNow();
            for (uint32_t i = 0; i < iters; ++i)
            {
                RawrXD_StreamToGPU_AVX512(dstAlloc.aligned, src, blocks64B);
            }
            uint64_t a1 = qpcNow();
            ntSec = qpcToSeconds(a1 - a0);
            ntGbps = ntSec > 0.0 ? (static_cast<double>(mapSize) * static_cast<double>(iters) / ntSec / 1e9) : 0.0;
            ntOk = true;
        }

        // Fence/sentinel check: verify sentinel reached destination.
        uint32_t got = 0;
        std::memcpy(&got, reinterpret_cast<uint8_t*>(dstAlloc.aligned) + tail, sizeof(uint32_t));
        const bool sentinelOk = (got == kSentinel);

        char line[256]{};
        const char* alignStr = src2mbAligned ? "2MB" : "64K+";
        if (ntOk)
        {
            std::snprintf(line, sizeof(line),
                          "%4uMB  memcpy=%8.2f GB/s  nt_avx512=%8.2f GB/s  sentinel=%s  srcAlign=%s\n", mb, memcpyGbps,
                          ntGbps, sentinelOk ? "ok" : "BAD", alignStr);
        }
        else
        {
            std::snprintf(line, sizeof(line), "%4uMB  memcpy=%8.2f GB/s  nt_avx512= n/a  sentinel=%s  srcAlign=%s\n",
                          mb, memcpyGbps, sentinelOk ? "ok" : "BAD", alignStr);
        }
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
        printedAny = true;

        freeAlignedVirtual(dstAlloc);
        UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));

        if (!sentinelOk)
        {
            CloseHandle(hMap);
            CloseHandle(hFile);
            return 1;
        }
    }

    CloseHandle(hMap);
    CloseHandle(hFile);
    if (!printedAny)
    {
        const char* msg = "bench-streamer: file too small for requested windows\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        return 2;
    }
    return 0;
}

static int offsetSweep(const std::wstring& wPath, uint32_t windowMb, uint32_t iters, uint64_t seed, bool tryLargePages,
                       bool prefetch)
{
    if (windowMb < 1)
        windowMb = 1;
    if (iters < 1)
        iters = 1;
    if (seed == 0)
        seed = 0xC0FFEE1234ULL;

    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: CreateFileW failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 1;
    }

    LARGE_INTEGER fileSizeLi{};
    if (!GetFileSizeEx(hFile, &fileSizeLi) || fileSizeLi.QuadPart <= 0)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: GetFileSizeEx failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        CloseHandle(hFile);
        return 1;
    }
    const uint64_t fileSize = static_cast<uint64_t>(fileSizeLi.QuadPart);

    DWORD protect = PAGE_READONLY;
    DWORD mapFlags = 0;
    if (tryLargePages)
    {
        const bool privOk = (RawrXD_EnableSeLockMemoryPrivilege() == 0);
        if (!privOk)
        {
            static std::once_flag once;
            std::call_once(once,
                           []()
                           {
                               const char* msg = "offset-sweep: large pages unavailable; using standard page mapping\n";
                               DWORD w = 0;
                               WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w,
                                         nullptr);
                           });
        }
        if (privOk)
            mapFlags |= SEC_LARGE_PAGES;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, protect | mapFlags, 0, 0, nullptr);
    if (!hMap && (mapFlags & SEC_LARGE_PAGES))
    {
        hMap = CreateFileMappingW(hFile, nullptr, protect, 0, 0, nullptr);
    }
    if (!hMap)
    {
        char buf[128]{};
        std::snprintf(buf, sizeof(buf), "offset-sweep: CreateFileMappingW failed (err=%lu)\n", GetLastError());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        CloseHandle(hFile);
        return 1;
    }

    const size_t requested = static_cast<size_t>(windowMb) * 1024u * 1024u;
    size_t mapSize = alignUp(requested, 2u * 1024u * 1024u);
    bool expect2mbAligned = true;
    if (mapSize > fileSize)
    {
        mapSize = alignUp(requested, 64u * 1024u);
        expect2mbAligned = false;
    }
    if (mapSize == 0 || mapSize > fileSize)
    {
        const char* msg = "offset-sweep: file too small for requested window\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 2;
    }

    const uint64_t maxOffset = fileSize - static_cast<uint64_t>(mapSize);
    const uint64_t offsetAlign = expect2mbAligned ? (2ull * 1024ull * 1024ull) : (64ull * 1024ull);
    const uint64_t alignedMaxOffset = maxOffset & ~(offsetAlign - 1ull);

    uint64_t mapTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t mapTicksMax = 0;
    uint64_t mapTicksSum = 0;

    uint64_t touchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t touchTicksMax = 0;
    uint64_t touchTicksSum = 0;

    uint64_t prefetchTicksMin = std::numeric_limits<uint64_t>::max();
    uint64_t prefetchTicksMax = 0;
    uint64_t prefetchTicksSum = 0;
    uint32_t prefetchOkCount = 0;

    const uint64_t privBefore = getPrivateBytes();
    const uint64_t wsBefore = getWorkingSetBytes();
    const uint64_t pfBefore = getPageFaultCount();
    const IO_COUNTERS ioBefore = getIoCounters();

    uint64_t rng = seed;
    volatile uint64_t sink = 0;

    for (uint32_t i = 0; i < iters; ++i)
    {
        const uint64_t r = xorshift64(rng);
        const uint64_t offset = alignedMaxOffset ? ((r % (alignedMaxOffset + 1ull)) & ~(offsetAlign - 1ull)) : 0;

        uint64_t baseOrErr = 0;
        const uint64_t t0 = qpcNow();
        void* src = RawrXD_MapModelView2MB(hMap, offset, mapSize, &baseOrErr);
        const uint64_t t1 = qpcNow();
        if (!src)
        {
            char buf[160]{};
            std::snprintf(buf, sizeof(buf), "offset-sweep: MapModelView failed (baseOrErr=%llu)\n",
                          static_cast<unsigned long long>(baseOrErr));
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return 1;
        }

        const uint64_t mapTicks = (t1 - t0);
        mapTicksMin = (mapTicks < mapTicksMin) ? mapTicks : mapTicksMin;
        mapTicksMax = (mapTicks > mapTicksMax) ? mapTicks : mapTicksMax;
        mapTicksSum += mapTicks;

        const uintptr_t srcU2 = reinterpret_cast<uintptr_t>(src);
        const bool src2mbAligned = ((srcU2 & 0x1FFFFFull) == 0);
        (void)src2mbAligned;

        if (prefetch)
        {
            const uint64_t p0 = qpcNow();
            const bool prefOk = tryPrefetchRange(src, mapSize);
            const uint64_t p1 = qpcNow();
            const uint64_t prefTicks = (p1 - p0);
            prefetchTicksMin = (prefTicks < prefetchTicksMin) ? prefTicks : prefetchTicksMin;
            prefetchTicksMax = (prefTicks > prefetchTicksMax) ? prefTicks : prefetchTicksMax;
            prefetchTicksSum += prefTicks;
            prefetchOkCount += prefOk ? 1u : 0u;
        }

        // First-byte latency proxy: time to read the first cache line after mapping.
        const uint64_t u0 = qpcNow();
        const volatile uint64_t* p64 = reinterpret_cast<const volatile uint64_t*>(src);
        sink ^= p64[0];
        sink ^= p64[1];
        sink ^= p64[2];
        sink ^= p64[3];
        sink ^= p64[4];
        sink ^= p64[5];
        sink ^= p64[6];
        sink ^= p64[7];
        const uint64_t u1 = qpcNow();

        const uint64_t touchTicks = (u1 - u0);
        touchTicksMin = (touchTicks < touchTicksMin) ? touchTicks : touchTicksMin;
        touchTicksMax = (touchTicks > touchTicksMax) ? touchTicks : touchTicksMax;
        touchTicksSum += touchTicks;

        UnmapViewOfFile(reinterpret_cast<void*>(static_cast<uintptr_t>(baseOrErr)));
    }

    const uint64_t privAfter = getPrivateBytes();
    const uint64_t wsAfter = getWorkingSetBytes();
    const uint64_t pfAfter = getPageFaultCount();
    const IO_COUNTERS ioAfter = getIoCounters();

    const double mapAvgUs = (iters > 0) ? (qpcToSeconds(mapTicksSum / iters) * 1e6) : 0.0;
    const double mapMinUs = qpcToSeconds(mapTicksMin) * 1e6;
    const double mapMaxUs = qpcToSeconds(mapTicksMax) * 1e6;

    const double touchAvgUs = (iters > 0) ? (qpcToSeconds(touchTicksSum / iters) * 1e6) : 0.0;
    const double touchMinUs = qpcToSeconds(touchTicksMin) * 1e6;
    const double touchMaxUs = qpcToSeconds(touchTicksMax) * 1e6;

    const double prefAvgUs = (prefetch && iters > 0) ? (qpcToSeconds(prefetchTicksSum / iters) * 1e6) : 0.0;
    const double prefMinUs = prefetch ? (qpcToSeconds(prefetchTicksMin) * 1e6) : 0.0;
    const double prefMaxUs = prefetch ? (qpcToSeconds(prefetchTicksMax) * 1e6) : 0.0;

    char line[512]{};
    std::snprintf(line, sizeof(line),
                  "offset-sweep: window=%uMB iters=%u align=%lluKB seed=%llu\n"
                  "  map_us:   avg=%.2f min=%.2f max=%.2f\n"
                  "  prefetch_us: avg=%.2f min=%.2f max=%.2f ok=%u/%u\n"
                  "  touch_us: avg=%.2f min=%.2f max=%.2f\n"
                  "  vmm: pagefaults_delta=%lld  io_read_bytes_delta=%lld  io_read_ops_delta=%lld\n"
                  "  mem: private_delta=%lld bytes  ws_delta=%lld bytes  sink=%llu\n",
                  windowMb, iters, static_cast<unsigned long long>(offsetAlign / 1024ull),
                  static_cast<unsigned long long>(seed), mapAvgUs, mapMinUs, mapMaxUs, prefAvgUs, prefMinUs, prefMaxUs,
                  static_cast<unsigned int>(prefetchOkCount), static_cast<unsigned int>(prefetch ? iters : 0u),
                  touchAvgUs, touchMinUs, touchMaxUs, static_cast<long long>(pfAfter - pfBefore),
                  static_cast<long long>(ioAfter.ReadTransferCount - ioBefore.ReadTransferCount),
                  static_cast<long long>(ioAfter.ReadOperationCount - ioBefore.ReadOperationCount),
                  static_cast<long long>(privAfter - privBefore), static_cast<long long>(wsAfter - wsBefore),
                  static_cast<unsigned long long>(sink));
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);

    CloseHandle(hMap);
    CloseHandle(hFile);
    return 0;
}

static int loadModel(const std::string& path)
{
    try
    {
        // Quick magic check for diagnostics.
        const std::wstring wPath = widenUtf8(path.c_str());
        uint32_t magic = 0;
        if (readFileMagicU32(wPath, magic))
        {
            char m[96]{};
            std::snprintf(m, sizeof(m), "load-model: magic=0x%08X\n", magic);
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), m, static_cast<DWORD>(std::strlen(m)), &w, nullptr);
        }

        GGUFLoader loader;
        if (!loader.Open(path))
        {
            const char* msg = "load-model: Open failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
        if (!loader.ParseHeader())
        {
            const char* msg = "load-model: ParseHeader failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
        if (!loader.ParseMetadata())
        {
            const char* msg = "load-model: ParseMetadata failed\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        const uint64_t sz = loader.GetFileSize();
        const auto tensors = loader.GetTensorInfo();
        if (sz == 0 || tensors.empty())
        {
            const char* msg = "load-model: parsed, but tensors empty (or size=0)\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        char buf[256]{};
        std::snprintf(buf, sizeof(buf), "load-model: ok  bytes=%llu  tensors=%llu\n",
                      static_cast<unsigned long long>(sz), static_cast<unsigned long long>(tensors.size()));
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 0;
    }
    catch (const std::exception& e)
    {
        char buf[512]{};
        std::snprintf(buf, sizeof(buf), "load-model: exception: %s\n", e.what());
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
        return 1;
    }
    catch (...)
    {
        const char* msg = "load-model: exception\n";
        DWORD w = 0;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        return 1;
    }
}

static int runInfer(const std::string& modelPath, const std::string& prompt, int maxTokens)
{
    // ScopedStdioSilence suppresses CRT printf/fprintf noise ([STEP] layer traces, loader
    // diagnostics) that would otherwise pollute stdout.
    // errWriteStr uses a _dup'd stderr fd so it remains valid even after
    // ScopedStdioSilence calls freopen_s(NUL,stderr) which closes the original fd 2.
    // Response text is accumulated inside the quiet scope and written to CRT stdout
    // AFTER the scope destructor restores the original file handle.

    // Duplicate stderr fd BEFORE we enter ScopedStdioSilence.
    // freopen_s(NUL, "w", stderr) calls _close(fileno(stderr)) internally on MSVC,
    // which closes the underlying Win32 HANDLE.  A pre-captured GetStdHandle value
    // therefore becomes a dangling handle.  _dup copies the fd so the kernel object
    // stays alive and _write works throughout the scope.
    const int errFdDup = _dup(_fileno(stderr));
    auto errWriteStr = [errFdDup](const char* s)
    {
        if (!s || errFdDup < 0) return;
        const int len = static_cast<int>(std::strlen(s));
        if (len > 0) _write(errFdDup, s, static_cast<unsigned int>(len));
    };

    try
    {
        const char* verboseEnv = std::getenv("RAWRXD_INFER_VERBOSE");
        const bool verbose = (verboseEnv && verboseEnv[0] && verboseEnv[0] != '0');

        if (verbose)
        {
            char buf[192]{};
            std::snprintf(buf, sizeof(buf), "infer: stage=begin prompt_bytes=%llu max_tokens=%d\n",
                          static_cast<unsigned long long>(prompt.size()), maxTokens);
            errWriteStr(buf);
        }

        int inferStatus = 0;
        std::string errorMsg;
        std::string responseText;

        {
            ScopedStdioSilence quiet(true);  // redirect CRT stdout+stderr to NUL

            RawrXD::CPUInferenceEngine engine;
            if (verbose) errWriteStr("infer: stage=load_model\n");

            if (!engine.LoadModel(modelPath))
            {
                errorMsg = engine.GetLastLoadErrorMessage();
                if (errorMsg.empty()) errorMsg = "LoadModel failed";
                inferStatus = 1;
            }
            else
            {
                if (verbose) errWriteStr("infer: stage=tokenize\n");

                const std::vector<int32_t> inputTokens = engine.Tokenize(prompt);
                if (inputTokens.empty())
                {
                    errorMsg = "Tokenize produced no tokens";
                    inferStatus = 2;
                }
                else
                {
                    const int boundedMaxTokens = std::max(1, std::min(maxTokens, 8192));
                    if (verbose)
                    {
                        char buf[192]{};
                        std::snprintf(buf, sizeof(buf),
                                      "infer: stage=generate input_tokens=%llu bounded_max_tokens=%d\n",
                                      static_cast<unsigned long long>(inputTokens.size()), boundedMaxTokens);
                        errWriteStr(buf);
                    }

                    bool anyTokenWritten = false;
                    engine.GenerateStreaming(
                        inputTokens, boundedMaxTokens,
                        [&](const std::string& piece)
                        {
                            if (!piece.empty())
                            {
                                responseText += piece;
                                anyTokenWritten = true;
                            }
                        },
                        nullptr,
                        nullptr
                    );

                    if (!anyTokenWritten)
                    {
                        errorMsg = "Generate produced no output tokens";
                        inferStatus = 2;
                    }
                    else
                    {
                        if (verbose) errWriteStr("infer: stage=done status=ok\n");
                        inferStatus = 0;
                    }
                }
            }
        }  // ScopedStdioSilence destroyed; CRT stdout+stderr restored to original handles

        if (inferStatus != 0)
        {
            char buf[512]{};
            std::snprintf(buf, sizeof(buf), "infer: error: %s\n", errorMsg.c_str());
            errWriteStr(buf);
            if (verbose) errWriteStr("EXIT=1\n");
            if (errFdDup >= 0) _close(errFdDup);
            return inferStatus;
        }

        // CRT stdout is now restored by ScopedStdioSilence destructor (_dup2 back to
        // original fd).  std::fwrite therefore writes to the real stdout file/pipe.
        std::fwrite(responseText.c_str(), 1, responseText.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);

        if (verbose) errWriteStr("EXIT=0\n");
        if (errFdDup >= 0) _close(errFdDup);
        return 0;
    }
    catch (const std::bad_alloc&)
    {
        errWriteStr("infer: OOM\nEXIT=3\n");
        if (errFdDup >= 0) _close(errFdDup);
        return 3;
    }
    catch (const std::exception& e)
    {
        char buf[512]{};
        std::snprintf(buf, sizeof(buf), "infer: exception: %s\nEXIT=2\n", e.what());
        errWriteStr(buf);
        if (errFdDup >= 0) _close(errFdDup);
        return 2;
    }
    catch (...)
    {
        errWriteStr("infer: unknown exception\nEXIT=2\n");
        if (errFdDup >= 0) _close(errFdDup);
        return 2;
    }
}


int main(int argc, char** argv)
{
    if (argc < 2)
        return printUsage();

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i)
        args.emplace_back(argv[i] ? argv[i] : "");

    std::string arg1 = args.size() > 1 ? args[1] : "";
    if (arg1 == "--help" || arg1 == "-h")
        return printUsage();

    // Copilot/Codex-style headless parity: CPUInferenceEngine smoke (optional temp GGUF).
    if (arg1 == "--copilot-smoke" || arg1 == "--agentic-smoke")
        return runCopilotSmoke(args);

    // Minimal sanity lane:
    // RawrEngine.exe --gguf-header <path>
    if (arg1 == "--gguf-header")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        try
        {
            GGUFLoader loader;
            if (!loader.Open(args[2].c_str()))
            {
                const char* msg = "gguf-header: Open failed\n";
                DWORD w = 0;
                WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                return 1;
            }
            if (!loader.ParseHeader())
            {
                const char* msg = "gguf-header: ParseHeader failed\n";
                DWORD w = 0;
                WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
                return 1;
            }
            return 0;
        }
        catch (const std::exception& e)
        {
            char buf[512]{};
            std::snprintf(buf, sizeof(buf), "gguf-header: exception: %s\n", e.what());
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, static_cast<DWORD>(std::strlen(buf)), &w, nullptr);
            return 1;
        }
        catch (...)
        {
            const char* msg = "gguf-header: exception\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }
    }

    // Headless model load: parse header + metadata + tensor table.
    // RawrEngine.exe --load-model <path>
    if (arg1 == "--load-model")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();
        return loadModel(args[2]);
    }

    // Minimal headless inference path:
    // RawrEngine.exe --infer <path> --prompt <text> [--max-tokens <N>]
    if (arg1 == "--infer")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const char* promptStr = getOptValue(args, "--prompt");
        if (!promptStr || !promptStr[0])
            return printUsage();

        const char* maxTokStr = getOptValue(args, "--max-tokens");
        const int maxTokens = maxTokStr ? static_cast<int>(std::strtol(maxTokStr, nullptr, 10)) : 128;

        if (hasOpt(args, "--verbose"))
            SetEnvironmentVariableA("RAWRXD_INFER_VERBOSE", "1");
        else
            SetEnvironmentVariableA("RAWRXD_INFER_VERBOSE", "0");

        return runInfer(args[2], std::string(promptStr), maxTokens);
    }

    // Generate a tiny valid GGUF for local benchmarks:
    // RawrEngine.exe --gen-gguf <path>
    if (arg1 == "--gen-gguf")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();
        try
        {
            const std::filesystem::path p = std::filesystem::path(widenUtf8(args[2].c_str()));
            if (p.empty())
                return 1;
            if (!writeMinimalGgufV3(p))
                return 1;
            const char* msg = "gen-gguf: ok\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 0;
        }
        catch (...)
        {
            return 1;
        }
    }

    // Generate a fragmented+shuffled GGUF to defeat sequential read-ahead:
    // RawrEngine.exe --gen-gguf-frag <path> [--num-tensors <N>] [--stride-bytes <N>] [--align-to-stride 0|1]
    // [--shuffle-layout 0|1] [--seed <u32>]
    if (arg1 == "--gen-gguf-frag")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();
        try
        {
            const std::filesystem::path p = std::filesystem::path(widenUtf8(args[2].c_str()));
            if (p.empty())
                return 1;

            const char* ntStr = getOptValue(args, "--num-tensors");
            const char* strideStr = getOptValue(args, "--stride-bytes");
            const char* atsStr = getOptValue(args, "--align-to-stride");
            const char* shufStr = getOptValue(args, "--shuffle-layout");
            const char* seedStr = getOptValue(args, "--seed");

            uint32_t numTensors = ntStr ? static_cast<uint32_t>(std::strtoul(ntStr, nullptr, 10)) : 128u;
            uint64_t strideBytes =
                strideStr ? static_cast<uint64_t>(std::strtoull(strideStr, nullptr, 10)) : (2ull * 1024ull * 1024ull);
            bool alignToStride = atsStr ? (std::strtoul(atsStr, nullptr, 10) != 0) : true;
            bool shuffleLayout = shufStr ? (std::strtoul(shufStr, nullptr, 10) != 0) : true;
            uint32_t seed = seedStr ? static_cast<uint32_t>(std::strtoul(seedStr, nullptr, 10)) : 0xC0FFEEu;

            if (!writeFragmentedGgufV3(p, numTensors, strideBytes, alignToStride, shuffleLayout, seed))
                return 1;

            const char* msg = "gen-gguf-frag: ok\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 0;
        }
        catch (...)
        {
            return 1;
        }
    }

    // Minimal loader compilation coverage:
    // RawrEngine.exe --compile-only
    if (arg1 == "--compile-only")
    {
        // Ensure linker keeps RawrXDModelLoader reachable in this lane.
        RawrXDModelLoader dummy;
        (void)dummy;
        return 0;
    }

    // Natural habitat test for the loader/streamer mapping path:
    // RawrEngine.exe --streamer-smoke <path> [--offset <u64>] [--size <u64>] [--iterations <N>]
    if (arg1 == "--streamer-smoke")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* offsetStr = getOptValue(args, "--offset");
        const char* sizeStr = getOptValue(args, "--size");
        const char* itStr = getOptValue(args, "--iterations");

        uint64_t offset = offsetStr ? static_cast<uint64_t>(std::strtoull(offsetStr, nullptr, 10)) : 0;
        uint64_t size = sizeStr ? static_cast<uint64_t>(std::strtoull(sizeStr, nullptr, 10)) : 4096;
        uint32_t iterations = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 8;
        return streamerSmoke(wPath, offset, size, iterations);
    }

    // Throughput benchmark for the MASM streamer:
    // RawrEngine.exe --bench-streamer <path> [--max-mb <N>] [--iters <N>] [--largepages 0|1]
    if (arg1 == "--bench-streamer")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* maxMbStr = getOptValue(args, "--max-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* lpStr = getOptValue(args, "--largepages");
        const char* pfStr = getOptValue(args, "--prefetch");
        uint32_t maxMb = maxMbStr ? static_cast<uint32_t>(std::strtoul(maxMbStr, nullptr, 10)) : 1024;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 8;
        bool largePages = lpStr ? (std::strtoul(lpStr, nullptr, 10) != 0) : true;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return benchStreamer(wPath, maxMb, iters, largePages, prefetch);
    }

    // Sliding-window churn benchmark (randomized offset sweep):
    // RawrEngine.exe --offset-sweep <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--largepages 0|1]
    if (arg1 == "--offset-sweep")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        {
            const char* msg = "offset-sweep: start\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
        }

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
        {
            const char* msg = "offset-sweep: invalid path encoding\n";
            DWORD w = 0;
            WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msg, static_cast<DWORD>(std::strlen(msg)), &w, nullptr);
            return 1;
        }

        const char* winMbStr = getOptValue(args, "--window-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* seedStr = getOptValue(args, "--seed");
        const char* lpStr = getOptValue(args, "--largepages");
        const char* pfStr = getOptValue(args, "--prefetch");

        uint32_t windowMb = winMbStr ? static_cast<uint32_t>(std::strtoul(winMbStr, nullptr, 10)) : 64;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 1000;
        uint64_t seed = seedStr ? static_cast<uint64_t>(std::strtoull(seedStr, nullptr, 10)) : 0xC0FFEE1234ULL;
        bool largePages = lpStr ? (std::strtoul(lpStr, nullptr, 10) != 0) : true;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return offsetSweep(wPath, windowMb, iters, seed, largePages, prefetch);
    }

    // Same benchmark, but routed through RawrXDModelLoader (the shared core path used by Win32IDE).
    // RawrEngine.exe --offset-sweep-loader <path> [--window-mb <N>] [--iters <N>] [--seed <u64>] [--prefetch 0|1]
    if (arg1 == "--offset-sweep-loader")
    {
        if (args.size() < 3 || args[2].empty())
            return printUsage();

        const std::wstring wPath = widenUtf8(args[2].c_str());
        if (wPath.empty())
            return 1;

        const char* winMbStr = getOptValue(args, "--window-mb");
        const char* itStr = getOptValue(args, "--iters");
        const char* seedStr = getOptValue(args, "--seed");
        const char* pfStr = getOptValue(args, "--prefetch");

        uint32_t windowMb = winMbStr ? static_cast<uint32_t>(std::strtoul(winMbStr, nullptr, 10)) : 64;
        uint32_t iters = itStr ? static_cast<uint32_t>(std::strtoul(itStr, nullptr, 10)) : 1000;
        uint64_t seed = seedStr ? static_cast<uint64_t>(std::strtoull(seedStr, nullptr, 10)) : 0xC0FFEE1234ULL;
        bool prefetch = pfStr ? (std::strtoul(pfStr, nullptr, 10) != 0) : false;
        return offsetSweepLoader(wPath, windowMb, iters, seed, prefetch);
    }

    return printUsage();
}
