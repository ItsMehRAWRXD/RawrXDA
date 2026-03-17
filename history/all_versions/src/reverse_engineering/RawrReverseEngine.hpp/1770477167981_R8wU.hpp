#pragma once

#include "RawrCodex.hpp"
#include "RawrDumpBin.hpp"
#include "RawrCompiler.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace RawrXD {
namespace ReverseEngineering {

// ============================================================================
// RAWR REVERSE ENGINE - Complete Binary Analysis & Manipulation Suite
// Provides unified interface for all reverse engineering operations
// ============================================================================

// Forward declarations
class RawrReverseEngine;

// ============================================================================
// Call Graph Analysis
// ============================================================================

struct CallNode {
    uint64_t address;
    std::string name;
    std::vector<uint64_t> callees;  // Functions this one calls
    std::vector<uint64_t> callers;  // Functions that call this one
    size_t instructionCount;
    size_t basicBlockCount;
    bool isLeaf;      // No calls to other functions
    bool isRecursive; // Calls itself
};

struct CallGraph {
    std::unordered_map<uint64_t, CallNode> nodes;
    uint64_t entryPoint;
    size_t totalFunctions;
    size_t totalEdges;
};

// ============================================================================
// Data Flow Analysis
// ============================================================================

struct DataFlowInfo {
    uint64_t address;
    std::string varName;
    std::string type;
    bool isParameter;
    bool isReturnValue;
    bool isGlobal;
    std::vector<uint64_t> definitions;  // Where value is defined
    std::vector<uint64_t> uses;         // Where value is used
};

// ============================================================================
// Signature Matching
// ============================================================================

struct SignatureMatch {
    std::string libraryName;
    std::string functionName;
    uint64_t matchedAddress;
    float confidence;   // 0.0 - 1.0
    std::string matchType; // "exact", "partial", "heuristic"
};

// ============================================================================
// Decompilation Result
// ============================================================================

struct DecompilationResult {
    bool success;
    std::string code;       // Decompiled C/C++ code
    std::string pseudocode; // High-level pseudocode
    std::vector<std::string> variables;
    std::vector<std::string> types;
    std::vector<std::string> warnings;
};

// ============================================================================
// Binary Diff
// ============================================================================

struct BinaryDiff {
    struct Change {
        uint64_t address;
        std::vector<uint8_t> original;
        std::vector<uint8_t> modified;
        std::string changeType; // "added", "removed", "modified"
    };
    
    std::vector<Change> changes;
    float similarityScore;
    size_t totalBytesChanged;
};

// ============================================================================
// Main Reverse Engineering Engine Class
// ============================================================================

class RawrReverseEngine {
public:
    RawrReverseEngine() {}
    ~RawrReverseEngine() = default;

    // ========================================================================
    // Core Loading & Analysis
    // ========================================================================

    bool LoadBinary(const std::string& path) {
        m_currentPath = path;
        bool result = m_codex.LoadBinary(path);
        if (result) {
            AnalyzeStructure();
        }
        return result;
    }

    // ========================================================================
    // Call Graph Analysis
    // ========================================================================

    CallGraph BuildCallGraph() {
        CallGraph graph;
        graph.entryPoint = 0;
        graph.totalFunctions = 0;
        graph.totalEdges = 0;
        
        auto symbols = m_codex.GetSymbols();
        
        for (const auto& sym : symbols) {
            if (sym.type != "function") continue;
            
            CallNode node;
            node.address = sym.address;
            node.name = sym.name;
            node.instructionCount = 0;
            node.basicBlockCount = 1;
            node.isLeaf = true;
            node.isRecursive = false;
            
            // Disassemble function to find calls
            auto disasm = m_codex.Disassemble(sym.address, 500);
            node.instructionCount = disasm.size();
            
            for (const auto& instr : disasm) {
                if (instr.mnemonic == "call") {
                    node.isLeaf = false;
                    // Parse call target from operands
                    uint64_t target = ParseCallTarget(instr.operands);
                    if (target != 0) {
                        node.callees.push_back(target);
                        if (target == sym.address) {
                            node.isRecursive = true;
                        }
                        graph.totalEdges++;
                    }
                }
                // Count basic block boundaries
                if (instr.mnemonic == "jmp" || instr.mnemonic == "ret" ||
                    instr.mnemonic.find("j") == 0) {  // jcc instructions
                    node.basicBlockCount++;
                }
            }
            
            graph.nodes[sym.address] = node;
            graph.totalFunctions++;
        }
        
        // Build caller relationships (reverse edges)
        for (auto& [addr, node] : graph.nodes) {
            for (uint64_t callee : node.callees) {
                if (graph.nodes.find(callee) != graph.nodes.end()) {
                    graph.nodes[callee].callers.push_back(addr);
                }
            }
        }
        
        return graph;
    }

