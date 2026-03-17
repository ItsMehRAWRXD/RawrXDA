/**
 * RawrXD_IDE_Wrapper.h — C++ IDE integration wrapper
 *
 * Thread-safe C++ interface over the MASM IPC / Agent core.
 * No CRT dependency: all memory via HeapAlloc, all I/O via Win32.
 * Designed for consumption by VS Code extension host or remote IDE clients.
 *
 * Compile: cl /nologo /W4 /WX /EHsc /MD /I. /c RawrXD_IDE_Wrapper.cpp
 */

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ---------------------------------------------------------------------------
//  Error codes (COM-style HRESULT subset)
// ---------------------------------------------------------------------------
#define RAWRXD_S_OK             ((HRESULT)0x00000000L)
#define RAWRXD_E_FAIL           ((HRESULT)0x80004005L)
#define RAWRXD_E_OUTOFMEMORY    ((HRESULT)0x8007000EL)
#define RAWRXD_E_NOTINIT        ((HRESULT)0x80040201L)
#define RAWRXD_E_TIMEOUT        ((HRESULT)0x80040202L)
#define RAWRXD_E_BUSY           ((HRESULT)0x80040203L)
#define RAWRXD_E_BADARG         ((HRESULT)0x80040204L)

// ---------------------------------------------------------------------------
//  IPC message types (must match RawrXD_IPC_Bridge.asm)
// ---------------------------------------------------------------------------
enum class RawrXDMsgType : DWORD {
    InferenceRequest   = 1,
    InferenceResponse  = 2,
    TokenStream        = 3,
    FileOpen           = 4,
    FileEdit           = 5,
    CursorMove         = 6,
    CompletionRequest  = 7,
    CompletionResponse = 8,
    BuildRequest       = 0x1040,
    Shutdown           = 255
};

// ---------------------------------------------------------------------------
//  Completion result (heap-allocated, caller must call Release())
// ---------------------------------------------------------------------------
struct RawrXDCompletion {
    char*   text;       // NUL-terminated completion text
    DWORD   length;     // byte length (excluding NUL)
    float   score;      // model confidence [0,1]
    DWORD   latencyMs;  // end-to-end latency in milliseconds

    void Release();     // implemented in .cpp: HeapFree(GetProcessHeap(),0,this)
};

// ---------------------------------------------------------------------------
//  Token stream callback — called from agent thread for each new token
// ---------------------------------------------------------------------------
typedef void (CALLBACK* RawrXDTokenCallback)(
    const char* token,      // NUL-terminated token text
    DWORD       tokenLen,   // byte length
    float       score,      // token probability
    void*       userCtx     // caller-supplied context pointer
);

// ---------------------------------------------------------------------------
//  Build result
// ---------------------------------------------------------------------------
struct RawrXDBuildResult {
    BOOL    succeeded;
    DWORD   exitCode;
    char*   outputLog;      // heap-allocated; caller calls Release()
    DWORD   logLength;

    void Release();
};

// ---------------------------------------------------------------------------
//  RawrXDIDE — Main IDE integration class
// ---------------------------------------------------------------------------
class RawrXDIDE {
public:
    RawrXDIDE();
    ~RawrXDIDE();

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------
    /**
     * Initialize the IDE backend: connect to IPC shared memory, start the
     * agent polling thread, load the specified model.
     *
     * @param modelPath  Absolute path to the .gguf or .safetensors model file.
     *                   May be NULL to defer model loading.
     * @returns RAWRXD_S_OK on success.
     */
    HRESULT Initialize(const char* modelPath = nullptr);

    /**
     * Shut down cleanly: stop agent thread, release IPC handles, free GDI.
     */
    void Shutdown();

    // -----------------------------------------------------------------------
    // Model management
    // -----------------------------------------------------------------------
    /**
     * Set the active model path. Does not load; call LoadModel() after.
     */
    HRESULT SetModelPath(const char* path);

    /**
     * Load the model previously set via SetModelPath (or Initialize).
     * Blocks until the model is resident in memory or an error occurs.
     * @param timeoutMs  Maximum milliseconds to wait; 0 = wait forever.
     */
    HRESULT LoadModel(DWORD timeoutMs = 60000);

