// =============================================================================
// RAWRXD_BACKEND.H - Pure C++20 Backend Infrastructure
// =============================================================================
// Register Allocator | Optimizer | SEH Exception | RTTI | Runtime
// No external dependencies - Pure C++20
// =============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <memory>
#include <functional>
#include <optional>

namespace RawrXD {
namespace Backend {

// =============================================================================
// INTERMEDIATE REPRESENTATION
// =============================================================================

namespace IR {

enum class TypeClass : uint8_t {
    Void, Bool, I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64, F80,
    Ptr, Func, Struct, Array
};

struct IRType {
    TypeClass cls = TypeClass::Void;
    uint8_t bits = 0;
    uint16_t elements = 0;  // For arrays/vectors
    uint32_t align = 1;
    uint32_t size = 0;
    
    static IRType Void() { return {TypeClass::Void, 0, 0, 1, 0}; }
    static IRType Bool() { return {TypeClass::Bool, 1, 0, 1, 1}; }
    static IRType I8()   { return {TypeClass::I8, 8, 0, 1, 1}; }
    static IRType I16()  { return {TypeClass::I16, 16, 0, 2, 2}; }
    static IRType I32()  { return {TypeClass::I32, 32, 0, 4, 4}; }
    static IRType I64()  { return {TypeClass::I64, 64, 0, 8, 8}; }
    static IRType U8()   { return {TypeClass::U8, 8, 0, 1, 1}; }
    static IRType U16()  { return {TypeClass::U16, 16, 0, 2, 2}; }
    static IRType U32()  { return {TypeClass::U32, 32, 0, 4, 4}; }
    static IRType U64()  { return {TypeClass::U64, 64, 0, 8, 8}; }
    static IRType F32()  { return {TypeClass::F32, 32, 0, 4, 4}; }
    static IRType F64()  { return {TypeClass::F64, 64, 0, 8, 8}; }
    static IRType Ptr()  { return {TypeClass::Ptr, 64, 0, 8, 8}; }
    
    bool isInteger() const {
        return cls >= TypeClass::I8 && cls <= TypeClass::U64;
    }
    bool isFloat() const { return cls >= TypeClass::F32 && cls <= TypeClass::F80; }
    bool isPointer() const { return cls == TypeClass::Ptr; }
    bool isSigned() const {
        return cls >= TypeClass::I8 && cls <= TypeClass::I64;
    }
};

struct Value;
struct Instruction;
struct BasicBlock;
struct Function;

struct Use {
    Value* val = nullptr;
    Instruction* user = nullptr;
    uint32_t operandIndex = 0;
};

struct Value {
    uint32_t id = 0;
    IRType type;
    std::vector<Use*> uses;
    
    Value(IRType t) : type(t) { static uint32_t nextId = 0; id = nextId++; }
    virtual ~Value() = default;
    
    void addUse(Use* use) { uses.push_back(use); }
    void removeUse(Use* use) {
        auto it = std::find(uses.begin(), uses.end(), use);
        if (it != uses.end()) uses.erase(it);
    }
    
    void replaceAllUsesWith(Value* newVal);
};

struct Constant : public Value {
    enum class Kind { Int, Float, Null, Undef } kind;
    union {
        int64_t intVal;
        double floatVal;
    };
    
    static Constant* createInt(IRType type, int64_t val);
    static Constant* createFloat(IRType type, double val);
    static Constant* createNull(IRType type);
    
    Constant(IRType t, Kind k) : Value(t), kind(k) {}
};

struct Instruction : public Value {
    enum class Opcode : uint16_t {
        // Terminators
        Ret, Br, CondBr, Switch, Unreachable,
        // Binary
        Add, Sub, Mul, SDiv, UDiv, SRem, URem,
        FAdd, FSub, FMul, FDiv, FRem,
        And, Or, Xor, Shl, LShr, AShr,
        // Comparison
        ICmp, FCmp,
        // Memory
        Alloca, Load, Store, GetElementPtr, MemCpy, MemSet,
        // Conversion
        Trunc, ZExt, SExt, FPToUI, FPToSI, UIToFP, SIToFP, FPTrunc, FPExt,
        BitCast, PtrToInt, IntToPtr,
        // Control
        Call, Invoke, LandingPad, Resume,
        // Other
        Phi, Select, ExtractValue, InsertValue,
        // Atomic
        AtomicRMW, CmpXchg, Fence
    };
    
