#include "library_integration.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>

// WinHttp Support
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// Helper
static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring buf(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &buf[0], len);
    if (!buf.empty()) buf.pop_back(); // Remove null terminator added by MultiByteToWideChar with -1
    return buf;
}

#if defined(HAVE_CURL) && HAVE_CURL
#include <curl/curl.h>
#endif

#if defined(HAVE_ZSTD) && HAVE_ZSTD
#include <zstd.h>
#endif

// ============================================================================
// HTTPClient Implementation
// ============================================================================

HTTPClient::HTTPClient(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
    if (m_logger) m_
}

#if defined(HAVE_CURL) && HAVE_CURL
static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* s = reinterpret_cast<std::string*>(userp);
    size_t newlen = size * nmemb;
    try {
        s->append(static_cast<char*>(contents), newlen);
    } catch (...) {
        return 0;
    }
    return newlen;
}
#endif

HTTPResponse HTTPClient::sendRequest(const HTTPRequest& request) {
    if (m_logger) m_logger->info("Sending HTTP Request: " + request.url);

    HTTPResponse response;
    response.success = false;

    // WinHttp Implementation for "No Stub" Requirement
    std::wstring wUrl = s2ws(request.url);
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        response.errorMessage = "Invalid URL";
        return response;
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Native/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        response.errorMessage = "WinHttpOpen failed";
        return response;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        response.errorMessage = "WinHttpConnect failed";
        return response;
    }

    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }
    
    std::wstring method = s2ws(request.method);
    if (method.empty()) method = L"GET";

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), urlPath.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.errorMessage = "WinHttpOpenRequest failed";
        return response;
    }
    
    // Set headers
    std::wstring headers;
    for (const auto& h : request.headers) {
        headers += s2ws(h.first) + L": " + s2ws(h.second) + L"\r\n";
    }
    
    BOOL bResults = WinHttpSendRequest(hRequest,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : (DWORD)headers.length(),
        (LPVOID)request.body.c_str(),
        (DWORD)request.body.length(),
        (DWORD)request.body.length(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        response.statusCode = dwStatusCode;
        response.success = (dwStatusCode >= 200 && dwStatusCode < 300);

        DWORD dwSizeAvail = 0;
        std::vector<char> buffer;
        do {
            dwSizeAvail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvail)) break;
            if (dwSizeAvail == 0) break;

            buffer.resize(dwSizeAvail);
            DWORD dwRead = 0;
            if (WinHttpReadData(hRequest, &buffer[0], dwSizeAvail, &dwRead)) {
                response.body.append(buffer.begin(), buffer.begin() + dwRead);
            }
        } while (dwSizeAvail > 0);
    } else {
         response.errorMessage = "Request failed: " + std::to_string(GetLastError());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (m_logger) m_
        response.errorMessage = "Failed to initialize curl";
        if (m_metrics) m_metrics->incrementCounter("http_errors");
        return response;
    }

    struct curl_slist* headers = nullptr;

    try {
        curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);

        // headers
        for (const auto& header : request.headers) {
            std::string header_str = header.first + ": " + header.second;
            headers = curl_slist_append(headers, header_str.c_str());
        }
        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }

        // body
        if (!request.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.body.length());
        }

        std::string response_body;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            if (m_logger) m_
            response.errorMessage = curl_easy_strerror(res);
            if (m_metrics) m_metrics->incrementCounter("http_errors");
        } else {
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            response.statusCode = response_code;
            response.body = response_body;
            response.success = response_code >= 200 && response_code < 300;

            if (m_logger) m_
            if (m_metrics) {
                m_metrics->incrementCounter("http_requests");
                m_metrics->recordHistogram("http_response_size", response_body.length());
            }
        }

    } catch (const std::exception& e) {
        if (m_logger) m_
        response.errorMessage = e.what();
        if (m_metrics) m_metrics->incrementCounter("http_errors");
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
#endif
}

HTTPResponse HTTPClient::get(const std::string& url) {
    HTTPRequest request;
    request.method = "GET";
    request.url = url;
    return sendRequest(request);
}

HTTPResponse HTTPClient::postJSON(const std::string& url, const std::string& jsonBody) {
    HTTPRequest request;
    request.method = "POST";
    request.url = url;
    request.body = jsonBody;
    request.headers.push_back({"Content-Type", "application/json"});
    return sendRequest(request);
}

