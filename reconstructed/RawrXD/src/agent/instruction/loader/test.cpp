#include <QtTest>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QFileSystemWatcher>

class InstructionLoaderTest : public QObject {
    Q_OBJECT

private slots:
    void testLoadAndReload() {
        QString tmp = QDir::tempPath() + "/rawrxd_instr_test.md";

        // Write initial instructions
        QFile f(tmp);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write("INSTR: HelloAI\nAlways greet the user.");
            f.close();
        } else {
            QFAIL("Failed to create temp instruction file");
        }

        QFileSystemWatcher watcher;
        bool notified = false;
        connect(&watcher, &QFileSystemWatcher::fileChanged, this, [&](const QString& path){
            if (path == tmp) notified = true;
        });

        QVERIFY(watcher.addPath(tmp));

        // Modify file and ensure watcher notices
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            f.write("INSTR: NewMarker\nChanged content");
            f.close();
        } else {
            QFAIL("Failed to modify temp instruction file");
        }

        QTest::qWait(600);
        QVERIFY(notified);

        // Read file content and verify change
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QTextStream(&f).readAll();
            f.close();
            QVERIFY(content.contains("NewMarker"));
        } else {
            QFAIL("Failed to open temp instruction file for read");
        }

        // Cleanup
        QFile::remove(tmp);
    }
};

QTEST_MAIN(InstructionLoaderTest)
#include "instruction_loader_test.moc"
