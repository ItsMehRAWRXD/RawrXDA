/**
 * RawrXD SafeMode CLI - lightweight command-line fallback for RawrXD IDE accessibility.
 *
 * This limited CLI mirrors the essential monitoring and configuration commands required
 * when the GUI cannot be launched but does not pull in heavyweight backend services
 * like the API server or streaming model loader.
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "gui.h"
#include "telemetry.h"
#include "settings.h"

namespace fs = std::filesystem;

namespace telemetry {
bool Initialize() {
    return true;
}

bool Poll(TelemetrySnapshot& snapshot) {
    using namespace std::chrono;
    auto now = system_clock::now();
    snapshot.timeMs = static_cast<uint64_t>(duration_cast<milliseconds>(now.time_since_epoch()).count());
    snapshot.cpuTempValid = true;
    snapshot.cpuTempC = 52.1;
    snapshot.cpuUsagePercent = 18.7;
    snapshot.cpuPowerValid = true;
    snapshot.cpuPowerW = 24.6;
    snapshot.gpuTempValid = true;
    snapshot.gpuTempC = 49.3;
    snapshot.gpuUsagePercent = 12.1;
    snapshot.gpuPowerValid = true;
    snapshot.gpuPowerW = 17.2;
    snapshot.vramUsedMB = 1988.5;
    snapshot.gpuVendor = "StubVendor";
    return true;
}

void Shutdown() {}
} // namespace telemetry

namespace Color {
constexpr const char* Reset = "\033[0m";
constexpr const char* Red = "\033[31m";
constexpr const char* Green = "\033[32m";
constexpr const char* Yellow = "\033[33m";
constexpr const char* Cyan = "\033[36m";
constexpr const char* Bold = "\033[1m";
constexpr const char* Dim = "\033[2m";
} // namespace Color

struct SafeModeState {
    RawrXD::AppState app_state;
    std::atomic<bool> running{true};
    std::atomic<bool> governor_running{false};
    std::string workspace_root = ".";
    bool verbose{false};
    bool allow_unsafe_paths{false};
    std::string governor_status{"stopped"};
};

static SafeModeState g_state;

void printBanner();
void printHelp();
void printUsage(const char* prog);
void dispatchCommand(const std::string& input);
std::vector<std::string> tokenize(const std::string& input);
std::string joinTokens(const std::vector<std::string>& tokens, size_t start = 0);

inline void printColored(const char* color, const std::string& message) {
    std::cout << color << message << Color::Reset << '\n';
}

inline void printInfo(const std::string& message) {
    printColored(Color::Cyan, message);
}

inline void printSuccess(const std::string& message) {
    printColored(Color::Green, message);
}

inline void printWarning(const std::string& message) {
    printColored(Color::Yellow, message);
}

inline void printError(const std::string& message) {
    printColored(Color::Red, message);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string joinTokens(const std::vector<std::string>& tokens, size_t start) {
    std::ostringstream oss;
    for (size_t i = start; i < tokens.size(); ++i) {
        if (i > start) oss << ' ';
        oss << tokens[i];
    }
    return oss.str();
}

void cmdStatus(const std::vector<std::string>&) {
    std::cout << Color::Cyan << "\n=== SafeMode Status ===" << Color::Reset << '\n';
    std::cout << "  Workspace root: " << (g_state.workspace_root.empty() ? "." : g_state.workspace_root) << '\n';
    std::cout << "  Governor: " << g_state.governor_status << " (" << (g_state.governor_running.load() ? "simulated" : "stopped") << ")" << '\n';
    std::cout << "  Verbose: " << (g_state.verbose ? "on" : "off") << '\n';
    std::cout << "  Unsafe paths: " << (g_state.allow_unsafe_paths ? "allowed" : "blocked") << '\n';
    std::cout << '\n';
}

void cmdTelemetry(const std::vector<std::string>&) {
    telemetry::TelemetrySnapshot snapshot;
    if (!telemetry::Poll(snapshot)) {
        printWarning("Telemetry polling failed");
        return;
    }

    std::cout << Color::Cyan << "\n=== Telemetry ===" << Color::Reset << '\n';
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  CPU Temp: " << (snapshot.cpuTempValid ? std::to_string(snapshot.cpuTempC) + "°C" : "N/A") << '\n';
    std::cout << "  GPU Temp: " << (snapshot.gpuTempValid ? std::to_string(snapshot.gpuTempC) + "°C" : "N/A") << '\n';
    std::cout << "  CPU Util: " << snapshot.cpuUsagePercent << "%" << '\n';
    std::cout << "  GPU Util: " << snapshot.gpuUsagePercent << "%" << '\n';
    std::cout << "  VRAM Used: " << snapshot.vramUsedMB << " MB" << '\n';
    std::cout << "  Vendor: " << snapshot.gpuVendor << '\n';
    std::cout << Color::Reset << '\n';
}

void cmdGovernor(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printError("Usage: governor <start|stop>");
        return;
    }

    auto action = toLower(args[1]);
    if (action == "start") {
        if (g_state.governor_running.load()) {
            printWarning("Governor already running");
            return;
        }

        g_state.governor_running = true;
        g_state.governor_status = "running";
        g_state.app_state.enable_overclock_governor = true;
        printSuccess("Governor simulated as running");
    } else if (action == "stop") {
        if (!g_state.governor_running.load()) {
            printWarning("Governor is not running");
            return;
        }

        g_state.governor_running = false;
        g_state.governor_status = "stopped";
        g_state.app_state.enable_overclock_governor = false;
        printSuccess("Governor stopped");
    } else {
        printError("Unknown governor command: " + args[1]);
    }
}

void cmdWorkspace(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printInfo("Current workspace: " + (g_state.workspace_root.empty() ? "." : g_state.workspace_root));
        return;
    }

    fs::path candidate = args[1];
    if (!fs::exists(candidate)) {
        printError("Path does not exist: " + candidate.string());
        return;
    }

    g_state.workspace_root = fs::absolute(candidate).string();
    printSuccess("Workspace set to: " + g_state.workspace_root);
}

void cmdSettings(const std::vector<std::string>&) {
    std::cout << Color::Cyan << "\n=== Settings ===" << Color::Reset << '\n';
    std::cout << "  Workspace: " << (g_state.workspace_root.empty() ? "." : g_state.workspace_root) << '\n';
    std::cout << "  Verbose: " << (g_state.verbose ? "on" : "off") << '\n';
    std::cout << "  Unsafe paths: " << (g_state.allow_unsafe_paths ? "allowed" : "blocked") << '\n';
    std::cout << "  Governor status: " << g_state.governor_status << '\n';
    std::cout << "  Target all-core: " << g_state.app_state.target_all_core_mhz << " MHz" << '\n';
    std::cout << "  Boost step: " << g_state.app_state.boost_step_mhz << " MHz" << '\n';
    std::cout << '\n';
}

void cmdSave(const std::vector<std::string>&) {
    RawrXD::Settings settings;
    settings.Set("workspace", g_state.workspace_root);
    settings.SetBool("verbose", g_state.verbose);
    settings.SetBool("unsafe_paths", g_state.allow_unsafe_paths);
    settings.SetBool("enable_overclock_governor", g_state.app_state.enable_overclock_governor);
    settings.SetInt("boost_step_mhz", static_cast<int>(g_state.app_state.boost_step_mhz));
    settings.SetInt("target_all_core_mhz", static_cast<int>(g_state.app_state.target_all_core_mhz));
    settings.Set("governor_status", g_state.governor_status);

    if (settings.Save("settings_safemode.json")) {
        printSuccess("Settings saved to settings_safemode.json");
    } else {
        printError("Failed to save settings");
    }
}

void cmdApi(const std::vector<std::string>&) {
    printWarning("API server support is disabled in this SafeMode build");
}

void cmdVerbose(const std::vector<std::string>& args) {
    if (args.size() >= 2) {
        auto desired = toLower(args[1]);
        g_state.verbose = (desired == "on" || desired == "true" || desired == "1");
    } else {
        g_state.verbose = !g_state.verbose;
    }
    printInfo(std::string("Verbose mode: ") + (g_state.verbose ? "on" : "off"));
}

void cmdClear(const std::vector<std::string>&) {
    std::system("cls");
}

void cmdHelp(const std::vector<std::string>&) {
    printHelp();
}

void dispatchCommand(const std::string& input) {
    auto tokens = tokenize(input);
    if (tokens.empty()) return;

    auto cmd = toLower(tokens[0]);
    if (cmd == "help" || cmd == "?") cmdHelp(tokens);
    else if (cmd == "status") cmdStatus(tokens);
    else if (cmd == "telemetry" || cmd == "telem") cmdTelemetry(tokens);
    else if (cmd == "governor" || cmd == "gov") cmdGovernor(tokens);
    else if (cmd == "workspace" || cmd == "ws") cmdWorkspace(tokens);
    else if (cmd == "settings") cmdSettings(tokens);
    else if (cmd == "save") cmdSave(tokens);
    else if (cmd == "verbose" || cmd == "verb") cmdVerbose(tokens);
    else if (cmd == "clear" || cmd == "cls") cmdClear(tokens);
    else if (cmd == "api") cmdApi(tokens);
    else if (cmd == "quit" || cmd == "exit" || cmd == "q") g_state.running = false;
    else printError("Unknown command: " + tokens[0] + ". Type 'help' for a list of commands.");
}

void printBanner() {
    std::cout << Color::Cyan;
    std::cout << " ____                     __  ______  " << '\n';
    std::cout << "|  _ \\ __ ___      ___ __|  \\/  |  _ \\ " << '\n';
    std::cout << "| |_) / _` \\ \\ /\\ / / '__| |\\/| | | | |" << '\n';
    std::cout << "|  _ < (_| |\\ V  V /| |  | |  | | |_| |" << '\n';
    std::cout << "|_| \\_\\__,_| \\_/\\_/ |_|  |_|  |_|____/ " << '\n';
    std::cout << "                                        " << '\n';
    std::cout << Color::Reset;
    std::cout << Color::Bold << Color::Yellow
              << "SafeMode CLI v1.0.0 - GUI-free RawrXD fallback" << '\n'
              << Color::Reset << '\n';
    std::cout << Color::Dim << "Type 'help' for available commands, 'quit' to exit" << Color::Reset << '\n' << '\n';
}

void printHelp() {
    std::cout << Color::Cyan << "\n=== RawrXD SafeMode CLI Commands ===" << Color::Reset << '\n';
    std::cout << "  help               Show this list" << '\n';
    std::cout << "  status             Show workspace / governor status" << '\n';
    std::cout << "  telemetry          Print CPU/GPU telemetry" << '\n';
    std::cout << "  governor start     Simulate starting the governor" << '\n';
    std::cout << "  governor stop      Simulate stopping the governor" << '\n';
    std::cout << "  workspace <path>   Set or show the workspace root" << '\n';
    std::cout << "  settings           Display the current CLI settings" << '\n';
    std::cout << "  save               Persist settings to settings_safemode.json" << '\n';
    std::cout << "  verbose [on|off]   Toggle verbose logging" << '\n';
    std::cout << "  clear              Clear the console" << '\n';
    std::cout << "  api                API server is disabled in this build" << '\n';
    std::cout << "  quit               Exit SafeMode CLI" << '\n' << '\n';
}

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] [command]\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help\n";
    std::cout << "  -w, --workspace DIR  Set workspace root\n";
    std::cout << "  -a, --api [PORT]     Mention API start (no-op)\n";
    std::cout << "  -g, --governor       Start governor simulation at launch\n";
    std::cout << "  -v, --verbose        Enable verbose output\n";
    std::cout << "  --unsafe             Allow operations outside workspace\n";
    std::cout << "If you supply a command after the options it will run once and exit.\n";
}

int main(int argc, char** argv) {
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    RawrXD::Settings settings;
    settings.Load("settings_safemode.json");
    g_state.workspace_root = settings.Get("workspace", "");
    if (g_state.workspace_root.empty()) {
        g_state.workspace_root = ".";
    }
    g_state.verbose = settings.GetBool("verbose", false);
    g_state.allow_unsafe_paths = settings.GetBool("unsafe_paths", false);
    g_state.app_state.enable_overclock_governor = settings.GetBool("enable_overclock_governor", false);
    g_state.app_state.boost_step_mhz = static_cast<uint32_t>(settings.GetInt("boost_step_mhz", 50));
    g_state.app_state.target_all_core_mhz = static_cast<uint32_t>(settings.GetInt("target_all_core_mhz", 0));
    g_state.governor_status = settings.Get("governor_status", "stopped");

    bool start_governor = false;
    bool start_api = false;
    int api_port = 11434;
    std::vector<std::string> oneshot;
    bool enteringOneshot = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (enteringOneshot) {
            oneshot.push_back(arg);
            continue;
        }

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-w" || arg == "--workspace") {
            if (i + 1 < argc) {
                g_state.workspace_root = argv[++i];
            }
        } else if (arg == "-a" || arg == "--api") {
            start_api = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                api_port = std::stoi(argv[++i]);
            }
        } else if (arg == "-g" || arg == "--governor") {
            start_governor = true;
        } else if (arg == "-v" || arg == "--verbose") {
            g_state.verbose = true;
        } else if (arg == "--unsafe") {
            g_state.allow_unsafe_paths = true;
        } else {
            enteringOneshot = true;
            oneshot.push_back(arg);
        }
    }

    if (!telemetry::Initialize()) {
        printWarning("Telemetry initialization failed, continuing without instrumentation");
    }

    printBanner();

    if (start_api) {
        cmdApi({"api", "start", std::to_string(api_port)});
    }

    if (start_governor) {
        cmdGovernor({"governor", "start"});
    }

    if (!oneshot.empty()) {
        dispatchCommand(joinTokens(oneshot));
        telemetry::Shutdown();
        return 0;
    }

    std::string input;
    while (g_state.running.load()) {
        std::cout << Color::Bold << Color::Cyan << "rawr> " << Color::Reset;
        if (!std::getline(std::cin, input)) {
            break;
        }
        if (input.empty()) {
            continue;
        }
        dispatchCommand(input);
    }

    telemetry::Shutdown();
    printInfo("Goodbye!");
    return 0;
}
