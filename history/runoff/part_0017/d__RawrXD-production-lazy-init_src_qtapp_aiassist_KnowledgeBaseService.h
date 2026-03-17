#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>

// KnowledgeBaseService stores Q&A pairs and provides keyword search.
class KnowledgeBaseService : public QObject {
    Q_OBJECT
public:
    struct Entry {
        QString id;
        QString question;
        QString answer;
        QStringList tags;
        int hits{0};
    };

    explicit KnowledgeBaseService(QObject* parent = nullptr);
    ~KnowledgeBaseService();

    bool addEntry(const Entry& entry);
    bool removeEntry(const QString& id);
    QList<Entry> search(const QString& query, int maxResults = 5) const;
    Entry get(const QString& id) const;

    void incrementHit(const QString& id);

private:
    double score(const QString& query, const Entry& entry) const;

    QMap<QString, Entry> m_entries;
    mutable QMutex m_mutex;
};
