#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

namespace fs = std::filesystem;

class HFHubClient {
    std::string cache_dir;
    std::string base_url = "https://huggingface.co";
    
public:
    HFHubClient(const std::string& cache = "") {
        if (cache.empty()) {
            char* user_profile;
            size_t len;
            _dupenv_s(&user_profile, &len, "USERPROFILE");
            if (user_profile) {
                cache_dir = std::string(user_profile) + "\\.cache\\huggingface\\hub";
                free(user_profile);
            } else {
                cache_dir = ".\\hf_cache";
            }
        } else {
            cache_dir = cache;
        }
        fs::create_directories(cache_dir);
    }
    
    bool download_file(const std::string& url, const std::string& local_path) {
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
    
    std::string get_model_path(const std::string& repo_id, const std::string& filename) {
        // Convert repo_id to path: "microsoft/DialoGPT-medium" -> "models--microsoft--DialoGPT-medium"
        std::string repo_path = repo_id;
        std::replace(repo_path.begin(), repo_path.end(), '/', '-');
        std::string model_dir = cache_dir + "\\models--" + repo_path;
        
        // Check if file exists in cache
        std::string cached_file = model_dir + "\\snapshots\\main\\" + filename;
        if (fs::exists(cached_file)) {
            return cached_file;
        }
        
        // Download if not cached
        std::string url = base_url + "/" + repo_id + "/resolve/main/" + filename;
        std::string local_file = model_dir + "\\" + filename;
        fs::create_directories(model_dir);
        
        std::cout << "Downloading " << filename << " from " << repo_id << "..." << std::endl;
        if (download_file(url, local_file)) {
            // Create snapshot structure
            std::string snapshot_dir = model_dir + "\\snapshots\\main";
            fs::create_directories(snapshot_dir);
            
            // Move to snapshot (simplified)
            std::string final_path = snapshot_dir + "\\" + filename;
            fs::rename(local_file, final_path);
            return final_path;
        }
        
        return "";
    }
    
    std::vector<std::string> list_model_files(const std::string& repo_id) {
        std::vector<std::string> files;
        // In real implementation, would query HuggingFace API
        // For now, return common GGUF filenames
        files.push_back("model.gguf");
        files.push_back("model-q4_0.gguf");
        files.push_back("model-q8_0.gguf");
        return files;
    }
};
