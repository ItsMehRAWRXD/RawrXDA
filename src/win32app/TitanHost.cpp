// RawrXD-TitanHost.cpp — Inference worker process
//
// This is a standalone .exe that:
//   1. Creates a named pipe \\.\pipe\RawrXD_TitanHost_{PPID}
//   2. Loads RawrXD_Titan.dll via the same probe logic as BackendOrchestrator
//   3. Serves inference requests from the IDE over the pipe
//
// If the DLL causes a crash (AV, alignment fault, AVX-512 fault) the whole
// host process dies — the IDE pipe client detects ERROR_BROKEN_PIPE and
// restarts TitanHost cleanly.  The IDE GUI is never affected.
//
// Build: add this file as a separate EXE target in CMakeLists.txt,
//        link with kernel32.lib only.  No CRT exceptions.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>





// ============================================================================
// Simple JSON helpers (no external deps)
// ============================================================================

static std::string JsonStr(const std::string& key, const std::string& val)
{
    // Escape backslash and double-quote in val.
    std::string esc;
    esc.reserve(val.size());
    for (unsigned char c : val)
    {
        if (c == '"')
            esc += "\\\"";
        else if (c == '\\')
            esc += "\\\\";
        else if (c == '\n')
            esc += "\\n";
        else if (c == '\r')
            esc += "\\r";
        else if (c == '\t')
            esc += "\\t";
        else
            esc += static_cast<char>(c);
    }
    return "\"" + key + "\":\"" + esc + "\"";
}

static std::string JsonBool(const std::string& key, bool val)
{
    return "\"" + key + "\":" + (val ? "true" : "false");
}

static std::string JsonInt(const std::string& key, uint32_t val)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u", val);
    return "\"" + key + "\":" + buf;
}

// Minimal JSON field extractor.  Returns "" if key not found.
static std::string ExtractJsonString(const std::string& json, const char* key)
{
    std::string needle = std::string("\"") + key + "\":\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return "";
    pos += needle.size();
    std::string out;
    while (pos < json.size())
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            ++pos;
            switch (json[pos])
            {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                default:
                    out += json[pos];
                    break;
            }
        }
        else if (json[pos] == '"')
        {
            break;
        }
        else
        {
            out += json[pos];
        }
        ++pos;
    }
    return out;
}

static uint32_t ExtractJsonUint(const std::string& json, const char* key, uint32_t def = 0)
{
    std::string needle = std::string("\"") + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return def;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ')
        ++pos;
    if (pos >= json.size() || !isdigit(static_cast<unsigned char>(json[pos])))
        return def;
    return static_cast<uint32_t>(strtoul(json.c_str() + pos, nullptr, 10));
}

// ============================================================================
// Pipe frame I/O — [uint32_t len][payload]
// ============================================================================

static bool PipeWriteFrame(HANDLE hPipe, const std::string& data)
{
    uint32_t len = static_cast<uint32_t>(data.size());
    DWORD written;
    if (!WriteFile(hPipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len))
        return false;
    if (!WriteFile(hPipe, data.data(), len, &written, nullptr) || written != len)
        return false;
    return true;
}

static bool PipeReadFrame(HANDLE hPipe, std::string& out)
{
    uint32_t len = 0;
    DWORD bytesRead;
    if (!ReadFile(hPipe, &len, sizeof(len), &bytesRead, nullptr) || bytesRead != sizeof(len))
        return false;
    if (len == 0 || len > 8 * 1024 * 1024)
        return false;
    out.resize(len);
    DWORD total = 0;
    while (total < len)
    {
        DWORD got = 0;
        if (!ReadFile(hPipe, &out[total], len - total, &got, nullptr) || got == 0)
            return false;
        total += got;
    }
    return true;
}

// ============================================================================
// Titan DLL binding  (same probing logic as BackendOrchestrator)
// ============================================================================

typedef bool(__stdcall* PFN_Submit)(const char* prompt, uint64_t* requestId);
typedef bool(__stdcall* PFN_GetResult)(uint64_t requestId, char* buf, size_t bufLen, bool* done);
typedef void*(__stdcall* PFN_CreateEngine)();
typedef int(__stdcall* PFN_LoadModel)(void* engine, const wchar_t* path);
typedef uint32_t(__stdcall* PFN_SubmitEngine)(void* engine, const char* prompt, size_t maxTok);
typedef bool(__stdcall* PFN_GetResultEngine)(void* engine, uint32_t reqId, char* buf, size_t len, bool* done);
typedef const char*(__stdcall* PFN_GetLastError)();
typedef void(__stdcall* PFN_DestroyEngine)(void* engine);

