// static_analysis_engine.hpp — Phase 15: Static Analysis Engine (CFG/SSA)
// Control Flow Graph construction, SSA form transformation, dominator tree
// computation, constant propagation, dead code elimination, and liveness analysis.
//
// Architecture: Works on an IR (Intermediate Representation) of disassembled
//               or parsed code. Produces annotated CFG + SSA suitable for
//               the reverse engineering pipeline and code intelligence.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#pragma once

#ifndef RAWRXD_STATIC_ANALYSIS_ENGINE_HPP
#define RAWRXD_STATIC_ANALYSIS_ENGINE_HPP

#include "model_memory_hotpatch.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <functional>

// ============================================================================
// IR Opcode — Abstract instruction types
// ============================================================================
enum class IROpcode : uint16_t {
    Nop = 0,
    // Arithmetic
    Add, Sub, Mul, Div, Mod, Neg,
    // Bitwise
    And, Or, Xor, Not, Shl, Shr, Sar,
    // Memory
    Load, Store, Lea, MemCopy, MemSet,
    // Control Flow
    Jump, JumpIf, JumpIfNot, Call, Return, IndirectJump, IndirectCall,
    // Comparison
    Cmp, Test, SetFlag,
    // Data Movement
    Move, Phi, Select, ZeroExtend, SignExtend, Truncate,
    // Stack
    Push, Pop,
    // Special
    Syscall, Breakpoint, Undefined
};

// ============================================================================
// IR Operand — Register, immediate, memory reference
// ============================================================================
enum class OperandType : uint8_t {
    None = 0,
    Register,
    Immediate,
    Memory,
    Label,
    Phi          // SSA phi-function reference
};

struct IROperand {
    OperandType     type;
    uint32_t        regId;          // Register ID (if Register)
    int64_t         immValue;       // Immediate value
    uint32_t        memBase;        // Base register for memory access
    int32_t         memDisp;        // Memory displacement
    uint32_t        ssaVersion;     // SSA version number (0 = not in SSA form)
    std::string     label;          // Label name (if Label)

    IROperand()
        : type(OperandType::None), regId(0), immValue(0)
        , memBase(0), memDisp(0), ssaVersion(0) {}

    static IROperand reg(uint32_t id, uint32_t ssa = 0) {
        IROperand op;
        op.type = OperandType::Register;
        op.regId = id;
        op.ssaVersion = ssa;
        return op;
    }

    static IROperand imm(int64_t val) {
        IROperand op;
        op.type = OperandType::Immediate;
        op.immValue = val;
        return op;
    }

    static IROperand mem(uint32_t base, int32_t disp) {
        IROperand op;
        op.type = OperandType::Memory;
        op.memBase = base;
        op.memDisp = disp;
        return op;
    }

    static IROperand lbl(const std::string& name) {
        IROperand op;
        op.type = OperandType::Label;
        op.label = name;
        return op;
    }
};

// ============================================================================
// IR Instruction
// ============================================================================
struct IRInstruction {
    uint64_t            id;              // Unique instruction ID
    uint64_t            address;         // Original binary address
    IROpcode            opcode;
    IROperand           dest;
    IROperand           src1;
    IROperand           src2;
    std::string         comment;         // Analysis annotation
    bool                isDead;          // Dead code elimination flag
    uint32_t            blockId;         // Owning basic block

    // Phi-function sources (only for opcode == Phi)
    struct PhiSource {
        uint32_t fromBlock;
        IROperand value;
    };
    std::vector<PhiSource> phiSources;

    IRInstruction()
        : id(0), address(0), opcode(IROpcode::Nop), isDead(false), blockId(0) {}
};

// ============================================================================
// Basic Block — A sequence of instructions with single entry, single exit
// ============================================================================
struct BasicBlock {
    uint32_t            id;
    std::string         label;
    uint64_t            startAddr;
    uint64_t            endAddr;
    std::vector<uint64_t> instructionIds;     // Ordered instruction IDs

    // CFG edges
    std::vector<uint32_t> predecessors;
    std::vector<uint32_t> successors;

    // Dominator tree
    int32_t             immDominator;          // Immediate dominator block ID (-1 = entry)
    std::vector<uint32_t> dominated;           // Blocks this dominates
    int32_t             domFrontierOrder;       // DFS order in dominator tree

