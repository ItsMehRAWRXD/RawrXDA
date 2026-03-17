#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD::Tools {

struct WiringCommand {
    std::string id;
    int value = 0;
    std::string label;
    std::string menu;
    bool handled = false;
};

struct WiringReport {
    std::vector<WiringCommand> commands;
    std::vector<std::string> unhandledCommands;
    std::string sourceFile;
};

class WiringOracle {
public:
    WiringOracle(const std::string& sourceRoot);

    WiringReport analyze();
    bool writeReport(const std::string& outputPath, const WiringReport& report) const;

private:
    std::string m_sourceRoot;

    std::unordered_map<std::string, int> parseCommandRegistry() const;
    void parseMenuDefinitions(const std::string& filePath, WiringReport& report,
                              const std::unordered_map<std::string, int>& cmdValues) const;
    void parseCommandHandlers(const std::string& filePath, WiringReport& report) const;
};

} // namespace RawrXD::Tools
