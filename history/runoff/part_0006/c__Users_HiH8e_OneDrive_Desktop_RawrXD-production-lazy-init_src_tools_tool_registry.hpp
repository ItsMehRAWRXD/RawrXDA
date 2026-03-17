#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>

class ToolRegistry {
public:
    struct Tool {
        std::string name;
        std::string description;
        std::string category;
        std::function<std::string(const std::vector<std::string>&)> execute;
    };

    ToolRegistry();
    ~ToolRegistry();

    bool registerTool(const Tool& tool);
    bool unregisterTool(const std::string& name);
    
    std::vector<std::string> getToolNames() const;
    std::vector<Tool> getToolsByCategory(const std::string& category) const;
    Tool getTool(const std::string& name) const;
    
    std::string executeTool(const std::string& name, const std::vector<std::string>& parameters);
    
    // Predefined tool categories
    static const std::string FILE_OPS;
    static const std::string BUILD_TEST;
    static const std::string GIT_OPS;
    static const std::string SEARCH_ANALYSIS;
    static const std::string EXTERNAL_INTEGRATIONS;
    static const std::string SYSTEM_OPS;

private:
    std::map<std::string, Tool> tools_;
    
    void registerDefaultTools();
};