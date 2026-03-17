#pragma once
#include "auth/rbac_engine.hpp"
#include "hot_patcher.h"
#include "RawrXD_SignalSlot.h"

namespace RawrXD {
class SecurityHotpatchBridge {
    RBAC::RBACEngine* rbac;
    HotPatcher* patcher;
    Signal<const std::wstring&, bool> onPatchAuthorized;
public:
    SecurityHotpatchBridge(RBAC::RBACEngine* r, HotPatcher* p) : rbac(r), patcher(p) {}
    bool RequestPatch(const std::wstring& patchId, const std::vector<uint8_t>& code, const std::wstring& userToken) {
        if(!rbac->CheckPermission(userToken, L"hotpatch:apply")) {
            onPatchAuthorized.emit(patchId, false);
            return false;
        }
        auto hash = rbac->ComputeAuditHash(code);
        rbac->LogAuditEvent(userToken, L"PATCH_APPLY", patchId, hash);
        bool ok = patcher->ApplyPatch(patchId, code);
        onPatchAuthorized.emit(patchId, ok);
        return ok;
    }
    bool RequestRevert(const std::wstring& patchId, const std::wstring& userToken) {
        if(!rbac->CheckPermission(userToken, L"hotpatch:revert")) return false;
        rbac->LogAuditEvent(userToken, L"PATCH_REVERT", patchId, L"");
        return patcher->RevertPatch(patchId);
    }
};
} // namespace RawrXD
