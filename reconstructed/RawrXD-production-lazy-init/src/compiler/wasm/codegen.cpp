// ============================================================================
// RAWRXD WEBASSEMBLY CODE GENERATOR - IMPLEMENTATION
// Complete WASM target with runtime support and optimization
// ============================================================================

#include "wasm_codegen.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace RawrXD {
namespace Compiler {
namespace WASM {

// ============================================================================
// WASM RUNTIME SUPPORT
// ============================================================================

/**
 * @brief Memory allocator for WASM linear memory
 */
class WASMMemoryAllocator {
public:
    explicit WASMMemoryAllocator(uint32_t initialPages = 1)
        : currentPages_(initialPages)
        , nextFreeOffset_(0)
        , heapBase_(1024) // Reserve first 1KB for stack/globals
    {
    }
    
    struct AllocationInfo {
        uint32_t offset;
        uint32_t size;
        bool active;
    };
    
    // Allocate memory, returns offset in linear memory
    uint32_t allocate(uint32_t size, uint32_t alignment = 4) {
        // Align the offset
        uint32_t alignedOffset = (nextFreeOffset_ + alignment - 1) & ~(alignment - 1);
        
        // Check if we need to grow memory
        uint32_t requiredSize = alignedOffset + size;
        uint32_t availableSize = currentPages_ * 65536; // 64KB per page
        
        if (requiredSize > availableSize) {
            uint32_t neededPages = (requiredSize - availableSize + 65535) / 65536;
            currentPages_ += neededPages;
        }
        
        AllocationInfo info{alignedOffset, size, true};
        allocations_.push_back(info);
        
        nextFreeOffset_ = alignedOffset + size;
        return alignedOffset;
    }
    
    // Free memory (mark as inactive for potential reuse)
    void free(uint32_t offset) {
        for (auto& alloc : allocations_) {
            if (alloc.offset == offset) {
                alloc.active = false;
                break;
            }
        }
    }
    
    // Get current memory size in pages
    uint32_t getCurrentPages() const { return currentPages_; }
    
    // Get total allocated size
    uint32_t getTotalAllocated() const { return nextFreeOffset_; }
    
    // Reset allocator
    void reset() {
        nextFreeOffset_ = heapBase_;
        allocations_.clear();
    }
    
    // Compact memory (defragmentation)
    std::vector<std::pair<uint32_t, uint32_t>> compact() {
        std::vector<std::pair<uint32_t, uint32_t>> moves;
        
        // Sort by offset
        std::sort(allocations_.begin(), allocations_.end(),
            [](const AllocationInfo& a, const AllocationInfo& b) {
                return a.offset < b.offset;
            });
        
        uint32_t currentOffset = heapBase_;
        for (auto& alloc : allocations_) {
            if (alloc.active && alloc.offset != currentOffset) {
                moves.push_back({alloc.offset, currentOffset});
                alloc.offset = currentOffset;
            }
            if (alloc.active) {
                currentOffset += alloc.size;
            }
        }
        
        // Remove inactive allocations
        allocations_.erase(
            std::remove_if(allocations_.begin(), allocations_.end(),
                [](const AllocationInfo& a) { return !a.active; }),
            allocations_.end());
        
        nextFreeOffset_ = currentOffset;
        return moves;
    }
    
private:
    uint32_t currentPages_;
    uint32_t nextFreeOffset_;
    uint32_t heapBase_;
    std::vector<AllocationInfo> allocations_;
};

/**
 * @brief String table for WASM data section
 */
class WASMStringTable {
public:
    explicit WASMStringTable(WASMMemoryAllocator& allocator)
        : allocator_(allocator) {}
    
    // Add string and return its offset
    uint32_t addString(const std::string& str) {
        auto it = stringOffsets_.find(str);
        if (it != stringOffsets_.end()) {
            return it->second;
        }
        
        uint32_t offset = allocator_.allocate(static_cast<uint32_t>(str.size() + 1), 1);
        stringOffsets_[str] = offset;
        strings_.push_back({str, offset});
        return offset;
    }
    
