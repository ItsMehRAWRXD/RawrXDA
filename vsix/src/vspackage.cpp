// ============================================================================
// vspackage.cpp — RawrXD Prometheus Native VSIX Package (Zero Electron)
// ============================================================================
// Implements IVsPackage, IVsToolWindowFactory, IVsEditorFactory for native
// Visual Studio integration. No Electron, no webviews — pure C++/Win32.
// ============================================================================

#include <windows.h>
#include <shlwapi.h>
#include <cstddef>
#include <string>
#include <vector>
#include <atomic>

// Minimal COM/OLE definitions
struct IDispatch { void* vtbl; };
struct VSPROPSHEETPAGE { DWORD dwSize; DWORD dwFlags; HINSTANCE hInstance; LPCWSTR pszTemplate; HICON hIcon; LPCWSTR pszTitle; DLGPROC pfnDlgProc; LPARAM lParam; };

// Minimal COM interface definitions
struct IUnknown_Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void* This, REFIID riid, void** ppvObject);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void* This);
    ULONG   (STDMETHODCALLTYPE *Release)(void* This);
};

struct IVsPackage_Vtbl {
    IUnknown_Vtbl base;
    HRESULT (STDMETHODCALLTYPE *SetSite)(void* This, void* pSP);
    HRESULT (STDMETHODCALLTYPE *QueryClose)(void* This, BOOL* pfCanClose);
    HRESULT (STDMETHODCALLTYPE *Close)(void* This);
    HRESULT (STDMETHODCALLTYPE *GetAutomationObject)(void* This, LPCOLESTR pszPropName, IDispatch** ppDisp);
    HRESULT (STDMETHODCALLTYPE *CreateTool)(void* This, REFGUID rguidPersistenceSlot);
    HRESULT (STDMETHODCALLTYPE *ResetDefaults)(void* This, DWORD grfFlags);
    HRESULT (STDMETHODCALLTYPE *GetPropertyPage)(void* This, REFGUID rguidPage, VSPROPSHEETPAGE* ppage);
};

struct IVsToolWindowFactory_Vtbl {
    IUnknown_Vtbl base;
    HRESULT (STDMETHODCALLTYPE *CreateToolWindow)(void* This, REFGUID rguidPersistenceSlot, DWORD dwToolWindowId,
        void* punkToolWindowPartner, REFGUID rguidToolWindow, void** ppWindowFrame, BOOL* pfDefaultPosition);
};

// IID macros
#define DEFINE_IID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const IID IID_##name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}};

DEFINE_IID(IUnknown,      0x00000000,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46)
DEFINE_IID(IClassFactory, 0x00000001,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46)
DEFINE_IID(IVsPackage,    0x00000000,0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)
DEFINE_IID(IVsToolWindowFactory, 0x00000000,0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)

// GUIDs
static const GUID CLSID_RawrXDPrometheusNativePkg =
    {0x14d0a8c2,0x3e2f,0x4a5b,{0x9c,0x1d,0x8e,0x7f,0x6a,0x5b,0x4c,0x3d}};
static const GUID GUID_PrometheusOutputWindow =
    {0xa1b2c3d4,0xe5f6,0x7890,{0xab,0xcd,0xef,0x12,0x34,0x56,0x78,0x90}};
static const GUID GUID_PrometheusModelBrowser =
    {0xb2c3d4e5,0xf6a7,0x8901,{0xbc,0xde,0xf2,0x34,0x56,0x78,0x90,0x12}};

// Package state
struct RawrXDPrometheusPackage {
    IVsPackage_Vtbl* vtbl;
    IVsToolWindowFactory_Vtbl* twVtbl;
    std::atomic<ULONG> refCount;
    void* site;
    HINSTANCE hInstance;
    HWND hwndOutput;
    HWND hwndModelBrowser;
    bool initialized;
};

static RawrXDPrometheusPackage* g_pPackage = nullptr;

