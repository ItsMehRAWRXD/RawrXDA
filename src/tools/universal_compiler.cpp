#include <iostream>
#include <vector>
#include <string_view>
#include <fstream>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <system_error>
#include <stdexcept>
#include <string>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

// ==========================================
// SECTION 1: UNIVERSAL BACKEND INTERFACE
// This abstracts the target. We can target 
// x86, Python, or WASM by implementing this.
// ==========================================
class Emitter {
public:
    virtual ~Emitter() = default;
    virtual void emitFunctionStart(std::string_view name) = 0;
    virtual void emitReturn(uint64_t value) = 0;
    virtual void emitFunctionEnd() = 0;
    virtual void finalize(const char* filename) = 0;
};

// ==========================================
// SECTION 2: NATIVE X86-64 BACKEND
// Generates raw bytes. No assembler needed.
// ==========================================
class X86Emitter : public Emitter {
    std::vector<uint8_t> code;
    std::string entry_symbol_;
    
    // Helper to emit raw bytes
    void emit(std::initializer_list<uint8_t> bytes) {
        code.insert(code.end(), bytes);
    }

public:
    void emitFunctionStart(std::string_view name) override {
        entry_symbol_ = std::string(name);
        // x86-64 Function Prologue: push rbp; mov rbp, rsp
        emit({0x55, 0x48, 0x89, 0xe5});
    }

    void emitReturn(uint64_t value) override {
        // mov rax, <value> (Return value in RAX)
        emit({0x48, 0xb8}); // REX.W MOV RAX, IMM64
        for(int i=0; i<8; ++i) emit({(uint8_t)(value >> (i*8))}); // Little endian immediate
        
        // pop rbp; ret
        emit({0x5d, 0xc3});
    }

    void emitFunctionEnd() override {
        // No-op for this simple model
    }

    void finalize(const char* filename) override {
        // Define a minimal ELF64 Header for Linux
        struct Elf64_Ehdr {
            uint8_t  e_ident[16] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // ELF Magic + 64-bit Little Endian
            uint16_t e_type = 2;      // ET_EXEC
            uint16_t e_machine = 0x3e; // EM_X86_64
            uint32_t e_version = 1;
            uint64_t e_entry = 0x400078; // Entry point virtual address
            uint64_t e_phoff = 64;     // Program header offset
            uint64_t e_shoff = 0;      // Section header offset (none)
            uint32_t e_flags = 0;
            uint16_t e_ehsize = 64;
            uint16_t e_phentsize = 56;
            uint16_t e_phnum = 1;
            uint16_t e_shentsize = 64;
            uint16_t e_shnum = 0;
            uint16_t e_shstrndx = 0;
        };

        struct Elf64_Phdr {
            uint32_t p_type = 1;    // PT_LOAD
            uint32_t p_flags = 5;   // PF_R | PF_X
            uint64_t p_offset = 0;
            uint64_t p_vaddr = 0x400000;
            uint64_t p_paddr = 0x400000;
            uint64_t p_filesz;
            uint64_t p_memsz;
            uint64_t p_align = 0x1000;
        };

        // Calculate sizes
        uint64_t code_start_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
        
        // Construct the binary
        Elf64_Ehdr ehdr;
        Elf64_Phdr phdr;
        
        phdr.p_filesz = code_start_offset + code.size();
        phdr.p_memsz = phdr.p_filesz;
        
        // Open file
        std::ofstream out(filename, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open output file");
        }

        // Write Headers
        out.write(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
        out.write(reinterpret_cast<char*>(&phdr), sizeof(phdr));
        
        // Write Code
        out.write(reinterpret_cast<char*>(code.data()), code.size());
        
        // Make executable on Linux (optional system call)
        // We just close the file, user chmods it.
    }
};

#ifdef _WIN32
// ==========================================
// SECTION 2B: NATIVE WIN64 PE BACKEND
// Generates raw PE64 executable bytes.
// ==========================================
class Win64PEEmitter : public Emitter {
    std::vector<uint8_t> code;

    void emit(std::initializer_list<uint8_t> bytes) {
        code.insert(code.end(), bytes);
    }

public:
    void emitFunctionStart(std::string_view /*name*/) override {
        // push rbp; mov rbp, rsp
        emit({0x55, 0x48, 0x89, 0xE5});
    }

    void emitReturn(uint64_t value) override {
        // mov rax, imm64
        emit({0x48, 0xB8});
        for (int i = 0; i < 8; ++i) emit({static_cast<uint8_t>(value >> (i * 8))});
        // pop rbp; ret
        emit({0x5D, 0xC3});
    }