    enum class ICmpPred {
        EQ, NE, SLT, SLE, SGT, SGE,
        ULT, ULE, UGT, UGE
    };
    
    enum class FCmpPred {
        OEQ, ONE, OLT, OLE, OGT, OGE,
        UEQ, UNE, ULT, ULE, UGT, UGE,
        ORD, UNO, True, False
    };
    
    Opcode opcode;
    BasicBlock* parent = nullptr;
    std::vector<Use*> operands;
    std::vector<BasicBlock*> successors;
    std::vector<BasicBlock*> predecessors;
    
    bool isTerminator() const {
        return opcode >= Opcode::Ret && opcode <= Opcode::Unreachable;
    }
    
    bool mayWriteMemory() const {
        return opcode == Opcode::Store || opcode == Opcode::MemCpy ||
               opcode == Opcode::MemSet || opcode == Opcode::AtomicRMW ||
               opcode == Opcode::CmpXchg || opcode == Opcode::Call;
    }
    
    bool mayReadMemory() const {
        return opcode == Opcode::Load || opcode == Opcode::GetElementPtr ||
               opcode == Opcode::MemCpy || opcode == Opcode::Call;
    }
    
    Instruction(Opcode op, IRType t) : Value(t), opcode(op) {}
    
    void setOperand(uint32_t idx, Value* val);
    Value* getOperand(uint32_t idx) const {
        return idx < operands.size() ? operands[idx]->val : nullptr;
    }
};

struct BasicBlock {
    uint32_t id = 0;
    std::string name;
    Function* parent = nullptr;
    std::vector<std::unique_ptr<Instruction>> instructions;
    std::vector<BasicBlock*> predecessors;
    std::vector<BasicBlock*> successors;
    
    Instruction* terminator() {
        return instructions.empty() ? nullptr : instructions.back().get();
    }
    
    void append(std::unique_ptr<Instruction> inst) {
        inst->parent = this;
        instructions.push_back(std::move(inst));
    }
    
    void insertBefore(Instruction* before, std::unique_ptr<Instruction> inst);
    void remove(Instruction* inst);
    
    BasicBlock(const std::string& n = "") : name(n) {
        static uint32_t nextId = 0; id = nextId++;
    }
};

struct Function {
    std::string name;
    IRType returnType;
    std::vector<std::pair<IRType, std::string>> parameters;
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::vector<Value*> args;
    bool isVarArg = false;
    bool isExternal = false;
    bool isNoReturn = false;
    uint32_t stackSize = 0;
    
    Function(const std::string& n, IRType ret) : name(n), returnType(ret) {}
    
    BasicBlock* addBlock(const std::string& name = "") {
        auto* bb = new BasicBlock(name);
        bb->parent = this;
        blocks.emplace_back(bb);
        return bb;
    }
    
    BasicBlock* entry() { return blocks.empty() ? nullptr : blocks[0].get(); }
};

struct Module {
    std::string name;
    std::vector<std::unique_ptr<Function>> functions;
    std::unordered_map<std::string, Function*> functionMap;
    
    Function* getFunction(const std::string& name) {
        auto it = functionMap.find(name);
        return it != functionMap.end() ? it->second : nullptr;
    }
    
    Function* addFunction(const std::string& name, IRType ret) {
        auto* f = new Function(name, ret);
        functionMap[name] = f;
        functions.emplace_back(f);
        return f;
    }
};

// =============================================================================
// REGISTER ALLOCATOR - Linear Scan with Spilling
// =============================================================================

namespace CodeGen {

enum class RegClass : uint8_t { General, Float, Vector };

struct Register {
    uint8_t id;
    RegClass rclass;
    bool isAllocated = false;
    bool isCalleeSaved = false;
    std::string name() const;
    
