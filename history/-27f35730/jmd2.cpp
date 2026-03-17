#include "EnterpriseMemoryCatalog.hpp"
#include "Backend.hpp"
#include "CryptoVault.hpp"
#include "AuditLog.hpp"
#include "Metrics.hpp"
#include <QMutexLocker>
#include <QJsonDocument>
#include <QUuid>

namespace mem {

class EnterpriseMemoryCatalog::Impl {
public:
    Config cfg;
    BackendPtr backend;
    mutable QMutex mtx;
    QHash<QString, QString> roleCache;  // {user@repo} → role

    explicit Impl(const Config& c)
        : cfg(c),
          backend(makeBackend(c.postgresUri))
    {
        if (backend) {
            backend->migrate();
        }
        Metrics::increment("memory_catalog_start_total");
    }
};

EnterpriseMemoryCatalog::EnterpriseMemoryCatalog(const Config& cfg, QObject* parent)
    : QObject(parent),
      d(std::make_unique<Impl>(cfg)) {}

EnterpriseMemoryCatalog::~EnterpriseMemoryCatalog() = default;

bool EnterpriseMemoryCatalog::storeUserFact(const QString& userId,
                                           const QString& key,
                                           const QJsonValue& value) {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return false;

    QJsonDocument doc(value.toObject());
    QByteArray plain = doc.toJson(QJsonDocument::Compact);
    
    auto blob = d->cfg.encryption ? 
                CryptoVault::encrypt(plain, userId) :
                EncryptedBlob{plain, {}, {}};

    int64_t start = Metrics::startTimer();
    bool ok = d->backend->storeUserFact(userId, key, blob);
    Metrics::recordLatency("memory_store_user_fact_ms", Metrics::elapsedMs(start));

    if (ok && d->cfg.audit) {
        audit("storeUserFact", userId, "", {{"key", key}});
    }

    Metrics::increment("memory_user_facts_total");
    return ok;
}

std::optional<QJsonValue> EnterpriseMemoryCatalog::userFact(const QString& userId,
                                                           const QString& key) const {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return std::nullopt;

    int64_t start = Metrics::startTimer();
    auto blob = d->backend->userFact(userId, key);
    Metrics::recordLatency("memory_get_user_fact_ms", Metrics::elapsedMs(start));

    if (!blob)
        return std::nullopt;

    QByteArray plain = d->cfg.encryption ?
                       CryptoVault::decrypt(*blob, userId) :
                       blob->cipher;

    if (plain.isEmpty())
        return std::nullopt;

    QJsonDocument doc = QJsonDocument::fromJson(plain);
    return doc.object();
}

bool EnterpriseMemoryCatalog::recordIntentCorrection(const QString& userId,
                                                    const QString& pattern,
                                                    int correctIntent) {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return false;

    bool ok = d->backend->recordCorrection(userId, pattern, correctIntent);
    if (ok && d->cfg.audit) {
        QJsonObject detail;
        detail["pattern"] = pattern;
        detail["intent"] = correctIntent;
        audit("recordIntentCorrection", userId, "", detail);
    }
    Metrics::increment("memory_intent_corrections_total");
    return ok;
}

std::unordered_map<std::string, int> EnterpriseMemoryCatalog::getUserCorrectionHistory(
    const QString& userId) const {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return {};

    return d->backend->correctionHistory(userId);
}

bool EnterpriseMemoryCatalog::storeRepoTensor(const QString& repoId,
                                            const QString& tensorName,
                                            const ggml_tensor* tensor) {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected() || !tensor)
        return false;

    // TODO: Serialize ggml_tensor to QByteArray
    // For now, just store placeholder
    QByteArray plain;  // Actual serialization would go here
    
    auto blob = d->cfg.encryption ?
                CryptoVault::encrypt(plain, repoId) :
                EncryptedBlob{plain, {}, {}};

    int64_t start = Metrics::startTimer();
    bool ok = d->backend->storeRepoTensor(repoId, tensorName, blob);
    Metrics::recordLatency("memory_store_repo_tensor_ms", Metrics::elapsedMs(start));

    if (ok && d->cfg.audit) {
        audit("storeRepoTensor", "", repoId, {{"tensorName", tensorName}});
    }

    Metrics::increment("memory_repo_tensors_total");
    Metrics::recordBytes("memory_repo_tensor_bytes", plain.size());
    return ok;
}

ggml_tensor* EnterpriseMemoryCatalog::repoTensor(const QString& repoId,
                                               const QString& tensorName) const {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return nullptr;

    auto blob = d->backend->repoTensor(repoId, tensorName);
    if (!blob)
        return nullptr;

    QByteArray plain = d->cfg.encryption ?
                       CryptoVault::decrypt(*blob, repoId) :
                       blob->cipher;

    if (plain.isEmpty())
        return nullptr;

    // TODO: Deserialize ggml_tensor from QByteArray
    return nullptr;
}

