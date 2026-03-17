// ============================================================
// codegen_x64.h – Drop-in header for masm_solo_compiler
// Pure C++20, zero dependencies, real x64 encoding
// ============================================================
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <fstream>
#include <cstring>

namespace rawrxd {
namespace x64 {

// ─── Instruction Encoding Primitives ──────────────────────────────────────
enum class Rex : uint8_t {
    None = 0,
    W    = 0x48, // 64‑bit operand size
    R    = 0x44, // ModRM.reg extension
    X    = 0x42, // SIB.index extension  
    B    = 0x41  // ModRM.rm  extension
};

inline Rex operator|(Rex a, Rex b) { 
    return static_cast<Rex>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b)); 
}

struct ModRM {
    uint8_t mod : 2;
    uint8_t reg : 3;
    uint8_t rm  : 3;
    
    ModRM(uint8_t m, uint8_t r, uint8_t rm_) : mod(m), reg(r), rm(rm_) {}
    
    uint8_t pack() const { return (mod << 6) | (reg << 3) | rm; }
};

struct SIB {
    uint8_t scale : 2;
    uint8_t index : 3;
    uint8_t base  : 3;
    
    SIB(uint8_t s, uint8_t i, uint8_t b) : scale(s), index(i), base(b) {}
    
    uint8_t pack() const { return (scale << 6) | (index << 3) | base; }
};

// ─── Byte Stream Builder ──────────────────────────────────────────────────
class CodeBuffer {
    std::vector<uint8_t> buf;
    
public:
    void emit_u8(uint8_t b) { buf.push_back(b); }
    
    void emit_u16(uint16_t w) { 
        buf.push_back(w & 0xFF); 
        buf.push_back(w >> 8); 
    }
    
    void emit_u32(uint32_t d) {
        for(int i=0; i<4; i++) { 
            buf.push_back((d >> (i*8)) & 0xFF); 
        }
    }
    
    void emit_u64(uint64_t q) {
        for(int i=0; i<8; i++) { 
            buf.push_back((q >> (i*8)) & 0xFF); 
        }
    }
    
    void emit_rex(Rex r) { 
        if(r != Rex::None) emit_u8(static_cast<uint8_t>(r)); 
    }
    
    void emit_modrm(const ModRM& m) { emit_u8(m.pack()); }
    
    void emit_sib(const SIB& s) { emit_u8(s.pack()); }
    
    // x64 REX.W + Op + ModRM [+ SIB] [+ Disp] [+ Imm]
    void encode_rr(uint8_t opcode, uint8_t reg_dst, uint8_t reg_src, 
                   Rex prefix = Rex::W) {
        emit_rex(prefix | static_cast<Rex>((reg_src>>3)<<2) | 
                 static_cast<Rex>(reg_dst>>3));
        emit_u8(opcode);
        emit_modrm(ModRM(3, reg_dst&7, reg_src&7));
    }
    
    void encode_ri(uint8_t opcode, uint8_t reg, uint32_t imm, 
                   uint8_t imm_size = 4, Rex prefix = Rex::W) {
        emit_rex(prefix | static_cast<Rex>(reg>>3));
        emit_u8(opcode);
        emit_modrm(ModRM(3, 0, reg&7)); 
        if(imm_size == 1) emit_u8(imm);
        else if(imm_size == 4) emit_u32(imm);
        else emit_u64(imm);
    }
    
    const std::vector<uint8_t>& bytes() const { return buf; }
    size_t size() const { return buf.size(); }
    void clear() { buf.clear(); }
};

// ─── AST Node Types ───────────────────────────────────────────────────────
enum class NodeType { 
    PROGRAM, INSTRUCTION, DIRECTIVE, LABEL, REGISTER, IMMEDIATE, MEMORY, IDENTIFIER 
};

struct ASTNode {
    NodeType type;
    std::string value;
    std::vector<ASTNode> children;
    std::variant<uint64_t, std::string, int> imm_data;
};

