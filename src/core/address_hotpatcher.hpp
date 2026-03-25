#pragma once

#include "patch_result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr size_t RAWRXD_ADDR_PATCH_MAX_BYTES = 64;
constexpr size_t RAWRXD_ADDR_PATCH_MAX_RECORDS = 64;

enum AddressPatchStatusCode : int {
    RAWRXD_ADDR_PATCH_OK = 0,
    RAWRXD_ADDR_PATCH_INVALID_ARGS = 1,
    RAWRXD_ADDR_PATCH_ALREADY_ACTIVE = 2,
    RAWRXD_ADDR_PATCH_NOT_ACTIVE = 3,
    RAWRXD_ADDR_PATCH_NO_FREE_SLOT = 4,
    RAWRXD_ADDR_PATCH_PROTECT_FAILED = 5,
    RAWRXD_ADDR_PATCH_TOO_LARGE = 6,
    RAWRXD_ADDR_PATCH_DUMP_FAILED = 7,
};

struct AddressPatchDiagnostics {
    uint64_t applied = 0;
    uint64_t reverted = 0;
    uint64_t failed = 0;
    uint64_t active = 0;
};

struct AddressPatchRecord {
    uintptr_t address = 0;
    std::vector<uint8_t> originalBytes;
    std::vector<uint8_t> patchedBytes;
    std::string label;
    bool active = false;
    uint64_t sequence = 0;
};

class AddressHotpatcher {
public:
    static AddressHotpatcher& instance();

    PatchResult applyPatch(uintptr_t address,
                           const uint8_t* bytes,
                           size_t len,
                           const char* label = nullptr);
    PatchResult revertPatch(uintptr_t address);
    PatchResult revertAll();
    bool isPatched(uintptr_t address) const;
    std::string debugDump() const;
    AddressPatchDiagnostics getDiagnostics() const;
    void resetDiagnostics();

private:
    AddressHotpatcher() = default;
    ~AddressHotpatcher() = default;
    AddressHotpatcher(const AddressHotpatcher&) = delete;
    AddressHotpatcher& operator=(const AddressHotpatcher&) = delete;
};