    static constexpr uint8_t RAX = 0, RCX = 1, RDX = 2, RBX = 3;
    static constexpr uint8_t RSP = 4, RBP = 5, RSI = 6, RDI = 7;
    static constexpr uint8_t R8 = 8, R9 = 9, R10 = 10, R11 = 11;
    static constexpr uint8_t R12 = 12, R13 = 13, R14 = 14, R15 = 15;
    static constexpr uint8_t XMM0 = 0, XMM1 = 1, XMM2 = 2, XMM3 = 3;
    static constexpr uint8_t XMM4 = 4, XMM5 = 5, XMM6 = 6, XMM7 = 7;
    static constexpr uint8_t XMM8 = 8, XMM9 = 9, XMM10 = 10, XMM11 = 11;
    static constexpr uint8_t XMM12 = 12, XMM13 = 13, XMM14 = 14, XMM15 = 15;
    static constexpr uint8_t Invalid = 0xFF;
};

struct LiveInterval {
    Value* value = nullptr;
    uint32_t start = 0;
    uint32_t end = 0;
    int8_t reg = -1;        // Assigned register (-1 = unassigned)
    int32_t spillSlot = -1; // Stack slot for spilling
    uint32_t weight = 0;    // Priority weight
    
    bool operator<(const LiveInterval& other) const {
        return start < other.start;
    }
    
    bool covers(uint32_t pos) const {
        return pos >= start && pos <= end;
    }
    
    bool overlaps(const LiveInterval& other) const {
        return start < other.end && other.start < end;
    }
};

struct VRegInfo {
    uint32_t vregId = 0;
    RegClass rclass = RegClass::General;
    bool mustSpill = false;
    std::vector<uint32_t> defPoints;
    std::vector<uint32_t> usePoints;
};

class RegisterAllocator {
public:
    struct Config {
        uint32_t numGeneralRegs = 14;  // Exclude RSP, RBP
        uint32_t numFloatRegs = 16;    // XMM0-XMM15
        bool preserveRBX = true;
        bool preserveRSI = true;
        bool preserveRDI = true;
        bool preserveR12_R15 = true;
    };
    
    RegisterAllocator(const Config& cfg = {}) : config_(cfg) {}
    
    bool allocate(Function* func);
    
    // Results
    int8_t getRegister(Value* val) const;
    int32_t getStackSlot(Value* val) const;
    uint32_t getStackFrameSize() const { return stackSize_; }
    
    // Register masks
    static constexpr uint16_t GENERAL_REGS_MASK = 
        (1 << 14) - 1;  // RAX-R15 excluding RSP, RBP
    
private:
    Config config_;
    Function* func_ = nullptr;
    uint32_t stackSize_ = 0;
    uint32_t nextSlot_ = 0;
    
    std::vector<LiveInterval> intervals_;
    std::unordered_map<Value*, VRegInfo> vregInfo_;
    std::unordered_map<Value*, int8_t> regAssignment_;
    std::unordered_map<Value*, int32_t> spillSlots_;
    std::unordered_map<uint8_t, LiveInterval*> activeRegs_;
    
    std::vector<uint8_t> freeGeneralRegs_;
    std::vector<uint8_t> freeFloatRegs_;
    
    // Linear scan algorithm
    void computeLiveIntervals();
    void linearScan();
    void expireOldIntervals(uint32_t currentPos);
    bool assignRegister(LiveInterval* interval);
    void spill(LiveInterval* interval);
    void spill(LiveInterval* interval, LiveInterval* spillTo);
    
    // Helper functions
    uint32_t getPosition(BasicBlock* bb, size_t instIdx);
    uint32_t getBlockStart(BasicBlock* bb);
    uint32_t getBlockEnd(BasicBlock* bb);
    void addRange(Value* val, uint32_t start, uint32_t end);
    void addUse(Value* val, uint32_t pos);
    
    // Register class helpers
    bool isGeneralReg(uint8_t reg) const { return reg < 16; }
    bool isFloatReg(uint8_t reg) const { return reg >= 16 && reg < 32; }
    
