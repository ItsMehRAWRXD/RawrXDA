#include "WiringOracle.h"
#include "../win32app/IDELogger.h"
#include <windows.h>
#include <string>
#include <vector>

using RawrXD::Tools::WiringOracle;

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

    bool parseArgs(const std::vector<std::string>& args, std::string& sourceRoot, std::string& outputPath) {
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i] == "--source" && i + 1 < args.size()) {
                sourceRoot = args[++i];
            } else if (args[i] == "--out" && i + 1 < args.size()) {
                outputPath = args[++i];
            }
        }
        return !sourceRoot.empty() && !outputPath.empty();
    }
}

int main()
{
    IDELogger::getInstance().initialize("WiringOracle.log");

    std::string sourceRoot;
    std::string outputPath;
    auto args = getArgs();
    if (!parseArgs(args, sourceRoot, outputPath)) {
        MessageBoxA(nullptr,
            "Usage: WiringOracle.exe --source <repo-root> --out <report.json>",
            "Wiring Oracle",
            MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    WiringOracle oracle(sourceRoot);
    auto report = oracle.analyze();

    if (!oracle.writeReport(outputPath, report)) {
        return 2;
    }

    return 0;
}
