#include "hf_downloader.h"
#include <iostream>
#include <fstream>
#include <cstring>

#if defined(__has_include)
#if __has_include(<curl/curl.h>)
#include <curl/curl.h>
#define RAWRXD_HAS_CURL 1
#else
#define RAWRXD_HAS_CURL 0
#endif
#else
#define RAWRXD_HAS_CURL 0
#endif

#if !RAWRXD_HAS_CURL
#endif

#if RAWRXD_HAS_CURL
// Simple HTTP client using Windows API (or curl)
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
#endif

HFDownloader::HFDownloader()
    : is_downloading_(false), cancel_requested_(false) {
    std::memset(&current_progress_, 0, sizeof(DownloadProgress));
}

HFDownloader::~HFDownloader() {
    CancelDownload();
    if (download_thread_ && download_thread_->joinable()) {
        download_thread_->join();
    }
}

bool HFDownloader::SearchModels(const std::string& query, std::vector<ModelInfo>& results, 
                                const std::string& token) {
    std::string url = "https://huggingface.co/api/models?search=" + query + "&filter=gguf";
    std::string response;
    
    if (!FetchJSON(url, response, token)) {
        std::cerr << "Failed to search models on HuggingFace" << std::endl;
        return false;
    }
    
    // Parse JSON response (simplified - would use nlohmann/json in real implementation)
    std::cout << "Search results: " << response.substr(0, 200) << "..." << std::endl;
    
    return true;
}

bool HFDownloader::GetModelInfo(const std::string& repo_id, ModelInfo& info,
                               const std::string& token) {
    std::string url = "https://huggingface.co/api/models/" + repo_id;
    std::string response;
    
    if (!FetchJSON(url, response, token)) {
        std::cerr << "Failed to fetch model info: " << repo_id << std::endl;
        return false;
    }
    
    // Parse model info and set info struct
    info.repo_id = repo_id;
    info.model_name = repo_id;
    
    return ParseModelMetadata(response, info);
}

bool HFDownloader::DownloadModel(const std::string& repo_id, const std::string& filename,
                                 const std::string& output_dir, ProgressCallback callback,
                                 const std::string& token) {
    std::string url = BuildHFUrl(repo_id, filename);
    std::string output_path = output_dir + "/" + filename;
    
    return DownloadFile(url, output_path, callback, token);
}

bool HFDownloader::DownloadModelAsync(const std::string& repo_id, const std::string& filename,
                                      const std::string& output_dir, ProgressCallback callback,
                                      const std::string& token) {
    if (is_downloading_.load()) {
        std::cerr << "Download already in progress" << std::endl;
        return false;
    }
    
    cancel_requested_ = false;
    is_downloading_ = true;
    progress_callback_ = callback;
    
    download_thread_ = std::make_unique<std::thread>([this, repo_id, filename, output_dir, token]() {
        std::string url = BuildHFUrl(repo_id, filename);
        std::string output_path = output_dir + "/" + filename;
        DownloadFile(url, output_path, progress_callback_, token);
        is_downloading_ = false;
    });
    
    return true;
}

bool HFDownloader::CancelDownload() {
    cancel_requested_ = true;
    return true;
}

bool HFDownloader::ValidateHFToken(const std::string& token) {
    std::string url = "https://huggingface.co/api/whoami";
    std::string response;
    
    return FetchJSON(url, response, token);
}

std::vector<std::string> HFDownloader::ParseAvailableFormats(const std::string& repo_id,
                                                             const std::string& token) {
    std::vector<std::string> formats;
    
    std::string url = "https://huggingface.co/api/models/" + repo_id;
    std::string response;
    
    if (FetchJSON(url, response, token)) {
        // Extract GGUF files from response
        size_t pos = 0;
        while ((pos = response.find(".gguf", pos)) != std::string::npos) {
            size_t start = response.rfind("\"", pos);
            if (start != std::string::npos) {
                start++;
                std::string filename = response.substr(start, pos - start + 5);
                formats.push_back(filename);
            }
            pos++;
        }
    }
    
    return formats;
}

bool HFDownloader::FetchJSON(const std::string& url, std::string& response,
                            const std::string& token) {
#if RAWRXD_HAS_CURL
    // Use libcurl to perform an HTTP GET request and capture the response body.
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL init failed" << std::endl;
        return false;
    }

    struct curl_slist* headers = nullptr;
    if (!token.empty()) {
        std::string auth = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "RawrXD-AgenticIDE/1.0");

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);
    if (!success) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
#else
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(std::string::fromStdString(url)));
    if (!token.empty()) {
        const QByteArray auth = QByteArray("Bearer ") + QByteArray::fromStdString(token);
        request.setRawHeader("Authorization", auth);
    }

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    // Object::  // Signal connection removed\nloop.exec();

    bool success = (reply->error() == QNetworkReply::NoError);
    if (!success) {
        std::cerr << "Qt network error: "
                  << reply->errorString() << std::endl;
    } else {
        const QByteArray data = reply->readAll();
        response.assign(data.constData(), static_cast<size_t>(data.size()));
    }
    reply->deleteLater();
    return success;
#endif
}

bool HFDownloader::DownloadFile(const std::string& url, const std::string& output_path,
                               ProgressCallback callback, const std::string& token) {
    std::cout << "Downloading: " << url << " to " << output_path << std::endl;
#if RAWRXD_HAS_CURL
    // Perform download using libcurl and write directly to file
    FILE* fp = fopen(output_path.c_str(), "wb");
    if (!fp) {
        std::cerr << "Failed to open output file: " << output_path << std::endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "CURL init failed" << std::endl;
        fclose(fp);
        return false;
    }

    struct curl_slist* headers = nullptr;
    if (!token.empty()) {
        std::string auth = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "RawrXD-AgenticIDE/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](void* ptr, size_t size, size_t nmemb, void* stream) -> size_t {
        return fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
    });

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);
    if (!success) {
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
    }

    // Retrieve download size for progress reporting
    curl_off_t dl_total = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl_total);

    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(fp);

    // Populate progress information
    DownloadProgress progress;
    progress.current_file = output_path;
    progress.total_bytes = static_cast<uint64_t>(dl_total);
    progress.bytes_downloaded = success ? static_cast<uint64_t>(dl_total) : 0;
    progress.progress_percent = success && dl_total ? 100.0f : 0.0f;
    if (callback) {
        callback(progress);
    }
    current_progress_ = progress;

    return success;
#else
    // Fallback: CURL not available, download not supported in this build
    std::cerr << "ERROR: HTTP download not supported - rebuild with libcurl" << std::endl;
    std::cerr << "Install libcurl-devel or use pre-downloaded models" << std::endl;
    return false;
#endif
}

std::string HFDownloader::BuildHFUrl(const std::string& repo_id, const std::string& filename) const {
    return "https://huggingface.co/" + repo_id + "/resolve/main/" + filename;
}

std::string HFDownloader::GetAuthHeader(const std::string& token) const {
    return "Bearer " + token;
}

bool HFDownloader::ParseModelMetadata(const std::string& json_response, ModelInfo& info) {
    // Simplified JSON parsing
    // In real implementation, would use nlohmann/json
    return true;
}




