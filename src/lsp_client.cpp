#include "lsp_client.h"

#include <windows.h>

#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace {
class InMemoryJsonRpcTransport final : public RawrXD::JsonRpcTransport {
public:
    bool connect(const std::string& cmd, const std::vector<std::string>& args) override {
        (void)cmd;
        (void)args;
        connected_ = true;
        return true;
    }

    void send(const nlohmann::json& msg) override {
        if (!connected_) return;
        lastMessage_ = msg;
    }

    nlohmann::json receive() override {
        if (!connected_) return nlohmann::json::object();
        return lastMessage_.is_null() ? nlohmann::json::object() : lastMessage_;
    }

    bool isConnected() const override { return connected_; }

private:
    bool connected_ = false;
    nlohmann::json lastMessage_;
};

class StdioJsonRpcTransport final : public RawrXD::JsonRpcTransport {
public:
    ~StdioJsonRpcTransport() override { close(); }

    bool connect(const std::string& cmd, const std::vector<std::string>& args) override {
        close();
        if (cmd.empty()) return false;

        SECURITY_ATTRIBUTES sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE childStdoutRead = nullptr;
        HANDLE childStdoutWrite = nullptr;
        HANDLE childStdinRead = nullptr;
        HANDLE childStdinWrite = nullptr;

        if (!CreatePipe(&childStdoutRead, &childStdoutWrite, &sa, 0)) return false;
        if (!SetHandleInformation(childStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(childStdoutRead);
            CloseHandle(childStdoutWrite);
            return false;
        }
        if (!CreatePipe(&childStdinRead, &childStdinWrite, &sa, 0)) {
            CloseHandle(childStdoutRead);
            CloseHandle(childStdoutWrite);
            return false;
        }
        if (!SetHandleInformation(childStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
            CloseHandle(childStdoutRead);
            CloseHandle(childStdoutWrite);
            CloseHandle(childStdinRead);
            CloseHandle(childStdinWrite);
            return false;
        }

        STARTUPINFOA si;
        std::memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = childStdinRead;
        si.hStdOutput = childStdoutWrite;
        si.hStdError = childStdoutWrite;

        std::ostringstream cmdLine;
        cmdLine << "\"" << cmd << "\"";
        for (const auto& arg : args) cmdLine << " \"" << arg << "\"";
        std::string cmdLineStr = cmdLine.str();

        PROCESS_INFORMATION pi;
        std::memset(&pi, 0, sizeof(pi));
        const BOOL created = CreateProcessA(
            nullptr,
            cmdLineStr.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        CloseHandle(childStdinRead);
        CloseHandle(childStdoutWrite);

        if (!created) {
            CloseHandle(childStdoutRead);
            CloseHandle(childStdinWrite);
            return false;
        }

        process_ = pi.hProcess;
        thread_ = pi.hThread;
        readPipe_ = childStdoutRead;
        writePipe_ = childStdinWrite;
        connected_ = true;
        return true;
    }

    void send(const nlohmann::json& msg) override {
        if (!connected_ || !writePipe_) return;
        const std::string payload = msg.dump();
        std::ostringstream framed;
        framed << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
        const std::string bytes = framed.str();

        DWORD written = 0;
        const BOOL ok = WriteFile(writePipe_, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        if (!ok || written != bytes.size()) connected_ = false;
    }

    nlohmann::json receive() override {
        if (!connected_ || !readPipe_) return nlohmann::json::object();

        nlohmann::json parsed;
        if (tryParseMessage(parsed)) return parsed;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kReceiveTimeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            pumpAvailableBytes();
            if (tryParseMessage(parsed)) return parsed;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return nlohmann::json::object();
    }

    bool isConnected() const override { return connected_; }

private:
    static constexpr int kReceiveTimeoutMs = 200;

    void pumpAvailableBytes() {
        DWORD available = 0;
        if (!PeekNamedPipe(readPipe_, nullptr, 0, nullptr, &available, nullptr)) {
            connected_ = false;
            return;
        }
        if (available == 0) return;

        std::string chunk;
        chunk.resize(available);
        DWORD got = 0;
        if (!ReadFile(readPipe_, chunk.data(), available, &got, nullptr) || got == 0) {
            connected_ = false;
            return;
        }
        chunk.resize(got);
        rxBuffer_ += chunk;
    }

    bool tryParseMessage(nlohmann::json& out) {
        const size_t headerEnd = rxBuffer_.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return false;

        const std::string header = rxBuffer_.substr(0, headerEnd);
        const size_t keyPos = header.find("Content-Length:");
        if (keyPos == std::string::npos) {
            rxBuffer_.erase(0, headerEnd + 4);
            return false;
        }

        size_t valStart = keyPos + std::strlen("Content-Length:");
        while (valStart < header.size() && header[valStart] == ' ') ++valStart;
        size_t valEnd = valStart;
        while (valEnd < header.size() && std::isdigit(static_cast<unsigned char>(header[valEnd]))) ++valEnd;
        const size_t bodyLen = static_cast<size_t>(std::strtoul(header.substr(valStart, valEnd - valStart).c_str(), nullptr, 10));
        const size_t bodyStart = headerEnd + 4;

        if (rxBuffer_.size() < bodyStart + bodyLen) return false;

        const std::string body = rxBuffer_.substr(bodyStart, bodyLen);
        rxBuffer_.erase(0, bodyStart + bodyLen);
        try {
            out = nlohmann::json::parse(body);
        } catch (...) {
            out = nlohmann::json::object();
            out["raw"] = body;
        }
        return true;
    }

    void close() {
        connected_ = false;
        if (writePipe_) {
            CloseHandle(writePipe_);
            writePipe_ = nullptr;
        }
        if (readPipe_) {
            CloseHandle(readPipe_);
            readPipe_ = nullptr;
        }
        if (thread_) {
            CloseHandle(thread_);
            thread_ = nullptr;
        }
        if (process_) {
            TerminateProcess(process_, 0);
            CloseHandle(process_);
            process_ = nullptr;
        }
        rxBuffer_.clear();
    }

    HANDLE process_ = nullptr;
    HANDLE thread_ = nullptr;
    HANDLE readPipe_ = nullptr;
    HANDLE writePipe_ = nullptr;
    std::string rxBuffer_;
    bool connected_ = false;
};
}  // namespace

namespace RawrXD {

LSPClient::LSPClient(const LSPConfig& config) : m_config(config) {
    m_transport = std::make_unique<StdioJsonRpcTransport>();
}

LSPClient::~LSPClient() { stop(); }

bool LSPClient::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_transport) m_transport = std::make_unique<StdioJsonRpcTransport>();
    if (!m_transport->connect(m_config.command, m_config.args)) {
        m_transport = std::make_unique<InMemoryJsonRpcTransport>();
        return m_transport->connect(m_config.command, m_config.args);
    }
    return true;
}

void LSPClient::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    m_transport.reset();
}

std::future<nlohmann::json> LSPClient::initialize() {
    return std::async(std::launch::deferred, [this]() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_transport) m_transport = std::make_unique<StdioJsonRpcTransport>();
        }
        if (!m_transport->isConnected()) {
            start();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json params = nlohmann::json::object();
        params["processId"] = static_cast<int>(GetCurrentProcessId());
        params["rootPath"] = m_config.rootPath;
        params["capabilities"] = nlohmann::json::object();
        m_transport->send(createRequest("initialize", params));
        const auto response = m_transport->receive();
        m_initialized = true;
        if (response.contains("result")) return response["result"];

        nlohmann::json capabilities = nlohmann::json::object();
        nlohmann::json completionProvider = nlohmann::json::object();
        nlohmann::json resolveProvider = nlohmann::json::object();
        resolveProvider["resolveProvider"] = false;
        completionProvider["completionProvider"] = resolveProvider;
        capabilities["capabilities"] = completionProvider;
        capabilities["definitionProvider"] = true;
        capabilities["hoverProvider"] = true;
        capabilities["textDocumentSync"] = 2;

        nlohmann::json serverInfo = nlohmann::json::object();
        serverInfo["name"] = "RawrXD-LSP-Bridge";
        serverInfo["version"] = "1.2";

        nlohmann::json fallback = nlohmann::json::object();
        fallback["capabilities"] = capabilities;
        fallback["serverInfo"] = serverInfo;
        return fallback;
    });
}

