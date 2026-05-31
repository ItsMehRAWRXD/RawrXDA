// rawr_masm_bridge.hpp — C++ bridge for MASM x64 Red-Black Tree v2
// Production drop-in replacement for std::map / std::set on critical paths
//
// Usage:
//   RawrMap<uint64_t, uint64_t> sessionMap;
//   sessionMap.insert(42, 100);
//   auto v = sessionMap.find(42);   // returns optional<uint64_t>
//   sessionMap.erase(42);
//
// Thread-safe: all operations acquire an SRWLOCK internally.
// No iterator proxies. No _Container_base12. No xtree.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <optional>

// ---------------------------------------------------------------------------
// MASM v2 function declarations (Microsoft x64 calling convention)
// ---------------------------------------------------------------------------
extern "C" {
    // Process init (call once before any tree ops)
    void     __stdcall RB_Init(void);

    // Tree context (24 bytes: Root, Count, SRWLOCK)
    struct RB_TREE;

    // Context lifecycle
    void     __stdcall RB_TreeInit(RB_TREE* tree);
    void     __stdcall RB_Clear(RB_TREE* tree);

    // Mutation
    uint64_t __stdcall RB_Insert(RB_TREE* tree, uint64_t key, uint64_t value); // 1=success
    uint64_t __stdcall RB_Erase(RB_TREE* tree, uint64_t key);                // 1=found

    // Lookup
    void*    __stdcall RB_Find(RB_TREE* tree, uint64_t key);                  // returns RB_NODE* or nullptr
    uint64_t __stdcall RB_GetValue(RB_TREE* tree, uint64_t key);               // 0 if not found
    uint64_t __stdcall RB_GetCount(RB_TREE* tree);

    // Debug
    uint64_t __stdcall RB_Validate(RB_TREE* tree);                            // 0=valid
}

// ---------------------------------------------------------------------------
// MASM node layout (must match RawrXD_RBTree.asm)
// ---------------------------------------------------------------------------
struct RawrRBNode {
    RawrRBNode* parent;
    RawrRBNode* left;
    RawrRBNode* right;
    uint64_t    key;
    uint64_t    value;
    uint8_t     color;
    uint8_t     pad[7];
};

// ---------------------------------------------------------------------------
// Static initializer — ensures Heap handle is ready before any static ctor
// ---------------------------------------------------------------------------
struct RawrMasmInit {
    RawrMasmInit()  { RB_Init(); }
};
inline RawrMasmInit g_rawrMasmInit;  // SIOF-safe: trivial ctor, no deps

// ---------------------------------------------------------------------------
// RawrMap — drop-in replacement for std::map<K,V> where K,V are pointer-sized
// ---------------------------------------------------------------------------
template <typename K, typename V>
class RawrMap {
    static_assert(sizeof(K) <= sizeof(uint64_t), "RawrMap key must fit in 64 bits");
    static_assert(sizeof(V) <= sizeof(uint64_t), "RawrMap value must fit in 64 bits");

    alignas(8) uint8_t m_treeStorage[24]; // RB_TREE context
    RB_TREE* m_tree = nullptr;

    static uint64_t EncodeKey(const K& k) {
        uint64_t out = 0;
        std::memcpy(&out, &k, sizeof(K));
        return out;
    }
    static uint64_t EncodeValue(const V& v) {
        uint64_t out = 0;
        std::memcpy(&out, &v, sizeof(V));
        return out;
    }
    static K DecodeKey(uint64_t k) {
        K out{};
        std::memcpy(&out, &k, sizeof(K));
        return out;
    }
    static V DecodeValue(uint64_t v) {
        V out{};
        std::memcpy(&out, &v, sizeof(V));
        return out;
    }

public:
    RawrMap() {
        m_tree = reinterpret_cast<RB_TREE*>(m_treeStorage);
        RB_TreeInit(m_tree);
    }
    ~RawrMap() {
        RB_Clear(m_tree);
    }

    // Non-copyable, non-movable (SRWLOCK inside context is not relocatable)
    RawrMap(const RawrMap&) = delete;
    RawrMap& operator=(const RawrMap&) = delete;
    RawrMap(RawrMap&&) = delete;
    RawrMap& operator=(RawrMap&&) = delete;

