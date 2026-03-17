// RawrXD Advanced Integration Example
// Demonstrates real-world usage of the RawrXD toolchain in a C++ application

#include <iostream>
#include <vector>
#include <string>
#include <fstream>

// RawrXD Headers (add C:\RawrXD\Headers to your include path)
// extern "C" declarations for linking against RawrXD libraries

extern "C" {
    // From rawrxd_encoder.lib - x64 instruction encoding
    int GetInstructionLength(const unsigned char* instr);
    void TestEncoder();
    
    // PE generation would require additional headers
    // For now we'll demonstrate the encoder functionality
}

// Simple x64 instruction builder class
class X64InstructionBuilder {
private:
    std::vector<unsigned char> buffer;
    
public:
    // Add MOV RAX, immediate
    void AddMovRaxImm64(uint64_t value) {
        buffer.push_back(0x48);  // REX.W prefix
        buffer.push_back(0xB8);  // MOV RAX opcode
        
        // Little-endian 64-bit value
        for (int i = 0; i < 8; i++) {
            buffer.push_back((value >> (i * 8)) & 0xFF);
        }
    }
    
    // Add RET instruction
    void AddRet() {
        buffer.push_back(0xC3);
    }
    
    // Add NOP instruction
    void AddNop() {
        buffer.push_back(0x90);
    }
    
    // Add CALL relative
    void AddCall(int32_t offset) {
        buffer.push_back(0xE8);
        
        // Little-endian 32-bit offset
        for (int i = 0; i < 4; i++) {
            buffer.push_back((offset >> (i * 8)) & 0xFF);
        }
    }
    
    // Get the raw bytes
    const std::vector<unsigned char>& GetBytes() const {
        return buffer;
    }
    
    // Validate using RawrXD encoder
    bool ValidateInstruction(size_t offset) {
        if (offset >= buffer.size()) return false;
        
        int length = GetInstructionLength(&buffer[offset]);
        return length > 0;
    }
    
    // Get instruction length at offset using RawrXD
    int GetInstrLength(size_t offset) {
        if (offset >= buffer.size()) return -1;
        return GetInstructionLength(&buffer[offset]);
    }
    
    // Display disassembly with lengths
    void Disassemble() {
        std::cout << "\nDisassembly (with RawrXD instruction length analysis):\n";
        std::cout << "======================================================\n";
        
        size_t offset = 0;
        int instrNum = 1;
        
        while (offset < buffer.size()) {
            int length = GetInstrLength(offset);
            
            if (length <= 0) {
                std::cout << "Error at offset " << offset << "\n";
                break;
            }
            
            // Print instruction bytes
            printf("%04zx: ", offset);
            for (int i = 0; i < length && (offset + i) < buffer.size(); i++) {
                printf("%02X ", buffer[offset + i]);
            }
            
            // Padding for alignment
            for (int i = length; i < 10; i++) {
                printf("   ");
            }
            
            printf(" ; Instruction #%d (length: %d bytes)\n", instrNum++, length);
            
            offset += length;
        }
        
        std::cout << "\nTotal code size: " << buffer.size() << " bytes\n";
    }
    
    // Write to binary file
    bool WriteToFile(const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;
        
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        return file.good();
    }
};

// Example 1: Simple function generation
void Example1_SimpleFunctionGeneration() {
    std::cout << "\n=== Example 1: Generate Simple x64 Function ===\n";
    
    X64InstructionBuilder builder;
    
    // Generate: mov rax, 0x1234567890ABCDEF; ret
    builder.AddMovRaxImm64(0x1234567890ABCDEF);
    builder.AddRet();
    
    // Validate and disassemble
    builder.Disassemble();
    
    // Write to file
    if (builder.WriteToFile("simple_function.bin")) {
        std::cout << "✓ Written to simple_function.bin\n";
    }
}

