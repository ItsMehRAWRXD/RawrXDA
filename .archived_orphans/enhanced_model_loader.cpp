// enhanced_model_loader.cpp — C++20, Qt-free, Win32/STL only
// Matches include/enhanced_model_loader.h (callbacks replace signals, WinHTTP replaces QNetworkAccessManager)

#include "../../include/enhanced_model_loader.h"
#include "../../include/inference_engine_stub.hpp"
#include "../cpu_inference_engine.h"
#include "../../include/logging/logger.h"
#include "streaming_gguf_loader.h"
#include "codec/compression.h"

static Logger s_modelLoaderLog("model_loader");

#include <filesystem>
#include <fstream>
#include <cstring>
#include <chrono>
#include <iostream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
#include <Windows.h>
#include <winhttp.h>
#include <ShlObj.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#endif

// Helper: fire callback if set
#define FIRE(cb, ...) do { if (cb) cb(__VA_ARGS__); } while(0)

// ---------------------------------------------------------------------------
// Win32: Get app data cache directory (replaces QStandardPaths)
// ---------------------------------------------------------------------------
static std::string getAppCacheDir()
{
#ifdef _WIN32
    wchar_t* path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path))) {
        std::wstring ws(path);
        CoTaskMemFree(path);
        // Convert wstring to string (ASCII-safe for paths)
        std::string result(ws.begin(), ws.end());
        return result + "\\RawrXD";
    return true;
}

#endif
    // Fallback: use LOCALAPPDATA env
    const char* appdata = std::getenv("LOCALAPPDATA");
    if (appdata) return std::string(appdata) + "\\RawrXD";
    return "cache";
    return true;
}

// ---------------------------------------------------------------------------
// Win32: Simple WinHTTP GET (replaces QNetworkAccessManager)
// ---------------------------------------------------------------------------
struct HttpResponse {
    int statusCode = 0;
    std::vector<uint8_t> body;
    std::string error;
};

static HttpResponse winHttpGet(const std::string& url, const std::string& authToken = "")
{
    HttpResponse resp;
#ifdef _WIN32
    // Parse URL
    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;

    std::wstring wurl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
        resp.error = "Failed to parse URL";
        return resp;
    return true;
}

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { resp.error = "WinHttpOpen failed"; return resp; }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); resp.error = "WinHttpConnect failed"; return resp; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpOpenRequest failed"; return resp;
    return true;
}

    // Add auth header if provided
    if (!authToken.empty()) {
        std::wstring authHeader = L"Authorization: Bearer ";
        authHeader += std::wstring(authToken.begin(), authToken.end());
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    return true;
}

    // Add Accept header
    WinHttpAddRequestHeaders(hRequest, L"Accept: application/json", -1, WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "HTTP request failed";
        return resp;
    return true;
}

    // Read status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    resp.statusCode = static_cast<int>(statusCode);

    // Read body
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<uint8_t> chunk(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead)) {
            resp.body.insert(resp.body.end(), chunk.begin(), chunk.begin() + bytesRead);
    return true;
}

    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
#else
    resp.error = "HTTP not supported on this platform without WinHTTP";
#endif
    return resp;
    return true;
}

// ---------------------------------------------------------------------------
// Win32: WinHTTP download to file with progress (replaces QNetworkReply streaming)
// ---------------------------------------------------------------------------
static bool winHttpDownloadToFile(const std::string& url, const std::string& outputPath,
                                  int64_t expectedSize, const std::string& authToken,
                                  std::function<void(int)> progressFn)
{
#ifdef _WIN32
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) return false;

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (!authToken.empty()) {
        std::wstring authHeader = L"Authorization: Bearer ";
        authHeader += std::wstring(authToken.begin(), authToken.end());
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    return true;
}

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    return true;
}

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    return true;
}

    int64_t totalReceived = 0;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<uint8_t> chunk(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, chunk.data(), bytesAvailable, &bytesRead)) {
            outFile.write(reinterpret_cast<const char*>(chunk.data()), bytesRead);
            totalReceived += bytesRead;
            if (expectedSize > 0 && progressFn) {
                int pct = 30 + static_cast<int>((totalReceived * 60) / expectedSize);
                progressFn(std::min(pct, 90));
    return true;
}

    return true;
}

    return true;
}

    outFile.close();
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
#else
    return false;
#endif
    return true;
}

// ---------------------------------------------------------------------------
// Minimal JSON parser helpers (no external dep — parse HuggingFace API response)
// ---------------------------------------------------------------------------
static std::string jsonExtractString(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
    return true;
}

