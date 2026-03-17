#pragma once

#include <QString>
#include <QDateTime>

namespace mem {

/**
 * @struct AuditEntry
 * @brief Immutable audit log entry (Write-Once Read-Many)
 * 
 * Stored in append-only table with SHA-256 chain for tamper detection
 */
struct AuditEntry {
    QString action;           ///< Action performed (e.g., "storeUserFact", "recordCorrection")
    QString userId;           ///< User who performed action
    QString repoId;           ///< Repository affected (if applicable)
    QString detail;           ///< JSON detail blob
    QDateTime timestamp;      ///< When action occurred
    QString hash;             ///< SHA-256 hash of this entry + previous hash
    QString previousHash;     ///< Hash of previous entry (chain)
};

} // namespace mem
