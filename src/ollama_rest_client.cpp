#include "ollama_rest_client.h"
#include <iostream>

size_t OllamaRESTClient::curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((const char*)contents, size * nmemb);
    return size * nmemb;
}

OllamaRESTClient::OllamaRESTClient()
    : m_baseUrl("http://localhost:11434")
    , m_curl(curl_easy_init())
    , m_timeout_ms(5000) {}

OllamaRESTClient::~OllamaRESTClient() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
}

bool OllamaRESTClient::connect(const std::string& host, int port, int timeout_ms) {
    m_baseUrl = "http://" + host + ":" + std::to_string(port);
    m_timeout_ms = timeout_ms;
    return isServerReady();
}

bool OllamaRESTClient::isServerReady() {
    if (!m_curl) return false;

    std::string readBuffer;
    std::string url = m_baseUrl + "/api/tags";

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, (long)m_timeout_ms);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, (long)m_timeout_ms);

    CURLcode res = curl_easy_perform(m_curl);
    
    if (res != CURLE_OK) {
        return false;
    }

    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    return http_code == 200;
}

std::vector<OllamaRESTClient::OllamaModel> OllamaRESTClient::getAvailableModels() {
    std::vector<OllamaModel> result;

    if (!m_curl) return result;

    std::string readBuffer;
    std::string url = m_baseUrl + "/api/tags";

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, (long)m_timeout_ms);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(m_curl);

    if (res != CURLE_OK) {
        std::cerr << "Ollama REST error: " << curl_easy_strerror(res) << std::endl;
        return result;
    }

    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code != 200) {
        std::cerr << "Ollama HTTP " << http_code << std::endl;
        return result;
    }

    try {
        json response = json::parse(readBuffer);
        
        if (response.contains("models") && response["models"].is_array()) {
            for (const auto& model : response["models"]) {
                OllamaModel om;
                om.name = model.value("name", "unknown");
                om.id = om.name;  // Use name as ID for filtering
                om.description = model.value("name", "");  // Ollama returns name, not description
                om.size_bytes = model.value("size", 0);
                om.modified_at = model.value("modified_at", "");
                
                if (!om.name.empty()) {
                    result.push_back(om);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Ollama JSON parse error: " << e.what() << std::endl;
    }

    return result;
}

json OllamaRESTClient::getModelsJSON() {
    if (!m_curl) return json::array();

    std::string readBuffer;
    std::string url = m_baseUrl + "/api/tags";

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, (long)m_timeout_ms);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(m_curl);

    if (res != CURLE_OK) {
        return json::array();
    }

    try {
        json response = json::parse(readBuffer);
        return response.value("models", json::array());
    } catch (...) {
        return json::array();
    }
}

std::vector<OllamaRESTClient::OllamaModel> OllamaRESTClient::filterModels(
    const std::vector<OllamaModel>& models, 
    std::function<bool(const OllamaModel&)> predicate) {
    
    std::vector<OllamaModel> filtered;
    for (const auto& model : models) {
        if (predicate(model)) {
            filtered.push_back(model);
        }
    }
    return filtered;
}

const OllamaRESTClient::OllamaModel* OllamaRESTClient::findModelById(
    const std::vector<OllamaModel>& models, 
    const std::string& targetId) {
    
    for (const auto& model : models) {
        if (model.id == targetId) {
            return &model;
        }
    }
    return nullptr;
}
