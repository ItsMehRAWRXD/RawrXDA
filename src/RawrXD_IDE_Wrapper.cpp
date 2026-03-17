/**
 * RawrXD_IDE_Wrapper.cpp — Thread-safe C++ wrapper over MASM IPC core.
 *
 * Zero CRT dependency: HeapAlloc/HeapFree, Win32 synchronization.
 * All functions return RAWRXD_S_OK / RAWRXD_E_* error codes.
 */

#include "RawrXD_IDE_Wrapper.h"

// ---------------------------------------------------------------------------
// External MASM exports (IPC Bridge + GPU emission entry points)
// ---------------------------------------------------------------------------
extern "C" {
    HRESULT __cdecl IPC_Initialize(HANDLE* phCtx, DWORD role);
    HRESULT __cdecl IPC_PostMessage(HANDLE hCtx, DWORD type,
                                    const void* data, DWORD len);
    HRESULT __cdecl IPC_ReadMessage(HANDLE hCtx, void* buf, DWORD bufLen);
    void    __cdecl IPC_Shutdown(HANDLE hCtx);
}

// ---------------------------------------------------------------------------
// Internal constants
// ---------------------------------------------------------------------------
static const DWORD IPC_ROLE_GUI    = 0;
static const DWORD POLL_INTERVAL   = 50;    // ms
static const DWORD AGENT_STACK_SZ  = 65536; // 64 KB thread stack

// ---------------------------------------------------------------------------
// RawrXDCompletion::Release
// ---------------------------------------------------------------------------
void RawrXDCompletion::Release() {
    HeapFree(GetProcessHeap(), 0, this);
}

// ---------------------------------------------------------------------------
// RawrXDBuildResult::Release
// ---------------------------------------------------------------------------
void RawrXDBuildResult::Release() {
    if (outputLog)
        HeapFree(GetProcessHeap(), 0, outputLog);
    HeapFree(GetProcessHeap(), 0, this);
}

// ---------------------------------------------------------------------------
// RawrXDIDE helpers
// ---------------------------------------------------------------------------
void* RawrXDIDE::Alloc(SIZE_T bytes) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes);
}

