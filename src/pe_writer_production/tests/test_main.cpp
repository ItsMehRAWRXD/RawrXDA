// ============================================================================
// PE Writer Production - Comprehensive Test Suite
// Tests all components and functionality
// ============================================================================

#include "../pe_writer.h"
#include "../core/pe_validator.h"
#include "../emitter/code_emitter.h"
#include "../structures/import_resolver.h"
#include "../structures/relocation_manager.h"
#include "../structures/resource_manager.h"
#include "../config/config_parser.h"
#include "../ide_integration/ide_bridge.h"

#include <iostream>
#include <fstream>
#include <cassert>

namespace pewriter {
namespace tests {

// ============================================================================
// TEST UTILITIES
// ============================================================================

class TestRunner {
public:
    TestRunner() : testsRun_(0), testsPassed_(0) {}

    void runTest(const std::string& name, std::function<bool()> test) {
        testsRun_++;
        std::cout << "Running test: " << name << "... ";

        try {
            if (test()) {
                testsPassed_++;
                std::cout << "PASSED" << std::endl;
            } else {
                std::cout << "FAILED" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
        }
    }

    void printSummary() {
        std::cout << "\nTest Summary: " << testsPassed_ << "/" << testsRun_ << " tests passed";
        if (testsPassed_ == testsRun_) {
            std::cout << " ✓" << std::endl;
        } else {
            std::cout << " ✗" << std::endl;
        }
    }

private:
    int testsRun_;
    int testsPassed_;
};

// ============================================================================
// UNIT TESTS
// ============================================================================

bool testConfigParser() {
    ConfigParser parser;

    // Test JSON parsing
    std::string json = R"(
    {
        "architecture": "x64",
        "subsystem": "WINDOWS_CUI",
        "imageBase": "0x140000000",
        "sectionAlignment": 4096,
        "fileAlignment": 512,
        "libraries": ["kernel32.dll", "user32.dll"],
        "symbols": ["ExitProcess", "MessageBoxA"]
    })";

    PEConfig config;
    bool result = parser.parseString(json, config, true);
    assert(result);
    assert(config.architecture == PEArchitecture::x64);
    assert(config.subsystem == PESubsystem::WINDOWS_CUI);
    assert(config.imageBase == 0x140000000ULL);
    assert(config.libraries.size() == 2);
    assert(config.symbols.size() == 2);

