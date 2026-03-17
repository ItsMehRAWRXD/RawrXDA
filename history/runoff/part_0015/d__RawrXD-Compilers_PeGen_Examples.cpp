// ================================================================================
// RawrXD PE Generator - Complete Usage Examples
// ================================================================================
// Demonstrates PE generation, encoding, and output capabilities
// ================================================================================

#include "RawrXD_PE_Generator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ================================================================================
// EXAMPLE 1: Basic Executable Generation
// ================================================================================

// Simple shellcode: ExitProcess(0)
unsigned char minimal_shellcode[] = {
    0x48, 0x31, 0xC9,               // xor rcx, rcx
    0x48, 0xB8, 0x00, 0x00, 0x00,   // mov rax, ExitProcess (placeholder)
    0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xD0                      // call rax
};

void Example_BasicBuild() {
    printf("[*] Example 1: Basic Executable Generation\n");
    printf("===============================================\n\n");
    
    PEGENCTX ctx = {0};
    
    // Initialize with 1MB buffer
    printf("[*] Initializing PE generator...\n");
    if (!PeGen_Initialize(&ctx, 0x100000)) {
        printf("[-] Failed to initialize\n");
        return;
    }
    printf("[+] Initialized (Buffer: %p, Size: 0x%llX)\n", ctx.pBuffer, ctx.BufferSize);
    
    // Create DOS header
    printf("[*] Creating DOS header...\n");
    if (!PeGen_CreateDosHeader(&ctx)) {
        printf("[-] Failed to create DOS header\n");
        return;
    }
    printf("[+] DOS header created (MZ signature at offset 0)\n");
    
    // Create NT headers (console app at default base)
    printf("[*] Creating NT headers (PE32+)...\n");
    if (!PeGen_CreateNtHeaders(&ctx, 0x140000000ULL, IMAGE_SUBSYSTEM_WINDOWS_CUI)) {
        printf("[-] Failed to create NT headers\n");
        return;
    }
    printf("[+] NT headers created\n");
    printf("    ImageBase: 0x140000000\n");
    printf("    Subsystem: Console (3)\n");
    printf("    Machine: AMD64 (0x8664)\n");
    
    // Add .text section with shellcode
    printf("[*] Adding .text section...\n");
    if (!PeGen_AddSection(&ctx, ".text", 
        0x1000,                        // Virtual size
        sizeof(minimal_shellcode),     // Raw size
        SEC_CODE,                      // Code + Execute + Read
        minimal_shellcode)) {          // Data
        printf("[-] Failed to add .text section\n");
        return;
    }
    printf("[+] .text section added\n");
    printf("    VA: 0x1000, VSize: 0x1000\n");
    printf("    RawSize: %zu bytes\n", sizeof(minimal_shellcode));
    
    // Add .data section
    char message[] = "Hello from RawrXD PE Generator!\n";
    printf("[*] Adding .data section...\n");
    if (!PeGen_AddSection(&ctx, ".data",
        0x1000,
        sizeof(message),
        SEC_DATA,
        message)) {
        printf("[-] Failed to add .data section\n");
        return;
    }
    printf("[+] .data section added (%zu bytes)\n", sizeof(message));
    
    // Finalize PE
    printf("[*] Finalizing PE structure...\n");
    if (!PeGen_Finalize(&ctx, 0x1000)) {  // EP at start of .text
        printf("[-] Failed to finalize\n");
        return;
    }
    printf("[+] PE finalized\n");
    printf("    Entry point: 0x1000\n");
    printf("    Sections: %d\n", ctx.NumSections);
    
    // Cleanup
    printf("[*] Cleaning up...\n");
    PeGen_Cleanup(&ctx);
    printf("[+] Cleanup complete\n\n");
    
    printf("[SUCCESS] Basic executable generation complete!\n\n");
}

// ================================================================================
// EXAMPLE 2: Encoding with Different Methods
// ================================================================================

