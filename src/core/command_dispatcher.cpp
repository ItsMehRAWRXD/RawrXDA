#include "command_dispatcher.h"

namespace RawrXD::Core {
    CommandDispatcher& CommandDispatcher::getInstance() {
        static CommandDispatcher inst;
        return inst;
    }

    void CommandDispatcher::registerHandler(const std::string& cmdId, CommandHandler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_handlers[cmdId] = handler;
    }

    bool CommandDispatcher::execute(const std::string& cmdId, const nlohmann::json& args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(cmdId);
        if (it == m_handlers.end()) return false;
        return it->second(args);
    }

    std::vector<std::string> CommandDispatcher::listCommands() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> ids;
        for (const auto& [id, _] : m_handlers) ids.push_back(id);
        return ids;
    }
}