// Titan ABI (cdecl)
typedef int(__cdecl* PFN_TitanLoadModel)(const char* modelPath);
typedef int(__cdecl* PFN_TitanIsLoaded)();
typedef void(__cdecl* PFN_TitanUnloadModel)();
typedef int(__cdecl* PFN_TitanPredictChat)(const char* prompt, char* output, int outputMaxLen,
                                           int maxTokens, float temperature);

// Streaming API: callback receives UTF-8 token text; return false to cancel.
typedef bool(__cdecl* PFN_StreamCallback)(const char* tokenUtf8, void* userdata);
typedef bool(__stdcall* PFN_SubmitStreaming)(void* engine, const char* prompt, size_t maxTok,
                                             PFN_StreamCallback cb, void* userdata);

// ServeInference plugin ABI (cdecl)
typedef int(__cdecl* PFN_ServeVersion)();
typedef int(__cdecl* PFN_ServeLoadModel)(const char* pathUtf8);
typedef void(__cdecl* PFN_ServeUnloadModel)();
typedef int(__cdecl* PFN_ServeGenerate)(const char* promptUtf8, int maxTokens,
                                        void(__cdecl* onToken)(const char* utf8Fragment, int isLast, void* user),
                                        void* user);

struct TitanApi
{
    HMODULE hMod = nullptr;
    void* engineHandle = nullptr;
    bool modelLoaded = false;
    // Legacy API
    PFN_Submit submit = nullptr;
    PFN_GetResult getResult = nullptr;
    // Win32 API
    PFN_CreateEngine createEngine = nullptr;
    PFN_LoadModel loadModel = nullptr;
    PFN_SubmitEngine submitEngine = nullptr;
    PFN_GetResultEngine getResultEngine = nullptr;
    PFN_GetLastError getLastError = nullptr;
    PFN_DestroyEngine destroyEngine = nullptr;
    // Titan ABI fallback
    PFN_TitanLoadModel titanLoadModel = nullptr;
    PFN_TitanIsLoaded titanIsLoaded = nullptr;
    PFN_TitanUnloadModel titanUnloadModel = nullptr;
    PFN_TitanPredictChat titanPredictChat = nullptr;
    // Streaming API (optional — preferred when present)
    PFN_SubmitStreaming submitStreaming = nullptr;
    // ServeInference plugin API (fallback)
    PFN_ServeVersion serveVersion = nullptr;
    PFN_ServeLoadModel serveLoadModel = nullptr;
    PFN_ServeUnloadModel serveUnloadModel = nullptr;
    PFN_ServeGenerate serveGenerate = nullptr;
};

static HMODULE TryLoadDll(const wchar_t* candidate)
{
    HMODULE m = LoadLibraryW(candidate);
    if (m)
        return m;
    wchar_t exePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
        return nullptr;
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (!slash)
        slash = wcsrchr(exePath, L'/');
    if (!slash)
        return nullptr;
    *(slash + 1) = L'\0';
    std::wstring path = std::wstring(exePath) + candidate;
    return LoadLibraryW(path.c_str());
}

