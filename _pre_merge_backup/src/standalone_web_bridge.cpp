#include "standalone_web_bridge.hpp"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <wincrypt.h> // CryptAcquireContext/CryptCreateHash/CryptBinaryToStringA

#include <algorithm>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

#include "standalone_llama_runtime.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")

namespace {
RawrXD::Standalone::LlamaRuntime g_llama;
}

namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;
    bool is_websocket = false;
    std::string ws_key;
};

static std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && !not_space(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool parse_http_request(const std::string& data, HttpRequest& out) {
    std::istringstream iss(data);
    std::string line;
    if (!std::getline(iss, line)) return false;

    // Request line: METHOD SP PATH SP HTTP/1.1
    {
        std::istringstream rl(line);
        rl >> out.method;
        rl >> out.path;
        if (out.method.empty() || out.path.empty()) return false;
    }

    while (std::getline(iss, line)) {
        if (line == "\r" || line.empty()) break;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string k = lower(trim(line.substr(0, colon)));
        std::string v = trim(line.substr(colon + 1));
        out.headers.emplace_back(std::move(k), std::move(v));
    }

    for (const auto& [k, v] : out.headers) {
        if (k == "upgrade" && lower(v) == "websocket") out.is_websocket = true;
        if (k == "sec-websocket-key") out.ws_key = trim(v);
    }
    return true;
}

static bool send_all(SOCKET s, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = ::send(s, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

static std::string read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

static void send_http(SOCKET s, int code, const char* content_type, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << (code == 200 ? " OK" : " ERROR") << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    auto hdr = oss.str();
    send_all(s, hdr.data(), static_cast<int>(hdr.size()));
    if (!body.empty()) send_all(s, body.data(), static_cast<int>(body.size()));
}

static std::string http_date_rfc1123() {
    SYSTEMTIME st{};
    GetSystemTime(&st);
    char buf[128] = {};
    static const char* wday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    std::snprintf(buf, sizeof(buf), "%s, %02u %s %04u %02u:%02u:%02u GMT",
                  wday[st.wDayOfWeek], st.wDay, mon[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static void send_http_keepalive(SOCKET s, int code, const char* content_type, const std::string& body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << (code == 200 ? " OK" : " ERROR") << "\r\n";
    oss << "Connection: close\r\n";
    oss << "Date: " << http_date_rfc1123() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Content-Type: " << content_type << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "\r\n";
    auto hdr = oss.str();
    send_all(s, hdr.data(), static_cast<int>(hdr.size()));
    if (!body.empty()) send_all(s, body.data(), static_cast<int>(body.size()));
}

static std::string header_value(const HttpRequest& req, const std::string& key_lower) {
    for (const auto& [k, v] : req.headers) {
        if (k == key_lower) return v;
    }
    return {};
}

static bool has_path_traversal(const std::string& p) {
    // Hard fail on .. segments (we serve from a local web_root).
    return p.find("..") != std::string::npos;
}

static std::string path_without_query(const std::string& p) {
    auto q = p.find('?');
    if (q == std::string::npos) return p;
    return p.substr(0, q);
}

static std::string query_string(const std::string& p) {
    auto q = p.find('?');
    if (q == std::string::npos) return {};
    return p.substr(q + 1);
}

static int from_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '+') {
            out.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < s.size()) {
            int hi = from_hex(s[i + 1]);
            int lo = from_hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

static std::string query_param(const std::string& qs, const std::string& key) {
    size_t i = 0;
    while (i < qs.size()) {
        size_t amp = qs.find('&', i);
        if (amp == std::string::npos) amp = qs.size();
        size_t eq = qs.find('=', i);
        if (eq != std::string::npos && eq < amp) {
            std::string k = qs.substr(i, eq - i);
            if (k == key) return url_decode(qs.substr(eq + 1, amp - (eq + 1)));
        } else {
            if (qs.substr(i, amp - i) == key) return {};
        }
        i = amp + 1;
    }
    return {};
}

static std::wstring widen_utf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w;
    w.resize((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

static std::string narrow_utf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s;
    s.resize((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), s.data(), n, nullptr, nullptr);
    return s;
}

static std::wstring models_dir_from_webroot(const std::string& web_root) {
    std::wstring wr = widen_utf8(web_root);
    if (!wr.empty() && (wr.back() != L'\\' && wr.back() != L'/')) wr += L'\\';
    wr += L"models";
    return wr;
}

static bool is_safe_model_name(const std::string& model) {
    if (model.empty()) return false;
    if (model.find("..") != std::string::npos) return false;
    if (model.find('\\') != std::string::npos) return false;
    if (model.find('/') != std::string::npos) return false;
    if (model.find(':') != std::string::npos) return false;
    return true;
}

static bool ensure_dir_exists(const std::wstring& dir) {
    DWORD a = GetFileAttributesW(dir.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return true;
    return CreateDirectoryW(dir.c_str(), nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static bool write_file_atomic(const std::wstring& path, const std::string& bytes, bool overwrite, std::string& error) {
    error.clear();

    std::wstring tmp = path + L".tmp";
    DWORD disp = overwrite ? CREATE_ALWAYS : CREATE_NEW;
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, disp, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_EXISTS || e == ERROR_ALREADY_EXISTS) error = "File exists";
        else error = "CreateFile failed";
        return false;
    }

    DWORD wrote = 0;
    const char* p = bytes.data();
    size_t left = bytes.size();
    while (left > 0) {
        DWORD chunk = left > 0x7ffff000u ? 0x7ffff000u : (DWORD)left;
        if (!WriteFile(h, p, chunk, &wrote, nullptr) || wrote != chunk) {
            CloseHandle(h);
            DeleteFileW(tmp.c_str());
            error = "WriteFile failed";
            return false;
        }
        p += chunk;
        left -= chunk;
    }
    FlushFileBuffers(h);
    CloseHandle(h);

    // Replace target.
    if (overwrite) {
        DeleteFileW(path.c_str());
    }
    if (!MoveFileW(tmp.c_str(), path.c_str())) {
        DeleteFileW(tmp.c_str());
        error = "MoveFile failed";
        return false;
    }
    return true;
}

static std::wstring resolve_model_path(const StandaloneWebBridgeServer::Config& cfg, const std::string& model) {
    if (!is_safe_model_name(model)) return {};

    std::wstring dir = models_dir_from_webroot(cfg.web_root);
    std::wstring base = widen_utf8(model);

    std::wstring p1 = dir;
    if (!p1.empty() && p1.back() != L'\\') p1 += L'\\';
    p1 += base;
    if (GetFileAttributesW(p1.c_str()) != INVALID_FILE_ATTRIBUTES) return p1;

    std::wstring p2 = p1 + L".gguf";
    if (GetFileAttributesW(p2.c_str()) != INVALID_FILE_ATTRIBUTES) return p2;

    return {};
}

static nlohmann::json list_models_json(const StandaloneWebBridgeServer::Config& cfg) {
    nlohmann::json out;
    out["models"] = nlohmann::json::array();

    std::wstring dir = models_dir_from_webroot(cfg.web_root);
    std::wstring pattern = dir;
    if (!pattern.empty() && pattern.back() != L'\\') pattern += L'\\';
    pattern += L"*.gguf";

    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        nlohmann::json m;
        m["name"] = narrow_utf8(std::wstring(fd.cFileName));
        out["models"].push_back(std::move(m));
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    return out;
}

static std::string chat_messages_to_prompt(const nlohmann::json& messages) {
    if (!messages.is_array()) return {};
    std::ostringstream oss;
    for (const auto& m : messages) {
        if (!m.is_object()) continue;
        const std::string role = m.value("role", "");
        const std::string content = m.value("content", "");
        if (role.empty() && content.empty()) continue;
        if (!role.empty()) oss << role << ": ";
        oss << content << "\n";
    }
    return oss.str();
}

static bool sha1_base64_websocket_accept(const std::string& key, std::string& out_accept) {
    static const char* kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string in = key + kGuid;

    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    BYTE digest[20] = {};
    DWORD digest_len = sizeof(digest);
    if (!CryptAcquireContextA(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return false;
    if (!CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return false;
    }
    bool ok = false;
    if (CryptHashData(hash, reinterpret_cast<const BYTE*>(in.data()), static_cast<DWORD>(in.size()), 0) &&
        CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_len, 0)) {
        DWORD b64_len = 0;
        if (CryptBinaryToStringA(digest, digest_len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64_len)) {
            std::string b64;
            b64.resize(b64_len);
            if (CryptBinaryToStringA(digest, digest_len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(), &b64_len)) {
                // b64_len includes terminating NUL.
                if (!b64.empty() && b64.back() == '\0') b64.pop_back();
                out_accept = b64;
                ok = true;
            }
        }
    }
    if (hash) CryptDestroyHash(hash);
    if (prov) CryptReleaseContext(prov, 0);
    return ok;
}

static bool ws_recv_frame(SOCKET s, uint8_t& opcode, std::vector<uint8_t>& payload) {
    opcode = 0;
    payload.clear();

    uint8_t hdr[2] = {};
    int n = recv(s, reinterpret_cast<char*>(hdr), 2, MSG_WAITALL);
    if (n != 2) return false;

    bool fin = (hdr[0] & 0x80) != 0;
    opcode = hdr[0] & 0x0F;
    bool masked = (hdr[1] & 0x80) != 0;
    uint64_t len = (hdr[1] & 0x7F);

    if (len == 126) {
        uint16_t l16 = 0;
        if (recv(s, reinterpret_cast<char*>(&l16), 2, MSG_WAITALL) != 2) return false;
        len = ntohs(l16);
    } else if (len == 127) {
        uint64_t l64 = 0;
        if (recv(s, reinterpret_cast<char*>(&l64), 8, MSG_WAITALL) != 8) return false;
        len = _byteswap_uint64(l64);
    }

    uint8_t mask[4] = {};
    if (masked) {
        if (recv(s, reinterpret_cast<char*>(mask), 4, MSG_WAITALL) != 4) return false;
    }

    payload.resize(static_cast<size_t>(len));
    if (len > 0) {
        if (recv(s, reinterpret_cast<char*>(payload.data()), static_cast<int>(len), MSG_WAITALL) != static_cast<int>(len)) return false;
    }

    if (masked) {
        for (size_t i = 0; i < payload.size(); i++) payload[i] ^= mask[i & 3];
    }

    // MVP: only accept single-frame messages.
    if (!fin && opcode != 0x0) return false;
    return true;
}

static bool ws_send_text(SOCKET s, const std::string& text) {
    std::vector<uint8_t> out;
    out.push_back(0x81);
    if (text.size() < 126) {
        out.push_back(static_cast<uint8_t>(text.size()));
    } else if (text.size() <= 0xFFFF) {
        out.push_back(126);
        uint16_t l16 = htons(static_cast<uint16_t>(text.size()));
        out.insert(out.end(), reinterpret_cast<uint8_t*>(&l16), reinterpret_cast<uint8_t*>(&l16) + 2);
    } else {
        out.push_back(127);
        uint64_t l64 = _byteswap_uint64(static_cast<uint64_t>(text.size()));
        out.insert(out.end(), reinterpret_cast<uint8_t*>(&l64), reinterpret_cast<uint8_t*>(&l64) + 8);
    }
    out.insert(out.end(), text.begin(), text.end());
    return send_all(s, reinterpret_cast<const char*>(out.data()), static_cast<int>(out.size()));
}

static void ws_send_control(SOCKET s, uint8_t opcode) {
    uint8_t frame[2] = {static_cast<uint8_t>(0x80 | (opcode & 0x0F)), 0x00};
    send_all(s, reinterpret_cast<const char*>(frame), 2);
}

} // namespace

StandaloneWebBridgeServer::StandaloneWebBridgeServer(Config cfg) : m_cfg(std::move(cfg)) {}

StandaloneWebBridgeServer::~StandaloneWebBridgeServer() {
    stop();
}

bool StandaloneWebBridgeServer::start() {
    if (m_running.exchange(true)) return true;
    m_start_tick = GetTickCount64();
    resolve_default_web_root();
    m_http_thread = std::thread([this] { run_http(); });
    m_ws_thread = std::thread([this] { run_ws(); });
    return true;
}

void StandaloneWebBridgeServer::stop() {
    if (!m_running.exchange(false)) return;
    if (m_http_listen_socket != -1) {
        ::closesocket(static_cast<SOCKET>(m_http_listen_socket));
        m_http_listen_socket = -1;
    }
    if (m_ws_listen_socket != -1) {
        ::closesocket(static_cast<SOCKET>(m_ws_listen_socket));
        m_ws_listen_socket = -1;
    }
    if (m_http_thread.joinable()) m_http_thread.join();
    if (m_ws_thread.joinable()) m_ws_thread.join();
}

void StandaloneWebBridgeServer::run_http() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        m_running = false;
        return;
    }

    SOCKET ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) {
        WSACleanup();
        m_running = false;
        return;
    }
    m_http_listen_socket = static_cast<int>(ls);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_cfg.http_port);

    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(ls);
        WSACleanup();
        m_running = false;
        return;
    }
    if (listen(ls, SOMAXCONN) != 0) {
        closesocket(ls);
        WSACleanup();
        m_running = false;
        return;
    }

    while (m_running.load()) {
        SOCKET cs = accept(ls, nullptr, nullptr);
        if (cs == INVALID_SOCKET) break;
        std::thread([this, cs] {
            handle_http_client(static_cast<int>(cs));
            closesocket(cs);
        }).detach();
    }

    closesocket(ls);
    WSACleanup();
}

void StandaloneWebBridgeServer::handle_http_client(int client_socket) {
    SOCKET s = static_cast<SOCKET>(client_socket);

    std::string raw;
    raw.reserve(16 * 1024);

    // Read until end of headers (or cap out).
    for (;;) {
        char buf[8192];
        int n = recv(s, buf, static_cast<int>(sizeof(buf)), 0);
        if (n <= 0) return;
        raw.append(buf, buf + n);
        if (raw.find("\r\n\r\n") != std::string::npos) break;
        if (raw.size() > 64 * 1024) {
            send_http_keepalive(s, 431, "text/plain; charset=utf-8", "Headers too large");
            return;
        }
    }

    m_http_requests.fetch_add(1, std::memory_order_relaxed);

    HttpRequest req;
    if (!parse_http_request(raw, req)) return;

    if (has_path_traversal(req.path)) {
        send_http_keepalive(s, 400, "text/plain; charset=utf-8", "Invalid path");
        return;
    }

    // Handle CORS preflight (for fetch() / embedded browser consistency).
    if (req.method == "OPTIONS") {
        send_http_keepalive(s, 204, "text/plain; charset=utf-8", "");
        return;
    }

    // Read request body (if any).
    std::string req_body;
    {
        size_t hdr_end = raw.find("\r\n\r\n");
        size_t body_off = (hdr_end == std::string::npos) ? raw.size() : (hdr_end + 4);
        std::string cl = header_value(req, "content-length");
        uint64_t need = 0;
        if (!cl.empty()) need = std::strtoull(cl.c_str(), nullptr, 10);
        if (need > 0) {
            if (raw.size() > body_off) req_body.assign(raw.data() + body_off, raw.data() + raw.size());
            while (req_body.size() < need) {
                char buf2[8192];
                int n2 = recv(s, buf2, static_cast<int>(sizeof(buf2)), 0);
                if (n2 <= 0) break;
                req_body.append(buf2, buf2 + n2);
                if (req_body.size() > 32 * 1024 * 1024) {
                    send_http_keepalive(s, 413, "text/plain; charset=utf-8", "Payload too large");
                    return;
                }
            }
            if (req_body.size() > need) req_body.resize(static_cast<size_t>(need));
        }
    }

    const std::string route = path_without_query(req.path);
    const std::string qs = query_string(req.path);

    if (route == "/api/status" || route == "/health" || route == "/status") {
        nlohmann::json j;
        j["status"] = "ok";
        j["server"] = "RawrXD-Standalone-WebBridge";
        j["backend"] = "rawrxd-standalone-bridge";
        j["version"] = "qtfree";
        j["capabilities"] = nlohmann::json{
            {"models", true},
            {"generate", true},
            {"chat", true},
            {"model_blob_upload", true},
        };
        send_http_keepalive(s, 200, "application/json", j.dump());
        return;
    }

    if (route == "/api/stats") {
        uint64_t up_ms = GetTickCount64() - m_start_tick;
        std::ostringstream oss;
        oss << "uptime_ms=" << up_ms << "\n";
        oss << "http_port=" << m_cfg.http_port << "\n";
        oss << "ws_port=" << m_cfg.ws_port << "\n";
        oss << "http_requests=" << m_http_requests.load() << "\n";
        oss << "ws_requests=" << m_ws_requests.load() << "\n";
        oss << "model_requests=" << m_model_requests.load() << "\n";
        oss << "models_dir=" << m_cfg.web_root << "\\models\n";
        send_http_keepalive(s, 200, "text/plain; charset=utf-8", oss.str());
        return;
    }

    // Local model listing for the Qt-eliminated web UI (Ollama-compatible shape).
    if (route == "/api/tags" && req.method == "GET") {
        send_http_keepalive(s, 200, "application/json", list_models_json(m_cfg).dump());
        return;
    }

    // Upload a model blob (raw bytes) into web_root\\models\\<name>.gguf
    // Request: POST /api/models/upload?name=phi.gguf  (Content-Type: application/octet-stream)
    // Response: { ok, name }
    if (route == "/api/models/upload" && req.method == "POST") {
        const std::string name = query_param(qs, "name");
        if (!is_safe_model_name(name)) {
            send_http_keepalive(s, 400, "application/json", "{\"error\":\"Invalid name\"}");
            return;
        }
        if (req_body.empty()) {
            send_http_keepalive(s, 400, "application/json", "{\"error\":\"Empty body\"}");
            return;
        }
        if (req_body.size() > (size_t)8 * 1024 * 1024 * 1024ull) {
            send_http_keepalive(s, 413, "application/json", "{\"error\":\"Blob too large\"}");
            return;
        }

        std::wstring dir = models_dir_from_webroot(m_cfg.web_root);
        if (!ensure_dir_exists(dir)) {
            send_http_keepalive(s, 500, "application/json", "{\"error\":\"Failed to create models dir\"}");
            return;
        }

        std::wstring path = dir;
        if (!path.empty() && path.back() != L'\\') path += L'\\';
        path += widen_utf8(name);
        if (path.size() < 5 || _wcsicmp(path.c_str() + (path.size() - 5), L".gguf") != 0) {
            path += L".gguf";
        }

        const bool overwrite = query_param(qs, "overwrite") == "1";
        std::string write_err;
        if (!write_file_atomic(path, req_body, overwrite, write_err)) {
            nlohmann::json e = {{"error", write_err}};
            send_http_keepalive(s, 409, "application/json", e.dump());
            return;
        }

        nlohmann::json ok = {{"ok", true}, {"name", name}};
        send_http_keepalive(s, 200, "application/json", ok.dump());
        return;
    }

    // Local inference endpoints (Ollama-compatible shape; non-streaming only).
    if ((route == "/api/generate" || route == "/api/chat") && req.method == "POST") {
        m_model_requests.fetch_add(1, std::memory_order_relaxed);

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(req_body);
        } catch (...) {
            send_http_keepalive(s, 400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        const std::string model_name = j.value("model", m_cfg.default_model);
        std::wstring gguf = resolve_model_path(m_cfg, model_name);
        if (gguf.empty()) {
            send_http_keepalive(s, 404, "application/json", "{\"error\":\"Model not found under web_root\\\\models\"}");
            return;
        }

        std::string init_err;
        if (!g_llama.ensure_initialized(init_err)) {
            nlohmann::json e = {{"error", init_err}};
            send_http_keepalive(s, 500, "application/json", e.dump());
            return;
        }

        int32_t gpu_layers = j.value("gpu_layers", 999);
        if (gpu_layers < 0) gpu_layers = 0;

        std::string load_err;
        if (!g_llama.ensure_model_loaded(gguf, gpu_layers, load_err)) {
            nlohmann::json e = {{"error", load_err}};
            send_http_keepalive(s, 500, "application/json", e.dump());
            return;
        }

        std::string prompt;
        if (route == "/api/chat") {
            prompt = chat_messages_to_prompt(j.value("messages", nlohmann::json::array()));
        } else {
            prompt = j.value("prompt", "");
        }
        if (prompt.empty()) {
            send_http_keepalive(s, 400, "application/json", "{\"error\":\"Missing prompt\"}");
            return;
        }

        const int32_t max_tokens = j.value("num_predict", 512);
        auto r = g_llama.generate(prompt, max_tokens);
        if (!r.ok) {
            nlohmann::json e = {{"error", r.error}};
            send_http_keepalive(s, 500, "application/json", e.dump());
            return;
        }

        nlohmann::json resp;
        resp["model"] = model_name;
        resp["done"] = true;
        resp["response"] = r.text;
        resp["tokens_prompt"] = r.prompt_tokens;
        resp["tokens_predicted"] = r.generated_tokens;
        resp["t_prompt_ms"] = r.t_prompt_ms;
        resp["t_gen_ms"] = r.t_gen_ms;
        send_http_keepalive(s, 200, "application/json", resp.dump());
        return;
    }

    // Serve the UI by default.
    std::string path = route;
    if (path == "/" || path == "/index.html") path = "/standalone_interface.html";
    std::string full = m_cfg.web_root;
    if (!full.empty() && (full.back() != '\\' && full.back() != '/')) full += "\\";
    // simple path join, no traversal hardening (local-only dev tool)
    if (!path.empty() && (path.front() == '/' || path.front() == '\\')) path.erase(path.begin());
    full += path;

    auto file_body = read_file(full);
    if (file_body.empty()) {
        send_http_keepalive(s, 404, "text/plain; charset=utf-8", "Not found");
        return;
    }
    const char* ct = "text/html; charset=utf-8";
    if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") ct = "application/javascript";
    if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") ct = "text/css";
    send_http_keepalive(s, 200, ct, file_body);
}

void StandaloneWebBridgeServer::run_ws() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        m_running = false;
        return;
    }

    SOCKET ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ls == INVALID_SOCKET) {
        WSACleanup();
        m_running = false;
        return;
    }
    m_ws_listen_socket = static_cast<int>(ls);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_cfg.ws_port);

    int opt = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(ls);
        WSACleanup();
        m_running = false;
        return;
    }
    if (listen(ls, SOMAXCONN) != 0) {
        closesocket(ls);
        WSACleanup();
        m_running = false;
        return;
    }

    while (m_running.load()) {
        SOCKET cs = accept(ls, nullptr, nullptr);
        if (cs == INVALID_SOCKET) break;
        std::thread([this, cs] {
            handle_ws_client(static_cast<int>(cs));
            closesocket(cs);
        }).detach();
    }

    closesocket(ls);
    WSACleanup();
}

