#pragma once

#ifndef RAWRXD_PATCHRESULT_DEFINED
#define RAWRXD_PATCHRESULT_DEFINED

#include <string>
#include <cstdint>

struct PatchResult {
    bool        success = false;
    std::string detail;
    int         errorCode = 0;
    int64_t     elapsedMs = 0;

    static PatchResult ok(const std::string& msg = "Success", int64_t elapsed = 0) {
        PatchResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        r.elapsedMs = elapsed;
        return r;
    }

    static PatchResult error(const std::string& msg, int code = -1, int64_t elapsed = 0) {
        PatchResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        r.elapsedMs = elapsed;
        return r;
    }

    static PatchResult error(int code, const std::string& msg, int64_t elapsed = 0, int osError = 0) {
        PatchResult r;
        r.success   = false;
        r.errorCode = (osError != 0) ? osError : code;
        r.detail    = msg;
        r.elapsedMs = elapsed;
        return r;
    }
};

#endif // RAWRXD_PATCHRESULT_DEFINED
