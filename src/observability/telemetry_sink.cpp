#include "telemetry_sink.h"
#include <fstream>

namespace RawrXD::Observability {
    TelemetrySink& TelemetrySink::getInstance() {
        static TelemetrySink inst;
        return inst;
    }

    void TelemetrySink::recordEvent(const TelemetryEvent& evt) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.push_back(evt);
        if (m_buffer.size() >= m_flushThreshold) {
            flushUnlocked();
        }
    }

    void TelemetrySink::flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        flushUnlocked();
    }

    void TelemetrySink::flushUnlocked() {
        if (m_buffer.empty()) return;
        std::ofstream out(m_outputPath, std::ios::app);
        for (const auto& evt : m_buffer) {
            out << evt.timestamp << "|" << evt.category << "|" << evt.name << "|" << evt.value << "\n";
        }
        m_buffer.clear();
    }

    void TelemetrySink::setOutputPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outputPath = path;
    }
}
