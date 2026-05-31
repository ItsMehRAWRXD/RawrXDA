/**
 * feature_demo.cpp - Demonstration of all 50 AI IDE features
 */

#include "features/feature_connector.hpp"
#include <iostream>

using namespace rawrxd;

// Mock AI provider for demonstration
class MockAIProvider : public AIProvider {
public:
    std::future<std::string> complete(const std::string& prompt) override {
        std::promise<std::string> p;
        p.set_value("// AI generated code\nint result = 42;");
        return p.get_future();
    }

    std::future<std::string> completeStream(
        const std::string& prompt,
        std::function<void(const std::string& token)> onToken) override {
        (void)onToken;
        return complete(prompt);
    }

    std::vector<float> embed(const std::string& text) override {
        (void)text;
        return std::vector<float>(768, 0.01f);
    }

    std::string getModelName() const override { return "MockAI"; }
    uint32_t getContextLength() const override { return 4096; }
};

// Mock editor
class MockEditor : public EditorIntegration {
public:
    std::string getActiveDocument() override { return "main.cpp"; }
    std::string getSelection() override { return "selected code"; }
    void insertAtCursor(const std::string& text) override {
        std::cout << "[Editor] Inserted: " << text << "\n";
    }
};

// Mock terminal
class MockTerminal : public TerminalInterface {
public:
    std::future<std::string> execute(const std::string& command) override {
        std::promise<std::string> p;
        p.set_value("command output");
        return p.get_future();
    }
};

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       RawrXD AI IDE - 50 Feature Demonstration           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    // Create mock components
    auto provider = std::make_shared<MockAIProvider>();
    MockEditor editor;
    MockTerminal terminal;

    // ONE LINE TO CONNECT ALL 50 FEATURES
    FeatureConnector::connectAll(provider, &editor, &terminal);

    auto& registry = FeatureRegistry::instance();

    // Display all registered features
    std::cout << "=== Registered Features ===\n";
    auto features = registry.getAllFeatures();
    std::string currentCategory;
    for (const auto& f : features) {
        if (f.category != currentCategory) {
            currentCategory = f.category;
            std::cout << "\n" << currentCategory << ":\n";
        }
        std::cout << "  [" << (f.enabled ? "✓" : "✗") << "] " << f.id << " - " << f.name << "\n";
    }

    std::cout << "\n=== Feature Counts ===\n";
    std::cout << "Total: " << features.size() << " features\n";

    auto codeCompletion = registry.getFeaturesByCategory("Code Completion");
    auto codebaseIntel = registry.getFeaturesByCategory("Codebase Intelligence");
    auto agentic = registry.getFeaturesByCategory("Agentic");
    auto chat = registry.getFeaturesByCategory("Chat & Q&A");

    std::cout << "Code Completion: " << codeCompletion.size() << "\n";
    std::cout << "Codebase Intelligence: " << codebaseIntel.size() << "\n";
    std::cout << "Agentic: " << agentic.size() << "\n";
    std::cout << "Chat & Q&A: " << chat.size() << "\n";

    // Test Feature 1: Real-Time Autocomplete
    std::cout << "\n=== Testing Feature 1: Real-Time Autocomplete ===\n";
    Feature_RealTimeAutocomplete autocomplete;
    autocomplete.setProvider(provider);
    autocomplete.setEditor(&editor);
    autocomplete.onTextChanged("int main() {\n    int ", 18);
    std::cout << "Autocomplete configured\n";

    // Test Feature 5: Natural Language to Code
    std::cout << "\n=== Testing Feature 5: NL to Code ===\n";
    Feature_NaturalLanguageToCode nl2code;
    nl2code.setProvider(provider);
    std::string code = nl2code.generateFromDescription(
        "Create a function that calculates factorial", "cpp");
    std::cout << "Generated: " << code.substr(0, 50) << "...\n";

    // Test Feature 11: Codebase Index
    std::cout << "\n=== Testing Feature 11: Codebase Index ===\n";
    Feature_CodebaseIndex index;
    index.setProvider(provider);
    std::cout << "Index size: " << index.getIndexSize() << " chunks\n";

    // Test Feature 20: Composer Mode
    std::cout << "\n=== Testing Feature 20: Composer Mode ===\n";
    Feature_ComposerMode composer;
    composer.setProvider(provider);
    composer.setEditor(&editor);
    std::cout << "Composer configured\n";

    // Test Feature 29: Inline Chat
    std::cout << "\n=== Testing Feature 29: Inline Chat ===\n";
    Feature_InlineChat inlineChat;
    inlineChat.setProvider(provider);
    inlineChat.setEditor(&editor);
    inlineChat.startSession("Explain this code", "int x = 42;");
    std::cout << "Inline chat started\n";

    std::cout << "\n=== All 50 Features Connected Successfully ===\n";
    return 0;
}
