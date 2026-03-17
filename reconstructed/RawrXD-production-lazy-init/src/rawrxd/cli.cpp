#include "gui.h"
#include "api_server.h"
#include "telemetry.h"
#include "settings.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "cli_command_handler.h"
#include "cli_streaming_enhancements.h"
#include "performance_tuner.h"
#include <iostream>
#include <array>
#include <string>
#include <thread>
#include <atomic>
#include <sstream>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

using namespace std::chrono_literals;

enum class InputMode {
    SingleKey,   // Original single-key commands (h/p/g/etc)
    CommandLine  // Full command-line mode with text input
};

// Execute shell command (PowerShell or CMD)
std::string executeShellCommand(const std::string& cmd, bool usePowerShell = false) {
#ifdef _WIN32
    std::string fullCmd;
    if (usePowerShell) {
        fullCmd = "powershell.exe -NoProfile -Command \"" + cmd + "\"";
    } else {
        fullCmd = "cmd.exe /c \"" + cmd + "\"";
    }
    
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = _popen(fullCmd.c_str(), "r");
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
        _pclose(pipe);
    }
    return result;
#else
    return "Shell execution not supported on this platform";
#endif
}

int main(int argc, char** argv) {
    // Multi-instance support: Create named mutex to allow multiple instances
#ifdef _WIN32
    // Generate unique instance ID based on process ID
    DWORD pid = GetCurrentProcessId();
    std::string mutexName = "RawrXD_CLI_Instance_" + std::to_string(pid);
    HANDLE hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
    
    // Use random port allocation for maximum flexibility and to avoid conflicts
    uint16_t instancePort = APIServer::FindRandomAvailablePort(15000, 25000, 50);
    
    std::cout << "RawrXD CLI Instance [PID: " << pid << "] [Port: " << instancePort << "]\n";
#else
    uint16_t instancePort = APIServer::FindRandomAvailablePort(15000, 25000, 50);
#endif
    
    // Initialize performance tuner
    auto& perfTuner = Performance::GetPerformanceTuner();
    perfTuner.AutoTune();
    
    AppState state;
    std::cout << "RawrXD CLI v1.0.0 - Agentic IDE Command Line Interface\n";
    std::cout << "Multi-instance support enabled - you can open multiple windows\n";
    std::cout << "Type 'help' for available commands or use single-key shortcuts\n";
    std::cout << "Type 'mode' to toggle between command-line and single-key input\n";
    std::cout << "Use 'shell <cmd>' or 'ps <cmd>' to execute PowerShell/CMD commands\n" << std::endl;
    
    // Load settings
    Settings::LoadCompute(state);
    Settings::LoadOverclock(state);

    // Initialize telemetry
    telemetry::Initialize();

    // Start API server with instance-specific port
    APIServer api(state);
        state.api_server = &api;  // Store pointer for CLI command access
    api.Start(instancePort);

    // Initialize command handler for full feature set
    CLI::CommandHandler cmdHandler(state);
    
    // Initialize enhanced CLI features
    CLI::AutoCompleter autoCompleter;
    CLI::HistoryManager historyManager("rawrxd_history.txt");
    CLI::ProgressIndicator progressIndicator;

    // Start governor if requested
    OverclockGovernor governor;
    std::atomic<bool> governor_running{false};
    if (state.enable_overclock_governor) {
        governor.Start(state);
        governor_running = true;
    }

    InputMode inputMode = InputMode::CommandLine;
    bool running = true;
    telemetry::TelemetrySnapshot snap{};
    
    while (running) {
        if (inputMode == InputMode::CommandLine) {
            // Full command-line mode
            std::cout << "\nRawrXD> ";
            std::string line;
            std::getline(std::cin, line);
            
            if (line.empty()) continue;

            historyManager.Add(line);
            
            // Special mode toggle
            if (line == "mode") {
                inputMode = InputMode::SingleKey;
                std::cout << "Switched to single-key mode (h/p/g/a/r/+/-/s/q)" << std::endl;
                continue;
            }

            // History commands
            if (line == "history") {
                auto history = historyManager.GetAll();
                std::cout << "\nCommand History (" << history.size() << "):\n";
                for (const auto& cmd : history) {
                    std::cout << "  " << cmd << "\n";
                }
                continue;
            }
            if (line.rfind("history ", 0) == 0) {
                std::string pattern = line.substr(8);
                auto results = historyManager.Search(pattern);
                std::cout << "\nHistory Search: \"" << pattern << "\" (" << results.size() << "):\n";
                for (const auto& cmd : results) {
                    std::cout << "  " << cmd << "\n";
                }
                continue;
            }
            
            // Shell command execution
            if (line.substr(0, 6) == "shell " || line.substr(0, 4) == "cmd ") {
                std::string cmd = line.substr(line.find(' ') + 1);
                std::cout << "Executing CMD: " << cmd << std::endl;
                std::string result = executeShellCommand(cmd, false);
                std::cout << result;
                continue;
            }
            
            // PowerShell command execution
            if (line.substr(0, 3) == "ps " || line.substr(0, 11) == "powershell ") {
                std::string cmd = line.substr(line.find(' ') + 1);
                std::cout << "Executing PowerShell: " << cmd << std::endl;
                std::string result = executeShellCommand(cmd, true);
                std::cout << result;
                continue;
            }
            
            // Execute command through handler
            if (!cmdHandler.executeCommand(line)) {
                running = false;
            }
        }
        else {
            // Single-key mode (original behavior)
#ifdef _WIN32
            if (_kbhit()) {
                int c = _getch();
                
                // Mode toggle
                if (c == 'm') {
                    inputMode = InputMode::CommandLine;
                    std::cout << "\nSwitched to command-line mode. Type 'help' for commands." << std::endl;
                    continue;
                }
                
                switch (c) {
                case 'h':
                    std::cout << "h=help, p=status, g=toggle governor, a=apply profile, r=reset, +=inc, -=dec, s=save, q=quit, m=mode" << std::endl;
                    break;
                case 'p':
                    telemetry::Poll(snap);
                    std::cout << "CPU temp: " << (snap.cpuTempValid ? std::to_string(snap.cpuTempC) + "C" : "n/a") << "\n";
                    std::cout << "GPU temp: " << (snap.gpuTempValid ? std::to_string(snap.gpuTempC) + "C" : "n/a") << "\n";
                    std::cout << "Governor status: " << state.governor_status << " applied_offset: " << state.applied_core_offset_mhz << "\n";
                    break;
                case 'g':
                    if (governor_running) { 
                        governor.Stop(); 
                        governor_running = false; 
                        state.governor_status = "stopped"; 
                        std::cout << "Governor stopped\n"; 
                    }
                    else { 
                        governor.Start(state); 
                        governor_running = true; 
                        state.governor_status = "running"; 
                        std::cout << "Governor started\n"; 
                    }
                    break;
                case 'a':
                    cmdHandler.cmdOverclockApplyProfile();
                    break;
                case 'r':
                    cmdHandler.cmdOverclockReset();
                    break;
                case '+':
                case '=':
                    state.applied_core_offset_mhz += state.boost_step_mhz;
                    overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                    std::cout << "Increased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                    break;
                case '-':
                    state.applied_core_offset_mhz = std::max(0, state.applied_core_offset_mhz - (int)state.boost_step_mhz);
                    overclock_vendor::ApplyCpuOffsetMhz(state.applied_core_offset_mhz);
                    std::cout << "Decreased offset to " << state.applied_core_offset_mhz << " MHz" << std::endl;
                    break;
                case 's':
                    cmdHandler.cmdSaveSettings();
                    break;
                case 'q':
                    running = false; 
                    break;
                default:
                    std::cout << "Unknown command: " << (char)c << ". Press 'h' for help or 'm' to switch modes." << std::endl;
                }
            }
#else
            // Non-Windows: Use command-line mode only
            inputMode = InputMode::CommandLine;
#endif
        }

        // Poll telemetry and update state
        telemetry::Poll(snap);
        if (snap.cpuTempValid) state.current_cpu_temp_c = (uint32_t)std::lround(snap.cpuTempC);
        if (snap.gpuTempValid) state.current_gpu_hotspot_c = (uint32_t)std::lround(snap.gpuTempC);
        std::this_thread::sleep_for(200ms);
    }

    if (governor_running) governor.Stop();
    api.Stop();
    telemetry::Shutdown();
    
#ifdef _WIN32
    if (hMutex) {
        CloseHandle(hMutex);
    }
#endif
    
    std::cout << "Exiting RawrXD CLI" << std::endl;
    return 0;
}
