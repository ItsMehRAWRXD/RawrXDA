// masm_auth_stub.cpp - Configurable MASM auth shim with basic diagnostics
#include <windows.h>
#include <mutex>
#include <string>

namespace {

bool isAuthAllowed() {
    static std::once_flag flag;
    static bool allow = true;

    std::call_once(flag, []() {
        char* env = nullptr;
        size_t len = 0;
        if (_dupenv_s(&env, &len, "RAWRXD_AUTH_ALLOW") == 0 && env) {
            std::string value(env);
            _strlwr_s(env, len);
            allow = !(value == "0" || value == "false" || value == "no");
            free(env);
        }
    });

    return allow;
}

void logAuthDecision(const void* ctx, const char* resource, bool allowed) {
    char buffer[256] = {0};
    _snprintf_s(buffer, sizeof(buffer), _TRUNCATE,
                "[masm_auth_stub] ctx=%p resource=%s allowed=%s\n",
                ctx, resource ? resource : "<null>", allowed ? "true" : "false");
    OutputDebugStringA(buffer);
}

} // namespace

extern "C" {

// MASM-facing auth entrypoint; configurable via RAWRXD_AUTH_ALLOW
BOOL auth_authorize(void* ctx, const char* resource) {
    const bool allowed = isAuthAllowed();
    logAuthDecision(ctx, resource, allowed);
    return allowed ? TRUE : FALSE;
}

} // extern "C"
