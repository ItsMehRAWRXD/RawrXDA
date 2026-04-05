#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {

class VulkanTokenRenderer {
public:
    using SubmitFn = std::function<void(const char*, size_t, bool)>;

    void SetSubmitBackend(SubmitFn fn) { submit_ = std::move(fn); }

    void Submit(const std::string& token, bool done) const {
        if (submit_) {
            submit_(token.data(), token.size(), done);
        }
    }

    bool IsEnabled() const { return static_cast<bool>(submit_); }

private:
    SubmitFn submit_;
};

class DualEngineScheduler {
public:
    struct Result {
        bool ok = false;
        std::string text;
    };

    using EngineFn = std::function<Result()>;

    Result ExecuteSpeculative(const EngineFn& primary, const EngineFn& secondary, uint32_t timeoutMs) {
        auto p = std::async(std::launch::async, primary);
        auto s = std::async(std::launch::async, secondary);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

        while (std::chrono::steady_clock::now() < deadline) {
            if (p.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                Result r = p.get();
                if (r.ok && !r.text.empty()) return r;
            }
            if (s.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                Result r = s.get();
                if (r.ok && !r.text.empty()) return r;
            }
            Sleep(1);
        }

        if (p.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            Result r = p.get();
            if (r.ok && !r.text.empty()) return r;
        }
        if (s.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            Result r = s.get();
            if (r.ok && !r.text.empty()) return r;
        }

        return {};
    }
};

class NdjsonSimdParser {
public:
    using LineCallback = std::function<void(const std::string& token, bool done)>;

    void Push(const char* data, size_t len, const LineCallback& cb) {
        if (!data || len == 0) return;
        buffer_.append(data, len);

        size_t start = 0;
        while (true) {
            const size_t nl = FindNewline(buffer_.data() + start, buffer_.size() - start);
            if (nl == npos) break;

            const size_t lineEnd = start + nl;
            HandleLine(buffer_.data() + start, lineEnd - start, cb);
            start = lineEnd + 1;
        }

        if (start > 0) {
            buffer_.erase(0, start);
        }
    }

private:
    static constexpr size_t npos = static_cast<size_t>(-1);
    std::string buffer_;

    static size_t FindNewline(const char* data, size_t len) {
#if defined(__AVX2__)
        const __m256i needle = _mm256_set1_epi8('\n');
        size_t i = 0;
        for (; i + 32 <= len; i += 32) {
            const __m256i v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            const __m256i cmp = _mm256_cmpeq_epi8(v, needle);
            const uint32_t mask = static_cast<uint32_t>(_mm256_movemask_epi8(cmp));
            if (mask != 0) {
#if defined(_MSC_VER)
                unsigned long idx = 0;
                _BitScanForward(&idx, mask);
                return i + static_cast<size_t>(idx);
#else
                return i + static_cast<size_t>(__builtin_ctz(mask));
#endif
            }
        }
        for (; i < len; ++i) {
            if (data[i] == '\n') return i;
        }
        return npos;
#else
        for (size_t i = 0; i < len; ++i) {
            if (data[i] == '\n') return i;
        }
        return npos;
#endif
    }

    static std::string ExtractJsonField(const char* s, size_t n, const char* key) {
        std::string patt = std::string("\"") + key + "\":\"";
        const std::string line(s, n);
        const size_t p = line.find(patt);
        if (p == std::string::npos) return {};

        size_t i = p + patt.size();
        std::string out;
        out.reserve(128);

        while (i < line.size()) {
            const char c = line[i++];
            if (c == '\\' && i < line.size()) {
                const char e = line[i++];
                if (e == 'n') out.push_back('\n');
                else if (e == 'r') out.push_back('\r');
                else if (e == 't') out.push_back('\t');
                else out.push_back(e);
                continue;
            }
            if (c == '"') break;
            out.push_back(c);
        }
        return out;
    }

    static void HandleLine(const char* line, size_t len, const LineCallback& cb) {
        if (len == 0 || !cb) return;
        const std::string token = ExtractJsonField(line, len, "response");
        const std::string doneProbe(line, len);
        const bool done = doneProbe.find("\"done\":true") != std::string::npos;

        if (!token.empty() || done) {
            cb(token, done);
        }
    }
};

class OllamaBridge {
public:
    using TokenCallback = std::function<void(const std::string& token, bool done)>;

    OllamaBridge() = default;
    ~OllamaBridge() { Shutdown(); }

    bool Initialize() {
        if (initialized_) return true;
        WSADATA wsa = {};
        const int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
        initialized_ = (rc == 0);
        return initialized_;
    }

    void Shutdown() {
        if (initialized_) {
            WSACleanup();
            initialized_ = false;
        }
    }

    void SetContextWindow(int ctxLen) { contextWindow_ = (ctxLen > 0) ? ctxLen : 4096; }
    void SetModelOverride(const std::string& model) { modelOverride_ = model; }
    void SetPort(uint16_t p) { port_ = p; }
    void SetVulkanRenderer(std::shared_ptr<VulkanTokenRenderer> r) { renderer_ = std::move(r); }

