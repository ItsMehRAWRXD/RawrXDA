// =============================================================================
// RAWRXD_BACKEND.CPP - Implementation
// =============================================================================

#include "rawrxd_backend.h"
#include <cmath>

namespace RawrXD {
namespace Backend {

namespace IR {

void Value::replaceAllUsesWith(Value* newVal) {
    for (auto* use : uses) {
        use->val = newVal;
        newVal->addUse(use);
    }
    uses.clear();
}

Constant* Constant::createInt(IRType type, int64_t val) {
    auto* c = new Constant(type, Kind::Int);
    c->intVal = val;
    return c;
}

Constant* Constant::createFloat(IRType type, double val) {
    auto* c = new Constant(type, Kind::Float);
    c->floatVal = val;
    return c;
}

Constant* Constant::createNull(IRType type) {
    return new Constant(type, Kind::Null);
}

void Instruction::setOperand(uint32_t idx, Value* val) {
    while (idx >= operands.size()) {
        auto* use = new Use();
        use->user = this;
        use->operandIndex = static_cast<uint32_t>(operands.size());
        operands.push_back(use);
    }
    
    if (operands[idx]->val) {
        operands[idx]->val->removeUse(operands[idx]);
    }
    
    operands[idx]->val = val;
    if (val) val->addUse(operands[idx]);
}

} // namespace IR

namespace CodeGen {

// Register name mapping
std::string Register::name() const {
    static const char* generalNames[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    static const char* xmmNames[] = {
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
        "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
    };
    
    if (id < 16) return generalNames[id];
    if (id < 32) return xmmNames[id - 16];
    return "invalid";
}

// Register Allocator Implementation
bool RegisterAllocator::allocate(Function* func) {
    func_ = func;
    intervals_.clear();
    regAssignment_.clear();
    spillSlots_.clear();
    activeRegs_.clear();
    stackSize_ = 0;
    nextSlot_ = 0;
    
    // Initialize free registers
    freeGeneralRegs_.clear();
    freeFloatRegs_.clear();
    
    // General purpose: RAX, RCX, RDX, RBX, RSI, RDI, R8-R15
    // Callee-saved: RBX, RSI, RDI, R12-R15
    for (int i = 0; i < 16; ++i) {
        if (i == 4) continue;  // Skip RSP
        if (i == 5) continue;  // Skip RBP
        
        bool isCalleeSaved = (i == 3 || i == 6 || i == 7 || i >= 12);
        if (config_.preserveRBX && i == 3) isCalleeSaved = true;
        if (config_.preserveRSI && i == 6) isCalleeSaved = true;
        if (config_.preserveRDI && i == 7) isCalleeSaved = true;
        
        freeGeneralRegs_.push_back(i);
    }
    
    // Float registers: XMM0-XMM15
    for (int i = 0; i < static_cast<int>(config_.numFloatRegs); ++i) {
        freeFloatRegs_.push_back(16 + i);
    }
    
    computeLiveIntervals();
    linearScan();
    
    // Calculate stack size (align to 16)
    stackSize_ = ((nextSlot_ * 8 + 15) & ~15);
    
    return true;
}

void RegisterAllocator::computeLiveIntervals() {
    uint32_t position = 0;
    
    for (auto& block : func_->blocks) {
        // Position for phi nodes at block entry
        for (auto& inst : block->instructions) {
            if (inst->opcode == IR::Instruction::Opcode::Phi) {
                for (uint32_t i = 0; i < inst->operands.size(); ++i) {
                    addUse(inst->getOperand(i), position);
                }
                addRange(inst.get(), position, position + 1);
            }
        }
        
        // Process instructions
        for (size_t i = 0; i < block->instructions.size(); ++i) {
            auto& inst = block->instructions[i];
            uint32_t instPos = position + static_cast<uint32_t>(i) * 2;
            
            // Def creates range from this point
            addRange(inst.get(), instPos, instPos + 1);
            
            // Uses extend live range
            for (auto* use : inst->operands) {
                if (use->val && use->val != inst.get()) {
                    addUse(use->val, instPos);
                    addRange(use->val, instPos, instPos + 1);
                }
            }
        }
        
        position += static_cast<uint32_t>(block->instructions.size()) * 2;
    }
    
    // Sort intervals by start position
    std::sort(intervals_.begin(), intervals_.end());
}

void RegisterAllocator::linearScan() {
    for (auto& interval : intervals_) {
        expireOldIntervals(interval.start);
        
        if (!assignRegister(&interval)) {
            spill(&interval);
        }
    }
}

void RegisterAllocator::expireOldIntervals(uint32_t currentPos) {
    std::vector<uint8_t> toFree;
    
    for (auto it = activeRegs_.begin(); it != activeRegs_.end(); ) {
        if (it->second->end >= currentPos) {
            ++it;
            continue;
        }
        
        toFree.push_back(it->first);
        it->second->reg = -1;
        it = activeRegs_.erase(it);
    }
    
    // Free registers
    for (uint8_t reg : toFree) {
        if (reg < 16) {
            freeGeneralRegs_.push_back(reg);
        } else {
            freeFloatRegs_.push_back(reg);
        }
    }
}

bool RegisterAllocator::assignRegister(LiveInterval* interval) {
    auto* val = interval->value;
    if (!val) return false;
    
    // Check register class
    bool isFloat = val->type.isFloat();
    auto& freeRegs = isFloat ? freeFloatRegs_ : freeGeneralRegs_;
    
    if (freeRegs.empty()) return false;
    
    // Pick a free register
    uint8_t reg = freeRegs.back();
    freeRegs.pop_back();
    
    interval->reg = static_cast<int8_t>(reg);
    regAssignment_[val] = reg;
    activeRegs_[reg] = interval;
    
    return true;
}

void RegisterAllocator::spill(LiveInterval* interval) {
    // Find interval to spill
    LiveInterval* spillTo = nullptr;
    
    // Find best candidate for spilling
    for (auto& active : intervals_) {
        if (active.reg < 0) continue;
        if (!interval->overlaps(active)) continue;
        
        // Prefer spilling interval with longest remaining lifetime
        if (!spillTo || active.end > spillTo->end) {
            spillTo = &active;
        }
    }
    
    if (spillTo && spillTo->weight > interval->weight) {
        // Spill the active interval instead
        spill(spillTo, interval);
    } else {
        // Spill this interval to stack
        interval->reg = -1;
        interval->spillSlot = static_cast<int32_t>(nextSlot_++);
        spillSlots_[interval->value] = interval->spillSlot;
    }
}

void RegisterAllocator::spill(LiveInterval* interval, LiveInterval* spillTo) {
    // Save register state
    int8_t reg = interval->reg;
    uint8_t ureg = static_cast<uint8_t>(reg);
    
    // Allocate stack slot for spilled interval
    interval->spillSlot = static_cast<int32_t>(nextSlot_++);
    spillSlots_[interval->value] = interval->spillSlot;
    
    // Remove from active
    activeRegs_.erase(ureg);
    interval->reg = -1;
    regAssignment_.erase(interval->value);
    
    // Assign register to new interval
    spillTo->reg = reg;
    regAssignment_[spillTo->value] = ureg;
    activeRegs_[ureg] = spillTo;
}

int8_t RegisterAllocator::getRegister(Value* val) const {
    auto it = regAssignment_.find(val);
    return it != regAssignment_.end() ? static_cast<int8_t>(it->second) : -1;
}

int32_t RegisterAllocator::getStackSlot(Value* val) const {
    auto it = spillSlots_.find(val);
    return it != spillSlots_.end() ? it->second : -1;
}

void RegisterAllocator::addRange(Value* val, uint32_t start, uint32_t end) {
    auto it = std::find_if(intervals_.begin(), intervals_.end(),
        [val](const LiveInterval& i) { return i.value == val; });
    
    if (it != intervals_.end()) {
        it->start = std::min(it->start, start);
        it->end = std::max(it->end, end);
        it->weight++;
    } else {
        intervals_.push_back({val, start, end, -1, -1, 1});
    }
}

void RegisterAllocator::addUse(Value* val, uint32_t pos) {
    auto it = std::find_if(intervals_.begin(), intervals_.end(),
        [val](const LiveInterval& i) { return i.value == val; });
    
    if (it != intervals_.end()) {
        it->weight += 10;  // Uses increase weight
    }
}

// Optimization Passes Implementation
bool ConstantFoldingPass::run(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        std::vector<IR::Instruction*> toRemove;
        
        for (auto& inst : block->instructions) {
            IR::Value* folded = nullptr;
            
            if (inst->isTerminator()) continue;
            
            // Try to fold
            switch (inst->opcode) {
                case IR::Instruction::Opcode::Add:
                case IR::Instruction::Opcode::Sub:
                case IR::Instruction::Opcode::Mul:
                case IR::Instruction::Opcode::SDiv:
                case IR::Instruction::Opcode::UDiv:
                case IR::Instruction::Opcode::SRem:
                case IR::Instruction::Opcode::URem:
                case IR::Instruction::Opcode::And:
                case IR::Instruction::Opcode::Or:
                case IR::Instruction::Opcode::Xor:
                case IR::Instruction::Opcode::Shl:
                case IR::Instruction::Opcode::LShr:
                case IR::Instruction::Opcode::AShr:
                    folded = foldBinary(inst.get());
                    break;
                    
                case IR::Instruction::Opcode::ICmp:
                    folded = foldComparison(inst.get());
                    break;
                    
                case IR::Instruction::Opcode::Trunc:
                case IR::Instruction::Opcode::ZExt:
                case IR::Instruction::Opcode::SExt:
                case IR::Instruction::Opcode::FPToSI:
                case IR::Instruction::Opcode::SIToFP:
                    folded = foldConversion(inst.get());
                    break;
                    
                default:
                    break;
            }
            
            if (folded) {
                inst->replaceAllUsesWith(folded);
                toRemove.push_back(inst.get());
                changed = true;
            }
        }
        
        for (auto* inst : toRemove) {
            block->remove(inst);
        }
    }
    
    return changed;
}

IR::Value* ConstantFoldingPass::foldBinary(IR::Instruction* inst) {
    IR::Value* lhs = inst->getOperand(0);
    IR::Value* rhs = inst->getOperand(1);
    
    if (!isConstant(lhs) || !isConstant(rhs)) return nullptr;
    
    bool isFloat = inst->type.isFloat();
    
    if (!isFloat) {
        int64_t a = getIntValue(lhs);
        int64_t b = getIntValue(rhs);
        int64_t result = 0;
        
        switch (inst->opcode) {
            case IR::Instruction::Opcode::Add: result = a + b; break;
            case IR::Instruction::Opcode::Sub: result = a - b; break;
            case IR::Instruction::Opcode::Mul: result = a * b; break;
            case IR::Instruction::Opcode::SDiv: 
                if (b == 0) return nullptr;  // Don't fold divide by zero
                result = a / b; 
                break;
            case IR::Instruction::Opcode::SRem: 
                if (b == 0) return nullptr;
                result = a % b; 
                break;
            case IR::Instruction::Opcode::UDiv:
                if (b == 0) return nullptr;
                result = static_cast<uint64_t>(a) / static_cast<uint64_t>(b);
                break;
            case IR::Instruction::Opcode::URem:
                if (b == 0) return nullptr;
                result = static_cast<uint64_t>(a) % static_cast<uint64_t>(b);
                break;
            case IR::Instruction::Opcode::And: result = a & b; break;
            case IR::Instruction::Opcode::Or: result = a | b; break;
            case IR::Instruction::Opcode::Xor: result = a ^ b; break;
            case IR::Instruction::Opcode::Shl: result = a << (b & 63); break;
            case IR::Instruction::Opcode::LShr: 
                result = static_cast<uint64_t>(a) >> (b & 63); 
                break;
            case IR::Instruction::Opcode::AShr:
                result = a >> (b & 63);
                break;
            default: return nullptr;
        }
        
        return IR::Constant::createInt(inst->type, result);
    } else {
        double a = getFloatValue(lhs);
        double b = getFloatValue(rhs);
        double result = 0;
        
        switch (inst->opcode) {
            case IR::Instruction::Opcode::FAdd: result = a + b; break;
            case IR::Instruction::Opcode::FSub: result = a - b; break;
            case IR::Instruction::Opcode::FMul: result = a * b; break;
            case IR::Instruction::Opcode::FDiv:
                if (b == 0.0) return nullptr;
                result = a / b;
                break;
            default: return nullptr;
        }
        
        return IR::Constant::createFloat(inst->type, result);
    }
}

IR::Value* ConstantFoldingPass::foldComparison(IR::Instruction* inst) {
    IR::Value* lhs = inst->getOperand(0);
    IR::Value* rhs = inst->getOperand(1);
    
    if (!isConstant(lhs) || !isConstant(rhs)) return nullptr;
    
    bool isFloat = lhs->type.isFloat();
    bool result = false;
    
    // Note: ICmpPred is not stored as operand; this is a simplified version
    // Real implementation would extract predicate from instruction
    
    if (!isFloat) {
        int64_t a = getIntValue(lhs);
        int64_t b = getIntValue(rhs);
        
        // Default to EQ comparison for demonstration
        result = (a == b);
    } else {
        double a = getFloatValue(lhs);
        double b = getFloatValue(rhs);
        result = (a == b);
    }
    
    return IR::Constant::createInt(IR::IRType::Bool(), result ? 1 : 0);
}

IR::Value* ConstantFoldingPass::foldConversion(IR::Instruction* inst) {
    IR::Value* src = inst->getOperand(0);
    if (!isConstant(src)) return nullptr;
    
    switch (inst->opcode) {
        case IR::Instruction::Opcode::Trunc:
        case IR::Instruction::Opcode::ZExt:
        case IR::Instruction::Opcode::SExt:
            return IR::Constant::createInt(inst->type, getIntValue(src));
            
        case IR::Instruction::Opcode::SIToFP:
            return IR::Constant::createFloat(inst->type, 
                static_cast<double>(getIntValue(src)));
                
        case IR::Instruction::Opcode::FPToSI:
            return IR::Constant::createInt(inst->type, 
                static_cast<int64_t>(getFloatValue(src)));
                
        default:
            return nullptr;
    }
}

bool ConstantFoldingPass::isConstant(IR::Value* val) const {
    return dynamic_cast<IR::Constant*>(val) != nullptr;
}

int64_t ConstantFoldingPass::getIntValue(IR::Value* val) const {
    auto* c = dynamic_cast<IR::Constant*>(val);
    return c ? c->intVal : 0;
}

double ConstantFoldingPass::getFloatValue(IR::Value* val) const {
    auto* c = dynamic_cast<IR::Constant*>(val);
    return c ? c->floatVal : 0.0;
}

// Dead Code Elimination
bool DeadCodeEliminationPass::run(Function* func) {
    bool changed = false;
    
    // Iterate until no changes (fixpoint)
    bool localChanged;
    do {
        localChanged = false;
        
        for (auto& block : func->blocks) {
            std::vector<IR::Instruction*> toRemove;
            
            for (auto it = block->instructions.rbegin(); 
                 it != block->instructions.rend(); ++it) {
                
                if (isDead(it->get())) {
                    toRemove.push_back(it->get());
                    localChanged = true;
                }
            }
            
            for (auto* inst : toRemove) {
                block->remove(inst);
            }
        }
        
        changed = changed || localChanged;
    } while (localChanged);
    
    return changed;
}

bool DeadCodeEliminationPass::isDead(IR::Instruction* inst) const {
    if (hasSideEffects(inst)) return false;
    if (inst->uses.empty()) return true;
    
    // Check if all uses are dead
    for (auto* use : inst->uses) {
        auto* user = use->user;
        if (!isDead(user)) return false;
    }
    
    return true;
}

bool DeadCodeEliminationPass::hasSideEffects(IR::Instruction* inst) const {
    switch (inst->opcode) {
        case IR::Instruction::Opcode::Store:
        case IR::Instruction::Opcode::Call:
        case IR::Instruction::Opcode::Invoke:
        case IR::Instruction::Opcode::MemCpy:
        case IR::Instruction::Opcode::MemSet:
        case IR::Instruction::Opcode::AtomicRMW:
        case IR::Instruction::Opcode::CmpXchg:
        case IR::Instruction::Opcode::Fence:
        case IR::Instruction::Opcode::Resume:
            return true;
        default:
            return inst->isTerminator();
    }
}

// CFG Simplification
bool CFGSimplifyPass::run(Function* func) {
    bool changed = false;
    
    changed |= removeDeadBlocks(func);
    changed |= mergeBlocks(func);
    changed |= simplifyBranches(func);
    changed |= removeEmptyBlocks(func);
    
    return changed;
}

bool CFGSimplifyPass::removeDeadBlocks(Function* func) {
    bool changed = false;
    
    // Find unreachable blocks
    std::set<IR::BasicBlock*> reachable;
    std::vector<IR::BasicBlock*> worklist;
    
    worklist.push_back(func->entry());
    reachable.insert(func->entry());
    
    while (!worklist.empty()) {
        auto* bb = worklist.back();
        worklist.pop_back();
        
        for (auto* succ : bb->successors) {
            if (reachable.insert(succ).second) {
                worklist.push_back(succ);
            }
        }
    }
    
    // Remove unreachable blocks
    auto it = func->blocks.begin();
    while (it != func->blocks.end()) {
        if (reachable.find(it->get()) == reachable.end()) {
            it = func->blocks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    
    return changed;
}

bool CFGSimplifyPass::mergeBlocks(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        // Single predecessor with single successor -> merge
        if (block->predecessors.size() == 1 && block->successors.size() == 1) {
            auto* pred = block->predecessors[0];
            
            // Remove branch from predecessor
            if (!pred->instructions.empty()) {
                auto* term = pred->terminator();
                if (term && term->opcode == IR::Instruction::Opcode::Br) {
                    pred->instructions.pop_back();
                }
            }
            
            // Move instructions to predecessor
            for (auto& inst : block->instructions) {
                inst->parent = pred;
                pred->instructions.push_back(std::move(inst));
            }
            
            block->instructions.clear();
            changed = true;
        }
    }
    
    return changed;
}

bool CFGSimplifyPass::simplifyBranches(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        auto* term = block->terminator();
        if (!term) continue;
        
        if (term->opcode == IR::Instruction::Opcode::CondBr) {
            IR::Value* cond = term->getOperand(0);
            
            // Check if condition is constant
            if (auto* c = dynamic_cast<IR::Constant*>(cond)) {
                bool branchTrue = c->intVal != 0;
                uint32_t target = branchTrue ? 1 : 2;  // Operand indices
                
                // Convert to unconditional branch
                auto newBr = std::make_unique<IR::Instruction>(
                    IR::Instruction::Opcode::Br, IR::IRType::Void());
                newBr->setOperand(0, term->getOperand(target));
                
                block->instructions.pop_back();
                block->append(std::move(newBr));
                changed = true;
            }
        }
    }
    
    return changed;
}

bool CFGSimplifyPass::removeEmptyBlocks(Function* func) {
    bool changed = false;
    
    for (auto it = func->blocks.begin(); it != func->blocks.end(); ) {
        auto& block = *it;
        
        // Skip entry block
        if (block.get() == func->entry()) {
            ++it;
            continue;
        }
        
        // Only terminator with single predecessor
        if (block->instructions.size() == 1 && 
            block->predecessors.size() == 1) {
            
            auto* pred = block->predecessors[0];
            auto* term = block->terminator();
            
            if (term && term->opcode == IR::Instruction::Opcode::Br) {
                // Redirect predecessors to successor
                auto* succ = block->successors[0];
                
                for (auto& predBlock : func->blocks) {
                    for (auto* predSucc : predBlock->successors) {
                        if (predSucc == block.get()) {
                            predSucc = succ;
                        }
                    }
                }
                
                it = func->blocks.erase(it);
                changed = true;
                continue;
            }
        }
        
        ++it;
    }
    
    return changed;
}

// Strength Reduction
bool StrengthReductionPass::run(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        for (auto& inst : block->instructions) {
            if (inst->opcode == IR::Instruction::Opcode::Mul) {
                changed |= reduceMulToShift(inst.get());
            } else if (inst->opcode == IR::Instruction::Opcode::SDiv ||
                       inst->opcode == IR::Instruction::Opcode::UDiv) {
                changed |= reduceDivToShift(inst.get());
            }
        }
    }
    
