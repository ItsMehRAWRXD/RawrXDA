#pragma once

#include <string>
#include <functional>

using TerminalCallback = std::function<void(const std::string&)>;

class Terminal {
public:
    Terminal();
    ~Terminal();
    
    // Start terminal with specified shell
    bool start(const std::string& shell = "powershell");
    void stop();
    
    // Send command to terminal
    bool sendCommand(const std::string& command);
    
    // Set output callback
    void setCallback(TerminalCallback callback);
    
    // Status
    bool isRunning() const;
    std::string getShellType() const;
    
private:
    class Impl;
    Impl* m_impl;
};
