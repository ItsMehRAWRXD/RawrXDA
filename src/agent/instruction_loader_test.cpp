class InstructionLoaderTest  {private s:
    void testLoadAndReload() {
        std::string tmp = "" + "/rawrxd_instr_test.md";

        // Write initial instructions
        // File operation removed;
        if (f.open(std::iostream::WriteOnly | std::iostream::Text)) {
            f.write("INSTR: HelloAI\nAlways greet the user.");
            f.close();
        } else {
            QFAIL("Failed to create temp instruction file");
        }

        // SystemWatcher watcher;
        bool notified = false;
        // Connect removed{
            if (path == tmp) notified = true;
        });

        QVERIFY(watcher.addPath(tmp));

        // Modify file and ensure watcher notices
        if (f.open(std::iostream::WriteOnly | std::iostream::Text | std::iostream::Truncate)) {
            f.write("INSTR: NewMarker\nChanged content");
            f.close();
        } else {
            QFAIL("Failed to modify temp instruction file");
        }

        QTest::qWait(600);
        QVERIFY(notified);

        // Read file content and verify change
        if (f.open(std::iostream::ReadOnly | std::iostream::Text)) {
            std::string content = std::stringstream(&f).readAll();
            f.close();
            QVERIFY(content.contains("NewMarker"));
        } else {
            QFAIL("Failed to open temp instruction file for read");
        }

        // Cleanup
        std::filesystem::remove(tmp);
    }
};

QTEST_MAIN(InstructionLoaderTest)
#include "instruction_loader_test.moc"