static bool BindTitanApi(TitanApi& api)
{
    const wchar_t* candidates[] = {L"RawrXD_Titan.dll", L"RawrXD_InferenceEngine_Win32.dll",
                                   L"RawrXD_InferenceEngine.dll", L"RawrXD_ServeInference.dll",
                                   L"rawrxd_serve_inference.dll"};
    for (const wchar_t* cand : candidates)
    {
        HMODULE m = TryLoadDll(cand);
        if (!m)
            continue;

        auto legacySubmit = (PFN_Submit)GetProcAddress(m, "SubmitInference");
        auto legacyResult = (PFN_GetResult)GetProcAddress(m, "GetInferenceResult");
        auto createEngine = (PFN_CreateEngine)GetProcAddress(m, "CreateInferenceEngine");
        auto loadModel = (PFN_LoadModel)GetProcAddress(m, "InferenceEngine_LoadModel");
        auto subEngine = (PFN_SubmitEngine)GetProcAddress(m, "InferenceEngine_SubmitInference");
        auto getResEngine = (PFN_GetResultEngine)GetProcAddress(m, "InferenceEngine_GetResult");
        auto getLastErr = (PFN_GetLastError)GetProcAddress(m, "GetLastInferenceError");
        auto destroyEngine = (PFN_DestroyEngine)GetProcAddress(m, "DestroyInferenceEngine");
        auto subStream = (PFN_SubmitStreaming)GetProcAddress(m, "InferenceEngine_SubmitStreaming");
        auto titanLoadModel = (PFN_TitanLoadModel)GetProcAddress(m, "Titan_LoadModel");
        auto titanIsLoaded = (PFN_TitanIsLoaded)GetProcAddress(m, "Titan_IsLoaded");
        auto titanUnloadModel = (PFN_TitanUnloadModel)GetProcAddress(m, "Titan_UnloadModel");
        auto titanPredictChat = (PFN_TitanPredictChat)GetProcAddress(m, "Titan_PredictChat");
        auto serveVersion = (PFN_ServeVersion)GetProcAddress(m, "RawrXD_ServeInference_Version");
        auto serveLoadModel = (PFN_ServeLoadModel)GetProcAddress(m, "RawrXD_ServeInference_LoadModel");
        auto serveUnloadModel = (PFN_ServeUnloadModel)GetProcAddress(m, "RawrXD_ServeInference_UnloadModel");
        auto serveGenerate = (PFN_ServeGenerate)GetProcAddress(m, "RawrXD_ServeInference_Generate");

        const bool hasServeAbi =
            (serveVersion && serveLoadModel && serveUnloadModel && serveGenerate && serveVersion() == 1);
        const bool hasTitanAbi = (titanLoadModel && titanPredictChat);

        if ((legacySubmit && legacyResult) || (createEngine && subEngine && getResEngine) || hasServeAbi || hasTitanAbi)
        {
            api.hMod = m;
            api.submit = legacySubmit;
            api.getResult = legacyResult;
            api.createEngine = createEngine;
            api.loadModel = loadModel;
            api.submitEngine = subEngine;
            api.getResultEngine = getResEngine;
            api.getLastError = getLastErr;
            api.destroyEngine = destroyEngine;
            api.titanLoadModel = titanLoadModel;
            api.titanIsLoaded = titanIsLoaded;
            api.titanUnloadModel = titanUnloadModel;
            api.titanPredictChat = titanPredictChat;
            api.submitStreaming = subStream;  // optional — nullptr if DLL doesn't export it
            api.serveVersion = serveVersion;
            api.serveLoadModel = serveLoadModel;
            api.serveUnloadModel = serveUnloadModel;
            api.serveGenerate = serveGenerate;
            return true;
        }
        FreeLibrary(m);
    }
    return false;
}

static void CleanupTitanApi(TitanApi& api)
{
    if (api.modelLoaded)
    {
        if (api.titanUnloadModel)
        {
            api.titanUnloadModel();
        }
        else if (api.serveUnloadModel)
        {
            api.serveUnloadModel();
        }
        api.modelLoaded = false;
    }

    if (api.engineHandle)
    {
        if (api.destroyEngine)
        {
            api.destroyEngine(api.engineHandle);
        }
        api.engineHandle = nullptr;
    }

    if (api.hMod)
    {
        FreeLibrary(api.hMod);
        api.hMod = nullptr;
    }
}

// ============================================================================
// Inference dispatcher
// ============================================================================

static std::wstring Utf8ToWide(const std::string& u8)
{
    if (u8.empty())
        return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    if (n <= 0)
        return L"";
    std::wstring out;
    out.resize(static_cast<size_t>(n - 1));
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, &out[0], n);
    return out;
}

static std::string WideToUtf8(const std::wstring& w)
{
    if (w.empty())
        return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return std::string();
    std::string out;
    out.resize(static_cast<size_t>(n - 1));
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}

static bool LoadModelAtPath(TitanApi& api, const std::wstring& path, std::string& err)
{
    if (api.titanLoadModel)
    {
        const std::string pathUtf8 = WideToUtf8(path);
        const int rc = api.titanLoadModel(pathUtf8.c_str());
        api.modelLoaded = (rc == 0);
        if (!api.modelLoaded)
        {
            err = "Titan_LoadModel failed (code " + std::to_string(rc) + ")";
            return false;
        }
        return true;
    }

    if (api.serveLoadModel)
    {
        const std::string pathUtf8 = WideToUtf8(path);
        const int rc = api.serveLoadModel(pathUtf8.c_str());
        api.modelLoaded = (rc == 0);
        if (!api.modelLoaded)
        {
            err = "RawrXD_ServeInference_LoadModel failed (code " + std::to_string(rc) + ")";
            return false;
        }
        return true;
    }

    if (!api.loadModel || !api.engineHandle)
    {
        err = "InferenceEngine_LoadModel unavailable";
        return false;
    }
    const int rc = api.loadModel(api.engineHandle, path.c_str());
    api.modelLoaded = (rc == 0);
    if (!api.modelLoaded)
    {
        err = "InferenceEngine_LoadModel failed";
        if (api.getLastError)
        {
            const char* d = api.getLastError();
            if (d && d[0])
                err += std::string(": ") + d;
        }
        return false;
    }
    return true;
}

