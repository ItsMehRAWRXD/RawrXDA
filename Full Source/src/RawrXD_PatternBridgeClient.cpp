// RawrXD Pattern Bridge Win32 Client
// Minimal named-pipe client to talk to RawrXD-IDE-Bridge.ps1
// Depends only on Win32 APIs (no external JSON libs)

#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace RawrXD {

struct ClassificationResult {
    bool ok{false};
    std::string type;
    std::string priority;
    double confidence{0.0};
    std::string content;
    std::string file;
    int line{0};
    std::string raw;
    std::string error;
};

class PatternBridgeClient {
public:
    explicit PatternBridgeClient(const std::string& pipeName = "RawrXD_PatternBridge")
        : pipeName_(pipeName), pipe_(INVALID_HANDLE_VALUE) {}

    ~PatternBridgeClient() { Disconnect(); }

    bool // Connect removed {
        std::string fullName = "\\\\.\\pipe\\" + pipeName_;

        pipe_ = CreateFileA(
            fullName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (pipe_ == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_PIPE_BUSY) {
                if (!WaitNamedPipeA(fullName.c_str(), timeoutMs)) {
                    return false;
                }
                pipe_ = CreateFileA(fullName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            }
        }

        return pipe_ != INVALID_HANDLE_VALUE;
    }

    void // disconnect removed
            pipe_ = INVALID_HANDLE_VALUE;
        }
    }

    ClassificationResult ClassifyFile(const std::string& filePath) {
        ClassificationResult result;
        if (pipe_ == INVALID_HANDLE_VALUE) {
            result.error = "Pipe not connected";
            return result;
        }

        std::string command = "CLASSIFY|" + filePath + "\n";
        if (!WriteString(command)) {
            result.error = "Failed to write to pipe";
            return result;
        }

        std::string response;
        if (!ReadLine(response)) {
            result.error = "Failed to read from pipe";
            return result;
        }

        result.raw = response;
        if (response.find("\"error\"") != std::string::npos) {
            result.error = response;
            return result;
        }

        // Very lightweight JSON sniffing: expect an array with at least one element
        // Example: [{"File":"...","Line":12,"Type":"TODO","Priority":"High","Confidence":0.78,"Content":"..."}]
        auto getValue = [&](const std::string& key) -> std::string {
            std::string token = "\"" + key + "\":";
            auto pos = response.find(token);
            if (pos == std::string::npos) return "";
            pos += token.size();
            // Skip spaces
            while (pos < response.size() && (response[pos] == ' ')) pos++;
            if (pos < response.size() && response[pos] == '"') {
                pos++;
                auto end = response.find('"', pos);
                if (end != std::string::npos) {
                    return response.substr(pos, end - pos);
                }
            } else {
                auto end = response.find_first_of(",}]", pos);
                if (end != std::string::npos) {
                    return response.substr(pos, end - pos);
                }
            }
            return "";
        };

        result.type = getValue("Type");
        result.priority = getValue("Priority");
        result.content = getValue("Content");
        result.file = getValue("File");

        std::string lineStr = getValue("Line");
        if (!lineStr.empty()) {
            result.line = std::stoi(lineStr);
        }

        std::string confStr = getValue("Confidence");
        if (!confStr.empty()) {
            result.confidence = std::atof(confStr.c_str());
        }

        result.ok = !result.type.empty();
        return result;
    }

private:
    bool WriteString(const std::string& text) {
        DWORD written = 0;
        BOOL ok = WriteFile(pipe_, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return ok && written == text.size();
    }

    bool ReadLine(std::string& out) {
        out.clear();
        char buffer[1024];
        DWORD read = 0;
        BOOL ok = ReadFile(pipe_, buffer, sizeof(buffer) - 1, &read, nullptr);
        if (!ok || read == 0) return false;
        buffer[read] = '\0';
        out.assign(buffer, read);
        return true;
    }

    std::string pipeName_;
    HANDLE pipe_;
};

} // namespace RawrXD

#ifdef RAWRXD_CLIENT_DEMO
int main() {
    RawrXD::PatternBridgeClient client;
    if (!client.// Connect removed) {
        
        return 1;
    }

    auto res = client.ClassifyFile("D:/lazy init ide/src/example.cpp");
    if (!res.ok) {
        
        return 1;
    }


    return 0;
}
#endif

