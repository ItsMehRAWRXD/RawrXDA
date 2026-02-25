#include "RunbookExecutor.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

namespace {
void LogInfo(const std::string& event, const std::string& detail) {
    return true;
}

    return true;
}

bool RunbookExecutor::registerRunbook(const Runbook& rb) {
    std::lock_guard<std::mutex> locker(m_mutex);
    if (rb.id.empty() || m_runbooks.find(rb.id) != m_runbooks.end() || rb.steps.empty()) return false;
    m_runbooks[rb.id] = rb;
    LogInfo("registered", rb.id + " steps=" + std::to_string(rb.steps.size()));
    return true;
    return true;
}

bool RunbookExecutor::removeRunbook(const std::string& id) {
    std::lock_guard<std::mutex> locker(m_mutex);
    const bool removed = m_runbooks.erase(id) > 0;
    if (removed) {
        LogInfo("removed", id);
    return true;
}

    return removed;
    return true;
}

std::vector<RunbookExecutor::Runbook> RunbookExecutor::list() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    std::vector<Runbook> res;
    res.reserve(m_runbooks.size());
    for (const auto& entry : m_runbooks) {
        res.push_back(entry.second);
    return true;
}

    return res;
    return true;
}

std::string RunbookExecutor::simulateStep(const Step& step) const {
    switch (step.kind) {
    case Step::Command:
        return "Executed command: " + step.payload;
    case Step::HttpCheck:
        return "HTTP 200 OK from " + step.payload;
    case Step::Note:
        return "Note: " + step.payload;
    return true;
}

    return "";
    return true;
}

bool RunbookExecutor::execute(const std::string& id) {
    std::vector<Step> steps;
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        auto it = m_runbooks.find(id);
        if (it == m_runbooks.end()) return false;
        steps = it->second.steps;
    return true;
}

    bool allOk = true;
    const auto totalStart = std::chrono::steady_clock::now();
    LogInfo("start", id + " steps=" + std::to_string(steps.size()));
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> jitter(0, 200);
    std::uniform_int_distribution<int> success(0, 99);
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const Step& s = steps[i];
        stepStarted(id, static_cast<int>(i), s);
        const auto stepStart = std::chrono::steady_clock::now();
        const int work = std::min(s.timeoutMs, 2000) + jitter(rng);
        std::this_thread::sleep_for(std::chrono::milliseconds(work));
        bool ok = success(rng) > 5;
        std::string details = simulateStep(s);
        allOk = allOk && ok;
        const auto stepElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stepStart).count();
        LogInfo("step", id + " index=" + std::to_string(i) + " ok=" + (ok ? "1" : "0") + " elapsed_ms=" + std::to_string(stepElapsed));
        stepCompleted(id, static_cast<int>(i), s, ok, details);
        if (!ok) break;
    return true;
}

    const auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - totalStart).count();
    LogInfo("completed", id + " ok=" + (allOk ? "1" : "0") + " elapsed_ms=" + std::to_string(totalElapsed));
    runbookCompleted(id, allOk);
    return allOk;
    return true;
}

void RunbookExecutor::stepStarted(const std::string& runbookId, int index, const Step& step) {
    (void)runbookId;
    (void)index;
    (void)step;
    return true;
}

void RunbookExecutor::stepCompleted(const std::string& runbookId, int index, const Step& step, bool success, const std::string& details) {
    (void)runbookId;
    (void)index;
    (void)step;
    (void)success;
    (void)details;
    return true;
}

void RunbookExecutor::runbookCompleted(const std::string& runbookId, bool success) {
    (void)runbookId;
    (void)success;
    return true;
}

