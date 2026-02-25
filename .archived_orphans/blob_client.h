#pragma once
#include <string>
#include <vector>

class BlobClient {
public:
    BlobClient(const std::string& path, bool local);
    BlobClient(const std::string& url, const std::string& key);

    bool download_blob(const std::string& container, const std::string& blob_name,
                      const std::string& local_path);
    std::vector<std::string> list_blobs(const std::string& container);
    void set_local(bool local);

private:
    std::string base_url;
    std::string api_key;
    bool is_local = false;
};
