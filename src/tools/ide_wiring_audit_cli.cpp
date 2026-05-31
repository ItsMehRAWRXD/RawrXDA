#include "../win32app/IDEWiringAutoMapper.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void print_usage() {
    std::fprintf(stdout,
                 "RawrXD IDE Wiring Audit CLI\n"
                 "Usage:\n"
                 "  ide_wiring_audit_cli.exe [options]\n\n"
                 "Options:\n"
                 "  --report <path>            JSON report output path\n"
                 "  --submission <path>        JSON submission output path\n"
                 "  --reinspect-passes <n>     Reinspect passes (1-8)\n"
                 "  --no-legacy                Exclude legacy paths\n"
                 "  --no-boundary-check        Disable CLI/GUI start-end checks\n"
                 "  --help                     Show this help\n");
}

bool parse_int(const char* s, int& out) {
    if (!s || !*s) {
        return false;
    }
    char* end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || (end && *end != '\0')) {
        return false;
    }
    if (v < 1 || v > 8) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    RawrXD::Wiring::AutoMapperOptions options;
    std::string report_path = "wiring_auto_map_report.json";
    std::string submission_path = "wiring_generation_submission.json";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--report") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Missing value for --report\n");
                return 2;
            }
            report_path = argv[++i];
        } else if (arg == "--submission") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Missing value for --submission\n");
                return 2;
            }
            submission_path = argv[++i];
        } else if (arg == "--reinspect-passes") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Missing value for --reinspect-passes\n");
                return 2;
            }
            int p = 0;
            if (!parse_int(argv[i + 1], p)) {
                std::fprintf(stderr, "Invalid value for --reinspect-passes (expected 1-8)\n");
                return 2;
            }
            options.reinspectPasses = p;
            ++i;
        } else if (arg == "--no-legacy") {
            options.includeLegacy = false;
        } else if (arg == "--no-boundary-check") {
            options.checkCliGuiBoundaries = false;
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            return 2;
        }
    }

    const RawrXD::Wiring::AutoMapperReport report = RawrXD::Wiring::IDEWiringAutoMapper::run(options);

    const bool wrote_report = RawrXD::Wiring::IDEWiringAutoMapper::writeJson(report, report_path);
    const bool wrote_submission = RawrXD::Wiring::IDEWiringAutoMapper::writeSubmissionJson(report, submission_path);

    std::fprintf(stdout,
                 "WIRING_AUDIT features=%zu files=%zu missing_source=%zu boundary_gaps=%zu recommendations=%zu\n",
                 report.featuresScanned,
                 report.filesScanned,
                 report.missingSourceCount,
                 report.boundaryGapCount,
                 report.recommendationCount);

    std::fprintf(stdout, "REPORT %s %s\n", wrote_report ? "OK" : "FAIL", report_path.c_str());
    std::fprintf(stdout,
                 "SUBMISSION %s %s\n",
                 wrote_submission ? "OK" : "FAIL",
                 submission_path.c_str());

    return (wrote_report && wrote_submission) ? 0 : 1;
}