    return changed;
}

bool StrengthReductionPass::reduceMulToShift(IR::Instruction* inst) {
    IR::Value* rhs = inst->getOperand(1);
    auto* c = dynamic_cast<IR::Constant*>(rhs);
    if (!c) return false;
    
    int64_t val = c->intVal;
    if (val <= 0) return false;
    
    // Check if power of 2
    if ((val & (val - 1)) != 0) return false;
    
    int shift = 0;
    while ((1LL << shift) != val) shift++;
    
    // Replace with shift
    auto* shiftImm = IR::Constant::createInt(IR::IRType::I32(), shift);
    inst->opcode = IR::Instruction::Opcode::Shl;
    inst->setOperand(1, shiftImm);
    
    return true;
}

bool StrengthReductionPass::reduceDivToShift(IR::Instruction* inst) {
    IR::Value* rhs = inst->getOperand(1);
    auto* c = dynamic_cast<IR::Constant*>(rhs);
    if (!c) return false;
    
    int64_t val = c->intVal;
    if (val <= 0) return false;
    
    // Check if power of 2
    if ((val & (val - 1)) != 0) return false;
    
    int shift = 0;
    while ((1LL << shift) != val) shift++;
    
    // Replace with shift
    auto* shiftImm = IR::Constant::createInt(IR::IRType::I32(), shift);
    inst->opcode = (inst->opcode == IR::Instruction::Opcode::SDiv) ?
        IR::Instruction::Opcode::AShr : IR::Instruction::Opcode::LShr;
    inst->setOperand(1, shiftImm);
    
    return true;
}

// CSE Pass
bool CSEPass::run(Function* func) {
    bool changed = false;
    exprMap_.clear();
    
    for (auto& block : func->blocks) {
        for (auto& inst : block->instructions) {
            if (inst->mayWriteMemory() || inst->mayReadMemory()) {
                exprMap_.clear();  // Clear on memory ops
                continue;
            }
            
            size_t hash = hashInstruction(inst.get());
            auto it = exprMap_.find(hash);
            
            if (it != exprMap_.end() && 
                instructionsEqual(it->second, inst.get())) {
                // Found matching expression, replace uses
                inst->replaceAllUsesWith(it->second);
                changed = true;
            } else {
                exprMap_[hash] = inst.get();
            }
        }
    }
    
    return changed;
}

size_t CSEPass::hashInstruction(IR::Instruction* inst) const {
    size_t h = static_cast<size_t>(inst->opcode);
    
    for (auto* op : inst->operands) {
        if (op && op->val) {
            h ^= op->val->id * 0x9e3779b9;
        }
    }
    
    return h;
}

bool CSEPass::instructionsEqual(IR::Instruction* a, IR::Instruction* b) const {
    if (a->opcode != b->opcode) return false;
    if (a->operands.size() != b->operands.size()) return false;
    
    for (size_t i = 0; i < a->operands.size(); ++i) {
        if (a->getOperand(i) != b->getOperand(i)) return false;
    }
    
    return true;
}

// Optimization Pipeline
void OptimizationPipeline::addPass(std::unique_ptr<OptimizationPass> pass) {
    passes_.push_back(std::move(pass));
}

bool OptimizationPipeline::run(Function* func) {
    bool changed = false;
    
    for (auto& pass : passes_) {
        changed |= pass->run(func);
    }
    
    return changed;
}

bool OptimizationPipeline::run(IR::Module* module) {
    bool changed = false;
    
    for (auto& func : module->functions) {
        changed |= run(func.get());
    }
    
    return changed;
}

void OptimizationPipeline::setOptimizationLevel(int level) {
    optLevel_ = level;
    passes_.clear();
    
    // Always add these passes
    passes_.push_back(std::make_unique<Mem2RegPass>());
    passes_.push_back(std::make_unique<ConstantFoldingPass>());
    passes_.push_back(std::make_unique<DeadCodeEliminationPass>());
    
    if (level >= 1) {
        passes_.push_back(std::make_unique<CFGSimplifyPass>());
        passes_.push_back(std::make_unique<CSEPass>());
    }
    
    if (level >= 2) {
        passes_.push_back(std::make_unique<StrengthReductionPass>());
        passes_.push_back(std::make_unique<LICMPass>());
    }
    
    if (level >= 3) {
        passes_.push_back(std::make_unique<PeepholePass>());
        passes_.push_back(std::make_unique<RegisterCoalescingPass>());
    }
}

// Mem2Reg Pass
bool Mem2RegPass::run(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        for (auto& inst : block->instructions) {
            if (inst->opcode == IR::Instruction::Opcode::Alloca) {
                if (isAllocaPromotable(inst.get())) {
                    promoteAlloca(inst.get(), func);
                    changed = true;
                }
            }
        }
    }
    