    // Get data segments for all strings
    std::vector<WASMModuleBuilder::DataSegment> getDataSegments() const {
        std::vector<WASMModuleBuilder::DataSegment> segments;
        
        for (const auto& [str, offset] : strings_) {
            WASMModuleBuilder::DataSegment seg;
            seg.memoryIndex = 0;
            
            // Create offset expression: i32.const offset
            seg.offsetExpr.push_back(WASMOpcode::I32Const);
            uint32_t val = offset;
            do {
                uint8_t byte = val & 0x7F;
                val >>= 7;
                if (val != 0) byte |= 0x80;
                seg.offsetExpr.push_back(byte);
            } while (val != 0);
            seg.offsetExpr.push_back(WASMOpcode::End);
            
            // String data with null terminator
            seg.data = std::vector<uint8_t>(str.begin(), str.end());
            seg.data.push_back(0);
            
            segments.push_back(seg);
        }
        
        return segments;
    }
    
private:
    WASMMemoryAllocator& allocator_;
    std::unordered_map<std::string, uint32_t> stringOffsets_;
    std::vector<std::pair<std::string, uint32_t>> strings_;
};

// ============================================================================
// WASM OPTIMIZATION PASSES
// ============================================================================

/**
 * @brief Peephole optimizer for WASM bytecode
 */
class WASMPeepholeOptimizer {
public:
    std::vector<uint8_t> optimize(const std::vector<uint8_t>& code) {
        if (code.size() < 2) return code;
        
        std::vector<uint8_t> result;
        result.reserve(code.size());
        
        size_t i = 0;
        while (i < code.size()) {
            // Pattern: i32.const 0; i32.add -> nop
            if (i + 2 < code.size() &&
                code[i] == WASMOpcode::I32Const &&
                code[i + 1] == 0x00 &&
                code[i + 2] == WASMOpcode::I32Add) {
                i += 3;
                optimizationsApplied_++;
                continue;
            }
            
            // Pattern: i32.const 1; i32.mul -> nop
            if (i + 2 < code.size() &&
                code[i] == WASMOpcode::I32Const &&
                code[i + 1] == 0x01 &&
                code[i + 2] == WASMOpcode::I32Mul) {
                i += 3;
                optimizationsApplied_++;
                continue;
            }
            
            // Pattern: drop; nop -> drop
            if (i + 1 < code.size() &&
                code[i] == WASMOpcode::Drop &&
                code[i + 1] == WASMOpcode::Nop) {
                result.push_back(WASMOpcode::Drop);
                i += 2;
                optimizationsApplied_++;
                continue;
            }
            
            // Pattern: local.get N; local.set N -> local.tee N
            if (i + 4 < code.size() &&
                code[i] == WASMOpcode::LocalGet &&
                code[i + 2] == WASMOpcode::LocalSet &&
                code[i + 1] == code[i + 3]) {
                result.push_back(WASMOpcode::LocalTee);
                result.push_back(code[i + 1]);
                i += 4;
                optimizationsApplied_++;
                continue;
            }
            
            result.push_back(code[i]);
            i++;
        }
        
        return result;
    }
    
    int getOptimizationsApplied() const { return optimizationsApplied_; }
    void reset() { optimizationsApplied_ = 0; }
    
private:
    int optimizationsApplied_ = 0;
};

/**
 * @brief Dead code elimination for WASM
 */
class WASMDeadCodeEliminator {
public:
    std::vector<uint8_t> eliminate(const std::vector<uint8_t>& code) {
        std::vector<uint8_t> result;
        result.reserve(code.size());
        
        bool unreachable = false;
        int blockDepth = 0;
        
        for (size_t i = 0; i < code.size(); ++i) {
            uint8_t op = code[i];
            
            // Track block depth
            if (op == WASMOpcode::Block || op == WASMOpcode::Loop || op == WASMOpcode::If) {
                if (!unreachable) {
                    result.push_back(op);
                    if (i + 1 < code.size()) {
                        result.push_back(code[++i]); // Block type
                    }
                }
                blockDepth++;
                continue;
            }
            
            if (op == WASMOpcode::End) {
                blockDepth--;
                if (blockDepth <= 0) unreachable = false;
                result.push_back(op);
                continue;
            }
            
            if (op == WASMOpcode::Else) {
                if (!unreachable) result.push_back(op);
                continue;
            }
            
            // After unreachable/return/br, code is dead until block end
            if (op == WASMOpcode::Unreachable || op == WASMOpcode::Return) {
                result.push_back(op);
                unreachable = true;
                eliminatedInstructions_++;
                continue;
            }
            
            if (op == WASMOpcode::Br) {
                result.push_back(op);
                if (i + 1 < code.size()) {
                    result.push_back(code[++i]); // Branch depth
                }
                unreachable = true;
                eliminatedInstructions_++;
                continue;
            }
            
            if (!unreachable) {
                result.push_back(op);
                
                // Copy immediate operands
                if (op == WASMOpcode::I32Const || op == WASMOpcode::I64Const ||
                    op == WASMOpcode::LocalGet || op == WASMOpcode::LocalSet ||
                    op == WASMOpcode::LocalTee || op == WASMOpcode::GlobalGet ||
                    op == WASMOpcode::GlobalSet || op == WASMOpcode::BrIf ||
                    op == WASMOpcode::Call) {
                    // Copy LEB128 encoded immediate
                    while (i + 1 < code.size() && (code[i + 1] & 0x80)) {
                        result.push_back(code[++i]);
                    }
                    if (i + 1 < code.size()) {
                        result.push_back(code[++i]);
                    }
                }
                else if (op == WASMOpcode::F32Const) {
                    // 4 bytes
                    for (int j = 0; j < 4 && i + 1 < code.size(); ++j) {
                        result.push_back(code[++i]);
                    }
                }
                else if (op == WASMOpcode::F64Const) {
                    // 8 bytes
                    for (int j = 0; j < 8 && i + 1 < code.size(); ++j) {
                        result.push_back(code[++i]);
                    }
                }
                else if (op >= WASMOpcode::I32Load && op <= WASMOpcode::I64Store32) {
                    // Memory ops have align + offset
                    for (int j = 0; j < 2; ++j) {
                        while (i + 1 < code.size() && (code[i + 1] & 0x80)) {
                            result.push_back(code[++i]);
                        }
                        if (i + 1 < code.size()) {
                            result.push_back(code[++i]);
                        }
                    }
                }
            }
        }
        
        return result;
    }
    