    std::string PrintCallGraph(const CallGraph& graph) {
        std::ostringstream oss;
        oss << "=== Call Graph Analysis ===\n";
        oss << "Total Functions: " << graph.totalFunctions << "\n";
        oss << "Total Call Edges: " << graph.totalEdges << "\n\n";
        
        for (const auto& [addr, node] : graph.nodes) {
            oss << "Function: " << node.name << " @ 0x" << std::hex << addr << "\n";
            oss << "  Instructions: " << std::dec << node.instructionCount << "\n";
            oss << "  Basic Blocks: " << node.basicBlockCount << "\n";
            oss << "  Properties: ";
            if (node.isLeaf) oss << "[LEAF] ";
            if (node.isRecursive) oss << "[RECURSIVE] ";
            oss << "\n";
            
            if (!node.callees.empty()) {
                oss << "  Calls:\n";
                for (uint64_t callee : node.callees) {
                    oss << "    -> 0x" << std::hex << callee;
                    if (graph.nodes.find(callee) != graph.nodes.end()) {
                        oss << " (" << graph.nodes.at(callee).name << ")";
                    }
                    oss << "\n";
                }
            }
            
            if (!node.callers.empty()) {
                oss << "  Called by:\n";
                for (uint64_t caller : node.callers) {
                    oss << "    <- 0x" << std::hex << caller;
                    if (graph.nodes.find(caller) != graph.nodes.end()) {
                        oss << " (" << graph.nodes.at(caller).name << ")";
                    }
                    oss << "\n";
                }
            }
            oss << "\n";
        }
        
        return oss.str();
    }

    // ========================================================================
    // Signature Matching (FLIRT-style)
    // ========================================================================