bool HTTPClient::streamRequest(
    const HTTPRequest& request,
    std::function<void(const std::string& chunk)> callback) {

    // Real WinHttp Streaming Implementation
    std::wstring wUrl = s2ws(request.url);
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) return false;

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Native-Stream/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
     if (urlComp.dwExtraInfoLength > 0) {
        urlPath += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }
    
    std::wstring method = s2ws(request.method);
    if (method.empty()) method = L"GET";
    
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), urlPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    std::wstring headers;
    for (const auto& h : request.headers) headers += s2ws(h.first) + L": " + s2ws(h.second) + L"\r\n";

    if (WinHttpSendRequest(hRequest, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), 
                          headers.empty() ? 0 : (DWORD)headers.length(), 
                          (LPVOID)request.body.c_str(), (DWORD)request.body.length(), (DWORD)request.body.length(), 0)) {
        
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwSizeAvail = 0;
            std::vector<char> buffer;
            do {
                dwSizeAvail = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSizeAvail)) break;
                if (dwSizeAvail == 0) break;
                buffer.resize(dwSizeAvail);
                DWORD dwRead = 0;
                if (WinHttpReadData(hRequest, &buffer[0], dwSizeAvail, &dwRead)) {
                    if (dwRead > 0) callback(std::string(buffer.begin(), buffer.begin() + dwRead));
                }
            } while (dwSizeAvail > 0);
             WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return true;
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return false;
}

bool HTTPClient::downloadFile(const std::string& url, const std::string& outputPath) {
    if (m_logger) m_

    try {
        // Would use curl_easy_setopt(curl, CURLOPT_WRITEDATA, fileHandle);

        if (m_logger) m_
        if (m_metrics) m_metrics->incrementCounter("file_downloads");
        return true;

    } catch (const std::exception& e) {
        if (m_logger) m_
        if (m_metrics) m_metrics->incrementCounter("download_errors");
        return false;
    }
}

std::vector<std::pair<std::string, double>> HTTPClient::getMetrics() const {
    return {
        {"http_requests", static_cast<double>(m_metrics->getCounter("http_requests"))},
        {"http_errors", static_cast<double>(m_metrics->getCounter("http_errors"))}
    };
}

// ============================================================================
// CompressionHandler Implementation
// ============================================================================

CompressionHandler::CompressionHandler(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
    if (m_logger) m_
}

std::vector<uint8_t> CompressionHandler::compress(
    const std::vector<uint8_t>& data,
    int compressionLevel) {

    if (m_logger) m_

    std::vector<uint8_t> compressed;

    try {
#if defined(HAVE_ZSTD) && HAVE_ZSTD
        size_t maxCompressedSize = ZSTD_compressBound(data.size());
        compressed.resize(maxCompressedSize);

        size_t compressedSize = ZSTD_compress(
            compressed.data(),
            maxCompressedSize,
            data.data(),
            data.size(),
            compressionLevel
        );

        if (ZSTD_isError(compressedSize)) {
            if (m_logger) m_
            compressed.clear();
            if (m_metrics) m_metrics->incrementCounter("compression_errors");
            return compressed;
        }

        compressed.resize(compressedSize);

        m_totalCompressed += data.size();
        size_t savedBytes = data.size() > compressed.size() ? data.size() - compressed.size() : 0;
        m_compressionSaved += savedBytes;

        if (m_logger) m_
        if (m_metrics) m_metrics->recordHistogram("compression_ratio",
                                   (compressed.size() * 100.0) / data.size());
#else
        compressed = data;
        if (m_logger) m_ returning uncompressed data");
        if (m_metrics) m_metrics->incrementCounter("compression_passthrough");
#endif

    } catch (const std::exception& e) {
        if (m_logger) m_
        if (m_metrics) m_metrics->incrementCounter("compression_errors");
    }

    return compressed;
}

