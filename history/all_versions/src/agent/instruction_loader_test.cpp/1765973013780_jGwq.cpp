#include <QtTest>
#include <QFile>
#include <QDir>
#include <QThread>
#include "agentic_engine.h"

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

        AgenticEngine engine;
        engine.initialize();

        engine.setInstructionFilePath(tmp);
        QString loaded = engine.loadedInstructions();
        QVERIFY(!loaded.isEmpty());
        QVERIFY(loaded.contains("HelloAI"));

        // Modify file and ensure reload occurs
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            f.write("INSTR: NewMarker\nChanged content");
            f.close();
        } else {
            QFAIL("Failed to modify temp instruction file");
        }

        // Wait for filesystem watcher to notice and for the slot to run
        QTest::qWait(600);

        QString loaded2 = engine.loadedInstructions();
        QVERIFY(!loaded2.isEmpty());
        QVERIFY(loaded2.contains("NewMarker"));

        // Cleanup
        QFile::remove(tmp);
    }
};

QTEST_MAIN(InstructionLoaderTest)
#include "instruction_loader_test.moc"
