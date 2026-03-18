#include "Win32IDE.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

class TelemetryExportManager {
public:
    ~TelemetryExportManager();
};

TelemetryExportManager::~TelemetryExportManager() = default;

namespace {

struct TelemetryRecord {
    std::string type;
    std::string data;
    std::string timestamp;
};

std::string nowTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);

    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

int timeRangeToHours(const std::string& range) {
    if (range == "1h") return 1;
    if (range == "6h") return 6;
    if (range == "24h") return 24;
    if (range == "7d") return 24 * 7;
    if (range == "30d") return 24 * 30;
    return 24;
}

std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string escapeCsv(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string escapeXml(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::vector<TelemetryRecord> loadTelemetryRows(const std::vector<std::vector<std::string>>& rows) {
    std::vector<TelemetryRecord> records;
    records.reserve(rows.size() + 1);
    for (const auto& row : rows) {
        if (row.size() < 3) {
            continue;
        }
        TelemetryRecord record;
        record.type = row[0];
        record.data = row[1];
        record.timestamp = row[2];
        records.push_back(std::move(record));
    }

    TelemetryRecord session;
    session.type = "session";
    session.timestamp = nowTimestamp();
    session.data = std::string("{\"uptime_seconds\":") + std::to_string(GetTickCount64() / 1000ULL) + "}";
    records.push_back(std::move(session));

    return records;
}

bool writeJson(const std::vector<TelemetryRecord>& records, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "{\n  \"exported_at\": \"" << nowTimestamp() << "\",\n";
    file << "  \"count\": " << records.size() << ",\n";
    file << "  \"events\": [\n";
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        file << "    {\"type\":\"" << escapeJson(r.type)
             << "\",\"timestamp\":\"" << escapeJson(r.timestamp)
             << "\",\"data\":\"" << escapeJson(r.data) << "\"}";
        if (i + 1 < records.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "  ]\n}\n";
    return true;
}

bool writeCsv(const std::vector<TelemetryRecord>& records, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "timestamp,type,data\n";
    for (const auto& r : records) {
        file << '"' << escapeCsv(r.timestamp) << "\","
             << '"' << escapeCsv(r.type) << "\","
             << '"' << escapeCsv(r.data) << "\"\n";
    }
    return true;
}

bool writeXml(const std::vector<TelemetryRecord>& records, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<telemetry exported_at=\"" << escapeXml(nowTimestamp()) << "\">\n";
    for (const auto& r : records) {
        file << "  <event type=\"" << escapeXml(r.type) << "\" timestamp=\"" << escapeXml(r.timestamp) << "\">"
             << escapeXml(r.data) << "</event>\n";
    }
    file << "</telemetry>\n";
    return true;
}

bool writeYaml(const std::vector<TelemetryRecord>& records, const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    file << "exported_at: \"" << nowTimestamp() << "\"\n";
    file << "count: " << records.size() << "\n";
    file << "events:\n";
    for (const auto& r : records) {
        file << "  - timestamp: \"" << r.timestamp << "\"\n";
        file << "    type: \"" << r.type << "\"\n";
        file << "    data: \"" << escapeJson(r.data) << "\"\n";
    }
    return true;
}

std::string defaultExportDirectory() {
    const char* appData = std::getenv("APPDATA");
    if (appData && *appData) {
        return std::string(appData) + "\\RawrXD\\TelemetryExports";
    }
    return "TelemetryExports";
}

std::string extensionForFormat(const std::string& format) {
    if (format == "JSON") return ".json";
    if (format == "CSV") return ".csv";
    if (format == "XML") return ".xml";
    if (format == "YAML") return ".yaml";
    if (format == "Parquet") return ".parquet";
    if (format == "SQLite") return ".db";
    return ".log";
}

std::string defaultExportPath(const std::string& format) {
    const std::string dir = defaultExportDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string timestamp = nowTimestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');

    return dir + "\\telemetry_" + timestamp + extensionForFormat(format);
}

}  // namespace

bool Win32IDE::exportTelemetryData(const std::string& format, const std::string& timeRange, const std::string& filename) {
    initSQLite3Core();

    const int hours = timeRangeToHours(timeRange);
    std::ostringstream sql;
    sql << "SELECT event_type, event_data, timestamp FROM telemetry_events "
        << "WHERE timestamp >= datetime('now', '-" << hours << " hours') "
        << "ORDER BY timestamp DESC LIMIT 50000;";

    const auto rows = executeDatabaseQuery(sql.str());
    const auto records = loadTelemetryRows(rows);
    if (records.empty()) {
        appendToOutput("[TelemetryExport] No telemetry data available for selected range.\n", "Telemetry", OutputSeverity::Warning);
        return false;
    }

    const std::string outputPath = filename.empty() ? defaultExportPath(format) : filename;

    bool ok = false;
    if (format == "JSON") {
        ok = writeJson(records, outputPath);
    } else if (format == "CSV") {
        ok = writeCsv(records, outputPath);
    } else if (format == "XML") {
        ok = writeXml(records, outputPath);
    } else if (format == "YAML") {
        ok = writeYaml(records, outputPath);
    } else if (format == "Parquet" || format == "SQLite") {
        appendToOutput("[TelemetryExport] Format not supported in this build: " + format + "\n", "Telemetry", OutputSeverity::Error);
        return false;
    } else {
        appendToOutput("[TelemetryExport] Unknown format: " + format + "\n", "Telemetry", OutputSeverity::Error);
        return false;
    }

    if (ok) {
        appendToOutput("[TelemetryExport] Exported to: " + outputPath + "\n", "Telemetry", OutputSeverity::Success);
    } else {
        appendToOutput("[TelemetryExport] Failed to write: " + outputPath + "\n", "Telemetry", OutputSeverity::Error);
    }

    return ok;
}

std::vector<std::string> Win32IDE::getTelemetryExportFormats() {
    return {"JSON", "CSV", "XML", "YAML", "Parquet", "SQLite"};
}

std::string Win32IDE::getTelemetryExportDirectory() {
    return defaultExportDirectory();
}

void Win32IDE::handleTelemetryExportCommand() {
    if (showTelemetryExportDialog()) {
        exportTelemetryData("JSON", "24h", "");
    }
}

bool Win32IDE::showTelemetryExportDialog() {
    const std::string msg = "Telemetry export uses current defaults:\n"
                            "- Format: JSON\n"
                            "- Range: last 24h\n"
                            "- Destination: " + defaultExportDirectory() +
                            "\n\nContinue?";
    const int answer = MessageBoxA(getMainWindow(), msg.c_str(), "Telemetry Export", MB_OKCANCEL | MB_ICONINFORMATION);
    return answer == IDOK;
}
