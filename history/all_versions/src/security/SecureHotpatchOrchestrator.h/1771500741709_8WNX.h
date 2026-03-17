#pragma once
// SecureHotpatchOrchestrator.h — RBAC-protected patch layer with audit chain
// Wires Auth::RBACEngine → HotPatcher → AgentOrchestrator
// No Qt. No exceptions. C++20 only.

#include "../auth/rbac_engine.hpp"
#include "../hot_patcher.h"
#include "../agentic/AgentOrchestrator.h"
#include "../RawrXD_SignalSlot.h"
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <iostream>

class SecureHotpatchOrchestrator {
public:
    struct AuditEntry {
        std::string patch_name;
        std::string user;
        uint64_t timestamp;
        bool success;
    };

private:
    RawrXD::Auth::RBACEngine& m_rbac;
    HotPatcher& m_patcher;
    RawrXD::Agent::AgentOrchestrator& m_agent;
    RawrXD::Signal<const std::string&, bool> patchAuthorized;
    std::vector<AuditEntry> m_auditChain;
    std::mutex m_mutex;

public:
    SecureHotpatchOrchestrator(RawrXD::Auth::RBACEngine& rbac,
                                HotPatcher& patcher,
                                RawrXD::Agent::AgentOrchestrator& agent)
        : m_rbac(rbac), m_patcher(patcher), m_agent(agent)
    {
        patchAuthorized.connect([this](const std::string& name, bool ok) {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            m_auditChain.push_back({name, m_rbac.GetCurrentSession().username, ts, ok});
        });
    }

    // Request a patch with RBAC authorization check
    bool RequestPatch(const std::string& session_token,
                      const std::string& patch_name,
                      void* target_address,
                      const std::vector<unsigned char>& new_opcodes)
    {
        // Check RBAC permission
        auto auth = m_rbac.CheckPermission(session_token,
                                            RawrXD::Auth::Permission::HOTPATCH_MEMORY_WRITE);
        if (!auth.success) {
            std::cerr << "[SecureHotpatch] Denied: " << auth.detail << "\n";
            patchAuthorized.emit(patch_name, false);
            return false;
        }

        // Apply patch through HotPatcher
        bool ok = m_patcher.ApplyPatch(patch_name, target_address, new_opcodes);
        patchAuthorized.emit(patch_name, ok);

        if (ok) {
            std::cout << "[SecureHotpatch] Applied: " << patch_name << "\n";
            // Notify agent orchestrator of system event
            // (AgentOrchestrator doesn't have NotifySystemEvent, so we can run a status query)
        }

        return ok;
    }

    // Revert with RBAC check
    bool RequestRevert(const std::string& session_token, const std::string& patch_name) {
        auto auth = m_rbac.CheckPermission(session_token,
                                            RawrXD::Auth::Permission::HOTPATCH_MEMORY_REVERT);
        if (!auth.success) {
            std::cerr << "[SecureHotpatch] Revert denied: " << auth.detail << "\n";
            return false;
        }
        return m_patcher.RevertPatch(patch_name);
    }

    // Audit trail access
    const std::vector<AuditEntry>& GetAuditChain() const { return m_auditChain; }

    // Signal access
    auto& OnPatchAuthorized() { return patchAuthorized; }
};