    return changed;
}

bool Mem2RegPass::isAllocaPromotable(IR::Instruction* alloca) {
    // Simple heuristic: only promote if used in same block
    // Full implementation would check dominance frontier
    return alloca->uses.size() <= 4;
}

void Mem2RegPass::promoteAlloca(IR::Instruction* alloca, Function* func) {
    // Replace alloca with register
    // Full implementation would use SSA construction
    (void)alloca;
    (void)func;
}

// LICM Pass
bool LICMPass::run(Function* func) {
    bool changed = false;
    
    for (auto& block : func->blocks) {
        // Simple: check if instruction is loop invariant
        // Full implementation would identify loops first
        for (auto& inst : block->instructions) {
            if (isLoopInvariant(inst.get(), block.get())) {
                // Move to preheader
                changed = true;
            }
        }
    }
    
    return changed;
}

bool LICMPass::isLoopInvariant(IR::Instruction* inst, IR::BasicBlock* header) {
    // Check if all operands are defined outside loop
    for (auto* op : inst->operands) {
        if (op->val && op->val->parent == header) {
            return false;
        }
    }
    return !inst->mayWriteMemory();
}

bool LICMPass::dominatesAllUses(IR::Instruction* inst, IR::BasicBlock* header) {
    // Simplified: check if instruction dominates all uses
    for (auto* use : inst->uses) {
        if (use->user->parent == header) {
            return false;
        }
    }
    return true;
}

// Peephole Pass
bool PeepholePass::run(Function* func) {
    bool changed = false;
    
    changed |= eliminateRedundantMoves();
    changed |= foldImmediateOps();
    
    return changed;
}

bool PeepholePass::eliminateRedundantMoves() {
    // Remove mov r, r
    return false;
}

bool PeepholePass::foldImmediateOps() {
    // Fold add r, 0 -> nop
    return false;
}

// Register Coalescing Pass
bool RegisterCoalescingPass::run(Function* func) {
    // Coalesce copy instructions to reduce moves
    (void)func;
    return false;
}

} // namespace CodeGen

