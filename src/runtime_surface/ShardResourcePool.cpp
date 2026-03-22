#include "rawrxd/runtime/ShardResourcePool.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

ShardResourcePool& ShardResourcePool::instance() {
    static ShardResourcePool s;
    return s;
}

std::expected<std::uint64_t, std::string> ShardResourcePool::registerShard(const std::string& id,
                                                                         MmapRegion region) {
    if (!region.view || region.sizeBytes == 0) {
        return std::unexpected("ShardResourcePool: empty region");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t h = m_next++;
    m_entries[h] = Entry{std::move(region), 1, id};

    RawrXD::Logging::Logger::instance().info(
        "[ShardPool] register shard id=\"" + id + "\" handle=" + std::to_string(h) +
            " bytes=" + std::to_string(m_entries[h].region.sizeBytes),
        "RuntimeSurface");
    return h;
}

RuntimeResult ShardResourcePool::addRef(std::uint64_t handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(handle);
    if (it == m_entries.end()) {
        return std::unexpected("ShardResourcePool: unknown handle");
    }
    ++it->second.refcount;
    return {};
}

RuntimeResult ShardResourcePool::release(std::uint64_t handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(handle);
    if (it == m_entries.end()) {
        return std::unexpected("ShardResourcePool: unknown handle");
    }
    --it->second.refcount;
    if (it->second.refcount <= 0) {
        RawrXD::Logging::Logger::instance().info(
            "[ShardPool] drop shard handle=" + std::to_string(handle) + " id=\"" + it->second.id + "\"",
            "RuntimeSurface");
        m_entries.erase(it);
    }
    return {};
}

std::expected<const std::uint8_t*, std::string> ShardResourcePool::view(std::uint64_t handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(handle);
    if (it == m_entries.end()) {
        return std::unexpected("ShardResourcePool: unknown handle");
    }
    return it->second.region.bytes();
}

std::expected<std::size_t, std::string> ShardResourcePool::size(std::uint64_t handle) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(handle);
    if (it == m_entries.end()) {
        return std::unexpected("ShardResourcePool: unknown handle");
    }
    return it->second.region.size();
}

}  // namespace RawrXD::Runtime
