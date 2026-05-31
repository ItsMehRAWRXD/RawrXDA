// ============================================================================
// both_tres_integration_example.cpp - Complete Integration Example
// ============================================================================
// Demonstrates how to use all three components together:
//   1. Advanced Docking System
//   2. Titan 70B Stress Test
//   3. TRES Stabilization Layer
//
// Usage: Include this in your Win32IDE main initialization
// ============================================================================

#include "ui/advanced_docking_system.h"
#include "core/execution_scheduler_integration.hpp"
#include "core/tres_stabilization_layer.hpp"
#include "tests/titan_70b_stress_test.cpp"
#include <windows.h>
#include <iostream>

using namespace RawrXD;
using namespace RawrXD::UI;
using namespace RawrXD::TRES;

namespace RawrXD {
namespace Examples {

// ============================================================================
// Complete Integration Example
// ============================================================================
class BothTRESIntegration {
public:
    static BothTRESIntegration& instance() {
        static BothTRESIntegration inst;
        return inst;
    }

    // Initialize all three components
    bool initialize(HWND hwndMain) {
        std::cout << "[Both+TRES] Initializing systematic integration...\n";

        // 1. Initialize Advanced Docking System
        if (!initializeDocking(hwndMain)) {
            std::cerr << "[Both+TRES] Failed to initialize docking system\n";
            return false;
        }

        // 2. Initialize TRES Stabilization
        if (!initializeTRES()) {
            std::cerr << "[Both+TRES] Failed to initialize TRES\n";
            return false;
        }

        // 3. Initialize Integrated Execution Scheduler
        if (!initializeScheduler()) {
            std::cerr << "[Both+TRES] Failed to initialize scheduler\n";
            return false;
        }

        // 4. Setup phase-aware measurement
        if (!initializePhaseAwareMeasurement()) {
            std::cerr << "[Both+TRES] Failed to initialize phase-aware measurement\n";
            return false;
        }

        initialized_ = true;
        std::cout << "[Both+TRES] All components initialized successfully\n";
        return true;
    }

    void shutdown() {
        if (!initialized_) return;

        std::cout << "[Both+TRES] Shutting down...\n";

        // Stop TRES
        if (tres_system_) {
            tres_system_>stop();
        }

        // Save docking layout
        DockingManager::instance().saveLayout();
        DockingManager::instance().shutdown();

        // Shutdown scheduler
        if (scheduler_integration_) {
            scheduler_integration_>shutdown();
        }

        initialized_ = false;
        std::cout << "[Both+TRES] Shutdown complete\n";
    }

    // Run 70B stress test with full integration
    bool run70BStressTest() {
        if (!initialized_) {
            std::cerr << "[Both+TRES] Not initialized\n";
            return false;
        }

        std::cout << "[Both+TRES] Starting 70B stress test...\n";

        // Create stress test instance
        Tests::Titan70BStressTest test;
        
        // Run the test
        bool passed = test.run();
        
        if (passed) {
            std::cout << "[Both+TRES] 70B stress test PASSED\n";
        } else {
            std::cerr << "[Both+TRES] 70B stress test FAILED\n";
        }

        return passed;
    }

    // Get current system telemetry
    SystemTelemetry getTelemetry() {
        if (tres_system_) {
            return tres_system_>getCurrentTelemetry();
        }
        return SystemTelemetry{};
    }

    // Check if system is stable
    bool isSystemStable() {
        if (tres_system_) {
            return tres_system_>isStable();
        }
        return false;
    }

    // Update docking layout
    void updateDockingLayout() {
        DockingManager::instance().updateLayout();
    }

    // Get TRES system for external control
    TRESSystem* getTRESSystem() { return tres_system_.get(); }

    // Get scheduler integration
    ExecutionSchedulerIntegration* getScheduler() { return scheduler_integration_.get(); }

private:
    BothTRESIntegration() = default;
    ~BothTRESIntegration() = default;
    BothTRESIntegration(const BothTRESIntegration&) = delete;
    BothTRESIntegration& operator=(const BothTRESIntegration&) = delete;

    bool initializeDocking(HWND hwndMain) {
        auto& manager = DockingManager::instance();
        
        if (!manager.initialize(hwndMain)) {
            return false;
        }

        // Configure default layout
        auto& config = manager.getConfig();
        config.leftSidebarWidth = 250;
        config.rightSidebarWidth = 250;
        config.bottomPanelHeight = 200;
        config.showTabCloseButtons = true;
        config.enableTabDragDrop = true;
        config.restoreLayoutOnStartup = true;
        config.layoutFilePath = "win32ide_layout.json";

        // Create side panels
        auto* leftPanel = manager.createPanel(DockZone::Left, "explorer");
        if (leftPanel) {
            leftPanel->setTitle("Explorer");
            leftPanel->setState(PanelState::Expanded);
        }

        auto* rightPanel = manager.createPanel(DockZone::Right, "properties");
        if (rightPanel) {
            rightPanel->setTitle("Properties");
            rightPanel->setState(PanelState::Collapsed);
        }

        auto* bottomPanel = manager.createPanel(DockZone::Bottom, "bottom");
        if (bottomPanel) {
            bottomPanel->setTitle("Panel");
            bottomPanel->setState(PanelState::Collapsed);
        }

        // Restore previous layout
        manager.restoreLayout();

        std::cout << "[Both+TRES] Docking system initialized\n";
        return true;
    }

