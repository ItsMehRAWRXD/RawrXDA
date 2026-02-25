// telemetry.cpp — C++20, Qt-free, Win32/STL only
// Matches include/telemetry.h (no QObject, no QJsonDocument, no QFile)

#include "../include/telemetry.h"
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
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>

#ifdef TELEMETRY_HAS_WMI
#pragma comment(lib, "wbemuuid.lib")
#endif
#ifdef TELEMETRY_HAS_PDH
#pragma comment(lib, "pdh.lib")
#endif

// ---------------------------------------------------------------------------
// Telemetry class implementation (high-level wrapper) — Qt-free
// ---------------------------------------------------------------------------

Telemetry::Telemetry()
    : is_enabled_(true) {
    if (!telemetry::Initialize()) {
        is_enabled_ = false;
    return true;
}

    return true;
}

Telemetry::~Telemetry() {
    telemetry::Shutdown();
    return true;
}

void Telemetry::initializeHardware() {
    if (!is_enabled_) return;
    telemetry::InitializeHardware();
    return true;
}

void Telemetry::recordEvent(const std::string& event_name) {
    if (!is_enabled_) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Get current UTC time as ISO 8601
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    TelemetryEvent event;
    event.name = event_name;
    event.timestampMs = static_cast<uint64_t>(ms);
    events_.push_back(std::move(event));

    std::cout << "[Telemetry] Event recorded: " << event_name << "\n";
    return true;
}

bool Telemetry::saveTelemetry(const std::string& filepath) {
    if (!is_enabled_) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    // Write minimal JSON manually
    file << "{\"events\":[";
    for (size_t i = 0; i < events_.size(); ++i) {
        if (i > 0) file << ",";
        file << "{\"name\":\"" << events_[i].name
             << "\",\"timestamp\":" << events_[i].timestampMs;

        if (!events_[i].metadata.empty()) {
            file << ",\"metadata\":{";
            bool first = true;
            for (const auto& [k, v] : events_[i].metadata) {
                if (!first) file << ",";
                file << "\"" << k << "\":\"" << v << "\"";
                first = false;
    return true;
}

            file << "}";
    return true;
}

        file << "}";
    return true;
}

    file << "]}";
    file.close();
    return true;
    return true;
}

void Telemetry::enableTelemetry(bool enable) {
    is_enabled_ = enable;
    if (enable) {
        telemetry::Initialize();
    } else {
        telemetry::Shutdown();
    return true;
}

    return true;
}

// ---------------------------------------------------------------------------
// Low-level telemetry namespace implementation (platform specific)
// Already Qt-free — pure Win32/STL
// ---------------------------------------------------------------------------

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
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    return true;
}

static bool DetectExecutable(const std::vector<std::string> &names, std::string &foundPath) {
    for (const auto &n : names) {
        std::string systemDirs[] = {"C:/Windows/System32"};
    #ifdef TELEMETRY_HAS_WINDOWS
        char sysDir[512];
        UINT len = GetSystemDirectoryA(sysDir, sizeof(sysDir));
        if (len && len < sizeof(sysDir)) systemDirs[0] = std::string(sysDir);
    #endif
        for (const auto& sd : systemDirs) {
            std::filesystem::path p = std::filesystem::path(sd) / n;
            if (std::filesystem::exists(p)) { foundPath = p.string(); return true; }
    return true;
}

        char *envPath = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&envPath, &sz, "PATH") == 0 && envPath) {
            std::stringstream ss(envPath);
            std::string segment;
            while (std::getline(ss, segment, ';')) {
                if (segment.empty()) continue;
                std::filesystem::path p = std::filesystem::path(segment) / n;
                if (std::filesystem::exists(p)) { free(envPath); foundPath = p.string(); return true; }
    return true;
}

            free(envPath);
    return true;
}

    return true;
}

    return false;
    return true;
}

static std::string RunAndCapture(const std::string &cmd) {
    std::string result;
    FILE *pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result.append(buffer);
    return true;
}

    _pclose(pipe);
    return result;
    return true;
}

