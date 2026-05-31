// ============================================================================
// model_puller_cli.cpp — CLI Command Integration Implementation
// ============================================================================
// Full interactive CLI for the RawrXD Model Puller.
// Drives the ModelPuller API with rich console output.
// ============================================================================

#include "model_puller/model_puller_cli.h"
#include "model_puller/model_puller.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {

// ============================================================================
// Format helpers
// ============================================================================
std::string ModelPullerCLI::FormatSize(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f GB",
            static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
            static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB",
            static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return buf;
}

std::string ModelPullerCLI::FormatSpeed(double bytesPerSec) {
    char buf[64];
    if (bytesPerSec >= 1024.0 * 1024.0) {
        std::snprintf(buf, sizeof(buf), "%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
    } else if (bytesPerSec >= 1024.0) {
        std::snprintf(buf, sizeof(buf), "%.1f KB/s", bytesPerSec / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f B/s", bytesPerSec);
    }
    return buf;
}

std::string ModelPullerCLI::FormatETA(int seconds) {
    if (seconds < 0) return "??:??:??";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return buf;
}

// ============================================================================
// Progress bar — write inline with \r
// ============================================================================
void ModelPullerCLI::PrintProgressBar(double percent, double speedMBps, int etaSeconds) {
    // [████████░░░░] 45% (8.3/18.5 GB) 12.4 MB/s ETA 00:13:22
    int barWidth = 30;
    int filled = static_cast<int>(percent / 100.0 * barWidth);
    if (filled > barWidth) filled = barWidth;
    if (filled < 0) filled = 0;

    std::string bar(static_cast<size_t>(filled), '#');
    bar += std::string(static_cast<size_t>(barWidth - filled), '-');

    char line[256];
    std::snprintf(line, sizeof(line), "\r[%s] %5.1f%% %s ETA %s   ",
        bar.c_str(), percent,
        FormatSpeed(speedMBps * 1024.0 * 1024.0).c_str(),
        FormatETA(etaSeconds).c_str());

    std::cout << line << std::flush;
}

// ============================================================================
// PrintHelp
// ============================================================================
void ModelPullerCLI::PrintHelp() {
    std::cout << R"(
RawrXD Model Manager
====================

Usage: rawrxd model <command> [arguments]

Commands:
  pull <source>         Download a model
                          HuggingFace: bartowski/Qwen2.5-Coder-32B-Instruct-GGUF:Q4_K_M
                          native:      llama3.2:3b
                          URL:         https://host.com/model.gguf
                          Local:       C:\models\my_model.gguf

  list <repo>           List available GGUF files in a HuggingFace repo
                          rawrxd model list bartowski/Qwen2.5-Coder-32B-Instruct-GGUF

  list-local            Show all locally registered models

  search <query>        Search HuggingFace for GGUF models
                          rawrxd model search "qwen coder 32b"

  info <id>             Show details for a local model

  set-active <id>       Set a model as the active/default model

  remove <id>           Remove a model from the registry
                          --delete  Also delete the file from disk

  register <path>       Register an existing .gguf file
                          rawrxd model register C:\models\codestral.gguf

Options:
  --hf-token <token>    Hugging Face API token (for gated models)
  --models-dir <path>   Override models storage directory
  --help, -h            Show this help
)" << std::endl;
}

// ============================================================================
// Run — dispatch subcommand
// ============================================================================
int ModelPullerCLI::Run(int argc, const char* argv[]) {
    // argv[0] = "model"
    if (argc < 2) {
        PrintHelp();
        return 1;
    }

    std::string cmd = argv[1];

    // Parse global flags
    std::vector<std::string> args;
    std::string hfToken;
    std::string modelsDir;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--hf-token") && i + 1 < argc) {
            hfToken = argv[++i];
        } else if ((arg == "--models-dir") && i + 1 < argc) {
            modelsDir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintHelp();
            return 0;
        } else {
            args.push_back(arg);
        }
    }

    // Apply global config
    if (!hfToken.empty()) {
        ModelPuller::Instance().SetHFToken(hfToken);
    }
    if (!modelsDir.empty()) {
        ModelPuller::Instance().SetModelsBasePath(modelsDir);
    }

    // Enable UTF-8 console output on Windows
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    if (cmd == "pull")       return CmdPull(args);
    if (cmd == "list")       return CmdList(args);
    if (cmd == "list-local") return CmdListLocal(args);
    if (cmd == "search")     return CmdSearch(args);
    if (cmd == "info")       return CmdInfo(args);
    if (cmd == "set-active") return CmdSetActive(args);
    if (cmd == "remove")     return CmdRemove(args);
    if (cmd == "register")   return CmdRegister(args);

    std::cerr << "Unknown subcommand: " << cmd << "\n";
    PrintHelp();
    return 1;
}