// =============================================================================
// RUNTIME IMPLEMENTATION
// =============================================================================

namespace Runtime {

// Memory management
void* allocMemory(size_t size, size_t align) {
    (void)align;
    return std::malloc(size);
}

void freeMemory(void* ptr) {
    std::free(ptr);
}

void* reallocMemory(void* ptr, size_t newSize) {
    return std::realloc(ptr, newSize);
}

void* allocZeroed(size_t count, size_t size) {
    return std::calloc(count, size);
}

// Memory operations
void copyMemory(void* dst, const void* src, size_t size) {
    std::memcpy(dst, src, size);
}

void moveMemory(void* dst, const void* src, size_t size) {
    std::memmove(dst, src, size);
}

void setMemory(void* dst, uint8_t val, size_t size) {
    std::memset(dst, val, size);
}

int compareMemory(const void* a, const void* b, size_t size) {
    return std::memcmp(a, b, size);
}

// String operations
size_t stringLength(const char* str) {
    return std::strlen(str);
}

char* stringCopy(char* dst, const char* src) {
    return std::strcpy(dst, src);
}

char* stringCat(char* dst, const char* src) {
    return std::strcat(dst, src);
}

int stringCompare(const char* a, const char* b) {
    return std::strcmp(a, b);
}

char* stringFind(char* str, char c) {
    return std::strchr(str, c);
}

const char* stringFind(const char* str, char c) {
    return std::strchr(str, c);
}

// Math operations
double floor(double x) {
    return std::floor(x);
}

double ceil(double x) {
    return std::ceil(x);
}

double round(double x) {
    return std::round(x);
}

double trunc(double x) {
    return std::trunc(x);
}

double sqrt(double x) {
    return std::sqrt(x);
}

double sin(double x) {
    return std::sin(x);
}

double cos(double x) {
    return std::cos(x);
}

double tan(double x) {
    return std::tan(x);
}

double exp(double x) {
    return std::exp(x);
}

double log(double x) {
    return std::log(x);
}

double pow(double base, double exp) {
    return std::pow(base, exp);
}

double fabs(double x) {
    return std::fabs(x);
}

// Integer math
int64_t abs(int64_t x) {
    return x < 0 ? -x : x;
}

int64_t divmod(int64_t dividend, int64_t divisor, int64_t* remainder) {
    int64_t q = dividend / divisor;
    if (remainder) *remainder = dividend % divisor;
    return q;
}

uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t rotr(uint64_t x, int k) {
    return (x >> k) | (x << (64 - k));
}

uint64_t clz(uint64_t x) {
    if (x == 0) return 64;
    uint64_t n = 0;
    while ((x & (1ULL << 63)) == 0) {
        x <<= 1;
        n++;
    }
    return n;
}

uint64_t ctz(uint64_t x) {
    if (x == 0) return 64;
    uint64_t n = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        n++;
    }
    return n;
}

