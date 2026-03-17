#include "pe_writer.h"
#include <iostream>
#include <fstream>

int main() {
    try {
        // Create PE writer instance
        PEWriter writer;

        // Configure basic PE settings
        PEConfig config;
        config.architecture = PEArchitecture::AMD64;
        config.subsystem = PESubsystem::WINDOWS_CUI;
        config.entryPointRVA = 0x1000;  // Code section starts at 0x1000
        config.imageBase = 0x400000;   // Standard Windows image base
        config.sectionAlignment = 0x1000;
        config.fileAlignment = 0x200;

        writer.configure(config);

        // Add a simple code section with "Hello World" message box
        std::vector<uint8_t> code = {
            // Function prologue
            0x48, 0x89, 0x4C, 0x24, 0x08,           // mov [rsp+8], rcx
            0x48, 0x83, 0xEC, 0x28,                 // sub rsp, 0x28

            // Load "user32.dll" and "MessageBoxA"
            0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, // mov rax, [rip+reloc_user32]
            0x48, 0x8B, 0x00,                         // mov rax, [rax]
            0x48, 0x8B, 0x40, 0x08,                   // mov rax, [rax+8] (MessageBoxA)

            // Call MessageBoxA(NULL, "Hello World!", "PE Writer Demo", MB_OK)
            0x48, 0x31, 0xC9,                         // xor rcx, rcx
            0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00, // lea rdx, [rip+reloc_hello]
            0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00, // lea r8, [rip+reloc_title]
            0x45, 0x31, 0xC9,                         // xor r9d, r9d
            0xFF, 0xD0,                               // call rax

            // Function epilogue
            0x48, 0x83, 0xC4, 0x28,                 // add rsp, 0x28
            0xC3                                      // ret
        };

        writer.addCodeSection("text", code, 0x1000);

        // Add data section with strings
        std::vector<uint8_t> data = {
            // "Hello World!" string
            'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', 0,
            // "PE Writer Demo" string
            'P', 'E', ' ', 'W', 'r', 'i', 't', 'e', 'r', ' ', 'D', 'e', 'm', 'o', 0
        };

        writer.addDataSection("rdata", data, 0x2000);

        // Add imports
        writer.addImport("user32.dll", "MessageBoxA");

        // Build the PE file
        std::vector<uint8_t> peData = writer.build();

        // Validate the PE file
        if (writer.validate(peData)) {
            std::cout << "PE file validation successful!" << std::endl;
        } else {
            std::cout << "PE file validation failed!" << std::endl;
            return 1;
        }

        // Write to file
        std::ofstream outFile("hello_world.exe", std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(peData.data()), peData.size());
        outFile.close();

        std::cout << "PE file written to hello_world.exe" << std::endl;
        std::cout << "File size: " << peData.size() << " bytes" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}