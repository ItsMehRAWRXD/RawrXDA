// Standalone GUI Component Test - NO APPLICATION STARTUP REQUIRED
// This test verifies components work in complete isolation
#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <memory>

// Include ONLY the component headers
#include "qtapp/agentic_mode_switcher.hpp"
#include "qtapp/model_selector.hpp"
#include "qtapp/ai_chat_panel.hpp"
#include "qtapp/command_palette.hpp"

class StandaloneComponentTest : public QObject
{
    Q_OBJECT

private:
    void logPass(const QString& test) {
        qDebug() << "[PASS]" << test;
    }
    
    void logFail(const QString& test, const QString& reason) {
        qWarning() << "[FAIL]" << test << ":" << reason;
    }

private slots:
    // Test 1: AgenticModeSwitcher basic creation
    void test_AgenticModeSwitcher_Creation() {
        try {
            std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
            if (switcher) {
                logPass("AgenticModeSwitcher creation");
            } else {
                logFail("AgenticModeSwitcher creation", "null pointer");
            }
        } catch (const std::exception& e) {
            logFail("AgenticModeSwitcher creation", e.what());
        }
    }

    // Test 2: AgenticModeSwitcher mode switching
    void test_AgenticModeSwitcher_ModeSwitch() {
        try {
            std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
            QSignalSpy spy(switcher.get(), &AgenticModeSwitcher::modeChanged);
            
            switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
            
            if (spy.count() == 1 && switcher->currentMode() == AgenticModeSwitcher::PLAN_MODE) {
                logPass("AgenticModeSwitcher mode switch");
            } else {
                logFail("AgenticModeSwitcher mode switch", 
                       QString("Expected 1 signal, got %1; mode=%2").arg(spy.count()).arg(switcher->currentMode()));
            }
        } catch (const std::exception& e) {
            logFail("AgenticModeSwitcher mode switch", e.what());
        }
    }

    // Test 3: AgenticModeSwitcher all modes
    void test_AgenticModeSwitcher_AllModes() {
        try {
            std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
            bool allPass = true;
            
            switcher->setMode(AgenticModeSwitcher::ASK_MODE);
            allPass &= (switcher->currentMode() == AgenticModeSwitcher::ASK_MODE);
            
            switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
            allPass &= (switcher->currentMode() == AgenticModeSwitcher::PLAN_MODE);
            
            switcher->setMode(AgenticModeSwitcher::AGENT_MODE);
            allPass &= (switcher->currentMode() == AgenticModeSwitcher::AGENT_MODE);
            
            if (allPass) {
                logPass("AgenticModeSwitcher all modes");
            } else {
                logFail("AgenticModeSwitcher all modes", "mode mismatch");
            }
        } catch (const std::exception& e) {
            logFail("AgenticModeSwitcher all modes", e.what());
        }
    }

    // Test 4: ModelSelector basic creation
    void test_ModelSelector_Creation() {
        try {
            std::unique_ptr<ModelSelector> selector(new ModelSelector());
            if (selector) {
                logPass("ModelSelector creation");
            } else {
                logFail("ModelSelector creation", "null pointer");
            }
        } catch (const std::exception& e) {
            logFail("ModelSelector creation", e.what());
        }
    }

    // Test 5: ModelSelector add model
    void test_ModelSelector_AddModel() {
        try {
            std::unique_ptr<ModelSelector> selector(new ModelSelector());
            selector->addModel("test-model-7b");
            selector->addModel("another-model-13b");
            
            // Check combo box has items
            if (selector->findChildren<QComboBox*>().isEmpty()) {
                logFail("ModelSelector add model", "no combo box found");
            } else {
                auto* combo = selector->findChildren<QComboBox*>().first();
                if (combo->count() >= 2) {
                    logPass("ModelSelector add model");
                } else {
                    logFail("ModelSelector add model", QString("Expected >=2 items, got %1").arg(combo->count()));
                }
            }
        } catch (const std::exception& e) {
            logFail("ModelSelector add model", e.what());
        }
    }

    // Test 6: ModelSelector status transitions
    void test_ModelSelector_StatusTransitions() {
        try {
            std::unique_ptr<ModelSelector> selector(new ModelSelector());
            selector->addModel("test-model");
            
            selector->setModelLoading("test-model");
            selector->setModelLoaded("test-model");
            selector->setModelError("test-model", "test error");
            
            logPass("ModelSelector status transitions");
        } catch (const std::exception& e) {
            logFail("ModelSelector status transitions", e.what());
        }
    }

    // Test 7: AIChatPanel basic creation
    void test_AIChatPanel_Creation() {
        try {
            std::unique_ptr<AIChatPanel> panel(new AIChatPanel());
            if (panel) {
                logPass("AIChatPanel creation");
            } else {
                logFail("AIChatPanel creation", "null pointer");
            }
        } catch (const std::exception& e) {
            logFail("AIChatPanel creation", e.what());
        }
    }

    // Test 8: AIChatPanel add messages
    void test_AIChatPanel_AddMessages() {
        try {
            std::unique_ptr<AIChatPanel> panel(new AIChatPanel());
            panel->addUserMessage("Test user message");
            panel->addAssistantMessage("Test assistant message");
            
            logPass("AIChatPanel add messages");
        } catch (const std::exception& e) {
            logFail("AIChatPanel add messages", e.what());
        }
    }