    std::vector<SignatureMatch> MatchSignatures() {
        std::vector<SignatureMatch> matches;
        
        // Built-in signature patterns for common libraries
        struct SigPattern {
            std::string name;
            std::string library;
            std::vector<uint8_t> pattern;
            std::vector<uint8_t> mask; // 0xFF = exact, 0x00 = wildcard
        };
        
        std::vector<SigPattern> signatures = {
            // CRT signatures
            {"__security_check_cookie", "MSVCRT", 
             {0x48, 0x3B, 0x0D}, {0xFF, 0xFF, 0xFF}},
            {"memcpy", "MSVCRT",
             {0x48, 0x89, 0x54, 0x24}, {0xFF, 0xFF, 0xFF, 0xFF}},
            {"strlen", "MSVCRT",
             {0x48, 0x83, 0xEC, 0x28}, {0xFF, 0xFF, 0xFF, 0x00}},
            // Crypto signatures
            {"AES_set_encrypt_key", "OpenSSL",
             {0x48, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00}, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
            // Memory allocation
            {"malloc", "CRT",
             {0x48, 0x85, 0xC9, 0x74}, {0xFF, 0xFF, 0xFF, 0xFF}},
        };
        
        auto symbols = m_codex.GetSymbols();
        
        for (const auto& sym : symbols) {
            if (sym.type != "function") continue;
            
            // Get function prologue bytes
            auto disasm = m_codex.Disassemble(sym.address, 10);
            if (disasm.empty()) continue;
            
            std::vector<uint8_t> prologue;
            for (const auto& instr : disasm) {
                for (uint8_t b : instr.bytes) {
                    prologue.push_back(b);
                    if (prologue.size() >= 16) break;
                }
                if (prologue.size() >= 16) break;
            }
            
            // Match against signatures
            for (const auto& sig : signatures) {
                if (MatchPattern(prologue, sig.pattern, sig.mask)) {
                    SignatureMatch match;
                    match.libraryName = sig.library;
                    match.functionName = sig.name;
                    match.matchedAddress = sym.address;
                    match.confidence = 0.85f;
                    match.matchType = "heuristic";
                    matches.push_back(match);
                }
            }
        }
        
        return matches;
    }

    // ========================================================================
    // Simple Decompilation
    // ========================================================================

    DecompilationResult Decompile(uint64_t functionAddr) {
        DecompilationResult result;
        result.success = false;
        
        // Find the function symbol
        auto symbols = m_codex.GetSymbols();
        std::string funcName = "sub_" + std::to_string(functionAddr);
        for (const auto& sym : symbols) {
            if (sym.address == functionAddr) {
                funcName = sym.name;
                break;
            }
        }
        
        // Disassemble the function
        auto disasm = m_codex.Disassemble(functionAddr, 200);
        if (disasm.empty()) {
            result.warnings.push_back("Failed to disassemble function");
            return result;
        }
        
        // Generate pseudocode
        std::ostringstream pseudo;
        pseudo << "// Function: " << funcName << "\n";
        pseudo << "// Address: 0x" << std::hex << functionAddr << "\n\n";
        pseudo << "void " << funcName << "() {\n";
        
        int indentLevel = 1;
        std::string indent(indentLevel * 4, ' ');
        
        for (const auto& instr : disasm) {
            if (instr.mnemonic == "ret") {
                pseudo << indent << "return;\n";
                break;
            } else if (instr.mnemonic == "push") {
                pseudo << indent << "// push " << instr.operands << "\n";
            } else if (instr.mnemonic == "pop") {
                pseudo << indent << "// pop " << instr.operands << "\n";
            } else if (instr.mnemonic == "mov") {
                pseudo << indent << instr.operands << ";\n";
            } else if (instr.mnemonic == "call") {
                pseudo << indent << "call_" << instr.operands << "();\n";
            } else if (instr.mnemonic == "cmp") {
                pseudo << indent << "if (" << instr.operands << ") {\n";
                indentLevel++;
                indent = std::string(indentLevel * 4, ' ');
            } else if (instr.mnemonic.find("j") == 0 && instr.mnemonic != "jmp") {
                // Conditional jump
                pseudo << indent << "// conditional: " << instr.mnemonic << " " << instr.operands << "\n";
            } else if (instr.mnemonic == "jmp") {
                pseudo << indent << "goto label_" << instr.operands << ";\n";
            } else if (instr.mnemonic == "add") {
                pseudo << indent << instr.operands << " += ...;\n";
            } else if (instr.mnemonic == "sub") {
                pseudo << indent << instr.operands << " -= ...;\n";
            } else if (instr.mnemonic == "xor") {
                pseudo << indent << instr.operands << " ^= ...;\n";
            } else if (instr.mnemonic == "lea") {
                pseudo << indent << "// lea: " << instr.operands << "\n";
            } else if (instr.mnemonic != "nop") {
                pseudo << indent << "// " << instr.mnemonic << " " << instr.operands << "\n";
            }
        }
        
        pseudo << "}\n";
        
        result.success = true;
        result.pseudocode = pseudo.str();
        result.code = pseudo.str(); // Same for now
        
        return result;
    }

    // ========================================================================
    // Binary Diff
    // ========================================================================

    BinaryDiff CompareBinaries(const std::string& path1, const std::string& path2) {
        BinaryDiff diff;
        diff.similarityScore = 0.0f;
        diff.totalBytesChanged = 0;
        
        // Load both binaries
        std::ifstream file1(path1, std::ios::binary);
        std::ifstream file2(path2, std::ios::binary);
        
        if (!file1 || !file2) {
            return diff;
        }
        
        std::vector<uint8_t> data1((std::istreambuf_iterator<char>(file1)),
                                   std::istreambuf_iterator<char>());
        std::vector<uint8_t> data2((std::istreambuf_iterator<char>(file2)),
                                   std::istreambuf_iterator<char>());
        
        size_t minSize = std::min(data1.size(), data2.size());
        size_t maxSize = std::max(data1.size(), data2.size());
        size_t matching = 0;
        
        // Find differences
        for (size_t i = 0; i < minSize; ++i) {
            if (data1[i] == data2[i]) {
                matching++;
            } else {
                BinaryDiff::Change change;
                change.address = i;
                change.original = {data1[i]};
                change.modified = {data2[i]};
                change.changeType = "modified";
                diff.changes.push_back(change);
                diff.totalBytesChanged++;
            }
        }
        
        // Handle size difference
        if (data1.size() != data2.size()) {
            BinaryDiff::Change change;
            change.address = minSize;
            change.changeType = (data1.size() > data2.size()) ? "removed" : "added";
            diff.changes.push_back(change);
            diff.totalBytesChanged += (maxSize - minSize);
        }
        
        diff.similarityScore = static_cast<float>(matching) / maxSize;
        
        return diff;
    }

    std::string PrintBinaryDiff(const BinaryDiff& diff) {
        std::ostringstream oss;
        oss << "=== Binary Diff ===\n";
        oss << "Similarity: " << std::fixed << std::setprecision(2) 
            << (diff.similarityScore * 100) << "%\n";
        oss << "Total Bytes Changed: " << diff.totalBytesChanged << "\n\n";
        
        if (diff.changes.empty()) {
            oss << "Binaries are identical!\n";
        } else {
            oss << "Changes:\n";
            size_t shown = 0;
            for (const auto& change : diff.changes) {
                if (shown >= 50) {
                    oss << "... and " << (diff.changes.size() - 50) << " more changes\n";
                    break;
                }
                oss << "  0x" << std::hex << std::setw(8) << std::setfill('0') 
                    << change.address << " [" << change.changeType << "]";
                if (change.changeType == "modified") {
                    oss << " " << std::hex << (int)change.original[0] 
                        << " -> " << (int)change.modified[0];
                }
                oss << "\n";
                shown++;
            }
        }
        
        return oss.str();
    }

    // ========================================================================
    // Vulnerability Analysis
    // ========================================================================

    std::string AnalyzeVulnerabilities() {
        auto vulns = m_codex.DetectVulnerabilities();
        
        std::ostringstream oss;
        oss << "=== Vulnerability Analysis ===\n";
        oss << "Total Issues Found: " << vulns.size() << "\n\n";
        
        int critical = 0, high = 0, medium = 0, low = 0;
        
        for (const auto& vuln : vulns) {
            if (vuln.severity == "critical") critical++;
            else if (vuln.severity == "high") high++;
            else if (vuln.severity == "medium") medium++;
            else low++;
            
            oss << "[" << vuln.severity << "] " << vuln.type << "\n";
            oss << "  Address: 0x" << std::hex << vuln.address << "\n";
            oss << "  Description: " << vuln.description << "\n\n";
        }
        
        oss << "\nSummary:\n";
        oss << "  Critical: " << critical << "\n";
        oss << "  High: " << high << "\n";
        oss << "  Medium: " << medium << "\n";
        oss << "  Low: " << low << "\n";
        
        if (critical > 0) {
            oss << "\n⚠️ CRITICAL: Immediate attention required!\n";
        }
        
        return oss.str();
    }

    // ========================================================================
    // Control Flow Graph Recovery
    // ========================================================================

    std::string AnalyzeCFG(uint64_t functionAddr) {
        auto blocks = m_codex.AnalyzeControlFlow(functionAddr);

        std::ostringstream oss;
        oss << "=== Control Flow Graph @ 0x" << std::hex << functionAddr << " ===\n";
        oss << "Basic Blocks: " << std::dec << blocks.size() << "\n\n";

        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto& bb = blocks[i];
            oss << "BB" << i << ": 0x" << std::hex << bb.startAddress
                << " - 0x" << bb.endAddress;
            if (bb.isReturn) oss << " [RET]";
            if (bb.isCall)   oss << " [CALL]";
            oss << "\n";

            if (!bb.successors.empty()) {
                oss << "  -> successors:";
                for (uint64_t s : bb.successors) {
                    oss << " 0x" << std::hex << s;
                }
                oss << "\n";
            }
            if (!bb.predecessors.empty()) {
                oss << "  <- predecessors:";
                for (uint64_t p : bb.predecessors) {
                    oss << " 0x" << std::hex << p;
                }
                oss << "\n";
            }
            oss << "\n";
        }

        // Compute graph metrics
        size_t edges = 0;
        for (const auto& bb : blocks) edges += bb.successors.size();
        oss << "--- Graph Metrics ---\n";
        oss << "Nodes (basic blocks): " << std::dec << blocks.size() << "\n";
        oss << "Edges: " << edges << "\n";
        if (blocks.size() > 0) {
            // Cyclomatic complexity = E - N + 2
            int cc = static_cast<int>(edges) - static_cast<int>(blocks.size()) + 2;
            if (cc < 1) cc = 1;
            oss << "Cyclomatic Complexity: " << cc << "\n";
        }

        return oss.str();
    }

