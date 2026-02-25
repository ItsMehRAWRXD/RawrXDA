// ============================================================================
// digestion_cli.cpp — CLI entry-point for Digestion Engine (C++20, no Qt)
// ============================================================================
#include "digestion_reverse_engineering.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>

static void printUsage(const char* progName) {
    fprintf(stderr,
        "RawrXD Digestion Engine v1.0.0\n"
        "Enterprise code digestion and stub remediation system\n\n"
        "Usage: %s [options] <directory>\n\n"
        "Options:\n"
        "  -m, --max-files <count>       Maximum files to scan (0=unlimited) [default: 0]\n"
        "  -c, --chunk-size <size>        Files per parallel chunk [default: 50]\n"
        "  -t, --max-tasks-per-file <n>   Max stubs to fix per file (0=all) [default: 0]\n"
        "  -a, --apply                    Apply agentic fixes (default: dry-run)\n"
        "  -g, --git-mode                 Only scan git-modified files\n"
        "  -i, --incremental              Use hash cache for incremental scanning\n"
        "  --cache-file <path>            Cache file for incremental mode [default: .digestion_cache.json]\n"
        "  -j, --threads <count>          Thread count (0=auto) [default: 0]\n"
        "  -e, --exclude <pattern>        Exclude pattern (regex)\n"
        "  -b, --backup-dir <path>        Backup directory [default: .digest_backups]\n"
        "  -r, --report <path>            Report output file [default: digestion_report.json]\n"
        "  -h, --help                     Show this help\n"
        "  -v, --version                  Show version\n",
        progName);
}

static bool matchArg(const char* arg, const char* shortOpt, const char* longOpt) {
    return (strcmp(arg, shortOpt) == 0) || (strcmp(arg, longOpt) == 0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Defaults
    DigestionConfig config;
    std::string rootDir;
    std::string cacheFile = ".digestion_cache.json";
    std::string reportPath = "digestion_report.json";

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (matchArg(arg, "-h", "--help")) {
            printUsage(argv[0]);
            return 0;
        }
        if (matchArg(arg, "-v", "--version")) {
            printf("RawrXD-Digestion-Engine 1.0.0\n");
            return 0;
        }
        if (matchArg(arg, "-m", "--max-files") && i + 1 < argc) {
            config.maxFiles = atoi(argv[++i]);
            continue;
        }
        if (matchArg(arg, "-c", "--chunk-size") && i + 1 < argc) {
            config.chunkSize = atoi(argv[++i]);
            continue;
        }
        if (matchArg(arg, "-t", "--max-tasks-per-file") && i + 1 < argc) {
            config.maxTasksPerFile = atoi(argv[++i]);
            continue;
        }
        if (matchArg(arg, "-a", "--apply")) {
            config.applyExtensions = true;
            continue;
        }
        if (matchArg(arg, "-g", "--git-mode")) {
            config.useGitMode = true;
            continue;
        }
        if (matchArg(arg, "-i", "--incremental")) {
            config.incremental = true;
            continue;
        }
        if (strcmp(arg, "--cache-file") == 0 && i + 1 < argc) {
            cacheFile = argv[++i];
            continue;
        }
        if (matchArg(arg, "-j", "--threads") && i + 1 < argc) {
            config.threadCount = atoi(argv[++i]);
            continue;
        }
        if (matchArg(arg, "-e", "--exclude") && i + 1 < argc) {
            config.excludePatterns.push_back(argv[++i]);
            continue;
        }
        if (matchArg(arg, "-b", "--backup-dir") && i + 1 < argc) {
            config.backupDir = argv[++i];
            continue;
        }
        if (matchArg(arg, "-r", "--report") && i + 1 < argc) {
            reportPath = argv[++i];
            continue;
        }

        // Positional argument — root directory
        if (arg[0] != '-') {
            rootDir = arg;
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", arg);
        printUsage(argv[0]);
        return 1;
    }

    if (rootDir.empty()) {
        fprintf(stderr, "Error: no directory specified\n");
        printUsage(argv[0]);
        return 1;
    }

    DigestionReverseEngineeringSystem digester;

    // Load cache if incremental
    if (config.incremental) {
        digester.loadHashCache(cacheFile);
    }

    printf("RawrXD Digestion Engine starting...\n");
    printf("  Root:        %s\n", rootDir.c_str());
    printf("  Max files:   %d\n", config.maxFiles);
    printf("  Chunk size:  %d\n", config.chunkSize);
    printf("  Apply fixes: %s\n", config.applyExtensions ? "yes" : "no (dry-run)");
    printf("  Git mode:    %s\n", config.useGitMode ? "yes" : "no");
    printf("  Incremental: %s\n", config.incremental ? "yes" : "no");
    printf("  Threads:     %d (0=auto)\n", config.threadCount);
    fflush(stdout);

    auto startTime = std::chrono::steady_clock::now();

    // Run pipeline
    digester.runFullDigestionPipeline(rootDir, config);

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Print results
    DigestionStats stats = digester.stats();
    printf("\n===== Digestion Complete =====\n");
    printf("Files scanned:  %d\n", stats.scannedFiles.load());
    printf("Stubs found:    %d\n", stats.stubsFound.load());
    printf("Fixes applied:  %d\n", stats.extensionsApplied.load());
    printf("Errors:         %d\n", stats.errors.load());
    printf("Elapsed:        %lld ms\n", static_cast<long long>(elapsedMs));

    // Save cache if incremental
    if (config.incremental) {
        digester.saveHashCache(cacheFile);
        printf("Cache saved to: %s\n", cacheFile.c_str());
    }

    printf("Report:         %s\n", reportPath.c_str());
    fflush(stdout);

    return 0;
}