void Example_EncodingMethods() {
    printf("[*] Example 2: Encoding Methods Demonstration\n");
    printf("===============================================\n\n");
    
    PEGENCTX ctx = {0};
    
    printf("[*] Initializing PE generator with encoding...\n");
    if (!PeGen_Initialize(&ctx, 0x100000)) {
        printf("[-] Failed to initialize\n");
        return;
    }
    printf("[+] Initialized\n");
    printf("[+] Entropy key generated: 0x%016llX\n", ctx.EncoderKey);
    
    // Build basic PE
    PeGen_CreateDosHeader(&ctx);
    PeGen_CreateNtHeaders(&ctx, 0x140000000ULL, IMAGE_SUBSYSTEM_WINDOWS_CUI);
    
    // Add code section
    unsigned char code[] = {
        0x90, 0x90, 0x90, 0x90,  // NOP sled for demo
        0xC3                      // RET
    };
    
    if (!PeGen_AddSection(&ctx, ".code", 
        0x1000, 
        sizeof(code), 
        SEC_CODE, 
        code)) {
        printf("[-] Failed to add section\n");
        return;
    }
    printf("[+] .code section added (%zu bytes)\n", sizeof(code));
    
    // Demonstrate different encoding methods
    printf("\n[*] Available Encoding Methods:\n");
    printf("    0 = XOR (fast, simple)\n");
    printf("    1 = Rotate (add+ror)\n");
    printf("    2 = Polymorphic (triple-layer)\n\n");
    
    // Method 0: XOR
    printf("[*] Encoding section 0 with XOR...\n");
    if (!PeGen_EncodeSection(&ctx, 0, ENCODE_XOR)) {
        printf("[-] Failed to encode\n");
        return;
    }
    printf("[+] XOR encoding applied\n");
    printf("    Key: 0x%016llX\n", ctx.EncoderKey);
    printf("    Method: %d\n", ctx.EncoderMethod);
    printf("    Encoded: %s\n", ctx.IsEncoded ? "YES" : "NO");
    
    // Finalize and cleanup
    PeGen_Finalize(&ctx, 0x1000);
    PeGen_Cleanup(&ctx);
    
    printf("\n[SUCCESS] Encoding methods demonstrated!\n\n");
}

// ================================================================================
// EXAMPLE 3: Multiple Sections
// ================================================================================

void Example_MultipleSections() {
    printf("[*] Example 3: Multiple Sections\n");
    printf("===============================================\n\n");
    
    PEGENCTX ctx = {0};
    
    if (!PeGen_Initialize(&ctx, 0x200000)) {
        printf("[-] Initialization failed\n");
        return;
    }
    
    PeGen_CreateDosHeader(&ctx);
    PeGen_CreateNtHeaders(&ctx, 0x140000000ULL, IMAGE_SUBSYSTEM_WINDOWS_CUI);
    
    // .text section (code)
    unsigned char code[] = { 0x90, 0xC3 };
    printf("[*] Adding .text section (code)...\n");
    if (!PeGen_AddSection(&ctx, ".text", 0x1000, sizeof(code), SEC_CODE, code)) {
        printf("[-] Failed\n");
        return;
    }
    printf("[+] .text added\n");
    
    // .data section (initialized data)
    unsigned char data[] = { 0x00, 0x01, 0x02, 0x03 };
    printf("[*] Adding .data section (initialized)...\n");
    if (!PeGen_AddSection(&ctx, ".data", 0x1000, sizeof(data), SEC_DATA, data)) {
        printf("[-] Failed\n");
        return;
    }
    printf("[+] .data added\n");
    
    // .rdata section (read-only data)
    const char* string = "Read-only string";
    printf("[*] Adding .rdata section (read-only)...\n");
    if (!PeGen_AddSection(&ctx, ".rdata", 0x1000, strlen(string), SEC_RDATA, (void*)string)) {
        printf("[-] Failed\n");
        return;
    }
    printf("[+] .rdata added\n");
    
    printf("\n[*] Section Summary:\n");
    printf("    Total sections: %d\n", ctx.NumSections);
    printf("    PE ready for output\n\n");
    
    PeGen_Finalize(&ctx, 0x1000);
    PeGen_Cleanup(&ctx);
    
    printf("[SUCCESS] Multi-section example complete!\n\n");
}

