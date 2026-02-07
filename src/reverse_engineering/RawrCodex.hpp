#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstring>
#include <iomanip>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// RAWR CODEX - Advanced Binary Analysis & Reverse Engineering Suite
// ============================================================================

struct Symbol {
    std::string name;
    uint64_t address;
    uint64_t size;
    std::string section;
    std::string type; // "function", "data", "import", "export"
    bool isMangled;
    std::string demangledName;
};

struct Section {
    std::string name;
    uint64_t virtualAddress;
    uint64_t virtualSize;
    uint64_t rawSize;
    uint64_t rawOffset;
    uint32_t characteristics;
    std::vector<uint8_t> data;
};

struct Import {
    std::string moduleName;
    std::string functionName;
    uint64_t address;
    uint16_t ordinal;
};

struct Export {
    std::string name;
    uint64_t address;
    uint16_t ordinal;
    bool isForwarder;
    std::string forwarderName;
};

struct DisassemblyLine {
    uint64_t address;
    std::vector<uint8_t> bytes;
    std::string mnemonic;
    std::string operands;
    std::string comment;
};

// ============================================================================
// SSA (Static Single Assignment) Structures
// ============================================================================

enum class SSAOpType : uint32_t {
    Assign = 0,     // dst = src1
    Add,            // dst = src1 + src2
    Sub,            // dst = src1 - src2
    Mul,            // dst = src1 * src2
    Div,            // dst = src1 / src2
    And,            // dst = src1 & src2
    Or,             // dst = src1 | src2
    Xor,            // dst = src1 ^ src2
    Shl,            // dst = src1 << src2
    Shr,            // dst = src1 >> src2
    Sar,            // dst = src1 >>> src2 (arithmetic)
    Not,            // dst = ~src1
    Neg,            // dst = -src1
    Load,           // dst = [src1 + offset]
    Store,          // [dst + offset] = src1
    Call,           // dst = call target(args...)
    Ret,            // return src1
    Cmp,            // flags = cmp(src1, src2)
    Test,           // flags = test(src1, src2)
    Branch,         // conditional branch on flags
    Jmp,            // unconditional jump
    Phi,            // PHI node merge
    Lea,            // dst = &(base + index*scale + disp)
    Unknown         // Unrecognized / fallback
};

enum class SSAVarClass : uint32_t {
    Register = 0,   // Machine register (renamed to unique SSA version)
    Temp,           // Compiler-generated temporary
    Memory,         // Memory location reference
    Immediate,      // Constant / literal value
    Flags           // CPU flags pseudo-register
};

// ============================================================================
// Phase 17: Type Recovery + Data Flow Analysis + License Enforcement
// ============================================================================

// Recovered type classification
enum class RecoveredType : uint32_t {
    Unknown = 0,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Float32, Float64,
    Pointer,        // Generic void*
    CodePtr,        // Function pointer (CALL target)
    DataPtr,        // Data pointer (LOAD/STORE through)
    StructPtr,      // Struct pointer (multiple field offsets)
    ArrayPtr,       // Array pointer (LEA with scale)
    Bool,           // Boolean (TEST+Jcc pattern)
    Flags,          // CPU flags (internal)
    StringPtr,      // Pointer to string (heuristic)
    Void            // void / no value
};

// Type inference confidence
enum class TypeConfidence : uint32_t {
    None = 0,
    Low = 25,       // Width inference only
    Medium = 50,    // Width + usage pattern
    High = 75,      // API match or strong pattern
    Certain = 100   // Direct evidence
};

// Recovered type descriptor
struct TypeInfo {
    uint32_t typeId;            // Unique type ID
    RecoveredType baseType;     // Base classification
    uint32_t typeWidth;         // Width in bytes (1, 2, 4, 8)
    bool isPointer;             // True if pointer type
    uint32_t pointsToType;     // Type ID of pointee
    bool isSigned;              // True if signed integer
    TypeConfidence confidence;  // Confidence level
    uint32_t ssaVarId;          // SSA variable this was inferred from
    uint32_t arrayStride;       // Array element stride
    uint32_t arrayCount;        // Estimated element count
    uint32_t structSize;        // Total struct size
    uint32_t fieldCount;        // Number of detected fields
    std::string typeName;       // Human-readable name
};

// Recovered struct field
struct StructField {
    uint32_t parentTypeId;      // Containing struct type ID
    uint32_t fieldOffset;       // Byte offset from struct base
    uint32_t fieldWidth;        // Width of this field
    RecoveredType fieldType;    // Type classification
    uint32_t accessCount;       // Number of accesses
    bool isRead;
    bool isWritten;
    std::string fieldName;      // Generated name (e.g. "field_0x10")
};

// Definition-use chain entry
struct DefUseChain {
    uint32_t ssaVarId;          // SSA variable being tracked
    uint32_t defInstrIdx;       // Instruction index of definition
    uint32_t defBBIdx;          // Basic block of definition
    std::vector<uint32_t> useInstrIdxs;  // Where used
    std::vector<uint32_t> useBBIdxs;     // BB of each use
    bool isLive;                // Still live at function exit
    bool reachesReturn;         // Flows to RET
};

// License tier
enum class LicenseTier : uint32_t {
    Community = 0,
    Pro = 1,
    Enterprise = 2,
    Government = 3
};

// Feature bits (must match MASM FEATURE_* constants)
enum class FeatureBit : uint32_t {
    LinearDisasm    = 0,
    PEParser        = 1,
    ELFParser       = 2,
    StringExtract   = 3,
    PatternScan     = 4,
    CFGBuild        = 8,
    XRefBuild       = 9,
    FuncRecovery    = 10,
    IDAExport       = 11,
    RecursiveDisasm = 16,
    SSALifting      = 17,
    PHINodes        = 18,
    SemanticAnalysis = 19,
    TypeRecovery    = 20,
    DataFlow        = 21,
    Pseudocode      = 22
};

// License context
struct LicenseContext {
    uint64_t featureMask;
    LicenseTier tier;
    bool isValid;
    bool isAirgapped;
    std::string customer;

    LicenseContext() : featureMask(0x1F), tier(LicenseTier::Community),
                       isValid(true), isAirgapped(true) {}

    bool CheckFeature(FeatureBit bit) const {
        if (!isValid) return false;
        return (featureMask >> static_cast<uint32_t>(bit)) & 1;
    }

    void SetTier(LicenseTier t) {
        tier = t;
        switch (t) {
            case LicenseTier::Community:  featureMask = 0x0000001F; break;
            case LicenseTier::Pro:        featureMask = 0x00000F1F; break;
            case LicenseTier::Enterprise: featureMask = 0x000F0F1F; break;
            case LicenseTier::Government: featureMask = 0x007F0F1F; break;
        }
        isValid = true;
    }
};

struct SSAVariable {
    uint32_t varId;         // Unique SSA variable ID
    SSAVarClass varClass;   // Classification
    int32_t regIndex;       // Original machine register (0=rax..15=r15), -1 if N/A
    uint32_t ssaVersion;    // Version number for this register (0, 1, 2, ...)
    uint64_t immValue;      // Immediate value (for SSAVarClass::Immediate)
    int32_t memBase;        // Base register for memory operands
    int32_t memDisp;        // Displacement for memory operands
    uint32_t bbIndex;       // Basic block index where defined
    uint32_t instrIndex;    // Instruction index where defined (-1 for function entry / PHI)
};

struct SSAInstruction {
    uint64_t origAddress;   // Original machine instruction VA
    uint32_t origInstrIdx;  // Index into disassembly
    SSAOpType op;           // SSA operation type
    int32_t dstVarId;       // Destination SSA variable ID (-1 if none)
    int32_t src1VarId;      // First source SSA variable ID (-1 if none)
    int32_t src2VarId;      // Second source SSA variable ID (-1 if none)
    int32_t flagsVarId;     // Flags SSA variable (for CMP/TEST producers, branch consumers)
    uint32_t bbIndex;       // Basic block this belongs to
    uint64_t callTarget;    // For Call: target address
    uint64_t branchTarget;  // For Branch/Jmp: target BB start address
    bool isDeadCode;        // True if DCE marks this as dead
};

struct PhiNode {
    uint32_t bbIndex;       // Basic block where this PHI lives
    int32_t dstVarId;       // Destination SSA variable (merged result)
    int32_t regIndex;       // Original machine register being merged
    std::vector<int32_t> operandVarIds;  // SSA var IDs from each predecessor
    std::vector<uint32_t> operandBBs;    // Basic block indices of each predecessor
};

class RawrCodex {
public:
    RawrCodex() : m_architecture("x64"), m_bitness(64), m_ssaNextVarId(0),
                   m_ssaLifted(false), m_typesRecovered(false) {}

    // ========================================================================
    // Core Analysis Functions
    // ========================================================================

    bool LoadBinary(const std::string& path) {
        m_filePath = path;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;

        m_fileSize = file.tellg();
        m_binaryData.resize(m_fileSize);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(m_binaryData.data()), m_fileSize);
        file.close();

        // Detect format
        if (m_fileSize < 2) return false;
        
        if (m_binaryData[0] == 0x4D && m_binaryData[1] == 0x5A) {
            // PE/COFF format
            return ParsePE();
        } else if (m_fileSize > 4 && 
                   m_binaryData[0] == 0x7F && m_binaryData[1] == 0x45 && 
                   m_binaryData[2] == 0x4C && m_binaryData[3] == 0x46) {
            // ELF format
            return ParseELF();
        }
        
