#include "WiringOracle.h"
#include "../win32app/IDELogger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <regex>
#include <set>

using nlohmann::json;

namespace RawrXD::Tools {

WiringOracle::WiringOracle(const std::string& sourceRoot)
    : m_sourceRoot(sourceRoot)
{
}

WiringReport WiringOracle::analyze()
{
    WiringReport report;
    report.sourceFile = m_sourceRoot + "\\src\\win32app\\Win32IDE.cpp";

    auto cmdValues = parseCommandRegistry();
    parseMenuDefinitions(report.sourceFile, report, cmdValues);
    parseCommandHandlers(report.sourceFile, report);

    for (const auto& cmd : report.commands) {
        if (!cmd.handled) {
            report.unhandledCommands.push_back(cmd.id);
        }
    }

    return report;
}

bool WiringOracle::writeReport(const std::string& outputPath, const WiringReport& report) const
{
    json data;
    data["source"] = report.sourceFile;
    data["commands"] = json::array();
    data["unhandled"] = report.unhandledCommands;

    for (const auto& cmd : report.commands) {
        json entry;
        entry["id"] = cmd.id;
        entry["value"] = cmd.value;
        entry["label"] = cmd.label;
        entry["menu"] = cmd.menu;
        entry["handled"] = cmd.handled;
        data["commands"].push_back(entry);
    }

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        LOG_ERROR("Failed to write wiring report: " + outputPath);
        return false;
    }

    out << data.dump(2);
    out.flush();
    LOG_INFO("Wiring report written to " + outputPath);
    return true;
}

std::unordered_map<std::string, int> WiringOracle::parseCommandRegistry() const
{
    std::unordered_map<std::string, int> result;
    std::string registryPath = m_sourceRoot + "\\include\\command_registry.h";

    std::ifstream file(registryPath);
    if (!file.is_open()) {
        LOG_WARNING("Command registry not found: " + registryPath);
        return result;
    }

    std::regex cmdRegex(R"(constexpr\s+UINT\s+(CMD_[A-Z0-9_]+)\s*=\s*([0-9]+))");
    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, cmdRegex)) {
            result[match[1]] = std::stoi(match[2]);
        }
    }

    LOG_INFO("Parsed command registry entries: " + std::to_string(result.size()));
    return result;
}

void WiringOracle::parseMenuDefinitions(const std::string& filePath, WiringReport& report,
                                        const std::unordered_map<std::string, int>& cmdValues) const
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open IDE source: " + filePath);
        return;
    }

    std::regex createMenuRegex(R"(HMENU\s+(\w+)\s*=\s*CreatePopupMenu\(\))");
    std::regex appendMenuRegex(R"(AppendMenuA\((\w+),\s*MF_STRING,\s*(\w+),\s*\"([^\"]+)\"\))");
    std::regex attachMenuRegex(R"(AppendMenuA\(m_hMenu,\s*MF_POPUP,\s*\(UINT_PTR\)(\w+),\s*\"([^\"]+)\"\))");

    std::unordered_map<std::string, std::string> menuLabels;
    std::string line;

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, createMenuRegex)) {
            if (menuLabels.find(match[1]) == menuLabels.end()) {
                menuLabels[match[1]] = match[1];
            }
        } else if (std::regex_search(line, match, attachMenuRegex)) {
            menuLabels[match[1]] = match[2];
        } else if (std::regex_search(line, match, appendMenuRegex)) {
            WiringCommand cmd;
            cmd.menu = menuLabels[match[1]];
            cmd.id = match[2];
            cmd.label = match[3];
            cmd.value = cmdValues.count(cmd.id) ? cmdValues.at(cmd.id) : 0;
            report.commands.push_back(cmd);
        }
    }

    LOG_INFO("Menu items parsed: " + std::to_string(report.commands.size()));
}

void WiringOracle::parseCommandHandlers(const std::string& filePath, WiringReport& report) const
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open IDE source for handlers: " + filePath);
        return;
    }

    std::set<std::string> handled;
    std::regex caseRegex(R"(case\s+(IDM_[A-Z0-9_]+))");
    std::string line;

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, caseRegex)) {
            handled.insert(match[1]);
        }
    }

    for (auto& cmd : report.commands) {
        if (handled.find(cmd.id) != handled.end()) {
            cmd.handled = true;
        }
    }

    LOG_INFO("Command handlers found: " + std::to_string(handled.size()));
}

} // namespace RawrXD::Tools