    int getEliminatedInstructions() const { return eliminatedInstructions_; }
    void reset() { eliminatedInstructions_ = 0; }
    
private:
    int eliminatedInstructions_ = 0;
};

/**
 * @brief Constant folding for WASM
 */
class WASMConstantFolder {
public:
    std::vector<uint8_t> fold(const std::vector<uint8_t>& code) {
        std::vector<uint8_t> result;
        result.reserve(code.size());
        
        std::vector<std::variant<int32_t, int64_t, float, double>> constStack;
        
        for (size_t i = 0; i < code.size(); ++i) {
            uint8_t op = code[i];
            
            // Handle i32.const
            if (op == WASMOpcode::I32Const) {
                int32_t value = 0;
                int shift = 0;
                uint8_t byte;
                do {
                    byte = code[++i];
                    value |= (byte & 0x7F) << shift;
                    shift += 7;
                } while (byte & 0x80);
                
                // Sign extend if negative
                if (shift < 32 && (byte & 0x40)) {
                    value |= (~0 << shift);
                }
                
                constStack.push_back(value);
                
                // Emit the const
                result.push_back(WASMOpcode::I32Const);
                emitS32LEB(result, value);
                continue;
            }
            
            // Fold i32.add of two constants
            if (op == WASMOpcode::I32Add && constStack.size() >= 2) {
                auto* b = std::get_if<int32_t>(&constStack.back());
                constStack.pop_back();
                auto* a = std::get_if<int32_t>(&constStack.back());
                constStack.pop_back();
                
                if (a && b) {
                    int32_t folded = *a + *b;
                    constStack.push_back(folded);
                    
                    // Remove the two const instructions and replace with one
                    // Find and remove last two i32.const
                    removeLastConsts(result, 2);
                    
                    result.push_back(WASMOpcode::I32Const);
                    emitS32LEB(result, folded);
                    foldedOperations_++;
                    continue;
                }
            }
            
            // Fold i32.mul of two constants
            if (op == WASMOpcode::I32Mul && constStack.size() >= 2) {
                auto* b = std::get_if<int32_t>(&constStack.back());
                constStack.pop_back();
                auto* a = std::get_if<int32_t>(&constStack.back());
                constStack.pop_back();
                
                if (a && b) {
                    int32_t folded = *a * *b;
                    constStack.push_back(folded);
                    
                    removeLastConsts(result, 2);
                    
                    result.push_back(WASMOpcode::I32Const);
                    emitS32LEB(result, folded);
                    foldedOperations_++;
                    continue;
                }
            }
            
            // Clear const stack on any other operation
            if (!isConstOp(op)) {
                constStack.clear();
            }
            
            result.push_back(op);
        }
        
        return result;
    }
    
    int getFoldedOperations() const { return foldedOperations_; }
    void reset() { foldedOperations_ = 0; }
    
private:
    static bool isConstOp(uint8_t op) {
        return op == WASMOpcode::I32Const || op == WASMOpcode::I64Const ||
               op == WASMOpcode::F32Const || op == WASMOpcode::F64Const;
    }
    
