/**
 * @file RawrXD_ModelInvoker.hpp
 * @brief Pure Win32/C++20 Model Invoker - Zero Qt Dependencies
 */

#pragma once

#include "RawrXD_AgentKernel.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace RawrXD::Agent {

struct LLMResponse {
    bool success = false;
    std::string raw_output;
    std::string error;
    int tokens_used = 0;
    std::string reasoning;
    // std::vector<Action> parsed_plan; // defined in kernel or local
};

class ModelInvoker {
public:
    ModelInvoker();
    ~ModelInvoker();

    void set_backend(const std::string& backend, const std::string& endpoint, const std::string& api_key = "");
    void set_model(const std::string& model);

    LLMResponse invoke(const std::string& wish, const std::vector<std::string>& tools = {});
    
    /**
     * @brief Generate embedding for text
     */
    virtual std::vector<float> GenerateEmbedding(const std::string& model, const std::string& text);
    
private:
    std::string backend_ = "ollama";
    std::string endpoint_ = "http://localhost:11434";
    std::string api_key_;
    std::string model_ = "mistral";

    std::string send_http_request(const std::string& url, const std::string& payload);
};

} // namespace RawrXD::Agent
