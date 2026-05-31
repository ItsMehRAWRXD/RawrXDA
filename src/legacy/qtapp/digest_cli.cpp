#include "digestion_reverse_engineering_enterprise.h"

class CliRunner  {

public:
    CliRunner()  {}
    
    void run(const std::string &dir, int maxFiles, int chunkSize, int maxTasks, bool apply, 
             const std::string &checkpoint, bool resume, const std::stringList &excludes) {  // Signal connection removed\nout << "[" << lang << "] " << path << " (" << stubs << " stubs)\n";
            out.flush();
        });  // Signal connection removed\nout << "\n=== DIGESTION COMPLETE ===\n";
            out << "Duration: " << (elapsed / 1000.0) << "s\n";
            
            nlohmann::json stats = report["statistics"].toObject();
            out << "Files scanned: " << stats["scanned_files"] << "\n";
            out << "Stubs found: " << stats["stubs_found"] << "\n";
            out << "Extensions applied: " << stats["extensions_applied"] << "\n";
            out << "Critical stubs: " << stats["critical_stubs"] << "\n";
            out << "Errors: " << stats["errors"] << "\n";
            
            out << "\nStubs by language:\n";
            nlohmann::json byLang = stats["stubs_by_language"].toObject();
            for (auto it = byLang.begin(); it != byLang.end(); ++it) {
                out << "  " << it.key() << ": " << it.value() << "\n";
            }
            
            out << "\nReport saved to: digestion_report.json\n";
            out << "CSV report: digestion_report.csv\n";
            
            QCoreApplication::exit(0);
        });  // Signal connection removed\nif (critical) {
                err_out << "[CRITICAL] " << file << " - " << err << "\n";
            } else {
                err_out << "[ERROR] " << file << " - " << err << "\n";
            }
            err_out.flush();
        });  // Signal connection removed\nout << "\rProgress: " << done << "/" << total << " files | " 
                << stubs << " stubs | " << percent << "%    ";
            out.flush();
        });
        
        if (resume && !checkpoint.empty()) {
            std::stringstream out(stdout);
            out << "Resuming from checkpoint: " << checkpoint << "\n";
            out.flush();
            digester.resumeFromCheckpoint(checkpoint, apply);
        } else {
            std::stringstream out(stdout);
            out << "Starting digestion of: " << dir << "\n";
            out << "Max files: " << (maxFiles > 0 ? std::string::number(maxFiles) : "unlimited") << "\n";
            out << "Chunk size: " << chunkSize << "\n";
            out << "Apply extensions: " << (apply ? "yes" : "no") << "\n";
            out << "Excludes: " << (excludes.empty() ? "none" : excludes.join(", ")) << "\n";
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
    
    QCommandLineOption maxFilesOpt(std::stringList() << "m" << "max-files", 
                                   "Maximum files to scan (0=unlimited)", "0");
    QCommandLineOption chunkOpt(std::stringList() << "c" << "chunk-size", 
                                "Files per processing chunk", "50");
    QCommandLineOption tasksOpt(std::stringList() << "t" << "max-tasks", 
                                "Max stubs to fix per file (0=all)", "0");
    QCommandLineOption applyOpt(std::stringList() << "a" << "apply", 
                                "Apply agentic extensions to files");
    QCommandLineOption excludeOpt(std::stringList() << "e" << "exclude", 
                                  "Patterns to exclude (comma-separated)", "");
    QCommandLineOption checkpointOpt(std::stringList() << "s" << "save-checkpoint", 
                                     "Save checkpoint to path", "");
    QCommandLineOption resumeOpt(std::stringList() << "r" << "resume", 
                                 "Resume from checkpoint");
    
    parser.addOption(maxFilesOpt);
    parser.addOption(chunkOpt);
    parser.addOption(tasksOpt);
    parser.addOption(applyOpt);
    parser.addOption(excludeOpt);
    parser.addOption(checkpointOpt);
    parser.addOption(resumeOpt);
    
    parser.process(app);
    
    const std::stringList args = parser.positionalArguments();
    if (args.empty()) {
        parser.showHelp(1);
    }
    
    std::string dir = args[0];
    int maxFiles = parser.value(maxFilesOpt);
    int chunkSize = parser.value(chunkOpt);
    int maxTasks = parser.value(tasksOpt);
    bool apply = parser.isSet(applyOpt);
    bool resume = parser.isSet(resumeOpt);
    std::string checkpoint = parser.value(checkpointOpt);
    std::stringList excludes = parser.value(excludeOpt).split(',', SkipEmptyParts);
    
    CliRunner runner;
    // Timer::singleShot(0, [&]() {
        runner.run(dir, maxFiles, chunkSize, maxTasks, apply, checkpoint, resume, excludes);
    });
    
    return app.exec();
}

// MOC removed