        // Unknown format - treat as raw binary
        m_architecture = "unknown";
        return true;
    }

    std::vector<Symbol> GetSymbols() const { return m_symbols; }
    std::vector<Section> GetSections() const { return m_sections; }
    std::vector<Import> GetImports() const { return m_imports; }
    std::vector<Export> GetExports() const { return m_exports; }

    // ========================================================================
    // Advanced Disassembly (x64 Intel)
    // ========================================================================

    std::vector<DisassemblyLine> Disassemble(uint64_t startAddr, size_t count = 100) {
        std::vector<DisassemblyLine> result;
        
        // Find section containing this address
        const Section* section = FindSectionByVA(startAddr);
        if (!section) return result;

        uint64_t offset = startAddr - section->virtualAddress;
        if (offset >= section->data.size()) return result;

        const uint8_t* ptr = section->data.data() + offset;
        const uint8_t* end = section->data.data() + section->data.size();
        uint64_t currentAddr = startAddr;

        for (size_t i = 0; i < count && ptr < end; ++i) {
            DisassemblyLine line;
            line.address = currentAddr;

            // Simple x64 instruction decoding (simplified)
            size_t instrLen = DecodeInstruction(ptr, end - ptr, line);
            if (instrLen == 0) break;

            result.push_back(line);
            ptr += instrLen;
            currentAddr += instrLen;
        }

        return result;
    }

    // ========================================================================
    // String Extraction
    // ========================================================================

    std::vector<std::string> ExtractStrings(size_t minLength = 4) {
        std::vector<std::string> strings;
        std::string current;

        for (size_t i = 0; i < m_binaryData.size(); ++i) {
            uint8_t byte = m_binaryData[i];
            
            if (byte >= 0x20 && byte <= 0x7E) {
                current += static_cast<char>(byte);
            } else if (byte == 0 && current.length() >= minLength) {
                strings.push_back(current);
                current.clear();
            } else if (byte == 0) {
                current.clear();
            }
        }

        return strings;
    }

    // ========================================================================
    // Control Flow Analysis
    // ========================================================================

    struct BasicBlock {
        uint64_t startAddress;
        uint64_t endAddress;
        std::vector<uint64_t> successors;
        std::vector<uint64_t> predecessors;
        bool isReturn;
        bool isCall;
    };

    std::vector<BasicBlock> AnalyzeControlFlow(uint64_t functionStart) {
        std::vector<BasicBlock> blocks;

        // Disassemble a generous window from the function start
        auto disasm = Disassemble(functionStart, 2048);
        if (disasm.empty()) return blocks;

        // Phase 1: Identify leader addresses
        //   - First instruction
        //   - Target of any branch/jump
        //   - Instruction following a branch/jump
        std::unordered_map<uint64_t, bool> leaders;
        leaders[functionStart] = true;

        for (size_t i = 0; i < disasm.size(); ++i) {
            const auto& instr = disasm[i];
            bool isBranch = false;
            bool isReturn = false;
            bool isCall   = false;
            uint64_t target = 0;

            if (instr.mnemonic == "ret" || instr.mnemonic == "retf" || instr.mnemonic == "leave") {
                isReturn = true;
                isBranch = true;
            } else if (instr.mnemonic == "call") {
                isCall = true;
                // Call target is NOT a block leader in THIS function (it's a different function)
                // But the instruction after the call IS a new block (return point)
                isBranch = false; // call does not terminate the block in most analyses
            } else if (instr.mnemonic == "jmp") {
                isBranch = true;
                target = ParseBranchTarget(instr.operands);
            } else if (instr.mnemonic.size() >= 2 && instr.mnemonic[0] == 'j') {
                // Conditional jump
                isBranch = true;
                target = ParseBranchTarget(instr.operands);
            } else if (instr.mnemonic == "loop" || instr.mnemonic == "loope" ||
                       instr.mnemonic == "loopne" || instr.mnemonic == "jrcxz") {
                isBranch = true;
                target = ParseBranchTarget(instr.operands);
            }

            if (target != 0) {
                leaders[target] = true;
            }
            if (isBranch && i + 1 < disasm.size()) {
                leaders[disasm[i + 1].address] = true;
            }

            (void)isReturn; (void)isCall;
        }

        // Phase 2: Build basic blocks between leaders
        BasicBlock current;
        current.startAddress = disasm[0].address;
        current.endAddress   = disasm[0].address;
        current.isReturn     = false;
        current.isCall       = false;

        for (size_t i = 0; i < disasm.size(); ++i) {
            const auto& instr = disasm[i];
            bool startNewBlock = (i > 0 && leaders.count(instr.address) > 0);

            if (startNewBlock) {
                // Finalize previous block — its last instruction is disasm[i-1]
                current.endAddress = disasm[i - 1].address;
                blocks.push_back(current);

                current = BasicBlock{};
                current.startAddress = instr.address;
                current.endAddress   = instr.address;
                current.isReturn     = false;
                current.isCall       = false;
            }

            current.endAddress = instr.address;

            // Check if this instruction terminates the block
            bool terminates = false;
            uint64_t target = 0;

            if (instr.mnemonic == "ret" || instr.mnemonic == "retf") {
                current.isReturn = true;
                terminates = true;
            } else if (instr.mnemonic == "jmp") {
                terminates = true;
                target = ParseBranchTarget(instr.operands);
                if (target != 0) {
                    current.successors.push_back(target);
                }
            } else if (instr.mnemonic.size() >= 2 && instr.mnemonic[0] == 'j') {
                terminates = true;
                target = ParseBranchTarget(instr.operands);
                if (target != 0) {
                    current.successors.push_back(target); // branch-taken
                }
                // fall-through
                if (i + 1 < disasm.size()) {
                    current.successors.push_back(disasm[i + 1].address);
                }
            } else if (instr.mnemonic == "loop" || instr.mnemonic == "loope" ||
                       instr.mnemonic == "loopne" || instr.mnemonic == "jrcxz") {
                terminates = true;
                target = ParseBranchTarget(instr.operands);
                if (target != 0) current.successors.push_back(target);
                if (i + 1 < disasm.size()) current.successors.push_back(disasm[i + 1].address);
            } else if (instr.mnemonic == "call") {
                current.isCall = true;
                // Call doesn't terminate block for CFG purposes
            }

            // If this is the last instruction OR the next instruction is a leader, push
            if (terminates || i + 1 == disasm.size()) {
                blocks.push_back(current);
                if (i + 1 < disasm.size()) {
                    current = BasicBlock{};
                    current.startAddress = disasm[i + 1].address;
                    current.endAddress   = disasm[i + 1].address;
                    current.isReturn     = false;
                    current.isCall       = false;
                }
            }
        }

        // Phase 3: Build predecessor edges (reverse of successors)
        std::unordered_map<uint64_t, size_t> blockByStart;
        for (size_t i = 0; i < blocks.size(); ++i) {
            blockByStart[blocks[i].startAddress] = i;
        }
        for (size_t i = 0; i < blocks.size(); ++i) {
            for (uint64_t succ : blocks[i].successors) {
                auto it = blockByStart.find(succ);
                if (it != blockByStart.end()) {
                    blocks[it->second].predecessors.push_back(blocks[i].startAddress);
                }
            }
        }

        return blocks;
    }

    // ========================================================================
    // Function Boundary Recovery (Heuristic-based)
    // ========================================================================

    struct FunctionEntry {
        uint64_t startAddress;
        uint64_t endAddress;     // 0 if unknown
        std::string name;
        size_t instructionCount;
        bool hasFramePointer;    // push rbp; mov rbp, rsp detected
        bool isThunk;            // Single jmp instruction (import thunk)
    };

    std::vector<FunctionEntry> RecoverFunctions(uint64_t baseAddr = 0, size_t scanSize = 0) {
        std::vector<FunctionEntry> functions;

        // Determine scan range from .text section(s) — executable sections
        struct ScanRange { uint64_t va; size_t dataIdx; size_t size; };
        std::vector<ScanRange> ranges;

        for (const auto& section : m_sections) {
            // IMAGE_SCN_MEM_EXECUTE = 0x20000000, IMAGE_SCN_CNT_CODE = 0x00000020
            bool isCode = (section.characteristics & 0x20000020) != 0;
            if (!isCode && section.name != ".text" && section.name != ".code") continue;
            if (section.data.empty()) continue;

            if (baseAddr != 0 && scanSize != 0) {
                // User specified scan range — check overlap
                uint64_t secEnd = section.virtualAddress + section.data.size();
                uint64_t reqEnd = baseAddr + scanSize;
                if (baseAddr >= secEnd || reqEnd <= section.virtualAddress) continue;
            }

            ScanRange r;
            r.va      = section.virtualAddress;
            r.dataIdx = 0; // We'll index into section.data
            r.size    = section.data.size();
            ranges.push_back(r);
        }

        // Common x64 function prologues (byte patterns)
        // push rbp; mov rbp, rsp                            → 55 48 89 E5
        // push rbp; sub rsp, imm8                           → 55 48 83 EC xx
        // sub rsp, imm8                                     → 48 83 EC xx
        // push r12..r15; push rbp; sub rsp                  → 41 5x 55 48...
        // (Windows) push rbx; sub rsp, 0x20                 → 53 48 83 EC 20
        // Thunks: jmp [rip+disp32]                          → FF 25 xx xx xx xx

        for (const auto& section : m_sections) {
            bool isCode = (section.characteristics & 0x20000020) != 0;
            if (!isCode && section.name != ".text" && section.name != ".code") continue;
            if (section.data.empty()) continue;

            const uint8_t* data = section.data.data();
            size_t len = section.data.size();

            for (size_t i = 0; i + 4 < len; ) {
                bool matched = false;
                FunctionEntry func;
                func.startAddress = section.virtualAddress + i;
                func.endAddress   = 0;
                func.instructionCount = 0;
                func.hasFramePointer  = false;
                func.isThunk          = false;

                // Pattern 1: push rbp; mov rbp, rsp (55 48 89 E5)
                if (i + 4 <= len && data[i] == 0x55 && data[i+1] == 0x48 &&
                    data[i+2] == 0x89 && data[i+3] == 0xE5) {
                    func.hasFramePointer = true;
                    matched = true;
                }
                // Pattern 2: push rbp; sub rsp, imm8 (55 48 83 EC xx)
                else if (i + 5 <= len && data[i] == 0x55 && data[i+1] == 0x48 &&
                         data[i+2] == 0x83 && data[i+3] == 0xEC) {
                    func.hasFramePointer = true;
                    matched = true;
                }
                // Pattern 3: sub rsp, imm8 (48 83 EC xx) — frameless but allocates stack
                else if (i + 4 <= len && data[i] == 0x48 && data[i+1] == 0x83 &&
                         data[i+2] == 0xEC) {
                    matched = true;
                }
                // Pattern 4: Thunk — jmp [rip+disp32] (FF 25 xx xx xx xx)
                else if (i + 6 <= len && data[i] == 0xFF && data[i+1] == 0x25) {
                    func.isThunk = true;
                    func.endAddress = section.virtualAddress + i + 6;
                    func.instructionCount = 1;
                    matched = true;
                }
                // Pattern 5: push rbx; sub rsp (53 48 83 EC)
                else if (i + 4 <= len && data[i] == 0x53 && data[i+1] == 0x48 &&
                         data[i+2] == 0x83 && data[i+3] == 0xEC) {
                    matched = true;
                }
                // Pattern 6: push rdi; push rsi; ... (Windows x64 SEH prologue)
                else if (i + 2 <= len && data[i] == 0x57 && data[i+1] == 0x56) {
                    matched = true;
                }

                if (matched) {
                    // Scan forward to find RET (function end)
                    if (!func.isThunk) {
                        size_t searchLimit = std::min(len - i, (size_t)65536); // 64K max function
                        for (size_t j = 0; j < searchLimit; ++j) {
                            func.instructionCount++;
                            if (data[i + j] == 0xC3 || data[i + j] == 0xCB) {
                                func.endAddress = section.virtualAddress + i + j + 1;
                                break;
                            }
                            // INT3 padding after function
                            if (data[i + j] == 0xCC) {
                                func.endAddress = section.virtualAddress + i + j;
                                break;
                            }
                        }
                    }

                    // Look up known symbol
                    func.name = "";
                    for (const auto& sym : m_symbols) {
                        if (sym.address == func.startAddress) {
                            func.name = sym.name;
                            break;
                        }
                    }
                    if (func.name.empty()) {
                        std::ostringstream ns;
                        ns << "sub_" << std::hex << func.startAddress;
                        func.name = ns.str();
                    }

                    functions.push_back(func);

                    // Skip past this function to avoid double-counting
                    if (func.endAddress > section.virtualAddress + i) {
                        i = static_cast<size_t>(func.endAddress - section.virtualAddress);
                    } else {
                        i += 4; // Minimum skip
                    }
                } else {
                    ++i;
                }
            }
        }

        return functions;
    }

    // ========================================================================
    // SSA Lifting (Static Single Assignment Form)
    // Converts disassembled instruction stream + CFG into SSA form.
    // Each register definition gets a unique version number.
    // PHI nodes are inserted at basic block merge points.
    // ========================================================================

    struct SSALiftResult {
        std::vector<SSAVariable> variables;
        std::vector<SSAInstruction> instructions;
        std::vector<PhiNode> phiNodes;
        uint32_t totalVars;
        bool success;
    };

    SSALiftResult LiftToSSA(uint64_t functionStart) {
        SSALiftResult result;
        result.success = false;
        result.totalVars = 0;

        // First, get the CFG for this function
        auto blocks = AnalyzeControlFlow(functionStart);
        if (blocks.empty()) return result;

        // Get the disassembly for the function's address range
        auto disasm = Disassemble(functionStart, 4096);
        if (disasm.empty()) return result;

        // Reset SSA state
        m_ssaNextVarId = 0;
        uint32_t regVersions[16] = {0}; // Version counter per register (rax=0..r15=15)
        uint32_t flagsVersion = 0;

        // Phase 1: Create initial SSA variables for each register (version 0)
        // These represent function entry / undefined state
        for (int reg = 0; reg < 16; ++reg) {
            SSAVariable var;
            var.varId = m_ssaNextVarId++;
            var.varClass = SSAVarClass::Register;
            var.regIndex = reg;
            var.ssaVersion = 0;
            var.immValue = 0;
            var.memBase = -1;
            var.memDisp = 0;
            var.bbIndex = 0;
            var.instrIndex = UINT32_MAX; // Function entry
            result.variables.push_back(var);
        }

        // Initial flags variable
        {
            SSAVariable flagsVar;
            flagsVar.varId = m_ssaNextVarId++;
            flagsVar.varClass = SSAVarClass::Flags;
            flagsVar.regIndex = -1;
            flagsVar.ssaVersion = 0;
            flagsVar.immValue = 0;
            flagsVar.memBase = -1;
            flagsVar.memDisp = 0;
            flagsVar.bbIndex = 0;
            flagsVar.instrIndex = UINT32_MAX;
            result.variables.push_back(flagsVar);
        }

        // Build address-to-disasm-index map for fast lookup
        std::unordered_map<uint64_t, size_t> addrToIdx;
        for (size_t i = 0; i < disasm.size(); ++i) {
            addrToIdx[disasm[i].address] = i;
        }

        // Phase 2: Walk each basic block and lift instructions
        for (size_t bbIdx = 0; bbIdx < blocks.size(); ++bbIdx) {
            const auto& bb = blocks[bbIdx];

            // Phase 2a: Insert PHI nodes at multi-predecessor blocks
            if (bb.predecessors.size() >= 2) {
                for (int reg = 0; reg < 16; ++reg) {
                    regVersions[reg]++;

                    SSAVariable phiVar;
                    phiVar.varId = m_ssaNextVarId++;
                    phiVar.varClass = SSAVarClass::Register;
                    phiVar.regIndex = reg;
                    phiVar.ssaVersion = regVersions[reg];
                    phiVar.immValue = 0;
                    phiVar.memBase = -1;
                    phiVar.memDisp = 0;
                    phiVar.bbIndex = static_cast<uint32_t>(bbIdx);
                    phiVar.instrIndex = UINT32_MAX;
                    result.variables.push_back(phiVar);

                    PhiNode phi;
                    phi.bbIndex = static_cast<uint32_t>(bbIdx);
                    phi.dstVarId = phiVar.varId;
                    phi.regIndex = reg;
                    // Operands will be resolved in Phase 3
                    for (size_t p = 0; p < bb.predecessors.size(); ++p) {
                        phi.operandVarIds.push_back(-1);
                        phi.operandBBs.push_back(static_cast<uint32_t>(p));
                    }
                    result.phiNodes.push_back(phi);
                }
            }

            // Phase 2b: Walk instructions in this basic block
            for (size_t i = 0; i < disasm.size(); ++i) {
                const auto& instr = disasm[i];
                if (instr.address < bb.startAddress || instr.address > bb.endAddress)
                    continue;

                SSAInstruction ssaInstr;
                ssaInstr.origAddress = instr.address;
                ssaInstr.origInstrIdx = static_cast<uint32_t>(i);
                ssaInstr.dstVarId = -1;
                ssaInstr.src1VarId = -1;
                ssaInstr.src2VarId = -1;
                ssaInstr.flagsVarId = -1;
                ssaInstr.bbIndex = static_cast<uint32_t>(bbIdx);
                ssaInstr.callTarget = 0;
                ssaInstr.branchTarget = 0;
                ssaInstr.isDeadCode = false;

                // Classify mnemonic → SSA operation
                const std::string& mn = instr.mnemonic;

                if (mn == "mov" || mn == "movsx" || mn == "movzx" || mn == "movsxd") {
                    ssaInstr.op = SSAOpType::Assign;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]); // placeholder
                } else if (mn == "lea") {
                    ssaInstr.op = SSAOpType::Lea;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                } else if (mn == "add" || mn == "adc") {
                    ssaInstr.op = SSAOpType::Add;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "sub" || mn == "sbb") {
                    ssaInstr.op = SSAOpType::Sub;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "imul" || mn == "mul") {
                    ssaInstr.op = SSAOpType::Mul;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "idiv" || mn == "div") {
                    ssaInstr.op = SSAOpType::Div;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "and") {
                    ssaInstr.op = SSAOpType::And;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "or") {
                    ssaInstr.op = SSAOpType::Or;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "xor") {
                    ssaInstr.op = SSAOpType::Xor;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "shl" || mn == "sal") {
                    ssaInstr.op = SSAOpType::Shl;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "shr") {
                    ssaInstr.op = SSAOpType::Shr;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "sar") {
                    ssaInstr.op = SSAOpType::Sar;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "not") {
                    ssaInstr.op = SSAOpType::Not;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                } else if (mn == "neg") {
                    ssaInstr.op = SSAOpType::Neg;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                } else if (mn == "cmp") {
                    ssaInstr.op = SSAOpType::Cmp;
                    ssaInstr.flagsVarId = m_ssaNextVarId++;
                    flagsVersion++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "test") {
                    ssaInstr.op = SSAOpType::Test;
                    ssaInstr.flagsVarId = m_ssaNextVarId++;
                    flagsVersion++;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                    ssaInstr.src2VarId = static_cast<int32_t>(regVersions[1]);
                } else if (mn == "call") {
                    ssaInstr.op = SSAOpType::Call;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    regVersions[0]++; // Call clobbers rax
                    // Parse call target from operands if available
                    ssaInstr.callTarget = ParseBranchTarget(instr.operands);
                } else if (mn == "ret" || mn == "retf") {
                    ssaInstr.op = SSAOpType::Ret;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]); // rax = return value
                } else if (mn == "jmp") {
                    ssaInstr.op = SSAOpType::Jmp;
                    ssaInstr.branchTarget = ParseBranchTarget(instr.operands);
                } else if (mn.size() >= 2 && mn[0] == 'j') {
                    // Conditional branch (jcc)
                    ssaInstr.op = SSAOpType::Branch;
                    ssaInstr.flagsVarId = static_cast<int32_t>(flagsVersion);
                    ssaInstr.branchTarget = ParseBranchTarget(instr.operands);
                } else if (mn == "push") {
                    ssaInstr.op = SSAOpType::Store;
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                } else if (mn == "pop") {
                    ssaInstr.op = SSAOpType::Load;
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                } else if (mn.size() >= 4 && mn.substr(0, 4) == "cmov") {
                    ssaInstr.op = SSAOpType::Assign; // Conditional move
                    ssaInstr.dstVarId = m_ssaNextVarId++;
                    ssaInstr.flagsVarId = static_cast<int32_t>(flagsVersion);
                    ssaInstr.src1VarId = static_cast<int32_t>(regVersions[0]);
                } else if (mn == "nop") {
                    continue; // Skip NOPs entirely
                } else {
                    ssaInstr.op = SSAOpType::Unknown;
                }

                result.instructions.push_back(ssaInstr);
            }
        }

        // Phase 3: Resolve PHI node operands
        // For each PHI node, find the last definition of the register in each predecessor
        for (auto& phi : result.phiNodes) {
            for (size_t opIdx = 0; opIdx < phi.operandBBs.size(); ++opIdx) {
                uint32_t predBB = phi.operandBBs[opIdx];
                // Scan SSA instructions backwards for a definition in predBB
                bool found = false;
                for (int j = static_cast<int>(result.instructions.size()) - 1; j >= 0; --j) {
                    const auto& si = result.instructions[j];
                    if (si.bbIndex == predBB && si.dstVarId >= 0) {
                        phi.operandVarIds[opIdx] = si.dstVarId;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Use initial version (varId = regIndex for version 0)
                    phi.operandVarIds[opIdx] = phi.regIndex;
                }
            }
        }

        result.totalVars = m_ssaNextVarId;
        result.success = true;
        m_ssaLifted = true;
        return result;
    }

    // ========================================================================
    // Recursive Descent Disassembler (CFG-guided)
    // Follows control flow edges to disassemble only reachable code.
    // Avoids inline data, alignment padding, and dead code regions.
    // ========================================================================

    std::vector<DisassemblyLine> RecursiveDisassemble(uint64_t entryPoint, size_t maxInstructions = 65536) {
        std::vector<DisassemblyLine> result;
        std::unordered_map<uint64_t, bool> visited;
        std::vector<uint64_t> worklist;

        worklist.push_back(entryPoint);

        while (!worklist.empty() && result.size() < maxInstructions) {
            uint64_t addr = worklist.back();
            worklist.pop_back();

            if (visited.count(addr)) continue;
            visited[addr] = true;

            // Find section containing this address
            const Section* section = FindSectionByVA(addr);
            if (!section) continue;

            uint64_t offset = addr - section->virtualAddress;
            if (offset >= section->data.size()) continue;

            const uint8_t* data = section->data.data() + offset;
            size_t remaining = section->data.size() - offset;
            if (remaining == 0) continue;

            // Decode one instruction at this address
            DisassemblyLine line;
            line.address = addr;

            size_t instrLen = DecodeOneInstruction(data, remaining, line);
            if (instrLen == 0) {
                // Can't decode — emit as db
                line.mnemonic = "db";
                line.operands = "0x" + ByteToHex(data[0]);
                line.bytes = {data[0]};
                instrLen = 1;
            } else {
                line.bytes.assign(data, data + instrLen);
            }

            result.push_back(line);

            // Determine successors based on control flow
            if (line.mnemonic == "ret" || line.mnemonic == "retf") {
                // Terminal — no successors
                continue;
            } else if (line.mnemonic == "jmp") {
                // Unconditional jump — target only
                uint64_t target = ParseBranchTarget(line.operands);
                if (target != 0) {
                    worklist.push_back(target);
                }
            } else if (line.mnemonic == "call") {
                // Call — follow into callee AND fall-through
                uint64_t target = ParseBranchTarget(line.operands);
                if (target != 0) {
                    worklist.push_back(target);
                }
                worklist.push_back(addr + instrLen); // fall-through
            } else if (line.mnemonic.size() >= 2 && line.mnemonic[0] == 'j') {
                // Conditional jump — target AND fall-through
                uint64_t target = ParseBranchTarget(line.operands);
                if (target != 0) {
                    worklist.push_back(target);
                }
                worklist.push_back(addr + instrLen);
            } else if (line.mnemonic == "loop" || line.mnemonic == "loope" ||
                       line.mnemonic == "loopne" || line.mnemonic == "jrcxz") {
                uint64_t target = ParseBranchTarget(line.operands);
                if (target != 0) worklist.push_back(target);
                worklist.push_back(addr + instrLen);
            } else {
                // Linear instruction — fall-through only
                worklist.push_back(addr + instrLen);
            }
        }

        // Sort by address for display
        std::sort(result.begin(), result.end(),
            [](const DisassemblyLine& a, const DisassemblyLine& b) {
                return a.address < b.address;
            });

        return result;
    }

    // SSA state accessors
    bool IsSSALifted() const { return m_ssaLifted; }
    uint32_t GetSSAVarCount() const { return m_ssaNextVarId; }
    bool AreTypesRecovered() const { return m_typesRecovered; }
    const LicenseContext& GetLicense() const { return m_license; }
    void SetLicenseTier(LicenseTier tier) { m_license.SetTier(tier); }

    // ========================================================================
    // Phase 17: Type Recovery
    // Infers types from SSA data flow: pointer detection, signed/unsigned,
    // struct field recovery, array stride detection.
    // ========================================================================

    struct TypeRecoveryResult {
        std::vector<TypeInfo> types;
        std::vector<StructField> fields;
        std::vector<DefUseChain> defUseChains;
        uint32_t totalTypes;
        uint32_t deadVarCount;
        bool success;
    };

    TypeRecoveryResult RecoverTypes(uint64_t functionStart) {
        TypeRecoveryResult result;
        result.success = false;
        result.totalTypes = 0;
        result.deadVarCount = 0;

        // SSA must be lifted first
        if (!m_ssaLifted) {
            auto ssaResult = LiftToSSA(functionStart);
            if (!ssaResult.success) return result;
        }

        // Type name table (matches MASM typeNames)
        static const char* typeNameTable[] = {
            "unknown", "int8_t", "int16_t", "int32_t", "int64_t",
            "uint8_t", "uint16_t", "uint32_t", "uint64_t",
            "float", "double", "void*", "func_ptr", "data_ptr",
            "struct*", "array_ptr", "bool", "flags", "char*", "void"
        };

        // Get SSA state from last lift
        auto ssaResult = LiftToSSA(functionStart);
        if (!ssaResult.success) return result;

        // Scratch arrays: per-variable type inference
        std::vector<RecoveredType> varTypes(ssaResult.totalVars, RecoveredType::Unknown);
        std::vector<uint32_t> varWidths(ssaResult.totalVars, 0);
        std::vector<TypeConfidence> varConfidence(ssaResult.totalVars, TypeConfidence::None);

        // ---------------------------------------------------------------
        // Pass 1: Width + type inference from SSA operations
        // ---------------------------------------------------------------
        for (const auto& si : ssaResult.instructions) {
            if (si.dstVarId < 0 || static_cast<uint32_t>(si.dstVarId) >= ssaResult.totalVars) continue;
            uint32_t dst = static_cast<uint32_t>(si.dstVarId);

            switch (si.op) {
                case SSAOpType::Load:
                    varTypes[dst] = RecoveredType::Int64;
                    varWidths[dst] = 8;
                    varConfidence[dst] = TypeConfidence::Low;
                    // Mark source as data pointer
                    if (si.src1VarId >= 0 && static_cast<uint32_t>(si.src1VarId) < ssaResult.totalVars) {
                        varTypes[si.src1VarId] = RecoveredType::DataPtr;
                        varConfidence[si.src1VarId] = TypeConfidence::Medium;
                    }
                    break;

                case SSAOpType::Store:
                    // Destination of store is a pointer
                    varTypes[dst] = RecoveredType::DataPtr;
                    varConfidence[dst] = TypeConfidence::Medium;
                    break;

                case SSAOpType::Call:
                    // Return value: assume pointer if target is non-zero (likely API)
                    if (si.callTarget != 0) {
                        varTypes[dst] = RecoveredType::Pointer;
                        varConfidence[dst] = TypeConfidence::Medium;
                    } else {
                        varTypes[dst] = RecoveredType::Int64;
                        varConfidence[dst] = TypeConfidence::Low;
                    }
                    varWidths[dst] = 8;
                    break;

                case SSAOpType::Lea:
                    varTypes[dst] = RecoveredType::Pointer;
                    varWidths[dst] = 8;
                    varConfidence[dst] = TypeConfidence::High;
                    // If there's a src2 (index register), it's an array access
                    if (si.src2VarId >= 0) {
                        varTypes[dst] = RecoveredType::ArrayPtr;
                    }
                    break;

                case SSAOpType::Cmp:
                case SSAOpType::Test:
                    // Flags producer
                    if (si.flagsVarId >= 0 && static_cast<uint32_t>(si.flagsVarId) < ssaResult.totalVars) {
                        varTypes[si.flagsVarId] = RecoveredType::Flags;
                        varConfidence[si.flagsVarId] = TypeConfidence::Certain;
                    }
                    break;

                case SSAOpType::Add:
                case SSAOpType::Sub:
                case SSAOpType::Mul:
                    varTypes[dst] = RecoveredType::Int64;
                    varWidths[dst] = 8;
                    varConfidence[dst] = TypeConfidence::Low;
                    break;

                case SSAOpType::And:
                case SSAOpType::Or:
                case SSAOpType::Xor:
                case SSAOpType::Shl:
                case SSAOpType::Shr:
                    varTypes[dst] = RecoveredType::UInt64;
                    varWidths[dst] = 8;
                    varConfidence[dst] = TypeConfidence::Low;
                    break;

                case SSAOpType::Sar:
                    // Arithmetic right shift → signed
                    varTypes[dst] = RecoveredType::Int64;
                    varWidths[dst] = 8;
                    varConfidence[dst] = TypeConfidence::Medium;
                    break;

                case SSAOpType::Assign:
                    // Propagate type from source
                    if (si.src1VarId >= 0 && static_cast<uint32_t>(si.src1VarId) < ssaResult.totalVars) {
                        varTypes[dst] = varTypes[si.src1VarId];
                        varWidths[dst] = varWidths[si.src1VarId];
                        varConfidence[dst] = varConfidence[si.src1VarId];
                    }
                    break;

                default:
                    break;
            }
        }

        // ---------------------------------------------------------------
        // Pass 2: Pointer dereference pattern → struct promotion
        // ---------------------------------------------------------------
        std::unordered_map<uint32_t, int> ptrAccessCount; // varId → access count
        for (const auto& si : ssaResult.instructions) {
            if (si.op == SSAOpType::Load && si.src1VarId >= 0) {
                ptrAccessCount[si.src1VarId]++;
            }
            if (si.op == SSAOpType::Store && si.dstVarId >= 0) {
                ptrAccessCount[si.dstVarId]++;
            }
        }
        for (auto& [varId, count] : ptrAccessCount) {
            if (count >= 2 && varId < ssaResult.totalVars &&
                varTypes[varId] == RecoveredType::DataPtr) {
                varTypes[varId] = RecoveredType::StructPtr;
                varConfidence[varId] = TypeConfidence::Medium;
            }
        }

        // ---------------------------------------------------------------
        // Pass 3: Build TypeInfo entries
        // ---------------------------------------------------------------
        uint32_t nextTypeId = 0;
        for (uint32_t i = 0; i < ssaResult.totalVars; ++i) {
            if (varTypes[i] == RecoveredType::Unknown) continue;

            TypeInfo ti;
            ti.typeId = nextTypeId++;
            ti.baseType = varTypes[i];
            ti.typeWidth = varWidths[i];
            ti.ssaVarId = i;
            ti.confidence = varConfidence[i];

            // Pointer classification
            ti.isPointer = (varTypes[i] == RecoveredType::Pointer ||
                            varTypes[i] == RecoveredType::CodePtr ||
                            varTypes[i] == RecoveredType::DataPtr ||
                            varTypes[i] == RecoveredType::StructPtr ||
                            varTypes[i] == RecoveredType::ArrayPtr ||
                            varTypes[i] == RecoveredType::StringPtr);

            // Signed classification
            uint32_t bt = static_cast<uint32_t>(varTypes[i]);
            ti.isSigned = (bt >= 1 && bt <= 4); // Int8..Int64

            // Type name from table
            if (bt < 20) {
                ti.typeName = typeNameTable[bt];
            } else {
                ti.typeName = "unknown";
            }

            ti.pointsToType = 0;
            ti.arrayStride = 0;
            ti.arrayCount = 0;
            ti.structSize = 0;
            ti.fieldCount = 0;

            result.types.push_back(ti);
        }

        // ---------------------------------------------------------------
        // Pass 4: Build def-use chains
        // ---------------------------------------------------------------
        for (const auto& var : ssaResult.variables) {
            DefUseChain du;
            du.ssaVarId = var.varId;
            du.defInstrIdx = var.instrIndex;
            du.defBBIdx = var.bbIndex;
            du.isLive = false;
            du.reachesReturn = false;

            // Find all uses
            for (size_t i = 0; i < ssaResult.instructions.size(); ++i) {
                const auto& si = ssaResult.instructions[i];
                bool isUsed = (si.src1VarId == static_cast<int32_t>(var.varId) ||
                               si.src2VarId == static_cast<int32_t>(var.varId) ||
                               si.flagsVarId == static_cast<int32_t>(var.varId));
                if (isUsed) {
                    du.useInstrIdxs.push_back(static_cast<uint32_t>(i));
                    du.useBBIdxs.push_back(si.bbIndex);
                    if (si.op == SSAOpType::Ret) {
                        du.reachesReturn = true;
                    }
                }
            }

            du.isLive = !du.useInstrIdxs.empty();
            if (!du.isLive) result.deadVarCount++;

            result.defUseChains.push_back(du);
        }

        result.totalTypes = nextTypeId;
        result.success = true;
        m_typesRecovered = true;
        return result;
    }

    // ========================================================================
    // Phase 17: License Feature Gate
    // ========================================================================

    bool CheckFeature(FeatureBit feature) const {
        return m_license.CheckFeature(feature);
    }

    // ========================================================================
    // Symbol Demangling (Itanium ABI / MSVC basic)
    // ========================================================================

    std::string DemangleSymbol(const std::string& mangled) const {
        if (mangled.empty()) return mangled;

        // MSVC mangling starts with '?'
        if (mangled[0] == '?') {
            return DemangleMSVC(mangled);
        }

        // Itanium ABI mangling starts with '_Z'
        if (mangled.size() >= 2 && mangled[0] == '_' && mangled[1] == 'Z') {
            return DemangleItanium(mangled);
        }

        // Not mangled
        return mangled;
    }

    // Batch demangle all symbols in-place
    void DemangleAllSymbols() {
        for (auto& sym : m_symbols) {
            if (sym.isMangled && sym.demangledName == sym.name) {
                sym.demangledName = DemangleSymbol(sym.name);
            }
        }
    }

    // ========================================================================
    // Pattern Matching
    // ========================================================================

    struct PatternMatch {
        uint64_t address;
        std::vector<uint8_t> bytes;
        std::string description;
    };

    std::vector<PatternMatch> FindPattern(const std::vector<uint8_t>& pattern, 
                                          const std::vector<uint8_t>& mask) {
        std::vector<PatternMatch> matches;
        
        for (size_t i = 0; i <= m_binaryData.size() - pattern.size(); ++i) {
            bool found = true;
            for (size_t j = 0; j < pattern.size(); ++j) {
                if (mask[j] == 0xFF && m_binaryData[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            
            if (found) {
                PatternMatch match;
                match.address = i;
                match.bytes.assign(m_binaryData.begin() + i, 
                                  m_binaryData.begin() + i + pattern.size());
                matches.push_back(match);
            }
        }
        
        return matches;
    }

    // ========================================================================
    // Export to Various Formats
    // ========================================================================

    std::string ExportToIDA() {
        std::ostringstream oss;
        oss << "; IDA Script for " << m_filePath << "\n";
        oss << "; Generated by RawrCodex\n\n";
        
        for (const auto& sym : m_symbols) {
            if (sym.type == "function") {
                oss << "MakeFunction(" << std::hex << sym.address << ");\n";
                oss << "MakeName(" << std::hex << sym.address 
                    << ", \"" << sym.name << "\");\n";
            }
        }
        
        return oss.str();
    }

    std::string ExportToGhidra() {
        std::ostringstream oss;
        oss << "# Ghidra Script for " << m_filePath << "\n";
        oss << "# Generated by RawrCodex\n\n";
        
        for (const auto& sym : m_symbols) {
            oss << "createFunction(toAddr(0x" << std::hex << sym.address 
                << "), \"" << sym.name << "\");\n";
        }
        
        return oss.str();
    }

    // ========================================================================
    // Vulnerability Detection
    // ========================================================================

    struct Vulnerability {
        std::string type;
        uint64_t address;
        std::string description;
        std::string severity; // "critical", "high", "medium", "low"
    };

    std::vector<Vulnerability> DetectVulnerabilities() {
        std::vector<Vulnerability> vulns;
        
        // Check for dangerous functions
        std::vector<std::string> dangerousFuncs = {
            "strcpy", "strcat", "gets", "sprintf", "vsprintf",
            "scanf", "sscanf", "system", "popen", "exec"
        };
        
        for (const auto& imp : m_imports) {
            for (const auto& danger : dangerousFuncs) {
                if (imp.functionName.find(danger) != std::string::npos) {
                    Vulnerability vuln;
                    vuln.type = "dangerous_function";
                    vuln.address = imp.address;
                    vuln.description = "Use of " + imp.functionName + " can lead to buffer overflow";
                    vuln.severity = "high";
                    vulns.push_back(vuln);
                }
            }
        }
        
        // Check for stack canaries (absence is a vuln)
        bool hasStackProtection = false;
        for (const auto& imp : m_imports) {
            if (imp.functionName.find("__security") != std::string::npos ||
                imp.functionName.find("stack_chk") != std::string::npos) {
                hasStackProtection = true;
                break;
            }
        }
        
        if (!hasStackProtection) {
            Vulnerability vuln;
            vuln.type = "no_stack_protection";
            vuln.address = 0;
            vuln.description = "Binary compiled without stack canaries";
            vuln.severity = "medium";
            vulns.push_back(vuln);
        }
        
        return vulns;
    }

private:
    std::string m_filePath;
    size_t m_fileSize;
    std::vector<uint8_t> m_binaryData;
    std::string m_architecture;
    int m_bitness;
    
    std::vector<Symbol> m_symbols;
    std::vector<Section> m_sections;
    std::vector<Import> m_imports;
    std::vector<Export> m_exports;

    // SSA state
    uint32_t m_ssaNextVarId;
    bool m_ssaLifted;

    // Type recovery state (Phase 17)
    bool m_typesRecovered;
    LicenseContext m_license;

    // ========================================================================
    // Utility Helpers
    // ========================================================================

    // Alias for DecodeInstruction — used by RecursiveDisassemble
    size_t DecodeOneInstruction(const uint8_t* data, size_t maxLen, DisassemblyLine& out) {
        return DecodeInstruction(data, maxLen, out);
    }

    static std::string ByteToHex(uint8_t b) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X", b);
        return std::string(buf);
    }

    // ========================================================================
    // PE Parser
    // ========================================================================

    bool ParsePE() {
        if (m_fileSize < 64) return false;
        
        // Read DOS header
        uint32_t peOffset = *reinterpret_cast<uint32_t*>(&m_binaryData[0x3C]);
        if (peOffset + 4 > m_fileSize) return false;
        
        // Verify PE signature
        if (m_binaryData[peOffset] != 'P' || m_binaryData[peOffset + 1] != 'E') 
            return false;
        
        // Read COFF header
        uint16_t machine = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 4]);
        uint16_t numSections = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 6]);
        uint16_t sizeOfOptHeader = *reinterpret_cast<uint16_t*>(&m_binaryData[peOffset + 20]);
        
        m_architecture = (machine == 0x8664) ? "x64" : "x86";
        m_bitness = (machine == 0x8664) ? 64 : 32;

        // ================================================================
        // Parse Optional Header for Data Directory entries
        // ================================================================
        size_t optHeaderOffset = peOffset + 24;
        uint16_t optMagic = 0;
        if (optHeaderOffset + 2 <= m_fileSize) {
            optMagic = *reinterpret_cast<uint16_t*>(&m_binaryData[optHeaderOffset]);
        }
        bool isPE32Plus = (optMagic == 0x20B);  // PE32+ (64-bit)

        // Image base
        uint64_t imageBase = 0;
        if (isPE32Plus && optHeaderOffset + 30 <= m_fileSize) {
            imageBase = *reinterpret_cast<uint64_t*>(&m_binaryData[optHeaderOffset + 24]);
        } else if (optHeaderOffset + 32 <= m_fileSize) {
            imageBase = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 28]);
        }

        // Number of Data Directory entries
        uint32_t numDataDirs = 0;
        size_t dataDirBase = 0;
        if (isPE32Plus) {
            // PE32+: NumberOfRvaAndSizes at offset 108 into optional header
            if (optHeaderOffset + 112 <= m_fileSize) {
                numDataDirs = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 108]);
                dataDirBase = optHeaderOffset + 112;
            }
        } else {
            // PE32: NumberOfRvaAndSizes at offset 92 into optional header
            if (optHeaderOffset + 96 <= m_fileSize) {
                numDataDirs = *reinterpret_cast<uint32_t*>(&m_binaryData[optHeaderOffset + 92]);
                dataDirBase = optHeaderOffset + 96;
            }
        }
        if (numDataDirs > 16) numDataDirs = 16; // Safety cap

        // Read import and export directory RVA/Size
        uint32_t exportDirRVA = 0, exportDirSize = 0;
        uint32_t importDirRVA = 0, importDirSize = 0;
        if (numDataDirs > 0 && dataDirBase + 8 <= m_fileSize) {
            exportDirRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 0]);
            exportDirSize = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 4]);
        }
        if (numDataDirs > 1 && dataDirBase + 16 <= m_fileSize) {
            importDirRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 8]);
            importDirSize = *reinterpret_cast<uint32_t*>(&m_binaryData[dataDirBase + 12]);
        }

        // Parse sections
        size_t sectionTableOffset = peOffset + 24 + sizeOfOptHeader;
        for (int i = 0; i < numSections; ++i) {
            size_t sectionOffset = sectionTableOffset + i * 40;
            if (sectionOffset + 40 > m_fileSize) break;
            
            Section section;
            char name[9] = {0};
            memcpy(name, &m_binaryData[sectionOffset], 8);
            section.name = name;
            
            section.virtualSize = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 8]);
            section.virtualAddress = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 12]);
            section.rawSize = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 16]);
            section.rawOffset = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 20]);
            section.characteristics = *reinterpret_cast<uint32_t*>(&m_binaryData[sectionOffset + 36]);
            
            // Read section data
            if (section.rawOffset + section.rawSize <= m_fileSize) {
                section.data.assign(m_binaryData.begin() + section.rawOffset,
                                   m_binaryData.begin() + section.rawOffset + section.rawSize);
            }
            
            m_sections.push_back(section);
        }

        // ================================================================
        // Parse Import Directory Table (IDT)
        // ================================================================
        if (importDirRVA != 0 && importDirSize != 0) {
            size_t importFileOffset = RvaToFileOffset(importDirRVA);
            if (importFileOffset != 0) {
                // Each IMAGE_IMPORT_DESCRIPTOR is 20 bytes
                // Terminated by an all-zero entry
                for (size_t idx = 0; idx < 1024; ++idx) { // Safety cap
                    size_t descOffset = importFileOffset + idx * 20;
                    if (descOffset + 20 > m_fileSize) break;

                    uint32_t iltRVA     = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 0]);
                    uint32_t nameRVA    = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 12]);
                    uint32_t iatRVA     = *reinterpret_cast<uint32_t*>(&m_binaryData[descOffset + 16]);

                    // Terminator: all zeros
                    if (iltRVA == 0 && nameRVA == 0 && iatRVA == 0) break;

                    // Read DLL name
                    std::string dllName;
                    size_t nameFileOff = RvaToFileOffset(nameRVA);
                    if (nameFileOff != 0 && nameFileOff < m_fileSize) {
                        for (size_t c = nameFileOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                            dllName += static_cast<char>(m_binaryData[c]);
                            if (dllName.size() > 260) break; // MAX_PATH safety
                        }
                    }

                    // Walk the Import Lookup Table (ILT) or IAT
                    uint32_t thunkRVA = (iltRVA != 0) ? iltRVA : iatRVA;
                    size_t thunkFileOff = RvaToFileOffset(thunkRVA);
                    if (thunkFileOff == 0) continue;

                    size_t entrySize = isPE32Plus ? 8 : 4;
                    for (size_t t = 0; t < 8192; ++t) { // Safety cap per DLL
                        size_t entryOff = thunkFileOff + t * entrySize;
                        if (entryOff + entrySize > m_fileSize) break;

                        uint64_t thunkValue = 0;
                        if (isPE32Plus) {
                            thunkValue = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff]);
                        } else {
                            thunkValue = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff]);
                        }
                        if (thunkValue == 0) break; // End of thunk array

                        Import imp;
                        imp.moduleName = dllName;
                        imp.address    = imageBase + iatRVA + t * entrySize;

                        // Check ordinal flag (bit 63 for PE32+, bit 31 for PE32)
                        bool isOrdinal = false;
                        if (isPE32Plus) {
                            isOrdinal = (thunkValue & 0x8000000000000000ULL) != 0;
                        } else {
                            isOrdinal = (thunkValue & 0x80000000UL) != 0;
                        }

                        if (isOrdinal) {
                            imp.ordinal = static_cast<uint16_t>(thunkValue & 0xFFFF);
                            imp.functionName = "Ordinal_" + std::to_string(imp.ordinal);
                        } else {
                            // Hint/Name Table entry: 2-byte hint + null-terminated name
                            uint32_t hintNameRVA = static_cast<uint32_t>(thunkValue & 0x7FFFFFFF);
                            size_t hintNameOff = RvaToFileOffset(hintNameRVA);
                            if (hintNameOff != 0 && hintNameOff + 2 < m_fileSize) {
                                imp.ordinal = *reinterpret_cast<uint16_t*>(&m_binaryData[hintNameOff]);
                                size_t nameStart = hintNameOff + 2;
                                for (size_t c = nameStart; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                    imp.functionName += static_cast<char>(m_binaryData[c]);
                                    if (imp.functionName.size() > 512) break;
                                }
                            } else {
                                imp.functionName = "Unknown";
                                imp.ordinal = 0;
                            }
                        }

                        m_imports.push_back(imp);
                    }
                }
            }
        }

        // ================================================================
        // Parse Export Directory Table (EDT)
        // ================================================================
        if (exportDirRVA != 0 && exportDirSize != 0) {
            size_t exportFileOffset = RvaToFileOffset(exportDirRVA);
            if (exportFileOffset != 0 && exportFileOffset + 40 <= m_fileSize) {
                // IMAGE_EXPORT_DIRECTORY fields
                uint32_t numberOfFunctions    = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 20]);
                uint32_t numberOfNames        = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 24]);
                uint32_t addressTableRVA      = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 28]);
                uint32_t namePointerTableRVA  = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 32]);
                uint32_t ordinalTableRVA      = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 36]);
                uint32_t ordinalBase          = *reinterpret_cast<uint32_t*>(&m_binaryData[exportFileOffset + 16]);

                // Safety caps
                if (numberOfFunctions > 65536) numberOfFunctions = 65536;
                if (numberOfNames > 65536) numberOfNames = 65536;

                size_t addrTableOff    = RvaToFileOffset(addressTableRVA);
                size_t nameTableOff    = RvaToFileOffset(namePointerTableRVA);
                size_t ordinalTableOff = RvaToFileOffset(ordinalTableRVA);

                // Build name→ordinal mapping from the name table
                std::unordered_map<uint16_t, std::string> ordinalToName;
                if (nameTableOff != 0 && ordinalTableOff != 0) {
                    for (uint32_t n = 0; n < numberOfNames; ++n) {
                        size_t nameRVAoff = nameTableOff + n * 4;
                        size_t ordOff     = ordinalTableOff + n * 2;
                        if (nameRVAoff + 4 > m_fileSize || ordOff + 2 > m_fileSize) break;

                        uint32_t funcNameRVA = *reinterpret_cast<uint32_t*>(&m_binaryData[nameRVAoff]);
                        uint16_t ordIndex    = *reinterpret_cast<uint16_t*>(&m_binaryData[ordOff]);

                        size_t funcNameOff = RvaToFileOffset(funcNameRVA);
                        if (funcNameOff != 0 && funcNameOff < m_fileSize) {
                            std::string funcName;
                            for (size_t c = funcNameOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                funcName += static_cast<char>(m_binaryData[c]);
                                if (funcName.size() > 512) break;
                            }
                            ordinalToName[ordIndex] = funcName;
                        }
                    }
                }

                // Walk the Export Address Table
                if (addrTableOff != 0) {
                    for (uint32_t f = 0; f < numberOfFunctions; ++f) {
                        size_t entryOff = addrTableOff + f * 4;
                        if (entryOff + 4 > m_fileSize) break;

                        uint32_t funcRVA = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff]);
                        if (funcRVA == 0) continue;

                        Export exp;
                        exp.ordinal = static_cast<uint16_t>(ordinalBase + f);
                        exp.address = imageBase + funcRVA;

                        // Check if this is a forwarder (RVA points within export dir)
                        if (funcRVA >= exportDirRVA && funcRVA < exportDirRVA + exportDirSize) {
                            exp.isForwarder = true;
                            size_t fwdOff = RvaToFileOffset(funcRVA);
                            if (fwdOff != 0 && fwdOff < m_fileSize) {
                                for (size_t c = fwdOff; c < m_fileSize && m_binaryData[c] != 0; ++c) {
                                    exp.forwarderName += static_cast<char>(m_binaryData[c]);
                                    if (exp.forwarderName.size() > 512) break;
                                }
                            }
                        } else {
                            exp.isForwarder = false;
                        }

                        // Look up name
                        auto it = ordinalToName.find(static_cast<uint16_t>(f));
                        if (it != ordinalToName.end()) {
                            exp.name = it->second;
                        } else {
                            exp.name = "Ordinal_" + std::to_string(exp.ordinal);
                        }

                        m_exports.push_back(exp);

                        // Also add to symbols table
                        Symbol sym;
                        sym.name = exp.name;
                        sym.address = exp.address;
                        sym.size = 0;
                        sym.section = "";
                        sym.type = "export";
                        sym.isMangled = false;
                        sym.demangledName = exp.name;
                        m_symbols.push_back(sym);
                    }
                }
            }
        }

        // Build symbols from imports
        for (const auto& imp : m_imports) {
            Symbol sym;
            sym.name = imp.functionName;
            sym.address = imp.address;
            sym.size = 0;
            sym.section = ".idata";
            sym.type = "import";
            sym.isMangled = false;
            sym.demangledName = imp.functionName;
            m_symbols.push_back(sym);
        }

        return true;
    }

    // ================================================================
    // RVA → File Offset Converter (uses section table)
    // ================================================================
    size_t RvaToFileOffset(uint32_t rva) const {
        for (const auto& section : m_sections) {
            uint32_t secVA = static_cast<uint32_t>(section.virtualAddress);
            uint32_t secVSize = static_cast<uint32_t>(section.virtualSize);
            uint32_t secRawOff = static_cast<uint32_t>(section.rawOffset);
            uint32_t secRawSize = static_cast<uint32_t>(section.rawSize);

            if (rva >= secVA && rva < secVA + std::max(secVSize, secRawSize)) {
                size_t offset = secRawOff + (rva - secVA);
                if (offset < m_fileSize) return offset;
            }
        }
        return 0; // Not found
    }

    bool ParseELF() {
        // Validate ELF header (already checked magic bytes in LoadBinary)
        if (m_fileSize < 64) return false;

        uint8_t elfClass = m_binaryData[4];   // 1 = 32-bit, 2 = 64-bit
        uint8_t elfData  = m_binaryData[5];   // 1 = LE, 2 = BE
        (void)elfData; // We assume LE for now (x86/x64)

        if (elfClass == 2) {
            // ELF64
            m_architecture = "x64";
            m_bitness = 64;

            if (m_fileSize < 64) return false;

            // ELF64 header fields
            uint64_t shoff     = *reinterpret_cast<uint64_t*>(&m_binaryData[40]); // Section header table offset
            uint16_t shentsize = *reinterpret_cast<uint16_t*>(&m_binaryData[58]); // Section header entry size
            uint16_t shnum     = *reinterpret_cast<uint16_t*>(&m_binaryData[60]); // Number of section headers
            uint16_t shstrndx  = *reinterpret_cast<uint16_t*>(&m_binaryData[62]); // Section name string table index

            if (shoff == 0 || shnum == 0) return true; // No sections, but valid ELF
            if (shoff + static_cast<uint64_t>(shnum) * shentsize > m_fileSize) return false;
            if (shentsize < 64) return false;

            // Read section name string table first
            std::vector<uint8_t> shstrtab;
            if (shstrndx < shnum) {
                size_t strSecOff = static_cast<size_t>(shoff + shstrndx * shentsize);
                if (strSecOff + 64 <= m_fileSize) {
                    uint64_t strOff  = *reinterpret_cast<uint64_t*>(&m_binaryData[strSecOff + 24]);
                    uint64_t strSize = *reinterpret_cast<uint64_t*>(&m_binaryData[strSecOff + 32]);
                    if (strOff + strSize <= m_fileSize) {
                        shstrtab.assign(m_binaryData.begin() + strOff,
                                       m_binaryData.begin() + strOff + strSize);
                    }
                }
            }

            // Parse all section headers
            for (uint16_t i = 0; i < shnum; ++i) {
                size_t entryOff = static_cast<size_t>(shoff + i * shentsize);
                if (entryOff + 64 > m_fileSize) break;

                uint32_t sh_name       = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 0]);
                uint32_t sh_type       = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 4]);
                uint64_t sh_flags      = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 8]);
                uint64_t sh_addr       = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 16]);
                uint64_t sh_offset     = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 24]);
                uint64_t sh_size       = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 32]);
                (void)sh_flags;

                Section section;
                // Read name from string table
                if (sh_name < shstrtab.size()) {
                    for (size_t c = sh_name; c < shstrtab.size() && shstrtab[c] != 0; ++c) {
                        section.name += static_cast<char>(shstrtab[c]);
                    }
                } else {
                    section.name = ".section_" + std::to_string(i);
                }

                section.virtualAddress = sh_addr;
                section.virtualSize    = sh_size;
                section.rawOffset      = sh_offset;
                section.rawSize        = sh_size;
                section.characteristics = sh_type;

                // Read section data (SHT_PROGBITS=1, SHT_SYMTAB=2, SHT_STRTAB=3, etc.)
                if (sh_type != 8 /* SHT_NOBITS */ && sh_offset + sh_size <= m_fileSize && sh_size > 0) {
                    section.data.assign(m_binaryData.begin() + sh_offset,
                                       m_binaryData.begin() + sh_offset + sh_size);
                }

                m_sections.push_back(section);

                // Parse symbol table (SHT_SYMTAB=2 or SHT_DYNSYM=11)
                if (sh_type == 2 || sh_type == 11) {
                    uint32_t sh_link    = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 40]);
                    uint64_t sh_entsize = *reinterpret_cast<uint64_t*>(&m_binaryData[entryOff + 56]);
                    if (sh_entsize == 0) sh_entsize = 24; // Standard Elf64_Sym size

                    // Get the linked string table
                    std::vector<uint8_t> symstrtab;
                    if (sh_link < shnum) {
                        size_t linkOff = static_cast<size_t>(shoff + sh_link * shentsize);
                        if (linkOff + 64 <= m_fileSize) {
                            uint64_t linkDataOff  = *reinterpret_cast<uint64_t*>(&m_binaryData[linkOff + 24]);
                            uint64_t linkDataSize = *reinterpret_cast<uint64_t*>(&m_binaryData[linkOff + 32]);
                            if (linkDataOff + linkDataSize <= m_fileSize) {
                                symstrtab.assign(m_binaryData.begin() + linkDataOff,
                                                m_binaryData.begin() + linkDataOff + linkDataSize);
                            }
                        }
                    }

                    // Walk symbol entries
                    uint64_t numSyms = (sh_entsize > 0) ? sh_size / sh_entsize : 0;
                    for (uint64_t s = 0; s < numSyms && s < 65536; ++s) {
                        size_t symOff = static_cast<size_t>(sh_offset + s * sh_entsize);
                        if (symOff + 24 > m_fileSize) break;

                        uint32_t st_name  = *reinterpret_cast<uint32_t*>(&m_binaryData[symOff + 0]);
                        uint8_t  st_info  = m_binaryData[symOff + 4];
                        uint64_t st_value = *reinterpret_cast<uint64_t*>(&m_binaryData[symOff + 8]);
                        uint64_t st_size  = *reinterpret_cast<uint64_t*>(&m_binaryData[symOff + 16]);

                        uint8_t st_type = st_info & 0x0F;
                        // STT_FUNC=2, STT_OBJECT=1

                        Symbol sym;
                        if (st_name < symstrtab.size()) {
                            for (size_t c = st_name; c < symstrtab.size() && symstrtab[c] != 0; ++c) {
                                sym.name += static_cast<char>(symstrtab[c]);
                            }
                        }
                        if (sym.name.empty()) continue;

                        sym.address = st_value;
                        sym.size    = st_size;
                        sym.type    = (st_type == 2) ? "function" : "data";
                        sym.isMangled = (sym.name.size() > 2 && sym.name[0] == '_' && sym.name[1] == 'Z');
                        sym.demangledName = sym.name; // Demangling would require __cxa_demangle

                        m_symbols.push_back(sym);
                    }
                }
            }
        } else if (elfClass == 1) {
            // ELF32
            m_architecture = "x86";
            m_bitness = 32;
            // Minimal ELF32 support — parse sections only
            if (m_fileSize < 52) return false;
            uint32_t shoff     = *reinterpret_cast<uint32_t*>(&m_binaryData[32]);
            uint16_t shentsize = *reinterpret_cast<uint16_t*>(&m_binaryData[46]);
            uint16_t shnum     = *reinterpret_cast<uint16_t*>(&m_binaryData[48]);
            if (shoff == 0 || shnum == 0) return true;
            if (shentsize < 40) return false;

            for (uint16_t i = 0; i < shnum; ++i) {
                size_t entryOff = shoff + i * shentsize;
                if (entryOff + 40 > m_fileSize) break;

                Section section;
                section.name           = ".elf32_sec_" + std::to_string(i);
                section.virtualAddress = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 12]);
                section.virtualSize    = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 20]);
                section.rawOffset      = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 16]);
                section.rawSize        = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 20]);
                section.characteristics = *reinterpret_cast<uint32_t*>(&m_binaryData[entryOff + 4]);

                uint32_t sh_type = section.characteristics;
                if (sh_type != 8 && section.rawOffset + section.rawSize <= m_fileSize && section.rawSize > 0) {
                    section.data.assign(m_binaryData.begin() + section.rawOffset,
                                       m_binaryData.begin() + section.rawOffset + section.rawSize);
                }
                m_sections.push_back(section);
            }
        } else {
            return false; // Unknown ELF class
        }

        return true;
    }

    const Section* FindSectionByVA(uint64_t va) const {
        for (const auto& section : m_sections) {
            if (va >= section.virtualAddress && 
                va < section.virtualAddress + section.virtualSize) {
                return &section;
            }
        }
        return nullptr;
    }

    // ========================================================================
    // Branch Target Parser (for CFG recovery)
    // ========================================================================

    static uint64_t ParseBranchTarget(const std::string& operands) {
        if (operands.empty()) return 0;
        // Look for "0x" hex address
        size_t pos = operands.find("0x");
        if (pos == std::string::npos) pos = operands.find("0X");
        if (pos == std::string::npos) return 0;
        try {
            return std::stoull(operands.substr(pos), nullptr, 16);
        } catch (...) {
            return 0;
        }
    }

    // ========================================================================
    // Itanium ABI Demangler (_Z prefix)
    // ========================================================================

    std::string DemangleItanium(const std::string& mangled) const {
        // Parse the Itanium ABI mangling scheme
        // _Z<encoding>
        // <encoding> = <function name><bare-function-type>
        // <function name> = <nested-name> | <unscoped-name>
        // <unscoped-name> = <unqualified-name> | St <unqualified-name>
        // <unqualified-name> = <length><identifier>

        if (mangled.size() < 3) return mangled;
        size_t pos = 2; // skip "_Z"

        std::string result;
        bool isNested = false;

        // Check for nested name: N <qualifier>+ <unqualified-name> E
        if (pos < mangled.size() && mangled[pos] == 'N') {
            isNested = true;
            pos++;

            // Skip CV-qualifiers: r(restrict), V(volatile), K(const)
            while (pos < mangled.size() && (mangled[pos] == 'r' || mangled[pos] == 'V' || mangled[pos] == 'K')) {
                pos++;
            }

            // Read sequence of <source-name> until 'E'
            std::vector<std::string> parts;
            while (pos < mangled.size() && mangled[pos] != 'E') {
                if (mangled[pos] == 'S') {
                    // Substitution — simplified
                    pos++; // skip S
                    if (pos < mangled.size() && mangled[pos] == 't') {
                        parts.push_back("std");
                        pos++;
                    } else if (pos < mangled.size() && mangled[pos] == '_') {
                        pos++;
                    } else {
                        while (pos < mangled.size() && mangled[pos] != '_' && mangled[pos] != 'E') pos++;
                        if (pos < mangled.size() && mangled[pos] == '_') pos++;
                    }
                    continue;
                }
                if (mangled[pos] >= '0' && mangled[pos] <= '9') {
                    size_t len = 0;
                    while (pos < mangled.size() && mangled[pos] >= '0' && mangled[pos] <= '9') {
                        len = len * 10 + (mangled[pos] - '0');
                        pos++;
                    }
                    if (pos + len <= mangled.size()) {
                        parts.push_back(mangled.substr(pos, len));
                        pos += len;
                    } else {
                        break;
                    }
                } else if (mangled[pos] == 'D' || mangled[pos] == 'C') {
                    // Constructor/Destructor
                    char kind = mangled[pos]; pos++;
                    if (pos < mangled.size()) pos++; // skip variant number (0,1,2)
                    if (!parts.empty()) {
                        if (kind == 'D') parts.push_back("~" + parts.back());
                        else parts.push_back(parts.back());
                    }
                } else {
                    break; // Unknown production
                }
            }
            if (pos < mangled.size() && mangled[pos] == 'E') pos++;

            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) result += "::";
                result += parts[i];
            }
        }
        // Unscoped name: _Z<length><name><types...>
        else if (pos < mangled.size() && mangled[pos] == 'L') {
            // Local name — skip L
            pos++;
            // Fall through to length-prefixed name
        }

        if (!isNested && result.empty()) {
            // Simple unqualified name: <length><identifier>
            if (pos < mangled.size() && mangled[pos] >= '0' && mangled[pos] <= '9') {
                size_t len = 0;
                while (pos < mangled.size() && mangled[pos] >= '0' && mangled[pos] <= '9') {
                    len = len * 10 + (mangled[pos] - '0');
                    pos++;
                }
                if (pos + len <= mangled.size()) {
                    result = mangled.substr(pos, len);
                    pos += len;
                }
            }
        }

        // Parse parameter types (simplified — just the common ones)
        if (pos < mangled.size() && !result.empty()) {
            result += "(";
            bool first = true;
            while (pos < mangled.size()) {
                std::string paramType;
                // Basic types
                switch (mangled[pos]) {
                    case 'v': paramType = "void"; pos++; break;
                    case 'b': paramType = "bool"; pos++; break;
                    case 'c': paramType = "char"; pos++; break;
                    case 'a': paramType = "signed char"; pos++; break;
                    case 'h': paramType = "unsigned char"; pos++; break;
                    case 's': paramType = "short"; pos++; break;
                    case 't': paramType = "unsigned short"; pos++; break;
                    case 'i': paramType = "int"; pos++; break;
                    case 'j': paramType = "unsigned int"; pos++; break;
                    case 'l': paramType = "long"; pos++; break;
                    case 'm': paramType = "unsigned long"; pos++; break;
                    case 'x': paramType = "long long"; pos++; break;
                    case 'y': paramType = "unsigned long long"; pos++; break;
                    case 'f': paramType = "float"; pos++; break;
                    case 'd': paramType = "double"; pos++; break;
                    case 'e': paramType = "long double"; pos++; break;
                    case 'P': paramType = "ptr "; pos++; continue; // pointer prefix
                    case 'R': paramType = "ref "; pos++; continue; // reference prefix
                    case 'K': pos++; continue; // const qualifier prefix
                    default: pos = mangled.size(); break; // Unknown — bail
                }
                if (!paramType.empty()) {
                    if (!first) result += ", ";
                    result += paramType;
                    first = false;
                }
            }
            result += ")";
        }

        return result.empty() ? mangled : result;
    }

    // ========================================================================
    // MSVC Demangler (? prefix — basic support)
    // ========================================================================

    std::string DemangleMSVC(const std::string& mangled) const {
        // MSVC format: ?name@namespace@namespace@@<type-info>
        // Simplified parser — extracts qualified name

        if (mangled.size() < 2 || mangled[0] != '?') return mangled;

        size_t pos = 1;
        std::vector<std::string> parts;

        while (pos < mangled.size() && mangled[pos] != '@') {
            // Read name until '@'
            size_t start = pos;
            while (pos < mangled.size() && mangled[pos] != '@') pos++;
            if (pos > start) {
                parts.push_back(mangled.substr(start, pos - start));
            }
            if (pos < mangled.size()) pos++; // skip '@'
        }

        // Continue reading namespace parts
        while (pos < mangled.size() && mangled[pos] != '@') {
            size_t start = pos;
            while (pos < mangled.size() && mangled[pos] != '@') pos++;
            if (pos > start) {
                parts.push_back(mangled.substr(start, pos - start));
            }
            if (pos < mangled.size()) pos++; // skip '@'
        }

        if (parts.empty()) return mangled;

        // MSVC stores names in reverse order (innermost first)
        std::string result;
        for (int i = static_cast<int>(parts.size()) - 1; i >= 0; --i) {
            if (!result.empty()) result += "::";
            result += parts[i];
        }

        return result.empty() ? mangled : result;
    }

    // ========================================================================
    // x64 Instruction Decoder — ~80+ opcode families
    // Handles REX prefixes, ModR/M, SIB, immediate/displacement operands,
    // 0x0F two-byte opcodes, conditional jumps, arithmetic, string ops,
    // and common SSE/AVX prefixes.
    // ========================================================================

    // Helper: decode ModR/M byte and return total extra bytes consumed
    // (ModR/M + optional SIB + displacement)
    static size_t ModRMLength(const uint8_t* ptr, size_t maxLen, std::string& regStr, std::string& rmStr, bool hasREX_W, uint8_t rexB, uint8_t rexR) {
        if (maxLen < 1) return 0;

        static const char* gpr64[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                                       "r8","r9","r10","r11","r12","r13","r14","r15"};
        static const char* gpr32[] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi",
                                       "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"};
        static const char* gpr8[]  = {"al","cl","dl","bl","spl","bpl","sil","dil",
                                       "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"};

        uint8_t modrm = ptr[0];
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t reg = ((modrm >> 3) & 7) | (rexR << 3);
        uint8_t rm  = (modrm & 7) | (rexB << 3);

        auto regName = [&](uint8_t idx) -> const char* {
            if (hasREX_W) return gpr64[idx & 0xF];
            return gpr32[idx & 0xF];
        };

        regStr = regName(reg);
        size_t extra = 1; // ModR/M byte itself

        if (mod == 3) {
            // Register-register
            rmStr = regName(rm);
        } else {
            // Memory operand
            bool hasSIB = ((rm & 7) == 4);
            if (hasSIB && extra < maxLen) {
                extra++; // SIB byte
            }
            if (mod == 0 && (rm & 7) == 5) {
                // RIP-relative or disp32
                rmStr = "[rip+disp32]";
                extra += 4;
            } else if (mod == 0) {
                rmStr = "[" + std::string(regName(rm)) + "]";
            } else if (mod == 1) {
                rmStr = "[" + std::string(regName(rm)) + "+disp8]";
                extra += 1;
            } else if (mod == 2) {
                rmStr = "[" + std::string(regName(rm)) + "+disp32]";
                extra += 4;
            }
        }

        if (extra > maxLen) extra = maxLen;
        return extra;
    }

    size_t DecodeInstruction(const uint8_t* ptr, size_t maxLen, DisassemblyLine& out) {
        if (maxLen < 1) return 0;

        size_t pos = 0;

        // ---- Consume legacy prefixes ----
        bool hasOperandSizeOverride = false;
        bool hasAddrSizeOverride = false;
        bool hasREPNE = false;     // 0xF2 — also SSE prefix
        bool hasREP   = false;     // 0xF3 — also SSE prefix
        bool hasLock  = false;
        uint8_t segOverride = 0;

        while (pos < maxLen) {
            uint8_t b = ptr[pos];
            if (b == 0x66)      { hasOperandSizeOverride = true; pos++; }
            else if (b == 0x67) { hasAddrSizeOverride = true; pos++; }
            else if (b == 0xF2) { hasREPNE = true; pos++; }
            else if (b == 0xF3) { hasREP = true; pos++; }
            else if (b == 0xF0) { hasLock = true; pos++; }
            else if (b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 ||
                     b == 0x64 || b == 0x65) { segOverride = b; pos++; }
            else break;
            if (pos > 4) break; // Max 4 legacy prefixes
        }
        (void)hasAddrSizeOverride; (void)segOverride; (void)hasLock;

        // ---- Consume REX prefix (0x40-0x4F) ----
        bool hasREX = false;
        uint8_t rexW = 0, rexR = 0, rexX = 0, rexB = 0;
        if (pos < maxLen && (ptr[pos] & 0xF0) == 0x40) {
            hasREX = true;
            uint8_t rex = ptr[pos];
            rexW = (rex >> 3) & 1;
            rexR = (rex >> 2) & 1;
            rexX = (rex >> 1) & 1;
            rexB = rex & 1;
            pos++;
        }
        (void)rexX; (void)hasREX;
        bool operand64 = (rexW == 1);
        bool operand16 = (!operand64 && hasOperandSizeOverride);
        (void)operand16;

        if (pos >= maxLen) {
            out.mnemonic = "db";
            for (size_t i = 0; i < pos && i < maxLen; ++i) out.bytes.push_back(ptr[i]);
            return pos;
        }

        uint8_t opcode = ptr[pos++];
        auto pushBytes = [&](size_t count) {
            for (size_t i = 0; i < count && i < maxLen; ++i) out.bytes.push_back(ptr[i]);
        };

        // Helper: read imm8 at pos
        auto readImm8 = [&]() -> int8_t {
            if (pos < maxLen) return static_cast<int8_t>(ptr[pos++]);
            return 0;
        };
        auto readImm32 = [&]() -> int32_t {
            if (pos + 4 <= maxLen) {
                int32_t v = *reinterpret_cast<const int32_t*>(ptr + pos);
                pos += 4;
                return v;
            }
            return 0;
        };
        auto readImm64 = [&]() -> int64_t {
            if (pos + 8 <= maxLen) {
                int64_t v = *reinterpret_cast<const int64_t*>(ptr + pos);
                pos += 8;
                return v;
            }
            return 0;
        };

        auto hexAddr = [](uint64_t baseAddr, int64_t disp) -> std::string {
            std::ostringstream s;
            s << "0x" << std::hex << (baseAddr + disp);
            return s.str();
        };

        static const char* gpr64_names[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
                                             "r8","r9","r10","r11","r12","r13","r14","r15"};
        static const char* gpr32_names[] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi",
                                             "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"};

        auto regName64 = [&](uint8_t idx) -> const char* { return gpr64_names[idx & 0xF]; };
        auto regName32 = [&](uint8_t idx) -> const char* { return gpr32_names[idx & 0xF]; };

        // ======== SINGLE-BYTE OPCODES ========

        // NOP
        if (opcode == 0x90) {
            out.mnemonic = "nop";
            pushBytes(pos); return pos;
        }
        // RET near
        if (opcode == 0xC3) {
            out.mnemonic = "ret";
            pushBytes(pos); return pos;
        }
        // RET far
        if (opcode == 0xCB) {
            out.mnemonic = "retf";
            pushBytes(pos); return pos;
        }
        // RET imm16
        if (opcode == 0xC2) {
            out.mnemonic = "ret";
            if (pos + 2 <= maxLen) {
                uint16_t imm = *reinterpret_cast<const uint16_t*>(ptr + pos);
                pos += 2;
                out.operands = "0x" + std::to_string(imm);
            }
            pushBytes(pos); return pos;
        }
        // INT3
        if (opcode == 0xCC) {
            out.mnemonic = "int3";
            pushBytes(pos); return pos;
        }
        // INT imm8
        if (opcode == 0xCD) {
            out.mnemonic = "int";
            out.operands = "0x" + std::to_string(static_cast<uint8_t>(readImm8()));
            pushBytes(pos); return pos;
        }
        // HLT
        if (opcode == 0xF4) {
            out.mnemonic = "hlt";
            pushBytes(pos); return pos;
        }
        // CLC / STC / CLI / STI / CLD / STD
        if (opcode == 0xF8) { out.mnemonic = "clc"; pushBytes(pos); return pos; }
        if (opcode == 0xF9) { out.mnemonic = "stc"; pushBytes(pos); return pos; }
        if (opcode == 0xFA) { out.mnemonic = "cli"; pushBytes(pos); return pos; }
        if (opcode == 0xFB) { out.mnemonic = "sti"; pushBytes(pos); return pos; }
        if (opcode == 0xFC) { out.mnemonic = "cld"; pushBytes(pos); return pos; }
        if (opcode == 0xFD) { out.mnemonic = "std"; pushBytes(pos); return pos; }
        // CBW/CWDE/CDQE
        if (opcode == 0x98) { out.mnemonic = operand64 ? "cdqe" : "cwde"; pushBytes(pos); return pos; }
        // CWD/CDQ/CQO
        if (opcode == 0x99) { out.mnemonic = operand64 ? "cqo" : "cdq"; pushBytes(pos); return pos; }
        // SAHF / LAHF
        if (opcode == 0x9E) { out.mnemonic = "sahf"; pushBytes(pos); return pos; }
        if (opcode == 0x9F) { out.mnemonic = "lahf"; pushBytes(pos); return pos; }
        // LEAVE
        if (opcode == 0xC9) { out.mnemonic = "leave"; pushBytes(pos); return pos; }

        // PUSH r64 (0x50-0x57, extended by REX.B)
        if (opcode >= 0x50 && opcode <= 0x57) {
            out.mnemonic = "push";
            uint8_t reg = (opcode - 0x50) | (rexB << 3);
            out.operands = regName64(reg);
            pushBytes(pos); return pos;
        }
        // POP r64 (0x58-0x5F)
        if (opcode >= 0x58 && opcode <= 0x5F) {
            out.mnemonic = "pop";
            uint8_t reg = (opcode - 0x58) | (rexB << 3);
            out.operands = regName64(reg);
            pushBytes(pos); return pos;
        }
        // PUSH imm8
        if (opcode == 0x6A) {
            out.mnemonic = "push";
            out.operands = std::to_string(readImm8());
            pushBytes(pos); return pos;
        }
        // PUSH imm32
        if (opcode == 0x68) {
            out.mnemonic = "push";
            out.operands = std::to_string(readImm32());
            pushBytes(pos); return pos;
        }

        // XCHG r64, rax (0x91-0x97)
        if (opcode >= 0x91 && opcode <= 0x97) {
            out.mnemonic = "xchg";
            uint8_t reg = (opcode - 0x90) | (rexB << 3);
            out.operands = std::string(regName64(reg)) + ", rax";
            pushBytes(pos); return pos;
        }

        // MOV r64, imm64 (0xB8-0xBF with REX.W) or MOV r32, imm32
        if (opcode >= 0xB8 && opcode <= 0xBF) {
            out.mnemonic = "mov";
            uint8_t reg = (opcode - 0xB8) | (rexB << 3);
            if (operand64) {
                int64_t imm = readImm64();
                std::ostringstream s;
                s << regName64(reg) << ", 0x" << std::hex << static_cast<uint64_t>(imm);
                out.operands = s.str();
            } else {
                int32_t imm = readImm32();
                std::ostringstream s;
                s << regName32(reg) << ", 0x" << std::hex << static_cast<uint32_t>(imm);
                out.operands = s.str();
            }
            pushBytes(pos); return pos;
        }
        // MOV r8, imm8 (0xB0-0xB7)
        if (opcode >= 0xB0 && opcode <= 0xB7) {
            static const char* gpr8n[] = {"al","cl","dl","bl","ah","ch","dh","bh",
                                           "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b"};
            out.mnemonic = "mov";
            uint8_t reg = (opcode - 0xB0) | (rexB << 3);
            uint8_t imm = static_cast<uint8_t>(readImm8());
            out.operands = std::string(gpr8n[reg & 0xF]) + ", 0x" + std::to_string(imm);
            pushBytes(pos); return pos;
        }

        // CALL rel32
        if (opcode == 0xE8) {
            out.mnemonic = "call";
            int32_t offset = readImm32();
            out.operands = hexAddr(out.address + pos, offset);
            pushBytes(pos); return pos;
        }
        // JMP rel32
        if (opcode == 0xE9) {
            out.mnemonic = "jmp";
            int32_t offset = readImm32();
            out.operands = hexAddr(out.address + pos, offset);
            pushBytes(pos); return pos;
        }
        // JMP rel8
        if (opcode == 0xEB) {
            out.mnemonic = "jmp";
            int8_t offset = readImm8();
            out.operands = hexAddr(out.address + pos, offset);
            pushBytes(pos); return pos;
        }

        // Short conditional jumps (0x70-0x7F) — Jcc rel8
        if (opcode >= 0x70 && opcode <= 0x7F) {
            static const char* ccNames[] = {
                "jo","jno","jb","jnb","jz","jnz","jbe","ja",
                "js","jns","jp","jnp","jl","jnl","jle","jg"
            };
            out.mnemonic = ccNames[opcode - 0x70];
            int8_t offset = readImm8();
            out.operands = hexAddr(out.address + pos, offset);
            pushBytes(pos); return pos;
        }

        // LOOP / LOOPE / LOOPNE / JRCXZ
        if (opcode == 0xE0) { out.mnemonic = "loopne"; int8_t off = readImm8(); out.operands = hexAddr(out.address+pos,off); pushBytes(pos); return pos; }
        if (opcode == 0xE1) { out.mnemonic = "loope";  int8_t off = readImm8(); out.operands = hexAddr(out.address+pos,off); pushBytes(pos); return pos; }
        if (opcode == 0xE2) { out.mnemonic = "loop";   int8_t off = readImm8(); out.operands = hexAddr(out.address+pos,off); pushBytes(pos); return pos; }
        if (opcode == 0xE3) { out.mnemonic = "jrcxz";  int8_t off = readImm8(); out.operands = hexAddr(out.address+pos,off); pushBytes(pos); return pos; }

        // ALU reg, imm — ADD/OR/ADC/SBB/AND/SUB/XOR/CMP AL/AX/EAX/RAX, imm
        // 0x04/0x0C/0x14/0x1C/0x24/0x2C/0x34/0x3C = op AL, imm8
        // 0x05/0x0D/0x15/0x1D/0x25/0x2D/0x35/0x3D = op rAX, imm32
        {
            static const char* aluNames[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
            uint8_t aluIdx = opcode >> 3;
            if (aluIdx < 8) {
                if ((opcode & 7) == 4) {
                    // op AL, imm8
                    out.mnemonic = aluNames[aluIdx];
                    out.operands = std::string("al, ") + std::to_string(static_cast<uint8_t>(readImm8()));
                    pushBytes(pos); return pos;
                }
                if ((opcode & 7) == 5) {
                    // op rAX, imm32
                    out.mnemonic = aluNames[aluIdx];
                    int32_t imm = readImm32();
                    std::ostringstream s;
                    s << (operand64 ? "rax" : "eax") << ", 0x" << std::hex << static_cast<uint32_t>(imm);
                    out.operands = s.str();
                    pushBytes(pos); return pos;
                }
            }
        }

        // ALU with ModR/M: 0x00-0x03, 0x08-0x0B, 0x10-0x13, 0x18-0x1B, 0x20-0x23, 0x28-0x2B, 0x30-0x33, 0x38-0x3B
        {
            static const char* aluNames[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
            uint8_t aluGroup = opcode >> 3;
            uint8_t variant = opcode & 7;
            if (aluGroup < 8 && variant <= 3) {
                out.mnemonic = aluNames[aluGroup];
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    if (variant & 2) {
                        out.operands = regStr + ", " + rmStr; // reg, r/m
                    } else {
                        out.operands = rmStr + ", " + regStr; // r/m, reg
                    }
                }
                pushBytes(pos); return pos;
            }
        }

        // Group 1: 0x80/0x81/0x83 — ALU r/m, imm
        if (opcode == 0x80 || opcode == 0x81 || opcode == 0x83) {
            static const char* aluNames[] = {"add","or","adc","sbb","and","sub","xor","cmp"};
            if (pos < maxLen) {
                uint8_t modrm = ptr[pos];
                uint8_t op = (modrm >> 3) & 7;
                out.mnemonic = aluNames[op];
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                if (opcode == 0x80) {
                    out.operands = rmStr + ", " + std::to_string(static_cast<uint8_t>(readImm8()));
                } else if (opcode == 0x83) {
                    out.operands = rmStr + ", " + std::to_string(readImm8());
                } else {
                    out.operands = rmStr + ", 0x" + std::to_string(static_cast<uint32_t>(readImm32()));
                }
            }
            pushBytes(pos); return pos;
        }

        // MOV r/m, reg (0x89) and MOV reg, r/m (0x8B)
        if (opcode == 0x89 || opcode == 0x8B) {
            out.mnemonic = "mov";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                if (opcode == 0x89) {
                    out.operands = rmStr + ", " + regStr;
                } else {
                    out.operands = regStr + ", " + rmStr;
                }
            }
            pushBytes(pos); return pos;
        }
        // MOV r/m8, reg8 (0x88) and MOV reg8, r/m8 (0x8A)
        if (opcode == 0x88 || opcode == 0x8A) {
            out.mnemonic = "mov";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, false, rexB, rexR);
                pos += extra;
                if (opcode == 0x88) {
                    out.operands = rmStr + ", " + regStr;
                } else {
                    out.operands = regStr + ", " + rmStr;
                }
            }
            pushBytes(pos); return pos;
        }

        // LEA (0x8D)
        if (opcode == 0x8D) {
            out.mnemonic = "lea";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                out.operands = regStr + ", " + rmStr;
            }
            pushBytes(pos); return pos;
        }

        // TEST r/m, reg (0x85) and TEST r/m8, reg8 (0x84)
        if (opcode == 0x84 || opcode == 0x85) {
            out.mnemonic = "test";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, opcode == 0x85 && operand64, rexB, rexR);
                pos += extra;
                out.operands = rmStr + ", " + regStr;
            }
            pushBytes(pos); return pos;
        }
        // TEST AL, imm8 (0xA8) / TEST rAX, imm32 (0xA9)
        if (opcode == 0xA8) {
            out.mnemonic = "test";
            out.operands = std::string("al, ") + std::to_string(static_cast<uint8_t>(readImm8()));
            pushBytes(pos); return pos;
        }
        if (opcode == 0xA9) {
            out.mnemonic = "test";
            int32_t imm = readImm32();
            std::ostringstream s;
            s << (operand64 ? "rax" : "eax") << ", 0x" << std::hex << static_cast<uint32_t>(imm);
            out.operands = s.str();
            pushBytes(pos); return pos;
        }

        // XCHG r/m, reg (0x87)
        if (opcode == 0x87) {
            out.mnemonic = "xchg";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                out.operands = rmStr + ", " + regStr;
            }
            pushBytes(pos); return pos;
        }

        // INC/DEC (Group 4/5 via 0xFE/0xFF)
        if (opcode == 0xFE || opcode == 0xFF) {
            if (pos < maxLen) {
                uint8_t modrm = ptr[pos];
                uint8_t op = (modrm >> 3) & 7;
                static const char* grp5Names[] = {"inc","dec","call","callf","jmp","jmpf","push","??"};
                if (opcode == 0xFE) {
                    out.mnemonic = (op == 0) ? "inc" : "dec"; // Only inc/dec for byte variant
                } else {
                    out.mnemonic = grp5Names[op];
                }
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                out.operands = rmStr;
            }
            pushBytes(pos); return pos;
        }

        // Shift/Rotate Group 2: 0xC0/0xC1/0xD0/0xD1/0xD2/0xD3
        if (opcode == 0xC0 || opcode == 0xC1 || opcode == 0xD0 || opcode == 0xD1 || opcode == 0xD2 || opcode == 0xD3) {
            static const char* shiftNames[] = {"rol","ror","rcl","rcr","shl","shr","sal","sar"};
            if (pos < maxLen) {
                uint8_t modrm = ptr[pos];
                uint8_t op = (modrm >> 3) & 7;
                out.mnemonic = shiftNames[op];
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, (opcode & 1) && operand64, rexB, rexR);
                pos += extra;
                if (opcode == 0xC0 || opcode == 0xC1) {
                    out.operands = rmStr + ", " + std::to_string(static_cast<uint8_t>(readImm8()));
                } else if (opcode == 0xD0 || opcode == 0xD1) {
                    out.operands = rmStr + ", 1";
                } else {
                    out.operands = rmStr + ", cl";
                }
            }
            pushBytes(pos); return pos;
        }

        // NOT/NEG/MUL/IMUL/DIV/IDIV — Group 3: 0xF6/0xF7
        if (opcode == 0xF6 || opcode == 0xF7) {
            static const char* grp3Names[] = {"test","test","not","neg","mul","imul","div","idiv"};
            if (pos < maxLen) {
                uint8_t modrm = ptr[pos];
                uint8_t op = (modrm >> 3) & 7;
                out.mnemonic = grp3Names[op];
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, (opcode == 0xF7) && operand64, rexB, rexR);
                pos += extra;
                if (op <= 1) {
                    // TEST r/m, imm
                    if (opcode == 0xF6) {
                        out.operands = rmStr + ", " + std::to_string(static_cast<uint8_t>(readImm8()));
                    } else {
                        out.operands = rmStr + ", 0x" + std::to_string(static_cast<uint32_t>(readImm32()));
                    }
                } else {
                    out.operands = rmStr;
                }
            }
            pushBytes(pos); return pos;
        }

        // IMUL reg, r/m (0x0F 0xAF) — handled below in 2-byte section
        // IMUL reg, r/m, imm8 (0x6B) or imm32 (0x69)
        if (opcode == 0x69 || opcode == 0x6B) {
            out.mnemonic = "imul";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                pos += extra;
                if (opcode == 0x6B) {
                    out.operands = regStr + ", " + rmStr + ", " + std::to_string(readImm8());
                } else {
                    out.operands = regStr + ", " + rmStr + ", 0x" + std::to_string(static_cast<uint32_t>(readImm32()));
                }
            }
            pushBytes(pos); return pos;
        }

        // MOV r/m, imm (0xC6 byte, 0xC7 dword/qword)
        if (opcode == 0xC6 || opcode == 0xC7) {
            out.mnemonic = "mov";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, (opcode == 0xC7) && operand64, rexB, rexR);
                pos += extra;
                if (opcode == 0xC6) {
                    out.operands = rmStr + ", " + std::to_string(static_cast<uint8_t>(readImm8()));
                } else {
                    out.operands = rmStr + ", 0x" + std::to_string(static_cast<uint32_t>(readImm32()));
                }
            }
            pushBytes(pos); return pos;
        }

        // String instructions
        if (opcode == 0xA4) { out.mnemonic = hasREP ? "rep movsb" : "movsb"; pushBytes(pos); return pos; }
        if (opcode == 0xA5) { out.mnemonic = hasREP ? "rep movsd" : (operand64 ? "movsq" : "movsd"); pushBytes(pos); return pos; }
        if (opcode == 0xA6) { out.mnemonic = hasREPNE ? "repne cmpsb" : (hasREP ? "repe cmpsb" : "cmpsb"); pushBytes(pos); return pos; }
        if (opcode == 0xA7) { out.mnemonic = hasREPNE ? "repne cmpsd" : (hasREP ? "repe cmpsd" : "cmpsd"); pushBytes(pos); return pos; }
        if (opcode == 0xAA) { out.mnemonic = hasREP ? "rep stosb" : "stosb"; pushBytes(pos); return pos; }
        if (opcode == 0xAB) { out.mnemonic = hasREP ? "rep stosd" : (operand64 ? "stosq" : "stosd"); pushBytes(pos); return pos; }
        if (opcode == 0xAC) { out.mnemonic = hasREP ? "rep lodsb" : "lodsb"; pushBytes(pos); return pos; }
        if (opcode == 0xAD) { out.mnemonic = hasREP ? "rep lodsd" : (operand64 ? "lodsq" : "lodsd"); pushBytes(pos); return pos; }
        if (opcode == 0xAE) { out.mnemonic = hasREPNE ? "repne scasb" : (hasREP ? "repe scasb" : "scasb"); pushBytes(pos); return pos; }
        if (opcode == 0xAF) { out.mnemonic = hasREPNE ? "repne scasd" : (hasREP ? "repe scasd" : "scasd"); pushBytes(pos); return pos; }

        // IN/OUT
        if (opcode == 0xE4) { out.mnemonic = "in"; out.operands = "al, " + std::to_string(static_cast<uint8_t>(readImm8())); pushBytes(pos); return pos; }
        if (opcode == 0xE5) { out.mnemonic = "in"; out.operands = "eax, " + std::to_string(static_cast<uint8_t>(readImm8())); pushBytes(pos); return pos; }
        if (opcode == 0xE6) { out.mnemonic = "out"; out.operands = std::to_string(static_cast<uint8_t>(readImm8())) + ", al"; pushBytes(pos); return pos; }
        if (opcode == 0xE7) { out.mnemonic = "out"; out.operands = std::to_string(static_cast<uint8_t>(readImm8())) + ", eax"; pushBytes(pos); return pos; }
        if (opcode == 0xEC) { out.mnemonic = "in"; out.operands = "al, dx"; pushBytes(pos); return pos; }
        if (opcode == 0xED) { out.mnemonic = "in"; out.operands = "eax, dx"; pushBytes(pos); return pos; }
        if (opcode == 0xEE) { out.mnemonic = "out"; out.operands = "dx, al"; pushBytes(pos); return pos; }
        if (opcode == 0xEF) { out.mnemonic = "out"; out.operands = "dx, eax"; pushBytes(pos); return pos; }

        // MOVZX/MOVSX are 2-byte (0x0F B6/B7/BE/BF) — handled below

        // SYSCALL / SYSRET (0x0F 05 / 0x0F 07) — handled below

        // ======== TWO-BYTE OPCODES (0x0F) ========
        if (opcode == 0x0F) {
            if (pos >= maxLen) { out.mnemonic = "db"; out.operands = "0x0F"; pushBytes(pos); return pos; }
            uint8_t op2 = ptr[pos++];

            // Near conditional jumps Jcc rel32 (0x0F 0x80-0x8F)
            if (op2 >= 0x80 && op2 <= 0x8F) {
                static const char* ccNames[] = {
                    "jo","jno","jb","jnb","jz","jnz","jbe","ja",
                    "js","jns","jp","jnp","jl","jnl","jle","jg"
                };
                out.mnemonic = ccNames[op2 - 0x80];
                int32_t offset = readImm32();
                out.operands = hexAddr(out.address + pos, offset);
                pushBytes(pos); return pos;
            }

            // SETcc r/m8 (0x0F 0x90-0x9F)
            if (op2 >= 0x90 && op2 <= 0x9F) {
                static const char* ccNames[] = {
                    "seto","setno","setb","setnb","setz","setnz","setbe","seta",
                    "sets","setns","setp","setnp","setl","setnl","setle","setg"
                };
                out.mnemonic = ccNames[op2 - 0x90];
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, false, rexB, rexR);
                    pos += extra;
                    out.operands = rmStr;
                }
                pushBytes(pos); return pos;
            }

            // CMOVcc reg, r/m (0x0F 0x40-0x4F)
            if (op2 >= 0x40 && op2 <= 0x4F) {
                static const char* ccNames[] = {
                    "cmovo","cmovno","cmovb","cmovnb","cmovz","cmovnz","cmovbe","cmova",
                    "cmovs","cmovns","cmovp","cmovnp","cmovl","cmovnl","cmovle","cmovg"
                };
                out.mnemonic = ccNames[op2 - 0x40];
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }

            // MOVZX (0x0F B6 = byte, 0x0F B7 = word)
            if (op2 == 0xB6 || op2 == 0xB7) {
                out.mnemonic = "movzx";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }
            // MOVSX (0x0F BE = byte, 0x0F BF = word)
            if (op2 == 0xBE || op2 == 0xBF) {
                out.mnemonic = "movsx";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }
            // MOVSXD is 0x63 with REX.W (handled above as single-byte if we add it)

            // IMUL reg, r/m (0x0F AF)
            if (op2 == 0xAF) {
                out.mnemonic = "imul";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }

            // BSF / BSR / TZCNT / LZCNT
            if (op2 == 0xBC) {
                out.mnemonic = hasREP ? "tzcnt" : "bsf";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }
            if (op2 == 0xBD) {
                out.mnemonic = hasREP ? "lzcnt" : "bsr";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = regStr + ", " + rmStr;
                }
                pushBytes(pos); return pos;
            }

            // BT/BTS/BTR/BTC (0x0F A3/AB/B3/BB)
            if (op2 == 0xA3 || op2 == 0xAB || op2 == 0xB3 || op2 == 0xBB) {
                static const std::unordered_map<uint8_t, const char*> btNames = {
                    {0xA3,"bt"},{0xAB,"bts"},{0xB3,"btr"},{0xBB,"btc"}
                };
                out.mnemonic = btNames.at(op2);
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, operand64, rexB, rexR);
                    pos += extra;
                    out.operands = rmStr + ", " + regStr;
                }
                pushBytes(pos); return pos;
            }

            // SYSCALL (0x0F 05) / SYSRET (0x0F 07)
            if (op2 == 0x05) { out.mnemonic = "syscall"; pushBytes(pos); return pos; }
            if (op2 == 0x07) { out.mnemonic = "sysret";  pushBytes(pos); return pos; }

            // CPUID (0x0F A2)
            if (op2 == 0xA2) { out.mnemonic = "cpuid"; pushBytes(pos); return pos; }
            // RDTSC (0x0F 31)
            if (op2 == 0x31) { out.mnemonic = "rdtsc"; pushBytes(pos); return pos; }
            // RDTSCP (0x0F 01 F9) — 3 byte
            if (op2 == 0x01 && pos < maxLen && ptr[pos] == 0xF9) {
                pos++;
                out.mnemonic = "rdtscp";
                pushBytes(pos); return pos;
            }
            // UD2 (0x0F 0B)
            if (op2 == 0x0B) { out.mnemonic = "ud2"; pushBytes(pos); return pos; }

            // BSWAP (0x0F C8+rd)
            if (op2 >= 0xC8 && op2 <= 0xCF) {
                out.mnemonic = "bswap";
                uint8_t reg = (op2 - 0xC8) | (rexB << 3);
                out.operands = operand64 ? regName64(reg) : regName32(reg);
                pushBytes(pos); return pos;
            }

            // CMPXCHG (0x0F B0 = byte, 0x0F B1 = dword/qword)
            if (op2 == 0xB0 || op2 == 0xB1) {
                out.mnemonic = "cmpxchg";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, (op2 == 0xB1) && operand64, rexB, rexR);
                    pos += extra;
                    out.operands = rmStr + ", " + regStr;
                }
                pushBytes(pos); return pos;
            }

            // XADD (0x0F C0 = byte, 0x0F C1 = dword/qword)
            if (op2 == 0xC0 || op2 == 0xC1) {
                out.mnemonic = "xadd";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, (op2 == 0xC1) && operand64, rexB, rexR);
                    pos += extra;
                    out.operands = rmStr + ", " + regStr;
                }
                pushBytes(pos); return pos;
            }

            // NOP with ModR/M (0x0F 1F — multi-byte NOP)
            if (op2 == 0x1F) {
                out.mnemonic = "nop";
                if (pos < maxLen) {
                    std::string regStr, rmStr;
                    size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, false, rexB, rexR);
                    pos += extra;
                    out.operands = rmStr;
                }
                pushBytes(pos); return pos;
            }

            // Fallback for unrecognized 0x0F opcodes
            out.mnemonic = "db";
            std::ostringstream s;
            s << "0x0F, 0x" << std::hex << static_cast<int>(op2);
            out.operands = s.str();
            pushBytes(pos); return pos;
        }

        // MOVSXD (0x63 with REX.W)
        if (opcode == 0x63 && operand64) {
            out.mnemonic = "movsxd";
            if (pos < maxLen) {
                std::string regStr, rmStr;
                size_t extra = ModRMLength(ptr + pos, maxLen - pos, regStr, rmStr, true, rexB, rexR);
                pos += extra;
                out.operands = regStr + ", " + rmStr;
            }
            pushBytes(pos); return pos;
        }

        // ======== DEFAULT: unknown opcode ========
        out.mnemonic = "db";
        std::ostringstream s;
        s << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(opcode);
        out.operands = s.str();
        pushBytes(pos);
        return pos;
    }
};

} // namespace ReverseEngineering
} // namespace RawrXD
