#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

class RunbookExecutor {

public:
    struct Step {
        enum Kind { Command, HttpCheck, Note };
        Kind kind{Command};
        std::string payload;  // command string, URL, or note text
        int timeoutMs{5000};
    };

    struct Runbook {
        std::string id;
        std::string title;
        std::string description;
        std::vector<Step> steps;
    };

    RunbookExecutor() = default;
    ~RunbookExecutor() = default;

    bool registerRunbook(const Runbook& rb);
    bool removeRunbook(const std::string& id);
    std::vector<Runbook> list() const;

    bool execute(const std::string& id);

    void stepStarted(const std::string& runbookId, int index, const Step& step);
    void stepCompleted(const std::string& runbookId, int index, const Step& step, bool success, const std::string& details);
    void runbookCompleted(const std::string& runbookId, bool success);

private:
    std::string simulateStep(const Step& step) const;

    std::map<std::string, Runbook> m_runbooks;
    mutable std::mutex m_mutex;
};