// ─── Register Mapping ─────────────────────────────────────────────────────
static const std::unordered_map<std::string, uint8_t> reg_id = {
    {"rax",0}, {"rcx",1}, {"rdx",2}, {"rbx",3}, {"rsp",4}, {"rbp",5}, 
    {"rsi",6}, {"rdi",7}, {"r8",8}, {"r9",9}, {"r10",10}, {"r11",11}, 
    {"r12",12}, {"r13",13}, {"r14",14}, {"r15",15},
    {"eax",0}, {"ecx",1}, {"edx",2}, {"ebx",3}, {"esp",4}, {"ebp",5}, 
    {"esi",6}, {"edi",7}, {"r8d",8}, {"r9d",9}, {"r10d",10}, {"r11d",11}, 
    {"r12d",12}, {"r13d",13}, {"r14d",14}, {"r15d",15}
};

// ─── Code Generator ───────────────────────────────────────────────────────
class Generator {
    CodeBuffer code;
    
    struct Fixup { 
        size_t offset; 
        std::string label; 
    };
    
    std::vector<Fixup> fixups;
    std::unordered_map<std::string, size_t> labels;
    size_t current_rva = 0x1000;
    
public:
    void generate(const ASTNode& ast) {
        if(ast.type == NodeType::PROGRAM) {
            for(const auto& child : ast.children) {
                generate_node(child);
            }
        } else {
            generate_node(ast);
        }
    }
    
    void generate_node(const ASTNode& node) {
        switch(node.type) {
            case NodeType::INSTRUCTION: emit_instruction(node); break;
            case NodeType::LABEL:       labels[node.value] = code.size(); break;
            case NodeType::DIRECTIVE:   emit_directive(node); break;
            default: break;
        }
    }
    
    uint8_t reg_code(const std::string& name) const {
        if (name == "rax" || name == "eax" || name == "ax" || name == "al") return 0;
        if (name == "rcx" || name == "ecx" || name == "cx" || name == "cl") return 1;
        if (name == "rdx" || name == "edx" || name == "dx" || name == "dl") return 2;
        if (name == "rbx" || name == "ebx" || name == "bx" || name == "bl") return 3;
        if (name == "rsp" || name == "esp" || name == "sp") return 4;
        if (name == "rbp" || name == "ebp" || name == "bp") return 5;
        if (name == "rsi" || name == "esi" || name == "si") return 6;
        if (name == "rdi" || name == "edi" || name == "di") return 7;
        if (name == "r8" || name == "r8d") return 8;
        if (name == "r9" || name == "r9d") return 9;
        if (name == "r10" || name == "r10d") return 10;
        if (name == "r11" || name == "r11d") return 11;
        if (name == "r12" || name == "r12d") return 12;
        if (name == "r13" || name == "r13d") return 13;
        if (name == "r14" || name == "r14d") return 14;
        if (name == "r15" || name == "r15d") return 15;
        return 0;
    }
    
    bool is_x64_register(const std::string& name) const {
        return name.length() >= 2 && name[0] == 'r' && (std::isalpha(name[1]) || name[1] >= '8');
    }
    