    // Callee-saved management
    std::vector<uint8_t> calleeSavedUsed_;
    void saveCalleeSaved();
    void restoreCalleeSaved();
};

// =============================================================================
// MACHINE CODE GENERATOR
// =============================================================================

struct MachineOperand {
    enum class Kind { Reg, Imm, Mem, Label, Global };
    Kind kind;
    uint8_t reg = Register::Invalid;
    int64_t imm = 0;
    int32_t disp = 0;
    uint8_t scale = 1;
    uint8_t size = 8;
    std::string label;
    Value* vreg = nullptr;
    
    static MachineOperand Reg(uint8_t r, uint8_t sz = 8) {
        return {Kind::Reg, r, 0, 0, 1, sz, "", nullptr};
    }
    static MachineOperand Imm(int64_t val, uint8_t sz = 8) {
        return {Kind::Imm, Register::Invalid, val, 0, 1, sz, "", nullptr};
    }
    static MachineOperand Mem(uint8_t base, int32_t disp = 0, uint8_t sz = 8) {
        return {Kind::Mem, base, 0, disp, 1, sz, "", nullptr};
    }
    static MachineOperand Label(const std::string& name) {
        return {Kind::Label, Register::Invalid, 0, 0, 1, 0, name, nullptr};
    }
};

struct MachineInstr {
    enum class Opcode : uint16_t {
        // Data movement
        MOV, MOVZX, MOVSX, LEA, XCHG,
        // Stack
        PUSH, POP,
        // Arithmetic
        ADD, SUB, MUL, IMUL, DIV, IDIV, INC, DEC, NEG,
        // Bitwise
        AND, OR, XOR, NOT, SHL, SHR, SAR,
        // Compare
        CMP, TEST,
        // Control flow
        JMP, JE, JNE, JZ, JNZ, JL, JLE, JG, JGE, JB, JBE, JA, JAE,
        CALL, RET,
        // SSE/AVX
        MOVSS, MOVSD, MOVAPS, MOVUPS,
        ADDSS, ADDSD, ADDPS, ADDPD,
        SUBSS, SUBSD, SUBPS, SUBPD,
        MULSS, MULSD, MULPS, MULPD,
        DIVSS, DIVSD, DIVPS, DIVPD,
        // AVX
        VMOVAPS, VADDPS, VSUBPS, VMULPS, VDIVPS,
        // Memory
        MOVDQA, MOVDQU, MOVNTI,
        // Atomic
        LOCK_ADD, LOCK_SUB, LOCK_XCHG, CMPXCHG,
        // Pseudo
        LABEL, PROLOGUE, EPILOGUE, SEH_PUSH, SEH_PROLOGUE
    };
    
    Opcode opcode;
    std::vector<MachineOperand> operands;
    std::string comment;
    uint32_t address = 0;
    uint32_t size = 0;
    
    bool isTerminator() const {
        return opcode == Opcode::JMP || opcode == Opcode::RET;
    }
};

struct MachineFunction {
    Function* irFunc = nullptr;
    std::string name;
    std::vector<std::vector<MachineInstr>> blocks;
    uint32_t stackSize = 0;
    uint32_t framePointerOffset = 0;
    std::vector<uint8_t> calleeSavedRegs;
    std::vector<uint32_t> sehPrologues;  // SEH unwind info
};

class MachineCodeGenerator {
public:
    MachineCodeGenerator(RegisterAllocator& ra) : regAlloc_(ra) {}
    
    std::unique_ptr<MachineFunction> generate(Function* func);
    
private:
    RegisterAllocator& regAlloc_;
    MachineFunction* current_ = nullptr;
    std::unordered_map<Value*, uint8_t> vregMapping_;
    std::unordered_map<BasicBlock*, size_t> blockMapping_;
    
    void emitInstruction(MachineInstr::Opcode op, 
                        std::initializer_list<MachineOperand> ops);
    void emitPrologue();
    void emitEpilogue();
    
    void lowerInstruction(Instruction* inst);
    void lowerBinaryOp(Instruction* inst);
    void lowerComparison(Instruction* inst);
    void lowerMemoryOp(Instruction* inst);
    void lowerCall(Instruction* inst);
    void lowerConversion(Instruction* inst);
    