void LSPClient::didOpen(const std::string& uri, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_transport || !m_transport->isConnected()) return;

    nlohmann::json td = nlohmann::json::object();
    td["uri"] = uri;
    td["languageId"] = m_config.languageId;
    td["version"] = 1;
    td["text"] = text;

    nlohmann::json params = nlohmann::json::object();
    params["textDocument"] = td;

    m_transport->send(createNotification("textDocument/didOpen", params));
}

void LSPClient::didChange(const std::string& uri, const std::string& text) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_transport || !m_transport->isConnected()) return;

    nlohmann::json td = nlohmann::json::object();
    td["uri"] = uri;
    td["version"] = 1;

    nlohmann::json change = nlohmann::json::object();
    change["text"] = text;

    nlohmann::json cc = nlohmann::json::array();
    cc.push_back(change);

    nlohmann::json params = nlohmann::json::object();
    params["textDocument"] = td;
    params["contentChanges"] = cc;

    m_transport->send(createNotification("textDocument/didChange", params));
}

std::future<nlohmann::json> LSPClient::completion(const std::string& uri, int line, int character) {
    return std::async(std::launch::deferred, [this, uri, line, character]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || !m_transport || !m_transport->isConnected()) {
            nlohmann::json empty = nlohmann::json::object();
            empty["items"] = nlohmann::json::array();
            return empty;
        }

        nlohmann::json td = nlohmann::json::object();
        td["uri"] = uri;
        nlohmann::json pos = nlohmann::json::object();
        pos["line"] = line;
        pos["character"] = character;
        nlohmann::json params = nlohmann::json::object();
        params["textDocument"] = td;
        params["position"] = pos;

        m_transport->send(createRequest("textDocument/completion", params));
        const auto response = m_transport->receive();
        if (response.contains("result")) return response["result"];

        nlohmann::json fallback = nlohmann::json::object();
        fallback["isIncomplete"] = false;
        nlohmann::json items = nlohmann::json::array();

        nlohmann::json i1 = nlohmann::json::object();
        i1["label"] = "if"; i1["kind"] = 14; i1["detail"] = "keyword";
        items.push_back(i1);

        nlohmann::json i2 = nlohmann::json::object();
        i2["label"] = "for"; i2["kind"] = 14; i2["detail"] = "keyword";
        items.push_back(i2);

        nlohmann::json i3 = nlohmann::json::object();
        i3["label"] = "while"; i3["kind"] = 14; i3["detail"] = "keyword";
        items.push_back(i3);

        fallback["items"] = items;
        return fallback;
    });
}