static bool InitEngine(TitanApi& api, std::string& err)
{
    if (api.titanPredictChat)
    {
        if (api.titanIsLoaded)
        {
            api.modelLoaded = (api.titanIsLoaded() != 0);
        }
        if (api.modelLoaded)
        {
            return true;
        }

        wchar_t modelPath[2048] = {};
        const DWORD envLen = GetEnvironmentVariableW(L"RAWRXD_NATIVE_MODEL_PATH", modelPath, 2048);
        if (envLen > 0 && envLen < 2048)
        {
            return LoadModelAtPath(api, std::wstring(modelPath), err);
        }
        err = "No model loaded: use {\"cmd\":\"load_model\",\"path\":...} or set RAWRXD_NATIVE_MODEL_PATH";
        return false;
    }

    if (api.serveGenerate)
    {
        if (api.modelLoaded)
            return true;
        err = "No model loaded: use {\"cmd\":\"load_model\",\"path\":...} or set RAWRXD_NATIVE_MODEL_PATH";
        return false;
    }

    if (api.createEngine && !api.engineHandle)
    {
        api.engineHandle = api.createEngine();
        if (!api.engineHandle)
        {
            err = "CreateInferenceEngine returned null";
            return false;
        }
    }
    if (api.modelLoaded)
        return true;
    if (!api.loadModel || !api.engineHandle)
    {
        err = "Inference engine not initialized for model load";
        return false;
    }
    wchar_t modelPath[2048] = {};
    const DWORD envLen = GetEnvironmentVariableW(L"RAWRXD_NATIVE_MODEL_PATH", modelPath, 2048);
    if (envLen > 0 && envLen < 2048)
    {
        return LoadModelAtPath(api, std::wstring(modelPath), err);
    }
    err = "No model loaded: use {\"cmd\":\"load_model\",\"path\":...} or set RAWRXD_NATIVE_MODEL_PATH";
    return false;
}

// Tool-call sentinel embedded in completion text.
// Convention: if completion starts with this prefix the remainder is JSON:
//   {"name":"toolName","args":{...}}
static constexpr char kToolCallPrefix[] = "[RAWRXD_TOOL_CALL]";
static constexpr size_t kToolCallPrefixLen = sizeof(kToolCallPrefix) - 1;