    void emit_instruction(const ASTNode& inst) {
        const std::string& mnem = inst.value;
        auto& ops = inst.children;
        
        // NOP - simplest case
        if(mnem == "nop") {
            code.emit_u8(0x90);
            return;
        }
        
        // RET
        if(mnem == "ret" || mnem == "retn") {
            code.emit_u8(0xC3);
            return;
        }
        
        // MOV r64/r32, r64/r32
        if(mnem == "mov" && ops.size() == 2 && 
           ops[0].type == NodeType::REGISTER && ops[1].type == NodeType::REGISTER) {
            const auto& dst_name = ops[0].value;
            const auto& src_name = ops[1].value;
            bool is_64 = is_x64_register(dst_name) || is_x64_register(src_name);
            
            uint8_t dst = reg_code(dst_name);
            uint8_t src = reg_code(src_name);
            
            if(is_64) {
                code.emit_rex(Rex::W | ((src>>3) ? Rex::R : Rex::None) | 
                             ((dst>>3) ? Rex::B : Rex::None));
                code.emit_u8(0x89);
            } else {
                if(dst >= 8 || src >= 8) {
                    code.emit_u8(0x40 | ((src>>3) ? 4 : 0) | ((dst>>3) ? 1 : 0));
                }
                code.emit_u8(0x89);
            }
            code.emit_modrm(ModRM(3, src&7, dst&7));
            return;
        }
        
        // MOV r64/r32, imm64/imm32
        if(mnem == "mov" && ops.size() == 2 && 
           ops[0].type == NodeType::REGISTER && 
           ops[1].type == NodeType::IMMEDIATE) {
            const auto& dst_name = ops[0].value;
            uint8_t dst = reg_code(dst_name);
            uint64_t imm = std::get<uint64_t>(ops[1].imm_data);
            bool is_64 = is_x64_register(dst_name);
            
            if(is_64) {
                if(imm <= 0x7FFFFFFF) {
                    code.emit_u8(dst >= 8 ? 0x49 : 0x48);
                    code.emit_u8(0xC7);
                    code.emit_modrm(ModRM(3, 0, dst&7));
                    code.emit_u32(static_cast<uint32_t>(imm));
                } else {
                    code.emit_u8(dst >= 8 ? 0x49 : 0x48);
                    code.emit_u8(0xB8 + (dst&7));
                    code.emit_u64(imm);
                }
            } else {
                if(dst >= 8) code.emit_u8(0x41);
                code.emit_u8(0xB8 + (dst&7));
                code.emit_u32(static_cast<uint32_t>(imm));
            }
            return;
        }
        
        // ADD/SUB/AND/OR/XOR r64, r64
        if((mnem == "add" || mnem == "sub" || mnem == "and" || 
           mnem == "or" || mnem == "xor") && ops.size() == 2) {
            static const std::unordered_map<std::string, uint8_t> opcodes = {
                {"add", 0x01}, {"sub", 0x29}, {"and", 0x21}, 
                {"or", 0x09}, {"xor", 0x31}
            };
            uint8_t dst = reg_code(ops[0].value);
            uint8_t src = reg_code(ops[1].value);
            code.emit_rex(Rex::W | ((src>>3) ? Rex::R : Rex::None) | 
                         ((dst>>3) ? Rex::B : Rex::None));
            code.emit_u8(opcodes.at(mnem));
            code.emit_modrm(ModRM(3, src&7, dst&7));
            return;
        }
        
        // PUSH r64
        if(mnem == "push" && ops.size() == 1 && ops[0].type == NodeType::REGISTER) {
            uint8_t r = reg_code(ops[0].value);
            if(r >= 8) code.emit_u8(0x41);
            code.emit_u8(0x50 + (r&7));
            return;
        }
        
        // POP r64
        if(mnem == "pop" && ops.size() == 1 && ops[0].type == NodeType::REGISTER) {
            uint8_t r = reg_code(ops[0].value);
            if(r >= 8) code.emit_u8(0x41);
            code.emit_u8(0x58 + (r&7));
            return;
        }
        
        // JMP label
        if(mnem == "jmp" && ops.size() == 1 && ops[0].type == NodeType::IDENTIFIER) {
            code.emit_u8(0xE9);
            fixups.push_back({code.size(), ops[0].value});
            code.emit_u32(0);
            return;
        }
        
        // CALL label
        if(mnem == "call" && ops.size() == 1 && ops[0].type == NodeType::IDENTIFIER) {
            code.emit_u8(0xE8);
            fixups.push_back({code.size(), ops[0].value});
            code.emit_u32(0);
            return;
        }
        
        // INT3
        if(mnem == "int3") {
            code.emit_u8(0xCC);
            return;
        }
        
        // SYSCALL
        if(mnem == "syscall") {
            code.emit_u16(0x050F);
            return;
        }
        
        // Unknown instruction - skip silently
    }
    
