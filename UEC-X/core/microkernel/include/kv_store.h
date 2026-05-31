// UEC-X Microkernel - KV Store
// Key-value state storage for extensions

#pragma once

#include "uec_core.h"
#include <unordered_map>
#include <variant>
#include <chrono>

namespace uec {

// =============================================================================
// KV Value Types
// =============================================================================

using KVValue = std::variant<
    std::monostate,    // null
    bool,              // boolean
    int64_t,           // integer
    double,            // float
    std::string,       // string
    std::vector<uint8_t>  // binary
>;

enum class KVType : uint32_t {
    Null = 0,
    Boolean = 1,
    Integer = 2,
    Float = 3,
    String = 4,
    Binary = 5
};

// =============================================================================
// KV Entry
// =============================================================================

struct KVEntry {
    std::string key;
    KVValue value;
    ExtensionId owner;
    Timestamp createdAt;
    Timestamp modifiedAt;
    std::chrono::seconds ttl{0};  // 0 = no expiration
    std::atomic<uint64_t> accessCount{0};
    
    bool IsExpired() const;
    void Touch();
};

// =============================================================================
// KV Store
// =============================================================================

class UEC_API KVStore {
public:
    KVStore();
    ~KVStore();

    // Non-copyable, non-movable
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) = delete;
    KVStore& operator=(KVStore&&) = delete;

    // Lifecycle
    Result<void> Initialize(size_t maxEntries = 100000);
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Basic operations
    Result<void> Set(const std::string& key, const KVValue& value, 
                      ExtensionId owner = 0,
                      std::chrono::seconds ttl = std::chrono::seconds(0));
    Result<KVValue> Get(const std::string& key) const;
    Result<void> Delete(const std::string& key);
    bool Exists(const std::string& key) const;
    
    // Type-safe getters
    Result<bool> GetBool(const std::string& key) const;
    Result<int64_t> GetInt(const std::string& key) const;
    Result<double> GetFloat(const std::string& key) const;
    Result<std::string> GetString(const std::string& key) const;
    Result<std::vector<uint8_t>> GetBinary(const std::string& key) const;

    // Atomic operations
    Result<int64_t> Increment(const std::string& key, int64_t delta = 1);
    Result<int64_t> Decrement(const std::string& key, int64_t delta = 1);
    Result<bool> CompareAndSwap(const std::string& key, const KVValue& expected, 
                                   const KVValue& desired);

    // Batch operations
    Result<void> SetBatch(const std::vector<std::pair<std::string, KVValue>>& items,
                           ExtensionId owner = 0);
    Result<std::vector<KVValue>> GetBatch(const std::vector<std::string>& keys) const;
    Result<void> DeleteBatch(const std::vector<std::string>& keys);

    // Namespaced operations
    Result<void> SetNamespaced(const std::string& ns, const std::string& key, 
                                const KVValue& value, ExtensionId owner = 0);
    Result<KVValue> GetNamespaced(const std::string& ns, const std::string& key) const;
    Result<void> DeleteNamespace(const std::string& ns);
    std::vector<std::string> GetNamespaceKeys(const std::string& ns) const;

    // Query
    std::vector<std::string> GetKeys(const std::string& pattern = "*") const;
    std::vector<std::string> GetKeysByOwner(ExtensionId owner) const;
    size_t GetEntryCount() const;
    size_t GetMemoryUsage() const;

    // Expiration
    Result<void> SetTTL(const std::string& key, std::chrono::seconds ttl);
    Result<void> Persist(const std::string& key);  // Remove TTL
    Result<std::chrono::seconds> GetTTL(const std::string& key) const;
    Result<void> Expire(const std::string& key);  // Expire immediately
    
    // Cleanup
    size_t ExpireEntries();  // Remove expired entries, return count removed
    void StartExpirationThread(std::chrono::seconds interval = std::chrono::seconds(60));
    void StopExpirationThread();

    // Persistence
    Result<void> SaveToFile(const std::string& path) const;
    Result<void> LoadFromFile(const std::string& path);

    // Events
    using KVChangeHandler = std::function<void(const std::string& key, 
                                                const KVValue& oldValue,
                                                const KVValue& newValue)>;
    Result<uint64_t> Subscribe(const std::string& keyPattern, KVChangeHandler handler);
    Result<void> Unsubscribe(uint64_t subscriptionId);

private:
    struct Subscription {
        uint64_t id;
        std::string pattern;
        KVChangeHandler handler;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<KVEntry>> m_entries;
    std::unordered_map<uint64_t, Subscription> m_subscriptions;
    std::atomic<uint64_t> m_nextSubscriptionId{1};
    std::atomic<bool> m_initialized{false};
    size_t m_maxEntries = 100000;
    
    std::thread m_expirationThread;
    std::atomic<bool> m_expirationRunning{false};
    
    void ExpirationLoop(std::chrono::seconds interval);
    void NotifySubscribers(const std::string& key, const KVValue& oldVal, 
                           const KVValue& newVal);
    bool MatchPattern(const std::string& key, const std::string& pattern) const;
};

} // namespace uec
