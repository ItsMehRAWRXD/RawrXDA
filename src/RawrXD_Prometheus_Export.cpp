// RawrXD Prometheus Exporter - v22.4.0
// Metrics Export for Grafana Singularity

#include <windows.h>
#include <iostream>
#include <sstream>

extern "C" {
    void Titan_GetTelemetryData(void* ptr);
}

extern "C" __declspec(dllexport) const char* Core_ExportPrometheusMetrics() {
    static char metricsBuf[1024];
    uint64_t data[4]; // [Inf, Bytes, Mem]
    Titan_GetTelemetryData(data);
    
    std::stringstream ss;
    ss << "# HELP rawrxd_inference_time_ms Time taken for last inference\n";
    ss << "# TYPE rawrxd_inference_time_ms gauge\n";
    ss << "rawrxd_inference_time_ms " << data[0] << "\n";
    
    ss << "# HELP rawrxd_io_bytes_total Total bytes processed from NVMe\n";
    ss << "# TYPE rawrxd_io_bytes_total counter\n";
    ss << "rawrxd_io_bytes_total " << data[1] << "\n";
    
    ss << "# HELP rawrxd_memory_usage_mb Current VRAM/RAM allocation\n";
    ss << "# TYPE rawrxd_memory_usage_mb gauge\n";
    ss << "rawrxd_memory_usage_mb " << data[2] << "\n";
    
    lstrcpyA(metricsBuf, ss.str().c_str());
    return metricsBuf;
}
