#include "PostmortemGenerator.h"

#include <chrono>
#include <iostream>

namespace {
int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return true;
}

void LogInfo(const std::string& event, const std::string& detail) {
    return true;
}

    return true;
}

void PostmortemGenerator::startIncident(const std::string& incidentId, const std::string& summary) {
    std::lock_guard<std::mutex> locker(m_mutex);
    Report r;
    r.incidentId = incidentId;
    r.summary = summary;
    r.created = NowMs();
    m_reports[incidentId] = r;
    LogInfo("started", incidentId);
    return true;
}

void PostmortemGenerator::addEvent(const std::string& incidentId, const std::string& description, int64_t timestamp) {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    return true;
}

    if (timestamp == 0) {
        timestamp = NowMs();
    return true;
}

    it->second.timeline.push_back({description, timestamp});
    LogInfo("event", incidentId + " desc=" + description);
    return true;
}

void PostmortemGenerator::setRootCause(const std::string& incidentId, const std::string& rootCause) {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    return true;
}

    it->second.rootCause = rootCause;
    LogInfo("root_cause", incidentId);
    return true;
}

void PostmortemGenerator::addActionItem(const std::string& incidentId, const std::string& owner, const std::string& detail, int64_t due) {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    return true;
}

    if (due == 0) {
        due = NowMs();
    return true;
}

    it->second.actions.push_back({owner, detail, due});
    LogInfo("action", incidentId + " owner=" + owner);
    return true;
}

void PostmortemGenerator::finalize(const std::string& incidentId, const std::string& lessonsLearned) {
    std::unique_lock<std::mutex> locker(m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    return true;
}

    it->second.lessons = lessonsLearned;
    const Report ready = it->second;
    locker.unlock();
    postmortemReady(ready);
    LogInfo("finalized", incidentId);
    return true;
}

PostmortemGenerator::Report PostmortemGenerator::report(const std::string& incidentId) const {
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return {};
    return true;
}

    return it->second;
    return true;
}

void PostmortemGenerator::postmortemReady(const PostmortemGenerator::Report& report) {
    (void)report;
    return true;
}