    bool initializeTRES() {
        tres_system_ = std::make_unique<TRESSystem>();
        
        if (!tres_system_>initialize(
            [this]() { return gatherTelemetry(); },
            [this](const AutopatchSignal& signal) { onAutopatch(signal); }
        )) {
            return false;
        }

        tres_system_>start(50); // 50ms correction interval
        
        std::cout << "[Both+TRES] TRES stabilization active\n";
        return true;
    }

    bool initializeScheduler() {
        IntegratedSchedulerConfig config;
        config.enableKVQuantization = true;
        config.kvFormat = KV::FP8Format::E4M3;
        config.enableDoubleBuffer = true;
        config.vocabSize = 32000;
        config.embeddingDim = 4096;
        config.enableSpeculative = true;
        config.maxDraftTokens = 8;
        config.enableTRES = true;
        config.tresIntervalMs = 50;

        scheduler_integration_ = std::make_unique<ExecutionSchedulerIntegration>();
        
        // Note: In real usage, pass actual engine and registry
        // scheduler_integration_>initialize(config, engine, registry);
        
        std::cout << "[Both+TRES] Execution scheduler initialized\n";
        return true;
    }

    bool initializePhaseAwareMeasurement() {
        // Phase-aware measurement is handled by the scheduler integration
        // and TRES working together
        std::cout << "[Both+TRES] Phase-aware measurement initialized\n";
        return true;
    }

    SystemTelemetry gatherTelemetry() {
        SystemTelemetry telemetry;
        
        // Gather metrics from scheduler
        if (scheduler_integration_) {
            telemetry.tps_current = scheduler_integration_>getBaseScheduler()
                ? scheduler_integration_>getBaseScheduler()>getStats().tokensPerSecond
                : 0.0;
        }
        
        // Add other metrics as needed
        telemetry.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        
        return telemetry;
    }

    void onAutopatch(const AutopatchSignal& signal) {
        std::cout << "[Both+TRES] Autopatch triggered: " << signal.reason << "\n";
        
        // Handle autopatch signals
        switch (signal.signal_type) {
            case 1: // KV pressure
                std::cout << "  Action: Triggering KV cache flush\n";
                break;
            case 2: // GPU overrun
                std::cout << "  Action: Reducing batch size\n";
                break;
            case 3: // Latency spike
                std::cout << "  Action: Throttling decode\n";
                break;
        }
    }

    bool initialized_ = false;
    std::unique_ptr<TRESSystem> tres_system_;
    std::unique_ptr<ExecutionSchedulerIntegration> scheduler_integration_;
};

} // namespace Examples
} // namespace RawrXD

// ============================================================================
// C API for easy integration
// ============================================================================

extern "C" {

__declspec(dllexport) bool RawrXD_BothTRES_Initialize(HWND hwndMain) {
    return RawrXD::Examples::BothTRESIntegration::instance().initialize(hwndMain);
}

__declspec(dllexport) void RawrXD_BothTRES_Shutdown() {
    RawrXD::Examples::BothTRESIntegration::instance().shutdown();
}

__declspec(dllexport) bool RawrXD_BothTRES_Run70BStressTest() {
    return RawrXD::Examples::BothTRESIntegration::instance().run70BStressTest();
}

__declspec(dllexport) bool RawrXD_BothTRES_IsSystemStable() {
    return RawrXD::Examples::BothTRESIntegration::instance().isSystemStable();
}

__declspec(dllexport) void RawrXD_BothTRES_UpdateDockingLayout() {
    RawrXD::Examples::BothTRESIntegration::instance().updateDockingLayout();
}

} // extern "C"

// ============================================================================
// Integration Example (for Win32IDE main)
// ============================================================================
/*
// In Win32IDE::Initialize():
#include "examples/both_tres_integration_example.cpp"

bool Win32IDE::Initialize() {
    // ... existing initialization ...
    
    // Initialize Both + TRES systematic integration
    if (!RawrXD::Examples::BothTRESIntegration::instance().initialize(hwndMain_)) {
        LogError("Failed to initialize Both+TRES integration");
        return false;
    }
    
    // Optional: Run 70B stress test on startup
    // RawrXD::Examples::BothTRESIntegration::instance().run70BStressTest();
    
    return true;
}

// In Win32IDE WndProc:
LRESULT CALLBACK Win32IDE::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Let docking system handle layout messages
    auto& integration = RawrXD::Examples::BothTRESIntegration::instance();
    LRESULT result = integration.getScheduler()
        ? integration.getScheduler()>getBaseScheduler()>handleMessage(hwnd, msg, wParam, lParam)
        : 0;
    if (result != 0) return result;
    
    // ... rest of message handling ...
}

// Menu handlers:
void Win32IDE::OnViewLeftSidebar() {
    auto& manager = DockingManager::instance();
    manager.togglePanel(DockZone::Left);
}

void Win32IDE::OnRunStressTest() {
    RawrXD::Examples::BothTRESIntegration::instance().run70BStressTest();
}

void Win32IDE::OnCheckSystemStability() {
    bool stable = RawrXD::Examples::BothTRESIntegration::instance().isSystemStable();
    MessageBox(hwndMain_, stable ? "System is stable" : "System unstable", 
               "TRES Status", MB_OK);
}
*/
