// ============================================================================
// bridge_titan_4a.cpp — Phase 4A C++ Bridge for RawrXD Titan UI kernel
// ============================================================================
// Provides the 4 extern "C" functions that ui.asm calls:
//   Bridge_SubmitCompletion, Bridge_GetSuggestionText,
//   Bridge_ClearSuggestion, Bridge_RequestSuggestion
//
// Loads RawrXD_Titan.dll at runtime via LoadLibrary for live 70B inference.
// Falls back to a simple demo ghost text if the DLL is not found.
// ============================================================================

#include <windows.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_GHOST_LEN     2048
#define MAX_SUGGEST_BUF   256

// ============================================================================
// ASM-side exports we can call (defined in ui.asm, declared PUBLIC)
// ============================================================================

extern "C" {
    // Bridge_OnSuggestionReady(const wchar_t* text, int len)
    // Parses multi-line ghost text, sets g_ghostActive, triggers repaint
    void Bridge_OnSuggestionReady(const wchar_t* text, int len);

    // Bridge_OnSuggestionComplete()  
    // Sets g_ghostActive=1, clears g_inferencePending, triggers repaint
    void Bridge_OnSuggestionComplete(void);

    // Phase 4B: Neural Pruning Scorer (ASM kernel)
    // ScoreCandidate(candidate_ptr, len, context_ptr, context_len) → score 0-255
    int  ScoreCandidate(const wchar_t* candidate, int len,
                        const wchar_t* context, int contextLen);

    // PruneCandidates(context_ptr, context_len) → best index or -1
    int  PruneCandidates(const wchar_t* context, int contextLen);

    // Shared data from ui.asm — candidate buffers
    extern wchar_t g_candidates[];
    extern int     g_candidateLens[];
    extern int     g_candidateScores[];
    extern int     g_candidateCount;
    extern int     g_bestCandidate;
    extern int     g_rlhfAcceptCount;
    extern int     g_rlhfRejectCount;

    // Editor context
    extern wchar_t g_textBuf[];
    extern int     g_totalChars;
    extern int     g_cursorLine;
    extern int     g_cursorCol;
    extern int     g_lineOff[];
}

// ============================================================================
// Titan DLL Interface — Dynamic loading via LoadLibrary
// ============================================================================

typedef struct {
    const char*  prompt;
    int          max_tokens;
    float        temperature;
    void       (*callback)(const char* text, int len);
} TITAN_INFERENCE_PARAMS;

typedef int  (*PFN_TITAN_INITIALIZE)(const char* model_path);
typedef int  (*PFN_TITAN_INFER_ASYNC)(TITAN_INFERENCE_PARAMS* params);
typedef void (*PFN_TITAN_SHUTDOWN)(void);

static HMODULE             g_hTitanDll       = NULL;
static PFN_TITAN_INITIALIZE   g_pfnTitanInit    = NULL;
static PFN_TITAN_INFER_ASYNC  g_pfnTitanInfer   = NULL;
static PFN_TITAN_SHUTDOWN     g_pfnTitanShutdown = NULL;
static bool                g_titanLoaded     = false;
static bool                g_titanInitDone   = false;

// ============================================================================
// Internal state
// ============================================================================

static wchar_t g_suggestionBuf[MAX_SUGGEST_BUF];
static int     g_suggestionLen  = 0;
static bool    g_hasResponse    = false;

// ============================================================================
// Titan DLL loader — called once on first inference request
// ============================================================================

static void TryLoadTitanDll(void)
{
    if (g_titanLoaded) return;
    g_titanLoaded = true;  // only try once

    g_hTitanDll = LoadLibraryA("RawrXD_Titan.dll");
    if (!g_hTitanDll) {
        // Try v2 name (rebuild without zombie lock)
        g_hTitanDll = LoadLibraryA("RawrXD_Titan_v2.dll");
    }
    if (!g_hTitanDll) {
        // Try adjacent directory
        g_hTitanDll = LoadLibraryA("bin\\RawrXD_Titan.dll");
    }
    if (!g_hTitanDll) {
        g_hTitanDll = LoadLibraryA("bin\\RawrXD_Titan_v2.dll");
    }
    if (!g_hTitanDll) return;

    g_pfnTitanInit  = (PFN_TITAN_INITIALIZE)GetProcAddress(g_hTitanDll, "Titan_Initialize");
    g_pfnTitanInfer = (PFN_TITAN_INFER_ASYNC)GetProcAddress(g_hTitanDll, "Titan_InferAsync");
    g_pfnTitanShutdown = (PFN_TITAN_SHUTDOWN)GetProcAddress(g_hTitanDll, "Titan_Shutdown");

    // Auto-initialize with default model path
    if (g_pfnTitanInit && !g_titanInitDone) {
        int ok = g_pfnTitanInit("models\\70b_simulation.gguf");
        g_titanInitDone = (ok == 0);
    }
}

