#pragma once
// HotpatchBridgeUnified.h — Single orchestrator replacing agent/agentic duplication
// Bridges HotPatcher ↔ LSP ↔ EventBus into a unified hotpatch pipeline
// No Qt. No exceptions. C++20 only.

#include "../hot_patcher.h"
#include "../lsp/lsp_hotpatch_bridge.hpp"
#include "../EventBus.h"
#include <string>
#include <iostream>

class HotpatchBridgeUnified {
    HotPatcher& m_engine;
    LSPHotpatchBridge& m_lsp;

public:
    HotpatchBridgeUnified(HotPatcher& engine, LSPHotpatchBridge& lsp)
        : m_engine(engine), m_lsp(lsp)
    {
        RawrXD::EventBus::Get().HotpatchApplied.connect([this]() {
            m_lsp.InvalidatePatchSymbols();
        });
    }

    bool ApplyWithLSPUpdate(const std::string& name,
                             void* target,
                             const std::vector<unsigned char>& opcodes)
    {
        if (m_engine.ApplyPatch(name, target, opcodes)) {
            std::cout << "[HotpatchBridge] Applied + LSP invalidated: " << name << "\n";
            RawrXD::EventBus::Get().HotpatchApplied.emit();
            return true;
        }
        return false;
    }

    bool RevertWithLSPUpdate(const std::string& name) {
        if (m_engine.RevertPatch(name)) {
            RawrXD::EventBus::Get().HotpatchReverted.emit(name);
            m_lsp.InvalidatePatchSymbols();
            return true;
        }
        return false;
    }

    HotPatcher& GetEngine() { return m_engine; }
    LSPHotpatchBridge& GetLSP() { return m_lsp; }
};
