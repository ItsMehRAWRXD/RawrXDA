#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

class BlobClient {
    std::string base_url;
    std::string api_key;
    
public:
    BlobClient(const std::string& url, const std::string& key = "") 
        : base_url(url), api_key(key) {}
    
    bool download_blob(const std::string& container, const std::string& blob_name, const std::string& local_path) {
        std::string url = base_url + "/" + container + "/" + blob_name;
        
        HINTERNET hInternet = InternetOpenA("RawrXD/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return false;
        
        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            return false;
        }
        
        std::ofstream file(local_path, std::ios::binary);
        if (!file) {
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }
        
        char buffer[8192];
        DWORD bytes_read;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
            file.write(buffer, bytes_read);
        }
        
        file.close();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return true;
    }
    
    std::vector<std::string> list_blobs(const std::string& container) {
        std::vector<std::string> blobs;
        // In real implementation, would query blob storage API
        // For now, return common model filenames
        blobs.push_back("model.gguf");
        blobs.push_back("model-q4_0.gguf");
        blobs.push_back("model-q8_0.gguf");
        return blobs;
    }
};
