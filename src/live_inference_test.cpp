// ============================================================================
// live_inference_test.cpp — Direct Ollama Live Inference Test
// ============================================================================
// Self-contained: Winsock HTTP POST to Ollama, no DLL needed.
// Proves real non-simulated inference end-to-end.
// ============================================================================

#include <stdint.h>
#include <string.h>

extern "C" {
    typedef void*          HANDLE;
    typedef void*          HMODULE;
    typedef unsigned long  DWORD;
    typedef int            BOOL;
    
    HANDLE  __stdcall GetStdHandle(DWORD nStdHandle);
    BOOL    __stdcall WriteFile(HANDLE h, const void* buf, DWORD nBytes,
                                DWORD* written, void* overlapped);
    void    __stdcall ExitProcess(unsigned int code);
    BOOL    __stdcall QueryPerformanceCounter(int64_t* lpCount);
    BOOL    __stdcall QueryPerformanceFrequency(int64_t* lpFreq);
    HMODULE __stdcall LoadLibraryA(const char* name);
    void*   __stdcall GetProcAddress(HMODULE h, const char* proc);
    int     __cdecl   wsprintfA(char* buf, const char* fmt, ...);
}

#define STD_OUTPUT_HANDLE ((DWORD)-11)

// ============================================================================
// Winsock — Dynamic Loading
// ============================================================================

typedef unsigned __int64 SOCKET_T;
#define INVALID_SOCK   ((SOCKET_T)(~(SOCKET_T)0))
#define AF_INET_       2
#define SOCK_STREAM_   1

#pragma pack(push, 1)
struct SockAddrIn {
    short          sin_family;
    unsigned short sin_port;
    unsigned long  sin_addr;
    char           sin_zero[8];
};
#pragma pack(pop)

typedef int      (__stdcall *PFN_WSAStartup)(unsigned short ver, void* data);
typedef SOCKET_T (__stdcall *PFN_socket)(int af, int type, int proto);
typedef int      (__stdcall *PFN_connect)(SOCKET_T s, const SockAddrIn* name, int namelen);
typedef int      (__stdcall *PFN_send_)(SOCKET_T s, const char* buf, int len, int flags);
typedef int      (__stdcall *PFN_recv_)(SOCKET_T s, char* buf, int len, int flags);
typedef int      (__stdcall *PFN_closesocket)(SOCKET_T s);
typedef int      (__stdcall *PFN_setsockopt)(SOCKET_T s, int level, int optname,
                                              const char* optval, int optlen);

static PFN_WSAStartup  pfn_WSAStartup = 0;
static PFN_socket      pfn_socket     = 0;
static PFN_connect     pfn_connect    = 0;
static PFN_send_       pfn_send       = 0;
static PFN_recv_       pfn_recv       = 0;
static PFN_closesocket pfn_closesocket = 0;
static PFN_setsockopt  pfn_setsockopt  = 0;

// ============================================================================
// Console output
// ============================================================================

static HANDLE g_hOut = 0;

static void Out(const char* s) {
    DWORD w = 0;
    int len = 0;
    while (s[len]) len++;
    WriteFile(g_hOut, s, (DWORD)len, &w, 0);
}

static void OutLine(const char* s) { Out(s); Out("\r\n"); }

static void OutInt(const char* label, int64_t val) {
    char buf[256];
    wsprintfA(buf, "%s%I64d", label, val);
    OutLine(buf);
}

static void OutFloat(const char* label, double val) {
    int w = (int)val;
    int f = (int)((val - (double)w) * 1000.0);
    if (f < 0) f = -f;
    char buf[256];
    wsprintfA(buf, "%s%d.%03d", label, w, f);
    OutLine(buf);
}

// ============================================================================
// String helpers
// ============================================================================

static int StrCopy(char* dst, int off, const char* src) {
    int i = 0;
    while (src[i]) { dst[off + i] = src[i]; i++; }
    return off + i;
}

static int IntToStr(char* dst, int off, int val) {
    if (val == 0) { dst[off++] = '0'; return off; }
    if (val < 0) { dst[off++] = '-'; val = -val; }
    char tmp[16]; int ti = 0;
    while (val > 0) { tmp[ti++] = '0' + (val % 10); val /= 10; }
    while (ti > 0) dst[off++] = tmp[--ti];
    return off;
}

static int JsonEscape(const char* src, char* dst, int dstMax) {
    int si = 0, di = 0;
    while (src[si] && di < dstMax - 6) {
        char c = src[si++];
        if (c == '"' || c == '\\') { dst[di++] = '\\'; dst[di++] = c; }
        else if (c == '\n') { dst[di++] = '\\'; dst[di++] = 'n'; }
        else if (c == '\r') { dst[di++] = '\\'; dst[di++] = 'r'; }
        else if (c == '\t') { dst[di++] = '\\'; dst[di++] = 't'; }
        else if ((unsigned char)c < 0x20) continue;
        else dst[di++] = c;
    }
    dst[di] = 0;
    return di;
}

// ============================================================================
// Main
// ============================================================================

