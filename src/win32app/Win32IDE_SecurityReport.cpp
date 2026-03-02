#include <string>
#include <sstream>

extern "C" {
int RawrXD_Security_SastScanPath(const char* rootPath);
int RawrXD_Security_ScanDependencies(const char* workspaceRoot);
int RawrXD_Security_SecretScanPath(const char* rootPath);
}

namespace RawrXD::IDE {

class SecurityReportBuilder {
public:
    std::string buildWorkspaceReport(const std::string& workspaceRoot) {
        const int sastCount = RawrXD_Security_SastScanPath(workspaceRoot.c_str());
        const int depCount = RawrXD_Security_ScanDependencies(workspaceRoot.c_str());
        const int secretCount = RawrXD_Security_SecretScanPath(workspaceRoot.c_str());

        std::ostringstream out;
        out << "RawrXD Unified Security Report\n";
        out << "Workspace: " << workspaceRoot << "\n";
        out << "----------------------------------------\n";
        out << "SAST findings: " << sastCount << "\n";
        out << "Dependencies discovered: " << depCount << "\n";
        out << "Secret hits: " << secretCount << "\n";

        int score = 100;
        score -= (sastCount > 25 ? 40 : sastCount);
        score -= (secretCount > 10 ? 30 : secretCount * 2);
        if (score < 0) score = 0;

        out << "Security posture score: " << score << "/100\n";
        if (score >= 85) out << "Status: GREEN\n";
        else if (score >= 60) out << "Status: YELLOW\n";
        else out << "Status: RED\n";

        return out.str();
    }
};

static SecurityReportBuilder g_report;

} // namespace RawrXD::IDE

extern "C" {

const char* RawrXD_IDE_BuildSecurityReport(const char* workspaceRoot) {
    static thread_local std::string report;
    report = RawrXD::IDE::g_report.buildWorkspaceReport(workspaceRoot ? workspaceRoot : ".");
    return report.c_str();
}

}
