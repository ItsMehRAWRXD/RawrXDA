// ============================================================================
// AI Log Analyzer — Intelligent Log Analysis and Anomaly Detection
// Analyzes logs for patterns, anomalies, and correlations
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <regex>

namespace RawrXD::Logs {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::string source;
    std::string message;
    std::map<std::string, std::string> metadata;
    std::string threadId;
};

struct LogPattern {
    std::string pattern;
    int frequency;
    std::chrono::system_clock::time_point firstSeen;
    std::chrono::system_clock::time_point lastSeen;
    std::vector<LogLevel> associatedLevels;
};

struct LogInsights {
    std::vector<LogPattern> patterns;
    std::vector<LogEntry> anomalies;
    std::map<std::string, int> errorFrequency;
    std::vector<std::string> recommendations;
    std::chrono::system_clock::time_point analyzedAt;
};

struct Event {
    std::string id;
    std::string type;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> attributes;
    std::vector<std::string> relatedLogIds;
};

class AILogAnalyzer {
public:
    explicit AILogAnalyzer(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    LogInsights AnalyzeLogs(const std::vector<LogEntry>& entries) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        LogInsights insights;
        insights.analyzedAt = std::chrono::system_clock::now();
        
        // Extract patterns
        insights.patterns = ExtractPatterns(entries);
        
        // Detect anomalies
        insights.anomalies = DetectAnomalies(entries);
        
        // Calculate error frequency
        for (const auto& entry : entries) {
            if (entry.level >= LogLevel::ERROR) {
                insights.errorFrequency[entry.source]++;
            }
        }
        
        // Generate recommendations
        insights.recommendations = GenerateRecommendations(insights);
        
        // AI-powered analysis
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiInsights = PerformAIAnalysis(entries);
            insights.recommendations.insert(insights.recommendations.end(),
                                            aiInsights.begin(), aiInsights.end());
        }
        
