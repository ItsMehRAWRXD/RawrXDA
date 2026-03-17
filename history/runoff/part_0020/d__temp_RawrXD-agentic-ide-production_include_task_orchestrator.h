#pragma once
#include "logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Structured JSON logging helper macros
#define LOG_DEBUG_JSON(msg) Logger::instance().debug(msg)
#define LOG_INFO_JSON(msg)  Logger::instance().info(msg)
#define LOG_ERROR_JSON(msg) Logger::instance().error(msg)

struct Task {
    std::string goal;
    std::string file;
    std::vector<std::string> dependencies;
};

class Agent {
public:
    Agent() = default;
    void analyzeTask(const Task &task);
};

// Simple metrics collector with counters and durations
class Metrics {
public:
    static Metrics &instance();

    void incrementTasksQueued();
    void incrementTasksProcessed();
    void observeAnalysisDuration(double ms);

    // Optional: dump current metrics to a file for external scraping
    void dumpToFile(const std::string &path);

private:
    Metrics() = default;

    std::atomic<uint64_t> m_tasksQueued{0};
    std::atomic<uint64_t> m_tasksProcessed{0};
    // naive rolling window; in production use histograms
    std::mutex m_durationsMutex;
    std::vector<double> m_analysisDurationsMs;
};

class Orchestra {
public:
    explicit Orchestra(std::vector<Task> &tasks);

    void start();
    void runAgents(int threads);

    // For graceful shutdown or external signaling
    void stop();

private:
    std::vector<Task> &m_tasks;
    std::deque<Task> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_stopping{false};
};

class ArchitectAgent {
public:
    explicit ArchitectAgent(std::vector<Task> &tasks);

    void analyzeGoal(const std::string &goal);
    void startOrchestra(int threads);

private:
    std::vector<Task> &m_tasks;
    Orchestra m_orchestra;
};
