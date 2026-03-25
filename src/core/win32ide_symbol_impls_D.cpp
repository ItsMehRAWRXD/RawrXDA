// win32ide_symbol_impls_D.cpp — RawrXD IDE debug agentic symbol implementations

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <algorithm>

struct LSP_SYMBOL_ENTRY {
    char name[128];
    uint32_t kind;      // 1=function, 2=type, 3=var
    uint32_t line;
    uint32_t col;
    uint32_t reserved;
};

struct LSP_STATS_OUT {
    uint32_t symbol_count;
    uint32_t query_count;
    float    syntax_weight;
    float    semantic_weight;
    uint32_t initialized;
    uint32_t reserved;
};

static LSP_SYMBOL_ENTRY g_lsp_symbols[4096] = {};
static uint32_t g_lsp_symbol_count   = 0;
static uint32_t g_lsp_query_count    = 0;
static float    g_lsp_syntax_weight  = 0.5f;
static float    g_lsp_semantic_weight = 0.5f;
static bool     g_lsp_initialized    = false;
static CRITICAL_SECTION g_lsp_cs;
static bool     g_lsp_cs_init        = false;

static void lsp_ensure_cs()
{
    if (!g_lsp_cs_init) {
        InitializeCriticalSection(&g_lsp_cs);
        g_lsp_cs_init = true;
    }
}

extern "C" {

// 1. asm_lsp_bridge_init
// Initialises the LSP bridge. If symbolIndex is non-null it is treated as a
// contiguous LSP_SYMBOL_ENTRY array; up to 4096 entries are copied in.
int asm_lsp_bridge_init(void* symbolIndex, void* /*contextAnalyzer*/)
{
    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    if (symbolIndex != nullptr) {
        // Caller supplies a pre-filled symbol table; copy it wholesale.
        // We always copy the maximum capacity — callers that supply fewer
        // entries should zero-pad the remainder (kind==0 acts as sentinel).
        const LSP_SYMBOL_ENTRY* src = static_cast<const LSP_SYMBOL_ENTRY*>(symbolIndex);
        uint32_t count = 0;
        for (uint32_t i = 0; i < 4096; ++i) {
            if (src[i].kind == 0 && src[i].name[0] == '\0') {
                break;          // hit zero sentinel — stop early
            }
            g_lsp_symbols[i] = src[i];
            ++count;
        }
        g_lsp_symbol_count = count;
    }

    g_lsp_initialized = true;
    LeaveCriticalSection(&g_lsp_cs);
    return 0;
}

// 2. asm_lsp_bridge_sync
// mode 0 = full sync (resets query counter)
// mode 1 = incremental (keeps query counter)
int asm_lsp_bridge_sync(uint32_t mode)
{
    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    if (mode == 0) {
        g_lsp_query_count = 0;
    }
    // mode 1 (incremental) — nothing extra to do in this layer; the
    // higher-level symbol provider pushes deltas through bridge_init.

    LeaveCriticalSection(&g_lsp_cs);
    return 0;
}

// 3. asm_lsp_bridge_query
// Copies up to maxSymbols entries into resultBuf and sets *outCount.
int asm_lsp_bridge_query(void* resultBuf, uint32_t maxSymbols, uint32_t* outCount)
{
    if (resultBuf == nullptr) {
        if (outCount) *outCount = 0;
        return -1;
    }

    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    uint32_t toCopy = (g_lsp_symbol_count < maxSymbols) ? g_lsp_symbol_count : maxSymbols;
    if (toCopy > 0) {
        memcpy(resultBuf, g_lsp_symbols, toCopy * sizeof(LSP_SYMBOL_ENTRY));
    }
    if (outCount) {
        *outCount = toCopy;
    }
    ++g_lsp_query_count;

    LeaveCriticalSection(&g_lsp_cs);
    return 0;
}

// 4. asm_lsp_bridge_invalidate
// Clears the symbol table and resets the query counter.
void asm_lsp_bridge_invalidate(void)
{
    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    memset(g_lsp_symbols, 0, sizeof(g_lsp_symbols));
    g_lsp_symbol_count = 0;
    g_lsp_query_count  = 0;

    LeaveCriticalSection(&g_lsp_cs);
}

// 5. asm_lsp_bridge_get_stats
// Fills an LSP_STATS_OUT structure with a snapshot of current state.
void asm_lsp_bridge_get_stats(void* statsOut)
{
    if (statsOut == nullptr) return;

    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    LSP_STATS_OUT* out   = static_cast<LSP_STATS_OUT*>(statsOut);
    out->symbol_count    = g_lsp_symbol_count;
    out->query_count     = g_lsp_query_count;
    out->syntax_weight   = g_lsp_syntax_weight;
    out->semantic_weight = g_lsp_semantic_weight;
    out->initialized     = g_lsp_initialized ? 1u : 0u;
    out->reserved        = 0u;

    LeaveCriticalSection(&g_lsp_cs);
}

// 6. asm_lsp_bridge_set_weights
// Updates syntax/semantic blend weights; both are clamped to [0.0, 1.0].
void asm_lsp_bridge_set_weights(float syntaxWeight, float semanticWeight)
{
    auto clamp01 = [](float v) -> float {
        return (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
    };

    lsp_ensure_cs();
    EnterCriticalSection(&g_lsp_cs);

    g_lsp_syntax_weight   = clamp01(syntaxWeight);
    g_lsp_semantic_weight = clamp01(semanticWeight);

    LeaveCriticalSection(&g_lsp_cs);
}

// 7. asm_lsp_bridge_shutdown
// Tears down the bridge: resets runtime state, releases the critical section.
void asm_lsp_bridge_shutdown(void)
{
    if (!g_lsp_cs_init) return;

    EnterCriticalSection(&g_lsp_cs);

    memset(g_lsp_symbols, 0, sizeof(g_lsp_symbols));
    g_lsp_symbol_count    = 0;
    g_lsp_query_count     = 0;
    g_lsp_syntax_weight   = 0.5f;
    g_lsp_semantic_weight = 0.5f;
    g_lsp_initialized     = false;

    LeaveCriticalSection(&g_lsp_cs);
    DeleteCriticalSection(&g_lsp_cs);
    g_lsp_cs_init = false;
}

} // extern "C"