    void emit_directive(const ASTNode& dir) {
        if(dir.value == ".align" && !dir.children.empty()) {
            int align = std::get<int>(dir.children[0].imm_data);
            while(code.size() % align) code.emit_u8(0x90);
        }
        else if(dir.value == ".byte" && !dir.children.empty()) {
            code.emit_u8(static_cast<uint8_t>(
                std::get<uint64_t>(dir.children[0].imm_data)));
        }
        else if(dir.value == ".qword" && !dir.children.empty()) {
            code.emit_u64(std::get<uint64_t>(dir.children[0].imm_data));
        }
    }
    
    void apply_fixups() {
        for(auto& f : fixups) {
            auto it = labels.find(f.label);
            if(it == labels.end()) {
                throw std::runtime_error("Undefined label: " + f.label);
            }
            
            int32_t rel = static_cast<int32_t>(it->second - (f.offset + 4));
            auto& b = const_cast<std::vector<uint8_t>&>(code.bytes());
            b[f.offset]     = rel & 0xFF;
            b[f.offset + 1] = (rel >> 8) & 0xFF;
            b[f.offset + 2] = (rel >> 16) & 0xFF;
            b[f.offset + 3] = (rel >> 24) & 0xFF;
        }
    }
    
    const CodeBuffer& buffer() const { return code; }
    CodeBuffer& buffer() { return code; }
};