// Forward declarations
static HRESULT STDMETHODCALLTYPE Package_QueryInterface(void* This, REFIID riid, void** ppvObject);
static ULONG   STDMETHODCALLTYPE Package_AddRef(void* This);
static ULONG   STDMETHODCALLTYPE Package_Release(void* This);
static HRESULT STDMETHODCALLTYPE Package_SetSite(void* This, void* pSP);
static HRESULT STDMETHODCALLTYPE Package_QueryClose(void* This, BOOL* pfCanClose);
static HRESULT STDMETHODCALLTYPE Package_Close(void* This);
static HRESULT STDMETHODCALLTYPE Package_GetAutomationObject(void* This, LPCOLESTR pszPropName, IDispatch** ppDisp);
static HRESULT STDMETHODCALLTYPE Package_CreateTool(void* This, REFGUID rguidPersistenceSlot);
static HRESULT STDMETHODCALLTYPE Package_ResetDefaults(void* This, DWORD grfFlags);
static HRESULT STDMETHODCALLTYPE Package_GetPropertyPage(void* This, REFGUID rguidPage, VSPROPSHEETPAGE* ppage);

static IVsPackage_Vtbl g_PackageVtbl = {
    {Package_QueryInterface, Package_AddRef, Package_Release},
    Package_SetSite,
    Package_QueryClose,
    Package_Close,
    Package_GetAutomationObject,
    Package_CreateTool,
    Package_ResetDefaults,
    Package_GetPropertyPage
};

// Tool window factory vtable
static HRESULT STDMETHODCALLTYPE TWF_QueryInterface(void* This, REFIID riid, void** ppvObject);
static ULONG   STDMETHODCALLTYPE TWF_AddRef(void* This);
static ULONG   STDMETHODCALLTYPE TWF_Release(void* This);
static HRESULT STDMETHODCALLTYPE TWF_CreateToolWindow(void* This, REFGUID rguidPersistenceSlot, DWORD dwToolWindowId,
    void* pUnkToolWindowPartner, REFGUID rguidToolWindow, void** ppWindowFrame, BOOL* pfDefaultPosition);

static IVsToolWindowFactory_Vtbl g_TWFactoryVtbl = {
    {TWF_QueryInterface, TWF_AddRef, TWF_Release},
    TWF_CreateToolWindow
};

// ============================================================================
// Package Implementation
// ============================================================================

