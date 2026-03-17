#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

// ==============================================================================
// RawrXD Telemetry - Prometheus/Grafana Exporter (v15.1)
// ==============================================================================

namespace RawrXD {
namespace Telemetry {

class PrometheusExporter {
private:
    std::map<std::string, double> m_metrics;
    std::mutex m_mtx;

public:
    static PrometheusExporter& Get() {
        static PrometheusExporter instance;
        return instance;
    }

    void RecordMetric(const std::string& key, double value) {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_metrics[key] = value;
    }

    void IncrementCounter(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_metrics[key] += 1.0;
    }

    std::string Scrape() {
        std::lock_guard<std::mutex> lock(m_mtx);
        std::ostringstream oss;
        for (const auto& kv : m_metrics) {
            oss << kv.first << " " << kv.second << "\n";
        }
        return oss.str();
    }
};

class CognitionLoop {
public:
    static void TriggerGhostText(const std::string& model_output) {
        PrometheusExporter::Get().IncrementCounter("agent_ghost_text_generated_total");
#ifdef _WIN32
        OutputDebugStringA(("[*] Ghost Text Rendered: " + model_output + "\n").c_str());
#endif
    }

    static void AcceptEdit() {
        PrometheusExporter::Get().IncrementCounter("agent_edit_accepted_total");
#ifdef _WIN32
        OutputDebugStringA("[+] Ghost Text Accepted by User.\n");
#endif
    }

    static void RejectEdit() {
        PrometheusExporter::Get().IncrementCounter("agent_edit_rejected_total");
#ifdef _WIN32
        OutputDebugStringA("[-] Ghost Text Rejected by User.\n");
#endif
    }
};

} // namespace Telemetry
} // namespace RawrXD