void RawrXDIDE::Free(void* p) {
    if (p) HeapFree(GetProcessHeap(), 0, p);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
RawrXDIDE::RawrXDIDE()
    : m_hIPC(nullptr), m_hAgentEvent(nullptr),
      m_hAgentThread(nullptr), m_hResponseEvent(nullptr),
      m_initialized(FALSE), m_modelLoaded(FALSE),
      m_streaming(FALSE), m_lastErr(0),
      m_pendingResponse(nullptr), m_pendingLen(0),
      m_streamCb(nullptr), m_streamCtx(nullptr) {
    InitializeSRWLock(&m_lock);
    m_modelPath[0] = '\0';
}

RawrXDIDE::~RawrXDIDE() {
    Shutdown();
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::Initialize(const char* modelPath) {
    if (m_initialized)
        return RAWRXD_S_OK;

    // IPC connection
    HRESULT hr = IPC_Initialize(&m_hIPC, IPC_ROLE_GUI);
    if (hr != RAWRXD_S_OK) {
        m_lastErr = (DWORD)hr;
        return hr;
    }

    // Events
    m_hAgentEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!m_hAgentEvent) { IPC_Shutdown(m_hIPC); m_hIPC = nullptr; return RAWRXD_E_FAIL; }

    m_hResponseEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!m_hResponseEvent) {
        CloseHandle(m_hAgentEvent); m_hAgentEvent = nullptr;
        IPC_Shutdown(m_hIPC); m_hIPC = nullptr;
        return RAWRXD_E_FAIL;
    }

    // Background polling thread
    m_hAgentThread = CreateThread(
        nullptr, AGENT_STACK_SZ,
        AgentPollThread, this,
        0, nullptr
    );
    if (!m_hAgentThread) {
        CloseHandle(m_hResponseEvent);
        CloseHandle(m_hAgentEvent);
        IPC_Shutdown(m_hIPC); m_hIPC = nullptr;
        return RAWRXD_E_FAIL;
    }

    m_initialized = TRUE;

    if (modelPath && modelPath[0]) {
        SetModelPath(modelPath);
    }

    return RAWRXD_S_OK;
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void RawrXDIDE::Shutdown() {
    if (!m_initialized) return;
    m_initialized = FALSE;

    // Signal polling thread to exit, then wake it
    if (m_hAgentThread) {
        if (m_hAgentEvent) SetEvent(m_hAgentEvent);
        WaitForSingleObject(m_hAgentThread, 3000);
        CloseHandle(m_hAgentThread);
        m_hAgentThread = nullptr;
    }
    if (m_hAgentEvent)   { CloseHandle(m_hAgentEvent);   m_hAgentEvent   = nullptr; }
    if (m_hResponseEvent){ CloseHandle(m_hResponseEvent); m_hResponseEvent = nullptr; }
    if (m_hIPC)          { IPC_Shutdown(m_hIPC);         m_hIPC          = nullptr; }
    if (m_pendingResponse){ Free(m_pendingResponse);      m_pendingResponse = nullptr; }
    m_modelLoaded = FALSE;
    m_streaming   = FALSE;
}

// ---------------------------------------------------------------------------
// SetModelPath
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::SetModelPath(const char* path) {
    if (!path || !path[0]) return RAWRXD_E_BADARG;
    // lstrcpyA capped to MAX_PATH
    lstrcpynA(m_modelPath, path, MAX_PATH - 1);
    m_modelPath[MAX_PATH - 1] = '\0';
    return RAWRXD_S_OK;
}

// ---------------------------------------------------------------------------
// LoadModel
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::LoadModel(DWORD timeoutMs) {
    if (!m_initialized)  return RAWRXD_E_NOTINIT;
    if (!m_modelPath[0]) return RAWRXD_E_BADARG;

    // Post a FILE_OPEN message with the model path as payload
    HRESULT hr = PostIPC(RawrXDMsgType::FileOpen,
                         m_modelPath, lstrlenA(m_modelPath) + 1);
    if (hr != RAWRXD_S_OK) return hr;

    hr = WaitResponse(timeoutMs);
    if (hr == RAWRXD_S_OK) m_modelLoaded = TRUE;
    return hr;
}

// ---------------------------------------------------------------------------
// UnloadModel
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::UnloadModel() {
    if (!m_initialized)  return RAWRXD_E_NOTINIT;
    if (!m_modelLoaded)  return RAWRXD_S_OK;
    // Signal shutdown to inference engine; re-init clears state
    HRESULT hr = PostIPC(RawrXDMsgType::Shutdown, nullptr, 0);
    m_modelLoaded = FALSE;
    return hr;
}

// ---------------------------------------------------------------------------
// GetCompletion — synchronous
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::GetCompletion(
        const char*        context,
        DWORD              ctxLen,
        DWORD              maxTokens,
        RawrXDCompletion** ppResult,
        DWORD              timeoutMs) {

    if (!m_initialized)  return RAWRXD_E_NOTINIT;
    if (!ppResult)       return RAWRXD_E_BADARG;
    if (!context)        return RAWRXD_E_BADARG;
    if (ctxLen == 0)     ctxLen = (DWORD)lstrlenA(context);

    // Build request payload: [maxTokens:4][context]
    DWORD payloadLen = 4 + ctxLen;
    char* payload = (char*)Alloc(payloadLen);
    if (!payload) return RAWRXD_E_OUTOFMEMORY;
    *(DWORD*)payload = maxTokens;
    CopyMemory(payload + 4, context, ctxLen);

    HRESULT hr = PostIPC(RawrXDMsgType::CompletionRequest, payload, payloadLen);
    Free(payload);
    if (hr != RAWRXD_S_OK) return hr;

    DWORD t0 = GetTickCount();
    hr = WaitResponse(timeoutMs);
    if (hr != RAWRXD_S_OK) return hr;
    DWORD latency = GetTickCount() - t0;

    // Build result object
    AcquireSRWLockExclusive(&m_lock);
    DWORD   respLen = m_pendingLen;
    char*   respBuf = m_pendingResponse;
    m_pendingResponse = nullptr;
    m_pendingLen      = 0;
    ReleaseSRWLockExclusive(&m_lock);

    if (!respBuf) return RAWRXD_E_FAIL;

    // Allocate result struct + text in one block
    SIZE_T allocSz = sizeof(RawrXDCompletion) + respLen + 1;
    RawrXDCompletion* res = (RawrXDCompletion*)Alloc(allocSz);
    if (!res) { Free(respBuf); return RAWRXD_E_OUTOFMEMORY; }
    res->text       = (char*)res + sizeof(RawrXDCompletion);
    res->length     = respLen;
    res->score      = 1.0f;     // model provides probability via IPC message
    res->latencyMs  = latency;
    CopyMemory(res->text, respBuf, respLen);
    res->text[respLen] = '\0';
    Free(respBuf);

    *ppResult = res;
    return RAWRXD_S_OK;
}

// ---------------------------------------------------------------------------
// StartStreamingCompletion
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::StartStreamingCompletion(
        const char*         context,
        DWORD               ctxLen,
        DWORD               maxTokens,
        RawrXDTokenCallback callback,
        void*               userCtx) {

    if (!m_initialized) return RAWRXD_E_NOTINIT;
    if (!callback)      return RAWRXD_E_BADARG;
    if (m_streaming)    return RAWRXD_E_BUSY;
    if (ctxLen == 0 && context) ctxLen = (DWORD)lstrlenA(context);

    AcquireSRWLockExclusive(&m_lock);
    m_streamCb  = callback;
    m_streamCtx = userCtx;
    m_streaming = TRUE;
    ReleaseSRWLockExclusive(&m_lock);

    DWORD payloadLen = 4 + ctxLen;
    char* payload = (char*)Alloc(payloadLen);
    if (!payload) {
        m_streaming = FALSE;
        return RAWRXD_E_OUTOFMEMORY;
    }
    *(DWORD*)payload = maxTokens;
    if (ctxLen) CopyMemory(payload + 4, context, ctxLen);
    HRESULT hr = PostIPC(RawrXDMsgType::InferenceRequest, payload, payloadLen);
    Free(payload);
    if (hr != RAWRXD_S_OK) m_streaming = FALSE;
    return hr;
}

// ---------------------------------------------------------------------------
// CancelStreaming
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::CancelStreaming() {
    AcquireSRWLockExclusive(&m_lock);
    m_streaming = FALSE;
    m_streamCb  = nullptr;
    m_streamCtx = nullptr;
    ReleaseSRWLockExclusive(&m_lock);
    return PostIPC(RawrXDMsgType::Shutdown, nullptr, 0);
}

// ---------------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::Build(const char* projectDir, RawrXDBuildResult** ppResult) {
    if (!m_initialized) return RAWRXD_E_NOTINIT;
    if (!ppResult)      return RAWRXD_E_BADARG;

    DWORD dirLen = projectDir ? (DWORD)lstrlenA(projectDir) : 0;
    HRESULT hr = PostIPC(RawrXDMsgType::BuildRequest,
                         projectDir, dirLen + 1);
    if (hr != RAWRXD_S_OK) return hr;

    hr = WaitResponse(60000);
    if (hr != RAWRXD_S_OK) return hr;

    AcquireSRWLockExclusive(&m_lock);
    DWORD  logLen  = m_pendingLen;
    char*  logBuf  = m_pendingResponse;
    m_pendingResponse = nullptr;
    m_pendingLen      = 0;
    ReleaseSRWLockExclusive(&m_lock);

    RawrXDBuildResult* res = (RawrXDBuildResult*)Alloc(sizeof(RawrXDBuildResult));
    if (!res) { Free(logBuf); return RAWRXD_E_OUTOFMEMORY; }
    res->outputLog  = logBuf;
    res->logLength  = logLen;
    res->succeeded  = (logBuf && logLen > 0 && logBuf[0] == '0') ? FALSE : TRUE;
    res->exitCode   = res->succeeded ? 0 : 1;
    *ppResult = res;
    return RAWRXD_S_OK;
}

