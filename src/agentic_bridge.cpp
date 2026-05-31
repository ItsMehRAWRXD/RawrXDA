#include "agentic_bridge.hpp"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <chrono>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

// RAII scope guard for WinHTTP handles
struct HttpScope {
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    ~HttpScope() {
        if (hRequest) { WinHttpCloseHandle(hRequest); hRequest = nullptr; }
        if (hConnect) { WinHttpCloseHandle(hConnect); hConnect = nullptr; }
        if (hSession) { WinHttpCloseHandle(hSession); hSession = nullptr; }
    }
    HttpScope(const HttpScope&) = delete;
    HttpScope& operator=(const HttpScope&) = delete;
    HttpScope() = default;
};

static bool CrackUrl(const std::string& url, std::wstring& host, INTERNET_PORT& port, std::wstring& path) {
    std::wstring wUrl(url.begin(), url.end());
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    WCHAR hn[256]{}, up[512]{};
    urlComp.lpszHostName = hn; urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath  = up; urlComp.dwUrlPathLength  = 512;
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) return false;
    host = hn; port = urlComp.nPort; path = up;
    return true;
}

AIAgenticBridge::AIAgenticBridge() = default;
AIAgenticBridge::~AIAgenticBridge() { Shutdown(); }

bool AIAgenticBridge::Initialize(const std::string& endpoint, const std::string& defaultModel) {
    endpoint_ = endpoint;
    currentModel_ = defaultModel;
    HttpScope scope;
    scope.hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!scope.hSession) return false;
    std::wstring host, path; INTERNET_PORT port;
    if (!CrackUrl(endpoint, host, port, path)) return false;
    scope.hConnect = WinHttpConnect(scope.hSession, host.c_str(), port, 0);
    if (!scope.hConnect) return false;
    scope.hRequest = WinHttpOpenRequest(scope.hConnect, L"GET", L"/api/tags",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!scope.hRequest) return false;
    if (!WinHttpSendRequest(scope.hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
    if (!WinHttpReceiveResponse(scope.hRequest, nullptr)) return false;
    DWORD status = 0, statusSz = sizeof(status);
    WinHttpQueryHeaders(scope.hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSz, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) return false;
    connected_.store(true);
    running_.store(true);
    workerThread_ = std::thread(&AIAgenticBridge::ProcessQueue, this);
    return true;
}

void AIAgenticBridge::Shutdown() {
    running_.store(false);
    queueCV_.notify_all();
    if (currentCancelFlag_) currentCancelFlag_->store(true);
    if (workerThread_.joinable()) workerThread_.join();
    connected_.store(false);
}

void AIAgenticBridge::RequestCompletion(const std::string& context,
    std::function<void(const std::string&)> callback) {
    auto req = std::make_shared<AIRequest>();
    req->type = AIRequestType::COMPLETION;
    req->context = context;
    req->prompt = "Complete the following code (provide only the completion):\n\n" + context;
    req->callback = callback;
    req->timestamp = std::chrono::steady_clock::now();
    { std::lock_guard<std::mutex> lock(queueMutex_); requestQueue_.push(req); }
    queueCV_.notify_one();
}

void AIAgenticBridge::SendChat(const std::string& message,
    std::function<void(const std::string&)> callback) {
    auto req = std::make_shared<AIRequest>();
    req->type = AIRequestType::CHAT;
    req->prompt = message;
    req->callback = callback;
    req->timestamp = std::chrono::steady_clock::now();
    { std::lock_guard<std::mutex> lock(queueMutex_); requestQueue_.push(req); }
    queueCV_.notify_one();
}

void AIAgenticBridge::RequestExplanation(const std::string& code,
    std::function<void(const std::string&)> callback) {
    auto req = std::make_shared<AIRequest>();
    req->type = AIRequestType::EXPLAIN;
    req->prompt = "Explain this code concisely:\n\n" + code;
    req->callback = callback;
    req->timestamp = std::chrono::steady_clock::now();
    { std::lock_guard<std::mutex> lock(queueMutex_); requestQueue_.push(req); }
    queueCV_.notify_one();
}

void AIAgenticBridge::RequestRefactor(const std::string& code, const std::string& instruction,
    std::function<void(const std::string&)> callback) {
    auto req = std::make_shared<AIRequest>();
    req->type = AIRequestType::REFACTOR;
    req->prompt = "Refactor this code to " + instruction + " (provide only the refactored code):\n\n" + code;
    req->callback = callback;
    req->timestamp = std::chrono::steady_clock::now();
    { std::lock_guard<std::mutex> lock(queueMutex_); requestQueue_.push(req); }
    queueCV_.notify_one();
}

std::vector<std::string> AIAgenticBridge::GetAvailableModels() {
    std::vector<std::string> models;
    if (!connected_.load()) return models;
    HttpScope scope;
    scope.hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!scope.hSession) return models;
    std::wstring host, path; INTERNET_PORT port;
    if (!CrackUrl(endpoint_, host, port, path)) return models;
    scope.hConnect = WinHttpConnect(scope.hSession, host.c_str(), port, 0);
    if (!scope.hConnect) return models;
    scope.hRequest = WinHttpOpenRequest(scope.hConnect, L"GET", L"/api/tags",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!scope.hRequest) return models;
    if (!WinHttpSendRequest(scope.hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return models;
    if (!WinHttpReceiveResponse(scope.hRequest, nullptr)) return models;
    std::string fullResponse;
    DWORD size = 0;
    do {
        size = 0;
        if (!WinHttpQueryDataAvailable(scope.hRequest, &size) || size == 0) break;
        std::string chunk(size, '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(scope.hRequest, chunk.data(), size, &downloaded)) break;
        chunk.resize(downloaded);
        fullResponse += chunk;
    } while (size > 0);
    ParseModelList(fullResponse, models);
    return models;
}

bool AIAgenticBridge::ParseModelList(const std::string& json, std::vector<std::string>& models) {
    size_t pos = 0;
    while ((pos = json.find("\"name\"", pos)) != std::string::npos) {
        pos += 6;
        size_t colon = json.find(':', pos);
        if (colon == std::string::npos) continue;
        size_t q1 = json.find('"', colon);
        if (q1 == std::string::npos) continue;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        std::string name = json.substr(q1 + 1, q2 - q1 - 1);
        if (!name.empty()) models.push_back(name);
        pos = q2;
    }
    return !models.empty();
}

void AIAgenticBridge::CancelCurrentRequest() {
    if (currentCancelFlag_) currentCancelFlag_->store(true);
}

double AIAgenticBridge::GetAverageResponseTime() const {
    size_t count = requestCount_.load();
    if (count == 0) return 0.0;
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(responseMutex_));
    return totalResponseTime_ / count;
}

void AIAgenticBridge::ProcessQueue() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queueMutex_);
        queueCV_.wait(lock, [this] { return !requestQueue_.empty() || !running_.load(); });
        if (!running_.load()) break;
        if (requestQueue_.empty()) continue;
        auto req = requestQueue_.front(); requestQueue_.pop();
        lock.unlock();
        if (req->cancelled.load()) continue;
        processing_.store(true);
        currentCancelFlag_ = std::make_shared<std::atomic<bool>>(false);
        auto t0 = std::chrono::steady_clock::now();
        std::string response;
        bool ok = CallOllamaAPI(req->prompt, response, currentCancelFlag_);
        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        requestCount_++;
        { std::lock_guard<std::mutex> lk(responseMutex_); totalResponseTime_ += elapsed; }
        if (ok && !req->cancelled.load() && req->callback) req->callback(response);
        processing_.store(false);
        currentCancelFlag_.reset();
    }
}

