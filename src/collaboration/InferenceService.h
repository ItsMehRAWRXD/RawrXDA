// InferenceService.h - HTTP+JSON wrapper for Sovereign inference server
#pragma once

#include <string>
#include <nlohmann/json.hpp>

class InferenceService {
public:
    // `endpoint` is the HTTP URL of the Sovereign inference server,
    // e.g. "http://127.0.0.1:8000/v1/merge"
    explicit InferenceService(const std::string& endpoint,
                              long timeoutMs = 5000);

    // Sends a JSON payload and returns the parsed JSON response.
    // Throws std::runtime_error on network / HTTP errors.
    nlohmann::json requestResolution(const nlohmann::json& payload);

private:
    std::string m_endpoint;
    long        m_timeoutMs;
};
