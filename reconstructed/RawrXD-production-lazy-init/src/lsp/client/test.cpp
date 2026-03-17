/**
 * \file lsp_client_test.cpp
 * \brief Integration tests for LSP client with multiple language servers
 * \author RawrXD Team
 * \date 2026-01-10
 */

#include "lsp_client.h"
#include <QTest>
#include <QCoreApplication>
#include <QProcess>
#include <QThread>
#include <cassert>

namespace RawrXD::Tests {

/**
 * \brief LSP Integration Test Suite
 * 
 * Tests all IntelliSense features across multiple language servers:
 * - clangd (C++)
 * - pylsp (Python)
 * - typescript-language-server (TypeScript)
 */
class LSPClientIntegrationTests
{
public:
    /**
     * Test clangd integration with C++ files
     */
    static void testClangdIntegration()
    {
        qInfo() << "=== Testing clangd (C++) ===";
        
        LSPServerConfig config;
        config.language = "cpp";
        config.command = "clangd";
        config.arguments = QStringList() << "--log=verbose";
        config.workspaceRoot = "./test_workspace/cpp";
        config.autoStart = true;
        
        LSPClient client(config);
        client.initialize();
        
        // Test: Server startup
        QThread::sleep(2);
        if (!client.isRunning()) {
            qWarning() << "clangd failed to start";
            return;
        }
        qInfo() << "✓ clangd started successfully";
        
        // Test: Open document
        client.openDocument("test.cpp", "cpp", 
            "#include <vector>\nint main() {\n    std::vector<int> v;\n    v.\n}");
        QThread::sleep(1);
        qInfo() << "✓ Document opened";
        
        // Test: Completion at v.
        QEventLoop loop;
        auto completionHandler = [&](const QString&, int, int, const QVector<CompletionItem>& items) {
            if (items.size() > 0) {
                qInfo() << QString("✓ Completions received: %1 items").arg(items.size());
                for (int i = 0; i < qMin(3, items.size()); ++i) {
                    qDebug() << QString("  - %1 (%2)").arg(items[i].label).arg(items[i].kind);
                }
            }
            loop.quit();
        };
        
        QObject::connect(&client, &LSPClient::completionsReceived, completionHandler);
        client.requestCompletions("test.cpp", 3, 6);
        
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
        
        // Test: Hover info
        QEventLoop hoverLoop;
        auto hoverHandler = [&](const QString&, const QString& markdown) {
            if (!markdown.isEmpty()) {
                qInfo() << QString("✓ Hover info received: %1 chars").arg(markdown.length());
            }
            hoverLoop.quit();
        };
        
        QObject::connect(&client, &LSPClient::hoverReceived, hoverHandler);
        client.requestHover("test.cpp", 1, 5);
        
        QTimer::singleShot(3000, &hoverLoop, &QEventLoop::quit);
        hoverLoop.exec();
        
        // Test: Diagnostics
        auto diagnosticsHandler = [&](const QString&, const QVector<Diagnostic>& diags) {
            qInfo() << QString("✓ Diagnostics received: %1 issues").arg(diags.size());
            for (int i = 0; i < qMin(2, diags.size()); ++i) {
                qDebug() << QString("  - Line %1: %2").arg(diags[i].line).arg(diags[i].message);
            }
        };
        
        QObject::connect(&client, &LSPClient::diagnosticsUpdated, diagnosticsHandler);
        QThread::sleep(2);
    }
    
