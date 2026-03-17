#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>

#include "AICodeIntelligence.hpp"
// New dependency-free analyzer module (AICI)
#include "aici/indexer.hpp"

static void print_usage(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0 << " --root <path> [--report <type>] [options]\n\n"
              << "Reports:\n"
              << "  health             Generate project health summary (existing engine)\n"
              << "  security           Generate security summary (existing engine)\n"
              << "  performance        Generate performance summary (existing engine)\n"
              << "  maintainability    Generate maintainability summary (existing engine)\n"
              << "  metrics            Summary metrics (AICI)\n"
              << "  symbols            List symbols (AICI)\n"
              << "  references         List call references (AICI)\n"
              << "  findings           List security findings (AICI)\n"
              << "  json|ndjson|sarif  Structured outputs (AICI)\n\n"
              << "Options (AICI): --ext <.ext> --include <glob> --exclude <glob> --threads <N> --no-default-excludes\n"
              << "Defaults to 'health' when --report is omitted.\n" << std::endl;
}

int main(int argc, char** argv) {
    std::string root;
    std::string report = "health";
    AICIIndexOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root = argv[++i];
        } else if (arg == "--report" && i + 1 < argc) {
            report = argv[++i];
        } else if (arg == "--ext" && i + 1 < argc) {
            opts.exts.push_back(argv[++i]);
        } else if (arg == "--include" && i + 1 < argc) {
            opts.include.push_back(argv[++i]);
        } else if (arg == "--exclude" && i + 1 < argc) {
            opts.exclude.push_back(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            opts.threads = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--no-default-excludes") {
            opts.useDefaultExcludes = false;
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
    AICIIndexer ix;

    try {
        // AICI extended reports
        if (report == "json" || report == "ndjson" || report == "sarif" || report == "symbols" || report == "references" || report == "findings" || report == "metrics") {
            if (opts.exts.empty()) opts.exts = {".c",".h",".cpp",".hpp",".cc",".hh",".cxx",".hxx",".js",".ts",".py"};
            auto idx = ix.index_root(root, opts);
            if (report == "json") { std::string s; idx.to_json(s); std::cout << s; return 0; }
            if (report == "ndjson") { std::string s; idx.to_ndjson(s); std::cout << s; return 0; }
            if (report == "sarif") { std::string s; idx.to_sarif(s); std::cout << s; return 0; }
            if (report == "symbols") { for (const auto& kv : idx.symbols_by_name()) for (const auto& s : kv.second) std::cout << s.kind << " " << s.name << " @ " << s.file << ":" << s.line << "\n"; return 0; }
            if (report == "references") { for (const auto& r : idx.references()) std::cout << r.symbol << "() @ " << r.file << ":" << r.line << "\n"; return 0; }
            if (report == "findings") { for (const auto& f : idx.findings()) std::cout << f.severity << " " << f.id << " " << f.title << " @ " << f.file << ":" << f.line << "\n"; return 0; }
            // metrics summary
            int files = 0, lines = 0, code = 0, comments = 0, complexity = 0;
            for (const auto& mkv : idx.metrics_by_file()) { files++; lines += mkv.second.lines; code += mkv.second.codeLines; comments += mkv.second.commentLines; complexity += mkv.second.complexity; }
            std::cout << "Files: " << files << ", Lines: " << lines << ", Code: " << code << ", Comments: " << comments << ", AvgComplexity: " << (files ? (complexity / files) : 0) << "\n";
            return 0;
        } else if (report == "security") {
            auto m = ai.generateSecurityReport(root);
            std::cout << "security_issues: " << m["security_issues"] << std::endl;
        } else if (report == "performance") {
            auto m = ai.generatePerformanceReport(root);
            std::cout << "performance_issues: " << m["performance_issues"] << std::endl;
        } else if (report == "maintainability") {
            auto m = ai.generateMaintainabilityReport(root);
            std::cout << "maintainability_issues: " << m["maintainability_issues"] << std::endl;
        } else if (report == "predict") {
            size_t files = 0;
            for (auto& p : std::filesystem::recursive_directory_iterator(root)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
                    auto m = ai.predictCodeQuality(p.path().string());
                    std::cout << p.path().string() << ": length=" << m["length"] << ", todo_count=" << m["todo_count"] << std::endl;
                    ++files;
                }
            }
            std::cout << "files: " << files << std::endl;
        } else { // health
            auto m = ai.generateCodeHealthReport(root);
            std::cout << "files: " << m["files"] << ", todos: " << m["todos"] << ", status: " << m["status"] << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 3;
    }

    return 0;
}
