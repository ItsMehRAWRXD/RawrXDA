// InferenceService.cpp - Implementation of HTTP+JSON wrapper
#include "InferenceService.h"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

InferenceService::InferenceService(const std::string& endpoint, long timeoutMs)
    : m_endpoint(endpoint), m_timeoutMs(timeoutMs)
{
    // libcurl global init can be done once in main() – we keep it simple here.
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

nlohmann::json InferenceService::requestResolution(const nlohmann::json& payload)
{
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init() failed");

    std::string requestBody = payload.dump();
    std::string responseBody;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, m_endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_timeoutMs);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
    curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");               // forbid any proxy
    curl_easy_setopt(curl, CURLOPT_INTERFACE, "127.0.0.1");     // bind to loopback only

    CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK)
        throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(rc));

    if (httpCode != 200)
        throw std::runtime_error("Inference server returned HTTP " + std::to_string(httpCode));

    // Parse JSON – any parse error bubbles up as an exception.
    return nlohmann::json::parse(responseBody);
}
