#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>
#include <QVector>

// SemanticCompletionEngine provides lightweight statistical completions using n-gram token windows and recent history.
class SemanticCompletionEngine : public QObject {
    Q_OBJECT
public:
    struct Suggestion {
        QString text;
        double score{0.0};
        int startColumn{0};
    };

    explicit SemanticCompletionEngine(QObject* parent = nullptr);
    ~SemanticCompletionEngine();

    // Feed code buffers to build token statistics
    void ingestFile(const QString& filePath, const QString& content);
    void clear();

    // Generate suggestions for a line prefix
    QList<Suggestion> suggest(const QString& filePath, const QString& linePrefix, int maxSuggestions = 5);

signals:
    void suggestionReady(const QString& filePath, const QList<Suggestion>& results);

private:
    QStringList tokenize(const QString& text) const;
    void updateModel(const QStringList& tokens);
    double ngramScore(const QStringList& context, const QString& candidate) const;

    QMap<QString, int> m_unigram;
    QMap<QString, int> m_bigram;
    QMap<QString, int> m_trigram;
    QMutex m_mutex;
};
