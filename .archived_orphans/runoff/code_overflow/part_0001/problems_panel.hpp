#pragma once
#include <string>

struct BuildError;

class ProblemsPanel {
public:
    ProblemsPanel() = default;
    virtual ~ProblemsPanel() = default;

    void addProblem(const BuildError& error);
    void addProblem(const std::string& file, int line, int col,
                    const std::string& message, int severity) {}
    void clearProblems() {}
};
