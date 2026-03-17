#pragma once
/**
 * @file problems_panel.hpp
 * @brief Problems panel and build output — stub for build_output_connector.
 */
#include <string>
#include <vector>

struct BuildConfiguration {
    std::string buildTool;
    std::string sourceFile;
    std::string buildDirectory;
    std::vector<std::string> extraArgs;
};

class ProblemsPanel {
public:
    ProblemsPanel() = default;
    virtual ~ProblemsPanel() = default;
    void clearProblems() {}
    void addProblem(const std::string& file, int line, int col,
                    const std::string& message, int severity) {}
    void setBuildInProgress(bool inProgress) {}
};