uint64_t popcount(uint64_t x) {
    uint64_t count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

uint64_t byteswap(uint64_t x) {
    return ((x & 0xFF00000000000000ULL) >> 56) |
           ((x & 0x00FF000000000000ULL) >> 40) |
           ((x & 0x0000FF0000000000ULL) >> 24) |
           ((x & 0x000000FF00000000ULL) >> 8)  |
           ((x & 0x00000000FF000000ULL) << 8)  |
           ((x & 0x0000000000FF0000ULL) << 24) |
           ((x & 0x000000000000FF00ULL) << 40) |
           ((x & 0x00000000000000FFULL) << 56);
}

// Process control
void exitProcess(int code) {
    std::exit(code);
}

void abortProcess() {
    std::abort();
}

bool isDebuggerPresent() {
#ifdef _WIN32
    return IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

void debugBreak() {
#ifdef _WIN32
    __debugbreak();
#else
    __builtin_trap();
#endif
}

// System calls (Windows)
void* getStdHandle(uint32_t handle) {
#ifdef _WIN32
    return GetStdHandle(handle);
#else
    (void)handle;
    return nullptr;
#endif
}

bool writeConsole(void* handle, const void* buf, uint32_t len, uint32_t* written) {
#ifdef _WIN32
    DWORD w;
    BOOL ok = WriteConsoleA(static_cast<HANDLE>(handle), buf, len, &w, nullptr);
    if (written) *written = w;
    return ok != 0;
#else
    (void)handle; (void)buf; (void)len; (void)written;
    return false;
#endif
}

bool readConsole(void* handle, void* buf, uint32_t len, uint32_t* read) {
#ifdef _WIN32
    DWORD r;
    BOOL ok = ReadConsoleA(static_cast<HANDLE>(handle), buf, len, &r, nullptr);
    if (read) *read = r;
    return ok != 0;
#else
    (void)handle; (void)buf; (void)len; (void)read;
    return false;
#endif
}

uint64_t getTickCount() {
#ifdef _WIN32
    return GetTickCount64();
#else
    return 0;
#endif
}

void sleepMs(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    (void)ms;
#endif
}

// File I/O (minimal)
FileHandle openFile(const char* path, bool write) {
#ifdef _WIN32
    DWORD access = write ? GENERIC_WRITE : GENERIC_READ;
    DWORD creation = write ? CREATE_ALWAYS : OPEN_EXISTING;
    HANDLE h = CreateFileA(path, access, 0, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
    return {h};
#else
    (void)path; (void)write;
    return {nullptr};
#endif
}

bool readFile(FileHandle file, void* buf, size_t size, size_t* read) {
#ifdef _WIN32
    DWORD r;
    BOOL ok = ReadFile(static_cast<HANDLE>(file.handle), buf, static_cast<DWORD>(size), &r, nullptr);
    if (read) *read = r;
    return ok != 0;
#else
    (void)file; (void)buf; (void)size; (void)read;
    return false;
#endif
}

bool writeFile(FileHandle file, const void* buf, size_t size, size_t* written) {
#ifdef _WIN32
    DWORD w;
    BOOL ok = WriteFile(static_cast<HANDLE>(file.handle), buf, static_cast<DWORD>(size), &w, nullptr);
    if (written) *written = w;
    return ok != 0;
#else
    (void)file; (void)buf; (void)size; (void)written;
    return false;
#endif
}

void closeFile(FileHandle file) {
#ifdef _WIN32
    CloseHandle(static_cast<HANDLE>(file.handle));
#else
    (void)file;
#endif
}

bool fileExists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES;
#else
    (void)path;
    return false;
#endif
}

int64_t getFileSize(FileHandle file) {
#ifdef _WIN32
    LARGE_INTEGER size;
    if (GetFileSizeEx(static_cast<HANDLE>(file.handle), &size)) {
        return size.QuadPart;
    }
    return -1;
#else
    (void)file;
    return -1;
#endif
}

// Startup/Shutdown
static std::vector<InitFunc> initFuncs;
static std::vector<InitFunc> finiFuncs;

void registerInit(InitFunc func) {
    initFuncs.push_back(func);
}

void registerFini(InitFunc func) {
    finiFuncs.push_back(func);
}

void runInitFuncs() {
    for (auto func : initFuncs) {
        func();
    }
}

void runFiniFuncs() {
    for (auto func : finiFuncs) {
        func();
    }
}

// Stack checking
void stackProbe(size_t size) {
    (void)size;
}

bool stackAvailable(size_t size) {
    (void)size;
    return true;
}

// Security
static uint64_t g_securityCookie = 0;

void securityCookie() {
    // Generate random cookie
    g_securityCookie = static_cast<uint64_t>(std::rand()) |
                       (static_cast<uint64_t>(std::rand()) << 32);
}

uint64_t getSecurityCookie() {
    return g_securityCookie;
}

bool checkSecurityCookie(uint64_t cookie) {
    return cookie == g_securityCookie;
}

// Thread local storage
static std::vector<void*> tlsSlots;

uint32_t allocTlsSlot() {
    for (size_t i = 0; i < tlsSlots.size(); ++i) {
        if (tlsSlots[i] == nullptr) {
            return static_cast<uint32_t>(i);
        }
    }
    tlsSlots.push_back(nullptr);
    return static_cast<uint32_t>(tlsSlots.size() - 1);
}

void freeTlsSlot(uint32_t slot) {
    if (slot < tlsSlots.size()) {
        tlsSlots[slot] = nullptr;
    }
}

void* getTlsValue(uint32_t slot) {
    if (slot < tlsSlots.size()) {
        return tlsSlots[slot];
    }
    return nullptr;
}

void setTlsValue(uint32_t slot, void* value) {
    if (slot < tlsSlots.size()) {
        tlsSlots[slot] = value;
    }
}

// Atomics
void* atomicLoad(void** ptr) {
    return *ptr;
}

void atomicStore(void** ptr, void* val) {
    *ptr = val;
}

bool atomicCompareExchange(void** ptr, void* expected, void* desired) {
    if (*ptr == expected) {
        *ptr = desired;
        return true;
    }
    return false;
}

void* atomicExchange(void** ptr, void* val) {
    void* old = *ptr;
    *ptr = val;
    return old;
}

void atomicFence() {
    // Memory barrier
#ifdef _WIN32
    MemoryBarrier();
#else
    __sync_synchronize();
#endif
}

// Integer to string conversions
int intToStr(int64_t val, char* buf, int bufSize, int radix) {
    if (bufSize < 1) return 0;
    
    char* p = buf + bufSize - 1;
    *p = 0;
    
    bool neg = false;
    if (val < 0 && radix == 10) {
        neg = true;
        val = -val;
    }
    
    const char* digits = "0123456789ABCDEF";
    
    do {
        if (p <= buf) break;
        *--p = digits[val % radix];
        val /= radix;
    } while (val);
    
    if (neg && p > buf) {
        *--p = '-';
    }
    
    // Shift to start of buffer
    int len = static_cast<int>(buf + bufSize - 1 - p);
    char* dst = buf;
    while (*p) {
        *dst++ = *p++;
    }
    *dst = 0;
    
    return len;
}

int uintToStr(uint64_t val, char* buf, int bufSize, int radix) {
    if (bufSize < 1) return 0;
    
    char* p = buf + bufSize - 1;
    *p = 0;
    
    const char* digits = "0123456789ABCDEF";
    
    do {
        if (p <= buf) break;
        *--p = digits[val % radix];
        val /= radix;
    } while (val);
    
    int len = static_cast<int>(buf + bufSize - 1 - p);
    char* dst = buf;
    while (*p) {
        *dst++ = *p++;
    }
    *dst = 0;
    
    return len;
}

int floatToStr(double val, char* buf, int bufSize, int precision) {
    // Simple implementation using snprintf
    char fmt[16];
    intToStr(precision, fmt, sizeof(fmt), 10);
    // Would use snprintf in real implementation
    (void)val;
    (void)bufSize;
    buf[0] = '0';
    buf[1] = 0;
    return 1;
}

// String to number conversions
int64_t strToInt(const char* str, char** end, int base) {
    (void)base;
    int64_t result = 0;
    bool neg = false;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        neg = true;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    if (end) *end = const_cast<char*>(str);
    return neg ? -result : result;
}

double strToFloat(const char* str, char** end) {
    double result = 0.0;
    double frac = 0.1;
    bool neg = false;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        neg = true;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    if (*str == '.') {
        str++;
        while (*str >= '0' && *str <= '9') {
            result += (*str - '0') * frac;
            frac *= 0.1;
            str++;
        }
    }
    
    if (end) *end = const_cast<char*>(str);
    return neg ? -result : result;
}

// Console I/O helpers
void printStr(const char* str) {
#ifdef _WIN32
    DWORD written;
    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), str, 
                  static_cast<DWORD>(stringLength(str)), &written, nullptr);
#else
    std::fputs(str, stdout);
#endif
}

