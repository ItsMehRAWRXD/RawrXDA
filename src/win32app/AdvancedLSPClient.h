#pragma once
#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>

namespace RawrXD::LSP {

struct WorkspaceSymbol {
    std::string name;
    int kind;
    std::string location_uri;
    int line;
    int character;
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<nlohmann::json>> changes;
};

class AdvancedLSPClient {
public:
    static AdvancedLSPClient& GetInstance();

    // LSP 3.17+ Workspace Operations
    std::future<std::vector<WorkspaceSymbol>> QueryWorkspaceSymbols(const std::string& query);
    std::future<WorkspaceEdit> PrepareGlobalRename(const std::string& uri, int line, int character, const std::string& newName);

private:
    AdvancedLSPClient() = default;
    
    uint64_t m_nextRequestId = 1;
    std::string SendRequest(const std::string& method, nlohmann::json params);
};

}
