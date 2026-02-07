#include "telemetry.h"
#if defined(_WIN32) && defined(__has_include)
#  if __has_include(<windows.h>)
#    include <windows.h>
#    define TELEMETRY_HAS_WINDOWS 1
#  endif
#  if __has_include(<pdh.h>)
#    include <pdh.h>
#    define TELEMETRY_HAS_PDH 1
#  endif
#  if __has_include(<wbemidl.h>)
#    include <wbemidl.h>
#    define TELEMETRY_HAS_WMI 1
#  endif
#endif
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <string>
#include <mutex>
#include <vector>
#include <sstream>

#ifdef TELEMETRY_HAS_WMI
#pragma comment(lib, "wbemuuid.lib")
#endif
#ifdef TELEMETRY_HAS_PDH
#pragma comment(lib, "pdh.lib")
#endif

namespace telemetry {

static IWbemLocator *g_pLocator = nullptr;
static IWbemServices *g_pServices = nullptr;
static PDH_HQUERY g_cpuQuery = nullptr;
static PDH_HCOUNTER g_cpuCounter = nullptr;
static bool g_vendorNvidia = false;
static bool g_vendorAmd = false;
static std::string g_gpuVendor;
static std::mutex g_lock;
static bool g_initialized = false;
static uint64_t g_startMs = 0;

static uint64_t NowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool DetectExecutable(const std::vector<std::string> &names, std::string &foundPath) {
    for (const auto &n : names) {
        // Check system32 and PATH search - gracefully handle missing Win32 APIs
        std::string systemDirs[] = {"C:/Windows/System32"};
    #ifdef TELEMETRY_HAS_WINDOWS
        char sysDir[512];
        UINT len = GetSystemDirectoryA(sysDir, sizeof(sysDir));
        if (len && len < sizeof(sysDir)) systemDirs[0] = std::string(sysDir);
    #endif
        for (const auto& sd : systemDirs) {
            std::filesystem::path p = std::filesystem::path(sd) / n;
            if (std::filesystem::exists(p)) { foundPath = p.string(); return true; }
        }
        // PATH search
        char *envPath = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&envPath, &sz, "PATH") == 0 && envPath) {
            std::stringstream ss(envPath);
            std::string segment;
            while (std::getline(ss, segment, ';')) {
                if (segment.empty()) continue;
                std::filesystem::path p = std::filesystem::path(segment) / n;
                if (std::filesystem::exists(p)) { free(envPath); foundPath = p.string(); return true; }
            }
            free(envPath);
        }
    }
    return false;
}

static std::string RunAndCapture(const std::string &cmd) {
    std::string result;
    FILE *pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result.append(buffer);
    }
    _pclose(pipe);
    return result;
}

static double ParseFirstNumber(const std::string &text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if ((text[i] >= '0' && text[i] <= '9') || (text[i] == '-' && i + 1 < text.size() && isdigit(text[i+1]))) {
            size_t j = i;
            while (j < text.size() && (isdigit(text[j]) || text[j] == '.')) j++;
            try { return std::stod(text.substr(i, j - i)); } catch(...) { return -1.0; }
        }
    }
    return -1.0;
}

bool Initialize() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_initialized) return true;

    g_startMs = NowMs();

    // COM init - only if WMI available
    HRESULT hr = E_FAIL;
#ifdef TELEMETRY_HAS_WMI
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
#endif
    if (FAILED(hr)) {
        // still proceed without WMI
    }
    hr = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);
    // Ignore failure - may already be initialized elsewhere.

