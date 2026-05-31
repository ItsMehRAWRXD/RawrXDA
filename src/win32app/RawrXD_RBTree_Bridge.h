#pragma once

// RawrXD_RBTree_Bridge.h — C++ Bridge for MASM x64 Red-Black Tree
// Drop-in replacement for std::map<uint64_t, uint64_t>
// Eliminates xtree/xmemory crashes by bypassing the STL entirely
//
// Usage:
//   RawrXD_RBTree map;
//   map.Insert(42, 100);
//   uint64_t val = map.Get(42);  // returns 100
//   map.Clear();
//
// Link with: RawrXD_RBTree.obj (assembled from RawrXD_RBTree.asm)

#include <cstdint>
#include <functional>

// ============================================================================
// MASM Function Declarations (extern "C" to prevent name mangling)
// ============================================================================

extern "C" {
    // Initialize the heap allocator (call once at startup)
    void RB_Init(void);

    // Core operations
    int  RB_Insert(uint64_t key, uint64_t value);
    void* RB_Find(uint64_t key);           // Returns RB_NODE* or nullptr
    uint64_t RB_GetValue(uint64_t key);    // Returns 0 if not found
    uint64_t RB_GetCount(void);
    void* RB_GetRoot(void);                // Returns RB_NODE* or nullptr
    void RB_Clear(void);

    // Node accessors (for iteration)
    // Node layout (48 bytes):
    //   +0x00  Parent   (void*)
    //   +0x08  Left     (void*)
    //   +0x10  Right    (void*)
    //   +0x18  Key      (uint64_t)
    //   +0x20  Value    (uint64_t)
    //   +0x28  Color    (uint8_t)  0=Black, 1=Red

    inline void* RB_NodeGetParent(void* node) { return node ? *(void**)node : nullptr; }
    inline void* RB_NodeGetLeft(void* node)   { return node ? *(void**)((uint8_t*)node + 0x08) : nullptr; }
    inline void* RB_NodeGetRight(void* node)  { return node ? *(void**)((uint8_t*)node + 0x10) : nullptr; }
    inline uint64_t RB_NodeGetKey(void* node)   { return node ? *(uint64_t*)((uint8_t*)node + 0x18) : 0; }
    inline uint64_t RB_NodeGetValue(void* node) { return node ? *(uint64_t*)((uint8_t*)node + 0x20) : 0; }
    inline bool   RB_NodeIsRed(void* node)    { return node ? *((uint8_t*)node + 0x28) != 0 : false; }
}

// ============================================================================
// C++ Wrapper Class
// ============================================================================

class RawrXD_RBTree
{
public:
    // Initialize the global heap on first construction
    RawrXD_RBTree() { static bool initialized = [](){ RB_Init(); return true; }(); (void)initialized; }
    ~RawrXD_RBTree() = default;

    // Insert or update a key-value pair
    bool Insert(uint64_t key, uint64_t value) {
        return RB_Insert(key, value) != 0;
    }

    // Find a value (returns 0 if not found — use Contains() to disambiguate)
    uint64_t Get(uint64_t key) const {
        return RB_GetValue(key);
    }

    // Check if key exists
    bool Contains(uint64_t key) const {
        return RB_Find(key) != nullptr;
    }

    // Remove all entries
    void Clear() {
        RB_Clear();
    }

    // Number of entries
    uint64_t Size() const {
        return RB_GetCount();
    }

    bool Empty() const {
        return Size() == 0;
    }

    // Iteration support (in-order traversal)
    void ForEach(std::function<void(uint64_t key, uint64_t value)> callback) const {
        void* root = RB_GetRoot();
        InOrderTraversal(root, callback);
    }

    // Find by predicate (returns first match)
    bool FindIf(std::function<bool(uint64_t key, uint64_t value)> predicate,
                uint64_t& outKey, uint64_t& outValue) const {
        void* root = RB_GetRoot();
        return InOrderFind(root, predicate, outKey, outValue);
    }

private:
    static void InOrderTraversal(void* node,
        const std::function<void(uint64_t, uint64_t)>& callback) {
        if (!node) return;
        InOrderTraversal(RB_NodeGetLeft(node), callback);
        callback(RB_NodeGetKey(node), RB_NodeGetValue(node));
        InOrderTraversal(RB_NodeGetRight(node), callback);
    }

    static bool InOrderFind(void* node,
        const std::function<bool(uint64_t, uint64_t)>& predicate,
        uint64_t& outKey, uint64_t& outValue) {
        if (!node) return false;
        if (InOrderFind(RB_NodeGetLeft(node), predicate, outKey, outValue)) return true;
        if (predicate(RB_NodeGetKey(node), RB_NodeGetValue(node))) {
            outKey = RB_NodeGetKey(node);
            outValue = RB_NodeGetValue(node);
            return true;
        }
        return InOrderFind(RB_NodeGetRight(node), predicate, outKey, outValue);
    }
};

// ============================================================================
// Scoped Guard (RAII for temporary trees)
// ============================================================================

class RawrXD_RBTreeGuard
{
public:
    RawrXD_RBTreeGuard() = default;
    ~RawrXD_RBTreeGuard() { tree.Clear(); }

    RawrXD_RBTree* operator->() { return &tree; }
    RawrXD_RBTree& operator*() { return tree; }

private:
    RawrXD_RBTree tree;
};
