#pragma once
#include <QString>
#include <QJsonObject>
#include <QStringList>

class SelfCode {
public:
    // High-level helpers
    bool editSource(const QString& filePath,
                    const QString& oldSnippet,
                    const QString& newSnippet);

    bool addInclude(const QString& hppFile,
                    const QString& includeLine);

    bool regenerateMOC(const QString& header);

    bool rebuildTarget(const QString& target,
                       const QString& config = "Release");

    QString lastError() const { return m_lastError; }

private:
    QString m_lastError;

    // Low-level helpers
    bool replaceInFile(const QString& path,
                       const QString& oldText,
                       const QString& newText);
    bool insertAfterIncludeGuard(const QString& hpp,
                                 const QString& includeLine);
    bool runProcess(const QString& program,
                    const QStringList& args);
};
