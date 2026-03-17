#pragma once
#include <atomic>
#include <string>
#include <vector>

class Metrics {
public:
    static Metrics& instance();

    void incMessagesProcessed();
    void incVoiceStart();
    void incErrors();

    void observeModelCallMs(int ms);

    // Snapshot for export
    struct Snapshot {
        unsigned long long messagesProcessed;
        unsigned long long voiceStarts;
        unsigned long long errors;
        std::vector<unsigned long long> latencyBuckets; // ms buckets
    };

    Snapshot snapshot() const;
    bool exportToFile(const std::string& path) const; // writes JSON-like

private:
    Metrics();

    std::atomic<unsigned long long> m_messagesProcessed{0};
    std::atomic<unsigned long long> m_voiceStarts{0};
    std::atomic<unsigned long long> m_errors{0};

    // Buckets: <10, <20, <50, <100, <200, <500, <1000, >=1000
    std::vector<std::atomic<unsigned long long>> m_latencyBuckets;
};