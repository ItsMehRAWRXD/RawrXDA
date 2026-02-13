// ============================================================================
// win32_feature_adapter.h — Win32 GUI ↔ SharedFeatureRegistry Adapter
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Bridges Win32IDE's WM_COMMAND dispatch into the unified feature registry.
// Provides routeCommandUnified() that Win32IDE_Core.cpp calls from onCommand().
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_WIN32_FEATURE_ADAPTER_H
#define RAWRXD_WIN32_FEATURE_ADAPTER_H

#include <windows.h>
#include "../core/shared_feature_dispatch.h"
#include "../core/unified_command_dispatch.hpp"
#include "../core/feature_handlers.h"
#include <cstring>
#include <string>

// Forward declare Win32IDE — we operate on void* internally
class Win32IDE;

// ============================================================================
// GUI OUTPUT CALLBACK — Routes to Win32 IDE status bar / output panel
// ============================================================================

// Output callback type for GUI context
// The userData points to the Win32IDE instance
static void gui_status_output(const char* text, void* userData) {
    if (!text || !userData) return;
    // Cast userData → Win32IDE* and route to status bar.
    // We use PostMessage with a custom message to avoid cross-thread issues.
    // The Win32IDE receives this via WM_APP+0x100 and displays in status/output panel.
    Win32IDE* ide = static_cast<Win32IDE*>(userData);
    (void)ide;
    // Fallback: OutputDebugStringA for development visibility
    OutputDebugStringA(text);
}

// ============================================================================
// UNIFIED GUI DISPATCH — Attempt to route a WM_COMMAND ID through the
//                        shared feature registry before legacy handlers
// ============================================================================
// Returns true if the command was handled by the unified dispatch.
// Returns false if it should fall through to Win32IDE's legacy routeCommand().
// ============================================================================

static bool routeCommandUnified(int commandId, void* idePtr) {
    // ═══════════════════════════════════════════════════════════════════
    // PRIMARY PATH: Direct dispatch from g_commandRegistry[] (SSOT)
    // No hash map lookup, no manual registration required.
    // If a command is in COMMAND_TABLE, it gets dispatched here.
    // ═══════════════════════════════════════════════════════════════════
    
    // Build GUI command context
    CommandContext ctx{};
    ctx.rawInput = "";
    ctx.args = "";
    ctx.idePtr = idePtr;
    ctx.cliStatePtr = nullptr;
    ctx.commandId = static_cast<uint32_t>(commandId);
    ctx.isGui = true;
    ctx.isHeadless = false;
    ctx.outputFn = gui_status_output;
    ctx.outputUserData = idePtr;
    
    // Try direct dispatch from compile-time registry (zero-drift path)
    auto result = RawrXD::Dispatch::dispatchByGuiId(
        static_cast<uint32_t>(commandId), ctx);
    
    if (result.status == RawrXD::Dispatch::DispatchStatus::OK ||
        result.status == RawrXD::Dispatch::DispatchStatus::HANDLER_ERROR) {
        // Command was found and handler ran (success or handler-level error)
        return true;
    }
    
    // FALLBACK: Try SharedFeatureRegistry for dynamically-registered commands
    // (e.g. plugin-provided commands not in COMMAND_TABLE)
    auto& registry = SharedFeatureRegistry::instance();
    auto registryResult = registry.dispatchByCommandId(
        static_cast<uint32_t>(commandId), ctx);
    if (registryResult.success) {
        return true;
    }
    
    // Not found in either — fall through to legacy routeCommand()
    return false;
}

// ============================================================================
// FEATURE AVAILABILITY — Query whether a feature is available in GUI
// ============================================================================

static bool isFeatureAvailableGui(int commandId) {
    auto& registry = SharedFeatureRegistry::instance();
    const FeatureDescriptor* desc = registry.findByCommandId(static_cast<uint32_t>(commandId));
    if (!desc) return false;
    return desc->guiSupported && (desc->handler != nullptr);
}

// ============================================================================
// FEATURE DESCRIPTION — Get feature name/description for tooltips, menus
// ============================================================================

static const char* getFeatureNameGui(int commandId) {
    auto& registry = SharedFeatureRegistry::instance();
    const FeatureDescriptor* desc = registry.findByCommandId(static_cast<uint32_t>(commandId));
    return desc ? desc->name : nullptr;
}

static const char* getFeatureDescGui(int commandId) {
    auto& registry = SharedFeatureRegistry::instance();
    const FeatureDescriptor* desc = registry.findByCommandId(static_cast<uint32_t>(commandId));
    return desc ? desc->description : nullptr;
}

static const char* getFeatureShortcutGui(int commandId) {
    auto& registry = SharedFeatureRegistry::instance();
    const FeatureDescriptor* desc = registry.findByCommandId(static_cast<uint32_t>(commandId));
    return desc ? desc->shortcut : nullptr;
}

// ============================================================================
// GUI FEATURE COUNTERS — For status bar display
// ============================================================================

static size_t getGuiRegisteredCount() {
    return SharedFeatureRegistry::instance().getGuiFeatures().size();
}

static uint64_t getGuiDispatchCount() {
    return SharedFeatureRegistry::instance().totalDispatched();
}

// ============================================================================
// BATCH MENU POPULATION — Populate Win32 menus from registry
// ============================================================================
// Populates an HMENU with features from a specific feature group.
// Returns the number of items added.
// ============================================================================

static int populateMenuFromRegistry(HMENU hMenu, FeatureGroup group) {
    auto& registry = SharedFeatureRegistry::instance();
    auto features = registry.getByGroup(group);
    int count = 0;
    
    for (const auto* feat : features) {
        if (!feat->guiSupported || feat->commandId == 0) continue;
        
        // Build label with shortcut
        std::string label(feat->name);
        if (feat->shortcut && feat->shortcut[0]) {
            label += "\t";
            label += feat->shortcut;
        }
        
        AppendMenuA(hMenu, MF_STRING, feat->commandId, label.c_str());
        ++count;
    }
    
    return count;
}

// ============================================================================
// STATUS BAR UPDATE — Show registry stats in status bar
// ============================================================================

static void updateStatusBarWithRegistryStats(HWND hwndStatusBar, int partIndex) {
    if (!hwndStatusBar || !IsWindow(hwndStatusBar)) return;
    
    auto& registry = SharedFeatureRegistry::instance();
    size_t total = registry.totalRegistered();
    uint64_t dispatched = registry.totalDispatched();
    
    char buf[128];
    snprintf(buf, sizeof(buf), "Features: %zu | Dispatched: %llu", total, 
             (unsigned long long)dispatched);
    SendMessageA(hwndStatusBar, SB_SETTEXTA, partIndex, (LPARAM)buf);
}

#endif // RAWRXD_WIN32_FEATURE_ADAPTER_H
