#include "address_hotpatcher.hpp"

#include <array>
#include <sstream>

#if defined(_WIN32) && defined(_M_X64)
extern "C" {
int rawrxd_addr_patch_apply(void* address, const void* patchBytes, size_t patchLen, const char* label);
int rawrxd_addr_patch_revert(void* address);
int rawrxd_addr_patch_revert_all(void);
int rawrxd_addr_patch_is_active(void* address);
int rawrxd_addr_patch_dump(char* outBuffer, size_t outBufferLen);
void rawrxd_addr_patch_diag_get(uint64_t* applied, uint64_t* reverted, uint64_t* failed);
void rawrxd_addr_patch_diag_reset(void);
uint64_t rawrxd_addr_patch_active_count(void);
}
#endif

namespace {

static const char* statusToMessage(int code) {
    switch (code) {
    case RAWRXD_ADDR_PATCH_OK:
        return "address hotpatch: ok";
    case RAWRXD_ADDR_PATCH_INVALID_ARGS:
        return "address hotpatch: invalid arguments";
    case RAWRXD_ADDR_PATCH_ALREADY_ACTIVE:
        return "address hotpatch: address already patched";
    case RAWRXD_ADDR_PATCH_NOT_ACTIVE:
        return "address hotpatch: patch not active";
    case RAWRXD_ADDR_PATCH_NO_FREE_SLOT:
        return "address hotpatch: no free record slot";
    case RAWRXD_ADDR_PATCH_PROTECT_FAILED:
        return "address hotpatch: memory protection failed";
    case RAWRXD_ADDR_PATCH_TOO_LARGE:
        return "address hotpatch: patch exceeds max bytes";
    case RAWRXD_ADDR_PATCH_DUMP_FAILED:
        return "address hotpatch: dump failed";
    default:
        return "address hotpatch: unknown error";
    }
}

static PatchResult makeStatusResult(int code, const char* okMsg) {
    if (code == RAWRXD_ADDR_PATCH_OK) {
        return PatchResult::ok(okMsg);
    }
    return PatchResult::error(statusToMessage(code), code);
}

} // namespace

AddressHotpatcher& AddressHotpatcher::instance() {
    static AddressHotpatcher inst;
    return inst;
}

PatchResult AddressHotpatcher::applyPatch(uintptr_t address,
                                          const uint8_t* bytes,
                                          size_t len,
                                          const char* label) {
    if (!address || !bytes || len == 0) {
        return PatchResult::error("address hotpatch: invalid arguments", RAWRXD_ADDR_PATCH_INVALID_ARGS);
    }
    if (len > RAWRXD_ADDR_PATCH_MAX_BYTES) {
        return PatchResult::error("address hotpatch: patch exceeds max bytes", RAWRXD_ADDR_PATCH_TOO_LARGE);
    }

#if defined(_WIN32) && defined(_M_X64)
    const int code = rawrxd_addr_patch_apply(reinterpret_cast<void*>(address), bytes, len, label);
    return makeStatusResult(code, "address hotpatch: applied");
#else
    (void)label;
    return PatchResult::error("address hotpatch: MASM backend unavailable", RAWRXD_ADDR_PATCH_PROTECT_FAILED);
#endif
}

PatchResult AddressHotpatcher::revertPatch(uintptr_t address) {
    if (!address) {
        return PatchResult::error("address hotpatch: invalid arguments", RAWRXD_ADDR_PATCH_INVALID_ARGS);
    }

#if defined(_WIN32) && defined(_M_X64)
    const int code = rawrxd_addr_patch_revert(reinterpret_cast<void*>(address));
    return makeStatusResult(code, "address hotpatch: reverted");
#else
    return PatchResult::error("address hotpatch: MASM backend unavailable", RAWRXD_ADDR_PATCH_PROTECT_FAILED);
#endif
}

PatchResult AddressHotpatcher::revertAll() {
#if defined(_WIN32) && defined(_M_X64)
    const int code = rawrxd_addr_patch_revert_all();
    return makeStatusResult(code, "address hotpatch: reverted all");
#else
    return PatchResult::error("address hotpatch: MASM backend unavailable", RAWRXD_ADDR_PATCH_PROTECT_FAILED);
#endif
}

bool AddressHotpatcher::isPatched(uintptr_t address) const {
    if (!address) {
        return false;
    }

#if defined(_WIN32) && defined(_M_X64)
    return rawrxd_addr_patch_is_active(reinterpret_cast<void*>(address)) != 0;
#else
    return false;
#endif
}

std::string AddressHotpatcher::debugDump() const {
#if defined(_WIN32) && defined(_M_X64)
    std::array<char, 8192> buffer{};
    const int code = rawrxd_addr_patch_dump(buffer.data(), buffer.size());
    if (code != RAWRXD_ADDR_PATCH_OK) {
        std::ostringstream oss;
        oss << "AddressHotpatcher dump error: " << statusToMessage(code)
            << " (code=" << code << ")\n";
        return oss.str();
    }
    return std::string(buffer.data());
#else
    return "AddressHotpatcher unavailable: MASM backend not built\n";
#endif
}

AddressPatchDiagnostics AddressHotpatcher::getDiagnostics() const {
    AddressPatchDiagnostics diagnostics{};
#if defined(_WIN32) && defined(_M_X64)
    rawrxd_addr_patch_diag_get(&diagnostics.applied, &diagnostics.reverted, &diagnostics.failed);
    diagnostics.active = rawrxd_addr_patch_active_count();
#endif
    return diagnostics;
}

void AddressHotpatcher::resetDiagnostics() {
#if defined(_WIN32) && defined(_M_X64)
    rawrxd_addr_patch_diag_reset();
#endif
}