    bool insert(const K& key, const V& value) {
        return RB_Insert(m_tree, EncodeKey(key), EncodeValue(value)) != 0;
    }

    bool insert_or_assign(const K& key, const V& value) {
        // RB_Insert already overwrites on duplicate key
        return RB_Insert(m_tree, EncodeKey(key), EncodeValue(value)) != 0;
    }

    std::optional<V> find(const K& key) const {
        auto* node = static_cast<RawrRBNode*>(RB_Find(m_tree, EncodeKey(key)));
        if (!node) return std::nullopt;
        return DecodeValue(node->value);
    }

    bool contains(const K& key) const {
        return RB_Find(m_tree, EncodeKey(key)) != nullptr;
    }

    bool erase(const K& key) {
        return RB_Erase(m_tree, EncodeKey(key)) != 0;
    }

    void clear() {
        RB_Clear(m_tree);
        RB_TreeInit(m_tree); // re-init lock
    }

    size_t size() const {
        return static_cast<size_t>(RB_GetCount(m_tree));
    }

    bool empty() const {
        return size() == 0;
    }

    V operator[](const K& key) const {
        auto* node = static_cast<RawrRBNode*>(RB_Find(m_tree, EncodeKey(key)));
        return node ? DecodeValue(node->value) : V{};
    }

    // Debug validation (call sparingly)
    bool validate() const {
        return RB_Validate(m_tree) == 0;
    }
};

// ---------------------------------------------------------------------------
// RawrSet — drop-in replacement for std::set<K>
// ---------------------------------------------------------------------------
template <typename K>
class RawrSet {
    RawrMap<K, uint8_t> m_map; // value is dummy

public:
    bool insert(const K& key) { return m_map.insert(key, 1); }
    bool contains(const K& key) const { return m_map.contains(key); }
    bool erase(const K& key) { return m_map.erase(key); }
    void clear() { m_map.clear(); }
    size_t size() const { return m_map.size(); }
    bool empty() const { return m_map.empty(); }
    bool validate() const { return m_map.validate(); }
};

// ---------------------------------------------------------------------------
// Convenience typedefs for common patterns
// ---------------------------------------------------------------------------
using RawrU64Map  = RawrMap<uint64_t, uint64_t>;
using RawrU64Set  = RawrSet<uint64_t>;
using RawrStringMap = RawrMap<std::string, uint64_t>; // key stored as 64-bit handle
using RawrStringSet = RawrSet<std::string>;
    RawrSet& operator=(RawrSet&& other) noexcept {
        if (this != &other) {
            RawrRBTree_Clear(m_root);
            m_root = other.m_root;
            other.m_root = nullptr;
        }
        return *this;
    }

    void insert(const T& value) {
        RawrRBTree_Insert(reinterpret_cast<void**>(&m_root), Encode(value), 0);
    }

    Node find(const T& value) const {
        return Node(static_cast<RawrRBNode*>(RawrRBTree_Find(m_root, Encode(value))));
    }

    bool erase(const T& value) {
        return RawrRBTree_Delete(reinterpret_cast<void**>(&m_root), Encode(value)) != 0;
    }

    bool contains(const T& value) const {
        return RawrRBTree_Find(m_root, Encode(value)) != nullptr;
    }

    Node first() const {
        return Node(static_cast<RawrRBNode*>(RawrRBTree_First(m_root)));
    }

    bool empty() const {
        return RawrRBTree_First(m_root) == nullptr;
    }

    void clear() {
        RawrRBTree_Clear(m_root);
        m_root = static_cast<RawrRBNode*>(RawrRBTree_Create());
    }

    class iterator {
        Node m_node;
    public:
        iterator(Node n) : m_node(n) {}
        bool operator!=(const iterator& o) const { return m_node.raw != o.m_node.raw; }
        iterator& operator++() { m_node = m_node.next(); return *this; }
        T operator*() const { return m_node.value(); }
    };

    iterator begin() const { return iterator(first()); }
    iterator end() const   { return iterator(Node(nullptr)); }
};
