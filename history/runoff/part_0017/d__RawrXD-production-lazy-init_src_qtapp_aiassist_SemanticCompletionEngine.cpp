#include "SemanticCompletionEngine.h"

#include <QRegularExpression>
#include <QtMath>

SemanticCompletionEngine::SemanticCompletionEngine(QObject* parent) : QObject(parent) {}
SemanticCompletionEngine::~SemanticCompletionEngine() {}

QStringList SemanticCompletionEngine::tokenize(const QString& text) const {
    QString normalized = text;
    QRegularExpression re("[\\W_]+");
    QStringList parts = normalized.split(re, Qt::SkipEmptyParts);
    return parts;
}

void SemanticCompletionEngine::updateModel(const QStringList& tokens) {
    for (int i = 0; i < tokens.size(); ++i) {
        QString t = tokens[i].toLower();
        m_unigram[t]++;
        if (i + 1 < tokens.size()) m_bigram[t + " " + tokens[i + 1].toLower()]++;
        if (i + 2 < tokens.size()) m_trigram[t + " " + tokens[i + 1].toLower() + " " + tokens[i + 2].toLower()]++;
    }
}

void SemanticCompletionEngine::ingestFile(const QString& filePath, const QString& content) {
    Q_UNUSED(filePath)
    QStringList tokens = tokenize(content);
    QMutexLocker locker(&m_mutex);
    updateModel(tokens);
}

void SemanticCompletionEngine::clear() {
    QMutexLocker locker(&m_mutex);
    m_unigram.clear();
    m_bigram.clear();
    m_trigram.clear();
}

double SemanticCompletionEngine::ngramScore(const QStringList& context, const QString& candidate) const {
    QString cand = candidate.toLower();
    QString c1 = context.size() >= 1 ? context.last().toLower() : QString();
    QString c2 = context.size() >= 2 ? context[context.size() - 2].toLower() + " " + c1 : QString();
    double uni = m_unigram.value(cand);
    double bi = c1.isEmpty() ? 0.0 : m_bigram.value(c1 + " " + cand);
    double tri = c2.isEmpty() ? 0.0 : m_trigram.value(c2 + " " + cand);
    return uni * 0.2 + bi * 0.5 + tri * 0.8;
}

QList<SemanticCompletionEngine::Suggestion> SemanticCompletionEngine::suggest(const QString& filePath, const QString& linePrefix, int maxSuggestions) {
    Q_UNUSED(filePath)
    QStringList tokens = tokenize(linePrefix);
    QMutexLocker locker(&m_mutex);
    QList<Suggestion> out;

    // consider next token candidates from unigrams
    for (auto it = m_unigram.begin(); it != m_unigram.end(); ++it) {
        double s = ngramScore(tokens, it.key());
        if (s > 0) {
            Suggestion sug{it.key(), s, linePrefix.size()};
            out.append(sug);
        }
    }

    std::sort(out.begin(), out.end(), [](const Suggestion& a, const Suggestion& b) {
        return a.score > b.score;
    });
    if (out.size() > maxSuggestions) out = out.mid(0, maxSuggestions);

    emit suggestionReady(filePath, out);
    return out;
}
