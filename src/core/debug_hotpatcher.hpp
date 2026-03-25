// debug_hotpatcher.hpp — Debug-focused hotpatch wiring helpers
// Installs policy-safe rewrite rules and exposes debug utilities.
#pragma once

#include "patch_result.hpp"
#include "proxy_hotpatcher.hpp"
#include "address_hotpatcher.hpp"

#include <mutex>
#include <string>
#include <vector>

class DebugHotpatcher {
public:
    static DebugHotpatcher& instance() {
        static DebugHotpatcher inst;
        return inst;
    }

    void setEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_enabled = enabled;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enabled;
    }

    PatchResult installPolicySafeKillRules(ProxyHotpatcher& proxy) {
        if (!isEnabled()) {
            return PatchResult::ok("debug hotpatcher: disabled");
        }

        // Clear existing rules by stable names (idempotent install).
        (void)proxy.remove_rewrite_rule("policy_stop_process_force_to_terminal_kill");
        (void)proxy.remove_rewrite_rule("policy_stop_process_id_to_terminal_kill");
        (void)proxy.remove_rewrite_rule("policy_kill_sigkill_to_terminal_kill");
        (void)proxy.remove_rewrite_rule("policy_pkill_to_terminal_kill");
        (void)proxy.remove_rewrite_rule("policy_stop_process_force_to_debug_stop");
        (void)proxy.remove_rewrite_rule("policy_taskkill_force_to_debug_stop");
        (void)proxy.remove_rewrite_rule("policy_denied_to_debug_stop_hint");

        OutputRewriteRule rr1 = {};
        rr1.name = "policy_stop_process_force_to_terminal_kill";
        rr1.pattern = "Stop-Process -Force";
        rr1.replacement = "!terminal_kill";
        rr1.enabled = true;
        proxy.add_rewrite_rule(rr1);

        OutputRewriteRule rr2 = {};
        rr2.name = "policy_stop_process_id_to_terminal_kill";
        rr2.pattern = "Stop-Process -Id";
        rr2.replacement = "!terminal_kill";
        rr2.enabled = true;
        proxy.add_rewrite_rule(rr2);

        OutputRewriteRule rr3 = {};
        rr3.name = "policy_kill_sigkill_to_terminal_kill";
        rr3.pattern = "kill -9";
        rr3.replacement = "!terminal_kill";
        rr3.enabled = true;
        proxy.add_rewrite_rule(rr3);

        OutputRewriteRule rr4 = {};
        rr4.name = "policy_pkill_to_terminal_kill";
        rr4.pattern = "pkill";
        rr4.replacement = "!terminal_kill";
        rr4.enabled = true;
        proxy.add_rewrite_rule(rr4);

        OutputRewriteRule rr5 = {};
        rr5.name = "policy_stop_process_force_to_debug_stop";
        rr5.pattern = "Stop-Process -Force -ErrorAction SilentlyContinue";
        rr5.replacement = "!debug_stop";
        rr5.enabled = true;
        proxy.add_rewrite_rule(rr5);

        OutputRewriteRule rr6 = {};
        rr6.name = "policy_taskkill_force_to_debug_stop";
        rr6.pattern = "taskkill /F /PID";
        rr6.replacement = "!debug_stop";
        rr6.enabled = true;
        proxy.add_rewrite_rule(rr6);

        OutputRewriteRule rr7 = {};
        rr7.name = "policy_denied_to_debug_stop_hint";
        rr7.pattern = "POLICY_DENIED: Command was not executed";
        rr7.replacement = "Use !debug_stop (debug target) or !terminal_kill (terminal process)";
        rr7.enabled = true;
        proxy.add_rewrite_rule(rr7);

        return PatchResult::ok("debug hotpatcher: policy-safe rules installed");
    }

    PatchResult applyDebugAddressPatch(uintptr_t address,
                                       const std::vector<uint8_t>& bytes,
                                       const char* label = "debug_addr_patch") {
        if (!isEnabled()) {
            return PatchResult::error("debug hotpatcher: disabled", 1);
        }
        if (bytes.empty()) {
            return PatchResult::error("debug hotpatcher: bytes empty", 2);
        }
        return AddressHotpatcher::instance().applyPatch(address, bytes.data(), bytes.size(), label);
    }

private:
    DebugHotpatcher() = default;
    ~DebugHotpatcher() = default;
    DebugHotpatcher(const DebugHotpatcher&) = delete;
    DebugHotpatcher& operator=(const DebugHotpatcher&) = delete;

    mutable std::mutex m_mutex;
    bool m_enabled = true;
};
