#pragma once

#include "MmapRegion.hpp"
#include "RuntimeTypes.hpp"

#include <cstdint>
#include <expected>
#include <mutex>
#include <string>
#include <unordered_map>

namespace RawrXD::Runtime {

/// Explicit shard registration only — **no** startup directory scan or index build.
/// Shared views are refcounted; drop last release to unmap.
class ShardResourcePool {
public:
    static ShardResourcePool& instance();

    [[nodiscard]] std::expected<std::uint64_t, std::string> registerShard(const std::string& id,
                                                                         MmapRegion region);
    [[nodiscard]] RuntimeResult addRef(std::uint64_t handle);
    [[nodiscard]] RuntimeResult release(std::uint64_t handle);

    [[nodiscard]] std::expected<const std::uint8_t*, std::string> view(std::uint64_t handle) const;
    [[nodiscard]] std::expected<std::size_t, std::string> size(std::uint64_t handle) const;

private:
    ShardResourcePool() = default;

    struct Entry {
        MmapRegion region;
        int refcount = 1;
        std::string id;
    };

    mutable std::mutex m_mutex{};
    std::unordered_map<std::uint64_t, Entry> m_entries{};
    std::uint64_t m_next = 1;
};

}  // namespace RawrXD::Runtime
