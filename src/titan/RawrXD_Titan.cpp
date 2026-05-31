// ============================================================
// RAWRXD TITAN DLL — Thin C-wrapper around llama.cpp
// ============================================================

#include <windows.h>
#include <string>
#include <cstring>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) int  Titan_LoadModel(const char* model_path);
__declspec(dllexport) int  Titan_IsLoaded(void);
__declspec(dllexport) void Titan_UnloadModel(void);
__declspec(dllexport) int  Titan_PredictFIM(const char* prefix, const char* suffix, char* output, int output_max_len, int max_tokens, float temperature);
__declspec(dllexport) int  Titan_PredictChat(const char* prompt, char* output, int output_max_len, int max_tokens, float temperature);

#ifdef __cplusplus
}
#endif

static HMODULE g_hLlamaDll = NULL;
static void* g_model = NULL;
static void* g_ctx = NULL;
static bool g_loaded = false;
static char g_last_error[512] = {0};

typedef void* (*llama_load_model_from_file_t)(const char* path, void* params);
typedef void* (*llama_new_context_with_model_t)(void* model, void* params);
typedef void  (*llama_free_t)(void* ctx);
typedef void  (*llama_free_model_t)(void* model);
typedef int   (*llama_tokenize_t)(void* model, const char* text, int* tokens, int n_max_tokens, bool add_bos, bool special);
typedef int   (*llama_detokenize_t)(void* model, const int* tokens, int n_tokens, char* buf, int buf_size, bool remove_special, bool unparse_special);
typedef int   (*llama_n_vocab_t)(void* model);
typedef int   (*llama_n_ctx_t)(void* ctx);
typedef void* (*llama_get_logits_t)(void* ctx);

static llama_load_model_from_file_t p_llama_load_model_from_file = NULL;
static llama_new_context_with_model_t p_llama_new_context_with_model = NULL;
static llama_free_t p_llama_free = NULL;
static llama_free_model_t p_llama_free_model = NULL;
static llama_tokenize_t p_llama_tokenize = NULL;
static llama_detokenize_t p_llama_detokenize = NULL;
static llama_n_vocab_t p_llama_n_vocab = NULL;
static llama_n_ctx_t p_llama_n_ctx = NULL;
static llama_get_logits_t p_llama_get_logits = NULL;

