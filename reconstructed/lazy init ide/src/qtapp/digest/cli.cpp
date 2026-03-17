#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QDebug>
#include <QTextStream>
#include "digestion_reverse_engineering_enterprise.h"

class CliRunner : public QObject {
    Q_OBJECT
public:
    CliRunner(QObject *parent = nullptr) : QObject(parent) {}
    
    void run(const QString &dir, int maxFiles, int chunkSize, int maxTasks, bool apply, 
             const QString &checkpoint, bool resume, const QStringList &excludes) {
        
        connect(&digester, &DigestionReverseEngineeringSystem::fileScanned, 
                [](const QString &path, int stubs, const QString &lang) {
            QTextStream out(stdout);
            out << "[" << lang << "] " << path << " (" << stubs << " stubs)\n";
            out.flush();
        });
        
        connect(&digester, &DigestionReverseEngineeringSystem::pipelineFinished, 
                [this](const QJsonObject &report, qint64 elapsed) {
            QTextStream out(stdout);
            out << "\n=== DIGESTION COMPLETE ===\n";
            out << "Duration: " << (elapsed / 1000.0) << "s\n";
            
            QJsonObject stats = report["statistics"].toObject();
            out << "Files scanned: " << stats["scanned_files"].toInt() << "\n";
            out << "Stubs found: " << stats["stubs_found"].toInt() << "\n";
            out << "Extensions applied: " << stats["extensions_applied"].toInt() << "\n";
            out << "Critical stubs: " << stats["critical_stubs"].toInt() << "\n";
            out << "Errors: " << stats["errors"].toInt() << "\n";
            
            out << "\nStubs by language:\n";
            QJsonObject byLang = stats["stubs_by_language"].toObject();
            for (auto it = byLang.begin(); it != byLang.end(); ++it) {
                out << "  " << it.key() << ": " << it.value().toInt() << "\n";
            }
            
            out << "\nReport saved to: digestion_report.json\n";
            out << "CSV report: digestion_report.csv\n";
            
            QCoreApplication::exit(0);
        });
        
        connect(&digester, &DigestionReverseEngineeringSystem::errorOccurred,
                [](const QString &file, const QString &err, bool critical) {
            QTextStream err_out(stderr);
            if (critical) {
                err_out << "[CRITICAL] " << file << " - " << err << "\n";
            } else {
                err_out << "[ERROR] " << file << " - " << err << "\n";
            }
            err_out.flush();
        });
        
        connect(&digester, &DigestionReverseEngineeringSystem::progressUpdate,
                [](int done, int total, int stubs, int percent) {
            QTextStream out(stdout);
            out << "\rProgress: " << done << "/" << total << " files | " 
                << stubs << " stubs | " << percent << "%    ";
            out.flush();
        });
        
        if (resume && !checkpoint.isEmpty()) {
            QTextStream out(stdout);
            out << "Resuming from checkpoint: " << checkpoint << "\n";
            out.flush();
            digester.resumeFromCheckpoint(checkpoint, apply);
        } else {
            QTextStream out(stdout);
            out << "Starting digestion of: " << dir << "\n";
            out << "Max files: " << (maxFiles > 0 ? QString::number(maxFiles) : "unlimited") << "\n";
            out << "Chunk size: " << chunkSize << "\n";
            out << "Apply extensions: " << (apply ? "yes" : "no") << "\n";
            out << "Excludes: " << (excludes.isEmpty() ? "none" : excludes.join(", ")) << "\n";
            out << "\n";
            out.flush();
            
            digester.runFullDigestionPipeline(dir, maxFiles, chunkSize, maxTasks, apply, excludes);
        }
    }

private:
    DigestionReverseEngineeringSystem digester;
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("RawrXD-DigestionEngine");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Multi-language codebase digestion and stub elimination system");
    parser.addHelpOption();
    parser.addVersionOption();
    
    parser.addPositionalArgument("directory", "Root directory to scan");
    
    QCommandLineOption maxFilesOpt(QStringList() << "m" << "max-files", 
                                   "Maximum files to scan (0=unlimited)", "0");
    QCommandLineOption chunkOpt(QStringList() << "c" << "chunk-size", 
                                "Files per processing chunk", "50");
    QCommandLineOption tasksOpt(QStringList() << "t" << "max-tasks", 
                                "Max stubs to fix per file (0=all)", "0");
    QCommandLineOption applyOpt(QStringList() << "a" << "apply", 
                                "Apply agentic extensions to files");
    QCommandLineOption excludeOpt(QStringList() << "e" << "exclude", 
                                  "Patterns to exclude (comma-separated)", "");
    QCommandLineOption checkpointOpt(QStringList() << "s" << "save-checkpoint", 
                                     "Save checkpoint to path", "");
    QCommandLineOption resumeOpt(QStringList() << "r" << "resume", 
                                 "Resume from checkpoint");
    
    parser.addOption(maxFilesOpt);
    parser.addOption(chunkOpt);
    parser.addOption(tasksOpt);
    parser.addOption(applyOpt);
    parser.addOption(excludeOpt);
    parser.addOption(checkpointOpt);
    parser.addOption(resumeOpt);
    
    parser.process(app);
    
    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(1);
    }
    
    QString dir = args[0];
    int maxFiles = parser.value(maxFilesOpt).toInt();
    int chunkSize = parser.value(chunkOpt).toInt();
    int maxTasks = parser.value(tasksOpt).toInt();
    bool apply = parser.isSet(applyOpt);
    bool resume = parser.isSet(resumeOpt);
    QString checkpoint = parser.value(checkpointOpt);
    QStringList excludes = parser.value(excludeOpt).split(',', Qt::SkipEmptyParts);
    
    CliRunner runner;
    QTimer::singleShot(0, [&]() {
        runner.run(dir, maxFiles, chunkSize, maxTasks, apply, checkpoint, resume, excludes);
    });
    
    return app.exec();
}

#include "digest_cli.moc"
