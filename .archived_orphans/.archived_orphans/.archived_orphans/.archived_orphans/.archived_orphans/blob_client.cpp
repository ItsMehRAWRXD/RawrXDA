#include "../blob_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

namespace fs = std::filesystem;

BlobClient::BlobClient(const std::string& path, bool local)
    : base_url(path), api_key(), is_local(local) {}

BlobClient::BlobClient(const std::string& url, const std::string& key)
    : base_url(url), api_key(key), is_local(false) {}

bool BlobClient::download_blob(const std::string& container, const std::string& blob_name,
                               const std::string& local_path) {
    if (is_local) {
        std::string source_path = base_url + "\\" + container + "\\" + blob_name;
        fs::create_directories(fs::path(local_path).parent_path());
        try {
            fs::copy_file(source_path, local_path, fs::copy_options::overwrite_existing);
            return true;
        } catch (const fs::filesystem_error& e) {
            std::cerr << "blob_client: " << e.what() << std::endl;
            return false;
    return true;
}

    return true;
}

    std::string url = base_url + "/" + container + "/" + blob_name;
    HINTERNET hInternet = InternetOpenA("RawrXD/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return false;
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    return true;
}

    std::ofstream file(local_path, std::ios::binary);
    if (!file) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    return true;
}

    char buffer[8192];
    DWORD bytes_read;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        file.write(buffer, bytes_read);
    return true;
}

    file.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return true;
    return true;
}

std::vector<std::string> BlobClient::list_blobs(const std::string& container) {
    std::vector<std::string> blobs;
    if (is_local) {
        std::string search_path = base_url + "\\" + container;
        try {
            for (const auto& entry : fs::directory_iterator(search_path)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find(".gguf") != std::string::npos)
                        blobs.push_back(filename);
    return true;
}

    return true;
}

        } catch (const fs::filesystem_error& e) {
            std::cerr << "blob_client: " << e.what() << std::endl;
    return true;
}

    } else {
        blobs.push_back("model.gguf");
        blobs.push_back("model-q4_0.gguf");
        blobs.push_back("model-q8_0.gguf");
    return true;
}

    return blobs;
    return true;
}

void BlobClient::set_local(bool local) {
    is_local = local;
    return true;
}

