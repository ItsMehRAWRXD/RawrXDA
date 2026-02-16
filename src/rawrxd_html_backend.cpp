/**
 * RawrXD-HTML-Backend — Standalone CLI backend for the HTML IDE only
 *
 * Non-dep exe: static CRT, Win32 + Winsock + WinHTTP only. No Node, no Qt, no Phase3 DLL.
 * Serves the API contract expected by the HTML IDE (launcher, chatbot, test harness).
 * Bridged to MASM/C++ backend: same API parity as tool_server; optional MASM model bridge
 * can be linked later via RAWR_HTML_BACKEND_HAS_MASM for /api/model/profiles etc.
 *
 * Build: cmake --build . --config Release --target RawrXD-HTML-Backend
 * Run:   RawrXD-HTML-Backend.exe [--port 11435] [--bind 127.0.0.1]
 *
 * Point the HTML IDE (or server.js backend URL) at http://127.0.0.1:11435
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Naive JSON extract (no nlohmann) for minimal deps
// ---------------------------------------------------------------------------
static std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t keyPos = json.find(search);
    if (keyPos == std::string::npos) return "";
    size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos) return "";
    size_t start = json.find('"', colon);
    if (start == std::string::npos) return "";
    start++;
    size_t end = start;
    while (end < json.size() && json[end] != '"') {
        if (json[end] == '\\') end++;
        end++;
    }
    if (end > json.size()) return "";
    std::string value = json.substr(start, end - start);
    std::string out;
    for (size_t i = 0; i < value.size(); i++) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            if (value[i + 1] == 'n') { out += '\n'; i++; continue; }
            if (value[i + 1] == 'r') { out += '\r'; i++; continue; }
            if (value[i + 1] == 't') { out += '\t'; i++; continue; }
            if (value[i + 1] == '"') { out += '"'; i++; continue; }
            if (value[i + 1] == '\\') { out += '\\'; i++; continue; }
        }
        out += value[i];
    }
    return out;
}

static std::string JsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string MakeErrorResponse(int code, const std::string& msg) {
    std::string json = "{\"error\":\"" + JsonEscape(msg) + "\"}";
    return "HTTP/1.1 " + std::to_string(code) + " Error\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
}

static std::string CorsHeaders() {
    return "Access-Control-Allow-Origin: *\r\n"
           "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
           "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With\r\n";
}

// ---------------------------------------------------------------------------
// Minimal HTTP server — HTML IDE API only
// ---------------------------------------------------------------------------
class HtmlBackendServer {
public:
    HtmlBackendServer(int port, const std::string& bindAddr)
        : port_(port), bindAddr_(bindAddr), running_(false), listenSocket_(INVALID_SOCKET),
          startTime_(std::chrono::steady_clock::now()), requestCount_(0) {}

    bool Start() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            std::fprintf(stderr, "[HTML-Backend] WSAStartup failed: %d\n", WSAGetLastError());
            return false;
        }
        listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket_ == INVALID_SOCKET) {
            std::fprintf(stderr, "[HTML-Backend] socket failed: %d\n", WSAGetLastError());
            WSACleanup();
            return false;
        }
        int reuse = 1;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(port_));
        if (bindAddr_ == "0.0.0.0" || bindAddr_.empty())
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            addr.sin_addr.s_addr = inet_addr(bindAddr_.c_str());

        if (bind(listenSocket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::fprintf(stderr, "[HTML-Backend] bind failed: %d\n", WSAGetLastError());
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }
        if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
            std::fprintf(stderr, "[HTML-Backend] listen failed: %d\n", WSAGetLastError());
            closesocket(listenSocket_);
            WSACleanup();
            return false;
        }
        running_ = true;
        thread_ = std::thread(&HtmlBackendServer::Loop, this);
        std::printf("[HTML-Backend] RawrXD HTML IDE backend on http://%s:%d (no-dep exe, MASM/C++ bridge)\n",
                    bindAddr_.empty() ? "127.0.0.1" : bindAddr_.c_str(), port_);
        return true;
    }

    void Stop() {
        running_ = false;
        if (listenSocket_ != INVALID_SOCKET) {
            closesocket(listenSocket_);
            listenSocket_ = INVALID_SOCKET;
        }
        if (thread_.joinable()) thread_.join();
        WSACleanup();
    }

private:
    int port_;
    std::string bindAddr_;
    std::atomic<bool> running_;
    SOCKET listenSocket_;
    std::thread thread_;
    std::chrono::steady_clock::time_point startTime_;
    std::atomic<uint64_t> requestCount_;

    std::string InjectCors(const std::string& resp) {
        size_t pos = resp.find("\r\n\r\n");
        if (pos == std::string::npos) return resp;
        return resp.substr(0, pos) + "\r\n" + CorsHeaders() + resp.substr(pos);
    }

    std::string ExtractBody(const std::string& req) {
        size_t pos = req.find("\r\n\r\n");
        return pos == std::string::npos ? "" : req.substr(pos + 4);
    }

    // ---- Handlers (parity with tool_server / HeadlessIDE) ----
    std::string HandleHealth() {
        std::string body = "{\"status\":\"ok\",\"version\":\"1.0.0\",\"backend\":\"rawrxd-html-backend\",\"models_loaded\":0}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string HandleStatus() {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        std::string body = "{\"running\":true,\"status\":\"ok\",\"version\":\"1.0.0\","
            "\"server\":\"RawrXD-HTML-Backend\",\"backend\":\"rawrxd-html-backend\","
            "\"pid\":" + std::to_string(GetCurrentProcessId()) + ","
            "\"uptime_seconds\":" + std::to_string(uptime) + ","
            "\"models_loaded\":0,\"model_loaded\":false,"
            "\"capabilities\":{\"cli_endpoint\":true,\"read_file\":true,\"list_dir\":true,\"ollama_proxy\":true,\"extension_creator\":true,\"copilot_creator\":true}}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string HandleTags() {
        // Proxy to Ollama /api/tags if available; else stub
        std::string host = "localhost";
        int ollamaPort = 11434;
        if (getenv("OLLAMA_HOST")) host = getenv("OLLAMA_HOST");
        if (getenv("OLLAMA_PORT")) ollamaPort = atoi(getenv("OLLAMA_PORT"));
        if (ollamaPort == port_) ollamaPort = 11434;

        std::wstring wHost(host.begin(), host.end());
        HINTERNET hSession = WinHttpOpen(L"RawrXD-HTML-Backend/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            std::string body = "{\"models\":[],\"message\":\"Ollama proxy unavailable (WinHttp)\"}";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(body.size()) + "\r\n\r\n" + body;
        }
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)ollamaPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); goto no_ollama; }
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", nullptr, nullptr, nullptr, 0);
        if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); goto no_ollama; }
        WinHttpSetTimeouts(hReq, 2000, 3000, 5000, 5000);
        if (!WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(hReq, nullptr)) {
            WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            goto no_ollama;
        }
        std::string body;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
            std::vector<char> buf(avail + 1, 0);
            DWORD read = 0;
            WinHttpReadData(hReq, buf.data(), avail, &read);
            body.append(buf.data(), read);
        }
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (!body.empty()) {
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(body.size()) + "\r\n\r\n" + body;
        }
no_ollama:
        body = "{\"models\":[],\"message\":\"Ollama not reachable; load a model via IDE or start Ollama\"}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string HandleGenerate(const std::string& body) {
        std::string host = "localhost";
        int ollamaPort = 11434;
        if (getenv("OLLAMA_HOST")) host = getenv("OLLAMA_HOST");
        if (getenv("OLLAMA_PORT")) ollamaPort = atoi(getenv("OLLAMA_PORT"));
        if (ollamaPort == port_) ollamaPort = 11434;

        std::wstring wHost(host.begin(), host.end());
        HINTERNET hSession = WinHttpOpen(L"RawrXD-HTML-Backend/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return MakeErrorResponse(502, "WinHttpOpen failed");
        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), (INTERNET_PORT)ollamaPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return MakeErrorResponse(502, "Cannot connect to Ollama"); }
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", nullptr, nullptr, nullptr, 0);
        if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return MakeErrorResponse(502, "OpenRequest failed"); }
        WinHttpSetTimeouts(hReq, 5000, 10000, 120000, 120000);
        BOOL sent = WinHttpSendRequest(hReq, L"Content-Type: application/json", -1,
                                        (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (!sent) {
            WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Send to Ollama failed — is Ollama running?");
        }
        if (!WinHttpReceiveResponse(hReq, nullptr)) {
            WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
            return MakeErrorResponse(502, "Ollama response failed");
        }
        DWORD status = 0, statusLen = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusLen, WINHTTP_NO_HEADER_INDEX);
        std::string respBody;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0) {
            std::vector<char> buf(avail + 1, 0);
            DWORD read = 0;
            WinHttpReadData(hReq, buf.data(), avail, &read);
            respBody.append(buf.data(), read);
        }
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return "HTTP/1.1 " + std::to_string(status) + " OK\r\nContent-Type: application/json\r\n"
               "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
    }

    std::string HandleReadFile(const std::string& body) {
        std::string path = ExtractJsonValue(body, "path");
        if (path.empty()) return MakeErrorResponse(400, "Missing 'path'");
        try {
            fs::path p = fs::weakly_canonical(path);
            if (!fs::exists(p) || !fs::is_regular_file(p))
                return MakeErrorResponse(404, "File not found: " + path);
            std::ifstream f(p, std::ios::binary);
            if (!f) return MakeErrorResponse(500, "Cannot open file");
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();
            if (content.size() > 1024 * 1024) {
                content = content.substr(0, 1024 * 1024);
                content += "\n\n[... truncated at 1MB ...]";
            }
            std::string json = "{\"success\":true,\"path\":\"" + JsonEscape(p.string()) + "\",\"content\":\"" + JsonEscape(content) + "\",\"size\":" + std::to_string(content.size()) + "}";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(json.size()) + "\r\n\r\n" + json;
        } catch (const std::exception& e) {
            return MakeErrorResponse(500, std::string("Read error: ") + e.what());
        }
    }

    std::string HandleListDir(const std::string& body) {
        std::string path = ExtractJsonValue(body, "path");
        if (path.empty()) path = ".";
        try {
            fs::path p = fs::weakly_canonical(path);
            if (!fs::exists(p) || !fs::is_directory(p))
                return MakeErrorResponse(404, "Directory not found: " + path);
            std::string entries = "[";
            bool first = true;
            for (const auto& e : fs::directory_iterator(p)) {
                if (!first) entries += ",";
                first = false;
                std::string name = e.path().filename().string();
                std::string type = e.is_directory() ? "dir" : "file";
                int64_t size = e.is_directory() ? 0 : (int64_t)e.file_size();
                entries += "{\"name\":\"" + JsonEscape(name) + "\",\"type\":\"" + type + "\",\"size\":" + std::to_string(size) + "}";
            }
            entries += "]";
            std::string json = "{\"entries\":" + entries + ",\"path\":\"" + JsonEscape(p.string()) + "\"}";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                   std::to_string(json.size()) + "\r\n\r\n" + json;
        } catch (const std::exception& e) {
            return MakeErrorResponse(500, std::string("List error: ") + e.what());
        }
    }

    std::string HandleFailures() {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        std::string body = "{\"failures\":[],\"stats\":{\"totalFailures\":0,\"totalRetries\":0,\"successAfterRetry\":0,\"retriesDeclined\":0,\"topReasons\":[]},\"uptime_seconds\":" + std::to_string(uptime) + "}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string HandleAgentsStatus() {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime_).count();
        std::string body = "{\"agents\":{\"failure_detector\":{\"active\":true},\"puppeteer\":{\"active\":true}},\"server_uptime\":" + std::to_string(uptime) + ",\"total_events\":0,\"status\":\"operational\"}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    std::string HandleCli(const std::string& body) {
        std::string command = ExtractJsonValue(body, "command");
        if (command.empty()) return MakeErrorResponse(400, "Missing 'command'");
        size_t s = command.find_first_not_of(" \t\n\r");
        size_t e = command.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) command = command.substr(s, e - s + 1);
        if (command.empty()) return MakeErrorResponse(400, "Empty command");

        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE hOutRead = nullptr, hOutWrite = nullptr;
        if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0) || !SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0)) {
            return MakeErrorResponse(500, "CreatePipe failed");
        }
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hOutWrite;
        si.hStdError = hOutWrite;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};
        std::string cmdLine = "cmd.exe /c " + command;
        if (!CreateProcessA(nullptr, (LPSTR)cmdLine.c_str(), nullptr, nullptr, TRUE,
                            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hOutRead); CloseHandle(hOutWrite);
            return MakeErrorResponse(500, "CreateProcess failed");
        }
        CloseHandle(hOutWrite);
        std::string output;
        char buf[512];
        DWORD read = 0;
        while (ReadFile(hOutRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = '\0';
            output += buf;
        }
        CloseHandle(hOutRead);
        WaitForSingleObject(pi.hProcess, 30000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        std::string json = "{\"success\":true,\"output\":\"" + JsonEscape(output) + "\",\"command\":\"" + JsonEscape(command) + "\"}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(json.size()) + "\r\n\r\n" + json;
    }

    std::string HandleMetrics() {
        uint64_t n = requestCount_.load();
        std::string body = "{\"metrics\":{\"total_requests\":" + std::to_string(n) + ",\"status\":\"ok\"}}";
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
               std::to_string(body.size()) + "\r\n\r\n" + body;
    }

    // Extension Creator — pure no-dep: write plugin manifest + stub to %APPDATA%\\RawrXD\\craft_room\\plugins
    std::string HandleExtensionCreate(const std::string& body) {
        std::string name = ExtractJsonValue(body, "name");
        std::string type = ExtractJsonValue(body, "type");
        if (name.empty()) name = "CustomPlugin";
        if (type.empty()) type = "Custom";
        const char* env = getenv("APPDATA");
        std::string base = env ? env : ".";
        base += "\\RawrXD\\craft_room\\plugins";
        fs::path dir = fs::path(base) / name;
        try {
            fs::create_directories(dir);
            std::string manifest = "{\"name\":\"" + name + "\",\"type\":\"" + type + "\",\"version\":\"1.0.0\",\"created_by\":\"RawrXD-HTML-Backend\"}";
            fs::path manifestPath = dir / "manifest.json";
            std::ofstream mf(manifestPath);
            if (mf) { mf << manifest; mf.close(); }
            std::string stub = "# " + name + " — RawrXD extension (local model / IDE)\nfunction Invoke-" + name + " { param([string]$Input) $Input }\nExport-ModuleMember -Function Invoke-" + name;
            fs::path stubPath = dir / (name + ".psm1");
            std::ofstream sf(stubPath);
            if (sf) { sf << stub; sf.close(); }
            std::string json = "{\"success\":true,\"path\":\"" + JsonEscape(dir.string()) + "\",\"message\":\"Extension creator: plugin created in pure no-dep backend\"}";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
        } catch (const std::exception& e) {
            return MakeErrorResponse(500, std::string("Extension create failed: ") + e.what());
        }
    }

    // Copilot Creator — pure no-dep: desktop copilot using local models (Ollama), IDE-driven
    std::string HandleCopilotCreate(const std::string& body) {
        std::string name = ExtractJsonValue(body, "name");
        std::string model = ExtractJsonValue(body, "model");
        if (name.empty()) name = "DesktopCopilot";
        if (model.empty()) model = "llama3.2";
        const char* env = getenv("APPDATA");
        std::string base = env ? env : ".";
        base += "\\RawrXD\\copilots";
        fs::path dir = fs::path(base) / name;
        try {
            fs::create_directories(dir);
            std::string manifest = "{\"name\":\"" + name + "\",\"model\":\"" + model + "\",\"backend\":\"ollama\",\"created_by\":\"RawrXD-HTML-Backend\",\"desktop_copilot\":true}";
            fs::path manifestPath = dir / "manifest.json";
            std::ofstream mf(manifestPath);
            if (mf) { mf << manifest; mf.close(); }
            std::string config = "{\"ollama_host\":\"localhost\",\"ollama_port\":11434,\"model\":\"" + model + "\"}";
            fs::path configPath = dir / "config.json";
            std::ofstream cf(configPath);
            if (cf) { cf << config; cf.close(); }
            std::string json = "{\"success\":true,\"path\":\"" + JsonEscape(dir.string()) + "\",\"model\":\"" + JsonEscape(model) + "\",\"message\":\"Copilot creator: desktop copilot (local model) created in pure no-dep backend\"}";
            return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(json.size()) + "\r\n\r\n" + json;
        } catch (const std::exception& e) {
            return MakeErrorResponse(500, std::string("Copilot create failed: ") + e.what());
        }
    }

    void Loop() {
        while (running_ && listenSocket_ != INVALID_SOCKET) {
            sockaddr_in clientAddr = {};
            int len = sizeof(clientAddr);
            SOCKET client = accept(listenSocket_, (sockaddr*)&clientAddr, &len);
            if (client == INVALID_SOCKET) continue;
            char buf[8192];
            int n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                requestCount_++;
                std::string response = Dispatch(std::string(buf));
                response = InjectCors(response);
                send(client, response.c_str(), (int)response.size(), 0);
            }
            closesocket(client);
        }
    }

    std::string Dispatch(const std::string& request) {
        std::istringstream iss(request);
        std::string method, path, version;
        iss >> method >> path >> version;
        std::string query;
        size_t q = path.find('?');
        if (q != std::string::npos) {
            query = path.substr(q + 1);
            path = path.substr(0, q);
        }
        std::string body = ExtractBody(request);

        if (method == "OPTIONS") {
            return "HTTP/1.1 204 No Content\r\n" + CorsHeaders() + "Content-Length: 0\r\n\r\n";
        }

        if (method == "GET" && path == "/health") return HandleHealth();
        if (method == "GET" && (path == "/status" || path == "/api/status")) return HandleStatus();
        if (method == "GET" && path == "/api/tags") return HandleTags();
        if (method == "GET" && (path == "/models" || path == "/api/models")) return HandleTags();
        if (method == "POST" && path == "/api/generate") return HandleGenerate(body);
        if (method == "POST" && path == "/api/read-file") return HandleReadFile(body);
        if (method == "POST" && path == "/api/list-dir") return HandleListDir(body);
        if (method == "GET" && path == "/api/failures") return HandleFailures();
        if (method == "GET" && path == "/api/agents/status") return HandleAgentsStatus();
        if (method == "POST" && path == "/api/cli") return HandleCli(body);
        if (method == "GET" && (path == "/api/metrics" || path == "/metrics")) return HandleMetrics();

        return MakeErrorResponse(404, "Not found: " + path);
    }
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    int port = 11435;
    std::string bindAddr = "127.0.0.1";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) { port = atoi(argv[++i]); continue; }
        if (a == "--bind" && i + 1 < argc) { bindAddr = argv[++i]; continue; }
        if (a == "--help" || a == "-h") {
            std::printf("RawrXD-HTML-Backend — standalone CLI backend for the HTML IDE\n"
                        "Usage: RawrXD-HTML-Backend.exe [--port 11435] [--bind 127.0.0.1]\n"
                        "Point the HTML IDE at http://127.0.0.1:%d\n", port);
            return 0;
        }
    }

    HtmlBackendServer server(port, bindAddr);
    if (!server.Start()) return 1;

    while (true) {
        Sleep(1000);
    }
    server.Stop();
    return 0;
}