    bool StreamCompletion(const std::string& model, const std::string& prompt, TokenCallback onToken) {
        if (!initialized_) return false;

        const std::string activeModel = modelOverride_.empty() ? model : modelOverride_;
        const std::string body = BuildGeneratePayload(activeModel, prompt, contextWindow_);
        const std::string req = BuildHttpRequest("POST", "/api/generate", body);

        SOCKET sock = ConnectLoopback(port_);
        if (sock == INVALID_SOCKET) return false;

        const bool sendOk = SendAll(sock, req.data(), req.size());
        if (!sendOk) {
            closesocket(sock);
            return false;
        }

        bool inBody = false;
        std::string headerBuf;
        char recvBuf[64 * 1024];
        NdjsonSimdParser parser;
        bool gotAnyToken = false;

        for (;;) {
            const int n = recv(sock, recvBuf, static_cast<int>(sizeof(recvBuf)), 0);
            if (n == 0) break;
            if (n < 0) {
                closesocket(sock);
                return false;
            }

            if (!inBody) {
                headerBuf.append(recvBuf, static_cast<size_t>(n));
                const size_t p = headerBuf.find("\r\n\r\n");
                if (p == std::string::npos) {
                    continue;
                }
                const size_t bodyStart = p + 4;
                inBody = true;
                const char* bodyData = headerBuf.data() + bodyStart;
                const size_t bodyLen = headerBuf.size() - bodyStart;
                parser.Push(bodyData, bodyLen, [&](const std::string& tok, bool done) {
                    if (!tok.empty()) gotAnyToken = true;
                    if (renderer_ && renderer_->IsEnabled()) renderer_->Submit(tok, done);
                    if (onToken) onToken(tok, done);
                });
                headerBuf.clear();
            } else {
                parser.Push(recvBuf, static_cast<size_t>(n), [&](const std::string& tok, bool done) {
                    if (!tok.empty()) gotAnyToken = true;
                    if (renderer_ && renderer_->IsEnabled()) renderer_->Submit(tok, done);
                    if (onToken) onToken(tok, done);
                });
            }
        }

        closesocket(sock);
        return gotAnyToken;
    }

    std::vector<std::string> ListModels() {
        std::vector<std::string> out;
        if (!initialized_) return out;

        const std::string req = BuildHttpRequest("GET", "/api/tags", "");
        SOCKET sock = ConnectLoopback(port_);
        if (sock == INVALID_SOCKET) return out;
        if (!SendAll(sock, req.data(), req.size())) {
            closesocket(sock);
            return out;
        }

        std::string raw;
        char buf[8192];
        for (;;) {
            const int n = recv(sock, buf, static_cast<int>(sizeof(buf)), 0);
            if (n <= 0) break;
            raw.append(buf, static_cast<size_t>(n));
        }
        closesocket(sock);

        const size_t h = raw.find("\r\n\r\n");
        if (h == std::string::npos) return out;
        const std::string body = raw.substr(h + 4);

        size_t pos = 0;
        while ((pos = body.find("\"name\":\"", pos)) != std::string::npos) {
            pos += 8;
            const size_t end = body.find('"', pos);
            if (end == std::string::npos) break;
            out.push_back(body.substr(pos, end - pos));
            pos = end + 1;
        }
        return out;
    }

private:
    bool initialized_ = false;
    uint16_t port_ = 11434;
    int contextWindow_ = 4096;
    std::string modelOverride_;
    std::shared_ptr<VulkanTokenRenderer> renderer_;

    static std::string EscapeJson(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 16);
        for (const char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out.push_back(c);
        }
        return out;
    }

    static std::string BuildGeneratePayload(const std::string& model, const std::string& prompt, int ctx) {
        return std::string("{\"model\":\"") + model +
               "\",\"prompt\":\"" + EscapeJson(prompt) +
               "\",\"stream\":true,\"options\":{\"temperature\":0.7,\"num_ctx\":" +
               std::to_string(ctx) + "}}";
    }

    static std::string BuildHttpRequest(const char* method, const char* path, const std::string& body) {
        std::string req;
        req.reserve(256 + body.size());
        req += method;
        req += " ";
        req += path;
        req += " HTTP/1.1\r\nHost: 127.0.0.1:11434\r\nConnection: close\r\n";
        if (!body.empty()) {
            req += "Content-Type: application/json\r\nContent-Length: ";
            req += std::to_string(body.size());
            req += "\r\n\r\n";
            req += body;
        } else {
            req += "\r\n";
        }
        return req;
    }

    static SOCKET ConnectLoopback(uint16_t port) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return INVALID_SOCKET;

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        const int timeoutMs = 15000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        if (connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
            closesocket(sock);
            return INVALID_SOCKET;
        }
        return sock;
    }

    static bool SendAll(SOCKET sock, const char* data, size_t len) {
        size_t sent = 0;
        while (sent < len) {
            const int n = send(sock, data + sent, static_cast<int>(len - sent), 0);
            if (n <= 0) return false;
            sent += static_cast<size_t>(n);
        }
        return true;
    }
};

}  // namespace RawrXD
