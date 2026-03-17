#include "SchematicStudio.h"
#include "IDELogger.h"
#include <windows.h>
#include <vector>
#include <string>

using RawrXD::Schematic::StudioConfig;
using RawrXD::Schematic::RunSchematicStudio;

namespace {
    std::vector<std::string> getArgs() {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i) {
            int len = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
            std::string arg(static_cast<size_t>(len), '\0');
            WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, arg.data(), len, nullptr, nullptr);
            if (!arg.empty() && arg.back() == '\0') arg.pop_back();
            args.push_back(arg);
        }
        LocalFree(argv);
        return args;
    }

    bool parseArgs(const std::vector<std::string>& args, StudioConfig& config) {
        for (size_t i = 1; i < args.size(); ++i) {
            const auto& arg = args[i];
            if (arg == "--input" && i + 1 < args.size()) {
                config.inputPath = args[++i];
            } else if (arg == "--output" && i + 1 < args.size()) {
                config.outputPath = args[++i];
            } else if (arg == "--auto-save") {
                config.autoSave = true;
            } else if (arg == "--no-auto-save") {
                config.autoSave = false;
            } else if (arg == "--auto-save-ms" && i + 1 < args.size()) {
                config.autoSaveMs = std::max(100, atoi(args[++i].c_str()));
            } else if (arg == "--no-labels") {
                config.showLabels = false;
            }
        }

        if (config.inputPath.empty()) {
            return false;
        }
        if (config.outputPath.empty()) {
            config.outputPath = config.inputPath;
        }
        return true;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    IDELogger::getInstance().initialize("SchematicStudio.log");

    StudioConfig config;
    auto args = getArgs();
    if (!parseArgs(args, config)) {
        MessageBoxA(nullptr,
            "Usage: RawrXD-SchematicStudio.exe --input <layout.json> [--output <layout.json>] [--auto-save] [--auto-save-ms <ms>] [--no-labels]",
            "Schematic Studio",
            MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    return RunSchematicStudio(config, hInstance);
}
