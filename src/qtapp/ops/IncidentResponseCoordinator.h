#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <deque>

class IncidentResponseCoordinator {

public:
    enum class Severity { Sev1, Sev2, Sev3, Sev4 };
    enum class Status { Open, Mitigating, Monitoring, Resolved, Cancelled };

    struct Incident {
        std::string id;
        std::string title;
        Severity severity{Severity::Sev3};
        Status status{Status::Open};
        std::vector<std::string> tags;
        std::string owner;
        std::string summary;
        std::string mitigation;
        int64_t openedAt{0};
        int64_t updatedAt{0};
        int64_t resolvedAt{0};
        int escalations{0};
    };

    IncidentResponseCoordinator() = default;
    ~IncidentResponseCoordinator() = default;

    std::string openIncident(const std::string& title, Severity severity, const std::vector<std::string>& tags = {});
    bool updateSummary(const std::string& id, const std::string& summary);
    bool addMitigation(const std::string& id, const std::string& mitigation);
    bool escalate(const std::string& id, const std::string& newOwner);
    bool setStatus(const std::string& id, Status status);
    Incident get(const std::string& id) const;
    std::vector<Incident> listOpen() const;
    std::vector<Incident> listRecent(int limit = 50) const;

    void incidentOpened(const Incident& inc);
    void incidentUpdated(const Incident& inc);
    void incidentResolved(const Incident& inc);
    void incidentEscalated(const Incident& inc);

private:
    std::string generateId() const;
    std::string statusToString(Status s) const;
    void enforceBoundedHistory();

    std::map<std::string, Incident> m_incidents;
    std::deque<std::string> m_order;
    mutable std::mutex m_mutex;
};