// ============================================================================
// Titan callback — invoked from DLL inference thread
// Converts ANSI result to wide char and feeds into ASM ghost text pipeline
// ============================================================================

static void TitanInferenceCallback(const char* text, int len)
{
    if (!text || len <= 0) return;

    // Convert ANSI → wide
    static wchar_t wbuf[MAX_GHOST_LEN];
    int wlen = 0;
    for (int i = 0; i < len && i < MAX_GHOST_LEN - 1; i++) {
        wbuf[wlen++] = (wchar_t)(unsigned char)text[i];
    }
    wbuf[wlen] = 0;

    // ================================================================
    // Phase 4B: Route through Neural Pruning Scorer
    // Store as candidate[0], score it, only deliver if score >= threshold
    // ================================================================
    g_candidateCount = 1;
    int copyLen = (wlen < 256) ? wlen : 255;
    for (int i = 0; i < copyLen; i++) {
        g_candidates[i] = wbuf[i];
    }
    g_candidates[copyLen] = 0;
    g_candidateLens[0] = copyLen;

    // Gather context: text before cursor position
    int cursorOffset = 0;
    if (g_cursorLine >= 0 && g_cursorLine < 4000) {
        cursorOffset = g_lineOff[g_cursorLine] + g_cursorCol;
    }
    int contextLen = (cursorOffset > 512) ? 512 : cursorOffset;
    const wchar_t* contextPtr = &g_textBuf[cursorOffset - contextLen];

    // Score via ASM kernel
    int bestIdx = PruneCandidates(contextPtr, contextLen);

    if (bestIdx < 0) {
        // Candidate pruned — below quality threshold
        return;
    }

    // Cache for Bridge_GetSuggestionText
    int sugCopy = (copyLen < MAX_SUGGEST_BUF - 1) ? copyLen : MAX_SUGGEST_BUF - 1;
    for (int i = 0; i < sugCopy; i++) g_suggestionBuf[i] = wbuf[i];
    g_suggestionBuf[sugCopy] = 0;
    g_suggestionLen = sugCopy;
    g_hasResponse = true;

    // Deliver to ASM ghost text pipeline
    Bridge_OnSuggestionReady(wbuf, wlen);
}

// ============================================================================
// Fallback ghost text — used when Titan DLL is not available
// Context-aware CPU path: scans recent text for patterns and generates
// plausible continuations (bracket closure, keyword completion, etc.)
// ============================================================================

// Simple keyword table for ASM/C/C++ completion
struct KwEntry { const wchar_t* prefix; const wchar_t* completion; };
static const KwEntry g_kwTable[] = {
    { L"if ",     L"(condition) {\n    \n}" },
    { L"if(",     L"condition) {\n    \n}" },
    { L"for ",    L"(int i = 0; i < n; i++) {\n    \n}" },
    { L"for(",    L"int i = 0; i < n; i++) {\n    \n}" },
    { L"while ",  L"(condition) {\n    \n}" },
    { L"while(",  L"condition) {\n    \n}" },
    { L"switch ", L"(expr) {\n    case 0:\n        break;\n    default:\n        break;\n}" },
    { L"#inc",    L"lude <>" },
    { L"#def",    L"ine " },
    { L"ret",     L"urn " },
    { L"NULL",    L";" },
    { L"proc",    L" PROC FRAME" },
    { L"PROC",    L" FRAME" },
    { L"mov ",    L"rax, rcx" },
    { L"lea ",    L"rcx, [rip + offset]" },
    { L"call",    L" FunctionName" },
    { L"push",    L" rbp" },
    { L"sub ",    L"rsp, 28h" },
    { L"xor ",    L"eax, eax" },
    { L"cmp ",    L"eax, 0" },
    { L"jmp ",    L"@label" },
    { L"fn ",     L"name() -> Result {\n    \n}" },
    { L"func",    L"tion name() {\n    \n}" },
    { L"def ",    L"function_name():\n    pass" },
    { L"class ",  L"Name {\npublic:\n    Name() = default;\n};" },
    { L"struct ", L"Name {\n    \n};" },
    { L"extern ", L"\"C\" {\n    \n}" },
    { L"//",      L" TODO: " },
    { L";",       L" TODO: " },
};
static const int g_kwCount = sizeof(g_kwTable) / sizeof(g_kwTable[0]);

