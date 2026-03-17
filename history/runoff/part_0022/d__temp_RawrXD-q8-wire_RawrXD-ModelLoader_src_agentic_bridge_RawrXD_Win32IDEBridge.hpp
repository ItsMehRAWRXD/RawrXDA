// RawrXD_Win32IDEBridge.hpp
// Integration bridge: CommandRegistry + PredictiveKernel + MMF + Hotpatch

#pragma once
#include "RawrXD_PredictiveCommandKernel.hpp"
#include "RawrXD_MmfProducer.hpp"
#include "RawrXD_HotpatchEngine.hpp"
#include "RawrXD_SelfManifestor.hpp"
#include <memory>
#include <expected>

namespace RawrXD::Agentic::Bridge {

enum class BridgeError {
    Success = 0,
    InitializationFailed,
    ComponentNotReady,
    RoutingFailed,
    MMFTimeout
};

class Win32IDEBridge {
public:
    static std::expected<std::unique_ptr<Win32IDEBridge>, BridgeError> create();
    
    ~Win32IDEBridge();
    
    Win32IDEBridge(const Win32IDEBridge&) = delete;
    Win32IDEBridge& operator=(const Win32IDEBridge&) = delete;
    
    std::expected<bool, BridgeError> 
        routeCommandThroughKernel(uint32_t commandId,
                                 const std::wstring& params = L"");
    
    std::expected<bool, BridgeError>
        dispatchToolViaMMF(uint32_t toolId,
                          const std::string& jsonPayload,
                          uint64_t correlationId = 0);
    
    bool applyHotpatch(uint64_t targetAddr, uint64_t destAddr);
    bool verifyIntegration() const;
    
    struct BridgeStats {
        uint32_t commandsRouted;
        uint32_t toolsDispatched;
        uint32_t hotpatchesApplied;
        uint32_t capabilitiesDiscovered;
    };
    BridgeStats getStats() const;
    
    void shutdown();

private:
    Win32IDEBridge() = default;
    bool initialize();
    
    std::unique_ptr<Kernel::AutonomousCommandKernel> kernel_;
    std::unique_ptr<MMF::MmfProducer> mmfProducer_;
    std::unique_ptr<Hotpatch::HotpatchEngine> hotpatchEngine_;
    std::unique_ptr<Manifest::SelfManifestor> manifestor_;
    
    std::atomic<uint32_t> commandsRouted_{0};
    std::atomic<uint32_t> toolsDispatched_{0};
};

Win32IDEBridge& getGlobalBridge();

} // namespace