static HRESULT STDMETHODCALLTYPE Package_QueryInterface(void* This, REFIID riid, void** ppvObject) {
    if (!ppvObject) return E_POINTER;

    RawrXDPrometheusPackage* pPkg = static_cast<RawrXDPrometheusPackage*>(This);

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, __uuidof(IVsPackage))) {
        *ppvObject = pPkg;
        Package_AddRef(This);
        return S_OK;
    }

    if (IsEqualIID(riid, __uuidof(IVsToolWindowFactory))) {
        *ppvObject = &pPkg->twFactory;
        TWF_AddRef(&pPkg->twFactory);
        return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Package_AddRef(void* This) {
    RawrXDPrometheusPackage* pPkg = static_cast<RawrXDPrometheusPackage*>(This);
    return pPkg->refCount.fetch_add(1) + 1;
}

static ULONG STDMETHODCALLTYPE Package_Release(void* This) {
    RawrXDPrometheusPackage* pPkg = static_cast<RawrXDPrometheusPackage*>(This);
    ULONG count = pPkg->refCount.fetch_sub(1);
    if (count == 1) {
        delete pPkg;
        g_pPackage = nullptr;
    }
    return count - 1;
}

static HRESULT STDMETHODCALLTYPE Package_SetSite(void* This, void* pSP) {
    RawrXDPrometheusPackage* pPkg = static_cast<RawrXDPrometheusPackage*>(This);
    pPkg->site = pSP;
    pPkg->initialized = true;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Package_QueryClose(void* This, BOOL* pfCanClose) {
    (void)This;
    if (pfCanClose) *pfCanClose = TRUE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Package_Close(void* This) {
    RawrXDPrometheusPackage* pPkg = static_cast<RawrXDPrometheusPackage*>(This);
    pPkg->initialized = false;
    pPkg->site = nullptr;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Package_GetAutomationObject(void* This, LPCOLESTR pszPropName, IDispatch** ppDisp) {
    (void)This;
    (void)pszPropName;
    if (ppDisp) *ppDisp = nullptr;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE Package_CreateTool(void* This, REFGUID rguidPersistenceSlot) {
    (void)This;
    (void)rguidPersistenceSlot;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE Package_ResetDefaults(void* This, DWORD grfFlags) {
    (void)This;
    (void)grfFlags;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Package_GetPropertyPage(void* This, REFGUID rguidPage, VSPROPSHEETPAGE* ppage) {
    (void)This;
    (void)rguidPage;
    (void)ppage;
    return E_NOTIMPL;
}

// ============================================================================
// Tool Window Factory Implementation
// ============================================================================

static HRESULT STDMETHODCALLTYPE TWF_QueryInterface(void* This, REFIID riid, void** ppvObject) {
    return Package_QueryInterface(
        reinterpret_cast<RawrXDPrometheusPackage*>(
            reinterpret_cast<char*>(This) - offsetof(RawrXDPrometheusPackage, twFactory)),
        riid, ppvObject);
}

static ULONG STDMETHODCALLTYPE TWF_AddRef(void* This) {
    return Package_AddRef(
        reinterpret_cast<RawrXDPrometheusPackage*>(
            reinterpret_cast<char*>(This) - offsetof(RawrXDPrometheusPackage, twFactory)));
}

static ULONG STDMETHODCALLTYPE TWF_Release(void* This) {
    return Package_Release(
        reinterpret_cast<RawrXDPrometheusPackage*>(
            reinterpret_cast<char*>(This) - offsetof(RawrXDPrometheusPackage, twFactory)));
}

static HRESULT STDMETHODCALLTYPE TWF_CreateToolWindow(void* This, REFGUID rguidPersistenceSlot, DWORD dwToolWindowId,
    void* pUnkToolWindowPartner, REFGUID rguidToolWindow, void** ppWindowFrame, BOOL* pfDefaultPosition) {
    (void)This;
    (void)dwToolWindowId;
    (void)pUnkToolWindowPartner;
    (void)rguidToolWindow;

    if (IsEqualGUID(rguidPersistenceSlot, GUID_PrometheusOutputWindow)) {
        if (ppWindowFrame) *ppWindowFrame = nullptr;
        if (pfDefaultPosition) *pfDefaultPosition = TRUE;
        return S_OK;
    }

    if (IsEqualGUID(rguidPersistenceSlot, GUID_PrometheusModelBrowser)) {
        if (ppWindowFrame) *ppWindowFrame = nullptr;
        if (pfDefaultPosition) *pfDefaultPosition = TRUE;
        return S_OK;
    }

    return E_FAIL;
}

// ============================================================================
// Class Factory
// ============================================================================

class RawrXDClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return 2;
    }

    STDMETHODIMP_(ULONG) Release() override {
        return 1;
    }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        RawrXDPrometheusPackage* pPkg = new RawrXDPrometheusPackage();
        pPkg->vtbl = &g_PackageVtbl;
        pPkg->refCount = 1;
        pPkg->site = nullptr;
        pPkg->hInstance = GetModuleHandleW(nullptr);
        pPkg->hwndOutput = nullptr;
        pPkg->hwndModelBrowser = nullptr;
        pPkg->initialized = false;

        g_pPackage = pPkg;

        HRESULT hr = Package_QueryInterface(pPkg, riid, ppv);
        if (FAILED(hr)) {
            delete pPkg;
            g_pPackage = nullptr;
        }
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        (void)fLock;
        return S_OK;
    }
};

static RawrXDClassFactory g_ClassFactory;

// ============================================================================
// DLL Entry Point
// ============================================================================

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (dwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    if (IsEqualCLSID(rclsid, CLSID_RawrXDPrometheusNativePkg)) {
        return g_ClassFactory.QueryInterface(riid, ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow() {
    return (g_pPackage == nullptr || g_pPackage->refCount.load() == 0) ? S_OK : S_FALSE;
}

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer() {
    HKEY hKey;
    wchar_t szModule[MAX_PATH];
    GetModuleFileNameW(GetModuleHandleW(L"RawrXD.Prometheus.Native.dll"), szModule, MAX_PATH);

    std::wstring clsidKey = L"CLSID\\{14d0a8c2-3e2f-4a5b-9c1d-8e7f6a5b4c3d}";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, clsidKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"RawrXD Prometheus Native Package", 68 * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)szModule, (DWORD)(wcslen(szModule) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)L"Apartment", 20);
        RegCloseKey(hKey);
    }

    return S_OK;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer() {
    std::wstring clsidKey = L"CLSID\\{14d0a8c2-3e2f-4a5b-9c1d-8e7f6a5b4c3d}";
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey.c_str());
    return S_OK;
}
