#include "ComplianceAuditService.h"

ComplianceAuditService::ComplianceAuditService(QObject* parent) : QObject(parent) {}
ComplianceAuditService::~ComplianceAuditService() {}

ComplianceAuditService::PolicyResult ComplianceAuditService::checkEncryption(bool enabled, bool keyRotationEnabled, int rotationDays) const {
    PolicyResult r;
    r.policyId = "encryption";
    r.timestamp = QDateTime::currentDateTimeUtc();
    r.passed = enabled && keyRotationEnabled && rotationDays <= 30;
    r.details = r.passed ? "Encryption OK" : "Encryption or key rotation not compliant";
    return r;
}

ComplianceAuditService::PolicyResult ComplianceAuditService::checkLogging(bool auditEnabled, int retentionDays) const {
    PolicyResult r;
    r.policyId = "logging";
    r.timestamp = QDateTime::currentDateTimeUtc();
    r.passed = auditEnabled && retentionDays >= 30;
    r.details = r.passed ? "Audit logging OK" : "Audit logging disabled or retention too low";
    return r;
}

ComplianceAuditService::PolicyResult ComplianceAuditService::checkBackups(int hoursSinceLastBackup) const {
    PolicyResult r;
    r.policyId = "backups";
    r.timestamp = QDateTime::currentDateTimeUtc();
    r.passed = hoursSinceLastBackup <= 72;
    r.details = r.passed ? "Backups fresh" : "Backups stale";
    return r;
}

void ComplianceAuditService::record(const PolicyResult& r) {
    QMutexLocker locker(&m_mutex);
    m_records[r.policyId].append(r);
    if (m_records[r.policyId].size() > 500) m_records[r.policyId].removeFirst();
    emit policyRecorded(r);
}

QList<ComplianceAuditService::PolicyResult> ComplianceAuditService::history(const QString& policyId, int limit) const {
    QMutexLocker locker(&m_mutex);
    if (policyId.isEmpty()) {
        QList<PolicyResult> all;
        for (const auto& list : m_records) all.append(list);
        if (limit > 0 && all.size() > limit) return all.mid(all.size() - limit);
        return all;
    }
    QList<PolicyResult> res = m_records.value(policyId);
    if (limit > 0 && res.size() > limit) return res.mid(res.size() - limit);
    return res;
}