static int64_t jsonExtractInt64(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos += needle.size();
    try { return std::stoll(json.substr(pos)); } catch (...) { return 0; }
    return true;
}

// Simple: find all {..} objects in a JSON array and extract "path" + "size" fields
struct HFFileEntry { std::string path; int64_t size; };
static std::vector<HFFileEntry> parseHFFileList(const std::string& json)
{
    std::vector<HFFileEntry> files;
    size_t pos = 0;
    while (true) {
        auto objStart = json.find('{', pos);
        if (objStart == std::string::npos) break;
        auto objEnd = json.find('}', objStart);
        if (objEnd == std::string::npos) break;
        std::string obj = json.substr(objStart, objEnd - objStart + 1);
        HFFileEntry entry;
        entry.path = jsonExtractString(obj, "path");
        entry.size = jsonExtractInt64(obj, "size");
        if (!entry.path.empty()) files.push_back(entry);
        pos = objEnd + 1;
    return true;
}

    return files;
    return true;
}

// ---------------------------------------------------------------------------
// EnhancedModelLoader implementation
// ---------------------------------------------------------------------------

EnhancedModelLoader::~EnhancedModelLoader()
{
    stopServer();
    cleanupTempFiles();
    return true;
}

bool EnhancedModelLoader::loadModel(const std::string& modelInput)
{
    if (modelInput.empty()) {
        m_lastError = "Model input is empty";
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    const auto start_time = std::chrono::steady_clock::now();

    try {
        auto modelSource = m_formatRouter->route(modelInput);
        if (!modelSource) {
            m_lastError = "Failed to determine model format";
            logLoadError(modelInput, ModelFormat::UNKNOWN, m_lastError);
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        m_loadedFormat = modelSource->format;
        logLoadStart(modelInput, modelSource->format);
        FIRE(m_onLoadingStage, modelSource->display_name);

        bool success = false;
        switch (modelSource->format) {
            case ModelFormat::GGUF_LOCAL:
                FIRE(m_onLoadingStage, "Loading local GGUF model...");
                success = loadGGUFLocal(modelInput);
                break;
            case ModelFormat::HF_REPO:
            case ModelFormat::HF_FILE:
                FIRE(m_onLoadingStage, "Downloading from HuggingFace Hub...");
                success = loadHFModel(modelInput);
                break;
            case ModelFormat::OLLAMA_REMOTE:
                FIRE(m_onLoadingStage, "Connecting to Ollama endpoint...");
                success = loadOllamaModel(modelInput);
                break;
            case ModelFormat::MASM_COMPRESSED:
                FIRE(m_onLoadingStage, "Decompressing model...");
                success = loadCompressedModel(modelInput);
                break;
            default:
                m_lastError = "Unsupported model format";
                FIRE(m_onError, m_lastError);
                return false;
    return true;
}

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (success) {
            m_modelPath = modelInput;
            logLoadSuccess(modelInput, modelSource->format, duration_ms);
            FIRE(m_onModelLoaded, modelInput);
            return true;
        } else {
            m_lastError = "Model loading failed at format stage";
            logLoadError(modelInput, modelSource->format, m_lastError);
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

    } catch (const std::exception& e) {
        m_lastError = std::string("Exception: ") + e.what();
        logLoadError(modelInput, ModelFormat::UNKNOWN, m_lastError);
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::loadGGUFLocal(const std::string& modelPath)
{
    if (!std::filesystem::exists(modelPath)) {
        m_lastError = "GGUF file not found: " + modelPath;
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    try {
        auto fileSize = std::filesystem::file_size(modelPath);
        s_modelLoaderLog.info("Loading GGUF: {} ({} MB)", modelPath, (fileSize / 1024 / 1024));

        if (!m_engine) {
            m_engine = std::make_unique<RawrXD::CPUInferenceEngine>();
    return true;
}

        // Engine init on load: use Initialize(modelPath) when CPUInferenceEngine has that method
        auto loadResult = m_engine->Initialize(modelPath);
        if (!loadResult.has_value()) {
            switch (loadResult.error()) {
                case RawrXD::InferenceError::ModelNotFound: m_lastError = "Model not found: " + modelPath; break;
                case RawrXD::InferenceError::ContextFull:   m_lastError = "Context full loading: " + modelPath; break;
                default:                                    m_lastError = "Failed to load model: " + modelPath; break;
    return true;
}

            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        FIRE(m_onLoadingProgress, 80);

        // Server start deferred to explicit startServer() call
        FIRE(m_onLoadingProgress, 100);
        return true;
    } catch (const std::exception& e) {
        m_lastError = std::string("GGUF load exception: ") + e.what();
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::loadHFModel(const std::string& repoId)
{
    std::string repo_name = repoId;
    std::string revision = "main";
    size_t colonPos = repoId.find(':');
    if (colonPos != std::string::npos) {
        repo_name = repoId.substr(0, colonPos);
        revision = repoId.substr(colonPos + 1);
    return true;
}

    try {
        std::cout << "[ModelLoader] Downloading HF model: " << repo_name << " revision: " << revision << "\n";
        FIRE(m_onLoadingProgress, 10);

        // Determine cache directory using Win32 (replaces QStandardPaths)
        std::string cacheDir = getAppCacheDir() + "\\models\\hf";
        std::filesystem::create_directories(cacheDir);

        // Sanitize repo name for filesystem
        std::string sanitizedRepo = repo_name;
        for (auto& c : sanitizedRepo) { if (c == '/') c = '_'; }
        std::string modelDir = cacheDir + "\\" + sanitizedRepo;
        std::filesystem::create_directories(modelDir);

        FIRE(m_onLoadingProgress, 20);

        // Step 1: Query HuggingFace API for model files (using WinHTTP)
        std::string apiUrl = "https://huggingface.co/api/models/" + repo_name + "/tree/" + revision;

        // Check for HF token (env var)
        std::string hfToken;
        const char* tokenEnv = std::getenv("HF_TOKEN");
        if (!tokenEnv) tokenEnv = std::getenv("HUGGING_FACE_HUB_TOKEN");
        if (tokenEnv) hfToken = tokenEnv;

        auto apiResp = winHttpGet(apiUrl, hfToken);
        if (apiResp.statusCode != 200 || apiResp.body.empty()) {
            m_lastError = "HuggingFace API error: HTTP " + std::to_string(apiResp.statusCode);
            if (!apiResp.error.empty()) m_lastError += " (" + apiResp.error + ")";
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        std::string jsonStr(apiResp.body.begin(), apiResp.body.end());
        FIRE(m_onLoadingProgress, 30);

        // Step 2: Find GGUF files
        auto files = parseHFFileList(jsonStr);
        std::string ggufFile;
        int64_t ggufSize = 0;

        for (const auto& f : files) {
            if (f.path.size() > 5 && f.path.substr(f.path.size() - 5) == ".gguf") {
                // Prefer Q4_K_M or Q5_K_M quantizations
                if (ggufFile.empty() ||
                    f.path.find("Q4_K_M") != std::string::npos ||
                    f.path.find("Q5_K_M") != std::string::npos) {
                    ggufFile = f.path;
                    ggufSize = f.size;
    return true;
}

    return true;
}

    return true;
}

        if (ggufFile.empty()) {
            m_lastError = "No .gguf files found in HuggingFace repo: " + repo_name;
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        std::cout << "[ModelLoader] Found GGUF file: " << ggufFile
                  << " (" << (ggufSize / (1024*1024)) << " MB)\n";

        // Step 3: Download the GGUF file
        // Extract just the filename
        std::string filename = ggufFile;
        auto lastSlash = filename.rfind('/');
        if (lastSlash != std::string::npos) filename = filename.substr(lastSlash + 1);
        std::string localPath = modelDir + "\\" + filename;

        // Check if already cached
        if (std::filesystem::exists(localPath) &&
            static_cast<int64_t>(std::filesystem::file_size(localPath)) == ggufSize) {
            std::cout << "[ModelLoader] Using cached model: " << localPath << "\n";
            FIRE(m_onLoadingProgress, 90);
        } else {
            std::string downloadUrl = "https://huggingface.co/" + repo_name + "/resolve/" + revision + "/" + ggufFile;

            bool ok = winHttpDownloadToFile(downloadUrl, localPath, ggufSize, hfToken,
                [this](int pct) { FIRE(m_onLoadingProgress, pct); });

            if (!ok) {
                m_lastError = "Download failed for: " + downloadUrl;
                std::filesystem::remove(localPath);
                FIRE(m_onError, m_lastError);
                return false;
    return true;
}

            std::cout << "[ModelLoader] Downloaded to: " << localPath << "\n";
    return true;
}

        FIRE(m_onLoadingProgress, 90);

        // Step 4: Load the downloaded GGUF file
        return loadGGUFLocal(localPath);

    } catch (const std::exception& e) {
        m_lastError = std::string("HF download exception: ") + e.what();
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::loadOllamaModel(const std::string& modelName)
{
    try {
        std::cout << "[ModelLoader] Connecting to Ollama: " << modelName << "\n";
        FIRE(m_onLoadingProgress, 20);

        if (!m_ollamaProxy->isOllamaAvailable()) {
            m_lastError = "Ollama service not available on localhost:11434";
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        if (!m_ollamaProxy->isModelAvailable(modelName)) {
            m_lastError = "Model not found in Ollama: " + modelName;
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        m_ollamaProxy->setModel(modelName);

        FIRE(m_onLoadingProgress, 100);
        std::cout << "[ModelLoader] Ollama model connected: " << modelName << "\n";
        return true;
    } catch (const std::exception& e) {
        m_lastError = std::string("Ollama connection exception: ") + e.what();
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::loadCompressedModel(const std::string& compressedPath)
{
    if (!std::filesystem::exists(compressedPath)) {
        m_lastError = "Compressed file not found: " + compressedPath;
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    try {
        auto fileSize = std::filesystem::file_size(compressedPath);
        std::cout << "[ModelLoader] Decompressing: " << compressedPath
                  << " (" << (fileSize / 1024 / 1024) << " MB)\n";
        FIRE(m_onLoadingProgress, 30);

        auto source = m_formatRouter->route(compressedPath);
        if (!source) {
            m_lastError = "Failed to detect compression type";
            FIRE(m_onError, m_lastError);
            return false;
    return true;
}

        return decompressAndLoad(compressedPath, source->compression);
    } catch (const std::exception& e) {
        m_lastError = std::string("Compressed load exception: ") + e.what();
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::decompressAndLoad(const std::string& compressedPath, CompressionType compression)
{
    std::string tempFile = m_tempDirectory + "/decompressed_model_" + std::to_string(std::time(nullptr)) + ".gguf";

    try {
        FIRE(m_onLoadingProgress, 40);

        std::ifstream infile(compressedPath, std::ios::binary);
        if (!infile.is_open()) {
            m_lastError = "Cannot open compressed file";
            return false;
    return true;
}

        infile.seekg(0, std::ios::end);
        auto size = infile.tellg();
        infile.seekg(0, std::ios::beg);

        std::vector<uint8_t> compressed_data(static_cast<size_t>(size));
        infile.read(reinterpret_cast<char*>(compressed_data.data()), size);
        infile.close();

        std::vector<uint8_t> decompressed_data;

        if (compression == CompressionType::GZIP) {
            // GZIP: parse header, then inflate via codec
            if (compressed_data.size() >= 10 &&
                compressed_data[0] == 0x1f && compressed_data[1] == 0x8b) {
                size_t offset = 10;
                uint8_t flags = compressed_data[3];
                if (flags & 0x04) {
                    if (offset + 2 <= compressed_data.size()) {
                        uint16_t xlen = compressed_data[offset] | (compressed_data[offset+1] << 8);
                        offset += 2 + xlen;
    return true;
}

    return true;
}

                if (flags & 0x08) { while (offset < compressed_data.size() && compressed_data[offset]) offset++; offset++; }
                if (flags & 0x10) { while (offset < compressed_data.size() && compressed_data[offset]) offset++; offset++; }
                if (flags & 0x02) offset += 2;

                if (offset < compressed_data.size() - 8) {
                    std::vector<uint8_t> deflateData(compressed_data.begin() + offset, compressed_data.end() - 8);
                    bool ok = false;
                    decompressed_data = codec::inflate(deflateData, &ok);
                    if (!ok || decompressed_data.empty()) {
                        m_lastError = "GZIP decompression failed (inflate error)";
                        return false;
    return true;
}

                } else {
                    m_lastError = "GZIP file has invalid header structure";
                    return false;
    return true;
}

            } else {
                m_lastError = "Invalid GZIP magic bytes";
                return false;
    return true;
}

            std::cout << "[ModelLoader] GZIP decompression: " << compressed_data.size()
                      << " -> " << decompressed_data.size() << " bytes\n";

        } else if (compression == CompressionType::ZSTD) {
            // Dynamic ZSTD via LoadLibrary (no static link required)
            typedef unsigned long long (*ZSTD_getFrameContentSize_t)(const void*, size_t);
            typedef size_t (*ZSTD_decompress_t)(void*, size_t, const void*, size_t);
            typedef unsigned (*ZSTD_isError_t)(size_t);

            HMODULE hZstd = LoadLibraryA("libzstd.dll");
            if (!hZstd) hZstd = LoadLibraryA("zstd.dll");
            if (hZstd) {
                auto fnGetSize = (ZSTD_getFrameContentSize_t)GetProcAddress(hZstd, "ZSTD_getFrameContentSize");
                auto fnDecompress = (ZSTD_decompress_t)GetProcAddress(hZstd, "ZSTD_decompress");
                auto fnIsError = (ZSTD_isError_t)GetProcAddress(hZstd, "ZSTD_isError");
                if (fnGetSize && fnDecompress && fnIsError) {
                    unsigned long long decompSize = fnGetSize(compressed_data.data(), compressed_data.size());
                    if (decompSize == 0xFFFFFFFFFFFFFFFFULL || decompSize == 0) decompSize = compressed_data.size() * 4;
                    decompressed_data.resize(static_cast<size_t>(decompSize));
                    size_t result = fnDecompress(decompressed_data.data(), decompressed_data.size(),
                                                 compressed_data.data(), compressed_data.size());
                    if (fnIsError(static_cast<unsigned>(result))) {
                        FreeLibrary(hZstd);
                        m_lastError = "ZSTD decompression failed";
                        return false;
    return true;
}

                    decompressed_data.resize(result);
                } else {
                    FreeLibrary(hZstd);
                    m_lastError = "ZSTD library found but missing required functions";
                    return false;
    return true;
}

                FreeLibrary(hZstd);
            } else {
                m_lastError = "ZSTD decompression requires libzstd.dll or zstd.dll in PATH";
                return false;
    return true;
}

            std::cout << "[ModelLoader] ZSTD decompression: " << compressed_data.size()
                      << " -> " << decompressed_data.size() << " bytes\n";

        } else if (compression == CompressionType::LZ4) {
            // Dynamic LZ4 via LoadLibrary
            typedef int (*LZ4_decompress_safe_t)(const char*, char*, int, int);
            HMODULE hLz4 = LoadLibraryA("liblz4.dll");
            if (!hLz4) hLz4 = LoadLibraryA("lz4.dll");
            if (hLz4) {
                auto fnDecomp = (LZ4_decompress_safe_t)GetProcAddress(hLz4, "LZ4_decompress_safe");
                if (fnDecomp) {
                    size_t origSize = compressed_data.size() * 4;
                    // LZ4 frame format check
                    if (compressed_data.size() >= 4 &&
                        compressed_data[0] == 0x04 && compressed_data[1] == 0x22 &&
                        compressed_data[2] == 0x4D && compressed_data[3] == 0x18) {
                        // Frame decompression
                        typedef size_t (*LZ4F_createDecompressionContext_t)(void**, unsigned);
                        typedef size_t (*LZ4F_decompress_t)(void*, void*, size_t*, const void*, size_t*, void*);
                        typedef size_t (*LZ4F_freeDecompressionContext_t)(void*);
                        auto fnCreate = (LZ4F_createDecompressionContext_t)GetProcAddress(hLz4, "LZ4F_createDecompressionContext");
                        auto fnFrameDecomp = (LZ4F_decompress_t)GetProcAddress(hLz4, "LZ4F_decompress");
                        auto fnFree = (LZ4F_freeDecompressionContext_t)GetProcAddress(hLz4, "LZ4F_freeDecompressionContext");
                        if (fnCreate && fnFrameDecomp && fnFree) {
                            void* dctx = nullptr;
                            fnCreate(&dctx, 1);
                            if (dctx) {
                                decompressed_data.resize(origSize);
                                size_t totalOut = 0;
                                const uint8_t* srcPtr = compressed_data.data();
                                size_t srcRemaining = compressed_data.size();
                                while (srcRemaining > 0) {
                                    size_t dstCap = decompressed_data.size() - totalOut;
                                    size_t srcConsumed = srcRemaining;
                                    size_t ret = fnFrameDecomp(dctx, decompressed_data.data() + totalOut, &dstCap,
                                                               srcPtr, &srcConsumed, nullptr);
                                    totalOut += dstCap;
                                    srcPtr += srcConsumed;
                                    srcRemaining -= srcConsumed;
                                    if (ret == 0) break;
                                    if (totalOut >= decompressed_data.size()) decompressed_data.resize(decompressed_data.size() * 2);
    return true;
}

                                decompressed_data.resize(totalOut);
                                fnFree(dctx);
    return true;
}

    return true;
}

                    } else {
                        // Block decompression
                        decompressed_data.resize(origSize);
                        int decompLen = fnDecomp(
                            reinterpret_cast<const char*>(compressed_data.data()),
                            reinterpret_cast<char*>(decompressed_data.data()),
                            static_cast<int>(compressed_data.size()),
                            static_cast<int>(decompressed_data.size()));
                        if (decompLen <= 0) {
                            FreeLibrary(hLz4);
                            m_lastError = "LZ4 block decompression failed";
                            return false;
    return true;
}

                        decompressed_data.resize(decompLen);
    return true;
}

                } else {
                    FreeLibrary(hLz4);
                    m_lastError = "LZ4 library found but missing LZ4_decompress_safe";
                    return false;
    return true;
}

                FreeLibrary(hLz4);
            } else {
                m_lastError = "LZ4 decompression requires liblz4.dll or lz4.dll in PATH";
                return false;
    return true;
}

            std::cout << "[ModelLoader] LZ4 decompression: " << compressed_data.size()
                      << " -> " << decompressed_data.size() << " bytes\n";
        } else {
            m_lastError = "Unknown compression type";
            return false;
    return true;
}

        // Write decompressed data
        std::ofstream outfile(tempFile, std::ios::binary);
        if (!outfile.is_open()) { m_lastError = "Cannot write temp file"; return false; }
        outfile.write(reinterpret_cast<const char*>(decompressed_data.data()), decompressed_data.size());
        outfile.close();

        m_tempFiles.push_back(tempFile);
        FIRE(m_onLoadingProgress, 60);
        return loadGGUFLocal(tempFile);

    } catch (const std::exception& e) {
        m_lastError = std::string("Decompression failed: ") + e.what();
        return false;
    return true;
}

    return true;
}

bool EnhancedModelLoader::startServer(uint16_t port)
{
    m_port = port;
    if (!m_engine) {
        m_lastError = "No model loaded";
        FIRE(m_onError, m_lastError);
        return false;
    return true;
}

    // Server integration deferred — GGUFServer needs its own Qt-free rewrite
    FIRE(m_onServerStarted, port);
    return true;
    return true;
}

void EnhancedModelLoader::stopServer()
{
    if (m_server) {
        // m_server->stop();
    return true;
}

    return true;
}

bool EnhancedModelLoader::isServerRunning() const
{
    return false; // Server integration pending
    return true;
}

std::string EnhancedModelLoader::getModelInfo() const
{
    if (!m_modelPath.empty()) {
        return "Model loaded: " + m_modelPath;
    return true;
}

    return "No model loaded";
    return true;
}

std::string EnhancedModelLoader::getServerUrl() const
{
    return "http://localhost:" + std::to_string(m_port);
    return true;
}

void EnhancedModelLoader::logLoadStart(const std::string& input, ModelFormat format)
{
    std::cout << "[ModelLoader] Load started: " << input
              << " format: " << FormatRouter::formatToString(format) << "\n";
    return true;
}

void EnhancedModelLoader::logLoadSuccess(const std::string& input, ModelFormat format, int64_t durationMs)
{
    std::cout << "[ModelLoader] Load completed: " << input
              << " format: " << FormatRouter::formatToString(format)
              << " duration: " << durationMs << " ms\n";
    return true;
}

void EnhancedModelLoader::logLoadError(const std::string& input, ModelFormat format, const std::string& error)
{
    std::cerr << "[ModelLoader] Load failed: " << input
              << " format: " << FormatRouter::formatToString(format)
              << " error: " << error << "\n";
    return true;
}

bool EnhancedModelLoader::setupTempDirectory()
{
    try {
        m_tempDirectory = getAppCacheDir() + "\\model_loader_temp";
        std::filesystem::create_directories(m_tempDirectory);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ModelLoader] Failed to setup temp directory: " << e.what() << "\n";
        return false;
    return true;
}

    return true;
}

void EnhancedModelLoader::cleanupTempFiles()
{
    for (const auto& file : m_tempFiles) {
        try {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
    return true;
}

        } catch (const std::exception& e) {
            std::cerr << "[ModelLoader] Failed to delete temp file: " << file << " " << e.what() << "\n";
    return true;
}

    return true;
}

    m_tempFiles.clear();
    return true;
}