    std::string ExportCFGDot(uint64_t functionAddr) {
        auto blocks = m_codex.AnalyzeControlFlow(functionAddr);

        std::ostringstream dot;
        dot << "digraph CFG_0x" << std::hex << functionAddr << " {\n";
        dot << "  graph [fontname=\"Courier New\" rankdir=TB];\n";
        dot << "  node [shape=box fontname=\"Courier New\" fontsize=10];\n\n";

        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto& bb = blocks[i];
            dot << "  bb_0x" << std::hex << bb.startAddress
                << " [label=\"BB" << std::dec << i << "\\n0x" << std::hex << bb.startAddress
                << " - 0x" << bb.endAddress << "\"";
            if (bb.isReturn) dot << " color=red";
            dot << "];\n";
        }
        dot << "\n";

        for (const auto& bb : blocks) {
            for (uint64_t succ : bb.successors) {
                dot << "  bb_0x" << std::hex << bb.startAddress
                    << " -> bb_0x" << succ << ";\n";
            }
        }

        dot << "}\n";
        return dot.str();
    }

    // ========================================================================
    // Function Boundary Recovery
    // ========================================================================

    std::string RecoverFunctions() {
        auto functions = m_codex.RecoverFunctions();

        std::ostringstream oss;
        oss << "=== Function Boundary Recovery ===\n";
        oss << "Recovered " << functions.size() << " functions\n\n";

        size_t thunks = 0, framed = 0, frameless = 0;
        for (const auto& fn : functions) {
            if (fn.isThunk) thunks++;
            else if (fn.hasFramePointer) framed++;
            else frameless++;
        }
        oss << "Frame-pointer functions: " << framed << "\n";
        oss << "Frameless functions: " << frameless << "\n";
        oss << "Import thunks: " << thunks << "\n\n";

        for (const auto& fn : functions) {
            oss << "0x" << std::hex << std::setw(12) << std::setfill('0') << fn.startAddress;
            if (fn.endAddress != 0) {
                oss << " - 0x" << std::setw(12) << fn.endAddress;
                oss << " (" << std::dec << (fn.endAddress - fn.startAddress) << " bytes)";
            }
            oss << "  " << fn.name;
            if (fn.isThunk) oss << " [THUNK]";
            if (fn.hasFramePointer) oss << " [FP]";
            oss << "\n";
        }

        return oss.str();
    }

