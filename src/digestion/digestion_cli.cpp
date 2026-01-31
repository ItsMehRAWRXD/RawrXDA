#include "digestion_reverse_engineering.h"
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("RawrXD-Digestion-Engine");
    app.setApplicationVersion("1.0.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Enterprise code digestion and stub remediation system");
    parser.addHelpOption();
    parser.addVersionOption();
    
    parser.addPositionalArgument("directory", "Root directory to scan");
    
    QCommandLineOption maxFilesOpt(std::stringList() << "m" << "max-files", 
        "Maximum files to scan (0=unlimited)", "count", "0");
    QCommandLineOption chunkSizeOpt(std::stringList() << "c" << "chunk-size", 
        "Files per parallel chunk", "size", "50");
    QCommandLineOption maxTasksOpt(std::stringList() << "t" << "max-tasks-per-file", 
        "Max stubs to fix per file (0=all)", "count", "0");
    QCommandLineOption applyOpt(std::stringList() << "a" << "apply", 
        "Apply agentic fixes (default: dry-run)");
    QCommandLineOption gitModeOpt(std::stringList() << "g" << "git-mode", 
        "Only scan git-modified files");
    QCommandLineOption incrementalOpt(std::stringList() << "i" << "incremental", 
        "Use hash cache for incremental scanning");
    QCommandLineOption cacheFileOpt(std::stringList() << "cache-file", 
        "Cache file for incremental mode", "path", ".digestion_cache.json");
    QCommandLineOption threadsOpt(std::stringList() << "j" << "threads", 
        "Thread count (0=auto)", "count", "0");
    QCommandLineOption excludeOpt(std::stringList() << "e" << "exclude", 
        "Exclude pattern (regex)", "pattern");
    QCommandLineOption backupDirOpt(std::stringList() << "b" << "backup-dir", 
        "Backup directory", "path", ".digest_backups");
    QCommandLineOption reportOpt(std::stringList() << "r" << "report", 
        "Report output file", "path", "digestion_report.json");
    
    parser.addOption(maxFilesOpt);
    parser.addOption(chunkSizeOpt);
    parser.addOption(maxTasksOpt);
    parser.addOption(applyOpt);
    parser.addOption(gitModeOpt);
    parser.addOption(incrementalOpt);
    parser.addOption(cacheFileOpt);
    parser.addOption(threadsOpt);
    parser.addOption(excludeOpt);
    parser.addOption(backupDirOpt);
    parser.addOption(reportOpt);
    
    parser.process(app);
    
    const std::stringList args = parser.positionalArguments();
    if (args.empty()) {
        parser.showHelp(1);
    }
    
    std::string rootDir = args[0];
    
    DigestionConfig config;
    config.maxFiles = parser.value(maxFilesOpt);
    config.chunkSize = parser.value(chunkSizeOpt);
    config.maxTasksPerFile = parser.value(maxTasksOpt);
    config.applyExtensions = parser.isSet(applyOpt);
    config.useGitMode = parser.isSet(gitModeOpt);
    config.incremental = parser.isSet(incrementalOpt);
    config.threadCount = parser.value(threadsOpt);
    config.backupDir = parser.value(backupDirOpt);
    
    if (parser.isSet(excludeOpt)) {
        config.excludePatterns << parser.value(excludeOpt);
    }
    
    DigestionReverseEngineeringSystem digester;
    
    // Progress reporting
    // Object::  // Signal connection removed\nfflush(stderr);
        });
    
    // Object::  // Signal connection removed\nint stubs = report["statistics"].toObject()["stubs_found"];
            int applied = report["statistics"].toObject()["extensions_applied"];
            
            printf("Files scanned: %d\n", report["statistics"].toObject()["scanned_files"]);
            printf("Stubs found: %d\n", stubs);
            printf("Fixes applied: %d\n", applied);
            
            std::string reportPath = parser.value(reportOpt);
            // File operation removed;
            if (reportFile.open(std::iostream::WriteOnly)) {
                reportFile.write(void*(report).toJson(void*::Indented));
                printf("Report saved to: %s\n", qPrintable(reportPath));
            }
            
            app.quit();
        });
    
    // Object::  // Signal connection removed\n});
    
    // Load cache if incremental
    if (config.incremental) {
        digester.loadHashCache(parser.value(cacheFileOpt));
    }
    
    // Run
    // Timer::singleShot(0, [&]() {
        digester.runFullDigestionPipeline(rootDir, config);
        
        // Save cache
        if (config.incremental) {
            digester.saveHashCache(parser.value(cacheFileOpt));
        }
    });
    
    return app.exec();
}