// ============================================================================
// CmdPull
// ============================================================================
int ModelPullerCLI::CmdPull(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model pull <source>\n";
        return 1;
    }

    std::string source = args[0];
    std::cout << "\n";

    PullResult result = ModelPuller::Instance().Pull(source,
        [](const PullStatus& status) {
            switch (status.step) {
                case PullStep::FetchingFileList:
                    std::cout << "[" << status.stepNumber << "/4] " << status.stepDescription << "\n";
                    break;
                case PullStep::ResolvingQuantization:
                    std::cout << "[" << status.stepNumber << "/4] " << status.stepDescription << "\n";
                    break;
                case PullStep::Downloading: {
                    auto& p = status.downloadProgress;
                    double pct = p.progressPercent;
                    PrintProgressBar(pct, p.speedBytesPerSec / (1024.0 * 1024.0), p.etaSeconds);
                    break;
                }
                case PullStep::Verifying:
                    std::cout << "\n[" << status.stepNumber << "/4] " << status.stepDescription << "\n";
                    break;
                case PullStep::Registering:
                    std::cout << "[" << status.stepNumber << "/4] " << status.stepDescription << "\n";
                    break;
                case PullStep::Complete:
                    std::cout << "\n";
                    break;
                case PullStep::Failed:
                    std::cout << "\n[FAIL] " << status.stepDescription << "\n";
                    break;
                default:
                    break;
            }
        });

    if (result.success) {
        std::cout << "Model ID:   " << result.modelId << "\n";
        std::cout << "File:       " << result.filePath << "\n";
        std::cout << "Size:       " << FormatSize(result.sizeBytes) << "\n";
        std::cout << "SHA256:     " << result.sha256 << "\n";
        return 0;
    } else {
        std::cerr << "Pull failed: " << result.error << "\n";
        return 1;
    }
}

// ============================================================================
// CmdList — list GGUF files in a HF repo
// ============================================================================
int ModelPullerCLI::CmdList(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model list <repo>\n";
        return 1;
    }

    std::vector<HFFileInfo> files = ModelPuller::Instance().ListQuantizations(args[0]);

    if (files.empty()) {
        std::cerr << "No GGUF files found in " << args[0] << "\n";
        return 1;
    }

    // Print table
    std::cout << "\n";
    std::cout << std::left << std::setw(50) << "FILENAME"
              << std::right << std::setw(12) << "SIZE"
              << "  " << std::left << std::setw(12) << "QUANT"
              << "\n";
    std::cout << std::string(76, '-') << "\n";

    for (auto& f : files) {
        std::cout << std::left << std::setw(50) << f.filename
                  << std::right << std::setw(12) << FormatSize(f.sizeBytes)
                  << "  " << std::left << std::setw(12) << f.quantization
                  << "\n";
    }

    std::cout << "\n" << files.size() << " GGUF file(s) found.\n";
    std::cout << "Pull with: rawrxd model pull " << args[0] << ":<quantization>\n\n";

    return 0;
}

// ============================================================================
// CmdListLocal
// ============================================================================
int ModelPullerCLI::CmdListLocal(const std::vector<std::string>& /*args*/) {
    auto models = ModelPuller::Instance().ListLocalModels();

    if (models.empty()) {
        std::cout << "\nNo local models registered.\n";
        std::cout << "Pull a model: rawrxd model pull <source>\n\n";
        return 0;
    }

    std::cout << "\n";
    std::cout << std::left
              << std::setw(3)  << " "
              << std::setw(28) << "ID"
              << std::setw(30) << "NAME"
              << std::right << std::setw(10) << "QUANT"
              << std::setw(12) << "SIZE"
              << "  " << std::left << std::setw(10) << "SOURCE"
              << "\n";
    std::cout << std::string(95, '-') << "\n";

    for (auto& m : models) {
        std::string activeMarker = m.active ? " * " : "   ";
        std::string srcShort = m.source;
        if (srcShort.size() > 10) srcShort = srcShort.substr(0, 10) + "...";

        std::cout << std::left
                  << activeMarker
                  << std::setw(28) << (m.id.size() > 27 ? m.id.substr(0, 27) : m.id)
                  << std::setw(30) << (m.name.size() > 29 ? m.name.substr(0, 29) : m.name)
                  << std::right << std::setw(10) << m.quantization
                  << std::setw(12) << FormatSize(m.sizeBytes)
                  << "  " << std::left << srcShort
                  << "\n";
    }

    std::cout << "\n" << models.size() << " model(s) registered. (* = active)\n"
              << "Models dir: " << ModelPuller::Instance().GetIndex().GetModelsBasePath() << "\n\n";

    return 0;
}

