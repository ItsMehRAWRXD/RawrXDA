#include "api_server.h"
#include "app_state.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace RawrXD {

struct APIServer::Impl {
    httplib::Server svr;
    std::thread server_thread;
    std::atomic<bool> is_running{false};
    int port{0};

    void LogRequest(const httplib::Request& req, const httplib::Response& res, const std::string& method) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "[" << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << "] "
                  << method << " " << req.path << " - " << res.status << std::endl;
    }
};

APIServer::APIServer(AppState& app_state)
    : app_state_(app_state), impl_(std::make_unique<Impl>())
{
}

APIServer::~APIServer() {
    Stop();
}

bool APIServer::Initialize(int port) {
    impl_->port = port;
    
    // Ollama-compatible endpoints
    impl_->svr.Post("/api/generate", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleGenerateRequest(req, res);
        impl_->LogRequest(req, res, "POST");
    });

    impl_->svr.Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleChatCompletionsRequest(req, res);
        impl_->LogRequest(req, res, "POST");
    });

    impl_->svr.Get("/api/tags", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleTagsRequest(req, res);
        impl_->LogRequest(req, res, "GET");
    });

    impl_->svr.Post("/api/pull", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandlePullRequest(req, res);
        impl_->LogRequest(req, res, "POST");
    });

    return true;
}

bool APIServer::Start() {
    if (impl_->is_running) return false;
    
    std::cout << "Starting Production API Server on port " << impl_->port << "..." << std::endl;
    impl_->is_running = true;
    
    if (!impl_->svr.listen("0.0.0.0", impl_->port)) {
        impl_->is_running = false;
        return false;
    }
    
    return true;
}

bool APIServer::StartAsync() {
    if (impl_->is_running) return false;
    
    impl_->server_thread = std::thread([this]() {
        this->Start();
    });
    
    // Give it a moment to start or fail
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return impl_->is_running;
}

void APIServer::Stop() {
    if (impl_->is_running) {
        impl_->svr.stop();
        if (impl_->server_thread.joinable()) {
            impl_->server_thread.join();
        }
        impl_->is_running = false;
    }
}

bool APIServer::IsRunning() const {
    return impl_->is_running;
}

int APIServer::GetPort() const {
    return impl_->port;
}

std::string APIServer::GetStatus() const {
    return impl_->is_running ? "Running" : "Stopped";
}

void APIServer::RegisterEndpoint(const std::string& endpoint, RequestHandler handler) {
    impl_->svr.Post(endpoint.c_str(), [handler](const httplib::Request& req, httplib::Response& res) {
        res.set_content(handler(req.body), "application/json");
    });
}

// ... internal handlers ...
void APIServer::HandleGenerateRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = json::parse(req.body);
        std::string prompt = j.value("prompt", "");
        std::string model = j.value("model", "default");

        std::cout << "API Generate Request: model=" << model << " prompt_len=" << prompt.length() << std::endl;

        json response = {
            {"model", model},
            {"created_at", "2023-01-01T00:00:00Z"},
            {"response", "This is a placeholder response from RawrXD Production API Server. Model loading is in progress."},
            {"done", true}
        };

        res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", e.what()}}).dump(), "application/json");
    }
}

void APIServer::HandleChatCompletionsRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = json::parse(req.body);
        auto messages = j.value("messages", json::array());
        std::string model = j.value("model", "default");

        std::cout << "API Chat Request: model=" << model << " messages=" << messages.size() << std::endl;

        json response = {
            {"id", "chatcmpl-123"},
            {"object", "chat.completion"},
            {"created", 1677652288},
            {"model", model},
            {"choices", json::array({
                {
                    {"index", 0},
                    {"message", {
                        {"role", "assistant"},
                        {"content", "RawrXD AI is ready. Autonomous mode enabled."}
                    }},
                    {"finish_reason", "stop"}
                }
            })},
            {"usage", {
                {"prompt_tokens", 9},
                {"completion_tokens", 12},
                {"total_tokens", 21}
            }}
        };

        res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        res.set_content(json({{"error", e.what()}}).dump(), "application/json");
    }
}

void APIServer::HandleTagsRequest(const httplib::Request& req, httplib::Response& res) {
    json response = {
        {"models", json::array({
            {
                {"name", "rawrxd-7b-v1"},
                {"modified_at", "2023-11-20T12:00:00Z"},
                {"size", 4800000000},
                {"digest", "sha256:1234567890abcdef"}
            }
        })}
    };
    res.set_content(response.dump(), "application/json");
}

void APIServer::HandlePullRequest(const httplib::Request& req, httplib::Response& res) {
    try {
        auto j = json::parse(req.body);
        std::string name = j.value("name", "");
        
        std::cout << "API Pull Request: model=" << name << std::endl;

        // Simulate pull progress
        json response = {
            {"status", "pulling " + name},
            {"digest", "sha256:123..."},
            {"total", 4800000000},
            {"completed", 4800000000}
        };

        res.set_content(response.dump(), "application/json");
    } catch (...) {
        res.status = 400;
    }
}

} // namespace RawrXD