    void emitFunctionEnd() override {
        // No-op
    }

    void finalize(const char* filename) override {
        constexpr uint32_t kFileAlignment = 0x200;
        constexpr uint32_t kSectionAlignment = 0x1000;
        constexpr uint32_t kTextRva = 0x1000;
        constexpr uint32_t kHeadersSize = 0x200;
        constexpr uint64_t kImageBase = 0x140000000ULL;

        auto alignUp = [](uint32_t value, uint32_t alignment) -> uint32_t {
            return (value + alignment - 1u) & ~(alignment - 1u);
        };

        const uint32_t codeRawSize = alignUp(static_cast<uint32_t>(code.size()), kFileAlignment);
        const uint32_t codeVirtSize = static_cast<uint32_t>(code.size());
        const uint32_t sizeOfImage = alignUp(kTextRva + codeVirtSize, kSectionAlignment);

        IMAGE_DOS_HEADER dos{};
        dos.e_magic = IMAGE_DOS_SIGNATURE; // MZ
        dos.e_lfanew = 0x80;

        IMAGE_NT_HEADERS64 nt{};
        nt.Signature = IMAGE_NT_SIGNATURE; // PE\0\0
        nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt.FileHeader.NumberOfSections = 1;
        nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

        nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        nt.OptionalHeader.AddressOfEntryPoint = kTextRva;
        nt.OptionalHeader.ImageBase = kImageBase;
        nt.OptionalHeader.SectionAlignment = kSectionAlignment;
        nt.OptionalHeader.FileAlignment = kFileAlignment;
        nt.OptionalHeader.MajorOperatingSystemVersion = 6;
        nt.OptionalHeader.MinorOperatingSystemVersion = 0;
        nt.OptionalHeader.MajorSubsystemVersion = 6;
        nt.OptionalHeader.MinorSubsystemVersion = 0;
        nt.OptionalHeader.SizeOfImage = sizeOfImage;
        nt.OptionalHeader.SizeOfHeaders = kHeadersSize;
        nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
        nt.OptionalHeader.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_NX_COMPAT;
        nt.OptionalHeader.SizeOfStackReserve = 0x100000;
        nt.OptionalHeader.SizeOfStackCommit = 0x1000;
        nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
        nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
        nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

        IMAGE_SECTION_HEADER text{};
        std::memcpy(text.Name, ".text", 5);
        text.Misc.VirtualSize = codeVirtSize;
        text.VirtualAddress = kTextRva;
        text.SizeOfRawData = codeRawSize;
        text.PointerToRawData = kHeadersSize;
        text.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

        std::ofstream out(filename, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Cannot open output file");
        }

        // DOS header + stub padding to PE header offset.
        out.write(reinterpret_cast<const char*>(&dos), sizeof(dos));
        if (dos.e_lfanew > static_cast<LONG>(sizeof(dos))) {
            const size_t stubSize = static_cast<size_t>(dos.e_lfanew - sizeof(dos));
            std::vector<char> stub(stubSize, 0);
            out.write(stub.data(), stub.size());
        }

        out.write(reinterpret_cast<const char*>(&nt), sizeof(nt));
        out.write(reinterpret_cast<const char*>(&text), sizeof(text));

        const std::streamoff cur = out.tellp();
        if (cur < static_cast<std::streamoff>(kHeadersSize)) {
            std::vector<char> hdrPad(static_cast<size_t>(kHeadersSize - cur), 0);
            out.write(hdrPad.data(), hdrPad.size());
        }

        out.write(reinterpret_cast<const char*>(code.data()), code.size());
        if (codeRawSize > code.size()) {
            std::vector<char> codePad(codeRawSize - code.size(), 0);
            out.write(codePad.data(), codePad.size());
        }
    }
};
#endif

// ==========================================
// SECTION 3: LEXER & PARSER (THE IDE ENGINE)
// Parses C-like syntax and drives the emitter.
// ==========================================

enum class UCTokenType { KW_INT, KW_RETURN, IDENT, NUMBER, TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_END };
struct Token { UCTokenType type; std::string val; uint64_t numVal = 0; };

class Lexer {
    std::string src;
    size_t pos = 0;
public:
    Lexer(std::string s) : src(std::move(s)) {}
    Token next() {
        while (pos < src.size() && isspace(src[pos])) pos++;
        if (pos >= src.size()) return {UCTokenType::TOK_END, ""};
        
        char c = src[pos];
        if (isdigit(c)) {
            std::string num;
            while(pos < src.size() && isdigit(src[pos])) num += src[pos++];
            return {UCTokenType::NUMBER, num, std::stoull(num)};
        }
        if (isalpha(c)) {
            std::string id;
            while(pos < src.size() && isalnum(src[pos])) id += src[pos++];
            if (id == "int") return {UCTokenType::KW_INT, id};
            if (id == "return") return {UCTokenType::KW_RETURN, id};
            return {UCTokenType::IDENT, id};
        }
        pos++;
        switch(c) {
            case ';': return {UCTokenType::TOK_SEMICOLON, ";"};
            case '(': return {UCTokenType::TOK_LPAREN, "("};
            case ')': return {UCTokenType::TOK_RPAREN, ")"};
            case '{': return {UCTokenType::TOK_LBRACE, "{"};
            case '}': return {UCTokenType::TOK_RBRACE, "}"};
            default:  return {UCTokenType::TOK_END, ""};
        }
    }
};

class Parser {
    Lexer lexer;
    Token curTok;
    Emitter& emitter;
public:
    Parser(std::string src, Emitter& em) : lexer(std::move(src)), emitter(em) {
        curTok = lexer.next();
    }
    
