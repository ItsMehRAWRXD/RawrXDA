#include "symbol_table.h"

#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD {
namespace ZCCF {

struct SymbolTable::Impl {
    std::vector<SymbolEntry> entries;
    std::vector<bool>        inUse;
    std::unordered_map<std::string, uint16_t> nameToSlot;
    std::atomic<uint64_t>    epoch{0};
    mutable std::mutex       mu;

    Impl() : entries(kMaxSymbols), inUse(kMaxSymbols, false) {}
};

namespace {

void CopyBounded(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == nullptr) {
        return;
    }
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

std::string EntryName(const SymbolEntry& entry) {
    return std::string(entry.name);
}

} // namespace

SymbolTable::SymbolTable() : m_impl(std::make_unique<Impl>()) {}
SymbolTable::~SymbolTable() = default;

SymbolResult<SymbolHandle> SymbolTable::Insert(SymbolEntry entry) {
    std::lock_guard<std::mutex> lock(m_impl->mu);

    std::string name = EntryName(entry);
    if (!name.empty() && m_impl->nameToSlot.count(name) != 0) {
        return std::unexpected(SymbolError::DuplicateName);
    }

    for (uint32_t i = 0; i < kMaxSymbols; ++i) {
        if (m_impl->inUse[i]) {
            continue;
        }

        uint8_t version = static_cast<uint8_t>(m_impl->entries[i].version + 1);
        if (version == 0) {
            version = 1;
        }

        entry.version = version;
        entry.handle = SymbolHandle::Make(0, static_cast<uint16_t>(i), version);
        m_impl->entries[i] = entry;
        m_impl->inUse[i] = true;
        if (!name.empty()) {
            m_impl->nameToSlot[name] = static_cast<uint16_t>(i);
        }
        return entry.handle;
    }

    return std::unexpected(SymbolError::TableFull);
}

SymbolResult<void> SymbolTable::Update(SymbolHandle h, const SymbolEntry& updated) {
    std::lock_guard<std::mutex> lock(m_impl->mu);

    uint16_t slot = h.slot();
    if (h.IsNull() || slot >= kMaxSymbols || !m_impl->inUse[slot]) {
        return std::unexpected(SymbolError::NotFound);
    }

    SymbolEntry& current = m_impl->entries[slot];
    if (current.version != h.version()) {
        return std::unexpected(SymbolError::StaleHandle);
    }

    std::string oldName = EntryName(current);
    std::string newName = EntryName(updated);
    if (newName != oldName && !newName.empty() && m_impl->nameToSlot.count(newName) != 0) {
        return std::unexpected(SymbolError::DuplicateName);
    }

    SymbolEntry next = updated;
    next.handle = current.handle;
    next.version = current.version;
    m_impl->entries[slot] = next;

    if (!oldName.empty()) {
        m_impl->nameToSlot.erase(oldName);
    }
    if (!newName.empty()) {
        m_impl->nameToSlot[newName] = slot;
    }
    return {};
}

SymbolResult<void> SymbolTable::Remove(SymbolHandle h) {
    std::lock_guard<std::mutex> lock(m_impl->mu);

    uint16_t slot = h.slot();
    if (h.IsNull() || slot >= kMaxSymbols || !m_impl->inUse[slot]) {
        return std::unexpected(SymbolError::NotFound);
    }

    SymbolEntry& current = m_impl->entries[slot];
    if (current.version != h.version()) {
        return std::unexpected(SymbolError::StaleHandle);
    }

    std::string oldName = EntryName(current);
    if (!oldName.empty()) {
        m_impl->nameToSlot.erase(oldName);
    }

    current = SymbolEntry{};
    current.version = static_cast<uint8_t>(h.version() + 1);
    m_impl->inUse[slot] = false;
    return {};
}

void SymbolTable::Clear() {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    for (uint32_t i = 0; i < kMaxSymbols; ++i) {
        m_impl->entries[i] = SymbolEntry{};
        m_impl->inUse[i] = false;
    }
    m_impl->nameToSlot.clear();
    m_impl->epoch.store(0);
}

const SymbolEntry* SymbolTable::Resolve(SymbolHandle h) const noexcept {
    if (h.IsNull() || h.slot() >= kMaxSymbols) {
        return nullptr;
    }

    const SymbolEntry& entry = m_impl->entries[h.slot()];
    if (!m_impl->inUse[h.slot()] || entry.version != h.version()) {
        return nullptr;
    }
    return &entry;
}

SymbolHandle SymbolTable::FindByName(std::string_view qualifiedName) const noexcept {
    std::lock_guard<std::mutex> lock(m_impl->mu);
    auto it = m_impl->nameToSlot.find(std::string(qualifiedName));
    if (it == m_impl->nameToSlot.end()) {
        return SymbolHandle::Null();
    }
    const SymbolEntry& entry = m_impl->entries[it->second];
    return entry.handle;
}

void SymbolTable::PublishEpoch() noexcept {
    m_impl->epoch.fetch_add(1);
}

uint64_t SymbolTable::CurrentEpoch() const noexcept {
    return m_impl->epoch.load();
}

uint32_t SymbolTable::SymbolCount() const noexcept {
    uint32_t count = 0;
    for (uint32_t i = 0; i < kMaxSymbols; ++i) {
        if (m_impl->inUse[i]) {
            ++count;
        }
    }
    return count;
}

} // namespace ZCCF
} // namespace RawrXD
