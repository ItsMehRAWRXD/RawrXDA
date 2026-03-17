#pragma once

#include <QString>
#include <QJsonValue>
#include <QJsonObject>
#include <memory>
#include <optional>
#include <functional>
#include "CryptoVault.hpp"

namespace mem {

/**
 * @class IBackend
 * @brief Abstract backend interface for user facts and repo tensors
 * 
 * Implementations:
 * - SqliteBackend: Local SQLite database
 * - PostgresBackend: Remote Postgres with connection pooling
 */
class IBackend {
public:
    virtual ~IBackend() = default;

    /**
     * @brief Initialize database schema
     */
    virtual void migrate() = 0;

    /**
     * @brief Store user fact (encrypted)
     */
    virtual bool storeUserFact(const QString& userId,
                              const QString& key,
                              const EncryptedBlob& blob) = 0;

    /**
     * @brief Retrieve user fact
     */
    virtual std::optional<EncryptedBlob> userFact(const QString& userId,
                                                 const QString& key) const = 0;

    /**
     * @brief Store repo tensor
     */
    virtual bool storeRepoTensor(const QString& repoId,
                                const QString& tensorName,
                                const EncryptedBlob& blob) = 0;

    /**
     * @brief Retrieve repo tensor
     */
    virtual std::optional<EncryptedBlob> repoTensor(const QString& repoId,
                                                   const QString& tensorName) const = 0;

    /**
     * @brief Get user role in repository
     * @return Role name ("OWNER", "CONTRIBUTOR", "READER")
     */
    virtual QString role(const QString& userId, const QString& repoId) const = 0;

    /**
     * @brief Set user role
     */
    virtual bool setRole(const QString& userId,
                        const QString& repoId,
                        const QString& role) = 0;

    /**
     * @brief Record intent correction
     */
    virtual bool recordCorrection(const QString& userId,
                                 const QString& pattern,
                                 int intent) = 0;

    /**
     * @brief Get correction history for user
     */
    virtual std::unordered_map<std::string, int> correctionHistory(const QString& userId) const = 0;

    /**
     * @brief Get database statistics
     */
    virtual std::unordered_map<std::string, int64_t> getStats() const = 0;

    /**
     * @brief Clear all data
     */
    virtual void clear() = 0;

    /**
     * @brief Check if backend is connected
     */
    virtual bool isConnected() const = 0;
};

using BackendPtr = std::unique_ptr<IBackend>;

/**
 * @brief Factory function to create appropriate backend
 * @param postgresUri Empty = SQLite, "postgresql://..." = Postgres
 * @return Backend instance
 */
BackendPtr makeBackend(const QString& postgresUri);

} // namespace mem