    return true;
}

bool testCodeEmitter() {
    CodeEmitter emitter;
    emitter.setArchitecture(PEArchitecture::x64);

    // Test basic instruction emission
    bool result = emitter.emitMOV_R64_IMM64(RAX, 0x123456789ABCDEF0ULL);
    assert(result);

    result = emitter.emitCALL_REL32(0x1000);
    assert(result);

    result = emitter.emitRET();
    assert(result);

    const auto& code = emitter.getCode();
    assert(code.size() > 0);

    return true;
}

bool testImportResolver() {
    ImportResolver resolver;

    resolver.setLibraries({"kernel32.dll", "user32.dll"});

    bool result = resolver.addImport("kernel32.dll", "ExitProcess");
    assert(result);

    result = resolver.addImport("user32.dll", "MessageBoxA");
    assert(result);

    result = resolver.resolve();
    assert(result);

    const auto& importTable = resolver.getImportTable();
    assert(importTable.size() > 0);

    return true;
}

bool testPEValidator() {
    PEValidator validator;

    // Test config validation
    PEConfig config;
    config.sectionAlignment = 4096;
    config.fileAlignment = 512;
    config.imageBase = 0x140000000ULL;

    bool result = validator.validateConfig(config);
    assert(result);

    // Test invalid config
    config.sectionAlignment = 1000; // Not power of 2
    result = validator.validateConfig(config);
    assert(!result);

    return true;
}

bool testPEWriterBasic() {
    PEWriter writer;

    PEConfig config;
    config.architecture = PEArchitecture::x64;
    config.subsystem = PESubsystem::WINDOWS_CUI;
    config.imageBase = 0x140000000ULL;

    bool result = writer.configure(config);
    assert(result);

    // Add a simple code section
    CodeSection codeSection;
    codeSection.name = ".text";
    codeSection.code = {0x48, 0x31, 0xC0, 0xC3}; // xor rax, rax; ret
    codeSection.executable = true;
    codeSection.readable = true;

    result = writer.addCodeSection(codeSection);
    assert(result);

    // Add kernel32 import
    result = writer.addImport({"kernel32.dll", "ExitProcess"});
    assert(result);

    // Build
    result = writer.build();
    assert(result);

    // Validate
    result = writer.validate();
    assert(result);

    // Write to file
    result = writer.writeToFile("test_output.exe");
    assert(result);

    return true;
}

bool testIDEBridge() {
    IDEBridge bridge;

    bool result = bridge.initialize();
    assert(result);

    // Test VS Code command handling
    std::string resultStr;
    result = bridge.handleVSCodeCommand("unknown_command", {}, resultStr);
    assert(!result);

    return true;
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

bool testFullPECreation() {
    PEWriter writer;

    // Configure
    PEConfig config;
    config.architecture = PEArchitecture::x64;
    config.subsystem = PESubsystem::WINDOWS_CUI;
    config.imageBase = 0x140000000ULL;
    config.enableASLR = true;
    config.enableDEP = true;

    bool result = writer.configure(config);
    assert(result);

    // Add code section with proper function
    CodeEmitter emitter;
    emitter.setArchitecture(PEArchitecture::x64);

    // Generate: sub rsp, 28h; xor rcx, rcx; call ExitProcess; add rsp, 28h; ret
    emitter.emitSUB_RSP_IMM8(0x28);
    emitter.emitMOV_RCX_IMM32(0);
    emitter.emitCALL_REL32(0); // Will be relocated
    emitter.emitADD_RSP_IMM8(0x28);
    emitter.emitRET();

    CodeSection codeSection;
    codeSection.name = ".text";
    codeSection.code = emitter.getCode();
    codeSection.executable = true;
    codeSection.readable = true;

    result = writer.addCodeSection(codeSection);
    assert(result);

    // Add imports
    result = writer.addImport({"kernel32.dll", "ExitProcess"});
    assert(result);

    // Add relocations
    RelocationEntry reloc;
    reloc.offset = 7; // Offset of call instruction
    reloc.type = IMAGE_REL_AMD64_REL32;
    reloc.symbol = "ExitProcess";
    result = writer.addRelocation(reloc);
    assert(result);

    // Build and validate
    result = writer.build();
    assert(result);

    result = writer.validate();
    assert(result);

    // Write final executable
    result = writer.writeToFile("full_test.exe");
    assert(result);

    return true;
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

bool testPerformance() {
    PEWriter writer;
    PEConfig config;

    auto start = std::chrono::high_resolution_clock::now();

    // Configure
    bool result = writer.configure(config);
    assert(result);

    // Add multiple sections
    for (int i = 0; i < 10; ++i) {
        CodeSection section;
        section.name = ".text" + std::to_string(i);
        section.code.resize(1024, 0x90); // NOPs
        result = writer.addCodeSection(section);
        assert(result);
    }

    // Add many imports
    for (int i = 0; i < 50; ++i) {
        result = writer.addImport({"library" + std::to_string(i) + ".dll",
                                  "function" + std::to_string(i)});
        assert(result);
    }

    // Build
    result = writer.build();
    assert(result);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Performance test completed in " << duration.count() << "ms";

    // Should complete in reasonable time
    assert(duration.count() < 5000); // Less than 5 seconds

    return true;
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

void runAllTests() {
    TestRunner runner;

    std::cout << "=== PE Writer Production Test Suite ===\n" << std::endl;

    // Unit tests
    runner.runTest("Config Parser", testConfigParser);
    runner.runTest("Code Emitter", testCodeEmitter);
    runner.runTest("Import Resolver", testImportResolver);
    runner.runTest("PE Validator", testPEValidator);
    runner.runTest("IDE Bridge", testIDEBridge);

    // Integration tests
    runner.runTest("Basic PE Writer", testPEWriterBasic);
    runner.runTest("Full PE Creation", testFullPECreation);

    // Performance tests
    runner.runTest("Performance Test", testPerformance);

    runner.printSummary();
}

} // namespace tests
} // namespace pewriter

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char* argv[]) {
    pewriter::tests::runAllTests();
    return 0;
}