static void safe_strcpy(char* dst, int dst_size, const char* src) {
    if (!dst || dst_size <= 0) return;
    if (!src) { dst[0] = 0; return; }
    int i = 0;
    while (i < dst_size - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void set_error(const char* msg) {
    safe_strcpy(g_last_error, sizeof(g_last_error), msg);
    OutputDebugStringA("[Titan] ERROR: ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

static bool init_llama_backend(void) {
    if (g_hLlamaDll) return true;
    const char* paths[] = { "llama.dll", "D:\\rawrxd\\bin\\llama.dll", NULL };
    for (int i = 0; paths[i]; i++) {
        g_hLlamaDll = LoadLibraryA(paths[i]);
        if (g_hLlamaDll) break;
    }
    if (!g_hLlamaDll) { set_error("Failed to load llama.dll"); return false; }
    p_llama_load_model_from_file = (llama_load_model_from_file_t)GetProcAddress(g_hLlamaDll, "llama_load_model_from_file");
    p_llama_new_context_with_model = (llama_new_context_with_model_t)GetProcAddress(g_hLlamaDll, "llama_new_context_with_model");
    p_llama_free = (llama_free_t)GetProcAddress(g_hLlamaDll, "llama_free");
    p_llama_free_model = (llama_free_model_t)GetProcAddress(g_hLlamaDll, "llama_free_model");
    p_llama_tokenize = (llama_tokenize_t)GetProcAddress(g_hLlamaDll, "llama_tokenize");
    p_llama_detokenize = (llama_detokenize_t)GetProcAddress(g_hLlamaDll, "llama_detokenize");
    p_llama_n_vocab = (llama_n_vocab_t)GetProcAddress(g_hLlamaDll, "llama_n_vocab");
    p_llama_n_ctx = (llama_n_ctx_t)GetProcAddress(g_hLlamaDll, "llama_n_ctx");
    p_llama_get_logits = (llama_get_logits_t)GetProcAddress(g_hLlamaDll, "llama_get_logits");
    if (!p_llama_load_model_from_file || !p_llama_new_context_with_model || !p_llama_tokenize) {
        set_error("llama.dll missing required exports");
        FreeLibrary(g_hLlamaDll); g_hLlamaDll = NULL;
        return false;
    }
    return true;
}

__declspec(dllexport) int Titan_LoadModel(const char* model_path) {
    if (!model_path || !model_path[0]) { set_error("null model_path"); return 0; }
    if (g_loaded) Titan_UnloadModel();
    if (!init_llama_backend()) return 0;
    unsigned char mparams[64] = {0};
    *(int*)(mparams + 0) = 0;
    g_model = p_llama_load_model_from_file(model_path, mparams);
    if (!g_model) { set_error("llama_load_model_from_file failed"); return 0; }
    unsigned char cparams[128] = {0};
    *(int*)(cparams + 0) = 4096;
    g_ctx = p_llama_new_context_with_model(g_model, cparams);
    if (!g_ctx) { p_llama_free_model(g_model); g_model = NULL; set_error("llama_new_context_with_model failed"); return 0; }
    g_loaded = true;
    return 1;
}

__declspec(dllexport) int Titan_IsLoaded(void) { return g_loaded ? 1 : 0; }

__declspec(dllexport) void Titan_UnloadModel(void) {
    if (g_ctx && p_llama_free) { p_llama_free(g_ctx); g_ctx = NULL; }
    if (g_model && p_llama_free_model) { p_llama_free_model(g_model); g_model = NULL; }
    g_loaded = false;
}

static int generate_tokens(const char* prompt, char* output, int output_max_len, int max_tokens, float temperature) {
    if (!g_loaded || !g_model || !g_ctx) { safe_strcpy(output, output_max_len, "// Titan: model not loaded"); return -1; }
    if (!prompt || !output || output_max_len <= 0) return -1;
    int n_vocab = p_llama_n_vocab(g_model);
    int n_ctx = p_llama_n_ctx(g_ctx);
    int* tokens = (int*)malloc(n_ctx * sizeof(int));
    if (!tokens) { safe_strcpy(output, output_max_len, "// Titan: malloc failed"); return -1; }
    int n_tokens = p_llama_tokenize(g_model, prompt, tokens, n_ctx, true, true);
    if (n_tokens < 0) { free(tokens); safe_strcpy(output, output_max_len, "// Titan: tokenize failed"); return -1; }
    std::string result;
    int n_gen = 0;
    for (int i = 0; i < max_tokens && n_tokens < n_ctx; i++) {
        float* logits = (float*)p_llama_get_logits(g_ctx);
        if (!logits) break;
        int best_token = 0;
        float best_logit = logits[0];
        for (int v = 1; v < n_vocab; v++) { if (logits[v] > best_logit) { best_logit = logits[v]; best_token = v; } }
        if (best_token == 2) break;
        tokens[n_tokens++] = best_token; n_gen++;
        char buf[32] = {0};
        p_llama_detokenize(g_model, &best_token, 1, buf, sizeof(buf), false, false);
        result += buf;
        if (result.find("<|fim_") != std::string::npos) break;
        if (result.find("<|endoftext|>") != std::string::npos) break;
    }
    free(tokens);
    safe_strcpy(output, output_max_len, result.c_str());
    return n_gen;
}

__declspec(dllexport) int Titan_PredictFIM(const char* prefix, const char* suffix, char* output, int output_max_len, int max_tokens, float temperature) {
    if (!prefix) prefix = "";
    if (!suffix) suffix = "";
    std::string prompt = std::string("<|fim_prefix|>") + prefix + "<|fim_suffix|>" + suffix + "<|fim_middle|>";
    return generate_tokens(prompt.c_str(), output, output_max_len, max_tokens, temperature);
}

__declspec(dllexport) int Titan_PredictChat(const char* prompt, char* output, int output_max_len, int max_tokens, float temperature) {
    if (!prompt) prompt = "";
    return generate_tokens(prompt, output, output_max_len, max_tokens, temperature);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(hModule);
    else if (reason == DLL_PROCESS_DETACH) Titan_UnloadModel();
    return TRUE;
}