    // ========================================================================
    // SSA Lifting (Static Single Assignment Form)
    // ========================================================================

    std::string LiftSSA(uint64_t functionAddr) {
        auto ssaResult = m_codex.LiftToSSA(functionAddr);

        std::ostringstream oss;
        oss << "=== SSA Lifting @ 0x" << std::hex << functionAddr << " ===\n";

        if (!ssaResult.success) {
            oss << "ERROR: SSA lifting failed. Ensure binary is loaded and CFG is built.\n";
            return oss.str();
        }

        oss << "SSA Variables: " << std::dec << ssaResult.totalVars << "\n";
        oss << "SSA Instructions: " << ssaResult.instructions.size() << "\n";
        oss << "PHI Nodes: " << ssaResult.phiNodes.size() << "\n\n";

        // SSA opcode name table
        static const char* ssaOpNames[] = {
            "assign", "add", "sub", "mul", "div", "and", "or", "xor",
            "shl", "shr", "sar", "not", "neg", "load", "store", "call",
            "ret", "cmp", "test", "branch", "jmp", "phi", "lea", "unknown"
        };

        // Print PHI nodes grouped by basic block
        if (!ssaResult.phiNodes.empty()) {
            oss << "--- PHI Nodes ---\n";
            uint32_t currentBB = UINT32_MAX;
            for (const auto& phi : ssaResult.phiNodes) {
                if (phi.bbIndex != currentBB) {
                    currentBB = phi.bbIndex;
                    oss << "  BB" << std::dec << currentBB << ":\n";
                }
                oss << "    v" << phi.dstVarId << " = phi(";
                for (size_t k = 0; k < phi.operandVarIds.size(); ++k) {
                    if (k > 0) oss << ", ";
                    oss << "v" << phi.operandVarIds[k] << ":BB" << phi.operandBBs[k];
                }
                oss << ")  [reg=" << phi.regIndex << "]\n";
            }
            oss << "\n";
        }

        // Print SSA instructions grouped by basic block
        oss << "--- SSA Instructions ---\n";
        uint32_t currentBB = UINT32_MAX;
        size_t deadCount = 0;

        for (const auto& si : ssaResult.instructions) {
            if (si.bbIndex != currentBB) {
                currentBB = si.bbIndex;
                oss << "  BB" << std::dec << currentBB << ":\n";
            }

            if (si.isDeadCode) {
                deadCount++;
                continue; // Skip dead code in output
            }

            uint32_t opIdx = static_cast<uint32_t>(si.op);
            const char* opName = (opIdx < 24) ? ssaOpNames[opIdx] : "unknown";

            oss << "    ";
            if (si.dstVarId >= 0) {
                oss << "v" << si.dstVarId << " = ";
            }

            oss << opName;

            if (si.src1VarId >= 0) {
                oss << " v" << si.src1VarId;
            }
            if (si.src2VarId >= 0) {
                oss << ", v" << si.src2VarId;
            }
            if (si.flagsVarId >= 0) {
                oss << " [flags=v" << si.flagsVarId << "]";
            }
            if (si.op == SSAOpType::Call && si.callTarget != 0) {
                oss << " -> 0x" << std::hex << si.callTarget;
            }
            if ((si.op == SSAOpType::Branch || si.op == SSAOpType::Jmp) && si.branchTarget != 0) {
                oss << " -> 0x" << std::hex << si.branchTarget;
            }

            oss << "    ; @0x" << std::hex << si.origAddress << std::dec << "\n";
        }

        oss << "\n--- SSA Statistics ---\n";
        oss << "Total SSA Variables: " << ssaResult.totalVars << "\n";
        oss << "Total SSA Instructions: " << ssaResult.instructions.size() << "\n";
        oss << "Dead Code Instructions: " << deadCount << "\n";
        oss << "PHI Nodes: " << ssaResult.phiNodes.size() << "\n";

        return oss.str();
    }

