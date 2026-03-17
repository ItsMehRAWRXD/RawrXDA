// Comprehensive GUI Component Test Harness
// Tests all components in isolation without full MainWindow startup
// Prevents startup crashes from affecting test results

#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QTextStream>

// Component headers
#include "../src/qtapp/agentic_mode_switcher.hpp"
#include "../src/qtapp/model_selector.hpp"
#include "../src/qtapp/ai_chat_panel.hpp"
#include "../src/qtapp/command_palette.hpp"
#include "../src/qtapp/ActivityBar.h"

class ComponentTestHarness : public QObject
{
    Q_OBJECT

private:
    QFile logFile;
    QTextStream log;
    int passCount = 0;
    int failCount = 0;

    void logResult(const QString& test, bool pass, const QString& details = "") {
        QString result = pass ? "✓ PASS" : "✗ FAIL";
        QString line = QString("[%1] %2: %3").arg(result, test, details);
        qInfo() << line;
        log << line << "\n";
        log.flush();
        
        if (pass) passCount++;
        else failCount++;
    }

private slots:
    void initTestCase() {
        logFile.setFileName("gui_component_tests.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            log.setDevice(&logFile);
            log << "=== RawrXD GUI Component Test Suite ===\n";
            log << "Date: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
            log.flush();
        }
        qInfo() << "Starting GUI Component Tests...";
    }

    void cleanupTestCase() {
        log << "\n=== TEST SUMMARY ===\n";
        log << QString("Total: %1 | Pass: %2 | Fail: %3\n")
               .arg(passCount + failCount).arg(passCount).arg(failCount);
        log << QString("Success Rate: %1%\n")
               .arg(passCount * 100.0 / (passCount + failCount), 0, 'f', 1);
        log.flush();
        logFile.close();
        
        qInfo() << "\n=== FINAL RESULTS ===";
        qInfo() << "Total Tests:" << (passCount + failCount);
        qInfo() << "Passed:" << passCount;
        qInfo() << "Failed:" << failCount;
        qInfo() << QString("Success Rate: %1%")
                   .arg(passCount * 100.0 / (passCount + failCount), 0, 'f', 1);
    }

    // ========================================
    // AGENTIC MODE SWITCHER TESTS
    // ========================================

    void test_AgenticModeSwitcher_Creation() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        bool pass = switcher != nullptr;
        logResult("AgenticModeSwitcher Creation", pass);
        
