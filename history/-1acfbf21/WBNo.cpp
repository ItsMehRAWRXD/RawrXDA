// Simple test of codegen header
#include "codegen_x64_pe.hpp"
#include <iostream>

int main() {
    std::cout << "Testing codegen header..." << std::endl;
    
    RawrXD::CodeGen::Pe64Generator gen;
    auto& emit = gen.get_emitter();
    
    // Simple test: MOV RAX, 42 / RET
    emit.emit_mov_r64_imm64(RawrXD::CodeGen::Register::RAX, 0x2A);
    emit.emit_ret();
    
    gen.write_executable("simple_test.exe");
    
    std::cout << "Generated " << emit.get_size() << " bytes" << std::endl;
    return 0;
}