// ================================================================================
// EXAMPLE 4: Using Quick Builder
// ================================================================================

void Example_QuickBuilder() {
    printf("[*] Example 4: Quick Builder Helper\n");
    printf("===============================================\n\n");
    
    PEGENCTX ctx = {0};
    
    // Simple code and data
    unsigned char code[] = { 0x90, 0x90, 0xC3 };
    unsigned char data[] = { 'H', 'i', '!' };
    
    printf("[*] Using PeGen_BuildExecutable helper...\n");
    printf("    Code: %d bytes\n", sizeof(code));
    printf("    Data: %d bytes\n", sizeof(data));
    printf("    ImageBase: 0x140000000\n");
    printf("    Subsystem: Console\n");
    printf("    Entry Point: 0x1000\n\n");
    
    if (!PeGen_BuildExecutable(&ctx, "quick_test.exe", code, sizeof(code), data, sizeof(data))) {
        printf("[-] Quick build failed\n");
        return;
    }
    
    printf("[+] PE structure created\n");
    printf("[+] Sections: %d\n", ctx.NumSections);
    printf("[+] Ready for deployment\n\n");
    
    PeGen_Cleanup(&ctx);
    
    printf("[SUCCESS] Quick builder example complete!\n\n");
}

// ================================================================================
// MAIN
// ================================================================================

int main(int argc, char* argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     RawrXD PE Generator - Complete Usage Examples         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    // Run examples
    Example_BasicBuild();
    Example_EncodingMethods();
    Example_MultipleSections();
    Example_QuickBuilder();
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                 All Examples Complete!                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    return 0;
}

// ================================================================================
// ADVANCED: Custom Encoding Pipeline
// ================================================================================

void AdvancedExample_CustomEncoding() {
    printf("[*] Advanced Example: Custom Encoding Pipeline\n\n");
    
    PEGENCTX ctx = {0};
    PeGen_Initialize(&ctx, 0x100000);
    PeGen_CreateDosHeader(&ctx);
    PeGen_CreateNtHeaders(&ctx, 0x140000000ULL, IMAGE_SUBSYSTEM_WINDOWS_CUI);
    
    // Add payload
    unsigned char payload[] = {
        0x55, 0x48, 0x89, 0xE5,  // push rbp; mov rbp, rsp
        0x48, 0x83, 0xEC, 0x20,  // sub rsp, 0x20
        0xC9, 0xC3               // leave; ret
    };
    
    if (!PeGen_AddSection(&ctx, ".payload", 0x1000, sizeof(payload), SEC_CODE, payload)) {
        printf("[-] Add section failed\n");
        return;
    }
    
    printf("[+] Payload section added (%zu bytes)\n", sizeof(payload));
    printf("[+] Payload key: 0x%016llX\n", ctx.EncoderKey);
    
    // Apply polymorphic encoding (strongest)
    printf("[*] Applying polymorphic encoding...\n");
    if (!PeGen_EncodeSection(&ctx, 0, ENCODE_POLYMORPHIC)) {
        printf("[-] Encoding failed\n");
        return;
    }
    printf("[+] Polymorphic encoding complete\n");
    printf("[+] Encoded with method: %d (polymorphic)\n", ctx.EncoderMethod);
    
    PeGen_Finalize(&ctx, 0x1000);
    PeGen_Cleanup(&ctx);
    
    printf("\n[SUCCESS] Custom encoding pipeline complete!\n\n");
}