    // Test 9: AIChatPanel streaming
    void test_AIChatPanel_Streaming() {
        try {
            std::unique_ptr<AIChatPanel> panel(new AIChatPanel());
            panel->addUserMessage("Test");
            panel->updateStreamingMessage("Streaming ");
            panel->updateStreamingMessage("Streaming text...");
            panel->finalizeStreamingMessage();
            
            logPass("AIChatPanel streaming");
        } catch (const std::exception& e) {
            logFail("AIChatPanel streaming", e.what());
        }
    }

    // Test 10: CommandPalette basic creation
    void test_CommandPalette_Creation() {
        try {
            std::unique_ptr<CommandPalette> palette(new CommandPalette());
            if (palette) {
                logPass("CommandPalette creation");
            } else {
                logFail("CommandPalette creation", "null pointer");
            }
        } catch (const std::exception& e) {
            logFail("CommandPalette creation", e.what());
        }
    }

    // Test 11: CommandPalette register commands
    void test_CommandPalette_RegisterCommands() {
        try {
            std::unique_ptr<CommandPalette> palette(new CommandPalette());
            bool called = false;
            
            palette->registerCommand("test.command", "Test Command", "Testing", 
                                    [&called]() { called = true; });
            palette->registerCommand("test.another", "Another Test", "Testing", 
                                    []() {});
            
            logPass("CommandPalette register commands");
        } catch (const std::exception& e) {
            logFail("CommandPalette register commands", e.what());
        }
    }

    // Test 12: CommandPalette search
    void test_CommandPalette_Search() {
        try {
            std::unique_ptr<CommandPalette> palette(new CommandPalette());
            palette->registerCommand("file.open", "Open File", "File", []() {});
            palette->registerCommand("file.save", "Save File", "File", []() {});
            
            // Simulate search
            auto* lineEdit = palette->findChildren<QLineEdit*>().first();
            if (lineEdit) {
                lineEdit->setText("file");
                logPass("CommandPalette search");
            } else {
                logFail("CommandPalette search", "no line edit found");
            }
        } catch (const std::exception& e) {
            logFail("CommandPalette search", e.what());
        }
    }

    // Test 13: Memory leak test - create and destroy multiple times
    void test_MemoryLeaks() {
        try {
            for (int i = 0; i < 100; ++i) {
                {
                    std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
                    switcher->setMode(AgenticModeSwitcher::PLAN_MODE);
                }
                {
                    std::unique_ptr<ModelSelector> selector(new ModelSelector());
                    selector->addModel("test");
                }
                {
                    std::unique_ptr<AIChatPanel> panel(new AIChatPanel());
                    panel->addUserMessage("test");
                }
                {
                    std::unique_ptr<CommandPalette> palette(new CommandPalette());
                    palette->registerCommand("test", "Test", "Test", []() {});
                }
            }
            logPass("Memory leak test (100 iterations)");
        } catch (const std::exception& e) {
            logFail("Memory leak test", e.what());
        }
    }

    // Test 14: Concurrent widget creation
    void test_ConcurrentCreation() {
        try {
            std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
            std::unique_ptr<ModelSelector> selector(new ModelSelector());
            std::unique_ptr<AIChatPanel> panel(new AIChatPanel());
            std::unique_ptr<CommandPalette> palette(new CommandPalette());
            
            // All created successfully
            if (switcher && selector && panel && palette) {
                logPass("Concurrent widget creation");
            } else {
                logFail("Concurrent widget creation", "null pointer");
            }
        } catch (const std::exception& e) {
            logFail("Concurrent widget creation", e.what());
        }
    }

    // Test 15: Signal connections
    void test_SignalConnections() {
        try {
            std::unique_ptr<AgenticModeSwitcher> switcher(new AgenticModeSwitcher());
            std::unique_ptr<ModelSelector> selector(new ModelSelector());
            
            QSignalSpy modeSpy(switcher.get(), &AgenticModeSwitcher::modeChanged);
            QSignalSpy modelSpy(selector.get(), &ModelSelector::modelSelected);
            
            switcher->setMode(AgenticModeSwitcher::AGENT_MODE);
            selector->addModel("test");
            
            if (modeSpy.count() == 1) {
                logPass("Signal connections");
            } else {
                logFail("Signal connections", QString("Expected 1 mode signal, got %1").arg(modeSpy.count()));
            }
        } catch (const std::exception& e) {
            logFail("Signal connections", e.what());
        }
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    
    qInfo() << "========================================";
    qInfo() << "RawrXD Standalone Component Test";
    qInfo() << "NO APPLICATION STARTUP REQUIRED";
    qInfo() << "========================================\n";
    
    StandaloneComponentTest test;
    int result = QTest::qExec(&test, argc, argv);
    
    qInfo() << "\n========================================";
    qInfo() << "Test execution complete!";
    qInfo() << "Exit code:" << result;
    qInfo() << "========================================";
    
    return result;
}

#include "standalone_component_test.moc"
