#pragma once
// RawrCodex.hpp - Stub for Qt-free build
// Provides binary analysis codex for reverse engineering module

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace RawrXD {
namespace ReverseEngineering {

class RawrCodex {
public:
    RawrCodex() = default;
    ~RawrCodex() = default;

    struct CodexEntry {
        std::string name;
        uint64_t address = 0;
        uint64_t size = 0;
        std::string type;
        std::vector<uint8_t> data;
    };

    bool load(const std::string& path) { (void)path; return false; }
    bool save(const std::string& path) const { (void)path; return false; }

    void addEntry(const CodexEntry& entry) { entries_[entry.name] = entry; }
    const CodexEntry* findEntry(const std::string& name) const {
        auto it = entries_.find(name);
        return it != entries_.end() ? &it->second : nullptr;
    }

    size_t entryCount() const { return entries_.size(); }
    void clear() { entries_.clear(); }

private:
    std::map<std::string, CodexEntry> entries_;
};

} // namespace ReverseEngineering
} // namespace RawrXD