    uint8_t getReg(Value* val);
    uint8_t getRegForResult(Value* val);
    
    // Address mode encoding
    MachineOperand encodeAddress(Value* ptr);
    void emitLoad(uint8_t dst, MachineOperand src, uint8_t size);
    void emitStore(MachineOperand dst, uint8_t src, uint8_t size);
};

// =============================================================================
// OPTIMIZATION PASSES
// =============================================================================

class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    virtual bool run(Function* func) = 0;
    virtual const char* name() const = 0;
};

// Constant Folding
class ConstantFoldingPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "ConstantFolding"; }
    
private:
    Value* foldBinary(Instruction* inst);
    Value* foldComparison(Instruction* inst);
    Value* foldConversion(Instruction* inst);
    
    bool isConstant(Value* val) const;
    int64_t getIntValue(Value* val) const;
    double getFloatValue(Value* val) const;
};

// Dead Code Elimination
class DeadCodeEliminationPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "DeadCodeElimination"; }
    
private:
    bool isDead(Instruction* inst) const;
    bool hasSideEffects(Instruction* inst) const;
};

// Common Subexpression Elimination
class CSEPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "CSE"; }
    
private:
    std::unordered_map<size_t, Instruction*> exprMap_;
    size_t hashInstruction(Instruction* inst) const;
    bool instructionsEqual(Instruction* a, Instruction* b) const;
};

// Control Flow Simplification
class CFGSimplifyPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "CFGSimplify"; }
    
private:
    bool removeDeadBlocks(Function* func);
    bool mergeBlocks(Function* func);
    bool simplifyBranches(Function* func);
    bool removeEmptyBlocks(Function* func);
};

// Register Coalescing
class RegisterCoalescingPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "RegisterCoalescing"; }
};

// Memory to Register Promotion
class Mem2RegPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "Mem2Reg"; }
    
private:
    bool isAllocaPromotable(Instruction* alloca);
    void promoteAlloca(Instruction* alloca, Function* func);
};

// Loop Invariant Code Motion
class LICMPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "LICM"; }
    
private:
    bool isLoopInvariant(Instruction* inst, BasicBlock* header);
    bool dominatesAllUses(Instruction* inst, BasicBlock* header);
};

// Strength Reduction
class StrengthReductionPass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "StrengthReduction"; }
    
private:
    bool reduceMulToShift(Instruction* inst);
    bool reduceDivToShift(Instruction* inst);
};

// Peephole Optimizer
class PeepholePass : public OptimizationPass {
public:
    bool run(Function* func) override;
    const char* name() const override { return "Peephole"; }
    
private:
    bool eliminateRedundantMoves();
    bool foldImmediateOps();
};

// Optimization Pipeline
class OptimizationPipeline {
public:
    void addPass(std::unique_ptr<OptimizationPass> pass);
    bool run(Function* func);
    bool run(Module* module);
    
    void setOptimizationLevel(int level);
    
private:
    std::vector<std::unique_ptr<OptimizationPass>> passes_;
    int optLevel_ = 2;
};

} // namespace CodeGen

// =============================================================================
// EXCEPTION HANDLING - Windows SEH
// =============================================================================

