// RawrXD_Win32IDEBridge.cpp
// Implementation of full integration bridge

#include "RawrXD_Win32IDEBridge.hpp"
#include <spdlog/spdlog.h>

namespace RawrXD::Agentic::Bridge {

std::expected<std::unique_ptr<Win32IDEBridge>, BridgeError>
Win32IDEBridge::create() {
    auto bridge = std::make_unique<Win32IDEBridge>();
    if (!bridge->initialize()) {
        return std::unexpected(BridgeError::InitializationFailed);
    }
    return bridge;
}

bool Win32IDEBridge::initialize() {
    spdlog::info("Initializing Win32IDEBridge...");
    
    kernel_ = std::make_unique<Kernel::AutonomousCommandKernel>(4);
    if (!kernel_) {
        spdlog::error("Failed to initialize AutonomousCommandKernel");
        return false;
    }
    kernel_->start();
    
    auto mmfResult = MMF::MmfProducer::create(L"RawrXD_MMF_Global");
    if (!mmfResult) {
        spdlog::error("Failed to initialize MMF producer");
        return false;
    }
    mmfProducer_ = std::move(mmfResult.value());
    
    auto hotpatchResult = Hotpatch::HotpatchEngine::create();
    if (!hotpatchResult) {
        spdlog::error("Failed to initialize Hotpatch engine");
        return false;
    }
    hotpatchEngine_ = std::move(hotpatchResult.value());
    
    auto manifestResult = Manifest::SelfManifestor::scanCurrentProcess();
    if (!manifestResult) {
        spdlog::error("Failed to scan manifest");
        return false;
    }
    manifestor_ = std::move(manifestResult.value());
    
    spdlog::info("Win32IDEBridge initialized successfully");
    return true;
}

Win32IDEBridge::~Win32IDEBridge() {
    shutdown();
}

std::expected<bool, BridgeError>
Win32IDEBridge::routeCommandThroughKernel(uint32_t commandId,
                                         const std::wstring& params) {
    if (!kernel_) {
        return std::unexpected(BridgeError::ComponentNotReady);
    }
    
    Kernel::Command cmd{
        .id = commandId,
        .priority = Kernel::CommandPriority::Normal,
        .createdAt = GetTickCount64()
    };
    
    if (kernel_->submit(cmd)) {
        commandsRouted_.fetch_add(1, std::memory_order_relaxed);
        spdlog::debug("Command routed: 0x{:X}", commandId);
        return true;
    }
    
    return std::unexpected(BridgeError::RoutingFailed);
}

std::expected<bool, BridgeError>
Win32IDEBridge::dispatchToolViaMMF(uint32_t toolId,
                                   const std::string& jsonPayload,
                                   uint64_t correlationId) {
    if (!mmfProducer_) {
        return std::unexpected(BridgeError::ComponentNotReady);
    }
    
    std::span<const uint8_t> payload(
        reinterpret_cast<const uint8_t*>(jsonPayload.data()),
        jsonPayload.size()
    );
    
    if (!mmfProducer_->sendToolInvocation(toolId, payload, correlationId)) {
        spdlog::warn("Tool dispatch failed for toolId: {}", toolId);
        return std::unexpected(BridgeError::RoutingFailed);
    }
    
    if (!mmfProducer_->isConsumerHealthy(500)) {
        spdlog::warn("MMF consumer unhealthy for toolId: {}", toolId);
    }
    
    toolsDispatched_.fetch_add(1, std::memory_order_relaxed);
    spdlog::debug("Tool dispatched: {} bytes via MMF", payload.size());
    return true;
}

bool Win32IDEBridge::applyHotpatch(uint64_t targetAddr, uint64_t destAddr) {
    if (!hotpatchEngine_) return false;
    
    auto result = hotpatchEngine_->patchWithRelativeJump(targetAddr, destAddr);
    return result.has_value();
}

bool Win32IDEBridge::verifyIntegration() const {
    bool ok = true;
    
    if (!kernel_) {
        spdlog::error("Kernel not initialized");
        ok = false;
    }
    
    if (!mmfProducer_) {
        spdlog::error("MMF producer not initialized");
        ok = false;
    }
    
    if (!hotpatchEngine_) {
        spdlog::error("Hotpatch engine not initialized");
        ok = false;
    }
    
    if (!manifestor_) {
        spdlog::error("Manifestor not initialized");
        ok = false;
    }
    
    if (ok) {
        auto mmfStats = mmfProducer_->getStats();
        auto hotpatchStats = hotpatchEngine_->getStats();
        auto manifestStats = manifestor_->getStats();
        
        spdlog::info("Bridge status: MMF healthy, {} hotpatches applied, {} capabilities",
                     hotpatchStats.patchesApplied,
                     manifestStats.capabilityExports);
    }
    
    return ok;
}

Win32IDEBridge::BridgeStats Win32IDEBridge::getStats() const {
    uint32_t hotpatches = hotpatchEngine_ ? hotpatchEngine_->getStats().patchesApplied : 0;
    uint32_t capabilities = manifestor_ ? manifestor_->getStats().capabilityExports : 0;
    
    return {
        commandsRouted_.load(),
        toolsDispatched_.load(),
        hotpatches,
        capabilities
    };
}

void Win32IDEBridge::shutdown() {
    if (mmfProducer_) {
        mmfProducer_->signalShutdown();
    }
    
    if (hotpatchEngine_) {
        hotpatchEngine_->rollbackAll();
    }
    
    if (kernel_) {
        kernel_->stop();
    }
    
    spdlog::info("Win32IDEBridge shutdown complete");
}

static Win32IDEBridge* g_bridge = nullptr;

Win32IDEBridge& getGlobalBridge() {
    if (!g_bridge) {
        auto result = Win32IDEBridge::create();
        if (result) {
            g_bridge = result.value().release();
        }
    }
    return *g_bridge;
}

}
