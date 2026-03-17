// Win32IDE_NetworkRelay_Bridge.cpp — Formal ASM Integration Layer
#include "Win32IDE.h"
#include <winsock2.h>

// ASM Function Prototypes (x64 Windows ABI: RCX, RDX, R8, R9, stack align 16)
extern "C" {
    // Returns: 0=success, non-zero=error
    int __cdecl RelayEngine_Init(size_t poolSize);
    
    // Returns: ContextID (>=0) or -1 on error
    int __cdecl RelayEngine_CreateContext(
        SOCKET clientSocket, 
        SOCKET targetSocket, 
        void* entryPtr
    );
    
    // Blocks until connection closes
    void __cdecl RelayEngine_RunBiDirectional(int contextId);
    
    // Hotpatch installation for traffic inspection
    void __cdecl NetworkInstallSnipe(void* targetAddr, void* sniperFunc);
    
    // Statistics retrieval
    struct RelayStats {
        uint64_t bytesRx;
        uint64_t bytesTx;
        uint32_t active;
        uint32_t padding;
    };
    void __cdecl RelayEngine_GetStats(void* entryPtr, RelayStats* outStats);

    // Sniper system functions
    int __cdecl Sniper_InstallAtOffset(void* connectionContext, void* sniperContext, int offset);
    void* __cdecl SniperTrampolineDispatcher();
    void* __cdecl Sniper_ScanPattern(void* buffer, int bufLen, void* pattern, int patLen);
    void __cdecl Sniper_MutateBuffer(void* buffer, int bufLen, void* pattern, int patLen, void* replacement, int replLen);
}

// C++ Wrapper for RAII compliance
class NetworkRelayContext {
    int m_ctxId;
public:
    NetworkRelayContext(SOCKET client, SOCKET target, PortForwardEntry* entry) 
        : m_ctxId(-1) {
        if (entry) {
            m_ctxId = RelayEngine_CreateContext(client, target, entry);
        }
    }
    
    void Run() {
        if (m_ctxId >= 0) {
            RelayEngine_RunBiDirectional(m_ctxId);
        }
    }
    
    bool IsValid() const { return m_ctxId >= 0; }
};

// Replacement for std::thread(relayConnection...)
void StartRelayWorker(SOCKET client, SOCKET target, PortForwardEntry* entry) {
    std::thread([client, target, entry]() {
        NetworkRelayContext ctx(client, target, entry);
        if (ctx.IsValid()) {
            ctx.Run();
        }
    }).detach();
}

// Initialization hook called from Win32IDE::initNetworkPanel()
bool InitializeNetworkRelayEngine() {
    static bool initialized = false;
    if (!initialized) {
        // 64MB pool = 1024 concurrent connections * 64KB buffers
        int result = RelayEngine_Init(64 * 1024 * 1024);
        initialized = (result == 0);
    }
    return initialized;
}

// Sniper system structures and constants
enum SniperType {
    SNIPER_TYPE_INSPECT = 0,    // Read-only analysis
    SNIPER_TYPE_MUTATE = 1,     // Modify payload in-flight
    SNIPER_TYPE_BLOCK = 2,      // Drop matching packets
    SNIPER_TYPE_MIRROR = 3      // Copy to analysis buffer
};

#pragma pack(push, 1)
struct SniperContext {
    uint32_t Type;
    void* PatternPtr;           // Pattern to match (e.g., "password=")
    uint32_t PatternLen;
    void* ReplacementPtr;       // For MUTATE type
    uint32_t ReplacementLen;
    uint64_t HitCount;
    uint64_t ByteCounter;       // Atomic
    uint32_t Flags;
};
#pragma pack(pop)

// Sniper wrapper class
class NetworkTrafficSniper {
    SniperContext* m_context;
public:
    NetworkTrafficSniper(SniperType type, const char* pattern, const char* replacement = nullptr) 
        : m_context(nullptr) {
        m_context = new SniperContext();
        m_context->Type = type;
        m_context->PatternPtr = (void*)pattern;
        m_context->PatternLen = pattern ? strlen(pattern) : 0;
        m_context->ReplacementPtr = (void*)replacement;
        m_context->ReplacementLen = replacement ? strlen(replacement) : 0;
        m_context->HitCount = 0;
        m_context->ByteCounter = 0;
        m_context->Flags = 0;
    }
    
    ~NetworkTrafficSniper() {
        if (m_context) {
            delete m_context;
        }
    }
    
    bool InstallOnConnection(void* connectionContext, int offsetInRelayProc = 0x120) {
        if (!m_context || !connectionContext) return false;
        return Sniper_InstallAtOffset(connectionContext, m_context, offsetInRelayProc) == 0;
    }
    
    uint64_t GetHitCount() const { return m_context ? m_context->HitCount : 0; }
    uint64_t GetByteCount() const { return m_context ? m_context->ByteCounter : 0; }
};

// Helper function to install traffic inspector
void InstallTrafficInspector(SOCKET client, SOCKET target, PortForwardEntry* entry) {
    // Create sniper context for credential detection
    NetworkTrafficSniper* sniper = new NetworkTrafficSniper(
        SNIPER_TYPE_INSPECT, 
        "password="  // Look for credential leaks
    );
    
    // Get relay context ID
    int ctx = RelayEngine_CreateContext((uint64_t)client, (uint64_t)target, entry);
    if (ctx >= 0) {
        // Calculate connection context pointer
        // Assuming g_RelayContextPool is accessible or passed somehow
        // For now, this is a placeholder - would need to expose the pool
        void* connectionContext = nullptr; // TODO: Get from relay engine
        
        if (connectionContext) {
            sniper->InstallOnConnection(connectionContext);
        }
    }
}