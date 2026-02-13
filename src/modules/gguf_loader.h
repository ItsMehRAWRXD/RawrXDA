#pragma once
#include <string>

class GGUFLoader {
public:
    GGUFLoader();
    ~GGUFLoader();
    bool Load(const std::string& path);
    void* get_data() const;
    size_t get_size() const;
};