// ---------------------------------------------------------------------------
// IsAgentConnected
// ---------------------------------------------------------------------------
BOOL RawrXDIDE::IsAgentConnected() const {
    return m_initialized && m_hIPC != nullptr;
}

DWORD RawrXDIDE::GetLastError() const { return m_lastErr; }

// ---------------------------------------------------------------------------
// DebugLog — wsprintfA-based, OutputDebugStringA
// ---------------------------------------------------------------------------
void RawrXDIDE::DebugLog(const char* fmt, ...) {
    char buf[512];
    // Manual va_list formatting via wvsprintfA
    va_list args;
    va_start(args, fmt);
    wvsprintfA(buf, fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}

// ---------------------------------------------------------------------------
// AgentPollThread — polls IPC ring and dispatches tokens
// ---------------------------------------------------------------------------
DWORD WINAPI RawrXDIDE::AgentPollThread(LPVOID param) {
    RawrXDIDE* self = reinterpret_cast<RawrXDIDE*>(param);
    char buf[4096];

    while (self->m_initialized) {
        WaitForSingleObject(self->m_hAgentEvent, POLL_INTERVAL);
        if (!self->m_initialized) break;

        int read = (int)(DWORD_PTR)IPC_ReadMessage(self->m_hIPC, buf, sizeof(buf));
        if (read <= 0) continue;

        // First 4 bytes = message type
        DWORD msgType = *(DWORD*)buf;
        const char* payload = buf + 4;
        DWORD payloadLen = (read >= 4) ? (DWORD)read - 4 : 0;

        if (msgType == (DWORD)RawrXDMsgType::TokenStream) {
            // Streaming token: forward to callback
            AcquireSRWLockShared(&self->m_lock);
            RawrXDTokenCallback cb  = self->m_streamCb;
            void*               ctx = self->m_streamCtx;
            BOOL streaming          = self->m_streaming;
            ReleaseSRWLockShared(&self->m_lock);
            if (streaming && cb && payloadLen > 0) {
                cb(payload, payloadLen, 1.0f, ctx);
            }
        } else if (msgType == (DWORD)RawrXDMsgType::CompletionResponse ||
                   msgType == (DWORD)RawrXDMsgType::InferenceResponse) {
            // Synchronous response: store in pending buffer, signal caller
            AcquireSRWLockExclusive(&self->m_lock);
            if (self->m_pendingResponse) {
                HeapFree(GetProcessHeap(), 0, self->m_pendingResponse);
            }
            self->m_pendingResponse = (char*)HeapAlloc(GetProcessHeap(),
                                                        HEAP_ZERO_MEMORY,
                                                        payloadLen + 1);
            if (self->m_pendingResponse && payloadLen > 0) {
                CopyMemory(self->m_pendingResponse, payload, payloadLen);
                self->m_pendingLen = payloadLen;
            }
            ReleaseSRWLockExclusive(&self->m_lock);
            SetEvent(self->m_hResponseEvent);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// PostIPC
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::PostIPC(RawrXDMsgType type, const void* data, DWORD dataLen) {
    if (!m_hIPC) return RAWRXD_E_NOTINIT;
    return IPC_PostMessage(m_hIPC, (DWORD)type, data, dataLen);
}

// ---------------------------------------------------------------------------
// WaitResponse
// ---------------------------------------------------------------------------
HRESULT RawrXDIDE::WaitResponse(DWORD timeoutMs) {
    DWORD res = WaitForSingleObject(m_hResponseEvent, timeoutMs);
    if (res == WAIT_OBJECT_0) return RAWRXD_S_OK;
    if (res == WAIT_TIMEOUT)  return RAWRXD_E_TIMEOUT;
    return RAWRXD_E_FAIL;
}

// ---------------------------------------------------------------------------
// C factory functions
// ---------------------------------------------------------------------------
extern "C"
HRESULT __cdecl RawrXD_CreateIDE(const char* modelPath, RawrXDIDE** ppIDE) {
    if (!ppIDE) return RAWRXD_E_BADARG;
    void* mem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(RawrXDIDE));
    if (!mem) return RAWRXD_E_OUTOFMEMORY;
    RawrXDIDE* ide = new(mem) RawrXDIDE();
    HRESULT hr = ide->Initialize(modelPath);
    if (hr != RAWRXD_S_OK) {
        ide->~RawrXDIDE();
        HeapFree(GetProcessHeap(), 0, mem);
        *ppIDE = nullptr;
        return hr;
    }
    *ppIDE = ide;
    return RAWRXD_S_OK;
}

extern "C"
void __cdecl RawrXD_DestroyIDE(RawrXDIDE* pIDE) {
    if (!pIDE) return;
    pIDE->Shutdown();
    pIDE->~RawrXDIDE();
    HeapFree(GetProcessHeap(), 0, pIDE);
}
