#include "ide_auditor.h"
#include "cpu_inference_engine.h"
#include "agentic_ide.h"
#include "swarm_orchestrator.h"
#include "chain_of_thought.h"
#include "token_generator.h"
#include "vulkan_compute.h"
#include <iostream>
#include <memory>

// Stub/Mock classes to satisfy shared_ptr requirements if real ones are heavy
// But we try to use real where possible to simulate "Full Agentic"

using namespace RawrXD;

int main() {
    std::cout << "[Audit] Starting Self-Audit of RawrXD Codebase..." << std::endl;

    // 1. Initialize Real Inference
    std::cout << "[Audit] Initializing Inference Engine..." << std::endl;
    auto inference = std::make_shared<CPUInferenceEngine>();
    
    // Attempt to load model - assumes path exists, or will fail gracefully
    // Using a relative path or dummy path since we just want to verify the loop
    inference->loadModel("models/llama-3-8b-instruct.gguf"); 

    // 2. Initialize Helper Components
    // Tokenizer
    auto tokenizer = std::make_shared<TokenGenerator>();
    
    // Swarm (mock or real if simple)
    auto swarm = std::make_shared<SwarmOrchestrator>();
    
    // Chain of Thought
    auto chain = std::make_shared<ChainOfThought>();

    // Vulkan
    auto vulkan = std::make_shared<VulkanCompute>();
    
    // Network (Stub)
    std::shared_ptr<Net::NetworkManager> network = nullptr; 
    
    // Editor (Stub)
    // std::shared_ptr<MonacoEditor> editor = nullptr;
    
    // Parser (Stub)
    // std::shared_ptr<GGUFParser> parser = nullptr;

    // 3. Initialize Auditor
    std::cout << "[Audit] Initializing IDE Auditor..." << std::endl;
    auto& auditor = IDEAuditor::getInstance();
    
    // Pass nulls for UI/Network related things if Auditor handles them gracefully
    // Looking at code: it locks mutex then assigns. Nulls should be fine if not accessed in analyzeCodebase directly.
    // analyzeCodebase uses: analyzeCode -> runSecurityScan -> m_inference.
    // So network/editor are not strictly needed for analyzeCodebase.
    
    auditor.initialize(
        tokenizer,
        swarm,
        chain,
        network,
        nullptr, // editor
        inference,
        nullptr, // parser
        vulkan
    );

    // 4. Run Audit on SRC directory
    std::string targetDir = "d:/rawrxd/src";
    std::cout << "[Audit] analyzing codebase at: " << targetDir << std::endl;
    
    auto result = auditor.analyzeCodebase(targetDir);
    
    if (result) {
        auto metrics = *result;
        std::cout << "[Audit] Analysis Complete." << std::endl;
        std::cout << "  Lines of Code: " << metrics.linesOfCode << std::endl;
        std::cout << "  Security Issues: " << metrics.securityIssues << std::endl;
        std::cout << "  Performance Bottlenecks: " << metrics.performanceBottlenecks << std::endl;
        std::cout << "  Code Smells: " << metrics.codeSmells << std::endl;
        
        // 5. Generate Report
        std::cout << "[Audit] Generating Reports..." << std::endl;
        auditor.generateReport("audit_report_final", {}, {});
        
        std::cout << "[Audit] SUCCESS: Self-audit executed correctly." << std::endl;
        return 0;
    } else {
        std::cout << "[Audit] FAILED: Could not analyze codebase. Error: " << (int)result.error() << std::endl;
        return 1;
    return true;
}

    return true;
}