int EnterpriseMemoryCatalog::indexRepository(const QString& repoId,
                                            const QString& workspacePath,
                                            const QString& filePattern) {
    QMutexLocker l(&d->mtx);
    
    if (!canDo("", repoId, "index")) {
        emit accessDenied("", repoId, "index");
        return 0;
    }

    // TODO: Walk workspace, generate embeddings, index
    Metrics::increment("memory_repository_indexed_total");
    return 0;
}

bool EnterpriseMemoryCatalog::canDo(const QString& userId,
                                   const QString& repoId,
                                   const QString& action) const {
    QMutexLocker l(&d->mtx);
    if (!d->cfg.rbac) return true;

    QString cacheKey = userId + "@" + repoId;
    if (!d->roleCache.contains(cacheKey)) {
        QString r = d->backend->role(userId, repoId);
        const_cast<QHash<QString, QString>&>(d->roleCache).insert(cacheKey, r);
    }

    const QString& role = d->roleCache[cacheKey];
    static const QHash<QString, QSet<QString>> matrix{
        {QStringLiteral("OWNER"), {"read", "write", "agentic", "index", "delete"}},
        {QStringLiteral("CONTRIBUTOR"), {"read", "write", "agentic"}},
        {QStringLiteral("READER"), {"read"}}
    };

    bool ok = matrix.value(role).contains(action);
    if (!ok) {
        Metrics::increment("memory_rbac_denials_total");
        emit accessDenied(userId, repoId, action);
    }
    return ok;
}

QString EnterpriseMemoryCatalog::userRole(const QString& userId, const QString& repoId) const {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return QStringLiteral("READER");

    return d->backend->role(userId, repoId);
}

bool EnterpriseMemoryCatalog::grantRole(const QString& userId,
                                       const QString& repoId,
                                       const QString& role) {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return false;

    // Clear cache
    QString cacheKey = userId + "@" + repoId;
    d->roleCache.remove(cacheKey);

    bool ok = d->backend->setRole(userId, repoId, role);
    if (ok && d->cfg.audit) {
        QJsonObject detail;
        detail["newRole"] = role;
        audit("grantRole", userId, repoId, detail);
    }
    Metrics::increment("memory_role_grants_total");
    return ok;
}

bool EnterpriseMemoryCatalog::setContextTokens(int tokens) {
    int clamped = std::clamp(tokens, 4'000, 1'000'000);
    
    if (clamped == d->cfg.contextTokens)
        return true;  // No change

    // TODO: Hot-reload context (would involve llama_context manipulation)
    d->cfg.contextTokens = clamped;
    
    Metrics::record("memory_context_tokens", clamped);
    emit contextTokensChanged(clamped);
    return true;
}

int EnterpriseMemoryCatalog::contextTokens() const {
    return d->cfg.contextTokens;
}

int EnterpriseMemoryCatalog::estimatedVramMb() const {
    // Rough estimate: ~1 MB per 1k context @ Q8_0
    return d->cfg.contextTokens / 1'000;
}

QString EnterpriseMemoryCatalog::createSavepoint(const QString& userId, const QString& repoId) {
    QString id = QUuid::createUuid().toString();
    // TODO: Create DB savepoint
    return id;
}

bool EnterpriseMemoryCatalog::rollbackToSavepoint(const QString& userId,
                                                 const QString& repoId,
                                                 const QString& savepointId) {
    // TODO: Rollback DB transaction
    if (d->cfg.audit) {
        audit("rollbackToSavepoint", userId, repoId, {{"savepointId", savepointId}});
    }
    return true;
}

std::unordered_map<std::string, int64_t> EnterpriseMemoryCatalog::getStats() const {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return {};

    auto stats = d->backend->getStats();
    stats["context_tokens"] = d->cfg.contextTokens;
    stats["estimated_vram_mb"] = estimatedVramMb();
    return stats;
}

void EnterpriseMemoryCatalog::clearAll(const QString& userId) {
    QMutexLocker l(&d->mtx);
    if (!d->backend || !d->backend->isConnected())
        return;

    d->backend->clear();
    d->roleCache.clear();

    if (d->cfg.audit) {
        audit("clearAll", userId, "", {});
    }
    Metrics::increment("memory_clear_all_total");
}

void EnterpriseMemoryCatalog::audit(const QString& action,
                                   const QString& userId,
                                   const QString& repoId,
                                   const QJsonObject& detail) {
    if (!d->cfg.audit) return;

    AuditEntry entry{
        .action = action,
        .userId = userId,
        .repoId = repoId,
        .detail = QString::fromUtf8(QJsonDocument(detail).toJson(QJsonDocument::Compact)),
        .timestamp = QDateTime::currentDateTimeUtc()
    };

    emit auditEvent(entry);
}

} // namespace mem
