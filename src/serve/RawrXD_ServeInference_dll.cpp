// RawrXD_ServeInference_dll.cpp — Companion DLL for rawrxd.exe (Ollama-compatible HTTP server).
// Links InferenceEngine (CPU GGUF path). Exports match rawrxd_serve_inference_plugin.cpp.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "cpu_inference_engine.h"

#ifdef RAWRXD_ServeInference_EXPORTS
#define SIRVE_API __declspec(dllexport)
#else
#define SIRVE_API __declspec(dllimport)
#endif

extern "C"
{

    SIRVE_API int __cdecl RawrXD_ServeInference_Version(void)
    {
        return 1;
    }

    SIRVE_API int __cdecl RawrXD_ServeInference_LoadModel(const char* pathUtf8)
    {
        if (!pathUtf8 || !pathUtf8[0])
            return -1;
        try
        {
            auto eng = RawrXD::CPUInferenceEngine::GetSharedInstance();
            if (!eng)
                return -2;
            return eng->LoadModel(std::string(pathUtf8)) ? 0 : -3;
        }
        catch (...)
        {
            return -4;
        }
    }

    SIRVE_API void __cdecl RawrXD_ServeInference_UnloadModel(void)
    {
        // Shared singleton engine: no separate unload API; next LoadModel replaces weights.
    }

    SIRVE_API int __cdecl RawrXD_ServeInference_Generate(const char* promptUtf8, int maxTokens,
                                                         void(__cdecl* onToken)(const char* utf8Fragment, int isLast,
                                                                                void* user),
                                                         void* user)
    {
        if (!onToken)
            return -1;
        try
        {
            auto eng = RawrXD::CPUInferenceEngine::GetSharedInstance();
            if (!eng || !eng->IsModelLoaded())
                return -2;

            const std::string prompt = promptUtf8 ? std::string(promptUtf8) : std::string();
            if (prompt.empty())
            {
                onToken("", 1, user);
                return 0;
            }

            const std::vector<int32_t> toks = eng->Tokenize(prompt);
            const int mt = maxTokens > 0 ? maxTokens : 256;

            eng->GenerateStreaming(
                toks, mt,
                [&](const std::string& piece)
                {
                    if (!piece.empty())
                        onToken(piece.c_str(), 0, user);
                },
                [&]() { onToken("", 1, user); });

            return 0;
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "[RawrXD_ServeInference] %s\n", ex.what());
            return -5;
        }
        catch (...)
        {
            return -6;
        }
    }

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        DisableThreadLibraryCalls(hModule);
    return TRUE;
}