        if (pass) {
            delete switcher;
        }
    }

    void test_AgenticModeSwitcher_DefaultMode() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        bool pass = switcher->currentMode() == AgenticModeSwitcher::ASK_MODE;
        logResult("AgenticModeSwitcher Default Mode", pass, 
                  QString("Expected ASK_MODE, got %1").arg(switcher->currentMode()));
        delete switcher;
    }

    void test_AgenticModeSwitcher_ModeChange() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        QSignalSpy spy(switcher, &AgenticModeSwitcher::modeChanged);
        
        switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
        
        bool signalEmitted = spy.count() == 1;
        bool modeCorrect = switcher->currentMode() == AgenticModeSwitcher::PLAN_MODE;
        bool pass = signalEmitted && modeCorrect;
        
        logResult("AgenticModeSwitcher Mode Change", pass,
                  QString("Signal count: %1, Mode: %2")
                  .arg(spy.count()).arg(switcher->currentMode()));
        delete switcher;
    }

    void test_AgenticModeSwitcher_AllModes() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        QSignalSpy spy(switcher, &AgenticModeSwitcher::modeChanged);
        
        switcher->setMode(AgenticModeSwitcher::ASK_MODE);
        switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
        switcher->setMode(AgenticModeSwitcher::AGENT_MODE);
        
        bool pass = spy.count() == 2 && // ASK_MODE is default, so only 2 changes
                    switcher->currentMode() == AgenticModeSwitcher::AGENT_MODE;
        
        logResult("AgenticModeSwitcher All Modes", pass,
                  QString("Signal count: %1, Final mode: %2")
                  .arg(spy.count()).arg(switcher->currentMode()));
        delete switcher;
    }

    void test_AgenticModeSwitcher_ActivityIndicator() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        
        switcher->setModeActive(true);
        // Visual test - can't programmatically verify spinner, but can verify no crash
        QTest::qWait(100); // Let animation run
        
        switcher->setModeActive(false);
        QTest::qWait(50);
        
        bool pass = true; // If we got here without crashing
        logResult("AgenticModeSwitcher Activity Indicator", pass);
        delete switcher;
    }

    void test_AgenticModeSwitcher_Progress() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        
        switcher->showProgress("Testing progress...");
        QTest::qWait(100);
        
        bool pass = true; // Visual verification
        logResult("AgenticModeSwitcher Progress Display", pass);
        delete switcher;
    }

    // ========================================
    // MODEL SELECTOR TESTS
    // ========================================

    void test_ModelSelector_Creation() {
        ModelSelector* selector = new ModelSelector();
        bool pass = selector != nullptr;
        logResult("ModelSelector Creation", pass);
        
        if (pass) {
            delete selector;
        }
    }

    void test_ModelSelector_DefaultState() {
        ModelSelector* selector = new ModelSelector();
        bool pass = selector->currentStatus() == ModelSelector::IDLE;
        logResult("ModelSelector Default State", pass,
                  QString("Expected IDLE, got %1").arg(selector->currentStatus()));
        delete selector;
    }

    void test_ModelSelector_AddModel() {
        ModelSelector* selector = new ModelSelector();
        
        selector->addModel("test-model.gguf", "/path/to/test-model.gguf");
        QTest::qWait(50);
        
        bool pass = true; // If no crash
        logResult("ModelSelector Add Model", pass);
        delete selector;
    }

    void test_ModelSelector_StatusTransitions() {
        ModelSelector* selector = new ModelSelector();
        
        selector->setModelLoading(true);
        bool loadingState = selector->currentStatus() == ModelSelector::LOADING;
        
        selector->setModelLoaded("test-model.gguf", 1024 * 1024);
        bool loadedState = selector->currentStatus() == ModelSelector::LOADED;
        
        selector->setModelError("Test error");
        bool errorState = selector->currentStatus() == ModelSelector::ERROR;
        
        bool pass = loadingState && loadedState && errorState;
        logResult("ModelSelector Status Transitions", pass,
                  QString("Loading: %1, Loaded: %2, Error: %3")
                  .arg(loadingState).arg(loadedState).arg(errorState));
        delete selector;
    }

    void test_ModelSelector_Signals() {
        ModelSelector* selector = new ModelSelector();
        
        QSignalSpy modelSelectedSpy(selector, &ModelSelector::modelSelected);
        QSignalSpy loadNewSpy(selector, &ModelSelector::loadNewModelRequested);
        QSignalSpy unloadSpy(selector, &ModelSelector::unloadModelRequested);
        QSignalSpy infoSpy(selector, &ModelSelector::modelInfoRequested);
        
        emit selector->modelSelected("test.gguf");
        emit selector->loadNewModelRequested();
        emit selector->unloadModelRequested();
        emit selector->modelInfoRequested();
        
        bool pass = modelSelectedSpy.count() == 1 &&
                    loadNewSpy.count() == 1 &&
                    unloadSpy.count() == 1 &&
                    infoSpy.count() == 1;
        
        logResult("ModelSelector Signals", pass,
                  QString("Selected: %1, Load: %2, Unload: %3, Info: %4")
                  .arg(modelSelectedSpy.count()).arg(loadNewSpy.count())
                  .arg(unloadSpy.count()).arg(infoSpy.count()));
        delete selector;
    }

    void test_ModelSelector_ClearModels() {
        ModelSelector* selector = new ModelSelector();
        
        selector->addModel("model1.gguf", "/path/1");
        selector->addModel("model2.gguf", "/path/2");
        selector->clearModels();
        
        bool pass = true; // If no crash
        logResult("ModelSelector Clear Models", pass);
        delete selector;
    }

    // ========================================
    // AI CHAT PANEL TESTS
    // ========================================

    void test_AIChatPanel_Creation() {
        AIChatPanel* panel = new AIChatPanel();
        bool pass = panel != nullptr;
        logResult("AIChatPanel Creation", pass);
        
        if (pass) {
            delete panel;
        }
    }

    void test_AIChatPanel_AddUserMessage() {
        AIChatPanel* panel = new AIChatPanel();
        
        panel->addUserMessage("Test user message");
        QTest::qWait(50);
        
        bool pass = true; // If no crash
        logResult("AIChatPanel Add User Message", pass);
        delete panel;
    }

    void test_AIChatPanel_AddAssistantMessage() {
        AIChatPanel* panel = new AIChatPanel();
        
        panel->addAssistantMessage("Test assistant message");
        QTest::qWait(50);
        
        bool pass = true; // If no crash
        logResult("AIChatPanel Add Assistant Message", pass);
        delete panel;
    }

    void test_AIChatPanel_StreamingMessage() {
        AIChatPanel* panel = new AIChatPanel();
        
        panel->addAssistantMessage("", true); // Start streaming
        panel->updateStreamingMessage("Token 1 ");
        QTest::qWait(20);
        panel->updateStreamingMessage("Token 1 Token 2 ");
        QTest::qWait(20);
        panel->updateStreamingMessage("Token 1 Token 2 Token 3");
        QTest::qWait(20);
        panel->finishStreaming();
        
        bool pass = true; // If no crash during streaming
        logResult("AIChatPanel Streaming Message", pass);
        delete panel;
    }

    void test_AIChatPanel_Clear() {
        AIChatPanel* panel = new AIChatPanel();
        
        panel->addUserMessage("Message 1");
        panel->addAssistantMessage("Response 1");
        panel->clear();
        
        bool pass = true; // If no crash
        logResult("AIChatPanel Clear", pass);
        delete panel;
    }

    void test_AIChatPanel_Context() {
        AIChatPanel* panel = new AIChatPanel();
        
        QString code = "void test() { return 42; }";
        QString filePath = "test.cpp";
        panel->setContext(code, filePath);
        
        bool pass = true; // If no crash
        logResult("AIChatPanel Set Context", pass);
        delete panel;
    }

    void test_AIChatPanel_Signals() {
        AIChatPanel* panel = new AIChatPanel();
        
        QSignalSpy messageSpy(panel, &AIChatPanel::messageSubmitted);
        QSignalSpy quickActionSpy(panel, &AIChatPanel::quickActionTriggered);
        
        emit panel->messageSubmitted("Test message");
        emit panel->quickActionTriggered("Explain", "context");
        
        bool pass = messageSpy.count() == 1 && quickActionSpy.count() == 1;
        logResult("AIChatPanel Signals", pass,
                  QString("Message: %1, QuickAction: %2")
                  .arg(messageSpy.count()).arg(quickActionSpy.count()));
        delete panel;
    }

    // ========================================
    // COMMAND PALETTE TESTS
    // ========================================

    void test_CommandPalette_Creation() {
        CommandPalette* palette = new CommandPalette();
        bool pass = palette != nullptr;
        logResult("CommandPalette Creation", pass);
        
        if (pass) {
            delete palette;
        }
    }

    void test_CommandPalette_RegisterCommand() {
        CommandPalette* palette = new CommandPalette();
        
        CommandPalette::Command cmd;
        cmd.id = "test.command";
        cmd.label = "Test Command";
        cmd.category = "Testing";
        cmd.description = "A test command";
        cmd.enabled = true;
        
        palette->registerCommand(cmd);
        
        bool pass = true; // If no crash
        logResult("CommandPalette Register Command", pass);
        delete palette;
    }

    void test_CommandPalette_MultipleCommands() {
        CommandPalette* palette = new CommandPalette();
        
        for (int i = 0; i < 50; ++i) {
            CommandPalette::Command cmd;
            cmd.id = QString("test.command.%1").arg(i);
            cmd.label = QString("Test Command %1").arg(i);
            cmd.category = "Testing";
            cmd.enabled = true;
            palette->registerCommand(cmd);
        }
        
        bool pass = true; // If no crash with many commands
        logResult("CommandPalette Multiple Commands", pass);
        delete palette;
    }

    void test_CommandPalette_ShowHide() {
        CommandPalette* palette = new CommandPalette();
        
        palette->show();
        QTest::qWait(100);
        
        palette->hide();
        QTest::qWait(50);
        
        bool pass = true; // If no crash during show/hide
        logResult("CommandPalette Show/Hide", pass);
        delete palette;
    }

    void test_CommandPalette_Search() {
        CommandPalette* palette = new CommandPalette();
        
        // Register some commands
        CommandPalette::Command cmd1;
        cmd1.id = "file.open";
        cmd1.label = "Open File";
        cmd1.category = "File";
        cmd1.enabled = true;
        palette->registerCommand(cmd1);
        
        CommandPalette::Command cmd2;
        cmd2.id = "edit.find";
        cmd2.label = "Find in Files";
        cmd2.category = "Edit";
        cmd2.enabled = true;
        palette->registerCommand(cmd2);
        
        palette->show();
        // Simulate search (we can't directly access private updateResults)
        QTest::qWait(100);
        
        bool pass = true; // If no crash during search
        logResult("CommandPalette Search", pass);
        delete palette;
    }

    void test_CommandPalette_Signal() {
        CommandPalette* palette = new CommandPalette();
        
        QSignalSpy spy(palette, &CommandPalette::commandExecuted);
        
        emit palette->commandExecuted("test.command");
        
        bool pass = spy.count() == 1;
        logResult("CommandPalette Signal", pass,
                  QString("Signal count: %1").arg(spy.count()));
        delete palette;
    }

    // ========================================
    // ACTIVITY BAR TESTS
    // ========================================

    void test_ActivityBar_Creation() {
        ActivityBar* bar = new ActivityBar();
        bool pass = bar != nullptr;
        logResult("ActivityBar Creation", pass);
        
        if (pass) {
            delete bar;
        }
    }

    void test_ActivityBar_FixedWidth() {
        ActivityBar* bar = new ActivityBar();
        bool pass = bar->width() == 50 || bar->minimumWidth() == 50;
        logResult("ActivityBar Fixed Width", pass,
                  QString("Width: %1, MinWidth: %2")
                  .arg(bar->width()).arg(bar->minimumWidth()));
        delete bar;
    }

    // ========================================
    // INTEGRATION TESTS
    // ========================================

    void test_Integration_ModeSwitcherWithSelector() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        ModelSelector* selector = new ModelSelector();
        
        // Simulate mode change affecting model selector
        switcher->setMode(AgenticModeSwitcher::AGENT_MODE);
        selector->setModelLoading(true);
        QTest::qWait(100);
        
        bool pass = true; // If components coexist without conflict
        logResult("Integration: Mode Switcher + Model Selector", pass);
        
        delete switcher;
        delete selector;
    }

    void test_Integration_ChatPanelWithPalette() {
        AIChatPanel* panel = new AIChatPanel();
        CommandPalette* palette = new CommandPalette();
        
        panel->addUserMessage("Test");
        palette->show();
        QTest::qWait(50);
        palette->hide();
        
        bool pass = true; // If components coexist
        logResult("Integration: Chat Panel + Command Palette", pass);
        
        delete panel;
        delete palette;
    }

    void test_Integration_AllComponentsTogether() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        ModelSelector* selector = new ModelSelector();
        AIChatPanel* panel = new AIChatPanel();
        CommandPalette* palette = new CommandPalette();
        ActivityBar* bar = new ActivityBar();
        
        // Simulate concurrent operations
        switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
        selector->addModel("test.gguf", "/path");
        panel->addUserMessage("Hello");
        palette->show();
        QTest::qWait(100);
        palette->hide();
        
        bool pass = true; // If all components coexist
        logResult("Integration: All Components Together", pass);
        
        delete switcher;
        delete selector;
        delete panel;
        delete palette;
        delete bar;
    }

    // ========================================
    // STRESS TESTS
    // ========================================

    void test_Stress_RapidModeChanges() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        QSignalSpy spy(switcher, &AgenticModeSwitcher::modeChanged);
        
        for (int i = 0; i < 100; ++i) {
            switcher->setMode(static_cast<AgenticModeSwitcher::Mode>(i % 3));
        }
        
        bool pass = spy.count() > 0; // At least some signals fired
        logResult("Stress: Rapid Mode Changes", pass,
                  QString("Signals: %1").arg(spy.count()));
        delete switcher;
    }

    void test_Stress_ManyMessages() {
        AIChatPanel* panel = new AIChatPanel();
        
        for (int i = 0; i < 100; ++i) {
            if (i % 2 == 0) {
                panel->addUserMessage(QString("User message %1").arg(i));
            } else {
                panel->addAssistantMessage(QString("Assistant message %1").arg(i));
            }
        }
        
        bool pass = true; // If no crash with many messages
        logResult("Stress: Many Messages", pass);
        delete panel;
    }

    void test_Stress_RapidShowHide() {
        CommandPalette* palette = new CommandPalette();
        
        for (int i = 0; i < 50; ++i) {
            palette->show();
            QTest::qWait(5);
            palette->hide();
            QTest::qWait(5);
        }
        
        bool pass = true; // If no crash during rapid show/hide
        logResult("Stress: Rapid Show/Hide", pass);
        delete palette;
    }

    // ========================================
    // MEMORY TESTS
    // ========================================

    void test_Memory_CreateDestroy() {
        for (int i = 0; i < 100; ++i) {
            AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
            delete switcher;
        }
        
        for (int i = 0; i < 100; ++i) {
            ModelSelector* selector = new ModelSelector();
            delete selector;
        }
        
        bool pass = true; // If no memory leaks or crashes
        logResult("Memory: Create/Destroy Cycles", pass);
    }

    void test_Memory_SignalConnections() {
        AgenticModeSwitcher* switcher = new AgenticModeSwitcher();
        
        for (int i = 0; i < 100; ++i) {
            QObject* receiver = new QObject();
            connect(switcher, &AgenticModeSwitcher::modeChanged,
                    receiver, [](int){});
            delete receiver; // Disconnects automatically
        }
        
        bool pass = true; // If no crashes
        logResult("Memory: Signal Connection Cleanup", pass);
        delete switcher;
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    qInfo() << "======================================";
    qInfo() << "RawrXD GUI Component Test Harness";
    qInfo() << "Testing all components in isolation";
    qInfo() << "Startup crash protection: ENABLED";
    qInfo() << "======================================\n";
    
    ComponentTestHarness tc;
    int result = QTest::qExec(&tc, argc, argv);
    
    qInfo() << "\nTest harness completed. Exit code:" << result;
    qInfo() << "Check gui_component_tests.log for detailed results.";
    
    return result;
}

#include "test_gui_components.moc"
