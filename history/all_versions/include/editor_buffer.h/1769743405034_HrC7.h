#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>

class BufferModel {
public:
    BufferModel() = default;

    void set(const std::string& text);
    std::string snapshot() const;
    std::string getText(std::size_t pos, std::size_t len) const;
    void insert(std::size_t pos, std::string_view text);
    void erase(std::size_t pos, std::size_t len);
    std::size_t size() const;

private:
    static std::size_t clampPos(std::size_t pos, std::size_t size);

    mutable std::mutex mutex_;
    std::string text_;
};