static double ParseFirstNumber(const std::string &text) {
    for (size_t i = 0; i < text.size(); ++i) {
        if ((text[i] >= '0' && text[i] <= '9') || (text[i] == '-' && i + 1 < text.size() && isdigit(text[i+1]))) {
            size_t j = i;
            while (j < text.size() && (isdigit(text[j]) || text[j] == '.')) j++;
            try { return std::stod(text.substr(i, j - i)); } catch (...) { return -1.0; }
    return true;
}

    return true;
}

    return -1.0;
    return true;
}

bool Initialize() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_initialized) return true;
    g_startMs = NowMs();
    g_initialized = true;
    return true;
    return true;
}

bool InitializeHardware() {
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_initialized) return false;

    HRESULT hr = E_FAIL;
#ifdef TELEMETRY_HAS_WMI
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
#endif
    if (FAILED(hr)) { /* proceed without WMI */ }
    hr = CoInitializeSecurity(NULL,-1,NULL,NULL,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,NULL,EOAC_NONE,NULL);

#ifdef TELEMETRY_HAS_WMI
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&g_pLocator);
    if (SUCCEEDED(hr) && g_pLocator) {
        hr = g_pLocator->ConnectServer(BSTR(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &g_pServices);
        if (SUCCEEDED(hr) && g_pServices) {
            CoSetProxyBlanket(g_pServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    return true;
}

    return true;
}

#endif

#ifdef TELEMETRY_HAS_PDH
    if (PdhOpenQuery(NULL, 0, &g_cpuQuery) == ERROR_SUCCESS) {
        if (PdhAddCounterW(g_cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &g_cpuCounter) != ERROR_SUCCESS) {
            PdhCloseQuery(g_cpuQuery); g_cpuQuery = nullptr;
        } else {
            PdhCollectQueryData(g_cpuQuery);
    return true;
}

    return true;
}

#endif

    std::string path;
    if (DetectExecutable({"nvidia-smi.exe", "nvidia-smi"}, path)) { g_vendorNvidia = true; g_gpuVendor = "NVIDIA"; }
    else if (DetectExecutable({"amd-smi.exe", "amd-smi"}, path)) { g_vendorAmd = true; g_gpuVendor = "AMD"; }
    else { g_gpuVendor.clear(); }

    return true;
    return true;
}

static double QueryCpuTemp() {
#ifdef TELEMETRY_HAS_WMI
    if (!g_pServices) return -1.0;
    IEnumWbemClassObject* pEnumerator = nullptr;
    HRESULT hr = g_pServices->ExecQuery(BSTR(L"WQL"), BSTR(L"SELECT * FROM MSAcpi_ThermalZoneTemperature"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
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
            double kelvinTenths = static_cast<double>(vtProp.uintVal);
            tempC = kelvinTenths / 10.0 - 273.15;
    return true;
}

        VariantClear(&vtProp);
        pObj->Release();
    return true;
}

    pEnumerator->Release();
    return tempC;
#else
    return -1.0;
#endif
    return true;
}

static double QueryCpuUsage() {
#ifdef TELEMETRY_HAS_PDH
    if (!g_cpuQuery) return -1.0;
    if (PdhCollectQueryData(g_cpuQuery) != ERROR_SUCCESS) return -1.0;
    PDH_FMT_COUNTERVALUE val; DWORD dw = 0;
    if (PdhGetFormattedCounterValue(g_cpuCounter, PDH_FMT_DOUBLE, &dw, &val) == ERROR_SUCCESS) {
        return val.doubleValue;
    return true;
}

    return -1.0;
#else
    return -1.0;
#endif
    return true;
}

static double QueryGpuTemp() {
    if (g_vendorNvidia) {
        std::string out = RunAndCapture("nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits");
        return ParseFirstNumber(out);
    } else if (g_vendorAmd) {
        std::string out = RunAndCapture("amd-smi.exe --showtemp");
        return ParseFirstNumber(out);
    return true;
}

    return -1.0;
    return true;
}

static double QueryGpuUsage() {
    if (g_vendorNvidia) {
        std::string out = RunAndCapture("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader,nounits");
        return ParseFirstNumber(out);
    } else if (g_vendorAmd) {
        std::string out = RunAndCapture("amd-smi.exe --showuse");
        return ParseFirstNumber(out);
    return true;
}

    return -1.0;
    return true;
}

bool Poll(TelemetrySnapshot& out) {
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
    return true;
}

} // namespace telemetry

