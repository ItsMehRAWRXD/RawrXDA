#pragma once
// SecurityHotpatchBridge — Connects RBAC permission checks to HotPatcher
// Uses actual RawrXD::Auth::RBACEngine API (checkPermission, logAudit, authorize)
// Uses actual HotPatcher API (ApplyPatch with string+void*+vector<unsigned char>)
// No Qt. No exceptions. C++20 only.

#include "auth/rbac_engine.hpp"
#include "hot_patcher.h"
#include "WindowsDefenderBridge.h"
#include "RawrXD_SignalSlot.h"
#include <string>
#include <vector>

namespace RawrXD {

class SecurityHotpatchBridge {
    Auth::RBACEngine& rbac_;
    HotPatcher& patcher_;

    Signal<const std::string&, bool> onPatchAuthorized;
    Signal<const DefenderScanResult&> onDefenderAlert;

public:
    SecurityHotpatchBridge()
        : rbac_(Auth::RBACEngine::instance()), patcher_(*getDefaultPatcher()) {
        InitDefender();
    }

    SecurityHotpatchBridge(Auth::RBACEngine& rbac, HotPatcher& patcher)
        : rbac_(rbac), patcher_(patcher) {
        InitDefender();
    }

    // Request a patch with RBAC authorization
    bool RequestPatch(const std::string& patchName,
                      void* targetAddress,
                      const std::vector<unsigned char>& newOpcodes,
                      const char* sessionToken) {
        // Check HOTPATCH_MEMORY_WRITE permission
        Auth::AuthResult authRes = rbac_.authorize(
            sessionToken,
            Auth::Permission::HOTPATCH_MEMORY_WRITE,
            "hotpatch.apply",
            patchName.c_str());

        if (!authRes.success) {
            onPatchAuthorized.emit(patchName, false);
            return false;
        }

        // Windows Defender AMSI scan before applying
        auto& defender = GetDefenderBridge();
        std::wstring wPatchName(patchName.begin(), patchName.end());
        auto scanResult = defender.ScanOpcodes(newOpcodes, wPatchName);
        if (scanResult.wasBlocked) {
            rbac_.logAudit("", "hotpatch.defender_block", patchName,
                           "Blocked by Windows Defender: " + std::string(VerdictToString(scanResult.verdict)),
                           Auth::AuthResult::error("defender_blocked"));
            onDefenderAlert.emit(scanResult);
            onPatchAuthorized.emit(patchName, false);
            return false;
        }

        bool ok = patcher_.ApplyPatch(patchName, targetAddress, newOpcodes);
        onPatchAuthorized.emit(patchName, ok);

        // Log result
        rbac_.logAudit(
            "",  // userId resolved from session internally
            "hotpatch.apply",
            patchName,
            ok ? "Patch applied successfully" : "Patch application failed",
            ok ? Auth::AuthResult::ok("applied") : Auth::AuthResult::error("failed"));

        return ok;
    }

    // Revert a patch with RBAC authorization
    bool RequestRevert(const std::string& patchName,
                       const char* sessionToken) {
        Auth::AuthResult authRes = rbac_.authorize(
            sessionToken,
            Auth::Permission::HOTPATCH_MEMORY_REVERT,
            "hotpatch.revert",
            patchName.c_str());

        if (!authRes.success) {
            onPatchAuthorized.emit(patchName, false);
            return false;
        }

        bool ok = patcher_.RevertPatch(patchName);
        onPatchAuthorized.emit(patchName, ok);

        rbac_.logAudit("", "hotpatch.revert", patchName,
                       ok ? "Reverted" : "Revert failed",
                       ok ? Auth::AuthResult::ok("reverted") : Auth::AuthResult::error("failed"));
        return ok;
    }

    // Scan-and-patch with authorization
    bool RequestScanPatch(const std::string& patchName,
                          const std::vector<unsigned char>& signature,
                          const std::vector<unsigned char>& replacement,
                          const char* sessionToken) {
        Auth::AuthResult authRes = rbac_.authorize(
            sessionToken,
            Auth::Permission::HOTPATCH_LIVE_BINARY,
            "hotpatch.scan_patch",
            patchName.c_str());

        if (!authRes.success) return false;

        // Defender scan the replacement bytes
        auto& defender = GetDefenderBridge();
        std::wstring wName(patchName.begin(), patchName.end());
        auto scanResult = defender.ScanOpcodes(replacement, wName + L"_replacement");
        if (scanResult.wasBlocked) {
            rbac_.logAudit("", "hotpatch.defender_block", patchName,
                           "ScanPatch blocked by Windows Defender",
                           Auth::AuthResult::error("defender_blocked"));
            onDefenderAlert.emit(scanResult);
            return false;
        }

        return patcher_.ScanAndPatch(patchName, signature, replacement);
    }

    auto& AuthorizedSignal() { return onPatchAuthorized; }
    auto& DefenderAlertSignal() { return onDefenderAlert; }
    HotPatcher& GetPatcher() { return patcher_; }
    WindowsDefenderBridge& GetDefender() { return GetDefenderBridge(); }

private:
    void InitDefender() {
        auto& defender = GetDefenderBridge();
        if (!defender.IsInitialized()) {
            defender.Initialize();
        }
        // Wire Defender threat alerts to our signal
        defender.onThreatDetected.connect([this](const DefenderScanResult& r) {
            onDefenderAlert.emit(r);
        });
    }

    static HotPatcher* getDefaultPatcher() {
        static HotPatcher instance;
        return &instance;
    }
};

} // namespace RawrXD