// ─── PE Emitter ───────────────────────────────────────────────────────────
struct PEGenerator {
    static void emit(const std::string& path, const CodeBuffer& code, 
                     uint64_t entry_rva = 0x1000) {
        std::ofstream out(path, std::ios::binary);
        if(!out) throw std::runtime_error("Cannot open output");
        
        // DOS Header
        out.write("MZ", 2);
        out.seekp(0x3C);
        uint32_t pe_offset = 0x40;
        out.write(reinterpret_cast<const char*>(&pe_offset), 4);
        
        // PE Signature
        out.seekp(pe_offset);
        out.write("PE\0\0", 4);
        
        // COFF Header (x64)
        uint16_t machine = 0x8664;
        uint16_t sections = 1;
        uint32_t timestamp = 0;
        uint32_t symtab = 0;
        uint32_t symcount = 0;
        uint16_t opt_header_size = 0xF0;
        uint16_t characteristics = 0x22;
        
        out.write(reinterpret_cast<const char*>(&machine), 2);
        out.write(reinterpret_cast<const char*>(&sections), 2);
        out.write(reinterpret_cast<const char*>(&timestamp), 4);
        out.write(reinterpret_cast<const char*>(&symtab), 4);
        out.write(reinterpret_cast<const char*>(&symcount), 4);
        out.write(reinterpret_cast<const char*>(&opt_header_size), 2);
        out.write(reinterpret_cast<const char*>(&characteristics), 2);
        
        // Optional Header (PE32+)
        uint16_t magic = 0x20B;
        out.write(reinterpret_cast<const char*>(&magic), 2);
        
        uint8_t major_linker = 1, minor_linker = 0;
        out.write(reinterpret_cast<const char*>(&major_linker), 1);
        out.write(reinterpret_cast<const char*>(&minor_linker), 1);
        
        uint32_t code_size = static_cast<uint32_t>(code.size());
        out.write(reinterpret_cast<const char*>(&code_size), 4);
        
        uint32_t data_size = 0;
        out.write(reinterpret_cast<const char*>(&data_size), 4);
        out.write(reinterpret_cast<const char*>(&data_size), 4);
        
        uint32_t entry_point = static_cast<uint32_t>(entry_rva - 0x1000 + 0x200);
        out.write(reinterpret_cast<const char*>(&entry_point), 4);
        
        uint32_t base_of_code = 0x1000;
        out.write(reinterpret_cast<const char*>(&base_of_code), 4);
        
        // PE32+ fields
        uint64_t image_base = 0x140000000;
        uint32_t section_align = 0x1000;
        uint32_t file_align = 0x200;
        uint16_t major_os = 6, minor_os = 0;
        uint16_t major_img = 1, minor_img = 0;
        uint16_t major_sub = 6, minor_sub = 0;
        uint32_t win32_ver = 0;
        uint32_t image_size = 0x2000;
        uint32_t headers_size = 0x200;
        uint32_t checksum = 0;
        uint16_t subsystem = 3;
        uint16_t dll_chars = 0x8160;
        uint64_t stack_reserve = 0x100000, stack_commit = 0x1000;
        uint64_t heap_reserve = 0x100000, heap_commit = 0x1000;
        uint32_t loader_flags = 0;
        uint32_t rva_count = 16;
        
        out.write(reinterpret_cast<const char*>(&image_base), 8);
        out.write(reinterpret_cast<const char*>(&section_align), 4);
        out.write(reinterpret_cast<const char*>(&file_align), 4);
        out.write(reinterpret_cast<const char*>(&major_os), 2);
        out.write(reinterpret_cast<const char*>(&minor_os), 2);
        out.write(reinterpret_cast<const char*>(&major_img), 2);
        out.write(reinterpret_cast<const char*>(&minor_img), 2);
        out.write(reinterpret_cast<const char*>(&major_sub), 2);
        out.write(reinterpret_cast<const char*>(&minor_sub), 2);
        out.write(reinterpret_cast<const char*>(&win32_ver), 4);
        out.write(reinterpret_cast<const char*>(&image_size), 4);
        out.write(reinterpret_cast<const char*>(&headers_size), 4);
        out.write(reinterpret_cast<const char*>(&checksum), 4);
        out.write(reinterpret_cast<const char*>(&subsystem), 2);
        out.write(reinterpret_cast<const char*>(&dll_chars), 2);
        out.write(reinterpret_cast<const char*>(&stack_reserve), 8);
        out.write(reinterpret_cast<const char*>(&stack_commit), 8);
        out.write(reinterpret_cast<const char*>(&heap_reserve), 8);
        out.write(reinterpret_cast<const char*>(&heap_commit), 8);
        out.write(reinterpret_cast<const char*>(&loader_flags), 4);
        out.write(reinterpret_cast<const char*>(&rva_count), 4);
        
        // Data directories
        uint64_t zero64 = 0;
        for(int i = 0; i < 16; i++) {
            out.write(reinterpret_cast<const char*>(&zero64), 8);
        }
        
        // Section Table: .text
        char section_name[8] = {'.', 't', 'e', 'x', 't', 0, 0, 0};
        out.write(section_name, 8);
        
        uint32_t virt_size = code_size;
        uint32_t virt_addr = 0x1000;
        uint32_t raw_size = (code_size + 0x1FF) & ~0x1FF;
        uint32_t raw_ptr = 0x200;
        uint32_t relocs = 0, line_nums = 0;
        uint16_t reloc_count = 0, line_count = 0;
        uint32_t text_chars = 0x60000020;
        
        out.write(reinterpret_cast<const char*>(&virt_size), 4);
        out.write(reinterpret_cast<const char*>(&virt_addr), 4);
        out.write(reinterpret_cast<const char*>(&raw_size), 4);
        out.write(reinterpret_cast<const char*>(&raw_ptr), 4);
        out.write(reinterpret_cast<const char*>(&relocs), 4);
        out.write(reinterpret_cast<const char*>(&line_nums), 4);
        out.write(reinterpret_cast<const char*>(&reloc_count), 2);
        out.write(reinterpret_cast<const char*>(&line_count), 2);
        out.write(reinterpret_cast<const char*>(&text_chars), 4);
        
        // Padding to 0x200
        out.seekp(0x200);
        out.write(reinterpret_cast<const char*>(code.bytes().data()), 
                  code.bytes().size());
        
        while(out.tellp() % 0x200) out.put(0);
    }
};

}} // namespace rawrxd::x64