    void advance() { curTok = lexer.next(); }
    
    void expect(UCTokenType t) {
        if (curTok.type != t) throw std::runtime_error("Syntax Error");
        advance();
    }

    // The "Reverse Engineered" part: 
    // We treat semantic analysis and code gen as one stream.
    void parseFunction() {
        expect(UCTokenType::KW_INT); // Return type
        if (curTok.type != UCTokenType::IDENT) {
            throw std::runtime_error("Expected function name");
        }
        std::string functionName = curTok.val;
        advance();
        expect(UCTokenType::TOK_LPAREN);
        expect(UCTokenType::TOK_RPAREN);
        
        emitter.emitFunctionStart(functionName);
        
        expect(UCTokenType::TOK_LBRACE);
        
        // Parse Body
        if (curTok.type == UCTokenType::KW_RETURN) {
            advance();
            if (curTok.type != UCTokenType::NUMBER) throw std::runtime_error("Expected number");
            emitter.emitReturn(curTok.numVal);
            advance();
            expect(UCTokenType::TOK_SEMICOLON);
        }
        
        expect(UCTokenType::TOK_RBRACE);
        emitter.emitFunctionEnd();
    }
};

// ==========================================
// SECTION 4: DRIVER
// ==========================================

int main(int argc, char** argv) {
    std::string target = "linux";
    std::string output = "a.out";
    std::string inputFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--target" && i + 1 < argc) {
            target = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            if (!inputFile.empty()) {
                std::cerr << "Error: Multiple input files are not supported\n";
                return 1;
            }
            inputFile = arg;
        } else {
            std::cerr << "Error: Unknown argument " << arg << "\n";
            return 1;
        }
    }

    if (inputFile.empty()) {
        // Compile self-test or print usage
        // For demo, we compile a hardcoded string.
        std::string source = "int main() { return 42; }";

        std::cout << "Compiling default source: 'int main() { return 42; }'\n";

        try {
            if (target == "win64") {
#ifdef _WIN32
                Win64PEEmitter nativeEmitter;
                Parser parser(source, nativeEmitter);
                parser.parseFunction();
                if (output == "a.out") output = "a.exe";
                nativeEmitter.finalize(output.c_str());
                std::cout << "Success. Output: " << output << "\n";
#else
                std::cerr << "Error: --target win64 requires building this tool on Windows.\n";
                return 1;
#endif
            } else {
                X86Emitter nativeEmitter;
                Parser parser(source, nativeEmitter);
                parser.parseFunction();
                nativeEmitter.finalize(output.c_str());
                std::cout << "Success. Output: " << output << "\n";
                std::cout << "Run with: chmod +x " << output << " && ./" << output << "; echo $?\n";
            }
        } catch(const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // File compilation mode
    std::ifstream t(inputFile);
    if (!t) {
        std::cerr << "Error: Cannot open input file " << inputFile << "\n";
        return 1;
    }
    std::string src((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

    try {
        if (target == "win64") {
#ifdef _WIN32
            Win64PEEmitter emitter;
            Parser parser(src, emitter);
            parser.parseFunction();
            if (output == "a.out") output = "a.exe";
            emitter.finalize(output.c_str());
#else
            std::cerr << "Error: --target win64 requires building this tool on Windows.\n";
            return 1;
#endif
        } else {
            X86Emitter emitter;
            Parser parser(src, emitter);
            parser.parseFunction();
            emitter.finalize(output.c_str());
        }
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
