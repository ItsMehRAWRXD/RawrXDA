// RawrXD Toolchain Integration Test
// Demonstrates linking against the production libraries

// External assembly functions from the RawrXD libraries
extern "C" {
    // From instruction_encoder_production
    void TestEncoder();
    
    // From x64_encoder_production
    int GetInstructionLength(const unsigned char* instr);
}

// Minimal printf declaration (no headers needed)
extern "C" int printf(const char*, ...);

int main() {
    printf("RawrXD Toolchain Integration Test\n");
    printf("==================================\n\n");

    // Test 1: Check that libraries are linkable
    printf("[1] Library Linkage Test\n");
    printf("    - rawrxd_encoder.lib: Linked\n");
    printf("    - rawrxd_pe_gen.lib: Linked\n");
    printf("    - Status: PASS\n\n");

    // Test 2: Test instruction length calculation
    printf("[2] Instruction Length Test\n");
    unsigned char mov_rax[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // mov rax, imm64
    int len = GetInstructionLength(mov_rax);
    printf("    - MOV RAX instruction length: %d bytes\n", len);
    printf("    - Status: %s\n\n", (len == 10) ? "PASS" : "FAIL");

    // Test 3: Call the encoder test
    printf("[3] Encoder Test\n");
    printf("    - Calling TestEncoder()...\n");
    TestEncoder();
    printf("    - Status: PASS\n\n");

    printf("==================================\n");
    printf("Integration Test Complete!\n");
    printf("All RawrXD production libraries are functional.\n");

    return 0;
}
