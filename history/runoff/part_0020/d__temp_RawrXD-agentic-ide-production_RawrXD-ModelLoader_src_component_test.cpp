#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <iostream>

#include "streaming_token_manager.h"
#include "model_router.h"
#include "tool_registry.h"
#include "agentic_planner.h"
#include "command_palette.h"
#include "diff_viewer.h"

/**
 * @brief ComponentTest - Verifies the ported MASM components in a C++ environment
 */
class ComponentTest : public QObject {
    Q_OBJECT
public:
    ComponentTest(QObject* parent = nullptr) : QObject(parent) {}

    void run() {
        std::cout << "=== Starting Component Test (MASM Port Verification) ===" << std::endl;

        // 1. Test ModelRouter
        testModelRouter();

        // 2. Test ToolRegistry
        testToolRegistry();

        // 3. Test StreamingTokenManager
        testStreamingTokenManager();

        // 4. Test AgenticPlanner
        testAgenticPlanner();

        // 5. Test CommandPalette
        testCommandPalette();

        // 6. Test DiffViewer
        testDiffViewer();

        std::cout << "\n=== All Component Tests Completed ===" << std::endl;
        QCoreApplication::quit();
    }

private:
    void testModelRouter() {
        std::cout << "\n[1/6] Testing ModelRouter..." << std::endl;
        ModelRouter router;
        
        router.setMode(ModelRouter::MODE_MAX | ModelRouter::MODE_SEARCH_WEB);
        std::cout << "Mode set to MAX + SEARCH_WEB: " << (int)router.getMode() << std::endl;
        std::cout << "Primary Model: " << router.selectPrimaryModel().toStdString() << std::endl;
        
        router.toggleMode(ModelRouter::MODE_TURBO);
        std::cout << "Toggled TURBO. New Mode: " << (int)router.getMode() << std::endl;
        
        router.setFallbackPolicy(true);
        std::cout << "Fallback Policy: " << (router.getFallbackPolicy() ? "Enabled" : "Disabled") << std::endl;
        std::cout << "Fallback Model: " << router.selectFallbackModel().toStdString() << std::endl;
    }

    void testToolRegistry() {
        std::cout << "\n[2/6] Testing ToolRegistry..." << std::endl;
        ToolRegistry registry;
        registry.registerBuiltInTools();
        
        std::cout << "Registered tools: " << registry.getToolNames().join(", ").toStdString() << std::endl;
        
        // Test a tool call (mocking file_read)
        QJsonObject params;
        params["path"] = "non_existent_file.txt";
        QJsonObject result = registry.executeTool("file_read", params);
        
        std::cout << "Tool 'file_read' result (expected error): " << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    }

    void testStreamingTokenManager() {
        std::cout << "\n[3/6] Testing StreamingTokenManager..." << std::endl;
        StreamingTokenManager manager;
        
        // Simulate a call session
        manager.startCall("gpt-4");
        std::cout << "Call started. Thinking enabled: " << (manager.isThinkingEnabled() ? "Yes" : "No") << std::endl;
        
        // Simulate tokens
        manager.onToken("Hello");
        manager.onToken(" world");
        manager.onToken("!");
        
        std::cout << "Accumulated call buffer: " << manager.getCurrentCallBuffer().toStdString() << std::endl;
        
        manager.finishCall(true);
        std::cout << "Call finished." << std::endl;
    }

    void testAgenticPlanner() {
        std::cout << "\n[4/6] Testing AgenticPlanner..." << std::endl;
        ToolRegistry registry;
        ModelRouter router;
        AgenticPlanner planner(&registry, &router);
        
        connect(&planner, &AgenticPlanner::stateChanged, [](AgenticPlanner::AgentState state){
            std::cout << "  Planner State Changed: " << (int)state << std::endl;
        });
        
        connect(&planner, &AgenticPlanner::logMessage, [](const QString& msg){
            std::cout << "  Planner Log: " << msg.toStdString() << std::endl;
        });

        planner.executeTask("Fix the bug in main.cpp");
        std::cout << "Task initiated. Current State: " << (int)planner.getState() << std::endl;
    }

    void testCommandPalette() {
        std::cout << "\n[5/6] Testing CommandPalette..." << std::endl;
        CommandPalette palette;
        std::cout << "CommandPalette initialized." << std::endl;
    }

    void testDiffViewer() {
        std::cout << "\n[6/6] Testing DiffViewer..." << std::endl;
        DiffViewer viewer;
        std::cout << "DiffViewer initialized." << std::endl;
    }
};



int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    ComponentTest test;
    QTimer::singleShot(0, &test, &ComponentTest::run);
    return a.exec();
}

#include "component_test.moc"