// ============================================================================
// CmdSearch
// ============================================================================
int ModelPullerCLI::CmdSearch(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model search <query>\n";
        return 1;
    }

    // Join all args as query
    std::string query;
    for (auto& a : args) {
        if (!query.empty()) query += " ";
        query += a;
    }

    std::cout << "Searching HuggingFace for \"" << query << "\"...\n\n";
    auto results = ModelPuller::Instance().Search(query);

    if (results.empty()) {
        std::cout << "No GGUF models found.\n";
        return 0;
    }

    std::cout << std::left
              << std::setw(55) << "REPO"
              << std::right << std::setw(12) << "DOWNLOADS"
              << std::setw(8) << "LIKES"
              << "\n";
    std::cout << std::string(76, '-') << "\n";

    for (auto& r : results) {
        std::cout << std::left
                  << std::setw(55) << (r.repoId.size() > 54 ? r.repoId.substr(0, 54) : r.repoId)
                  << std::right << std::setw(12) << r.downloads
                  << std::setw(8) << r.likes
                  << "\n";
    }

    std::cout << "\n" << results.size() << " result(s). List files: rawrxd model list <repo>\n\n";

    return 0;
}

// ============================================================================
// CmdInfo
// ============================================================================
int ModelPullerCLI::CmdInfo(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model info <id>\n";
        return 1;
    }

    ModelEntry entry;
    if (!ModelPuller::Instance().GetIndex().GetModel(args[0], entry)) {
        std::cerr << "Model not found: " << args[0] << "\n";
        return 1;
    }

    std::cout << "\n";
    std::cout << "ID:            " << entry.id << "\n";
    std::cout << "Name:          " << entry.name << "\n";
    std::cout << "Quantization:  " << entry.quantization << "\n";
    std::cout << "Size:          " << FormatSize(entry.sizeBytes) << "\n";
    std::cout << "Path:          " << entry.absolutePath << "\n";
    std::cout << "Source:        " << entry.source << "\n";
    std::cout << "SHA256:        " << entry.sha256 << "\n";
    std::cout << "Downloaded:    " << entry.downloadedAt << "\n";
    std::cout << "Architecture:  " << (entry.architecture.empty() ? "(unknown)" : entry.architecture) << "\n";
    std::cout << "Tags:          " << (entry.tags.empty() ? "(none)" : entry.tags) << "\n";
    std::cout << "Active:        " << (entry.active ? "Yes" : "No") << "\n";
    std::cout << "\n";

    return 0;
}

// ============================================================================
// CmdSetActive
// ============================================================================
int ModelPullerCLI::CmdSetActive(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model set-active <id>\n";
        return 1;
    }

    if (ModelPuller::Instance().SetActiveModel(args[0])) {
        std::cout << "Active model set to: " << args[0] << "\n";
        return 0;
    } else {
        std::cerr << "Model not found: " << args[0] << "\n";
        return 1;
    }
}

// ============================================================================
// CmdRemove
// ============================================================================
int ModelPullerCLI::CmdRemove(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model remove <id> [--delete]\n";
        return 1;
    }

    bool deleteFile = false;
    std::string id;
    for (auto& a : args) {
        if (a == "--delete") {
            deleteFile = true;
        } else {
            id = a;
        }
    }

    if (id.empty()) {
        std::cerr << "No model ID specified.\n";
        return 1;
    }

    if (ModelPuller::Instance().RemoveModel(id, deleteFile)) {
        std::cout << "Removed model: " << id;
        if (deleteFile) std::cout << " (file deleted)";
        std::cout << "\n";
        return 0;
    } else {
        std::cerr << "Model not found: " << id << "\n";
        return 1;
    }
}

// ============================================================================
// CmdRegister
// ============================================================================
int ModelPullerCLI::CmdRegister(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "Usage: rawrxd model register <path> [--name <name>] [--tags <tags>]\n";
        return 1;
    }

    std::string path;
    std::string name;
    std::string tags;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--name" && i + 1 < args.size()) {
            name = args[++i];
        } else if (args[i] == "--tags" && i + 1 < args.size()) {
            tags = args[++i];
        } else {
            path = args[i];
        }
    }

    if (path.empty()) {
        std::cerr << "No file path specified.\n";
        return 1;
    }

    if (ModelPuller::Instance().RegisterLocalModel(path, name, tags)) {
        std::cout << "Registered: " << path << "\n";
        return 0;
    } else {
        std::cerr << "Failed to register: " << path << "\n";
        return 1;
    }
}

} // namespace RawrXD