    /**
     * Unload the current model and free GPU/CPU memory.
     */
    HRESULT UnloadModel();

    // -----------------------------------------------------------------------
    // Code completion
    // -----------------------------------------------------------------------
    /**
     * Request an inline code completion synchronously.
     *
     * @param context    Editor context string (file content up to cursor).
     * @param ctxLen     Byte length of context (0 = use lstrlenA).
     * @param maxTokens  Maximum tokens to generate.
     * @param ppResult   On success, receives a heap-allocated RawrXDCompletion.
     *                   Caller must call ppResult->Release().
     * @param timeoutMs  Maximum wait in milliseconds.
     */
    HRESULT GetCompletion(
        const char*          context,
        DWORD                ctxLen,
        DWORD                maxTokens,
        RawrXDCompletion**   ppResult,
        DWORD                timeoutMs = 10000
    );

    /**
     * Request a streaming completion. Tokens are delivered asynchronously
     * via the supplied callback.
     *
     * @param context    Editor context.
     * @param ctxLen     Context byte length.
     * @param maxTokens  Generation limit.
     * @param callback   Function invoked for each token (may be called from
     *                   a worker thread).
     * @param userCtx    Opaque pointer forwarded to callback.
     */
    HRESULT StartStreamingCompletion(
        const char*          context,
        DWORD                ctxLen,
        DWORD                maxTokens,
        RawrXDTokenCallback  callback,
        void*                userCtx
    );

    /** Cancel an in-progress streaming completion. */
    HRESULT CancelStreaming();

    // -----------------------------------------------------------------------
    // Build integration
    // -----------------------------------------------------------------------
    /**
     * Build the project synchronously. Invokes ml64.exe/MSVC via IPC.
     * @param projectDir Absolute path to project root.
     * @param ppResult   Receives build result (must Release()).
     */
    HRESULT Build(const char* projectDir, RawrXDBuildResult** ppResult);

    // -----------------------------------------------------------------------
    // Diagnostics
    // -----------------------------------------------------------------------
    /** Returns TRUE if the agent backend is connected and responsive. */
    BOOL IsAgentConnected() const;

    /** Returns the last Win32 error code recorded by the wrapper. */
    DWORD GetLastError() const;

    /** Write a diagnostic string to OutputDebugStringA. */
    void DebugLog(const char* fmt, ...);

private:
    // Internal state
    HANDLE  m_hIPC;             // IPC context (from IPC_Initialize)
    HANDLE  m_hAgentEvent;      // Manual-reset event for IPC wakeup
    HANDLE  m_hAgentThread;     // Background polling thread handle
    HANDLE  m_hResponseEvent;   // Auto-reset event: response ready
    SRWLOCK m_lock;             // Protects m_pendingResponse
    BOOL    m_initialized;
    BOOL    m_modelLoaded;
    BOOL    m_streaming;
    DWORD   m_lastErr;

    char    m_modelPath[MAX_PATH];
    char*   m_pendingResponse;  // heap buffer for sync completion response
    DWORD   m_pendingLen;

    RawrXDTokenCallback m_streamCb;
    void*               m_streamCtx;

    // Internal helpers
    static DWORD WINAPI AgentPollThread(LPVOID param);
    HRESULT PostIPC(RawrXDMsgType type, const void* data, DWORD dataLen);
    HRESULT WaitResponse(DWORD timeoutMs);
    void    DispatchToken(const char* tok, DWORD len, float score);
    void*   Alloc(SIZE_T bytes);
    void    Free(void* p);
};

// ---------------------------------------------------------------------------
//  Factory — create and destroy IDE instances without CRT new/delete
// ---------------------------------------------------------------------------
extern "C" {
    /** Allocate and Initialize a RawrXDIDE instance. */
    HRESULT __cdecl RawrXD_CreateIDE(const char* modelPath, RawrXDIDE** ppIDE);

    /** Shutdown and free a RawrXDIDE created by RawrXD_CreateIDE. */
    void    __cdecl RawrXD_DestroyIDE(RawrXDIDE* pIDE);
}