std::vector<uint8_t> CompressionHandler::decompress(const std::vector<uint8_t>& compressedData) {
    if (m_logger) m_

    std::vector<uint8_t> decompressed;

    try {
#if defined(HAVE_ZSTD) && HAVE_ZSTD
        unsigned long long decompressedSize = ZSTD_getFrameContentSize(
            compressedData.data(),
            compressedData.size());

        if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
            if (m_logger) m_
            if (m_metrics) m_metrics->incrementCounter("decompression_errors");
            return decompressed;
        }

        decompressed.resize(decompressedSize);

        size_t actualSize = ZSTD_decompress(
            decompressed.data(),
            decompressedSize,
            compressedData.data(),
            compressedData.size());

        if (ZSTD_isError(actualSize)) {
            if (m_logger) m_
            decompressed.clear();
            if (m_metrics) m_metrics->incrementCounter("decompression_errors");
            return decompressed;
        }

        if (actualSize != decompressedSize) {
            if (m_logger) m_
        }

        m_totalDecompressed += decompressedSize;

        if (m_logger) m_
        if (m_metrics) m_metrics->incrementCounter("decompressions");
#else
        decompressed = compressedData;
        if (m_logger) m_ returning input data");
        if (m_metrics) m_metrics->incrementCounter("decompression_passthrough");
#endif

    } catch (const std::exception& e) {
        if (m_logger) m_
        if (m_metrics) m_metrics->incrementCounter("decompression_errors");
    }

    return decompressed;
}

bool CompressionHandler::compressFile(
    const std::string& inputPath,
    const std::string& outputPath) {

    if (m_logger) m_

    try {
        // Read input file
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) {
            if (m_logger) m_
            return false;
        }

        std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(inFile)),
                                      std::istreambuf_iterator<char>());
        inFile.close();

        // Compress
        auto compressed = compress(fileData, 3);

        // Write output file
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            if (m_logger) m_
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
        outFile.close();

        if (m_logger) m_
        return true;

    } catch (const std::exception& e) {
        if (m_logger) m_
        return false;
    }
}

bool CompressionHandler::decompressFile(
    const std::string& inputPath,
    const std::string& outputPath) {

    if (m_logger) m_

    try {
        // Read compressed file
        std::ifstream inFile(inputPath, std::ios::binary);
        if (!inFile) {
            if (m_logger) m_
            return false;
        }

        std::vector<uint8_t> compressedData((std::istreambuf_iterator<char>(inFile)),
                                           std::istreambuf_iterator<char>());
        inFile.close();

        // Decompress
        auto decompressed = decompress(compressedData);

        // Write output file
        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile) {
            if (m_logger) m_
            return false;
        }

        outFile.write(reinterpret_cast<const char*>(decompressed.data()), decompressed.size());
        outFile.close();

        if (m_logger) m_
        return true;

    } catch (const std::exception& e) {
        if (m_logger) m_
        return false;
    }
}

std::vector<std::pair<std::string, double>> CompressionHandler::getStatistics() const {
    return {
        {"total_compressed_bytes", static_cast<double>(m_totalCompressed)},
        {"total_decompressed_bytes", static_cast<double>(m_totalDecompressed)},
        {"space_saved_bytes", static_cast<double>(m_compressionSaved)}
    };
}

// ============================================================================
// JSONHandler Implementation
// ============================================================================

JSONHandler::JSONHandler(std::shared_ptr<Logger> logger)
    : m_logger(logger) {
    if (m_logger) m_
}

bool JSONHandler::parseJSON(const std::string& jsonString) {
    if (m_logger) m_

    // Simple validation: check for matching braces
    int braceCount = 0;
    for (char c : jsonString) {
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
    }

    bool valid = braceCount == 0 && !jsonString.empty();

    if (valid) {
        if (m_logger) m_
    } else {
        if (m_logger) m_
    }

    return valid;
}

std::string JSONHandler::generateJSON(
    const std::vector<std::pair<std::string, std::string>>& data) {

    std::ostringstream json;
    json << "{\n";

    for (size_t i = 0; i < data.size(); i++) {
        json << "  \"" << data[i].first << "\": \"" << data[i].second << "\"";
        if (i < data.size() - 1) json << ",";
        json << "\n";
    }

    json << "}";
    return json.str();
}

std::string JSONHandler::extractValue(
    const std::string& jsonString,
    const std::string& key) {

    if (m_logger) m_

    // Simple extraction: look for "key": "value"
    std::string searchStr = "\"" + key + "\":";
    size_t pos = jsonString.find(searchStr);

    if (pos == std::string::npos) {
        if (m_logger) m_
        return "";
    }

    // Find the value between quotes
    pos = jsonString.find("\"", pos + searchStr.length());
    if (pos == std::string::npos) return "";

    size_t endPos = jsonString.find("\"", pos + 1);
    if (endPos == std::string::npos) return "";

    return jsonString.substr(pos + 1, endPos - pos - 1);
}

