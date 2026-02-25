#pragma once
#include <map>
#include <string>
#include <any>

struct ExecutionContext {
    std::map<std::string, std::string> env;
    std::map<std::string, std::any> variables;
    std::string workingDirectory;
    bool simulationMode = false;
};
