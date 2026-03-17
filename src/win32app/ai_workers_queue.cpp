#include <deque>
#include <functional>
#include <mutex>

namespace {
std::deque<std::function<void()>> g_invokeQueue;
std::mutex g_invokeQueueMutex;
}

void AIWorkersInvokeLater(std::function<void()> f) {
    if (!f) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_invokeQueueMutex);
    g_invokeQueue.push_back(std::move(f));
}

void AIWorkersProcessInvokeQueue() {
    std::deque<std::function<void()>> pending;
    {
        std::lock_guard<std::mutex> lock(g_invokeQueueMutex);
        pending.swap(g_invokeQueue);
    }
    for (auto& task : pending) {
        task();
    }
}
