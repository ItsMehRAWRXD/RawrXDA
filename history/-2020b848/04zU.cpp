#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

#include "AICodeIntelligence.hpp"

static void print_usage(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0 << " --root <path> [--report <type>]\n\n"
              << "Reports:\n"
              << "  security           Generate security report\n"
              << "  performance        Generate performance report\n"
              << "  maintainability    Generate maintainability report\n"
              << "  health             Generate project health summary\n"
              << "  metrics            Print simple metrics per file (lines, characters)\n"
              << "  predict            Print code quality predictions per file\n"
              << "Defaults to 'health' when --report is omitted.\n" << std::endl;
}

int main(int argc, char** argv) {
    std::string root;
    std::string report = "health";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root = argv[++i];
        } else if (arg == "--report" && i + 1 < argc) {
            report = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (root.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    if (!std::filesystem::exists(root)) {
        std::cerr << "Root path does not exist: " << root << std::endl;
        return 2;
    }

    AICodeIntelligence ai;

    try {
        if (report == "security") {
            auto m = ai.generateSecurityReport(root);
            std::cout << "security_issues: " << m["security_issues"] << std::endl;
        } else if (report == "performance") {
            auto m = ai.generatePerformanceReport(root);
            std::cout << "performance_issues: " << m["performance_issues"] << std::endl;
        } else if (report == "maintainability") {
            auto m = ai.generateMaintainabilityReport(root);
            std::cout << "maintainability_issues: " << m["maintainability_issues"] << std::endl;
        } else if (report == "metrics") {
            size_t files = 0;
            for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
                    auto m = ai.calculateCodeMetrics(p.path().string());
                    std::cout << p.path().string() << ": lines=" << m["lines"] << ", chars=" << m["characters"] << std::endl;
                    ++files;
                }
            }
            std::cout << "files: " << files << std::endl;
        } else if (report == "predict") {
            size_t files = 0;
            for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
                    auto m = ai.predictCodeQuality(p.path().string());
                    std::cout << p.path().string() << ": lines=" << m["lines"] << ", has_todo=" << m["has_todo"] << std::endl;
                    ++files;
                }
            }
            std::cout << "files: " << files << std::endl;
        } else { // health
            auto m = ai.generateCodeHealthReport(root);
            std::cout << "total_insights: " << m["total_insights"] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 3;
    }

    return 0;
}
