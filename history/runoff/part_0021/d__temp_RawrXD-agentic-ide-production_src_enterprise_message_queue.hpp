#pragma once

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>

struct Message {
    std::string messageId;
    std::string senderId;
    std::string content;
    long long timestamp;
};

class MessageQueue {
private:
    static MessageQueue* s_instance;
    static std::mutex s_mutex;
    std::queue<Message> m_queue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    std::thread m_processingThread;
    bool m_isRunning;
    std::function<void(const Message&)> m_messageHandler;

    MessageQueue() : m_isRunning(false) {}

public:
    static MessageQueue& instance();

    void start();
    void stop();
    void enqueue(const Message& message);
    void setMessageHandler(std::function<void(const Message&)> handler);
    bool isRunning() const;
    size_t queueSize() const;
};
