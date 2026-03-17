#include "engine.hpp"
#include "ui/window.hpp"
#include <iostream>

Engine::Engine() : hInstance_(nullptr) {}
Engine::~Engine() {}

bool Engine::initialize(HINSTANCE hInst) {
    hInstance_ = hInst;
    
    // Load configuration
    loadConfiguration();
    
    // Initialize agentic system
    initializeAgenticSystem();
    
    // Create main window
    MainWindow mainWindow(hInstance_);
    if (!mainWindow.create(L"RawrXD MASM IDE", 1200, 800)) {
        std::cerr << "Failed to create main window" << std::endl;
        return false;
    }
    
    std::cout << "Engine initialized successfully" << std::endl;
    return true;
}

int Engine::run() {
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void Engine::executeWish(const std::string& wish) {
    std::cout << "Executing wish: " << wish << std::endl;
    
    IDEAgentBridge::ExecutionMode mode = IDEAgentBridge::ExecutionMode::MANUAL_APPROVAL;
    if (configManager_.getString("agent.execution_mode") == "auto") {
        mode = IDEAgentBridge::ExecutionMode::AUTO_EXECUTE;
    } else if (configManager_.getString("agent.execution_mode") == "dry_run") {
        mode = IDEAgentBridge::ExecutionMode::DRY_RUN;
    }
    
    IDEAgentBridge::WishResult result = agentBridge_.executeWish(wish, mode);
    
    if (result.success) {
        std::cout << "Wish execution successful: " << result.finalOutput << std::endl;
    } else {
        std::cerr << "Wish execution failed: " << result.finalOutput << std::endl;
    }
}

void Engine::initializeAgenticSystem() {
    if (!agentBridge_.initialize()) {
        std::cerr << "Failed to initialize agentic bridge" << std::endl;
        return;
    }
    
    // Set up callbacks
    agentBridge_.setProgressCallback([](int progress, const std::string& message) {
        std::cout << "Progress: " << progress << "% - " << message << std::endl;
    });
    
    agentBridge_.setCompletionCallback([](const IDEAgentBridge::WishResult& result) {
        std::cout << "Wish completed: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
    });
    
    std::cout << "Agentic system initialized" << std::endl;
}

void Engine::loadConfiguration() {
    if (!configManager_.load()) {
        std::cout << "Using default configuration" << std::endl;
    } else {
        std::cout << "Configuration loaded successfully" << std::endl;
    }
}