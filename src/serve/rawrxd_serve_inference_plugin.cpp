// rawrxd_serve_inference_plugin.cpp — Optional inference DLL for RawrXD-Serve (minimal httpapi binary).
//
// Companion DLL must export (cdecl):
//   int  RawrXD_ServeInference_Version(void);   // return 1
//   int  RawrXD_ServeInference_LoadModel(const char* utf8Path);
//   void RawrXD_ServeInference_UnloadModel(void);
//   int  RawrXD_ServeInference_Generate(const char* promptUtf8, int maxTokens,
//        void(__cdecl* onToken)(const char* utf8Fragment, int isLast, void* user),
//        void* user);
#include "rawrxd_serve_inference_plugin.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>
#include <vector>

namespace RawrXD
{
namespace Serve
{
namespace InferencePlugin
{

namespace
{

using PFN_Version = int(__cdecl*)();
using PFN_LoadModel = int(__cdecl*)(const char* pathUtf8);
using PFN_UnloadModel = void(__cdecl*)();
using PFN_Generate = int(__cdecl*)(const char* promptUtf8, int maxTokens,
                                   void(__cdecl* onToken)(const char* utf8Fragment, int isLast, void* user),
                                   void* user);

HMODULE g_hMod = nullptr;
PFN_Version g_version = nullptr;
PFN_LoadModel g_loadModel = nullptr;
PFN_UnloadModel g_unloadModel = nullptr;
PFN_Generate g_generate = nullptr;

static bool BindExports()
{
    g_version = reinterpret_cast<PFN_Version>(GetProcAddress(g_hMod, "RawrXD_ServeInference_Version"));
    g_loadModel = reinterpret_cast<PFN_LoadModel>(GetProcAddress(g_hMod, "RawrXD_ServeInference_LoadModel"));
    g_unloadModel = reinterpret_cast<PFN_UnloadModel>(GetProcAddress(g_hMod, "RawrXD_ServeInference_UnloadModel"));
    g_generate = reinterpret_cast<PFN_Generate>(GetProcAddress(g_hMod, "RawrXD_ServeInference_Generate"));
    return g_version && g_loadModel && g_unloadModel && g_generate && g_version() == 1;
}

static void ClearExports()
{
    g_version = nullptr;
    g_loadModel = nullptr;
    g_unloadModel = nullptr;
    g_generate = nullptr;
}

static bool TryOneDll(const wchar_t* path, std::string& detailOut)
{
    if (!path || path[0] == L'\0')
        return false;
    HMODULE h = LoadLibraryW(path);
    if (!h)
        return false;
    g_hMod = h;
    if (!BindExports())
    {
        FreeLibrary(g_hMod);
        g_hMod = nullptr;
        ClearExports();
        return false;
    }
    char narrow[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, static_cast<int>(sizeof(narrow)), nullptr, nullptr);
    detailOut = std::string("loaded inference plugin: ") + narrow;
    return true;
}

static void AppendCandidate(std::vector<std::wstring>& out, std::wstring p)
{
    if (p.empty())
        return;
    for (const auto& e : out)
    {
        if (e == p)
            return;
    }
    out.push_back(std::move(p));
}

}  // namespace

bool tryLoad(std::string& detailOut)
{
    unloadDll();
    detailOut.clear();

    wchar_t envBuf[2048] = {};
    DWORD el = GetEnvironmentVariableW(L"RAWRXD_SERVE_INFERENCE_DLL", envBuf, static_cast<DWORD>(std::size(envBuf)));
    std::vector<std::wstring> candidates;
    if (el > 0 && el < std::size(envBuf))
        AppendCandidate(candidates, envBuf);

    wchar_t exePath[MAX_PATH] = {};
    DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
    {
        wchar_t* slash = wcsrchr(exePath, L'\\');
        if (!slash)
            slash = wcsrchr(exePath, L'/');
        if (slash)
        {
            *(slash + 1) = L'\0';
            std::wstring base(exePath);
            AppendCandidate(candidates, base + L"RawrXD_ServeInference.dll");
            AppendCandidate(candidates, base + L"rawrxd_serve_inference.dll");
            AppendCandidate(candidates, base + L"RawrXD_CPUInference_ServePlugin.dll");
        }
    }

    for (const auto& c : candidates)
    {
        if (TryOneDll(c.c_str(), detailOut))
            return true;
    }
    detailOut =
        "no inference plugin (set RAWRXD_SERVE_INFERENCE_DLL or ship RawrXD_ServeInference.dll next to rawrxd.exe)";
    return false;
}

void unloadDll()
{
    if (g_hMod)
    {
        if (g_unloadModel)
            g_unloadModel();
        FreeLibrary(g_hMod);
        g_hMod = nullptr;
    }
    ClearExports();
}

bool hasPlugin()
{
    return g_hMod && g_generate != nullptr;
}

bool loadModel(const std::string& pathUtf8, std::string& err)
{
    err.clear();
    if (!hasPlugin())
    {
        err = "inference plugin not loaded";
        return false;
    }
    if (pathUtf8.empty())
    {
        err = "empty model path";
        return false;
    }
    const int rc = g_loadModel(pathUtf8.c_str());
    if (rc != 0)
    {
        err = "RawrXD_ServeInference_LoadModel failed (code " + std::to_string(rc) + ")";
        return false;
    }
    return true;
}

void unloadModel()
{
    if (g_unloadModel)
        g_unloadModel();
}

struct GenerateCtx
{
    std::string accumulated;
    StreamTokenFn onToken;
};

static void __cdecl BridgeOnToken(const char* utf8Fragment, int isLast, void* user)
{
    auto* ctx = static_cast<GenerateCtx*>(user);
    const std::string piece = utf8Fragment ? utf8Fragment : "";
    ctx->accumulated += piece;
    if (ctx->onToken)
        ctx->onToken(piece, isLast != 0);
}

std::string generate(const GenerateRequest& req, StreamTokenFn onToken, std::string& err)
{
    err.clear();
    if (!hasPlugin())
    {
        err = "inference plugin not loaded";
        return {};
    }
    std::string prompt = req.prompt;
    if (prompt.empty() && !req.messages.empty())
    {
        for (const auto& m : req.messages)
        {
            if (m.role == "user" || m.role == "assistant" || m.role == "system")
            {
                if (!prompt.empty())
                    prompt += "\n";
                prompt += m.content;
            }
        }
    }
    if (prompt.empty())
    {
        err = "empty prompt";
        return {};
    }
    const int maxTok = req.num_predict > 0 ? req.num_predict : 256;
    GenerateCtx ctx;
    ctx.onToken = std::move(onToken);
    const int rc = g_generate(prompt.c_str(), maxTok, BridgeOnToken, &ctx);
    if (rc != 0)
    {
        err = "RawrXD_ServeInference_Generate failed (code " + std::to_string(rc) + ")";
        return {};
    }
    return ctx.accumulated;
}

}  // namespace InferencePlugin
}  // namespace Serve
}  // namespace RawrXD