// Check if wide string `text` (length `tlen`) ends with `suffix`
static bool EndsWith(const wchar_t* text, int tlen, const wchar_t* suffix) {
    int slen = 0;
    while (suffix[slen]) slen++;
    if (slen > tlen) return false;
    for (int i = 0; i < slen; i++) {
        if (text[tlen - slen + i] != suffix[i]) return false;
    }
    return true;
}

// Count un-closed brackets in last N chars
static int UnclosedBrackets(const wchar_t* text, int len) {
    int parens = 0, braces = 0, squares = 0;
    int start = (len > 200) ? len - 200 : 0;
    for (int i = start; i < len; i++) {
        switch (text[i]) {
            case L'(': parens++; break;
            case L')': if (parens > 0) parens--; break;
            case L'{': braces++; break;
            case L'}': if (braces > 0) braces--; break;
            case L'[': squares++; break;
            case L']': if (squares > 0) squares--; break;
        }
    }
    return parens + braces + squares;
}

static void FallbackSuggestion(const wchar_t* textBuf, int row, int col)
{
    // Get text up to cursor
    int cursorOffset = 0;
    if (row >= 0 && row < 4000) {
        cursorOffset = g_lineOff[row] + col;
    }
    if (cursorOffset <= 0) return;  // nothing to complete from

    // Extract the current line for keyword matching
    int lineStart = g_lineOff[row];
    int lineLen = cursorOffset - lineStart;

    // Try keyword completion first — match last N chars of current line
    const wchar_t* lineText = &g_textBuf[lineStart];
    const wchar_t* suggestion = nullptr;
    int sugLen = 0;

    // Skip leading whitespace for matching
    int trimStart = 0;
    while (trimStart < lineLen && lineText[trimStart] == L' ') trimStart++;
    const wchar_t* trimmedLine = lineText + trimStart;
    int trimmedLen = lineLen - trimStart;

    for (int k = 0; k < g_kwCount; k++) {
        if (EndsWith(trimmedLine, trimmedLen, g_kwTable[k].prefix)) {
            suggestion = g_kwTable[k].completion;
            break;
        }
    }

    // If no keyword match, try bracket closure
    if (!suggestion) {
        int unclosed = UnclosedBrackets(g_textBuf, cursorOffset);
        if (unclosed > 0) {
            // Suggest closing the most recent bracket
            // Scan backwards for the most recent opener
            for (int i = cursorOffset - 1; i >= 0; i--) {
                if (g_textBuf[i] == L'(' && unclosed > 0) {
                    static const wchar_t closeParen[] = L")";
                    suggestion = closeParen;
                    break;
                }
                if (g_textBuf[i] == L'{' && unclosed > 0) {
                    static const wchar_t closeBrace[] = L"\n}";
                    suggestion = closeBrace;
                    break;
                }
                if (g_textBuf[i] == L'[' && unclosed > 0) {
                    static const wchar_t closeSquare[] = L"]";
                    suggestion = closeSquare;
                    break;
                }
            }
        }
    }

    // Last resort: static placeholder
    if (!suggestion) {
        static const wchar_t placeholder[] = L"// [RawrXD: connect Ollama for live completion]";
        suggestion = placeholder;
    }

    sugLen = 0;
    while (suggestion[sugLen]) sugLen++;

    // Route through Neural Pruning Scorer (Phase 4B)
    g_candidateCount = 1;
    int copyLen = (sugLen < 256) ? sugLen : 255;
    for (int i = 0; i < copyLen; i++) g_candidates[i] = suggestion[i];
    g_candidates[copyLen] = 0;
    g_candidateLens[0] = copyLen;

    int contextLen = (cursorOffset > 512) ? 512 : cursorOffset;
    const wchar_t* contextPtr = &g_textBuf[cursorOffset - contextLen];

    int bestIdx = PruneCandidates(contextPtr, contextLen);
    if (bestIdx < 0) return;  // Pruned

    // Cache it
    for (int i = 0; i < sugLen && i < MAX_SUGGEST_BUF - 1; i++)
        g_suggestionBuf[i] = suggestion[i];
    g_suggestionBuf[sugLen] = 0;
    g_suggestionLen = sugLen;
    g_hasResponse = true;

    Bridge_OnSuggestionReady(suggestion, sugLen);
}