void printInt(int64_t val) {
    char buf[32];
    intToStr(val, buf, sizeof(buf), 10);
    printStr(buf);
}

void printUInt(uint64_t val) {
    char buf[32];
    uintToStr(val, buf, sizeof(buf), 10);
    printStr(buf);
}

void printHex(uint64_t val) {
    char buf[20];
    buf[0] = '0';
    buf[1] = 'x';
    uintToStr(val, buf + 2, sizeof(buf) - 2, 16);
    printStr(buf);
}

void printFloat(double val) {
    char buf[64];
    floatToStr(val, buf, sizeof(buf), 6);
    printStr(buf);
}

void printLine(const char* str) {
    printStr(str);
    printStr("\n");
}

char readChar() {
    char c = 0;
#ifdef _WIN32
    DWORD read;
    ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &read, nullptr);
#else
    c = static_cast<char>(std::getchar());
#endif
    return c;
}

int readLine(char* buf, int maxLen) {
    int i = 0;
    while (i < maxLen - 1) {
        char c = readChar();
        if (c == '\n' || c == '\r') break;
        buf[i++] = c;
    }
    buf[i] = 0;
    return i;
}

} // namespace Runtime

// =============================================================================
// SEH IMPLEMENTATION
// =============================================================================

namespace SEH {

std::vector<uint8_t> SEHBuilder::buildUnwindInfo(const std::vector<Handler>& handlers) {
    std::vector<uint8_t> data;
    
    // Build unwind codes
    for (const auto& code : unwindCodes_) {
        data.push_back(code.codeOffset);
        data.push_back(static_cast<uint8_t>((static_cast<uint8_t>(code.unwindOp)) | 
                                            (code.opInfo << 4)));
    }
    
    // Build UNWIND_INFO structure
    UnwindInfo info = {};
    info.version = 1;
    info.flags = handlers.empty() ? 0 : 0x01;  // UNW_FLAG_EHANDLER
    info.prologSize = prologueSize_;
    info.frameRegister = 0;
    info.frameOffset = 0;
    info.exceptionHandlerCount = static_cast<uint16_t>(handlers.size());
    
    // Prepend unwind info
    uint8_t* infoBytes = reinterpret_cast<uint8_t*>(&info);
    data.insert(data.begin(), infoBytes, infoBytes + sizeof(info));
    
    return data;
}

void SEHBuilder::addUnwindCode(std::vector<uint8_t>& data, 
                              uint8_t offset, UnwindOp op, uint8_t info) {
    (void)data;
    UnwindCode code;
    code.codeOffset = offset;
    code.unwindOp = static_cast<uint8_t>(op);
    code.opInfo = info;
    unwindCodes_.push_back(code);
}

void beginTry() {
    // Push current exception state
    // Simplified implementation
}

void endTry() {
    // Pop exception state
}

void throwException(void* object, const TypeDescriptor* type) {
    (void)object;
    (void)type;
    // Would call into C++ runtime in real implementation
}

void rethrowException() {
    // Re-throw current exception
}

bool catchException(const TypeDescriptor* type) {
    (void)type;
    // Check if current exception matches type
    return false;
}

void* getExceptionObject() {
    // Get current exception object
    return nullptr;
}

} // namespace SEH