// RunInference — SEH-guarded DLL dispatch with tool-call interleave.
// hPipe: server-side pipe to IDE.  seq: request sequence number.
// On structured exception the host emits a diagnostic frame then exits cleanly.
static bool RunInference(TitanApi& api, HANDLE hPipe, uint32_t seq, const std::string& prompt, uint32_t maxTokens,
                         std::string& completion, std::string& errOut)
{
    std::string initErr;
    if (!InitEngine(api, initErr))
    {
        errOut = initErr;
        return false;
    }

    const bool useLegacy = (api.submit && api.getResult);
    const bool useWin32 = (api.engineHandle && api.submitEngine && api.getResultEngine);
    const bool useServe = (api.serveGenerate != nullptr);
    const bool useTitan = (api.titanPredictChat != nullptr);
    if (!useLegacy && !useWin32 && !useServe && !useTitan)
    {
        errOut = "No usable inference API after DLL bind";
        return false;
    }

    std::string currentPrompt = prompt;
        // Tool-call interleave loop: each iteration is one inference round.
        for (int round = 0; round < 8; ++round)
        {
            uint64_t reqId = 0;
            if (useLegacy)
            {
                if (!api.submit(currentPrompt.c_str(), &reqId))
                {
                    errOut = "SubmitInference failed";
                    return false;
                }
            }
            else
            {
                if (useWin32)
                {
                    reqId = api.submitEngine(api.engineHandle, currentPrompt.c_str(), maxTokens > 0 ? maxTokens : 256);
                    if (reqId == 0)
                    {
                        errOut = "InferenceEngine_SubmitInference failed (returned 0)";
                        return false;
                    }
                }
            }

            // ---------------------------------------------------------------------------
            // Streaming path: if DLL exposes InferenceEngine_SubmitStreaming, drive it
            // through a callback that emits pipe frames per token.  Falls back to the
            // polling path when the export is absent.
            // ---------------------------------------------------------------------------
            struct StreamCtx {
                HANDLE pipe;
                uint32_t seq;
                bool pipeOk;
                std::string accumulated;
            };

            std::string roundText;

            const bool useStreamingApi = (!useLegacy && api.submitStreaming != nullptr);
            if (useStreamingApi)
            {
                StreamCtx ctx{ hPipe, static_cast<uint32_t>(seq), true, {} };
                std::string pending;
                pending.reserve(512);

                auto flushPending = [&](const std::string& payload) -> bool {
                    if (payload.empty())
                        return true;
                    std::string frame = "{\"cmd\":\"stream_token\",\"seq\":" +
                                        std::to_string(ctx.seq) + ",\"token\":";
                    frame += '"';
                    for (unsigned char ch : payload)
                    {
                        if (ch == '"') frame += "\\\"";
                        else if (ch == '\\') frame += "\\\\";
                        else if (ch == '\n') frame += "\\n";
                        else if (ch == '\r') frame += "\\r";
                        else if (ch < 0x20)
                        {
                            char esc[8];
                            snprintf(esc, sizeof(esc), "\\u%04X", ch);
                            frame += esc;
                        }
                        else frame += static_cast<char>(ch);
                    }
                    frame += "\"}";
                    if (!PipeWriteFrame(ctx.pipe, frame))
                    {
                        ctx.pipeOk = false;
                        return false;
                    }
                    return true;
                };

                auto streamCb = [](const char* tok, void* ud) -> bool {
                    auto* c = static_cast<StreamCtx*>(ud);
                    if (!tok || !tok[0]) return true;
                    c->accumulated += tok;
                    return true;
                };

                auto batchingCb = [&](const char* tok) -> bool {
                    if (!tok || !tok[0])
                        return true;
                    pending += tok;
                    if (pending.size() >= 256)
                    {
                        std::string chunk;
                        chunk.swap(pending);
                        return flushPending(chunk);
                    }
                    return true;
                };

                auto streamCbBatched = [](const char* tok, void* ud) -> bool {
                    auto* fn = static_cast<std::function<bool(const char*)>*>(ud);
                    return (*fn)(tok);
                };

                std::function<bool(const char*)> cbHolder = [&](const char* tok) -> bool {
                    if (!streamCb(tok, &ctx))
                        return false;
                    return batchingCb(tok);
                };

                api.submitStreaming(api.engineHandle, currentPrompt.c_str(),
                                    maxTokens > 0 ? maxTokens : 256,
                                    streamCbBatched, &cbHolder);
                if (!pending.empty())
                {
                    std::string chunk;
                    chunk.swap(pending);
                    if (!flushPending(chunk))
                    {
                        errOut = "Pipe write failed during streaming";
                        return false;
                    }
                }
                if (!ctx.pipeOk) {
                    errOut = "Pipe write failed during streaming";
                    return false;
                }
                roundText = ctx.accumulated;
            }
            else if (useServe)
            {
                struct ServeCtx
                {
                    HANDLE pipe;
                    uint32_t seq;
                    bool pipeOk;
                    std::string accumulated;
                };

                ServeCtx ctx{hPipe, static_cast<uint32_t>(seq), true, {}};
                std::string pending;
                pending.reserve(512);

                auto flushPending = [&](const std::string& payload) {
                    if (payload.empty())
                        return;
                    std::string frame =
                        "{\"cmd\":\"stream_token\",\"seq\":" + std::to_string(ctx.seq) + ",\"token\":";
                    frame += '"';
                    for (unsigned char ch : payload)
                    {
                        if (ch == '"')
                            frame += "\\\"";
                        else if (ch == '\\')
                            frame += "\\\\";
                        else if (ch == '\n')
                            frame += "\\n";
                        else if (ch == '\r')
                            frame += "\\r";
                        else if (ch < 0x20)
                        {
                            char esc[8];
                            snprintf(esc, sizeof(esc), "\\u%04X", ch);
                            frame += esc;
                        }
                        else
                        {
                            frame += static_cast<char>(ch);
                        }
                    }
                    frame += "\"}";
                    if (!PipeWriteFrame(ctx.pipe, frame))
                    {
                        ctx.pipeOk = false;
                    }
                };

                auto serveCb = [](const char* tok, int /*isLast*/, void* ud) -> void {
                    auto* pair = static_cast<std::pair<ServeCtx*, std::string*>*>(ud);
                    ServeCtx* c = pair->first;
                    std::string* pendingRef = pair->second;
                    const std::string piece = tok ? std::string(tok) : std::string();
                    if (piece.empty())
                        return;

                    c->accumulated += piece;
                    *pendingRef += piece;
                    if (pendingRef->size() < 256)
                        return;

                    std::string frame =
                        "{\"cmd\":\"stream_token\",\"seq\":" + std::to_string(c->seq) + ",\"token\":";
                    frame += '"';
                    for (unsigned char ch : *pendingRef)
                    {
                        if (ch == '"')
                            frame += "\\\"";
                        else if (ch == '\\')
                            frame += "\\\\";
                        else if (ch == '\n')
                            frame += "\\n";
                        else if (ch == '\r')
                            frame += "\\r";
                        else if (ch < 0x20)
                        {
                            char esc[8];
                            snprintf(esc, sizeof(esc), "\\u%04X", ch);
                            frame += esc;
                        }
                        else
                        {
                            frame += static_cast<char>(ch);
                        }
                    }
                    frame += "\"}";
                    if (!PipeWriteFrame(c->pipe, frame))
                    {
                        c->pipeOk = false;
                    }
                    pendingRef->clear();
                };

                std::pair<ServeCtx*, std::string*> serveUd{&ctx, &pending};

                const int rc = api.serveGenerate(
                    currentPrompt.c_str(), maxTokens > 0 ? static_cast<int>(maxTokens) : 256, serveCb, &serveUd);
                if (!pending.empty())
                {
                    flushPending(pending);
                    pending.clear();
                }
                if (!ctx.pipeOk)
                {
                    errOut = "Pipe write failed during ServeInference streaming";
                    return false;
                }
                if (rc != 0)
                {
                    errOut = "RawrXD_ServeInference_Generate failed (code " + std::to_string(rc) + ")";
                    return false;
                }
                roundText = ctx.accumulated;
            }
            else if (useTitan)
            {
                std::vector<char> outBuf(2 * 1024 * 1024, 0);
                const int rc = api.titanPredictChat(
                    currentPrompt.c_str(),
                    outBuf.data(),
                    static_cast<int>(outBuf.size()),
                    maxTokens > 0 ? static_cast<int>(maxTokens) : 256,
                    0.2f);
                if (rc < 0)
                {
                    errOut = "Titan_PredictChat failed (code " + std::to_string(rc) + ")";
                    return false;
                }
                roundText = outBuf.data();
            }
            else
            {
                // Polling fallback — emit stream_token frames as new bytes appear
                std::vector<char> buf(2 * 1024 * 1024, 0);
                const DWORD deadline = GetTickCount() + 120000;
                int resultPollCount = 0;
                size_t lastEmittedOffset = 0;
                while (true)
                {
                    bool done = false;
                    bool gotResult = false;
                    if (useLegacy)
                    {
                        gotResult = api.getResult(reqId, buf.data(), buf.size(), &done);
                    }
                    else
                    {
                        gotResult = api.getResultEngine(api.engineHandle, static_cast<uint32_t>(reqId), buf.data(),
                                                        buf.size(), &done);
                    }
                    // Emit any new bytes as stream_token frames
                    if (gotResult && lastEmittedOffset < buf.size())
                    {
                        const size_t newLen = strnlen(buf.data(), buf.size());
                        if (newLen > lastEmittedOffset)
                        {
                            std::string delta(buf.data() + lastEmittedOffset, newLen - lastEmittedOffset);
                            lastEmittedOffset = newLen;
                            std::string frame = "{\"cmd\":\"stream_token\",\"seq\":" +
                                                std::to_string(static_cast<uint32_t>(seq)) + ",\"token\":";
                            frame += '"';
                            for (unsigned char ch : delta) {
                                if (ch == '"') frame += "\\\"";
                                else if (ch == '\\') frame += "\\\\";
                                else if (ch == '\n') frame += "\\n";
                                else if (ch == '\r') frame += "\\r";
                                else if (ch < 0x20) { char esc[8]; snprintf(esc, sizeof(esc), "\\u%04X", ch); frame += esc; }
                                else frame += static_cast<char>(ch);
                            }
                            frame += "\"}}";
                            if (!PipeWriteFrame(hPipe, frame))
                            {
                                errOut = "Pipe write failed emitting stream_token";
                                return false;
                            }
                        }
                    }
                    if (gotResult && done)
                        break;
                    if (GetTickCount() >= deadline)
                    {
                        errOut = "Inference timed out (120 s)";
                        return false;
                    }
                    if (resultPollCount < 32)
                    {
                        Sleep(0);
                    }
                    else
                    {
                        Sleep(1);
                    }
                    ++resultPollCount;
                }
                roundText = buf.data();
            }

            // Check for embedded tool call: "[RAWRXD_TOOL_CALL]{...json...}"
            if (roundText.size() > kToolCallPrefixLen && roundText.compare(0, kToolCallPrefixLen, kToolCallPrefix) == 0)
            {

                std::string toolJson = roundText.substr(kToolCallPrefixLen);
                // Forward tool call to IDE as a framed message.
                std::string toolFrame = "{\"cmd\":\"tool_call\"," + JsonInt("seq", seq) + "," +
                                        JsonInt("round", static_cast<uint32_t>(round)) + "," +
                                        JsonStr("payload", toolJson) + "}";
                if (!PipeWriteFrame(hPipe, toolFrame))
                {
                    errOut = "Pipe write failed sending tool_call";
                    return false;
                }

                // Wait for IDE to execute the tool and send tool_result back.
                std::string toolResultFrame;
                const DWORD toolDeadline = GetTickCount() + 30000;
                int toolPollCount = 0;
                while (true)
                {
                    DWORD avail = 0;
                    if (PeekNamedPipe(hPipe, nullptr, 0, nullptr, &avail, nullptr) && avail >= 4)
                        break;
                    if (GetTickCount() >= toolDeadline)
                    {
                        errOut = "Tool-call timeout: IDE did not respond within 30 s";
                        return false;
                    }
                    if (toolPollCount < 16)
                    {
                        Sleep(0);
                    }
                    else
                    {
                        Sleep(1);
                    }
                    ++toolPollCount;
                }

                if (!PipeReadFrame(hPipe, toolResultFrame))
                {
                    errOut = "Pipe read failed reading tool_result";
                    return false;
                }

                // Inject tool result into next prompt round.
                std::string toolResult = ExtractJsonString(toolResultFrame, "result");
                currentPrompt = currentPrompt + "\n[TOOL_RESULT]" + toolResult + "\n[CONTINUE]";
                continue;  // next inference round
            }

            // Normal completion — no tool call.
            completion = roundText;
            return true;
        }

    errOut = "Tool-call round limit exceeded (8 rounds)";
    return false;
}

