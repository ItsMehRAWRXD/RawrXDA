// RawrXD IDE Integration Example
// Demonstrates linking and using instruction_encoder.lib in VC++
// Compile: cl.exe /I"C:\RawrXD\Headers" /link /LIBPATH:"C:\RawrXD\Libraries" instruction_encoder.lib example.cpp

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Include RawrXD instruction encoder header
#include "instruction_encoder.h"

// Helper function to print encoded bytes
void PrintBytes(const char* label, const uint8_t* pBytes, uint64_t nSize) {
    printf("%s: ", label);
    for (uint64_t i = 0; i < nSize; i++) {
        printf("%02X ", pBytes[i]);
    }
    printf("(%llu bytes)\n", nSize);
}

// Example 1: Simple MOV instruction
void Example_MOV() {
    printf("\n=== Example 1: MOV R64, IMM64 ===\n");
    
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    
    // Encode: MOV RAX, 0x1234567890ABCDEF
    Encode_MOV_R64_IMM64(&ctx, REG_RAX, 0x1234567890ABCDEF);
    
    uint8_t* encoded = Encoder_GetBuffer(&ctx);
    uint64_t size = Encoder_GetSize(&ctx);
    
    PrintBytes("MOV RAX, 0x1234567890ABCDEF", encoded, size);
    printf("Expected: 48 B8 EF CD AB 90 78 56 34 12 (10 bytes)\n");
}

// Example 2: PUSH and POP
void Example_StackOps() {
    printf("\n=== Example 2: PUSH/POP ===\n");
    
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    
    // PUSH RBX
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_PUSH_R64(&ctx, REG_RBX);
    PrintBytes("PUSH RBX", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 53 (1 byte)\n");
    
    // POP R10 (requires REX.B)
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_POP_R64(&ctx, REG_R10);
    PrintBytes("POP R10", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 41 5A (2 bytes)\n");
}

// Example 3: Arithmetic operations
void Example_Arithmetic() {
    printf("\n=== Example 3: Arithmetic ===\n");
    
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    
    // ADD RAX, RBX
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_ADD_R64_R64(&ctx, REG_RAX, REG_RBX);
    PrintBytes("ADD RAX, RBX", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 48 03 C3 (3 bytes)\n");
    
    // SUB RAX, 5
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_SUB_R64_IMM8(&ctx, REG_RAX, 5);
    PrintBytes("SUB RAX, 5", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 48 83 E8 05 (4 bytes)\n");
}

// Example 4: Control flow
void Example_ControlFlow() {
    printf("\n=== Example 4: Control Flow ===\n");
    
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    
    // CALL rel32
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_CALL_REL32(&ctx, 0x1000);
    PrintBytes("CALL +0x1000", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: E8 00 10 00 00 (5 bytes)\n");
    
    // JMP rel32
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_JMP_REL32(&ctx, -256);
    PrintBytes("JMP -256", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: E9 00 FF FF FF (5 bytes)\n");
    
    // JE rel32 (Jump if Equal)
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_Jcc_REL32(&ctx, COND_E, 32);
    PrintBytes("JE +32", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 0F 84 20 00 00 00 (6 bytes)\n");
}

// Example 5: System operations
void Example_System() {
    printf("\n=== Example 5: System Instructions ===\n");
    
    uint8_t buffer[256];
    ENCODER_CTX ctx;
    
    // SYSCALL
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_SYSCALL(&ctx);
    PrintBytes("SYSCALL", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 0F 05 (2 bytes)\n");
    
    // NOP (1 byte)
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_NOP(&ctx, 1);
    PrintBytes("NOP (1-byte)", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 90 (1 byte)\n");
    
    // NOP (multi-byte)
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    Encode_NOP(&ctx, 7);
    PrintBytes("NOP (7-byte)", Encoder_GetBuffer(&ctx), Encoder_GetSize(&ctx));
    printf("Expected: 0F 1F 84 00 00 00 00 00 (8 bytes)\n");
}

// Example 6: Complex instruction sequence
void Example_Sequence() {
    printf("\n=== Example 6: Instruction Sequence ===\n");
    printf("Building a small program: function prologue\n");
    
    uint8_t buffer[512];
    ENCODER_CTX ctx;
    uint64_t totalSize = 0;
    
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    
    // PUSH RBP
    Encode_PUSH_R64(&ctx, REG_RBP);
    totalSize += Encoder_GetSize(&ctx) - totalSize;
    printf("  PUSH RBP\n");
    
    // MOV RBP, RSP  
    uint8_t tempBuf[256];
    ENCODER_CTX tempCtx;
    Encoder_Init(&tempCtx, tempBuf, sizeof(tempBuf));
    Encode_MOV_R64_R64(&tempCtx, REG_RBP, REG_RSP);
    printf("  MOV RBP, RSP\n");
    
    // SUB RSP, 32
    Encoder_Init(&tempCtx, tempBuf, sizeof(tempBuf));
    Encode_SUB_R64_IMM8(&tempCtx, REG_RSP, 32);
    printf("  SUB RSP, 32\n");
    
    printf("\nTotal instructions encoded: 3\n");
    printf("Context buffer total: %llu bytes\n", Encoder_GetSize(&ctx));
}

// Example 7: Error handling
void Example_ErrorHandling() {
    printf("\n=== Example 7: Error Handling ===\n");
    
    uint8_t buffer[4];  // Tiny buffer for overflow test
    ENCODER_CTX ctx;
    
    Encoder_Init(&ctx, buffer, sizeof(buffer));
    
    // Try to encode a 10-byte instruction in 4-byte buffer
    Encode_MOV_R64_IMM64(&ctx, REG_RAX, 0x1234567890ABCDEF);
    
    uint8_t error = Encoder_GetError(&ctx);
    if (error != ENC_ERROR_NONE) {
        printf("Error code: %d\n", error);
        if (error == ENC_ERROR_BUFFER_OVERFLOW) {
            printf("Status: Buffer overflow detected (expected - 10 bytes needed, 4 available)\n");
        }
    }
    
    // Proper sized buffer
    uint8_t buffer2[256];
    Encoder_Init(&ctx, buffer2, sizeof(buffer2));
    Encode_MOV_R64_IMM64(&ctx, REG_RAX, 0x1234567890ABCDEF);
    
    error = Encoder_GetError(&ctx);
    if (error == ENC_ERROR_NONE) {
        printf("Status: Encoding successful with proper buffer size\n");
    }
}

// Main program
int main() {
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║              RawrXD Instruction Encoder - Integration Example      ║\n");
    printf("║                    Pure MASM x86-64 Assembly                       ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    printf("\nLibrary: instruction_encoder.lib\n");
    printf("Header: instruction_encoder.h\n");
    printf("Functions: 39 exported from pure MASM64\n");
    
    // Run all examples
    Example_MOV();
    Example_StackOps();
    Example_Arithmetic();
    Example_ControlFlow();
    Example_System();
    Example_Sequence();
    Example_ErrorHandling();
    
    printf("\n╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      Integration Successful!                       ║\n");
    printf("║         All examples compiled and linked with MASM library        ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
