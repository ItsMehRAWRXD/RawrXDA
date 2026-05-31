#ifndef RAWRXD_AGENTIC_BRIDGE_HPP
#define RAWRXD_AGENTIC_BRIDGE_HPP

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>

namespace RawrXD {

enum class AIRequestType { COMPLETION, CHAT, EXPLAIN, REFACTOR, GENERATE_TESTS, FIX_ERRORS };

struct AIRequest {
    AIRequestType type;
    std::string context;
    std::string prompt;
    std::function<void(const std::string&)> callback;
    std::chrono::steady_clock::time_point timestamp;
    std::atomic<bool> cancelled{false};
};

class AIAgenticBridge {
public:
    AIAgenticBridge();
    ~AIAgenticBridge();

    bool Initialize(const std::string& endpoint, const std::string& defaultModel);
    void Shutdown();
    bool IsConnected() const { return connected_.load(); }

    void RequestCompletion(const std::string& context, std::function<void(const std::string&)> callback);
    void SendChat(const std::string& message, std::function<void(const std::string&)> callback = nullptr);
    void RequestExplanation(const std::string& code, std::function<void(const std::string&)> callback);
    void RequestRefactor(const std::string& code, const std::string& instruction,
                         std::function<void(const std::string&)> callback);

    void SetModel(const std::string& model) { currentModel_ = model; }
    std::string GetModel() const { return currentModel_; }
    std::vector<std::string> GetAvailableModels();

    bool IsProcessing() const { return processing_.load(); }
    void CancelCurrentRequest();
    double GetAverageResponseTime() const;
    size_t GetRequestCount() const { return requestCount_.load(); }

private:
    void ProcessQueue();
    bool CallOllamaAPI(const std::string& prompt, std::string& response,
                       std::shared_ptr<std::atomic<bool>> cancelFlag);
    bool ParseModelList(const std::string& jsonResponse, std::vector<std::string>& models);

    std::string endpoint_;
    std::string currentModel_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> processing_{false};
    std::atomic<size_t> requestCount_{0};
    double totalResponseTime_ = 0.0;
    std::mutex responseMutex_;

    std::queue<std::shared_ptr<AIRequest>> requestQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::thread workerThread_;
    std::shared_ptr<std::atomic<bool>> currentCancelFlag_;
};

} // namespace RawrXD
#endif // RAWRXD_AGENTIC_BRIDGE_HPP