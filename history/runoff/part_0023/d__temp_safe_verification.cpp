// Safe verification test - only arithmetric and return
#include "codegen_x64_pe.hpp"
#include <iostream>

using namespace RawrXD::CodeGen;

int main() {
    std::cout << "=== RawrXD Safe Verification Test ===" << std::endl;
    
    Pe64Generator gen;
    auto& emit = gen.get_emitter();
    
    // Test 1: MOV + RET
    std::cout << "[1] MOV RAX, 42 / RET..." << std::endl;
    emit.emit_mov_r64_imm64(Register::RAX, 42);
    emit.emit_ret();
    gen.write_executable("test1_mov.exe");
    
    // Test 2: MOV + ADD + RET
    Pe64Generator gen2;
    auto& emit2 = gen2.get_emitter();
    std::cout << "[2] MOV/ADD/RET..." << std::endl;
    emit2.emit_mov_r64_imm64(Register::RAX, 10);
    emit2.emit_mov_r64_imm64(Register::RCX, 32);
    emit2.emit_add_r64_r64(Register::RAX, Register::RCX); // RAX = 42
    emit2.emit_ret();
    gen2.write_executable("test2_add.exe");
    
    // Test 3: XOR zeroing idiom
    Pe64Generator gen3;
    auto& emit3 = gen3.get_emitter();
    std::cout << "[3] XOR EAX,EAX / RET..." << std::endl;
    emit3.emit_xor_r32_r32(Register::EAX, Register::EAX); // Zero RAX
    emit3.emit_ret();
    gen3.write_executable("test3_xor.exe");
    
    // Test 4: Full ALU showcase
    Pe64Generator gen4;
    auto& emit4 = gen4.get_emitter();
    std::cout << "[4] Full ALU test..." << std::endl;
    emit4.emit_mov_r64_imm64(Register::RAX, 100);
    emit4.emit_mov_r64_imm64(Register::RCX, 50);
    emit4.emit_sub_r64_r64(Register::RAX, Register::RCX); // RAX = 50
    emit4.emit_mov_r64_imm64(Register::RDX, 25);
    emit4.emit_add_r64_r64(Register::RAX, Register::RDX); // RAX = 75
    emit4.emit_and_r64_imm32(Register::RAX, 0x7F); // RAX = 75
    emit4.emit_ret();
    gen4.write_executable("test4_alu.exe");
    
    // Test 5: Extended registers
    Pe64Generator gen5;
    auto& emit5 = gen5.get_emitter();
    std::cout << "[5] Extended register test (R8-R15)..." << std::endl;
    emit5.emit_mov_r64_imm64(Register::R8, 20);
    emit5.emit_mov_r64_imm64(Register::R9, 22);
    emit5.emit_add_r64_r64(Register::R8, Register::R9); // R8 = 42
    emit5.emit_mov_r64_r64(Register::RAX, Register::R8);
    emit5.emit_ret();
    gen5.write_executable("test5_extended.exe");
    
    std::cout << "\n=== All tests generated! ===" << std::endl;
    return 0;
}
