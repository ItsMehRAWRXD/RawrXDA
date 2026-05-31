#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>

namespace RawrXD::Core {

using CommandHandler = std::function<bool(const nlohmann::json& args)>;

struct CommandInfo {
    std::string id;
    std::string category;
    std::string description;
    CommandHandler handler;
    nlohmann::json schema;
    bool requiresConfirmation = false;
    bool isAsync = false;
};

class CommandDispatcher {
public:
    static CommandDispatcher& getInstance();
    
    // Registration
    void registerHandler(const std::string& cmdId, CommandHandler handler);
    void registerCommand(const CommandInfo& info);
    void unregisterCommand(const std::string& cmdId);
    
    // Execution
    bool execute(const std::string& cmdId, const nlohmann::json& args = {});
    bool executeAsync(const std::string& cmdId, const nlohmann::json& args, 
                      std::function<void(bool success, const nlohmann::json& result)> callback);
    
    // Query
    bool hasCommand(const std::string& cmdId) const;
    std::vector<std::string> listCommands() const;
    std::vector<std::string> listCommandsByCategory(const std::string& category) const;
    CommandInfo getCommandInfo(const std::string& cmdId) const;
    
    // Batch execution
    std::vector<bool> executeBatch(const std::vector<std::pair<std::string, nlohmann::json>>& commands);
    
    // Clear
    void clear();

private:
    CommandDispatcher() = default;
    ~CommandDispatcher() = default;
    
    CommandDispatcher(const CommandDispatcher&) = delete;
    CommandDispatcher& operator=(const CommandDispatcher&) = delete;
    
    mutable std::mutex m_mutex;
    std::map<std::string, CommandInfo> m_commands;
};

} // namespace RawrXD::Core
