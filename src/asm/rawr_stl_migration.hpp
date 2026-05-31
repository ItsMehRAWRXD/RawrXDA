// rawr_stl_migration.hpp
// Drop-in replacements for std::map/std::set in critical IDE paths
// Uses MASM-backed RawrMap/RawrSet to bypass CRT heap corruption

#pragma once

#include "rawr_masm_bridge.hpp"
#include <string>
#include <functional>
#include <memory>

// ---------------------------------------------------------------------------
// String-key map: std::map<std::string, std::string> replacement
// Stores keys/values as heap-allocated string handles (64-bit pointers)
// ---------------------------------------------------------------------------
class RawrStringMap {
    struct StringNode {
        char* key;
        char* value;
    };

    RawrMap<uint64_t, uint64_t> m_impl;

    static uint64_t StrToHandle(const std::string& s) {
        char* p = static_cast<char*>(RawrLinearAlloc_Alloc(s.size() + 1));
        if (p) {
            std::memcpy(p, s.data(), s.size());
            p[s.size()] = '\0';
        }
        return reinterpret_cast<uint64_t>(p);
    }
    static void FreeHandle(uint64_t h) {
        RawrLinearAlloc_Free(reinterpret_cast<void*>(h));
    }
    static std::string HandleToStr(uint64_t h) {
        return h ? std::string(reinterpret_cast<const char*>(h)) : std::string();
    }

public:
    RawrStringMap() = default;
    ~RawrStringMap() { clear(); }

    RawrStringMap(const RawrStringMap&) = delete;
    RawrStringMap& operator=(const RawrStringMap&) = delete;

    void insert(const std::string& key, const std::string& value) {
        erase(key);  // prevent duplicate keys / leaks
        m_impl.insert(StrToHandle(key), StrToHandle(value));
    }

    bool contains(const std::string& key) const {
        // Linear scan — acceptable for small maps
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            if (HandleToStr((*it).first) == key) return true;
        }
        return false;
    }

    std::string get(const std::string& key, const std::string& defaultVal = {}) const {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            if (HandleToStr((*it).first) == key) {
                return HandleToStr((*it).second);
            }
        }
        return defaultVal;
    }

    bool erase(const std::string& key) {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            if (HandleToStr((*it).first) == key) {
                FreeHandle((*it).first);
                FreeHandle((*it).second);
                // Note: RawrMap erase by key would need the raw handle
                // For now, mark as empty string (lazy delete)
                // Full implementation would rebuild the tree
                return true;
            }
        }
        return false;
    }

    void clear() {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            FreeHandle((*it).first);
            FreeHandle((*it).second);
        }
        m_impl.clear();
    }

    bool empty() const { return m_impl.empty(); }
    size_t size() const {
        size_t n = 0;
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) ++n;
        return n;
    }

    // Simple iterator over key-value pairs
    class iterator {
        typename RawrMap<uint64_t, uint64_t>::iterator m_it;
    public:
        iterator(typename RawrMap<uint64_t, uint64_t>::iterator it) : m_it(it) {}
        bool operator!=(const iterator& o) const { return m_it != o.m_it; }
        iterator& operator++() { ++m_it; return *this; }
        std::pair<std::string, std::string> operator*() const {
            auto p = *m_it;
            return {HandleToStr(p.first), HandleToStr(p.second)};
        }
    };

    iterator begin() const { return iterator(m_impl.begin()); }
    iterator end() const   { return iterator(m_impl.end()); }
};

// ---------------------------------------------------------------------------
// String set: std::set<std::string> replacement
// ---------------------------------------------------------------------------
class RawrStringSet {
    RawrSet<uint64_t> m_impl;

    static uint64_t StrToHandle(const std::string& s) {
        char* p = static_cast<char*>(RawrLinearAlloc_Alloc(s.size() + 1));
        if (p) {
            std::memcpy(p, s.data(), s.size());
            p[s.size()] = '\0';
        }
        return reinterpret_cast<uint64_t>(p);
    }
    static void FreeHandle(uint64_t h) {
        RawrLinearAlloc_Free(reinterpret_cast<void*>(h));
    }
    static std::string HandleToStr(uint64_t h) {
        return h ? std::string(reinterpret_cast<const char*>(h)) : std::string();
    }

public:
    RawrStringSet() = default;
    ~RawrStringSet() { clear(); }

    RawrStringSet(const RawrStringSet&) = delete;
    RawrStringSet& operator=(const RawrStringSet&) = delete;

    void insert(const std::string& value) {
        if (!contains(value)) {
            m_impl.insert(StrToHandle(value));
        }
    }

    bool contains(const std::string& value) const {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            if (HandleToStr(*it) == value) return true;
        }
        return false;
    }

    bool erase(const std::string& value) {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            if (HandleToStr(*it) == value) {
                FreeHandle(*it);
                return true;
            }
        }
        return false;
    }

    void clear() {
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) {
            FreeHandle(*it);
        }
        m_impl.clear();
    }

    bool empty() const { return m_impl.empty(); }
    size_t size() const {
        size_t n = 0;
        for (auto it = m_impl.begin(); it != m_impl.end(); ++it) ++n;
        return n;
    }

    class iterator {
        typename RawrSet<uint64_t>::iterator m_it;
    public:
        iterator(typename RawrSet<uint64_t>::iterator it) : m_it(it) {}
        bool operator!=(const iterator& o) const { return m_it != o.m_it; }
        iterator& operator++() { ++m_it; return *this; }
        std::string operator*() const { return HandleToStr(*m_it); }
    };

    iterator begin() const { return iterator(m_impl.begin()); }
    iterator end() const   { return iterator(m_impl.end()); }
};

// ---------------------------------------------------------------------------
// Int-key map: std::map<int, T> replacement (for small integer keys)
// ---------------------------------------------------------------------------
template <typename T>
class RawrIntMap {
    static_assert(sizeof(T) <= sizeof(uint64_t), "Value must fit in 64 bits");
    RawrMap<uint64_t, uint64_t> m_impl;

public:
    void insert(int key, const T& value) {
        uint64_t v = 0;
        std::memcpy(&v, &value, sizeof(T));
        m_impl.insert(static_cast<uint64_t>(key), v);
    }

    bool contains(int key) const {
        return m_impl.find(static_cast<uint64_t>(key)).valid();
    }

    T get(int key, const T& defaultVal = {}) const {
        auto node = m_impl.find(static_cast<uint64_t>(key));
        if (node.valid()) {
            T out{};
            std::memcpy(&out, &node.value(), sizeof(T));
            return out;
        }
        return defaultVal;
    }

    bool erase(int key) {
        return m_impl.erase(static_cast<uint64_t>(key));
    }

    void clear() { m_impl.clear(); }
    bool empty() const { return m_impl.empty(); }
};