namespace SEH {

#pragma pack(push, 1)

// SEH Unwind Info
struct UnwindInfo {
    uint8_t version : 3;
    uint8_t flags : 5;
    uint8_t prologSize;
    uint8_t frameRegister : 4;
    uint8_t frameOffset : 4;
    uint16_t exceptionHandlerCount;
    uint32_t exceptionHandlerRVA;
    uint32_t exceptionHandlerDataRVA;
};

struct UnwindCode {
    uint8_t codeOffset;
    uint8_t unwindOp : 4;
    uint8_t opInfo : 4;
};

struct ScopeTable {
    uint32_t count;
    uint32_t scopeRecordStart;
};

struct ScopeRecord {
    uint32_t beginAddress;
    uint32_t endAddress;
    uint32_t handlerAddress;
    uint32_t targetAddress;
};

struct TryBlockTable {
    uint32_t count;
    uint32_t tryBlockStart;
};

struct TryBlock {
    uint32_t lowOffset;
    uint32_t highOffset;
    uint32_t catchHandlerCount;
    uint32_t catchHandlerArrayRVA;
};

struct CatchHandler {
    uint32_t typeDescriptorRVA;
    uint32_t handlerRVA;
};

struct TypeDescriptor {
    uint64_t vtable;
    uint64_t name;
    uint32_t hash;
};

#pragma pack(pop)

// Unwind opcodes
enum class UnwindOp : uint8_t {
    PUSH_NONVOL = 0,
    ALLOC_LARGE = 1,
    ALLOC_SMALL = 2,
    SET_FPREG = 3,
    SAVE_NONVOL = 4,
    SAVE_NONVOL_FAR = 5,
    SAVE_XMM128 = 6,
    SAVE_XMM128_FAR = 7,
    EPILOG = 8,
    SPARE = 9,
    SAVE_XMM = 10,
    PUSH_MACHFRAME = 11
};

// Exception handling runtime
struct ExceptionContext {
    void* exceptionRecord;
    void* establisherFrame;
    void* contextRecord;
    void* dispatcherContext;
};

// Personality routine for C++ exceptions
extern "C" int __CxxFrameHandler3(
    void* exceptionRecord,
    void* establisherFrame,
    void* contextRecord,
    void* dispatcherContext
);

// Exception throwing
void throwException(void* object, const TypeDescriptor* type);
void rethrowException();

// Exception catching
bool catchException(const TypeDescriptor* type);
void* getExceptionObject();

// Unwind helpers
void beginTry();
void endTry();
void beginCatch();
void endCatch();

class SEHBuilder {
public:
    struct Handler {
        uint32_t tryStart;
        uint32_t tryEnd;
        uint32_t catchStart;
        uint32_t catchEnd;
        uint32_t typeDescriptorRVA;
    };
    
    std::vector<uint8_t> buildUnwindInfo(const std::vector<Handler>& handlers);
    std::vector<uint8_t> buildScopeTable(const std::vector<Handler>& handlers);
    
    void addUnwindCode(std::vector<uint8_t>& data, uint8_t offset, 
                       UnwindOp op, uint8_t info = 0);
    
private:
    uint32_t prologueSize_ = 0;
    std::vector<UnwindCode> unwindCodes_;
};

} // namespace SEH

// =============================================================================
// RTTI SUPPORT
// =============================================================================

namespace RTTI {

#pragma pack(push, 1)

struct TypeInfo {
    void* vtable;
    const char* name;
};

struct BaseClassDescriptor {
    TypeInfo* typeInfo;
    uint32_t numBases;
    int32_t offset;
    uint32_t attributes;
};

struct ClassHierarchyDescriptor {
    uint32_t signature;
    uint32_t attributes;
    uint32_t numBaseClasses;
    BaseClassDescriptor** baseClassArray;
};

struct CompleteObjectLocator {
    uint32_t signature;
    uint32_t offset;
    uint32_t constructorOffset;
    TypeInfo* typeInfo;
    ClassHierarchyDescriptor* classDescriptor;
};

#pragma pack(pop)

// Type identification
const std::type_info& getTypeInfo(const void* obj);
const char* getTypeName(const void* obj);
bool isOfType(const void* obj, const TypeInfo* type);
bool isDerivedFrom(const void* obj, const TypeInfo* baseType);

// Dynamic cast implementation
void* dynamicCast(void* obj, const TypeInfo* targetType);

// Type hash computation
uint32_t computeTypeHash(const char* name);
uint32_t computeTypeHash(const TypeInfo* type);

// Vtable helpers
void** getVtable(const void* obj);
void setVtable(void* obj, void** vtable);
bool hasVtable(const void* obj);

// Class registration
void registerClass(const TypeInfo* type, const ClassHierarchyDescriptor* hierarchy);
void registerBaseClass(const TypeInfo* derived, const TypeInfo* base, int32_t offset);

// Query functions
bool isPolymorphic(const TypeInfo* type);
size_t getTypeSize(const TypeInfo* type);
size_t getTypeAlign(const TypeInfo* type);

// Runtime type comparison
bool typesEqual(const TypeInfo* a, const TypeInfo* b);
bool typesCompatible(const TypeInfo* a, const TypeInfo* b);

} // namespace RTTI

