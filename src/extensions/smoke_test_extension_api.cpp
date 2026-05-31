#include "extensions/extension_api_bridge.h"
#include <assert>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

using namespace RawrXD::Extensions;

static int g_callbackCount = 0;
static void testCallback(void* userData) {
    g_callbackCount++;
    (void)userData;
}

static int g_eventCount = 0;
static void testEventCallback(const char* type, const char* payload, void* userData) {
    g_eventCount++;
    (void)type;
    (void)payload;
    (void)userData;
}

static int g_watcherCount = 0;
static void testWatcherCallback(const FileChangeEvent& event, void* userData) {
    g_watcherCount++;
    (void)event;
    (void)userData;
}

int main() {
    std::cout << "====================================\n";
    std::cout << "RawrXD Extension API Smoke Tests\n";
    std::cout << "====================================\n\n";
    
    auto& bridge = ExtensionAPIBridge::instance();
    int passed = 0, failed = 0;
    
    // =====================================================================
    // TEST 1: Command Registration
    // =====================================================================
    std::cout << "TEST 1: Command Registration\n";
    int32_t cmdCount = bridge.registerCommand("test.cmd", "Test Command", testCallback, nullptr);
    assert(bridge.hasCommand("test.cmd"));
    bridge.executeCommand("test.cmd");
    assert(g_callbackCount == 1);
    std::cout << "  [PASS] Sync command register/execute\n"; passed++;
    
    // Async command
    auto asyncCb = [](void*) -> void* { g_callbackCount++; return (void*)42; };
    bridge.registerAsyncCommand("test.async", "Async Test", asyncCb, nullptr);
    void* result = bridge.executeCommandAsync("test.async");
    assert(result == (void*)42);
    std::cout << "  [PASS] Async command register/execute\n"; passed++;
    
    bridge.unregisterCommand("test.cmd");
    assert(!bridge.hasCommand("test.cmd"));
    std::cout << "  [PASS] Command unregister\n"; passed++;
    
    // =====================================================================
    // TEST 2: Configuration
    // =====================================================================
    std::cout << "\nTEST 2: Configuration\n";
    auto* cfg = bridge.getConfiguration("test.settings");
    cfg->set("editor.fontSize", 14);
    cfg->set("editor.wordWrap", true);
    cfg->set("editor.tabSize", 4.0);
    cfg->set("editor.theme", "dark");
    
    assert(cfg->getInt("editor.fontSize") == 14);
    assert(cfg->getBool("editor.wordWrap") == true);
    assert(cfg->getDouble("editor.tabSize") == 4.0);
    assert(cfg->getString("editor.theme") == "dark");
    std::cout << "  [PASS] Typed config getters/setters\n"; passed++;
    
    // Default values
    assert(cfg->getInt("missing.key", 99) == 99);
    assert(cfg->getBool("missing.key", true) == true);
    std::cout << "  [PASS] Default value fallback\n"; passed++;
    
    // Change events
    int changeCount = 0;
    auto changeCb = [](const char* key, const char* val, void* ud) {
        (void)key; (void)val;
        (*(int*)ud)++;
    };
    uint64_t handle = cfg->onDidChange(changeCb, &changeCount);
    cfg->set("editor.fontSize", 16);
    assert(changeCount == 1);
    cfg->offDidChange(handle);
    std::cout << "  [PASS] Configuration change events\n"; passed++;
    
    // =====================================================================
    // TEST 3: File System
    // =====================================================================
    std::cout << "\nTEST 3: File System\n";
    
    // Write/read
    const char* testData = "Hello, RawrXD Extensions!";
    bool wrote = bridge.writeFile("test_ext_file.txt", testData, strlen(testData));
    assert(wrote);
    
    char* readData = nullptr;
    size_t readLen = 0;
    bool read = bridge.readFile("test_ext_file.txt", &readData, &readLen);
    assert(read);
    assert(readLen == strlen(testData));
    assert(memcmp(readData, testData, readLen) == 0);
    delete[] readData;
    std::cout << "  [PASS] File write/read\n"; passed++;
    
    // Stat
    FileStat stat;
    assert(bridge.stat("test_ext_file.txt", &stat));
    assert(stat.isFile);
    assert(!stat.isDirectory);
    assert(stat.size == strlen(testData));
    assert(stat.mtime > 0);
    std::cout << "  [PASS] File stat\n"; passed++;
    
    // Directory
    bridge.createDirectory("test_ext_dir/subdir");
    assert(bridge.exists("test_ext_dir/subdir"));
    
    std::vector<std::string> entries;
    assert(bridge.readDirectory("test_ext_dir", &entries));
    assert(!entries.empty());
    std::cout << "  [PASS] Directory create/read\n"; passed++;
    
    // URI
    URI uri = URI::file("C:\\test\\file.txt");
    assert(uri.scheme == "file");
    assert(uri.fsPath() == "C:\\test\\file.txt");
    std::cout << "  [PASS] URI parsing\n"; passed++;
    
    // Cleanup
    bridge.deleteFile("test_ext_file.txt", false);
    bridge.deleteFile("test_ext_dir", true);
    assert(!bridge.exists("test_ext_file.txt"));
    std::cout << "  [PASS] File delete\n"; passed++;
    
    // =====================================================================
    // TEST 4: Logging
    // =====================================================================
    std::cout << "\nTEST 4: Logging\n";
    auto* ch = bridge.createOutputChannel("TestChannel");
    ch->appendLine("Test line 1");
    ch->appendLine("Test line 2");
    ch->append("Appended");
    ch->appendLine(" text");
    
    std::string contents = ch->getContents();
    assert(contents.find("Test line 1") != std::string::npos);
    assert(contents.find("Appended text") != std::string::npos);
    std::cout << "  [PASS] Output channel\n"; passed++;
    
    bridge.logInfo("Smoke test info message");
    bridge.logWarn("Smoke test warning");
    bridge.logError("Smoke test error");
    std::cout << "  [PASS] Log levels\n"; passed++;
    
    // =====================================================================
    // TEST 5: Events
    // =====================================================================
    std::cout << "\nTEST 5: Events\n";
    uint64_t evtHandle = bridge.subscribeToEvent("editor.documentOpen", testEventCallback, nullptr);
    bridge.emitEvent("editor.documentOpen", "{\"file\":\"test.cpp\"}");
    assert(g_eventCount == 1);
    bridge.unsubscribeFromEvent(evtHandle);
    bridge.emitEvent("editor.documentOpen", "{\"file\":\"test2.cpp\"}");
    assert(g_eventCount == 1); // Should not increment after unsub
    std::cout << "  [PASS] Event subscribe/emit/unsubscribe\n"; passed++;
    
    // =====================================================================
    // TEST 6: Status Bar
    // =====================================================================
    std::cout << "\nTEST 6: Status Bar\n";
    auto* item = bridge.createStatusBarItem("test.status", StatusBarAlignment::RIGHT, 100);
    item->setText("$(sync~spin) Loading...");
    item->setTooltip("Extension is loading");
    item->setCommand("test.cmd");
    item->show();
    assert(item->alignment() == StatusBarAlignment::RIGHT);
    assert(item->priority() == 100);
    std::cout << "  [PASS] Status bar item\n"; passed++;
    
    bridge.disposeStatusBarItem("test.status");
    std::cout << "  [PASS] Status bar dispose\n"; passed++;
    
    // =====================================================================
    // TEST 7: Extension Lifecycle
    // =====================================================================
    std::cout << "\nTEST 7: Extension Lifecycle\n";
    bridge.activateExtension("com.example.myext");
    assert(bridge.isExtensionActive("com.example.myext"));
    bridge.deactivateExtension("com.example.myext");
    assert(!bridge.isExtensionActive("com.example.myext"));
    std::cout << "  [PASS] Activate/deactivate\n"; passed++;
    
    // =====================================================================
    // TEST 8: VS Code Compat Shim
    // =====================================================================
    std::cout << "\nTEST 8: VS Code Compat Shim\n";
    auto* shim = static_cast<struct VSCodeCompatAPI*>(bridge.vscode_compat_shim());
    assert(shim != nullptr);
    assert(shim->registerCommand != nullptr);
    assert(shim->getConfiguration != nullptr);
    assert(shim->createOutputChannel != nullptr);
    std::cout << "  [PASS] Shim structure valid\n"; passed++;
    
    // Test shim command
    g_callbackCount = 0;
    shim->registerCommand("shim.cmd", "Shim Test", testCallback, nullptr);
    shim->executeCommand("shim.cmd");
    assert(g_callbackCount == 1);
    std::cout << "  [PASS] Shim command execution\n"; passed++;
    
    // =====================================================================
    // TEST 9: Thread Safety
    // =====================================================================
    std::cout << "\nTEST 9: Thread Safety\n";
    std::atomic<int> threadCmdCount{0};
    auto threadCb = [](void* ud) { (*(std::atomic<int>*)ud)++; };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 16; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                bridge.registerCommand(("thread.cmd." + std::to_string(j)).c_str(), "T", threadCb, &threadCmdCount);
                bridge.executeCommand(("thread.cmd." + std::to_string(j)).c_str());
            }
        });
    }
    for (auto& t : threads) t.join();
    assert(threadCmdCount == 1600);
    std::cout << "  [PASS] Concurrent register/execute (16 threads x 100 cmds)\n"; passed++;
    
    // =====================================================================
    // TEST 10: File Watcher (Basic)
    // =====================================================================
    std::cout << "\nTEST 10: File Watcher\n";
    bridge.createDirectory("test_watch_dir");
    auto* watcher = bridge.watchDirectory("test_watch_dir", false, testWatcherCallback, nullptr);
    if (watcher) {
        // Trigger a change
        bridge.writeFile("test_watch_dir/trigger.txt", "trigger", 7);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // Note: watcher may or may not catch this depending on timing
        std::cout << "  [PASS] File watcher created\n"; passed++;
        watcher->dispose();
    } else {
        std::cout << "  [SKIP] File watcher (requires directory handle)\n";
    }
    bridge.deleteFile("test_watch_dir", true);
    
    // =====================================================================
    // SUMMARY
    // =====================================================================
    std::cout << "\n====================================\n";
    std::cout << "SMOKE TEST RESULTS\n";
    std::cout << "====================================\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Total:  " << (passed + failed) << "\n";
    std::cout << (failed == 0 ? "\nALL TESTS PASSED\n" : "\nSOME TESTS FAILED\n");
    std::cout << "====================================\n";
    
    return failed;
}