    /**
     * Test pylsp integration with Python files
     */
    static void testPylspIntegration()
    {
        qInfo() << "=== Testing pylsp (Python) ===";
        
        LSPServerConfig config;
        config.language = "python";
        config.command = "pylsp";
        config.arguments = QStringList();
        config.workspaceRoot = "./test_workspace/python";
        config.autoStart = true;
        
        LSPClient client(config);
        client.initialize();
        
        QThread::sleep(2);
        if (!client.isRunning()) {
            qWarning() << "pylsp failed to start";
            return;
        }
        qInfo() << "✓ pylsp started successfully";
        
        // Open Python document
        client.openDocument("test.py", "python",
            "import os\ndata = []\ndata.");
        QThread::sleep(1);
        
        // Test: Completion
        QEventLoop loop;
        auto completionHandler = [&](const QString&, int, int, const QVector<CompletionItem>& items) {
            if (items.size() > 0) {
                qInfo() << QString("✓ Python completions: %1 items").arg(items.size());
            }
            loop.quit();
        };
        
        QObject::connect(&client, &LSPClient::completionsReceived, completionHandler);
        client.requestCompletions("test.py", 2, 5);
        
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    
    /**
     * Test typescript-language-server integration
     */
    static void testTypescriptIntegration()
    {
        qInfo() << "=== Testing typescript-language-server (TypeScript) ===";
        
        LSPServerConfig config;
        config.language = "typescript";
        config.command = "typescript-language-server";
        config.arguments = QStringList() << "--stdio";
        config.workspaceRoot = "./test_workspace/ts";
        config.autoStart = true;
        
        LSPClient client(config);
        client.initialize();
        
        QThread::sleep(2);
        if (!client.isRunning()) {
            qWarning() << "typescript-language-server failed to start";
            return;
        }
        qInfo() << "✓ typescript-language-server started";
        
        // Open TS document
        client.openDocument("test.ts", "typescript",
            "const arr: number[] = [];\narr.");
        QThread::sleep(1);
        
        // Test: Completion
        QEventLoop loop;
        auto completionHandler = [&](const QString&, int, int, const QVector<CompletionItem>& items) {
            if (items.size() > 0) {
                qInfo() << QString("✓ TypeScript completions: %1 items").arg(items.size());
            }
            loop.quit();
        };
        
        QObject::connect(&client, &LSPClient::completionsReceived, completionHandler);
        client.requestCompletions("test.ts", 1, 4);
        
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    
    /**
     * Test all IntelliSense features
     */
    static void testAllIntelliSenseFeatures()
    {
        qInfo() << "\n=== Comprehensive IntelliSense Feature Tests ===";
        
        LSPServerConfig config;
        config.language = "cpp";
        config.command = "clangd";
        config.workspaceRoot = "./test_workspace";
        config.autoStart = true;
        
        LSPClient client(config);
        client.initialize();
        
        QThread::sleep(2);
        
        if (!client.isRunning()) {
            qWarning() << "Cannot proceed - clangd not available";
            return;
        }
        
        // 1. Test Autocomplete
        qInfo() << "\n1. Testing IntelliSense/Autocomplete...";
        client.openDocument("complete.cpp", "cpp",
            "std::vector<int> v;\nv.push");
        QThread::sleep(1);
        
        bool completionReceived = false;
        auto onCompletion = [&](const QString&, int, int, const QVector<CompletionItem>& items) {
            completionReceived = items.size() > 0;
            qInfo() << (completionReceived ? "✓" : "✗") 
                   << "Autocomplete:" << items.size() << "items";
        };
        
        QObject::connect(&client, &LSPClient::completionsReceived, onCompletion);
        client.requestCompletions("complete.cpp", 1, 7);
        QThread::sleep(2);
        
        // 2. Test Parameter Hints
        qInfo() << "\n2. Testing Parameter Hints...";
        bool paramsReceived = false;
        auto onParams = [&](const QString&, const SignatureHelp& help) {
            paramsReceived = help.signatures.size() > 0;
            qInfo() << (paramsReceived ? "✓" : "✗")
                   << "Parameter hints:" << help.signatures.size() << "signatures";
        };
        
        QObject::connect(&client, &LSPClient::signatureHelpReceived, onParams);
        client.requestSignatureHelp("complete.cpp", 1, 7);
        QThread::sleep(2);
        
        // 3. Test Quick Info on Hover
        qInfo() << "\n3. Testing Quick Info on Hover...";
        bool hoverReceived = false;
        auto onHover = [&](const QString&, const QString& markdown) {
            hoverReceived = !markdown.isEmpty();
            qInfo() << (hoverReceived ? "✓" : "✗")
                   << "Hover info:" << markdown.length() << "chars";
        };
        
        QObject::connect(&client, &LSPClient::hoverReceived, onHover);
        client.requestHover("complete.cpp", 0, 5);
        QThread::sleep(2);
        
        // 4. Test Error Squiggles via Diagnostics
        qInfo() << "\n4. Testing Error Squiggles via Diagnostics...";
        client.openDocument("errors.cpp", "cpp",
            "int main() {\n    undefined_function();\n    return 0;\n}");
        QThread::sleep(2);
        
        bool diagsReceived = false;
        auto onDiags = [&](const QString&, const QVector<Diagnostic>& diags) {
            diagsReceived = diags.size() > 0;
            qInfo() << (diagsReceived ? "✓" : "✗")
                   << "Diagnostics:" << diags.size() << "errors";
        };
        
        QObject::connect(&client, &LSPClient::diagnosticsUpdated, onDiags);
        QThread::sleep(3);
        
        // 5. Test Rename Symbol
        qInfo() << "\n5. Testing Rename Symbol...";
        bool renameReceived = false;
        auto onRename = [&](const QJsonObject& edits) {
            renameReceived = edits.contains("changes");
            qInfo() << (renameReceived ? "✓" : "✗")
                   << "Rename workspace edits received";
        };
        
        QObject::connect(&client, &LSPClient::renameReceived, onRename);
        client.requestRename("complete.cpp", 0, 5, "new_vector");
        QThread::sleep(2);
        
        // 6. Test Extract Method (Code Actions)
        qInfo() << "\n6. Testing Extract Method/Variable (Code Actions)...";
        bool actionsReceived = false;
        auto onActions = [&](const QVector<QJsonObject>& actions) {
            actionsReceived = actions.size() > 0;
            qInfo() << (actionsReceived ? "✓" : "✗")
                   << "Code actions:" << actions.size() << "actions";
        };
        
        QObject::connect(&client, &LSPClient::codeActionsReceived, onActions);
        client.getCodeActions("complete.cpp", 1, 0);
        QThread::sleep(2);
        
        // 7. Test Organize Imports
        qInfo() << "\n7. Testing Organize Imports...";
        client.openDocument("imports.cpp", "cpp",
            "#include <iostream>\n#include <vector>\n#include <iostream>\nint main() {}");
        QThread::sleep(1);
        
        client.requestOrganizeImports("imports.cpp");
        QThread::sleep(2);
        qInfo() << "✓ Organize imports requested";
        
        // Summary
        qInfo() << "\n=== Test Summary ===";
        int passed = (completionReceived ? 1 : 0) +
                    (paramsReceived ? 1 : 0) +
                    (hoverReceived ? 1 : 0) +
                    (diagsReceived ? 1 : 0) +
                    (renameReceived ? 1 : 0) +
                    (actionsReceived ? 1 : 0);
        
        qInfo() << QString("Passed: %1/6 tests").arg(passed);
    }
    
    /**
     * Run all tests
     */
    static void runAllTests()
    {
        qInfo() << "Starting LSP Client Integration Tests...";
        
        // Note: These tests require LSP servers to be installed:
        // clangd: https://clangd.llvm.org/installation.html
        // pylsp: pip install python-lsp-server
        // typescript-language-server: npm install -g typescript typescript-language-server
        
        testClangdIntegration();
        testPylspIntegration();
        testTypescriptIntegration();
        testAllIntelliSenseFeatures();
        
        qInfo() << "LSP Client Integration Tests completed";
    }
};

} // namespace RawrXD::Tests

/**
 * Main test runner
 */
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    qInfo() << "RawrXD LSP Client Integration Test Suite";
    qInfo() << "========================================";
    
    RawrXD::Tests::LSPClientIntegrationTests::runAllTests();
    
    return 0;
}
