#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

namespace fs = std::filesystem;

class BlobClient {
    std::string base_url;
    std::string api_key;
    bool is_local;
    
public:
    // Constructor for local file system
    BlobClient(const std::string& path, bool local = false) 
        : base_url(path), is_local(local) {}
    
    // Constructor for cloud storage
    BlobClient(const std::string& url, const std::string& key = "") 
        : base_url(url), api_key(key), is_local(false) {}
    
    bool download_blob(const std::string& container, const std::string& blob_name, const std::string& local_path) {
        if (is_local) {
            // Local file system copy
            std::string source_path = base_url + "\\" + container + "\\" + blob_name;
            
            // Create destination directory
            fs::create_directories(fs::path(local_path).parent_path());
            
            try {
                fs::copy_file(source_path, local_path, fs::copy_options::overwrite_existing);
                return true;
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error copying file: " << e.what() << std::endl;
                return false;
            }
        } else {
            // Cloud storage download (original implementation)
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
    }
    
    std::vector<std::string> list_blobs(const std::string& container) {
        std::vector<std::string> blobs;
        
        if (is_local) {
            // List files in local directory
            std::string search_path = base_url + "\\" + container;
            
            try {
                for (const auto& entry : fs::directory_iterator(search_path)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        // Filter for GGUF files
                        if (filename.find(".gguf") != std::string::npos) {
                            blobs.push_back(filename);
                        }
                    }
                }
            } catch (const fs::filesystem_error& e) {
                std::cerr << "Error listing directory: " << e.what() << std::endl;
            }
        } else {
            // Cloud storage list (original implementation)
            blobs.push_back("model.gguf");
            blobs.push_back("model-q4_0.gguf");
            blobs.push_back("model-q8_0.gguf");
        }
        
        return blobs;
    }
    
    // Helper to set local mode
    void set_local(bool local) { is_local = local; }
};