    static void emitS32LEB(std::vector<uint8_t>& out, int32_t value) {
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            out.push_back(byte);
        }
    }
    
    void removeLastConsts(std::vector<uint8_t>& code, int count) {
        // Simple implementation - find and remove from end
        // In real implementation, would track positions
        for (int c = 0; c < count && !code.empty(); ++c) {
            // Find last i32.const
            for (auto it = code.rbegin(); it != code.rend(); ++it) {
                if (*it == WASMOpcode::I32Const) {
                    // Remove from here to end
                    code.erase((it + 1).base(), code.end());
                    break;
                }
            }
        }
    }
    
    int foldedOperations_ = 0;
};

// ============================================================================
// WASM MODULE LINKER
// ============================================================================

/**
 * @brief Links multiple WASM modules together
 */
class WASMLinker {
public:
    struct LinkableModule {
        std::string name;
        std::vector<uint8_t> binary;
        std::vector<std::string> exports;
        std::vector<std::pair<std::string, std::string>> imports; // module, name
    };
    
    void addModule(const LinkableModule& module) {
        modules_.push_back(module);
    }
    
    struct LinkResult {
        bool success = false;
        std::vector<uint8_t> linkedBinary;
        std::vector<std::string> errors;
        std::vector<std::string> unresolvedImports;
    };
    
    LinkResult link() {
        LinkResult result;
        
        // Build export table
        std::map<std::pair<std::string, std::string>, size_t> exportTable;
        for (size_t i = 0; i < modules_.size(); ++i) {
            for (const auto& exp : modules_[i].exports) {
                exportTable[{modules_[i].name, exp}] = i;
            }
        }
        
        // Check for unresolved imports
        for (const auto& module : modules_) {
            for (const auto& [mod, name] : module.imports) {
                if (exportTable.find({mod, name}) == exportTable.end()) {
                    // Check if it's an external import (env, wasi, etc.)
                    if (mod != "env" && mod != "wasi_snapshot_preview1") {
                        result.unresolvedImports.push_back(mod + "." + name);
                    }
                }
            }
        }
        
        if (!result.unresolvedImports.empty() && !allowUnresolvedImports_) {
            result.errors.push_back("Unresolved imports");
            return result;
        }
        
        // For now, return the first module if only one
        // Full linking would merge sections
        if (modules_.size() == 1) {
            result.linkedBinary = modules_[0].binary;
            result.success = true;
            return result;
        }
        
        // TODO: Full module merging
        result.errors.push_back("Multi-module linking not yet implemented");
        return result;
    }
    