// ============================================================================
// Bridge functions — extern "C" exports matched by ui.asm EXTERNs
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Bridge_RequestSuggestion(const wchar_t* text, int row, int col)
// Called from WM_USER_SUGGESTION_REQ handler in ui.asm
// ASM passes: RCX=lea g_textBuf, EDX=g_cursorLine, R8D=g_cursorCol
// Returns: 0 on success/submitted, negative on error
//          ASM checks EAX and clears g_inferencePending on failure
// ----------------------------------------------------------------------------
int Bridge_RequestSuggestion(const wchar_t* text, int row, int col)
{
    if (!text) return -1;

    // Try loading the Titan DLL on first call
    TryLoadTitanDll();

    if (g_pfnTitanInfer && g_titanInitDone) {
        // Convert wide buffer context to ANSI for the model
        // CRITICAL: static buffer — survives scope for inference thread
        static char context[4096];
        int len = 0;
        while (text[len] && len < 4095) {
            context[len] = (char)text[len];    // simple narrow (ASCII range)
            len++;
        }
        context[len] = 0;

        // Submit async inference
        static TITAN_INFERENCE_PARAMS params;
        params.prompt     = context;
        params.max_tokens = 256;
        params.temperature = 0.4f;
        params.callback   = TitanInferenceCallback;
        int rc = g_pfnTitanInfer(&params);
        if (rc < 0) {
            // DLL returned error (e.g. -2=busy, -3=thread fail)
            // Return negative so ASM clears g_inferencePending
            return rc;
        }
        return 0;
    } else {
        // No Titan DLL — use fallback ghost text for testing
        FallbackSuggestion(text, row, col);
        return 0;  // fallback is synchronous, always succeeds
    }
}

// ----------------------------------------------------------------------------
// Bridge_SubmitCompletion(int command)
// Called when user accepts (1) or rejects (0) ghost text
// ASM: mov ecx, 1 / xor ecx, ecx ; call Bridge_SubmitCompletion
// ----------------------------------------------------------------------------
void Bridge_SubmitCompletion(int command)
{
    if (command == 1) {
        // Phase 4B: RLHF accept tracking
        InterlockedIncrement((volatile LONG*)&g_rlhfAcceptCount);
    } else {
        // Phase 4B: RLHF reject tracking
        InterlockedIncrement((volatile LONG*)&g_rlhfRejectCount);
    }
    // Clear cached suggestion
    g_hasResponse   = false;
    g_suggestionLen = 0;
}

// ----------------------------------------------------------------------------
// Bridge_GetSuggestionText(wchar_t* out, int size)
// Returns the current suggestion text. Called by UI paint path (future).
// Returns: number of wide chars written
// ----------------------------------------------------------------------------
int Bridge_GetSuggestionText(wchar_t* out, int size)
{
    if (!out || size <= 0 || !g_hasResponse) return 0;
    int copyLen = (g_suggestionLen < size - 1) ? g_suggestionLen : size - 1;
    for (int i = 0; i < copyLen; i++) out[i] = g_suggestionBuf[i];
    out[copyLen] = 0;
    return copyLen;
}

// ----------------------------------------------------------------------------
// Bridge_ClearSuggestion()
// Called when user presses Escape to dismiss ghost text
// ----------------------------------------------------------------------------
void Bridge_ClearSuggestion(void)
{
    g_hasResponse   = false;
    g_suggestionLen = 0;
    g_suggestionBuf[0] = 0;
}

// ----------------------------------------------------------------------------
// Bridge_AbortInference()
// Called when user types a new keystroke while inference is pending.
// Signals the DLL to cancel any in-flight request so the
// g_currentRequest.active flag gets released promptly.
// ----------------------------------------------------------------------------
void Bridge_AbortInference(void)
{
    // If DLL is loaded, we need a way to signal cancellation.
    // The DLL doesn't export a cancel function yet, but we can at least
    // clear the pending flag so the next suggestion request goes through.
    // When DLL adds Titan_Cancel(), wire it here.
    // For now: clear bridge-side state to allow retry
    g_hasResponse   = false;
    g_suggestionLen = 0;
}

// Inference router abort stub — referenced by ui.asm but not yet wired
int g_routerAbort = 0;

void InferenceRouter_Abort(void)
{
    g_routerAbort = 1;
}

}  // extern "C"

// ============================================================================
// End of bridge_titan_4a.cpp
// ============================================================================
