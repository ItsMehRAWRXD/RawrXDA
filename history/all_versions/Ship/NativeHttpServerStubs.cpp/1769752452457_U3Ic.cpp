#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>

extern "C" ULONG __stdcall HttpCloseRequestQueue(HANDLE hRequestQueue);

extern "C" ULONG __stdcall HttpCloseRequestHandle(HANDLE hRequestQueue) {
    return HttpCloseRequestQueue(hRequestQueue);
}

extern "C" uint32_t __stdcall InferenceEngine_Initialize() {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_LoadModel(const char* /*modelPath*/) {
    return 0;
}

extern "C" uint32_t __stdcall InferenceEngine_Generate(const char* /*prompt*/, char* /*outBuffer*/, uint32_t /*outBufferSize*/) {
    return 1;
}

static void EscapeJsonString(const char* src, char* dest, size_t dest_size) {
    if (!src || !dest || dest_size == 0) {
        return;
    }
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 2 < dest_size; ++i) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dest[out++] = '\\';
            dest[out++] = c;
        } else if (c == '\n') {
            dest[out++] = '\\';
            dest[out++] = 'n';
        } else if (c == '\r') {
            dest[out++] = '\\';
            dest[out++] = 'r';
        } else if (c == '\t') {
            dest[out++] = '\\';
            dest[out++] = 't';
        } else {
            dest[out++] = c;
        }
    }
    dest[out] = '\0';
}

extern "C" uint32_t __stdcall ToolExecuteJson(const char* json, char* outBuffer, uint32_t outBufferSize) {
    if (!outBuffer || outBufferSize == 0) {
        return 1;
    }

    std::string input = json ? json : "";

    auto extractValue = [&](const std::string& key) -> std::string {
        std::string needle = "\"" + key + "\"";
        size_t keyPos = input.find(needle);
        if (keyPos == std::string::npos) return "";
        size_t colon = input.find(':', keyPos + needle.size());
        if (colon == std::string::npos) return "";
        size_t quote = input.find('"', colon + 1);
        if (quote == std::string::npos) return "";
        size_t end = input.find('"', quote + 1);
        if (end == std::string::npos) return "";
        return input.substr(quote + 1, end - quote - 1);
    };

    auto unescapeJson = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char n = s[i + 1];
                if (n == 'n') out += '\n';
                else if (n == 'r') out += '\r';
                else if (n == 't') out += '\t';
                else out += n;
                ++i;
            } else {
                out += s[i];
            }
        }
        return out;
    };

    std::string tool = extractValue("tool");
    if (tool == "read_file") {
        std::string path = extractValue("path");
        if (path.empty()) {
            size_t paramsPos = input.find("\"params\"");
            if (paramsPos != std::string::npos) {
                path = extractValue("path");
            }
        }
        path = unescapeJson(path);

        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            std::string escaped;
            escaped.resize(content.size() * 2 + 1);
            EscapeJsonString(content.c_str(), escaped.data(), escaped.size());

            std::snprintf(outBuffer, outBufferSize,
                          "{\"content\":\"%s\",\"size\":%zu}",
                          escaped.c_str(), content.size());
            return 0;
        }
    }

    std::snprintf(outBuffer, outBufferSize,
                  "{\"error\":\"Tool execution failed or unknown tool\"}");
    return 1;
}
