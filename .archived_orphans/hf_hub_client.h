#pragma once

/**
 * @file hf_hub_client.h
 * @brief Stub: HuggingFace Hub client (impl in src/modules/hf_hub_client.cpp).
 */

#include <string>

class HFHubClient {
public:
    explicit HFHubClient(const std::string& cache = "");
    bool download_file(const std::string& url, const std::string& local_path);
    std::string get_model_path(const std::string& repo_id, const std::string& filename);
private:
    std::string cache_dir;
};