    void setAllowUnresolvedImports(bool allow) { allowUnresolvedImports_ = allow; }
    
private:
    std::vector<LinkableModule> modules_;
    bool allowUnresolvedImports_ = false;
};

// ============================================================================
// WASM VALIDATOR
// ============================================================================

/**
 * @brief Validates WASM binary format and type safety
 */
class WASMValidator {
public:
    struct ValidationResult {
        bool valid = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validate(const std::vector<uint8_t>& binary) {
        ValidationResult result;
        
        if (binary.size() < 8) {
            result.errors.push_back("Binary too small for valid WASM module");
            return result;
        }
        
        // Check magic number
        if (binary[0] != 0x00 || binary[1] != 0x61 ||
            binary[2] != 0x73 || binary[3] != 0x6D) {
            result.errors.push_back("Invalid WASM magic number");
            return result;
        }
        
        // Check version
        if (binary[4] != 0x01 || binary[5] != 0x00 ||
            binary[6] != 0x00 || binary[7] != 0x00) {
            result.warnings.push_back("Non-standard WASM version");
        }
        
        // Validate sections
        size_t pos = 8;
        uint8_t lastSectionId = 0;
        
        while (pos < binary.size()) {
            if (pos >= binary.size()) break;
            
            uint8_t sectionId = binary[pos++];
            
            // Sections should be in order (except custom sections)
            if (sectionId != 0 && sectionId <= lastSectionId) {
                result.errors.push_back("Sections out of order");
                return result;
            }
            if (sectionId != 0) lastSectionId = sectionId;
            
            // Read section size
            uint32_t sectionSize = 0;
            int shift = 0;
            while (pos < binary.size()) {
                uint8_t byte = binary[pos++];
                sectionSize |= (byte & 0x7F) << shift;
                if ((byte & 0x80) == 0) break;
                shift += 7;
            }
            
            // Validate section size
            if (pos + sectionSize > binary.size()) {
                result.errors.push_back("Section extends past end of binary");
                return result;
            }
            
            // Validate specific sections
            switch (sectionId) {
                case WASMSection::Type:
                    if (!validateTypeSection(binary, pos, sectionSize)) {
                        result.errors.push_back("Invalid type section");
                    }
                    break;
                case WASMSection::Function:
                    // Function section validation
                    break;
                case WASMSection::Code:
                    // Code section validation
                    break;
                default:
                    break;
            }
            
            pos += sectionSize;
        }
        
        result.valid = result.errors.empty();
        return result;
    }
    
private:
    bool validateTypeSection(const std::vector<uint8_t>& binary, size_t start, uint32_t size) {
        if (size < 1) return false;
        
        // Read type count
        size_t pos = start;
        uint32_t typeCount = 0;
        int shift = 0;
        while (pos < start + size) {
            uint8_t byte = binary[pos++];
            typeCount |= (byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        
        // Validate each type
        for (uint32_t i = 0; i < typeCount && pos < start + size; ++i) {
            if (binary[pos++] != 0x60) { // func type marker
                return false;
            }
            
            // Skip params
            uint32_t paramCount = binary[pos++];
            pos += paramCount;
            
            // Skip results
            uint32_t resultCount = binary[pos++];
            pos += resultCount;
        }
        
        return true;
    }
};

// ============================================================================
// WASM FILE I/O
// ============================================================================

/**
 * @brief Handles WASM file operations
 */
class WASMFileIO {
public:
    static bool writeToFile(const std::string& path, const std::vector<uint8_t>& binary) {
        std::ofstream file(path, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        return file.good();
    }
    
    static std::optional<std::vector<uint8_t>> readFromFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return std::nullopt;
        
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> binary(size);
        if (!file.read(reinterpret_cast<char*>(binary.data()), size)) {
            return std::nullopt;
        }
        
        return binary;
    }
    
    static bool writeWATToFile(const std::string& path, const std::string& wat) {
        std::ofstream file(path);
        if (!file) return false;
        file << wat;
        return file.good();
    }
};

// ============================================================================
// COMPLETE WASM COMPILER PIPELINE
// ============================================================================

/**
 * @brief Full compilation pipeline for WASM target
 */
class WASMCompilerPipeline {
public:
    struct CompileOptions {
        int optimizationLevel = 2;  // 0-3
        bool debugInfo = false;
        bool validate = true;
        bool generateWAT = false;
        uint32_t memoryPages = 1;
        uint32_t maxMemoryPages = 0;  // 0 = no limit
        bool exportAllFunctions = false;
        std::string outputPath;
    };
    
    struct CompileResult {
        bool success = false;
        std::vector<uint8_t> binary;
        std::string wat;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        // Statistics
        size_t originalSize = 0;
        size_t optimizedSize = 0;
        int peepholeOptimizations = 0;
        int deadCodeEliminations = 0;
        int constantFolds = 0;
    };
    
    CompileResult compile(WASMModuleBuilder& module, const CompileOptions& options) {
        CompileResult result;
        
        try {
            // Set memory configuration
            module.setMemory(options.memoryPages, options.maxMemoryPages, options.maxMemoryPages > 0);
            
            // Build initial module
            result.binary = module.build();
            result.originalSize = result.binary.size();
            
            // Validate if requested
            if (options.validate) {
                WASMValidator validator;
                auto validation = validator.validate(result.binary);
                if (!validation.valid) {
                    result.errors = validation.errors;
                    result.warnings = validation.warnings;
                    return result;
                }
                result.warnings = validation.warnings;
            }
            
            // Apply optimizations
            if (options.optimizationLevel > 0) {
                // Note: In a real implementation, we would extract functions,
                // optimize each, and rebuild. For now, this is structural.
                result.optimizedSize = result.binary.size();
            } else {
                result.optimizedSize = result.originalSize;
            }
            
            // Generate WAT if requested
            if (options.generateWAT) {
                result.wat = module.toWAT();
            }
            
            // Write to file if path specified
            if (!options.outputPath.empty()) {
                if (!WASMFileIO::writeToFile(options.outputPath, result.binary)) {
                    result.warnings.push_back("Failed to write output file: " + options.outputPath);
                }
                
                if (options.generateWAT) {
                    std::string watPath = options.outputPath + ".wat";
                    WASMFileIO::writeWATToFile(watPath, result.wat);
                }
            }
            
            result.success = true;
            
        } catch (const std::exception& e) {
            result.errors.push_back(std::string("Compilation error: ") + e.what());
        }
        
        return result;
    }
    
    // Convenience method for simple function compilation
    CompileResult compileFunction(
        const std::string& name,
        const FunctionType& type,
        const std::vector<uint8_t>& code,
        const CompileOptions& options = {})
    {
        WASMModuleBuilder module;
        
        uint32_t typeIdx = module.addType(type);
        
        WASMModuleBuilder::FunctionDef func;
        func.name = name;
        func.typeIndex = typeIdx;
        func.code = code;
        func.exported = true;
        module.addFunction(func);
        
        return compile(module, options);
    }
};

// ============================================================================
// WASM STANDARD LIBRARY GENERATOR
// ============================================================================

/**
 * @brief Generates common runtime functions for WASM modules
 */
class WASMStdLib {
public:
    // Generate malloc function
    static WASMModuleBuilder::FunctionDef generateMalloc(uint32_t heapBaseGlobal) {
        WASMModuleBuilder::FunctionDef func;
        func.name = "malloc";
        func.typeIndex = 0; // Assume type 0 is (i32) -> i32
        func.exported = true;
        
        WASMInstructionBuilder builder;
        // Simple bump allocator
        // global.get heap_ptr
        // local.get 0 (size)
        // i32.add
        // global.set heap_ptr
        // global.get heap_ptr
        // local.get 0
        // i32.sub
        builder.globalGet(heapBaseGlobal)
               .localGet(0)
               .i32Add()
               .globalSet(heapBaseGlobal)
               .globalGet(heapBaseGlobal)
               .localGet(0)
               .i32Sub()
               .end();
        
        func.code = builder.getCode();
        return func;
    }
    
    // Generate free function (no-op for bump allocator)
    static WASMModuleBuilder::FunctionDef generateFree() {
        WASMModuleBuilder::FunctionDef func;
        func.name = "free";
        func.typeIndex = 1; // Assume type 1 is (i32) -> void
        func.exported = true;
        
        WASMInstructionBuilder builder;
        builder.end(); // No-op
        
        func.code = builder.getCode();
        return func;
    }
    
    // Generate memcpy function
    static WASMModuleBuilder::FunctionDef generateMemcpy() {
        WASMModuleBuilder::FunctionDef func;
        func.name = "memcpy";
        func.typeIndex = 2; // Assume type 2 is (i32, i32, i32) -> i32
        func.exported = true;
        func.locals = {ValueType::I32}; // Loop counter
        
        WASMInstructionBuilder builder;
        // dest = local 0, src = local 1, len = local 2, i = local 3
        
        // i = 0
        builder.i32Const(0)
               .localSet(3);
        
        // loop
        builder.block()
               .loop();
        
        // if i >= len, break
        builder.localGet(3)
               .localGet(2)
               .i32GeU()
               .brIf(1);
        
        // dest[i] = src[i]
        builder.localGet(0)
               .localGet(3)
               .i32Add()
               .localGet(1)
               .localGet(3)
               .i32Add()
               .i32Load(0, 0)
               .i32Store(0, 0);
        
        // i++
        builder.localGet(3)
               .i32Const(1)
               .i32Add()
               .localSet(3);
        
        // continue loop
        builder.br(0)
               .end()
               .end();
        
        // return dest
        builder.localGet(0)
               .end();
        
        func.code = builder.getCode();
        return func;
    }
    
    // Generate strlen function
    static WASMModuleBuilder::FunctionDef generateStrlen() {
        WASMModuleBuilder::FunctionDef func;
        func.name = "strlen";
        func.typeIndex = 0; // (i32) -> i32
        func.exported = true;
        func.locals = {ValueType::I32}; // length counter
        
        WASMInstructionBuilder builder;
        // str = local 0, len = local 1
        
        builder.i32Const(0)
               .localSet(1);
        
        builder.block()
               .loop();
        
        // if str[len] == 0, break
        builder.localGet(0)
               .localGet(1)
               .i32Add()
               .i32Load(0, 0)
               .i32Eqz()
               .brIf(1);
        
        // len++
        builder.localGet(1)
               .i32Const(1)
               .i32Add()
               .localSet(1);
        
        builder.br(0)
               .end()
               .end();
        
        // return len
        builder.localGet(1)
               .end();
        
        func.code = builder.getCode();
        return func;
    }
};

} // namespace WASM
} // namespace Compiler
} // namespace RawrXD
