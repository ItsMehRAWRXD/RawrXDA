#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <string>

// Single Producer (Agent), Single Consumer (UI) Ring Buffer
template<typename T, size_t Size>
class SPSCQueue {
    std::vector<T> buffer;
    std::atomic<size_t> head{0}; // Producer writes here
    std::atomic<size_t> tail{0}; // Consumer reads here

public:
    SPSCQueue() : buffer(Size) {}

    bool Push(const T& item) {
        size_t currentHead = head.load(std::memory_order_relaxed);
        size_t nextHead = (currentHead + 1) % Size;

        if (nextHead == tail.load(std::memory_order_acquire)) {
            return false; // Full
        }

        buffer[currentHead] = item;
        head.store(nextHead, std::memory_order_release);
        return true;
    }

    std::optional<T> Pop() {
        size_t currentTail = tail.load(std::memory_order_relaxed);

        if (currentTail == head.load(std::memory_order_acquire)) {
            return std::nullopt; // Empty
        }

        T item = buffer[currentTail];
        tail.store((currentTail + 1) % Size, std::memory_order_release);
        return item;
    }
};

// Global Bridge
struct AgentTask {
    enum Type { INFERENCE_REQ, STOP_REQ, ANALYSIS_REQ } type;
    std::string payload;
};

struct AgentResponse {
    std::string tokenChunk;
    bool isDone;
};

// The pipelines
extern SPSCQueue<AgentTask, 1024> g_AgentInputQueue;
extern SPSCQueue<AgentResponse, 4096> g_AgentOutputQueue;
