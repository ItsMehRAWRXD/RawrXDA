/**
 * AI Commands Feature - Integration Test
 * 
 * This test verifies all AI commands are fully functional:
 * - /help
 * - /refactor <prompt>
 * - @plan <task>
 * - @analyze
 * - @generate <spec>
 */

#include <QApplication>
#include <QTest>
#include "src/qtapp/ai_chat_panel.hpp"

class TestAICommands : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Initialize Qt application for widget testing
        int argc = 1;
        char* argv[] = {(char*)"test"};
        app = new QApplication(argc, argv);
        
        panel = new AIChatPanel();
        panel->initialize();
    }

    void cleanupTestCase() {
        delete panel;
        delete app;
    }

    void testHelpCommand() {
        // Simulate typing /help
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "/help");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        // Verify help text is displayed
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("AI Commands"));
        QVERIFY(lastMessage.contains("/refactor"));
        QVERIFY(lastMessage.contains("@plan"));
        QVERIFY(lastMessage.contains("@analyze"));
        QVERIFY(lastMessage.contains("@generate"));
    }

    void testRefactorCommand() {
        // Set context code
        QString testCode = "void duplicateFunction() {\n    int x = 5;\n    int y = 10;\n}\n";
        panel->setContext(testCode, "test.cpp");
        
        // Simulate /refactor command
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "/refactor Extract common variables");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("refactoring") || lastMessage.contains("🔄"));
    }

    void testPlanCommand() {
        // Simulate @plan command
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@plan Add error handling");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("plan") || lastMessage.contains("📋"));
    }

    void testAnalyzeCommand() {
        // Set context code
        QString testCode = "int calculate(int a, int b) { return a + b; }";
        panel->setContext(testCode, "calculator.cpp");
        
        // Simulate @analyze command
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@analyze");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Analyzing") || lastMessage.contains("🔍"));
    }

    void testAnalyzeWithoutContext() {
        // Clear context
        panel->setContext("", "");
        
        // Simulate @analyze without context
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@analyze");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("No code selected") || lastMessage.contains("⚠"));
    }

    void testGenerateCommand() {
        // Simulate @generate command
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@generate A function to validate email addresses");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Generating") || lastMessage.contains("⚡"));
    }

    void testInvalidCommand() {
        // Simulate unknown command
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "/unknown");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Unknown command") || lastMessage.contains("❌"));
        QVERIFY(lastMessage.contains("/help"));
    }

    void testCommandsWithEmptyPrompt() {
        // Test /refactor without prompt
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "/refactor");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        QString lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Usage:"));
        
        panel->clear();
        
        // Test @plan without task
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@plan");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Usage:"));
        
        panel->clear();
        
        // Test @generate without spec
        QTest::keyClicks(panel->findChild<QLineEdit*>(), "@generate");
        QTest::mouseClick(panel->findChild<QPushButton*>("sendButton"), Qt::LeftButton);
        
        lastMessage = panel->lastAssistantMessage();
        QVERIFY(lastMessage.contains("Usage:"));
    }

private:
    QApplication* app;
    AIChatPanel* panel;
};

QTEST_MAIN(TestAICommands)
#include "test_ai_commands.moc"