extern "C" void __stdcall mainCRTStartup() {
    g_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    
    OutLine("============================================================");
    OutLine("  RawrXD — Live Ollama Inference Validation");
    OutLine("  Direct HTTP: 127.0.0.1:11434 bg40-unleashed:latest");
    OutLine("============================================================");
    Out("\r\n");
    
    // ── Load Winsock ──
    Out("[1] Loading ws2_32.dll... ");
    HMODULE hWs2 = LoadLibraryA("ws2_32.dll");
    if (!hWs2) { OutLine("FAILED!"); ExitProcess(1); }
    
    pfn_WSAStartup  = (PFN_WSAStartup) GetProcAddress(hWs2, "WSAStartup");
    pfn_socket      = (PFN_socket)     GetProcAddress(hWs2, "socket");
    pfn_connect     = (PFN_connect)    GetProcAddress(hWs2, "connect");
    pfn_send        = (PFN_send_)      GetProcAddress(hWs2, "send");
    pfn_recv        = (PFN_recv_)      GetProcAddress(hWs2, "recv");
    pfn_closesocket = (PFN_closesocket)GetProcAddress(hWs2, "closesocket");
    pfn_setsockopt  = (PFN_setsockopt) GetProcAddress(hWs2, "setsockopt");
    
    if (!pfn_WSAStartup || !pfn_socket || !pfn_connect ||
        !pfn_send || !pfn_recv || !pfn_closesocket) {
        OutLine("FAILED (missing functions)!");
        ExitProcess(2);
    }
    
    unsigned char wsaData[512] = {0};
    if (pfn_WSAStartup(0x0202, wsaData) != 0) {
        OutLine("WSAStartup FAILED!");
        ExitProcess(3);
    }
    OutLine("OK");
    
    // ── Test prompts ──
    const char* prompts[] = {
        "// Complete this function:\nint fibonacci(int n) {",
        "Write a single MASM64 instruction that swaps RAX and RBX.",
    };
    int maxTokens[] = { 50, 20 };
    int numPrompts = 2;
    
    int64_t freq;
    QueryPerformanceFrequency(&freq);
    
    int totalTokens = 0;
    double totalMs = 0;
    int successCount = 0;
    
    for (int p = 0; p < numPrompts; p++) {
        char hdr[128];
        wsprintfA(hdr, "\r\n[%d/%d] Prompt: ", p+1, numPrompts);
        Out(hdr);
        // Show first 60 chars
        char trunc[64];
        int tlen = 0;
        while (prompts[p][tlen] && tlen < 58) {
            trunc[tlen] = (prompts[p][tlen] == '\n') ? ' ' : prompts[p][tlen];
            tlen++;
        }
        trunc[tlen] = 0;
        Out(trunc);
        OutLine("...");
        
        // Build JSON body
        static char escaped[8192];
        JsonEscape(prompts[p], escaped, 8192);
        
        static char body[16384];
        int bp = 0;
        bp = StrCopy(body, bp, "{\"model\":\"bg40-unleashed:latest\",");
        bp = StrCopy(body, bp, "\"prompt\":\"");
        bp = StrCopy(body, bp, escaped);
        bp = StrCopy(body, bp, "\",\"stream\":false,");
        bp = StrCopy(body, bp, "\"options\":{\"num_predict\":");
        bp = IntToStr(body, bp, maxTokens[p]);
        bp = StrCopy(body, bp, ",\"temperature\":0.4}}");
        body[bp] = 0;
        
        // Build HTTP request
        static char httpReq[20000];
        int hp = 0;
        hp = StrCopy(httpReq, hp, "POST /api/generate HTTP/1.1\r\n");
        hp = StrCopy(httpReq, hp, "Host: 127.0.0.1:11434\r\n");
        hp = StrCopy(httpReq, hp, "Content-Type: application/json\r\n");
        hp = StrCopy(httpReq, hp, "Content-Length: ");
        hp = IntToStr(httpReq, hp, bp);
        hp = StrCopy(httpReq, hp, "\r\nConnection: close\r\n\r\n");
        for (int i = 0; i < bp; i++) httpReq[hp + i] = body[i];
        hp += bp;
        
        // Connect
        Out("  Connecting to 127.0.0.1:11434... ");
        SOCKET_T s = pfn_socket(AF_INET_, SOCK_STREAM_, 0);
        if (s == INVALID_SOCK) { OutLine("socket() FAILED!"); continue; }
        
        SockAddrIn addr;
        for (int i = 0; i < (int)sizeof(addr); i++) ((char*)&addr)[i] = 0;
        addr.sin_family = AF_INET_;
        addr.sin_port   = (unsigned short)((11434 >> 8) | ((11434 & 0xFF) << 8));
        addr.sin_addr   = 0x0100007F;  // 127.0.0.1
        
        if (pfn_connect(s, &addr, sizeof(addr)) != 0) {
            OutLine("connect() FAILED!");
            pfn_closesocket(s);
            continue;
        }
        OutLine("OK");
        
        // Set recv timeout: 300 seconds
        if (pfn_setsockopt) {
            int rcvTimeout = 300000;
            pfn_setsockopt(s, 0xFFFF, 0x1006,
                           (const char*)&rcvTimeout, sizeof(rcvTimeout));
        }
        
        // Send
        Out("  Sending HTTP POST... ");
        int sent = 0;
        while (sent < hp) {
            int rc = pfn_send(s, httpReq + sent, hp - sent, 0);
            if (rc <= 0) break;
            sent += rc;
        }
        if (sent < hp) { OutLine("FAILED!"); pfn_closesocket(s); continue; }
        OutLine("OK");
        
        // Receive
        Out("  Waiting for response (stream:false, max 300s)... ");
        int64_t tStart;
        QueryPerformanceCounter(&tStart);
        
        static char recvBuf[262144];
        int totalRecv = 0;
        while (totalRecv < (int)sizeof(recvBuf) - 1) {
            int rc = pfn_recv(s, recvBuf + totalRecv,
                              (int)sizeof(recvBuf) - 1 - totalRecv, 0);
            if (rc <= 0) break;
            totalRecv += rc;
        }
        recvBuf[totalRecv] = 0;
        pfn_closesocket(s);
        
        int64_t tEnd;
        QueryPerformanceCounter(&tEnd);
        double elapsedMs = (double)(tEnd - tStart) * 1000.0 / (double)freq;
        
        if (totalRecv == 0) {
            OutLine("NO DATA!");
            continue;
        }
        
        OutLine("OK");
        OutInt("  Bytes received: ", totalRecv);
        OutFloat("  Elapsed: ", elapsedMs);
        Out(" ms\r\n");
        
        // Parse HTTP response — find body after \r\n\r\n
        const char* bodyStart = recvBuf;
        for (int i = 0; i < totalRecv - 3; i++) {
            if (recvBuf[i]=='\r' && recvBuf[i+1]=='\n' &&
                recvBuf[i+2]=='\r' && recvBuf[i+3]=='\n') {
                bodyStart = recvBuf + i + 4;
                break;
            }
        }
        
        // Extract "response":"..." from JSON
        const char* needle = "\"response\":\"";
        int nlen = 12;
        int jsonLen = totalRecv - (int)(bodyStart - recvBuf);
        
        static char response[16384];
        int oi = 0;
        for (int i = 0; i < jsonLen - nlen; i++) {
            bool match = true;
            for (int j = 0; j < nlen; j++) {
                if (bodyStart[i+j] != needle[j]) { match = false; break; }
            }
            if (!match) continue;
            int start = i + nlen;
            for (int k = start; k < jsonLen && oi < 16380; k++) {
                if (bodyStart[k] == '"' && (k == start || bodyStart[k-1] != '\\')) break;
                if (bodyStart[k] == '\\' && k + 1 < jsonLen) {
                    k++;
                    if (bodyStart[k] == 'n') response[oi++] = '\n';
                    else if (bodyStart[k] == 'r') response[oi++] = '\r';
                    else if (bodyStart[k] == 't') response[oi++] = '\t';
                    else if (bodyStart[k] == '"') response[oi++] = '"';
                    else if (bodyStart[k] == '\\') response[oi++] = '\\';
                    else { response[oi++] = '\\'; if (oi<16380) response[oi++] = bodyStart[k]; }
                } else {
                    response[oi++] = bodyStart[k];
                }
            }
            break;
        }
        response[oi] = 0;
        
        if (oi > 0) {
            int tokEst = (oi + 3) / 4;
            double tps = tokEst > 0 ? (double)tokEst / (elapsedMs / 1000.0) : 0;
            
            OutInt("  Response chars: ", oi);
            OutInt("  ~Tokens (est):  ", tokEst);
            OutFloat("  ~TPS:           ", tps);
            Out("  Response:\r\n    \"");
            // Print first 800 chars
            char preview[801];
            int plen = oi < 800 ? oi : 800;
            for (int i = 0; i < plen; i++) preview[i] = response[i];
            preview[plen] = 0;
            Out(preview);
            OutLine("\"");
            
            totalTokens += tokEst;
            totalMs += elapsedMs;
            successCount++;
        } else {
            OutLine("  [WARN] No 'response' field found in JSON!");
            Out("  Raw body (first 500 chars): ");
            char raw[501];
            int rlen = jsonLen < 500 ? jsonLen : 500;
            for (int i = 0; i < rlen; i++) raw[i] = bodyStart[i];
            raw[rlen] = 0;
            OutLine(raw);
        }
    }
    
    // ── Summary ──
    Out("\r\n");
    OutLine("============================================================");
    OutLine("  LIVE INFERENCE RESULTS");
    OutLine("============================================================");
    OutInt("  Prompts:    ", numPrompts);
    OutInt("  Successful: ", successCount);
    OutInt("  Tot tokens: ", totalTokens);
    OutFloat("  Tot time:   ", totalMs);
    if (successCount > 0) {
        OutFloat("  Avg TPS:    ", (double)totalTokens / (totalMs / 1000.0));
    }
    OutLine("============================================================");
    
    ExitProcess(0);
}
