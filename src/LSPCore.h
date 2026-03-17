#pragma once
// LSPCore.h — Consolidated LSP integration point
// Replaces 4 competing LSP headers with a single unified facade
// No Qt. No exceptions. C++20 only.

#include "lsp_client.h"
#include "lsp/lsp_hotpatch_bridge.hpp"
#include "EventBus.h"
#include <string>
#include <iostream>

class LSPCore {
    LSPClient m_client;
    LSPHotpatchBridge* m_patchBridge = nullptr;

public:
    LSPCore() = default;

    void Initialize() {
        RawrXD::EventBus::Get().FileOpened.connect([this](const std::string& file) {
            m_client.DidOpen(file);
        });

        RawrXD::EventBus::Get().FileSaved.connect([this](const std::string& file) {
            m_client.DidSave(file);
        });

        RawrXD::EventBus::Get().FileClosing.connect([this](const std::string& file) {
            m_client.DidClose(file);
        });

        std::cout << "[LSPCore] Initialized with EventBus wiring\n";
    }

    void BridgeHotpatch(LSPHotpatchBridge* bridge) {
        m_patchBridge = bridge;
        if (m_patchBridge) {
            RawrXD::EventBus::Get().HotpatchApplied.connect([this]() {
                m_patchBridge->InvalidatePatchSymbols();
            });
        }
    }

    LSPClient& GetClient() { return m_client; }
    LSPHotpatchBridge* GetPatchBridge() { return m_patchBridge; }
};

// Compatibility aliases for old include patterns
#define LSPIntegration LSPCore
#define LanguageServerIntegration LSPCore