        return insights;
    }

    std::vector<LogEntry> DetectAnomalies(const std::vector<LogEntry>& entries) {
        std::vector<LogEntry> anomalies;
        
        // Statistical anomaly detection
        std::map<LogLevel, int> levelCounts;
        for (const auto& entry : entries) {
            levelCounts[entry.level]++;
        }
        
        double total = entries.size();
        double errorRate = levelCounts[LogLevel::ERROR] / total;
        double criticalRate = levelCounts[LogLevel::CRITICAL] / total;
        
        // Flag if error rate is unusually high
        if (errorRate > 0.1) { // More than 10% errors
            for (const auto& entry : entries) {
                if (entry.level == LogLevel::ERROR) {
                    anomalies.push_back(entry);
                }
            }
        }
        
        // Detect temporal anomalies (bursts of errors)
        auto bursts = DetectErrorBursts(entries);
        anomalies.insert(anomalies.end(), bursts.begin(), bursts.end());
        
        // Detect unusual patterns
        auto unusual = DetectUnusualPatterns(entries);
        anomalies.insert(anomalies.end(), unusual.begin(), unusual.end());
        
        return anomalies;
    }

    std::vector<Event> CorrelateEvents(const std::vector<Event>& events) {
        std::vector<Event> correlatedEvents;
        
        // Group events by time windows
        std::map<std::chrono::system_clock::time_point, std::vector<Event>> timeGroups;
        
        for (const auto& event : events) {
            // Round to nearest minute for grouping
            auto roundedTime = std::chrono::time_point_cast<std::chrono::minutes>(event.timestamp);
            timeGroups[roundedTime].push_back(event);
        }
        
        // Find correlations within time groups
        for (const auto& [time, group] : timeGroups) {
            if (group.size() > 1) {
                // Check for causal relationships
                for (size_t i = 0; i < group.size(); ++i) {
                    for (size_t j = i + 1; j < group.size(); ++j) {
                        if (AreRelated(group[i], group[j])) {
                            Event correlation;
                            correlation.id = "corr_" + group[i].id + "_" + group[j].id;
                            correlation.type = "correlation";
                            correlation.timestamp = time;
                            correlation.attributes["event1"] = group[i].id;
                            correlation.attributes["event2"] = group[j].id;
                            correlation.attributes["relationship"] = "temporal";
                            correlatedEvents.push_back(correlation);
                        }
                    }
                }
            }
        }
        
        return correlatedEvents;
    }

    std::string GenerateLogReport(const LogInsights& insights) {
        std::ostringstream report;
        report << "# Log Analysis Report\n\n";
        report << "Generated: " << FormatTime(insights.analyzedAt) << "\n\n";
        
        report << "## Patterns Detected\n";
        for (const auto& pattern : insights.patterns) {
            report << "- Pattern: " << pattern.pattern << "\n";
            report << "  Frequency: " << pattern.frequency << "\n";
            report << "  First seen: " << FormatTime(pattern.firstSeen) << "\n";
            report << "\n";
        }
        
        if (!insights.anomalies.empty()) {
            report << "## Anomalies Detected\n";
            for (const auto& anomaly : insights.anomalies) {
                report << "- [" << LogLevelToString(anomaly.level) << "] " << anomaly.message << "\n";
            }
            report << "\n";
        }
        
        if (!insights.errorFrequency.empty()) {
            report << "## Error Frequency by Source\n";
            for (const auto& [source, count] : insights.errorFrequency) {
                report << "- " << source << ": " << count << " errors\n";
            }
            report << "\n";
        }
        
        if (!insights.recommendations.empty()) {
            report << "## Recommendations\n";
            for (const auto& rec : insights.recommendations) {
                report << "- " << rec << "\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::vector<LogPattern> m_knownPatterns;

    std::vector<LogPattern> ExtractPatterns(const std::vector<LogEntry>& entries) {
        std::map<std::string, int> patternCounts;
        
        // Extract message patterns (simplified)
        for (const auto& entry : entries) {
            // Normalize message (remove variable parts)
            std::string normalized = NormalizeMessage(entry.message);
            patternCounts[normalized]++;
        }
        
        std::vector<LogPattern> patterns;
        for (const auto& [pattern, count] : patternCounts) {
            if (count > 1) { // Only include patterns that appear multiple times
                LogPattern lp;
                lp.pattern = pattern;
                lp.frequency = count;
                lp.firstSeen = entries.front().timestamp;
                lp.lastSeen = entries.back().timestamp;
                patterns.push_back(lp);
            }
        }
        
        return patterns;
    }

    std::vector<LogEntry> DetectErrorBursts(const std::vector<LogEntry>& entries) {
        std::vector<LogEntry> bursts;
        
        // Group entries by time windows
        std::map<std::chrono::system_clock::time_point, std::vector<LogEntry>> windows;
        
        for (const auto& entry : entries) {
            if (entry.level >= LogLevel::ERROR) {
                auto window = std::chrono::time_point_cast<std::chrono::minutes>(entry.timestamp);
                windows[window].push_back(entry);
            }
        }
        
        // Find windows with high error counts
        for (const auto& [time, windowEntries] : windows) {
            if (windowEntries.size() > 5) { // More than 5 errors in a minute
                bursts.insert(bursts.end(), windowEntries.begin(), windowEntries.end());
            }
        }
        
        return bursts;
    }

    std::vector<LogEntry> DetectUnusualPatterns(const std::vector<LogEntry>& entries) {
        std::vector<LogEntry> unusual;
        
        // Check for unusual log sources
        std::map<std::string, int> sourceCounts;
        for (const auto& entry : entries) {
            sourceCounts[entry.source]++;
        }
        
        double avgCount = entries.size() / static_cast<double>(sourceCounts.size());
        
        for (const auto& entry : entries) {
            if (sourceCounts[entry.source] > avgCount * 3) {
                unusual.push_back(entry);
            }
        }
        
        return unusual;
    }

    std::vector<std::string> GenerateRecommendations(const LogInsights& insights) {
        std::vector<std::string> recommendations;
        
        if (!insights.anomalies.empty()) {
            recommendations.push_back("Investigate " + 
                std::to_string(insights.anomalies.size()) + " detected anomalies");
        }
        
        if (!insights.errorFrequency.empty()) {
            auto maxErrors = std::max_element(insights.errorFrequency.begin(),
                                               insights.errorFrequency.end(),
                                               [](const auto& a, const auto& b) {
                                                   return a.second < b.second;
                                               });
            recommendations.push_back("Focus on reducing errors from " + maxErrors->first);
        }
        
        return recommendations;
    }

    std::vector<std::string> PerformAIAnalysis(const std::vector<LogEntry>& entries) {
        std::vector<std::string> insights;
        
        if (!m_aiClient || !m_aiClient->IsLoaded() || entries.empty()) {
            return insights;
        }

        // Sample entries for AI analysis
        std::string sample;
        int count = 0;
        for (const auto& entry : entries) {
            if (entry.level >= LogLevel::ERROR && count < 10) {
                sample += entry.message + "\n";
                count++;
            }
        }
        
        std::string prompt = "Analyze these error logs and provide insights:\n" + sample;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a log analysis expert. Identify root causes and patterns."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            insights.push_back("AI Analysis: " + result.response);
        }
        
        return insights;
    }

    bool AreRelated(const Event& e1, const Event& e2) {
        // Check if events are related based on attributes and timing
        if (e1.type == e2.type) return true;
        
        // Check for shared attributes
        for (const auto& [key, value] : e1.attributes) {
            auto it = e2.attributes.find(key);
            if (it != e2.attributes.end() && it->second == value) {
                return true;
            }
        }
        
        return false;
    }

    std::string NormalizeMessage(const std::string& message) {
        // Remove variable parts (numbers, IDs, etc.)
        std::string normalized = message;
        
        // Replace numbers with placeholder
        std::regex numberRegex("\\d+");
        normalized = std::regex_replace(normalized, numberRegex, "{NUM}");
        
        // Replace UUIDs
        std::regex uuidRegex("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
        normalized = std::regex_replace(normalized, uuidRegex, "{UUID}");
        
        return normalized;
    }

    std::string LogLevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Logs