    // ========================================================================
    // Recursive Descent Disassembly (CFG-guided)
    // ========================================================================

    std::string RecursiveDisassemble(uint64_t entryPoint) {
        auto disasm = m_codex.RecursiveDisassemble(entryPoint);

        std::ostringstream oss;
        oss << "=== Recursive Descent Disassembly @ 0x" << std::hex << entryPoint << " ===\n";
        oss << "Instructions decoded: " << std::dec << disasm.size() << "\n";
        oss << "(CFG-guided: only reachable code is shown)\n\n";

        for (const auto& line : disasm) {
            oss << "  0x" << std::hex << std::setw(12) << std::setfill('0') << line.address << ": ";

            // Print raw bytes (up to 8)
            for (size_t j = 0; j < line.bytes.size() && j < 8; ++j) {
                oss << std::hex << std::setw(2) << std::setfill('0') << (int)line.bytes[j] << " ";
            }
            // Pad to align mnemonics
            for (size_t j = line.bytes.size(); j < 8; ++j) {
                oss << "   ";
            }

            oss << std::setfill(' ') << std::left << std::setw(8) << line.mnemonic;
            if (!line.operands.empty()) {
                oss << " " << line.operands;
            }
            if (!line.comment.empty()) {
                oss << "  ; " << line.comment;
            }
            oss << "\n";
        }

        if (disasm.empty()) {
            oss << "  (no reachable code found from entry point)\n";
        }

        return oss.str();
    }