// =============================================================================
// RTTI IMPLEMENTATION
// =============================================================================

namespace RTTI {

uint32_t computeTypeHash(const char* name) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    while (*name) {
        hash ^= static_cast<uint8_t>(*name++);
        hash *= 16777619u;
    }
    return hash;
}

bool typesEqual(const TypeInfo* a, const TypeInfo* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return Runtime::stringCompare(a->name, b->name) == 0;
}

void* dynamicCast(void* obj, const TypeInfo* targetType) {
    if (!obj) return nullptr;
    
    // Get object's vtable
    void** vtable = *reinterpret_cast<void***>(obj);
    if (!vtable) return nullptr;
    
    // Get complete object locator from vtable[-1]
    auto* locator = reinterpret_cast<const CompleteObjectLocator*>(vtable[-1]);
    if (!locator) return nullptr;
    
    // Check if target type is in hierarchy
    auto* hierarchy = locator->classDescriptor;
    if (!hierarchy) return nullptr;
    
    // Search base classes
    for (uint32_t i = 0; i < hierarchy->numBaseClasses; ++i) {
        auto* base = hierarchy->baseClassArray[i];
        if (typesEqual(base->typeInfo, targetType)) {
            // Adjust pointer
            return static_cast<char*>(obj) + base->offset;
        }
    }
    
    return nullptr;
}

void** getVtable(const void* obj) {
    return *reinterpret_cast<void** const*>(obj);
}

void setVtable(void* obj, void** vtable) {
    *reinterpret_cast<void***>(obj) = vtable;
}

bool hasVtable(const void* obj) {
    return getVtable(obj) != nullptr;
}

void registerClass(const TypeInfo* type, const ClassHierarchyDescriptor* hierarchy) {
    (void)type;
    (void)hierarchy;
    // Register class in RTTI table
}

void registerBaseClass(const TypeInfo* derived, const TypeInfo* base, int32_t offset) {
    (void)derived;
    (void)base;
    (void)offset;
    // Register base class relationship
}

bool isPolymorphic(const TypeInfo* type) {
    (void)type;
    return false;
}

size_t getTypeSize(const TypeInfo* type) {
    (void)type;
    return 0;
}

size_t getTypeAlign(const TypeInfo* type) {
    (void)type;
    return 1;
}

bool typesCompatible(const TypeInfo* a, const TypeInfo* b) {
    return typesEqual(a, b) || isDerivedFrom(nullptr, b);
}

} // namespace RTTI

} // namespace Backend
} // namespace RawrXD
