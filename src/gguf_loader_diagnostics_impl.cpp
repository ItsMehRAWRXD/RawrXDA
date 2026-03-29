// gguf_loader_diagnostics_impl.cpp
// Production implementation for GGUFLoaderDiagnostics - includes the .h and
// provides out-of-line method definitions (the original .cpp redefines the class).

#include "gguf_loader_diagnostics.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {

GGUFLoaderDiagnostics::GGUFLoaderDiagnostics() = default;
GGUFLoaderDiagnostics::~GGUFLoaderDiagnostics() = default;

GGUFLoaderDiagnostics& GGUFLoaderDiagnostics::Instance() {
    static GGUFLoaderDiagnostics instance;
    return instance;
}

void GGUFLoaderDiagnostics::StartLoad(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelPath = modelPath;
    m_startTime = std::chrono::steady_clock::now();
    m_events.clear();
    m_totalMemoryPeak = 0;
    m_currentLoadId++;
    RecordStage(LoadStage::kFileOpen, true, "Starting GGUF load for: " + modelPath);
}

void GGUFLoaderDiagnostics::RecordStage(LoadStage stage, bool success,
    const std::string& detail, const std::string& errorMsg, uint64_t memoryUsage)
{
    LoadEvent evt;
    evt.stage = stage;
    evt.detail = detail;
    evt.success = success;
    evt.errorMessage = errorMsg;
    evt.timestamp = std::chrono::steady_clock::now();
    evt.memoryUsage = memoryUsage;
    auto elapsed = evt.timestamp - m_startTime;
    evt.durationMs = std::chrono::duration<double, std::milli>(elapsed).count();
    if (memoryUsage > m_totalMemoryPeak) m_totalMemoryPeak = memoryUsage;
    m_events.push_back(std::move(evt));
}

void GGUFLoaderDiagnostics::EndLoad() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    double totalMs = std::chrono::duration<double, std::milli>(elapsed).count();
    RecordStage(LoadStage::kComplete, true,
        "Load completed in " + std::to_string(totalMs) + " ms");
}

std::string GGUFLoaderDiagnostics::GetDiagnosticReport() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "=== GGUF Loader Diagnostics ===\n";
    ss << "Model: " << m_modelPath << "\n";
    ss << "Events: " << m_events.size() << "\n";
    for (const auto& e : m_events) {
        ss << "  [" << (e.success ? "OK" : "FAIL") << "] " << e.detail;
        if (!e.errorMessage.empty()) ss << " | " << e.errorMessage;
        ss << "\n";
    }
    return ss.str();
}

std::string GGUFLoaderDiagnostics::GetJsonReport() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::stringstream ss;
    ss << "{\"model\":\"" << EscapeJson(m_modelPath) << "\",\"events\":[";
    for (size_t i = 0; i < m_events.size(); i++) {
        if (i) ss << ",";
        ss << "{\"success\":" << (m_events[i].success ? "true" : "false")
           << ",\"detail\":\"" << EscapeJson(m_events[i].detail) << "\""
           << ",\"durationMs\":" << m_events[i].durationMs << "}";
    }
    ss << "]}";
    return ss.str();
}

const std::vector<GGUFLoaderDiagnostics::LoadEvent>& GGUFLoaderDiagnostics::GetEvents() const {
    return m_events;
}

std::vector<std::pair<std::string, std::string>> GGUFLoaderDiagnostics::GetFailedStages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, std::string>> failures;
    for (auto& e : m_events) {
        if (!e.success) failures.emplace_back(e.detail, e.errorMessage);
    }
    return failures;
}

bool GGUFLoaderDiagnostics::HasCriticalFailure() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::any_of(m_events.begin(), m_events.end(),
        [](const LoadEvent& e) { return !e.success; });
}

std::string GGUFLoaderDiagnostics::FormatMemory(uint64_t bytes) const {
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    const double kKB = 1024.0;
    const double kMB = kKB * 1024.0;
    const double kGB = kMB * 1024.0;

    if (bytes < 1024ull * 1024ull) {
        double kb = static_cast<double>(bytes) / kKB;
        oss << kb << " KB";
    } else if (bytes < 1024ull * 1024ull * 1024ull) {
        double mb = static_cast<double>(bytes) / kMB;
        oss << mb << " MB";
    } else {
        double gb = static_cast<double>(bytes) / kGB;
        oss << gb << " GB";
    }

    return oss.str();
}

