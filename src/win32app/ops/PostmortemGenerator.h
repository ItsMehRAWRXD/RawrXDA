#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

class PostmortemGenerator {

public:
    struct Event {
        std::string description;
        int64_t timestamp{0};
    };

    struct ActionItem {
        std::string owner;
        std::string detail;
        int64_t due{0};
    };

    struct Report {
        std::string incidentId;
        std::string summary;
        std::string rootCause;
        std::vector<Event> timeline;
        std::vector<ActionItem> actions;
        std::string lessons;
        int64_t created{0};
    };

    PostmortemGenerator() = default;

    void startIncident(const std::string& incidentId, const std::string& summary);
    void addEvent(const std::string& incidentId, const std::string& description, int64_t timestamp = 0);
    void setRootCause(const std::string& incidentId, const std::string& rootCause);
    void addActionItem(const std::string& incidentId, const std::string& owner, const std::string& detail, int64_t due = 0);
    void finalize(const std::string& incidentId, const std::string& lessonsLearned);
    Report report(const std::string& incidentId) const;

    void postmortemReady(const PostmortemGenerator::Report& report);

private:
    mutable std::mutex m_mutex;
    std::map<std::string, Report> m_reports;
};