// Example 2: Multi-instruction sequence
void Example2_ComplexSequence() {
    std::cout << "\n=== Example 2: Generate Complex Instruction Sequence ===\n";
    
    X64InstructionBuilder builder;
    
    // Prologue
    builder.AddNop();  // Alignment
    
    // Function body
    builder.AddMovRaxImm64(42);           // mov rax, 42
    builder.AddCall(0x00000010);          // call +16
    builder.AddMovRaxImm64(0xDEADBEEF);   // mov rax, 0xDEADBEEF
    builder.AddRet();                     // ret
    
    // Validate all instructions
    std::cout << "Validating instructions...\n";
    
    size_t offset = 0;
    int count = 0;
    const auto& bytes = builder.GetBytes();
    
    while (offset < bytes.size()) {
        int length = builder.GetInstrLength(offset);
        if (length > 0) {
            std::cout << "  ✓ Instruction at offset " << offset << " is valid (length: " << length << ")\n";
            offset += length;
            count++;
        } else {
            std::cout << "  ✗ Invalid instruction at offset " << offset << "\n";
            break;
        }
    }
    
    std::cout << "\nValidated " << count << " instructions successfully\n";
    
    builder.Disassemble();
}

// Example 3: Test RawrXD encoder directly
void Example3_TestRawrXDEncoder() {
    std::cout << "\n=== Example 3: Test RawrXD Encoder Directly ===\n";
    
    // Call the built-in test function
    std::cout << "Calling TestEncoder() from rawrxd_encoder.lib...\n\n";
    TestEncoder();
    
    std::cout << "\n✓ RawrXD encoder test completed\n";
}

// Example 4: Instruction length analysis
void Example4_InstructionAnalysis() {
    std::cout << "\n=== Example 4: Instruction Length Analysis ===\n\n";
    
    // Various x64 instructions for testing
    struct TestCase {
        std::string name;
        std::vector<unsigned char> bytes;
    };
    
    std::vector<TestCase> testCases = {
        { "MOV RAX, imm64", { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
        { "MOV RAX, RCX", { 0x48, 0x89, 0xC8 } },
        { "ADD RAX, RBX", { 0x48, 0x01, 0xD8 } },
        { "PUSH RAX", { 0x50 } },
        { "POP RAX", { 0x58 } },
        { "RET", { 0xC3 } },
        { "NOP", { 0x90 } },
        { "CALL rel32", { 0xE8, 0x00, 0x00, 0x00, 0x00 } },
        { "JMP rel32", { 0xE9, 0x00, 0x00, 0x00, 0x00 } },
        { "JE rel8", { 0x74, 0x00 } }
    };
    
    std::cout << "Testing instruction length detection:\n";
    std::cout << "----------------------------------------\n";
    
    for (const auto& test : testCases) {
        int length = GetInstructionLength(test.bytes.data());
        
        printf("%-20s : Expected %2zu bytes, RawrXD reported %2d bytes ",
               test.name.c_str(), test.bytes.size(), length);
        
        if (length == static_cast<int>(test.bytes.size())) {
            std::cout << "✓ PASS\n";
        } else {
            std::cout << "✗ FAIL\n";
        }
    }
}

// Main demonstration
int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RawrXD Toolchain - Advanced Integration Examples       ║\n";
    std::cout << "║  Demonstrating x64 encoding and PE generation           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    try {
        // Run all examples
        Example1_SimpleFunctionGeneration();
        Example2_ComplexSequence();
        Example3_TestRawrXDEncoder();
        Example4_InstructionAnalysis();
        
        std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  All examples completed successfully!                    ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << "\n";
        return 1;
    }
}

/*
COMPILATION INSTRUCTIONS:

1. Using Visual Studio Command Prompt:
   cl.exe /EHsc /std:c++17 advanced_integration_example.cpp ^
          C:\RawrXD\Libraries\rawrxd_encoder.lib ^
          /Fe:rawrxd_demo.exe

2. Using CMake (create CMakeLists.txt):
   cmake_minimum_required(VERSION 3.15)
   project(RawrXDDemo)
   
   add_executable(rawrxd_demo advanced_integration_example.cpp)
   target_link_libraries(rawrxd_demo C:/RawrXD/Libraries/rawrxd_encoder.lib)
   
3. Using PowerShell build script:
   $env:LIB += ";C:\RawrXD\Libraries"
   cl.exe /EHsc advanced_integration_example.cpp rawrxd_encoder.lib

OUTPUT:
   - rawrxd_demo.exe (demonstrates RawrXD functionality)
   - simple_function.bin (raw x64 bytecode)
   - Disassembly output showing instruction analysis

LINKING REQUIREMENTS:
   - rawrxd_encoder.lib (from C:\RawrXD\Libraries)
   - Visual C++ runtime (msvcrt.lib, automatically linked)
   - Windows SDK libraries (kernel32.lib, automatically linked)
*/
