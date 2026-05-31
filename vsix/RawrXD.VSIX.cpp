// =============================================================================
// RawrXD.VSIX.cpp — Native VSIX Package Entry Point (No Electron)
// =============================================================================
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <memory>

// GUIDs
// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
static const GUID CLSID_RawrXDPackage =
    { 0xA1B2C3D4, 0xE5F6, 0x7890, { 0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90 } };

// {D1E2F3A4-B5C6-7890-DEF1-234567890123}
static const GUID GUID_RawrXDEditor =
    { 0xD1E2F3A4, 0xB5C6, 0x7890, { 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23 } };

// {B1C2D3E4-F5A6-7890-BCDE-F12345678901}
static const GUID GUID_RawrXDMenu =
    { 0xB1C2D3E4, 0xF5A6, 0x7890, { 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89, 0x01 } };

// =============================================================================
// DLL EXPORTS
// =============================================================================

extern "C" {

__declspec(dllexport) HRESULT DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    if (rclsid == CLSID_RawrXDPackage) {
        // Return a simple class factory
        struct RawrXDClassFactory : public IClassFactory {
            STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
                if (riid == IID_IUnknown || riid == IID_IClassFactory) {
                    *ppv = this;
                    AddRef();
                    return S_OK;
                }
                *ppv = nullptr;
                return E_NOINTERFACE;
            }
            STDMETHODIMP_(ULONG) AddRef() override { return 2; }
            STDMETHODIMP_(ULONG) Release() override { return 1; }
            STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
                if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;
                // Return a minimal package object
                *ppv = nullptr;
                return E_NOTIMPL;
            }
            STDMETHODIMP LockServer(BOOL fLock) override { return S_OK; }
        };
        static RawrXDClassFactory factory;
        return factory.QueryInterface(riid, ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

__declspec(dllexport) HRESULT DllCanUnloadNow() {
    return S_OK;
}

__declspec(dllexport) HRESULT DllRegisterServer() {
    HKEY hKey;
    wchar_t szModule[MAX_PATH];
    GetModuleFileNameW(GetModuleHandleW(L"RawrXD.VSIX.dll"), szModule, MAX_PATH);

    // Register CLSID
    std::wstring clsidPath = L"CLSID\\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(L"RawrXD Package"), 30 * sizeof(wchar_t));
        HKEY hInproc;
        if (RegCreateKeyExW(hKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &hInproc, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hInproc, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(szModule), static_cast<DWORD>((wcslen(szModule) + 1) * sizeof(wchar_t)));
            RegSetValueExW(hInproc, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Apartment"), 20);
            RegCloseKey(hInproc);
        }
        RegCloseKey(hKey);
    }
    return S_OK;
}

__declspec(dllexport) HRESULT DllUnregisterServer() {
    std::wstring clsidPath = L"CLSID\\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}";
    SHDeleteKeyW(HKEY_CLASSES_ROOT, clsidPath.c_str());
    return S_OK;
}

} // extern "C"

// =============================================================================
// VSIX PACKAGE CLASS
// =============================================================================

class RawrXDPackage : public IUnknown {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) delete this;
        return ref;
    }

    RawrXDPackage() : m_refCount(1) {}

private:
    ULONG m_refCount = 1;
};

// =============================================================================
// DLL MAIN
// =============================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
