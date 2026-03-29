#include "ast_arena.h"

#include <algorithm>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace RawrXD {
namespace ZCCF {

namespace {

constexpr uint32_t kEncodableSlotsPerChunk = 0xFFFFu;

}

AstArena::AstArena() = default;

AstArena::~AstArena() {
    for (uint8_t i = 0; i < m_chunkCount; ++i) {
        if (m_chunks[i].memory != nullptr) {
            VirtualFree(m_chunks[i].memory, 0, MEM_RELEASE);
            m_chunks[i].memory = nullptr;
        }
    }
}

bool AstArena::AllocChunk() {
    if (m_chunkCount >= kMaxChunks) {
        return false;
    }

    void* mem = VirtualAlloc(nullptr, kChunkBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (mem == nullptr) {
        return false;
    }

    m_chunks[m_chunkCount].memory = mem;
    m_chunks[m_chunkCount].nextSlot.store(0);
    ++m_chunkCount;
    return true;
}

ArenaResult<AstHandle> AstArena::Alloc(const AstNode& node) {
    for (uint8_t chunkIndex = 0; chunkIndex < kMaxChunks; ++chunkIndex) {
        if (chunkIndex >= m_chunkCount) {
            if (!AllocChunk()) {
                return std::unexpected(ArenaError::OutOfMemory);
            }
        }

        Chunk& chunk = m_chunks[chunkIndex];
        uint32_t slot = chunk.nextSlot.fetch_add(1);
        if (slot >= kEncodableSlotsPerChunk || slot >= kSlotsPerChunk) {
            chunk.nextSlot.fetch_sub(1);
            continue;
        }

        auto* nodes = static_cast<AstNode*>(chunk.memory);
        nodes[slot] = node;
        AstHandle h = AstHandle::Make(chunkIndex, static_cast<uint16_t>(slot), m_generation.load());
        return h;
    }

    return std::unexpected(ArenaError::OutOfMemory);
}

const AstNode* AstArena::Resolve(AstHandle h) const noexcept {
    if (h.IsNull() || h.generation() != m_generation.load()) {
        return nullptr;
    }
    if (h.chunkId() >= m_chunkCount) {
        return nullptr;
    }

    const Chunk& chunk = m_chunks[h.chunkId()];
    if (chunk.memory == nullptr) {
        return nullptr;
    }
    if (h.slot() >= chunk.nextSlot.load()) {
        return nullptr;
    }

    const auto* nodes = static_cast<const AstNode*>(chunk.memory);
    return &nodes[h.slot()];
}

AstNode* AstArena::ResolveMut(AstHandle h) noexcept {
    return const_cast<AstNode*>(static_cast<const AstArena*>(this)->Resolve(h));
}

bool AstArena::Reset() {
    if (IsPinned()) {
        return false;
    }

    for (uint8_t i = 0; i < m_chunkCount; ++i) {
        if (m_chunks[i].memory != nullptr) {
            std::memset(m_chunks[i].memory, 0, kChunkBytes);
            m_chunks[i].nextSlot.store(0);
        }
    }

    uint8_t next = static_cast<uint8_t>(m_generation.load() + 1);
    if (next == 0) {
        next = 1;
    }
    m_generation.store(next);
    return true;
}

uint8_t AstArena::CurrentGeneration() const noexcept {
    return m_generation.load();
}

size_t AstArena::NodeCount() const noexcept {
    size_t total = 0;
    for (uint8_t i = 0; i < m_chunkCount; ++i) {
        total += static_cast<size_t>(m_chunks[i].nextSlot.load());
    }
    return total;
}

ArenaResult<void> AstArena::Pin() {
    uint8_t depth = m_pinDepth.load();
    if (depth >= kMaxPinDepth) {
        return std::unexpected(ArenaError::PinDepthExceeded);
    }
    m_pinDepth.fetch_add(1);
    return {};
}

void AstArena::Unpin() noexcept {
    uint8_t depth = m_pinDepth.load();
    if (depth > 0) {
        m_pinDepth.fetch_sub(1);
    }
}

bool AstArena::IsPinned() const noexcept {
    return m_pinDepth.load() != 0;
}

} // namespace ZCCF
} // namespace RawrXD
