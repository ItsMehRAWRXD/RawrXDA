#include "Win32TerminalManager.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    Win32TerminalManager tm;

    // Set up callbacks
    tm.onOutput = [](const std::string& output) {
        std::cout << "Output: " << output;
    };

    tm.onError = [](const std::string& error) {
        std::cerr << "Error: " << error;
    };

    tm.onStarted = []() {
        std::cout << "Terminal started" << std::endl;
    };

    tm.onFinished = [](int code) {
        std::cout << "Terminal finished with code: " << code << std::endl;
    };

    // Start PowerShell
    if (tm.start(Win32TerminalManager::PowerShell)) {
        std::cout << "PowerShell started successfully" << std::endl;

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Send a command
        tm.writeInput("echo 'Hello from Win32 IDE'\n");

        // Wait for output
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop
        tm.stop();
    } else {
        std::cerr << "Failed to start PowerShell" << std::endl;
        return 1;
    }

    return 0;
}