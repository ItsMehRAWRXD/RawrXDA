#include <windows.h>
#include <string>
#include <vector>

extern "C" int Test_Init(void);
extern "C" int Test_Discover(const char* pTestRunnerPath, const char* pDiscoverArgs);
extern "C" int Test_Run(const char* pCmdLine);

namespace {
static std::string wideToUtf8(const wchar_t* w) {
    if (!w || !*w) return {};
    int bytes = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string out(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), bytes, nullptr, nullptr);
    return out;
}
}

extern "C" __declspec(dllexport) int RawrXD_RunExternalTestsW(const wchar_t* runnerW,
                                                                const wchar_t* argsW) {
    // Ensure test subsystem is initialized.
    if (Test_Init() != 0) {
        return -1;
    }

    // No runner provided => execute built-in smoke tests.
    if (!runnerW || !*runnerW) {
        return Test_Run(nullptr) ? 0 : -1;
    }

    const std::string runner = wideToUtf8(runnerW);
    if (runner.empty()) {
        return -1;
    }

    std::string args = wideToUtf8(argsW);
    if (args.empty()) {
        args = "--list-tests";
    }

    // Discovery pass wires test explorer tree before execution.
    Test_Discover(runner.c_str(), args.c_str());

    // Build final command line for execution.
    std::string cmd = "\"" + runner + "\"";
    if (!args.empty()) {
        cmd.push_back(' ');
        cmd += args;
    }

    // Test_Run expects mutable C-string command line for CreateProcessA.
    std::vector<char> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back('\0');

    return Test_Run(mutableCmd.data()) ? 0 : -1;
}