    // Liveness
    std::unordered_set<uint32_t> liveIn;       // Variables live at entry
    std::unordered_set<uint32_t> liveOut;      // Variables live at exit
    std::unordered_set<uint32_t> defSet;       // Variables defined
    std::unordered_set<uint32_t> useSet;       // Variables used before defined

    // Analysis flags
    bool                isEntryBlock;
    bool                isExitBlock;
    bool                isLoop;
    uint32_t            loopDepth;

    BasicBlock()
        : id(0), startAddr(0), endAddr(0), immDominator(-1)
        , domFrontierOrder(-1), isEntryBlock(false)
        , isExitBlock(false), isLoop(false), loopDepth(0) {}
};

// ============================================================================
// Control Flow Graph
// ============================================================================
struct ControlFlowGraph {
    uint32_t                                    functionId;
    std::string                                 functionName;
    uint64_t                                    entryAddress;
    uint32_t                                    entryBlockId;
    std::unordered_map<uint32_t, BasicBlock>    blocks;
    std::unordered_map<uint64_t, IRInstruction> instructions;
    bool                                        inSSAForm;
    uint32_t                                    totalBlocks;
    uint64_t                                    totalInstructions;

    ControlFlowGraph()
        : functionId(0), entryAddress(0), entryBlockId(0)
        , inSSAForm(false), totalBlocks(0), totalInstructions(0) {}
};

// ============================================================================
// Analysis Pass Result
// ============================================================================
struct AnalysisPassResult {
    bool        success;
    const char* passName;
    const char* detail;
    int64_t     durationUs;
    uint32_t    itemsProcessed;

    static AnalysisPassResult ok(const char* pass, uint32_t items = 0, int64_t us = 0) {
        return { true, pass, "Completed successfully", us, items };
    }

    static AnalysisPassResult error(const char* pass, const char* msg) {
        return { false, pass, msg, 0, 0 };
    }
};

// ============================================================================
// Dominance Frontier
// ============================================================================
struct DominanceFrontier {
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> frontiers;
};

// ============================================================================
// Loop Info
// ============================================================================
struct LoopInfo {
    uint32_t                headerId;           // Loop header block
    std::unordered_set<uint32_t> bodyBlocks;    // All blocks in the loop
    std::vector<uint32_t>   backEdges;          // Back edge sources
    uint32_t                depth;              // Nesting depth
    uint32_t                tripCountEstimate;  // Estimated iteration count (0 = unknown)
};

// ============================================================================
// Constant Propagation Result
// ============================================================================
struct ConstantValue {
    uint32_t    regId;
    uint32_t    ssaVersion;
    int64_t     value;
    bool        isConstant;
};

// ============================================================================
// Analysis Statistics
// ============================================================================
struct AnalysisStats {
    std::atomic<uint64_t> functionsAnalyzed{0};
    std::atomic<uint64_t> blocksBuilt{0};
    std::atomic<uint64_t> instructionsParsed{0};
    std::atomic<uint64_t> phisInserted{0};
    std::atomic<uint64_t> deadCodeEliminated{0};
    std::atomic<uint64_t> constantsPropagated{0};
    std::atomic<uint64_t> loopsDetected{0};
    std::atomic<uint64_t> totalAnalysisTimeUs{0};
};

// ============================================================================
// Callbacks
// ============================================================================
using AnalysisProgressCallback = void(*)(const char* passName, uint32_t pct, void* userData);
using AnalysisCompleteCallback = void(*)(uint32_t functionId, bool success, void* userData);

// ============================================================================
// StaticAnalysisEngine — Main class
// ============================================================================
class StaticAnalysisEngine {
public:
    static StaticAnalysisEngine& instance();

    // ── CFG Construction ──
    AnalysisPassResult buildCFG(const std::vector<IRInstruction>& instructions,
                                 const std::string& functionName,
                                 uint64_t entryAddr,
                                 uint32_t* outFunctionId);

    AnalysisPassResult addEdge(uint32_t functionId, uint32_t fromBlock, uint32_t toBlock);
    AnalysisPassResult removeUnreachableBlocks(uint32_t functionId);

    // ── Dominator Analysis ──
    AnalysisPassResult computeDominators(uint32_t functionId);
    AnalysisPassResult computeDominanceFrontier(uint32_t functionId);
    AnalysisPassResult computePostDominators(uint32_t functionId);