void StandaloneWebBridgeServer::handle_ws_client(int client_socket) {
    SOCKET s = static_cast<SOCKET>(client_socket);

    char buf[8192];
    int n = recv(s, buf, static_cast<int>(sizeof(buf) - 1), 0);
    if (n <= 0) return;
    buf[n] = '\0';

    HttpRequest req;
    if (!parse_http_request(std::string(buf, buf + n), req)) return;
    if (!req.is_websocket || req.ws_key.empty()) return;

    std::string accept;
    if (!sha1_base64_websocket_accept(req.ws_key, accept)) return;

    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n";
    oss << "Upgrade: websocket\r\n";
    oss << "Connection: Upgrade\r\n";
    oss << "Sec-WebSocket-Accept: " << accept << "\r\n";
    oss << "\r\n";
    auto resp = oss.str();
    if (!send_all(s, resp.data(), static_cast<int>(resp.size()))) return;

    for (;;) {
        uint8_t opcode = 0;
        std::vector<uint8_t> payload;
        if (!ws_recv_frame(s, opcode, payload)) return;

        if (opcode == 0x8) return; // close
        if (opcode == 0x9) { ws_send_control(s, 0xA); continue; } // ping->pong
        if (opcode != 0x1) continue; // text only

        m_ws_requests.fetch_add(1, std::memory_order_relaxed);

        std::string text(payload.begin(), payload.end());
        nlohmann::json reqJson;
        try {
            reqJson = nlohmann::json::parse(text);
        } catch (...) {
            nlohmann::json err = {{"jsonrpc","2.0"},{"error", {{"code",-32700},{"message","Parse error"}}}};
            ws_send_text(s, err.dump());
            continue;
        }

        const nlohmann::json id = reqJson.contains("id") ? reqJson["id"] : nlohmann::json(nullptr);
        const std::string method = reqJson.value("method", "");
        nlohmann::json result = nlohmann::json::object();

        if (method == "getServerStatus") {
            result["listening"] = true;
            result["port"] = m_cfg.http_port;
            result["ws_port"] = m_cfg.ws_port;
        } else if (method == "sendToModel") {
            m_model_requests.fetch_add(1, std::memory_order_relaxed);
            const auto params = reqJson.value("params", nlohmann::json::object());
            const std::string prompt = params.value("prompt", "");
            const auto options = params.value("options", nlohmann::json::object());
            const int32_t max_tokens = options.value("num_predict", 512);
            const std::string model_name = options.value("model", m_cfg.default_model);

            if (prompt.empty()) {
                result["success"] = false;
                result["error"] = "Missing prompt";
            } else {
                std::wstring gguf = resolve_model_path(m_cfg, model_name);
                if (gguf.empty()) {
                    result["success"] = false;
                    result["error"] = "Model not found under web_root\\\\models";
                } else {
                    std::string init_err;
                    if (!g_llama.ensure_initialized(init_err)) {
                        result["success"] = false;
                        result["error"] = init_err;
                    } else {
                        std::string load_err;
                        if (!g_llama.ensure_model_loaded(gguf, 999, load_err)) {
                            result["success"] = false;
                            result["error"] = load_err;
                        } else {
                            auto r = g_llama.generate(prompt, max_tokens);
                            if (!r.ok) {
                                result["success"] = false;
                                result["error"] = r.error;
                            } else {
                                result["success"] = true;
                                result["response"] = r.text;
                            }
                        }
                    }
                }
            }
        } else {
            nlohmann::json err = {{"jsonrpc","2.0"},{"id", id},{"error", {{"code",-32601},{"message","Method not found"}}}};
            ws_send_text(s, err.dump());
            continue;
        }

        nlohmann::json respJson = {{"jsonrpc","2.0"},{"id", id},{"result", result}};
        ws_send_text(s, respJson.dump());
    }
}

void StandaloneWebBridgeServer::resolve_default_web_root() {
    if (m_cfg.web_root != "." && !m_cfg.web_root.empty()) return;

    auto exists = [](const std::string& dir) {
        std::string p = dir;
        if (!p.empty() && p.back() != '\\' && p.back() != '/') p += "\\";
        p += "standalone_interface.html";
        return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
    };

    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = exePath;
    auto pos = exeDir.find_last_of("\\/");
    if (pos != std::string::npos) exeDir.resize(pos);

    std::vector<std::string> candidates;
    candidates.push_back(".");
    candidates.push_back(exeDir);
    candidates.push_back(exeDir + "\\..");
    candidates.push_back(exeDir + "\\..\\..");
    candidates.push_back(exeDir + "\\..\\..\\..");

    for (auto& c : candidates) {
        char full[MAX_PATH] = {};
        if (GetFullPathNameA(c.c_str(), MAX_PATH, full, nullptr) == 0) continue;
        if (exists(full)) {
            m_cfg.web_root = full;
            return;
        }
    }
}
