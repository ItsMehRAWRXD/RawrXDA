#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include "digestion_reverse_engineering.h"
#include <QDebug>
#include <QJsonDocument>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("RawrXD-Digestion-Engine");
    app.setApplicationVersion("1.0.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Enterprise code digestion and stub remediation system");
    parser.addHelpOption();
    parser.addVersionOption();
    
    parser.addPositionalArgument("directory", "Root directory to scan");
    
    QCommandLineOption maxFilesOpt(QStringList() << "m" << "max-files", 
        "Maximum files to scan (0=unlimited)", "count", "0");
    QCommandLineOption chunkSizeOpt(QStringList() << "c" << "chunk-size", 
        "Files per parallel chunk", "size", "50");
    QCommandLineOption maxTasksOpt(QStringList() << "t" << "max-tasks-per-file", 
        "Max stubs to fix per file (0=all)", "count", "0");
    QCommandLineOption applyOpt(QStringList() << "a" << "apply", 
        "Apply agentic fixes (default: dry-run)");
    QCommandLineOption gitModeOpt(QStringList() << "g" << "git-mode", 
        "Only scan git-modified files");
    QCommandLineOption incrementalOpt(QStringList() << "i" << "incremental", 
        "Use hash cache for incremental scanning");
    QCommandLineOption cacheFileOpt(QStringList() << "cache-file", 
        "Cache file for incremental mode", "path", ".digestion_cache.json");
    QCommandLineOption threadsOpt(QStringList() << "j" << "threads", 
        "Thread count (0=auto)", "count", "0");
    QCommandLineOption excludeOpt(QStringList() << "e" << "exclude", 
        "Exclude pattern (regex)", "pattern");
    QCommandLineOption backupDirOpt(QStringList() << "b" << "backup-dir", 
        "Backup directory", "path", ".digest_backups");
    QCommandLineOption reportOpt(QStringList() << "r" << "report", 
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
    
    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(1);
    }
    
    QString rootDir = args[0];
    
    DigestionConfig config;
    config.maxFiles = parser.value(maxFilesOpt).toInt();
    config.chunkSize = parser.value(chunkSizeOpt).toInt();
    config.maxTasksPerFile = parser.value(maxTasksOpt).toInt();
    config.applyExtensions = parser.isSet(applyOpt);
    config.useGitMode = parser.isSet(gitModeOpt);
    config.incremental = parser.isSet(incrementalOpt);
    config.threadCount = parser.value(threadsOpt).toInt();
    config.backupDir = parser.value(backupDirOpt);
    
    if (parser.isSet(excludeOpt)) {
        config.excludePatterns << parser.value(excludeOpt);
    }
    
    DigestionReverseEngineeringSystem digester;
    
    // Progress reporting
    QObject::connect(&digester, &DigestionReverseEngineeringSystem::progressUpdate,
        [](int done, int total, int stubs, int percent) {
            fprintf(stderr, "\r[%3d%%] Scanned: %d/%d | Stubs: %d", percent, done, total, stubs);
            fflush(stderr);
        });
    
    QObject::connect(&digester, &DigestionReverseEngineeringSystem::pipelineFinished,
        [&app, &parser](const QJsonObject &report, qint64 elapsed) {
            fprintf(stderr, "\n\nPipeline complete in %.2f seconds\n", elapsed / 1000.0);
            
            int stubs = report["statistics"].toObject()["stubs_found"].toInt();
            int applied = report["statistics"].toObject()["extensions_applied"].toInt();
            
            printf("Files scanned: %d\n", report["statistics"].toObject()["scanned_files"].toInt());
            printf("Stubs found: %d\n", stubs);
            printf("Fixes applied: %d\n", applied);
            
            QString reportPath = parser.value(reportOpt);
            QFile reportFile(reportPath);
            if (reportFile.open(QIODevice::WriteOnly)) {
                reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
                printf("Report saved to: %s\n", qPrintable(reportPath));
            }
            
            app.quit();
        });
    
    QObject::connect(&digester, &DigestionReverseEngineeringSystem::errorOccurred,
        [](const QString &file, const QString &error) {
            fprintf(stderr, "\n[ERROR] %s: %s\n", qPrintable(file), qPrintable(error));
        });
    
    // Load cache if incremental
    if (config.incremental) {
        digester.loadHashCache(parser.value(cacheFileOpt));
    }
    
    // Run
    QTimer::singleShot(0, [&]() {
        digester.runFullDigestionPipeline(rootDir, config);
        
        // Save cache
        if (config.incremental) {
            digester.saveHashCache(parser.value(cacheFileOpt));
        }
    });
    
    return app.exec();
}
