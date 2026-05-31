#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <map>
#include "SovereignTelemetryHub.h"

namespace RawrXD::Runtime {

struct HotpatchBlock {
    void* targetAddr;
    void* originalData;
    void* newData;
    size_t size;
    bool isActive;
};

class SovereignHotpatchBridge {
public:
    static SovereignHotpatchBridge& instance();

    bool initialize();
    void shutdown();

    // Hotpatch Operations
    bool applyPatch(void* target, void* data, size_t size);
    bool revertPatch(void* target);
    bool verifyPatch(void* target);

    // Sovereign JIT: Mutation of ToolEngine worker functions
    bool mutateToolWorker(uint32_t slotId, void* specializedCode);

private:
    SovereignHotpatchBridge() : m_active(false) {}
    ~SovereignHotpatchBridge() { shutdown(); }

    void guardMemory(void* addr, size_t size, uint32_t protectType);

    bool m_active;
    std::mutex m_patchMutex;
    std::map<void*, HotpatchBlock> m_patchRegistry;
};

} // namespace RawrXD::Runtime
