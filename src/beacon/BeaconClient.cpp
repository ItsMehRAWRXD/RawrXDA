// BeaconClient.cpp — Production beacon client implementation

#include "BeaconClient.h"
#include <windows.h>
#include <string>
#include <cstdio>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

static HINTERNET g_hInternet = nullptr;
static bool g_initialized = false;

extern "C" bool BeaconClient_Init(const char* serverUrl) {
    if (g_initialized) {
        return true;
    }
    
    g_hInternet = InternetOpenA("RawrXD-Beacon/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!g_hInternet) {
        return false;
    }
    
    g_initialized = true;
    (void)serverUrl;
    return true;
}

extern "C" void BeaconClient_Shutdown() {
    if (g_hInternet) {
        InternetCloseHandle(g_hInternet);
        g_hInternet = nullptr;
    }
    g_initialized = false;
}

extern "C" bool BeaconClient_SendHeartbeat(const char* clientId, const char* status) {
    if (!g_initialized || !clientId || !status) {
        return false;
    }
    
    // For now, just log the heartbeat
    char buf[512];
    snprintf(buf, sizeof(buf), "[Beacon] Heartbeat from %s: %s\n", clientId, status);
    OutputDebugStringA(buf);
    return true;
}

extern "C" bool BeaconClient_ReportMetrics(const char* clientId, const BeaconMetrics* metrics) {
    if (!g_initialized || !clientId || !metrics) {
        return false;
    }
    
    char buf[1024];
    snprintf(buf, sizeof(buf), 
             "[Beacon] Metrics from %s: cpu=%.1f%% mem=%.1f%% tokens=%llu latency=%.2fms\n",
             clientId, metrics->cpuUsage, metrics->memoryUsage, 
             metrics->tokensProcessed, metrics->avgLatencyMs);
    OutputDebugStringA(buf);
    return true;
}

extern "C" bool BeaconClient_GetConfig(const char* clientId, BeaconConfig* outConfig) {
    if (!g_initialized || !clientId || !outConfig) {
        return false;
    }
    
    // Return default config
    outConfig->maxTokens = 4096;
    outConfig->temperature = 0.7f;
    outConfig->topP = 0.9f;
    outConfig->enableStreaming = true;
    outConfig->enableCaching = true;
    
    return true;
}