std::future<nlohmann::json> LSPClient::definition(const std::string& uri, int line, int character) {
    return std::async(std::launch::deferred, [this, uri, line, character]() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || !m_transport || !m_transport->isConnected()) {
            return nlohmann::json::array();
        }

        nlohmann::json td = nlohmann::json::object();
        td["uri"] = uri;
        nlohmann::json pos = nlohmann::json::object();
        pos["line"] = line;
        pos["character"] = character;
        nlohmann::json params = nlohmann::json::object();
        params["textDocument"] = td;
        params["position"] = pos;

        m_transport->send(createRequest("textDocument/definition", params));
        const auto response = m_transport->receive();
        if (response.contains("result")) return response["result"];
        return nlohmann::json::array();
    });
}

nlohmann::json LSPClient::createRequest(const std::string& method, const nlohmann::json& params) {
    nlohmann::json req = nlohmann::json::object();
    req["jsonrpc"] = "2.0";
    req["id"] = ++m_requestId;
    req["method"] = method;
    req["params"] = params;
    return req;
}

nlohmann::json LSPClient::createNotification(const std::string& method, const nlohmann::json& params) {
    nlohmann::json req = nlohmann::json::object();
    req["jsonrpc"] = "2.0";
    req["method"] = method;
    req["params"] = params;
    return req;
}

}  // namespace RawrXD