bool AIAgenticBridge::CallOllamaAPI(const std::string& prompt, std::string& response,
    std::shared_ptr<std::atomic<bool>> cancelFlag) {
    HttpScope scope;
    scope.hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!scope.hSession) return false;
    std::wstring host, path; INTERNET_PORT port;
    if (!CrackUrl(endpoint_, host, port, path)) return false;
    scope.hConnect = WinHttpConnect(scope.hSession, host.c_str(), port, 0);
    if (!scope.hConnect) return false;
    scope.hRequest = WinHttpOpenRequest(scope.hConnect, L"POST", L"/api/generate",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!scope.hRequest) return false;
    // Build JSON payload (no external dep)
    std::string jsonData = "{\"model\":\"" + currentModel_ + "\",\"prompt\":\"";
    for (char c : prompt) {
        if (c == '"')  { jsonData += "\\\""; continue; }
        if (c == '\\') { jsonData += "\\\\"; continue; }
        if (c == '\n') { jsonData += "\\n";  continue; }
        if (c == '\r') { jsonData += "\\r";  continue; }
        if (c == '\t') { jsonData += "\\t";  continue; }
        jsonData += c;
    }
    jsonData += "\",\"stream\":false}";
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(scope.hRequest, headers.c_str(), (DWORD)headers.size(),
        (LPVOID)jsonData.data(), (DWORD)jsonData.size(), (DWORD)jsonData.size(), 0)) return false;
    if (!WinHttpReceiveResponse(scope.hRequest, nullptr)) return false;
    DWORD status = 0, statusSz = sizeof(status);
    WinHttpQueryHeaders(scope.hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSz, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) return false;
    std::string fullResponse;
    DWORD size = 0;
    do {
        if (cancelFlag && cancelFlag->load()) return false;
        size = 0;
        if (!WinHttpQueryDataAvailable(scope.hRequest, &size) || size == 0) break;
        std::string chunk(size, '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(scope.hRequest, chunk.data(), size, &downloaded)) break;
        chunk.resize(downloaded);
        fullResponse += chunk;
    } while (size > 0);
    // Extract "response" field from JSON
    size_t rpos = fullResponse.find("\"response\":");
    if (rpos == std::string::npos) return false;
    size_t q1 = fullResponse.find('"', rpos + 11);
    if (q1 == std::string::npos) return false;
    size_t q2 = q1 + 1;
    while (q2 < fullResponse.size()) {
        if (fullResponse[q2] == '\\' && q2 + 1 < fullResponse.size()) { q2 += 2; continue; }
        if (fullResponse[q2] == '"') break;
        q2++;
    }
    if (q2 >= fullResponse.size()) return false;
    std::string raw = fullResponse.substr(q1 + 1, q2 - q1 - 1);
    // Unescape
    response.clear();
    response.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char n = raw[++i];
            if (n == 'n') response += '\n';
            else if (n == 't') response += '\t';
            else if (n == 'r') response += '\r';
            else response += n;
        } else { response += raw[i]; }
    }
    return !response.empty();
}

} // namespace RawrXD