    // ── SSA Transformation ──
    AnalysisPassResult convertToSSA(uint32_t functionId);
    AnalysisPassResult insertPhiFunctions(uint32_t functionId);
    AnalysisPassResult renameVariables(uint32_t functionId);
    AnalysisPassResult convertFromSSA(uint32_t functionId);

    // ── Optimization Passes ──
    AnalysisPassResult constantPropagation(uint32_t functionId);
    AnalysisPassResult deadCodeElimination(uint32_t functionId);
    AnalysisPassResult copyPropagation(uint32_t functionId);
    AnalysisPassResult commonSubexpressionElimination(uint32_t functionId);

    // ── Liveness Analysis ──
    AnalysisPassResult computeLiveness(uint32_t functionId);
    std::unordered_set<uint32_t> getLiveAt(uint32_t functionId, uint32_t blockId) const;

    // ── Loop Analysis ──
    AnalysisPassResult detectLoops(uint32_t functionId);
    std::vector<LoopInfo> getLoops(uint32_t functionId) const;
    uint32_t getLoopDepth(uint32_t functionId, uint32_t blockId) const;

    // ── Query ──
    const ControlFlowGraph* getCFG(uint32_t functionId) const;
    const BasicBlock* getBlock(uint32_t functionId, uint32_t blockId) const;
    const IRInstruction* getInstruction(uint32_t functionId, uint64_t instrId) const;
    std::vector<uint32_t> getAllFunctions() const;
    size_t functionCount() const;

    // ── Export ──
    PatchResult exportDot(uint32_t functionId, const char* filePath) const;
    PatchResult exportJSON(uint32_t functionId, const char* filePath) const;

    // ── Statistics ──
    const AnalysisStats& getStats() const { return m_stats; }
    void resetStats();

    // ── Callbacks ──
    void setProgressCallback(AnalysisProgressCallback cb, void* userData);
    void setCompleteCallback(AnalysisCompleteCallback cb, void* userData);

    // ── Bulk Analysis ──
    PatchResult runFullAnalysis(uint32_t functionId);

private:
    StaticAnalysisEngine();
    ~StaticAnalysisEngine();
    StaticAnalysisEngine(const StaticAnalysisEngine&) = delete;
    StaticAnalysisEngine& operator=(const StaticAnalysisEngine&) = delete;

    // Internal helpers
    uint32_t createBlock(ControlFlowGraph& cfg);
    void splitBlockAt(ControlFlowGraph& cfg, uint32_t blockId, uint64_t instrId);
    bool isControlFlow(IROpcode op) const;
    bool isTerminator(IROpcode op) const;

    // Dominator computation (Lengauer-Tarjan)
    void computeIDom(ControlFlowGraph& cfg);
    uint32_t intersect(const ControlFlowGraph& cfg,
                       const std::unordered_map<uint32_t, uint32_t>& idom,
                       uint32_t b1, uint32_t b2) const;

    // SSA helpers
    void placePhiFunctions(ControlFlowGraph& cfg, const DominanceFrontier& df);
    void renameBlock(ControlFlowGraph& cfg, uint32_t blockId,
                     std::unordered_map<uint32_t, std::vector<uint32_t>>& stacks,
                     std::unordered_map<uint32_t, uint32_t>& counters);

    // State
    mutable std::mutex                                  m_mutex;
    std::atomic<uint32_t>                               m_nextFunctionId{1};
    std::atomic<uint32_t>                               m_nextBlockId{1};
    std::atomic<uint64_t>                               m_nextInstrId{1};

    // Function registry
    std::unordered_map<uint32_t, ControlFlowGraph>      m_functions;

    // Dominance frontiers (per function)
    std::unordered_map<uint32_t, DominanceFrontier>     m_domFrontiers;

    // Loop info (per function)
    std::unordered_map<uint32_t, std::vector<LoopInfo>> m_loops;

    // Constant propagation results (per function)
    std::unordered_map<uint32_t, std::vector<ConstantValue>> m_constants;

    // Callbacks
    AnalysisProgressCallback m_progressCb     = nullptr;
    void*                    m_progressData   = nullptr;
    AnalysisCompleteCallback m_completeCb     = nullptr;
    void*                    m_completeData   = nullptr;

    // Statistics
    AnalysisStats            m_stats;
};

#endif // RAWRXD_STATIC_ANALYSIS_ENGINE_HPP
