// Unit tests for PE, ELF, and Mach-O writers

#include "masm/pe_writer.h"
#include "masm/elf_writer.h"
#include "masm/mach_o_writer.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

TEST(PEWriterTest, BasicPE) {
    PEWriter writer;
    writer.set_architecture(0x8664); // x64
    std::vector<uint8_t> code_section_data = { 0x48, 0x83, 0xEC, 0x28 }; // sub rsp, 0x28
    writer.add_section(".text", code_section_data, 0x60000020); // Code, Executable, Readable
    
    std::vector<uint8_t> pe_file = writer.write();
    
    ASSERT_FALSE(pe_file.empty());
    // Check for MZ header
    ASSERT_EQ(pe_file[0], 'M');
    ASSERT_EQ(pe_file[1], 'Z');
    // Check for PE signature
    uint32_t pe_offset = *(uint32_t*)&pe_file[0x3c];
    ASSERT_EQ(pe_file[pe_offset], 'P');
    ASSERT_EQ(pe_file[pe_offset+1], 'E');
}

TEST(ELFWriterTest, BasicELF) {
    ELFWriter writer;
    writer.set_architecture(0x3E); // x86-64
    std::vector<uint8_t> code_section_data = { 0x48, 0x83, 0xEC, 0x28 }; // sub rsp, 0x28
    writer.add_section(".text", code_section_data, 1, 6); // SHT_PROGBITS, ALLOC | EXECINSTR
    
    std::vector<uint8_t> elf_file = writer.write();
    
    ASSERT_FALSE(elf_file.empty());
    // Check for ELF header
    ASSERT_EQ(elf_file[0], 0x7f);
    ASSERT_EQ(elf_file[1], 'E');
    ASSERT_EQ(elf_file[2], 'L');
    ASSERT_EQ(elf_file[3], 'F');
}

TEST(MachOWriterTest, BasicMachO) {
    MachOWriter writer;
    writer.set_architecture(0x1000007, 3); // CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL
    std::vector<uint8_t> code_section_data = { 0x48, 0x83, 0xEC, 0x28 }; // sub rsp, 0x28
    writer.add_section("__text", "__TEXT", code_section_data);
    
    std::vector<uint8_t> macho_file = writer.write();
    
    ASSERT_FALSE(macho_file.empty());
    // Check for Mach-O header
    uint32_t magic = *(uint32_t*)macho_file.data();
    ASSERT_EQ(magic, 0xfeedfacf); // MH_MAGIC_64
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
