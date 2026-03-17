#pragma once

#include <QString>
#include <QJsonValue>
#include <QJsonObject>
#include <QMutex>
#include <sqlite3.h>
#include <memory>
#include <optional>
#include <unordered_map>

namespace mem {

/**
 * @class UserMemory
 * @brief Thread-safe SQLite wrapper for persistent user preferences and learning
 * 
 * Database schema:
 * - facts: User preferences and learned behaviors
 * - corrections: Intent correction history (learning from user feedback)
 * - settings: User-specific configuration
 */
class UserMemory {
public:
    /**
     * @brief Initialize user memory with SQLite database
     * @param dbPath Path to user.db file (created if doesn't exist)
     */
    explicit UserMemory(const QString& dbPath);
    ~UserMemory();

    /**
     * @brief Store a user fact (preference, setting, learned behavior)
     * @param key Unique identifier (e.g., "user_prefers_chat", "coding_style")
     * @param value JSON value to store
     * @return True if successful
     */
    bool storeFact(const QString& key, const QJsonValue& value);

    /**
     * @brief Retrieve a user fact
     * @param key Unique identifier
     * @return Optional JSON value if found
     */
    std::optional<QJsonValue> fact(const QString& key) const;

    /**
     * @brief Store typed user preference
     * @param key Preference key
     * @param value Preference value
     */
    template<typename T>
    bool storePreference(const QString& key, const T& value) {
        QJsonObject obj;
        if constexpr (std::is_same_v<T, bool>) {
            obj[key] = value;
        } else if constexpr (std::is_same_v<T, int>) {
            obj[key] = value;
        } else if constexpr (std::is_same_v<T, QString>) {
            obj[key] = value;
        }
        return storeFact(key, obj);
    }

    /**
     * @brief Retrieve typed user preference
     */
    template<typename T>
    std::optional<T> preference(const QString& key) const {
        auto val = fact(key);
        if (!val) return std::nullopt;
        
        if constexpr (std::is_same_v<T, bool>) {
            return val->toBool();
        } else if constexpr (std::is_same_v<T, int>) {
            return val->toInt();
        } else if constexpr (std::is_same_v<T, QString>) {
            return val->toString();
        }
        return std::nullopt;
    }

    /**
     * @brief Record user correction (when user rejected detected intent)
     * @param pattern The recognized pattern (e.g., "explain this code")
     * @param correctIntent The intent user indicated was correct
     * @return True if successful
     */
    bool recordCorrection(const QString& pattern, int correctIntent);

    /**
     * @brief Get correction count for a pattern
     * @param pattern Pattern to lookup
     * @return Number of times user corrected this pattern
     */
    int getCorrectionCount(const QString& pattern) const;

    /**
     * @brief Get the corrected intent for a pattern (if exists)
     * @param pattern Pattern to lookup
     * @return Optional corrected intent enum value
     */
    std::optional<int> getCorrectedIntent(const QString& pattern) const;

    /**
     * @brief Clear all user memory (destructive)
     */
    void clear();

    /**
     * @brief Get database statistics
     * @return Map of stat name → value
     */
    std::unordered_map<std::string, int> getStats() const;

    /**
     * @brief Verify database integrity
     * @return True if database is valid
     */
    bool isValid() const;

private:
    mutable QMutex mtx;
    sqlite3* db = nullptr;

    /**
     * @brief Initialize database schema
     */
    void initializeSchema();

    /**
     * @brief Execute SQL statement
     */
    bool execute(const char* sql, const std::function<void(sqlite3_stmt*)>& callback = {});

    /**
     * @brief Execute query and return results
     */
    std::vector<QJsonObject> query(const char* sql);
};

} // namespace mem
