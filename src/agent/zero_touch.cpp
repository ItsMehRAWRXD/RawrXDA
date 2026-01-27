#include "zero_touch.hpp"
#include "auto_bootstrap.hpp"

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QTimer>
#include <QDebug>

ZeroTouch::ZeroTouch(QObject* parent)
    : QObject(parent) {}

void ZeroTouch::installAll() {
    installFileWatcher();
    installGitHook();
    installVoiceTrigger();
    qDebug() << "Zero-touch triggers installed";
}

void ZeroTouch::installFileWatcher() {
    QString srcRoot = QDir::current().absoluteFilePath("src");
    QDir srcDir(srcRoot);
    if (!srcDir.exists()) {
        qDebug() << "ZeroTouch: src directory missing, skipping file watcher";
        return;
    }

    auto* watcher = new QFileSystemWatcher(this);
    QStringList files;
    QDirIterator it(srcRoot, QStringList() << "*.cpp" << "*.hpp", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files << it.next();
    }
    if (files.isEmpty()) {
        qDebug() << "ZeroTouch: no source files found for watcher";
        return;
    }

    watcher->addPaths(files);

    connect(watcher, &QFileSystemWatcher::fileChanged,
            this, [watcher](const QString& path) {
                if (!QFileInfo::exists(path)) {
                    // Editors rewrite files; re-add watcher silently
                    watcher->addPath(path);
                }

                if (!path.endsWith(".cpp") && !path.endsWith(".hpp")) {
                    return;
                }

                QTimer::singleShot(5000, watcher, [path]() {
                    QString wish = QStringLiteral("Auto-fix and ship after source change in %1")
                                        .arg(QFileInfo(path).fileName());
                    qputenv("RAWRXD_AUTO_APPROVE", "1");
                    AutoBootstrap::startWithWish(wish);
                });
            });
}

void ZeroTouch::installGitHook() {
    QDir hooksDir(QDir::current().absoluteFilePath(".git/hooks"));
    if (!hooksDir.exists()) {
        qDebug() << "ZeroTouch: git hooks directory missing - skip";
        return;
    }

    QString hookPath = hooksDir.filePath("post-commit");
    QString agentExe = QDir::current().absoluteFilePath("build/bin/Release/RawrXD-Agent.exe");
    agentExe.replace('\\', '/');

    QString hookScript = QString(
        "#!/bin/sh\n"
        "# RawrXD zero-touch trigger\n"
        "WISH=$(git log -1 --pretty=%B | head -1)\n"
        "if echo \"$WISH\" | grep -qE \"(ship|release|fix|add)\"; then\n"
        "  export RAWRXD_WISH=\"$WISH\"\n"
        "  %1\n"
        "fi\n"
    ).arg(agentExe);

    QFile hookFile(hookPath);
    if (!hookFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ZeroTouch: failed to write git hook" << hookPath;
        return;
    }

    hookFile.write(hookScript.toUtf8());
    hookFile.setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    hookFile.close();

    qDebug() << "ZeroTouch: post-commit hook installed";
}

void ZeroTouch::installVoiceTrigger() {
    QTimer* poller = new QTimer(this);
    poller->setInterval(2000);

    connect(poller, &QTimer::timeout, this, [this]() {
        QClipboard* clipboard = QApplication::clipboard();
        QString spoken = clipboard->text(QClipboard::Selection);
        if (spoken.isEmpty()) {
            spoken = clipboard->text(QClipboard::Clipboard);
        }

        if (spoken.isEmpty() || spoken == m_lastVoiceWish) {
            return;
        }

        if (spoken.length() > 10 && spoken.length() < 200 &&
            (spoken.contains("ship", Qt::CaseInsensitive) ||
             spoken.contains("release", Qt::CaseInsensitive) ||
             spoken.contains("fix", Qt::CaseInsensitive))) {
            m_lastVoiceWish = spoken;
            clipboard->clear(QClipboard::Selection);
            qputenv("RAWRXD_AUTO_APPROVE", "1");
            AutoBootstrap::startWithWish(spoken);
        }
    });

    poller->start();
}

/*
 ---------- 4. CI cron (GitHub Actions) ----------
 File: .github/workflows/zero_human.yml (job excerpt)
 on:
   schedule:
     - cron: '0 2 * * *'   # 02:00 UTC nightly
   workflow_dispatch:
 jobs:
   self_improve:
     runs-on: windows-latest
     steps:
       - uses: actions/checkout@v4
       - name: Let IDE improve itself
         run: |
           set RAWRXD_WISH="Improve inference speed by 10 % without quality loss"
           build\bin\Release\RawrXD-Agent.exe
         env:
           GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
           CERT_PASS: ${{ secrets.CERT_PASS }}
           AZURE_STORAGE_KEY: ${{ secrets.AZURE_STORAGE_KEY }}
           TWITTER_BEARER: ${{ secrets.TWITTER_BEARER }}
*/