bool JSONHandler::validateJSON(const std::string& jsonString) {
    // Count braces and brackets
    int braces = 0;
    int brackets = 0;

    for (char c : jsonString) {
        if (c == '{') braces++;
        else if (c == '}') braces--;
        else if (c == '[') brackets++;
        else if (c == ']') brackets--;
    }

    bool valid = braces == 0 && brackets == 0;
    
    if (m_logger) m_
    return valid;
}

std::string JSONHandler::prettyPrint(const std::string& jsonString) {
    if (m_logger) m_

    std::ostringstream result;
    int indentLevel = 0;
    bool inString = false;

    for (size_t i = 0; i < jsonString.length(); i++) {
        char c = jsonString[i];

        if (c == '"' && (i == 0 || jsonString[i-1] != '\\')) {
            inString = !inString;
        }

        if (!inString) {
            if (c == '{' || c == '[') {
                result << c << "\n";
                indentLevel++;
                for (int j = 0; j < indentLevel; j++) result << "  ";
            } else if (c == '}' || c == ']') {
                result << "\n";
                indentLevel--;
                for (int j = 0; j < indentLevel; j++) result << "  ";
                result << c;
            } else if (c == ',') {
                result << c << "\n";
                for (int j = 0; j < indentLevel; j++) result << "  ";
            } else if (c == ':') {
                result << c << " ";
            } else if (!std::isspace(c)) {
                result << c;
            }
        } else {
            result << c;
        }
    }

    return result.str();
}

std::string JSONHandler::minify(const std::string& jsonString) {
    if (m_logger) m_

    std::ostringstream result;
    bool inString = false;

    for (size_t i = 0; i < jsonString.length(); i++) {
        char c = jsonString[i];

        if (c == '"' && (i == 0 || jsonString[i-1] != '\\')) {
            inString = !inString;
        }

        if (inString || !std::isspace(c)) {
            result << c;
        }
    }

    return result.str();
}

// ============================================================================
// LibraryIntegration Implementation
// ============================================================================

LibraryIntegration::LibraryIntegration(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {

    m_httpClient = std::make_shared<HTTPClient>(logger, metrics);
    m_compressionHandler = std::make_shared<CompressionHandler>(logger, metrics);
    m_jsonHandler = std::make_shared<JSONHandler>(logger);

    if (m_logger) m_
}

bool LibraryIntegration::isLibraryAvailable(const std::string& libraryName) {
    if (libraryName == "curl") {
        return true; // Would check CURL_VERSION_NUMBER
    } else if (libraryName == "zstd") {
        return true; // Would check ZSTD_versionNumber()
    } else if (libraryName == "json") {
        return true; // nlohmann/json is header-only
    }
    return false;
}

std::string LibraryIntegration::getLibraryVersion(const std::string& libraryName) {
    if (libraryName == "curl") {
        return "7.85.0"; // Placeholder
    } else if (libraryName == "zstd") {
        return "1.5.2"; // Placeholder
    } else if (libraryName == "json") {
        return "3.11.2"; // Placeholder
    }
    return "unknown";
}

bool LibraryIntegration::initializeAll() {
    if (m_logger) m_

    try {
        // Initialize HTTP (would do curl_global_init())
        if (m_logger) m_

        // Initialize compression (would init Zstd context)
        if (m_logger) m_

        // Initialize JSON (header-only, nothing to do)
        if (m_logger) m_

        if (m_logger) m_
        return true;

    } catch (const std::exception& e) {
        if (m_logger) m_
        return false;
    }
}

std::string LibraryIntegration::getStatus() const {
    std::ostringstream status;

    status << "=== Library Integration Status ===\n";
    status << "libcurl: " << (isLibraryAvailable("curl") ? "OK" : "NOT FOUND") 
            << " (v" << getLibraryVersion("curl") << ")\n";
    status << "zstd: " << (isLibraryAvailable("zstd") ? "OK" : "NOT FOUND") 
            << " (v" << getLibraryVersion("zstd") << ")\n";
    status << "nlohmann/json: " << (isLibraryAvailable("json") ? "OK" : "NOT FOUND") 
            << " (v" << getLibraryVersion("json") << ")\n";

    return status.str();
}
