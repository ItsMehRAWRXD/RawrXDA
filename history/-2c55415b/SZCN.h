#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include "cpu_inference_engine.h"

namespace RawrXD {

class CompletionServer {
public:
    CompletionServer();
    ~CompletionServer();

    bool Start(uint16_t port, CPUInferenceEngine* engine, std::string model_path);
    void Stop();

private:
    void Run(uint16_t port);
    void HandleClient(int client_fd);
    std::string HandleCompleteRequest(const std::string& body);
    void HandleCompleteStreamRequest(int client_fd, const std::string& body);

    std::atomic<bool> running_;
    std::thread server_thread_;
    CPUInferenceEngine* engine_;
    std::string model_path_;
};

} // namespace RawrXD