// ============================================================================
// Named pipe server loop
// ============================================================================

static std::string BuildPipeName()
{
    // Parent PID is passed as command-line argument: TitanHost.exe <PPID>
    static char pipeName[64] = {};
    if (pipeName[0] == '\0')
    {
        DWORD ppid = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), (int*)&ppid);
        if (argv && ppid >= 2)
        {  // argc>=2
            ppid = static_cast<DWORD>(wcstoul(argv[1], nullptr, 10));
            LocalFree(argv);
        }
        snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\RawrXD_TitanHost_%u", ppid);
    }
    return pipeName;
}

int main(int /*argc*/, char* /*argv*/[])
{
    // Parse command line for parent PID.
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    DWORD parentPid = 0;
    if (wargc >= 2)
        parentPid = static_cast<DWORD>(wcstoul(wargv[1], nullptr, 10));
    if (wargv)
        LocalFree(wargv);

    if (parentPid == 0)
    {
        ExitProcess(4);
    }

    HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (!hParent)
    {
        ExitProcess(4);
    }
    if (WaitForSingleObject(hParent, 0) == WAIT_OBJECT_0)
    {
        CloseHandle(hParent);
        ExitProcess(4);
    }

    char pipeName[64];
    snprintf(pipeName, sizeof(pipeName), "\\\\.\\pipe\\RawrXD_TitanHost_%u", parentPid);

    // Bind Titan DLL immediately — crash here just kills this small process.
    TitanApi api;
    std::string currentModelPathUtf8;
    std::string bindErr;
    if (!BindTitanApi(api))
    {
        // Signal IDE we couldn't load the DLL by exiting with code 2.
        ExitProcess(2);
    }

    // Create the named pipe (message-mode, single instance per IDE process).
    HANDLE hPipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                    1,       // max instances
                                    65536,   // out buffer
                                    65536,   // in buffer
                                    0,       // default timeout (50 ms)
                                    nullptr  // default security
    );

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hParent);
        ExitProcess(3);
    }

    // Signal readiness via event named after the pipe (IDE waits on this).
    char evName[80];
    snprintf(evName, sizeof(evName), "RawrXD_TitanReady_%u", parentPid);
    HANDLE hReady = CreateEventA(nullptr, TRUE, FALSE, evName);

    // Accept one connection, but abort if parent process exits first.
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent)
    {
        if (hReady)
            CloseHandle(hReady);
        CloseHandle(hPipe);
        CloseHandle(hParent);
        ExitProcess(5);
    }

    bool connected = false;
    if (ConnectNamedPipe(hPipe, &ov))
    {
        connected = true;
    }
    else
    {
        const DWORD connectErr = GetLastError();
        if (connectErr == ERROR_PIPE_CONNECTED)
        {
            connected = true;
        }
        else if (connectErr == ERROR_IO_PENDING)
        {
            HANDLE waitHandles[2] = {ov.hEvent, hParent};
            const DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, 30000);
            if (wr == WAIT_OBJECT_0)
            {
                connected = true;
            }
            else
            {
                CancelIoEx(hPipe, &ov);
            }
        }
    }

    CloseHandle(ov.hEvent);

    if (!connected)
    {
        if (hReady)
            CloseHandle(hReady);
        CloseHandle(hPipe);
        CloseHandle(hParent);
        ExitProcess(6);
    }

    if (hReady)
    {
        SetEvent(hReady);
        CloseHandle(hReady);
    }

    // Serve requests.
    while (true)
    {
        if (WaitForSingleObject(hParent, 0) == WAIT_OBJECT_0)
            break;

        std::string req;
        if (!PipeReadFrame(hPipe, req))
            break;  // pipe broken → exit → IDE detects

        std::string cmd = ExtractJsonString(req, "cmd");
        if (cmd == "ping")
        {
            std::string resp = "{" + JsonBool("pong", true) + "," + JsonStr("build", "TitanHost/1.0") + "}";
            PipeWriteFrame(hPipe, resp);
            continue;
        }
        if (cmd == "exit")
        {
            std::string resp = "{\"bye\":true}";
            PipeWriteFrame(hPipe, resp);
            break;
        }
        if (cmd == "load_model")
        {
            const std::string pathU8 = ExtractJsonString(req, "path");
            std::string resp;
            if (pathU8.empty())
            {
                resp = "{" + JsonBool("ok", false) + "," + JsonStr("error", "load_model: empty path") + "}";
            }
            else
            {
                const std::wstring wpath = Utf8ToWide(pathU8);
                if (wpath.empty())
                {
                    resp = "{" + JsonBool("ok", false) + "," +
                           JsonStr("error", "load_model: UTF-8 path conversion failed") + "}";
                }
                else
                {
                    if (api.modelLoaded && pathU8 != currentModelPathUtf8)
                    {
                        if (api.titanUnloadModel)
                        {
                            api.titanUnloadModel();
                            api.modelLoaded = false;
                        }
                        else if (api.serveUnloadModel)
                        {
                            api.serveUnloadModel();
                            api.modelLoaded = false;
                        }
                    }

                    if (api.serveLoadModel || api.titanLoadModel)
                    {
                        std::string lmErr;
                        if (LoadModelAtPath(api, wpath, lmErr))
                        {
                            currentModelPathUtf8 = pathU8;
                            resp = "{" + JsonBool("ok", true) + "}";
                        }
                        else
                        {
                            resp = "{" + JsonBool("ok", false) + "," + JsonStr("error", lmErr) + "}";
                        }
                    }
                    else if (!api.createEngine && !api.engineHandle)
                    {
                        resp = "{" + JsonBool("ok", false) + "," +
                               JsonStr("error", "load_model: no CreateInferenceEngine in DLL") + "}";
                    }
                    else
                    {
                        if (!api.engineHandle && api.createEngine)
                        {
                            api.engineHandle = api.createEngine();
                        }
                        if (!api.engineHandle)
                        {
                            resp = "{" + JsonBool("ok", false) + "," +
                                   JsonStr("error", "CreateInferenceEngine returned null") + "}";
                        }
                        else if (!api.loadModel)
                        {
                            resp = "{" + JsonBool("ok", false) + "," +
                                   JsonStr("error", "InferenceEngine_LoadModel missing from DLL") + "}";
                        }
                        else
                        {
                            std::string lmErr;
                            if (LoadModelAtPath(api, wpath, lmErr))
                            {
                                currentModelPathUtf8 = pathU8;
                                resp = "{" + JsonBool("ok", true) + "}";
                            }
                            else
                            {
                                resp = "{" + JsonBool("ok", false) + "," + JsonStr("error", lmErr) + "}";
                            }
                        }
                    }
                }
            }
            if (!PipeWriteFrame(hPipe, resp))
                break;
            continue;
        }
        if (cmd == "infer")
        {
            std::string prompt = ExtractJsonString(req, "prompt");
            uint32_t maxTok = ExtractJsonUint(req, "max_tokens", 256);
            uint32_t seq = ExtractJsonUint(req, "seq", 0);
            std::string completion, error;

            bool ok = RunInference(api, hPipe, seq, prompt, maxTok, completion, error);
            std::string resp;
            if (ok)
            {
                resp = "{" + JsonInt("seq", seq) + "," + JsonBool("ok", true) + "," +
                       JsonStr("completion", completion) + "," + JsonStr("metadata", "") + "}";
            }
            else
            {
                resp = "{" + JsonInt("seq", seq) + "," + JsonBool("ok", false) + "," + JsonStr("error", error) + "}";
            }
            if (!PipeWriteFrame(hPipe, resp))
                break;
            continue;
        }
        // Unknown command — reply with error.
        std::string resp = "{" + JsonBool("ok", false) + "," + JsonStr("error", "unknown cmd: " + cmd) + "}";
        if (!PipeWriteFrame(hPipe, resp))
            break;
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    CloseHandle(hParent);
    CleanupTitanApi(api);
    return 0;
}