    // ========================================================================
    // Symbol Demangling
    // ========================================================================

    std::string DemangleAll() {
        m_codex.DemangleAllSymbols();

        auto symbols = m_codex.GetSymbols();
        std::ostringstream oss;
        oss << "=== Symbol Demangling ===\n";
        oss << "Total symbols: " << symbols.size() << "\n\n";

        size_t demangled = 0;
        for (const auto& sym : symbols) {
            if (sym.isMangled && sym.demangledName != sym.name) {
                oss << "  " << sym.name << "\n";
                oss << "    -> " << sym.demangledName << "\n";
                demangled++;
            }
        }

        if (demangled == 0) {
            oss << "No mangled symbols found (or all names are already demangled).\n";
        } else {
            oss << "\nDemangled " << demangled << " symbols.\n";
        }

        return oss.str();
    }

    std::string DemangleSingle(const std::string& symbol) {
        return m_codex.DemangleSymbol(symbol);
    }

    // ========================================================================
    // Hex Dump
    // ========================================================================

    std::string HexDump(uint64_t offset, size_t length) {
        std::ostringstream oss;
        oss << "=== Hex Dump @ 0x" << std::hex << offset << " ===\n\n";
        
        auto sections = m_codex.GetSections();
        const Section* targetSection = nullptr;
        
        for (const auto& section : sections) {
            if (offset >= section.virtualAddress && 
                offset < section.virtualAddress + section.virtualSize) {
                targetSection = &section;
                break;
            }
        }
        
        if (!targetSection) {
            oss << "Error: Address not in any section\n";
            return oss.str();
        }
        
        size_t sectionOffset = offset - targetSection->virtualAddress;
        size_t available = std::min(length, targetSection->data.size() - sectionOffset);
        
        for (size_t i = 0; i < available; i += 16) {
            // Address
            oss << std::hex << std::setw(8) << std::setfill('0') 
                << (offset + i) << "  ";
            
            // Hex bytes
            for (size_t j = 0; j < 16; ++j) {
                if (i + j < available) {
                    oss << std::hex << std::setw(2) << std::setfill('0') 
                        << (int)targetSection->data[sectionOffset + i + j] << " ";
                } else {
                    oss << "   ";
                }
                if (j == 7) oss << " ";
            }
            
            oss << " |";
            
            // ASCII
            for (size_t j = 0; j < 16 && i + j < available; ++j) {
                uint8_t b = targetSection->data[sectionOffset + i + j];
                oss << (b >= 0x20 && b < 0x7F ? (char)b : '.');
            }
            
            oss << "|\n";
        }
        
        return oss.str();
    }

    // ========================================================================
    // Getters for Direct Access
    // ========================================================================

    RawrCodex& GetCodex() { return m_codex; }
    RawrDumpBin& GetDumpBin() { return m_dumpbin; }
    RawrCompiler& GetCompiler() { return m_compiler; }
    
    std::string GetCurrentPath() const { return m_currentPath; }

private:
    RawrCodex m_codex;
    RawrDumpBin m_dumpbin;
    RawrCompiler m_compiler;
    std::string m_currentPath;
    
    void AnalyzeStructure() {
        // Additional analysis after loading
    }
    
    uint64_t ParseCallTarget(const std::string& operands) {
        // Simple hex parser for call targets
        if (operands.find("0x") != std::string::npos) {
            try {
                return std::stoull(operands.substr(operands.find("0x")), nullptr, 16);
            } catch (...) {}
        }
        return 0;
    }
    
    bool MatchPattern(const std::vector<uint8_t>& data, 
                      const std::vector<uint8_t>& pattern,
                      const std::vector<uint8_t>& mask) {
        if (data.size() < pattern.size()) return false;
        
        for (size_t i = 0; i < pattern.size(); ++i) {
            if (mask[i] == 0xFF && data[i] != pattern[i]) {
                return false;
            }
        }
        return true;
    }
};

} // namespace ReverseEngineering
} // namespace RawrXD
