// ContextFusionWiring.cpp — Integration wiring for ContextFusionEngine
// Connects all existing IDE subsystems to the unified context runtime.
//
// This file is the "glue" that transforms fragmented subsystems into one
// coherent IDE. No subsystem should be modified — only wired.

#include "core/ContextFusionEngine.h"
#include "win32app/GhostTextContextSubscriber.h"
#include "win32app/ChatPanelModelCaller.h"
#include "win32app/Win32IDE_GhostText.h"
#include "chat_interface.h"
#include "EventBus.h"

namespace RawrXD {

// ─────────────────────────────────────────────────────────────────────────────
// Global wiring state
// ─────────────────────────────────────────────────────────────────────────────

static std::unique_ptr<GhostTextContextSubscriber> g_ghostTextSubscriber;
static std::unique_ptr<ChatPanelModelCaller> g_chatPanelCaller;
static bool g_contextFusionWired = false;

// ─────────────────────────────────────────────────────────────────────────────
// EventBus → ContextFusionEngine bridge
// Converts legacy EventBus signals into ContextEvents
// ─────────────────────────────────────────────────────────────────────────────

class EventBusContextBridge {
public:
    static void Install() {
        auto& bus = EventBus::Get();
        auto& engine = ContextFusionEngine::Get();
        
        // Editor events
        bus.Subscribe(EventType::FileOpened, [](const Event& ev) {
            ContextFusionEngine::Get().EmitEvent(
                ContextEvent(ContextEvent::FILE_OPENED, "EventBus", (void*)&ev.data)
            );
        });
        
        bus.Subscribe(EventType::FileSaved, [](const Event& ev) {
            ContextFusionEngine::Get().EmitEvent(
                ContextEvent(ContextEvent::FILE_SAVED, "EventBus", (void*)&ev.data)
            );
        });
        
        // Agent events
        bus.Subscribe(EventType::AIResponse, [](const Event& ev) {
            ContextFusionEngine::Get().EmitEvent(
                ContextEvent(ContextEvent::AGENT_ACTION, "EventBus", (void*)&ev.data)
            );
        });
        
        // Build events — map to generic tool execution
        bus.Subscribe(EventType::ExtensionActivated, [](const Event& ev) {
            ContextFusionEngine::Get().EmitEvent(
                ContextEvent(ContextEvent::TOOL_EXECUTED, "EventBus", (void*)&ev.data)
            );
        });
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// UnifiedEditorContext → ContextFusionEngine bridge
// Converts legacy UnifiedEditorContext updates into ContextEvents
// ─────────────────────────────────────────────────────────────────────────────

class EditorContextBridge {
public:
    static void Install() {
        // This would connect to the existing UnifiedEditorContext
        // and emit ContextEvents on changes
        // Implementation depends on UnifiedEditorContext's signal interface
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Public API: WireContextFusion()
// Call this once during IDE initialization
// ─────────────────────────────────────────────────────────────────────────────

bool WireContextFusion(
    Win32IDE_GhostText* ghostText,
    IChatPanel* chatPanel,
    const std::string& modelEndpoint
) {
    if (g_contextFusionWired) {
        return true; // Already wired
    }
    
    // 1. Initialize ContextFusionEngine
    ContextFusionEngine::Get().Initialize();
    
    // 2. Install EventBus bridge
    EventBusContextBridge::Install();
    
    // 3. Wire Ghost Text
    if (ghostText) {
        g_ghostTextSubscriber = std::make_unique<GhostTextContextSubscriber>(ghostText);
        ContextFusionEngine::Get().Subscribe(g_ghostTextSubscriber.get());
    }
    
    // 4. Wire Chat Panel
    if (chatPanel) {
        g_chatPanelCaller = std::make_unique<ChatPanelModelCaller>(chatPanel);
        if (g_chatPanelCaller->Initialize(modelEndpoint)) {
            ContextFusionEngine::Get().Subscribe(g_chatPanelCaller.get());
            chatPanel->SetModelCaller(g_chatPanelCaller.get());
        }
    }
    
    // 5. Mark as wired
    g_contextFusionWired = true;
    
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API: UnwireContextFusion()
// Call this during IDE shutdown
// ─────────────────────────────────────────────────────────────────────────────

void UnwireContextFusion() {
    if (!g_contextFusionWired) return;
    
    // Unsubscribe all
    if (g_ghostTextSubscriber) {
        ContextFusionEngine::Get().Unsubscribe(g_ghostTextSubscriber.get());
        g_ghostTextSubscriber.reset();
    }
    
    if (g_chatPanelCaller) {
        ContextFusionEngine::Get().Unsubscribe(g_chatPanelCaller.get());
        g_chatPanelCaller->Shutdown();
        g_chatPanelCaller.reset();
    }
    
    // Shutdown engine
    ContextFusionEngine::Get().Shutdown();
    
    g_contextFusionWired = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API: IsContextFusionWired()
// ─────────────────────────────────────────────────────────────────────────────

bool IsContextFusionWired() {
    return g_contextFusionWired;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API: GetContextFusionStats()
// For debugging and monitoring
// ─────────────────────────────────────────────────────────────────────────────

struct ContextFusionStats {
    bool isWired;
    uint64_t frameVersion;
    size_t subscriberCount;
    std::string activeSubscribers;
};

ContextFusionStats GetContextFusionStats() {
    ContextFusionStats stats;
    stats.isWired = g_contextFusionWired;
    stats.frameVersion = ContextFusionEngine::Get().GetFrameVersion();
    stats.subscriberCount = ContextFusionEngine::Get().GetSubscriberCount();
    
    // Build subscriber list
    // (Would iterate actual subscribers in real implementation)
    stats.activeSubscribers = "GhostText, ChatPanel";
    
    return stats;
}

} // namespace RawrXD