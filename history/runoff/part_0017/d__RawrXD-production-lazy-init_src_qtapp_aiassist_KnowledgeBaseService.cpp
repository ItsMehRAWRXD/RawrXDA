#include "KnowledgeBaseService.h"

#include <QtMath>

KnowledgeBaseService::KnowledgeBaseService(QObject* parent) : QObject(parent) {}
KnowledgeBaseService::~KnowledgeBaseService() {}

bool KnowledgeBaseService::addEntry(const Entry& entry) {
    QMutexLocker locker(&m_mutex);
    if (entry.id.isEmpty() || m_entries.contains(entry.id)) return false;
    m_entries[entry.id] = entry;
    return true;
}

bool KnowledgeBaseService::removeEntry(const QString& id) {
    QMutexLocker locker(&m_mutex);
    return m_entries.remove(id) > 0;
}

KnowledgeBaseService::Entry KnowledgeBaseService::get(const QString& id) const {
    QMutexLocker locker(&m_mutex);
    return m_entries.value(id);
}

double KnowledgeBaseService::score(const QString& query, const Entry& entry) const {
    QString q = query.toLower();
    int hits = 0;
    if (entry.question.toLower().contains(q)) hits += 2;
    if (entry.answer.toLower().contains(q)) hits += 1;
    for (const QString& tag : entry.tags) if (tag.toLower().contains(q)) hits += 1;
    return hits + entry.hits * 0.1;
}

QList<KnowledgeBaseService::Entry> KnowledgeBaseService::search(const QString& query, int maxResults) const {
    QMutexLocker locker(&m_mutex);
    QList<QPair<double, Entry>> scored;
    for (const auto& e : m_entries) {
        double s = score(query, e);
        if (s > 0.0) scored.append(qMakePair(s, e));
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QList<Entry> out;
    for (int i = 0; i < scored.size() && i < maxResults; ++i) out.append(scored[i].second);
    return out;
}

void KnowledgeBaseService::incrementHit(const QString& id) {
    QMutexLocker locker(&m_mutex);
    if (m_entries.contains(id)) m_entries[id].hits++;
}
