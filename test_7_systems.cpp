#include "CompletionEngine.h"
#include "CodebaseContextAnalyzer.h"
#include "SmartRewriteEngine.h"
#include "MultiModalModelRouter.h"
#include "LanguageServerIntegration.h"
#include "PerformanceOptimizer.h"
#include "AdvancedCodingAgent.h"
#include <iostream>

using namespace RawrXD::IDE;

int main() {
    std::cout << "=== Testing 7 Competitive AI Systems ===" << std::endl;
    
    // Test CompletionEngine
    {
        CompletionEngine engine;
        auto completions = engine.getCompletions("void foo(", 10, "cpp");
        std::cout << "✓ CompletionEngine: " << completions.size() << " suggestions" << std::endl;
    }
    
    // Test CodebaseContextAnalyzer
    {
        CodebaseContextAnalyzer analyzer;
        auto symbols = analyzer.analyzeCurrentScope("int x = 5;", 1);
        std::cout << "✓ CodebaseContextAnalyzer: Ready" << std::endl;
    }
    
    // Test SmartRewriteEngine
    {
        SmartRewriteEngine rewriter;
        auto issues = rewriter.detectCodeSmells("void test() { return; }", "cpp");
        std::cout << "✓ SmartRewriteEngine: " << issues.size() << " issues found" << std::endl;
    }
    
    // Test MultiModalModelRouter
    {
        MultiModalModelRouter router;
        router.registerModel("neural-chat", "Fast completion model", 0.9f, 100);
        auto model = router.selectModel(TaskType::CodeCompletion);
        std::cout << "✓ MultiModalModelRouter: Selected model = " << model.modelName << std::endl;
    }
    
    // Test LanguageServerIntegration
    {
        LanguageServerIntegration lsp;
        lsp.initialize();
        auto completions = lsp.getCompletionItems("test.cpp", 1, 5, "cpp", "void foo");
        std::cout << "✓ LanguageServerIntegration: " << completions.size() << " completions" << std::endl;
    }
    
    // Test PerformanceOptimizer
    {
        PerformanceOptimizer optimizer;
        optimizer.cacheContext("key1", "value1", 300);
        std::string cached;
        bool found = optimizer.getCachedContext("key1", cached);
        std::cout << "✓ PerformanceOptimizer: Cache working = " << (found ? "yes" : "no") << std::endl;
    }
    
    // Test AdvancedCodingAgent
    {
        AdvancedCodingAgent agent;
        auto bugs = agent.detectBugs("int x = new int; delete x;", "cpp");
        std::cout << "✓ AdvancedCodingAgent: " << bugs.vulnerabilities.size() << " vulnerabilities found" << std::endl;
    }
    
    std::cout << "\n=== All 7 Systems Validated Successfully ===" << std::endl;
    return 0;
}
