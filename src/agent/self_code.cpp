#include "self_code.hpp"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>

bool SelfCode::editSource(const QString& filePath,
                          const QString& oldSnippet,
                          const QString& newSnippet) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot read " + filePath;
        return false;
    }
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    if (!content.contains(oldSnippet)) {
        m_lastError = "Old snippet not found in " + filePath;
        return false;
    }

    if (!replaceInFile(filePath, oldSnippet, newSnippet))
        return false;

    if (filePath.endsWith(".hpp") || filePath.endsWith(".h"))
        regenerateMOC(filePath);

    return true;
}

bool SelfCode::addInclude(const QString& hppFile,
                          const QString& includeLine) {
    if (!includeLine.startsWith("#include"))
        return false;

    QFile f(hppFile);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot read " + hppFile;
        return false;
    }
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    if (content.contains(includeLine))
        return true;

    int lastInclude = content.lastIndexOf("#include");
    if (lastInclude == -1) {
        return insertAfterIncludeGuard(hppFile, includeLine);
    }
    int insertPos = content.indexOf('\n', lastInclude);
    if (insertPos == -1)
        insertPos = content.length();
    else
        insertPos += 1;

    QString newContent = content.left(insertPos)
                         + includeLine + "\n"
                         + content.mid(insertPos);
    return replaceInFile(hppFile, content, newContent);
}

bool SelfCode::regenerateMOC(const QString& header) {
    QFile f(header);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot read " + header;
        return false;
    }
    QString h = QString::fromUtf8(f.readAll());
    f.close();
    bool needMoc = h.contains("Q_OBJECT") || h.contains("Q_PROPERTY") ||
                   h.contains("Q_SIGNALS") || h.contains("Q_SLOTS");
    if (!needMoc)
        return true;



    if (!QFile::setFileTime(header, QDateTime::currentDateTime())) {
        m_lastError = "Could not touch " + header;
        return false;
    }
    return true;
}

bool SelfCode::rebuildTarget(const QString& target,
                             const QString& config) {
    QStringList args = {"--build", "build", "--config", config, "--target", target};
    if (!runProcess("cmake", args))
        return false;

    QString exe = QDir::current().absoluteFilePath(
        QString("build/bin/%1/RawrXD-QtShell.exe").arg(config));
    QFileInfo fi(exe);
    if (!fi.exists() || fi.size() == 0) {
        m_lastError = "Binary not produced or zero size";
        return false;
    }
    return true;
}

bool SelfCode::replaceInFile(const QString& path,
                             const QString& oldText,
                             const QString& newText) {
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite | QIODevice::Text)) {
        m_lastError = "Cannot read/write " + path;
        return false;
    }
    QString content = QString::fromUtf8(f.readAll());
    int idx = content.indexOf(oldText);
    if (idx == -1) {
        m_lastError = "Old text not found (exact match required)";
        f.close();
        return false;
    }
    content.replace(idx, oldText.length(), newText);
    f.resize(0);
    f.write(content.toUtf8());
    f.close();
    return true;
}

bool SelfCode::insertAfterIncludeGuard(const QString& hpp,
                                       const QString& includeLine) {
    QFile f(hpp);
    if (!f.open(QIODevice::ReadWrite | QIODevice::Text)) {
        m_lastError = "Cannot read/write " + hpp;
        return false;
    }
    QString content = QString::fromUtf8(f.readAll());
    int pos = 0;
    if (content.startsWith("#pragma once")) {
        pos = content.indexOf('\n');
        if (pos != -1) pos += 1;
    } else if (content.contains("#ifndef")) {
        int endif = content.indexOf("#endif");
        if (endif != -1)
            pos = content.indexOf('\n', endif);
        if (pos != -1)
            pos += 1;
    }
    if (pos == -1) pos = 0;

    QString newContent = content.left(pos)
                         + includeLine + "\n"
                         + content.mid(pos);
    f.resize(0);
    f.write(newContent.toUtf8());
    f.close();
    return true;
}

bool SelfCode::runProcess(const QString& program,
                          const QStringList& args) {
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(120000)) {
        m_lastError = program + " timed out";
        return false;
    }
    if (proc.exitCode() != 0) {
        m_lastError = program + " failed: " + proc.readAllStandardError();
        return false;
    }
    return true;
}