#ifdef TELEMETRY_HAS_WMI
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&g_pLocator);
    if (SUCCEEDED(hr) && g_pLocator) {
        hr = g_pLocator->ConnectServer(BSTR(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &g_pServices);
        if (SUCCEEDED(hr) && g_pServices) {
            // Set proxy blanket
            CoSetProxyBlanket(g_pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            }
    }
#endif

    // PDH CPU usage
#ifdef TELEMETRY_HAS_PDH
    if (PdhOpenQuery(NULL, 0, &g_cpuQuery) == ERROR_SUCCESS) {
        if (PdhAddCounterW(g_cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &g_cpuCounter) != ERROR_SUCCESS) {
            PdhCloseQuery(g_cpuQuery); g_cpuQuery = nullptr;
        } else {
            PdhCollectQueryData(g_cpuQuery); // prime
        }
    }
#endif

    // GPU vendor detect via tools
    std::string path;
    if (DetectExecutable({"nvidia-smi.exe", "nvidia-smi"}, path)) { g_vendorNvidia = true; g_gpuVendor = "NVIDIA"; }
    else if (DetectExecutable({"amd-smi.exe", "amd-smi"}, path)) { g_vendorAmd = true; g_gpuVendor = "AMD"; }
    else { g_gpuVendor.clear(); }

    g_initialized = true;
    return true;
}

static double QueryCpuTemp() {
#ifdef TELEMETRY_HAS_WMI
    if (!g_pServices) return -1.0;
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = g_pServices->ExecQuery(BSTR(L"WQL"), BSTR(L"SELECT * FROM MSAcpi_ThermalZoneTemperature"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    if (FAILED(hr) || !pEnumerator) return -1.0;
    IWbemClassObject *pObj = nullptr;
    ULONG ret = 0;
    double tempC = -1.0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &ret);
    if (ret) {
        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = pObj->Get(L"CurrentTemperature", 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr) && (vtProp.vt == VT_UINT || vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)) {
            // Value is tenths of Kelvin
            double kelvinTenths = (double)vtProp.uintVal;
            tempC = kelvinTenths / 10.0 - 273.15;
        }
        VariantClear(&vtProp);
        pObj->Release();
    }
    pEnumerator->Release();
    return tempC;
#else
    return -1.0;
#endif
}

static double QueryCpuUsage() {
#ifdef TELEMETRY_HAS_PDH
    if (!g_cpuQuery) return -1.0;
    if (PdhCollectQueryData(g_cpuQuery) != ERROR_SUCCESS) return -1.0;
    PDH_FMT_COUNTERVALUE val; DWORD dw = 0;
    if (PdhGetFormattedCounterValue(g_cpuCounter, PDH_FMT_DOUBLE, &dw, &val) == ERROR_SUCCESS) {
        return val.doubleValue;
    }
    return -1.0;
#else
    return -1.0;
#endif
}

static double QueryGpuTemp() {
    if (g_vendorNvidia) {
        std::string out = RunAndCapture("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits");
        double v = ParseFirstNumber(out); return v;
    } else if (g_vendorAmd) {
        // amd-smi output often includes a table; find first number
        std::string out = RunAndCapture("amd-smi.exe --showtemp");
        double v = ParseFirstNumber(out); return v;
    }
    return -1.0;
}

static double QueryGpuUsage() {
    if (g_vendorNvidia) {
        std::string out = RunAndCapture("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits");
        double v = ParseFirstNumber(out); return v;
    } else if (g_vendorAmd) {
        std::string out = RunAndCapture("amd-smi.exe --showuse");
        double v = ParseFirstNumber(out); return v;
    }
    return -1.0;
}

bool Poll(TelemetrySnapshot &out) {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_initialized) return false;
    out.timeMs = NowMs() - g_startMs;

    double cTemp = QueryCpuTemp();
    if (cTemp > -50 && cTemp < 150) { out.cpuTempC = cTemp; out.cpuTempValid = true; }
    else { out.cpuTempValid = false; }

    out.cpuUsagePercent = QueryCpuUsage();
    double gTemp = QueryGpuTemp();
    if (gTemp > -50 && gTemp < 130) { out.gpuTempC = gTemp; out.gpuTempValid = true; }
    else { out.gpuTempValid = false; }

    out.gpuUsagePercent = QueryGpuUsage();
    out.gpuVendor = g_gpuVendor;
    return true;
}

void Shutdown() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_initialized) return;
    if (g_cpuQuery) { PdhCloseQuery(g_cpuQuery); g_cpuQuery = nullptr; }
    if (g_pServices) { g_pServices->Release(); g_pServices = nullptr; }
    if (g_pLocator) { g_pLocator->Release(); g_pLocator = nullptr; }
    CoUninitialize();
    g_initialized = false;
}

} // namespace telemetry
