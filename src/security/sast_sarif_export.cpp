#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace RawrXD::Security {

struct SarifFinding {
    std::string ruleId;
    std::string message;
    std::string file;
    int line = 1;
    int column = 1;
    std::string level = "warning";
};

class SarifExporter {
public:
    bool exportDeterministic(std::vector<SarifFinding> findings, const std::string& outPath) {
        std::sort(findings.begin(), findings.end(), [](const SarifFinding& a, const SarifFinding& b) {
            if (a.file != b.file) return a.file < b.file;
            if (a.line != b.line) return a.line < b.line;
            return a.ruleId < b.ruleId;
        });

        std::ofstream out(outPath, std::ios::trunc);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"$schema\": \"https://json.schemastore.org/sarif-2.1.0.json\",\n";
        out << "  \"version\": \"2.1.0\",\n";
        out << "  \"runs\": [{\n";
        out << "    \"tool\": {\"driver\": {\"name\": \"RawrXD SAST\", \"informationUri\": \"https://github.com/ItsMehRAWRXD/RawrXD\"}},\n";
        out << "    \"results\": [\n";

        for (size_t i = 0; i < findings.size(); ++i) {
            auto f = findings[i];
            std::replace(f.file.begin(), f.file.end(), '\\', '/');
            out << "      {\n";
            out << "        \"ruleId\": \"" << esc(f.ruleId) << "\",\n";
            out << "        \"level\": \"" << esc(f.level) << "\",\n";
            out << "        \"message\": {\"text\": \"" << esc(f.message) << "\"},\n";
            out << "        \"locations\": [{\n";
            out << "          \"physicalLocation\": {\n";
            out << "            \"artifactLocation\": {\"uri\": \"" << esc(f.file) << "\"},\n";
            out << "            \"region\": {\"startLine\": " << f.line << ", \"startColumn\": " << f.column << "}\n";
            out << "          }\n";
            out << "        }]\n";
            out << "      }";
            if (i + 1 < findings.size()) out << ",";
            out << "\n";
        }

        out << "    ]\n";
        out << "  }]\n";
        out << "}\n";
        return true;
    }

private:
    static std::string esc(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        return out;
    }
};

static SarifExporter g_sarif;

} // namespace RawrXD::Security

extern "C" {

bool RawrXD_Security_ExportSarif(const char* outPath) {
    if (!outPath) return false;
    std::vector<RawrXD::Security::SarifFinding> findings = {
        {"RAWRXD_SAMPLE_001", "Sample deterministic finding", "src/main.cpp", 10, 1, "warning"}
    };
    return RawrXD::Security::g_sarif.exportDeterministic(findings, outPath);
}

}