std::string GGUFLoaderDiagnostics::EscapeJson(const std::string& str) const {
    std::string out;
    out.reserve(str.size() * 2);
    for (char c : str) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

void GGUFLoaderDiagnostics::AnalyzeLoadPerformance() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_events.empty()) return;

    // Find the slowest stage and compute per-stage statistics
    double totalMs = 0.0;
    double maxStageMs = 0.0;
    std::string slowestStage;
    double prevMs = 0.0;

    for (size_t i = 0; i < m_events.size(); i++) {
        double stageMs = (i == 0) ? m_events[i].durationMs
                                  : m_events[i].durationMs - prevMs;
        if (stageMs < 0) stageMs = 0;
        prevMs = m_events[i].durationMs;

        if (stageMs > maxStageMs) {
            maxStageMs = stageMs;
            slowestStage = m_events[i].StageName();
        }
        totalMs = m_events[i].durationMs;
    }

    // Record analysis as an event so it's included in reports
    std::stringstream detail;
    detail << "Performance analysis: total=" << totalMs << "ms, "
           << "slowest_stage=" << slowestStage << " (" << maxStageMs << "ms), "
           << "peak_memory=" << FormatMemory(m_totalMemoryPeak) << ", "
           << "stages=" << m_events.size();

    LoadEvent evt;
    evt.stage = LoadStage::kUnknown;
    evt.detail = detail.str();
    evt.success = true;
    evt.timestamp = std::chrono::steady_clock::now();
    evt.durationMs = std::chrono::duration<double, std::milli>(
        evt.timestamp - m_startTime).count();
    evt.memoryUsage = m_totalMemoryPeak;
    m_events.push_back(std::move(evt));
}

std::string GGUFLoaderDiagnostics::GetRecommendedActions(
    const std::vector<std::pair<std::string, std::string>>& failures) const {
    if (failures.empty()) return "No issues detected. Load completed successfully.";

    std::stringstream ss;
    ss << "Recommended actions (" << failures.size() << " issue(s)):\n";
    for (size_t i = 0; i < failures.size(); i++) {
        const auto& [stage, err] = failures[i];
        ss << "  " << (i + 1) << ". [" << stage << "] ";

        // Produce actionable guidance based on stage/error content
        std::string lower = err;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("file") != std::string::npos || lower.find("open") != std::string::npos ||
            lower.find("not found") != std::string::npos) {
            ss << "Verify the GGUF file path exists and is accessible.";
        } else if (lower.find("memory") != std::string::npos || lower.find("alloc") != std::string::npos) {
            ss << "Insufficient memory. Try a smaller quantisation (Q4_K_S) or reduce context length.";
        } else if (lower.find("vulkan") != std::string::npos || lower.find("gpu") != std::string::npos) {
            ss << "GPU initialisation failed. Update drivers or fall back to CPU with -ngl 0.";
        } else if (lower.find("header") != std::string::npos || lower.find("magic") != std::string::npos) {
            ss << "Invalid GGUF header. The file may be corrupted or an unsupported version.";
        } else if (lower.find("vocab") != std::string::npos) {
            ss << "Vocabulary load failed. The tokeniser data may be truncated.";
        } else {
            ss << "Investigate error: " << err;
        }
        ss << "\n";
    }
    return ss.str();
}

std::string GGUFLoaderDiagnostics::GetFailedStagesJson() const {
    auto failures = GetFailedStages();
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < failures.size(); i++) {
        if (i) ss << ",";
        ss << "{\"stage\":\"" << EscapeJson(failures[i].first)
           << "\",\"error\":\"" << EscapeJson(failures[i].second) << "\"}";
    }
    ss << "]";
    return ss.str();
}

std::string GGUFLoaderDiagnostics::LoadEvent::StageName() const {
    switch (stage) {
        case LoadStage::kFileOpen:        return "FileOpen";
        case LoadStage::kHeaderParse:     return "HeaderParse";
        case LoadStage::kMetadataRead:    return "MetadataRead";
        case LoadStage::kVocabLoad:       return "VocabLoad";
        case LoadStage::kWeightDecode:    return "WeightDecode";
        case LoadStage::kVulkanInit:      return "VulkanInit";
        case LoadStage::kTransformerBuild: return "TransformerBuild";
        case LoadStage::kComplete:        return "Complete";
        default:                          return "Unknown";
    }
}

}  // namespace RawrXD
