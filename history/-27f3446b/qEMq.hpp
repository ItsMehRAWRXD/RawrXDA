#pragma once

#include <QtCore/QObject>
#include <QtCore/QJsonValue>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <memory>
#include <optional>

// Forward declares
namespace mem {
    class UserMemory;
    class RepoMemory;
    class CryptoVault;
    class IBackend;
    struct AuditEntry;
    class Metrics;
}

// Forward declare ggml types
struct ggml_tensor;

namespace mem {

/**
 * @class EnterpriseMemoryCatalog
 * @brief Production-grade memory system with RBAC, encryption, audit, and multi-backend support
 * 
 * Features:
 * - User preferences encrypted with AES-256-GCM
 * - Repository tensors with semantic search
 * - Role-based access control (OWNER/CONTRIBUTOR/READER)
 * - Immutable audit log (WORM)
 * - Postgres or SQLite backend
 * - Connection pooling & circuit breakers
 * - Horizontal scale-out ready
 * - Hot context reload (4k → 1M without restart)
 * - Per-request savepoints for undo
 * 
 * Integration point: Wire into AdvancedCodingAgent::AdvancedCodingAgent()
 */
class EnterpriseMemoryCatalog : public QObject {
    Q_OBJECT

public:
    /**
     * @struct Config
     * @brief Configuration options for enterprise memory
     */
    struct Config {
        int    contextTokens   = 131'072;   ///< 4 k–1 M
        bool   encryption      = true;      ///< AES-256-GCM
        bool   rbac            = true;      ///< Role-based access control
        bool   audit           = true;      ///< Immutable audit log
        QString postgresUri;                ///< "" = SQLite, "postgresql://..." = Postgres
        bool   enableClustering = false;    ///< Multi-gateway gossip
        int    connectionPoolSize = 8;      ///< For Postgres
    };

    /**
     * @brief Initialize enterprise memory catalog
     * @param cfg Configuration
     * @param parent Qt parent
     */
    explicit EnterpriseMemoryCatalog(const Config& cfg, QObject* parent = nullptr);
    ~EnterpriseMemoryCatalog();

    // ========== USER FACTS (Preferences & Learning) ==========

    /**
     * @brief Store user fact (thread-safe, transactional)
     * @param userId Current user (e.g., "alice@acme.com")
     * @param key Fact key (e.g., "user_prefers_chat")
     * @param value JSON value
     * @return True if successful
     */
    bool storeUserFact(const QString& userId,
                      const QString& key,
                      const QJsonValue& value);

    /**
     * @brief Retrieve user fact
     * @param userId User ID
     * @param key Fact key
     * @return Optional JSON value
     */
    std::optional<QJsonValue> userFact(const QString& userId,
                                      const QString& key) const;

    /**
     * @brief Record intent correction (learning)
     * @param userId User ID
     * @param pattern User phrase that was misclassified
     * @param correctIntent The correct intent
     */
    bool recordIntentCorrection(const QString& userId,
                               const QString& pattern,
                               int correctIntent);

    /**
     * @brief Get user's correction history
     * @param userId User ID
     * @return Map of pattern → correction count
     */
    std::unordered_map<std::string, int> getUserCorrectionHistory(const QString& userId) const;

    // ========== REPO TENSORS (Embeddings & Summaries) ==========

    /**
     * @brief Store repository tensor (vector embeddings, summaries, etc.)
     * @param repoId Repository ID (e.g., "acme/app")
     * @param tensorName Name of tensor (e.g., "file_embeddings", "summary_tree")
     * @param tensor ggml_tensor pointer
     * @return True if successful
     */
    bool storeRepoTensor(const QString& repoId,
                        const QString& tensorName,
                        const ggml_tensor* tensor);

    /**
     * @brief Retrieve repository tensor
     * @param repoId Repository ID
     * @param tensorName Tensor name
     * @return Tensor pointer or nullptr
     */
    ggml_tensor* repoTensor(const QString& repoId,
                           const QString& tensorName) const;

    /**
     * @brief Index repository codebase for semantic search
     * @param repoId Repository ID
     * @param workspacePath Root directory to index
     * @param filePattern Glob pattern (e.g., "*.cpp;*.h")
     * @return Number of files indexed
     */
    int indexRepository(const QString& repoId,
                       const QString& workspacePath,
                       const QString& filePattern = "*.cpp;*.h;*.ts;*.py");

    // ========== RBAC (Access Control) ==========

    /**
     * @brief Check if user can perform action on repo
     * @param userId User ID
     * @param repoId Repository ID
     * @param action Action (e.g., "read", "write", "agentic", "index", "delete")
     * @return True if action is permitted
     * 
     * Roles:
     * - OWNER: all actions
     * - CONTRIBUTOR: read, write, agentic
     * - READER: read only
     */
    bool canDo(const QString& userId,
              const QString& repoId,
              const QString& action) const;

    /**
     * @brief Get user's role in repository
     * @param userId User ID
     * @param repoId Repository ID
     * @return Role name ("OWNER", "CONTRIBUTOR", "READER")
     */
    QString userRole(const QString& userId, const QString& repoId) const;

    /**
     * @brief Grant role to user (ADMIN only)
     * @param userId User to grant to
     * @param repoId Repository
     * @param role Role name
     * @return True if successful
     */
    bool grantRole(const QString& userId,
                  const QString& repoId,
                  const QString& role);

    // ========== CONTEXT MANAGEMENT ==========

    /**
     * @brief Set context window size (hot-reload, no restart)
     * @param tokens Size in tokens [4'000, 1'000'000]
     * @return True if successful
     */
    bool setContextTokens(int tokens);

    /**
     * @brief Get current context window size
     */
    int contextTokens() const;

    /**
     * @brief Get estimated VRAM for current context
     */
    int estimatedVramMb() const;

    // ========== ADVANCED FEATURES ==========

    /**
     * @brief Create savepoint for current transaction
     * @param userId User ID
     * @param repoId Repository ID
     * @return Savepoint ID (for rollback)
     */
    QString createSavepoint(const QString& userId, const QString& repoId);

    /**
     * @brief Rollback to savepoint
     * @param userId User ID
     * @param repoId Repository ID
     * @param savepointId Savepoint ID from createSavepoint()
     * @return True if successful
     */
    bool rollbackToSavepoint(const QString& userId,
                            const QString& repoId,
                            const QString& savepointId);

    /**
     * @brief Get statistics
     * @return Map of stat name → value
     */
    std::unordered_map<std::string, int64_t> getStats() const;

    /**
     * @brief Clear all memory (destructive, audit logged)
     */
    void clearAll(const QString& userId);

signals:
    /**
     * @brief Emitted on any auditable event
     */
    void auditEvent(const mem::AuditEntry& entry);

    /**
     * @brief Emitted when context tokens change
     */
    void contextTokensChanged(int tokens);

    /**
     * @brief Emitted on RBAC denial
     */
    void accessDenied(const QString& userId, const QString& repoId, const QString& action);

private:
    class Impl;
    std::unique_ptr<Impl> d;

    /**
     * @brief Emit audit event
     */
    void audit(const QString& action,
              const QString& userId,
              const QString& repoId,
              const QJsonObject& detail = {});
};

} // namespace mem
