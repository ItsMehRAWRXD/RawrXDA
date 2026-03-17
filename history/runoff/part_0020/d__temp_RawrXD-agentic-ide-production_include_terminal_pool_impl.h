#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>

// ============================================================================
// TERMINAL POOL - Multi-Terminal Management
// ============================================================================

struct TerminalInfo {
    std::string id;
    std::string name;
    bool isActive = false;
};

class TerminalPool {
public:
    explicit TerminalPool(size_t poolSize = 4);
    ~TerminalPool();

    std::string executeCommand(const std::string& command);
    void executeAsync(const std::string& command, std::function<void(const std::string&)> callback);
    std::vector<TerminalInfo> getTerminals() const;

private:
    size_t m_poolSize;
};