// =============================================================================
// MINIMAL FREESTANDING RUNTIME
// =============================================================================

namespace Runtime {

// Memory management
void* allocMemory(size_t size, size_t align = 16);
void freeMemory(void* ptr);
void* reallocMemory(void* ptr, size_t newSize);
void* allocZeroed(size_t count, size_t size);

// Memory operations
void copyMemory(void* dst, const void* src, size_t size);
void moveMemory(void* dst, const void* src, size_t size);
void setMemory(void* dst, uint8_t val, size_t size);
int compareMemory(const void* a, const void* b, size_t size);

// String operations
size_t stringLength(const char* str);
char* stringCopy(char* dst, const char* src);
char* stringCat(char* dst, const char* src);
int stringCompare(const char* a, const char* b);
char* stringFind(char* str, char c);
const char* stringFind(const char* str, char c);

// Math operations (software fallback)
double floor(double x);
double ceil(double x);
double round(double x);
double trunc(double x);
double sqrt(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double exp(double x);
double log(double x);
double pow(double base, double exp);
double fabs(double x);

// Integer math
int64_t abs(int64_t x);
int64_t divmod(int64_t dividend, int64_t divisor, int64_t* remainder);
uint64_t rotl(uint64_t x, int k);
uint64_t rotr(uint64_t x, int k);
uint64_t clz(uint64_t x);
uint64_t ctz(uint64_t x);
uint64_t popcount(uint64_t x);
uint64_t byteswap(uint64_t x);

// Process control
void exitProcess(int code);
void abortProcess();
bool isDebuggerPresent();
void debugBreak();

// System calls (Windows)
void* getStdHandle(uint32_t handle);
bool writeConsole(void* handle, const void* buf, uint32_t len, uint32_t* written);
bool readConsole(void* handle, void* buf, uint32_t len, uint32_t* read);
uint64_t getTickCount();
void sleepMs(uint32_t ms);

// File I/O (minimal)
struct FileHandle { void* handle; };
FileHandle openFile(const char* path, bool write);
bool readFile(FileHandle file, void* buf, size_t size, size_t* read);
bool writeFile(FileHandle file, const void* buf, size_t size, size_t* written);
void closeFile(FileHandle file);
bool fileExists(const char* path);
int64_t getFileSize(FileHandle file);

// Startup/Shutdown
using InitFunc = void(*)();
void registerInit(InitFunc func);
void registerFini(InitFunc func);
void runInitFuncs();
void runFiniFuncs();

// Stack checking
void stackProbe(size_t size);
bool stackAvailable(size_t size);

// Security
void securityCookie();
uint64_t getSecurityCookie();
bool checkSecurityCookie(uint64_t cookie);

// Thread local storage
uint32_t allocTlsSlot();
void freeTlsSlot(uint32_t slot);
void* getTlsValue(uint32_t slot);
void setTlsValue(uint32_t slot, void* value);

// Atomics
void* atomicLoad(void** ptr);
void atomicStore(void** ptr, void* val);
bool atomicCompareExchange(void** ptr, void* expected, void* desired);
void* atomicExchange(void** ptr, void* val);
void atomicFence();

// Integer to string conversions
int intToStr(int64_t val, char* buf, int bufSize, int radix = 10);
int uintToStr(uint64_t val, char* buf, int bufSize, int radix = 10);
int floatToStr(double val, char* buf, int bufSize, int precision = 6);

// String to number conversions
int64_t strToInt(const char* str, char** end, int base = 10);
double strToFloat(const char* str, char** end);

// Console I/O helpers
void printStr(const char* str);
void printInt(int64_t val);
void printUInt(uint64_t val);
void printHex(uint64_t val);
void printFloat(double val);
void printLine(const char* str);
char readChar();
int readLine(char* buf, int maxLen);

} // namespace Runtime

} // namespace Backend
} // namespace RawrXD
