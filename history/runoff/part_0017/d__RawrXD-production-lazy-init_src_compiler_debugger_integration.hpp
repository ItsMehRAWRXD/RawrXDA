#pragma once

// ============================================================================
// RAWRXD DEBUGGER INTEGRATION
// Complete debugger with breakpoints, stepping, and variable inspection
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <sstream>
#include <fstream>
#include <cstdint>

namespace RawrXD {
namespace Compiler {
namespace Debug {

// ============================================================================
// DEBUG TYPES
// ============================================================================

/**
 * @brief Source location information
 */
struct SourceLocation {
    std::string file;
    int line = 0;
    int column = 0;
    
    bool operator==(const SourceLocation& other) const {
        return file == other.file && line == other.line;
    }
    
    bool operator<(const SourceLocation& other) const {
        if (file != other.file) return file < other.file;
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
    
    std::string toString() const {
        std::ostringstream oss;
        oss << file << ":" << line;
        if (column > 0) oss << ":" << column;
        return oss.str();
    }
};

/**
 * @brief Breakpoint definition
 */
struct Breakpoint {
    uint32_t id = 0;
    SourceLocation location;
    bool enabled = true;
    std::string condition;  // Conditional expression
    int hitCount = 0;
    int hitTarget = 0;      // Break when hitCount reaches this (0 = always)
    std::string logMessage; // Log instead of break (tracepoint)
    
    enum class Type {
        Line,           // Break on line
        Function,       // Break on function entry
        Data,           // Break on data change (watchpoint)
        Exception       // Break on exception
    };
    Type type = Type::Line;
    
    // For function breakpoints
    std::string functionName;
    
    // For data breakpoints
    std::string watchExpression;
    enum class AccessType { Read, Write, ReadWrite } accessType = AccessType::Write;
};

/**
 * @brief Stack frame information
 */
struct StackFrame {
    uint32_t id = 0;
    std::string name;       // Function name
    SourceLocation location;
    uint64_t instructionPointer = 0;
    uint64_t framePointer = 0;
    uint64_t stackPointer = 0;
    
    // Module/file info
    std::string moduleName;
    std::string moduleId;
};

/**
 * @brief Variable scope
 */
enum class VariableScope {
    Local,
    Arguments,
    Global,
    Register,
    Static,
    Environment
};

/**
 * @brief Debug variable representation
 */
struct DebugVariable {
    std::string name;
    std::string type;
    std::string value;
    VariableScope scope = VariableScope::Local;
    uint64_t address = 0;
    bool evaluateOnHover = true;
    
    // For structured types
    std::vector<DebugVariable> children;
    int childCount = 0;
    bool hasChildren = false;
    
    // Reference for lazy loading
    uint32_t variablesReference = 0;
    
    // Memory info
    size_t size = 0;
    std::string memoryReference;
};

/**
 * @brief Expression evaluation result
 */
struct EvaluationResult {
    bool success = false;
    std::string value;
    std::string type;
    std::vector<DebugVariable> children;
    int childCount = 0;
    bool hasChildren = false;
    uint32_t variablesReference = 0;
    std::string error;
};

/**
 * @brief Thread state
 */
struct ThreadState {
    uint32_t id = 0;
    std::string name;
    
    enum class Status {
        Running,
        Stopped,
        Suspended,
        Terminated
    };
    Status status = Status::Running;
    
    std::string stopReason;
    SourceLocation stopLocation;
    std::vector<StackFrame> callStack;
};

/**
 * @brief Debug event types
 */
enum class DebugEventType {
    Initialized,
    Stopped,
    Continued,
    Exited,
    Terminated,
    Thread,
    Output,
    Breakpoint,
    Module,
    LoadedSource,
    Process,
    Capabilities,
    Memory
};

struct DebugEvent {
    DebugEventType type;
    std::string reason;
    std::string description;
    
    // Event-specific data
    uint32_t threadId = 0;
    int exitCode = 0;
    std::string output;
    std::string category; // "console", "stdout", "stderr", etc.
    Breakpoint breakpoint;
    SourceLocation location;
};

// ============================================================================
// DEBUG ADAPTER PROTOCOL (DAP) TYPES
// ============================================================================

/**
 * @brief DAP request/response message
 */
struct DAPMessage {
    int seq = 0;
    std::string type;  // "request", "response", "event"
    std::string command;
    
    // For responses
    bool success = true;
    std::string message;
    
    // Generic data container
    std::map<std::string, std::variant<
        std::nullptr_t,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<std::string>
    >> body;
};

// ============================================================================
// SYMBOL TABLE
// ============================================================================

/**
 * @brief Debug symbol information
 */
struct DebugSymbol {
    std::string name;
    std::string mangledName;
    std::string type;
    SourceLocation definition;
    uint64_t address = 0;
    size_t size = 0;
    
    enum class Kind {
        Function,
        Variable,
        Parameter,
        Type,
        Namespace,
        Class,
        Struct,
        Enum,
        EnumMember,
        Constant
    };
    Kind kind = Kind::Variable;
    
    // For functions
    std::vector<std::pair<std::string, std::string>> parameters; // name, type
    std::string returnType;
    
    // For types
    std::vector<DebugSymbol> members;
};

/**
 * @brief Symbol table manager
 */
class SymbolTable {
public:
    void addSymbol(const DebugSymbol& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        symbolsByName_[symbol.name].push_back(symbol);
        if (symbol.address != 0) {
            symbolsByAddress_[symbol.address] = symbol;
        }
    }
    
    std::vector<DebugSymbol> findByName(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = symbolsByName_.find(name);
        if (it != symbolsByName_.end()) {
            return it->second;
        }
        return {};
    }
    
    std::optional<DebugSymbol> findByAddress(uint64_t address) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = symbolsByAddress_.find(address);
        if (it != symbolsByAddress_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<DebugSymbol> findByPattern(const std::string& pattern) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DebugSymbol> results;
        for (const auto& [name, symbols] : symbolsByName_) {
            if (matchPattern(name, pattern)) {
                results.insert(results.end(), symbols.begin(), symbols.end());
            }
        }
        return results;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        symbolsByName_.clear();
        symbolsByAddress_.clear();
    }
    
private:
    static bool matchPattern(const std::string& str, const std::string& pattern) {
        // Simple wildcard matching (* and ?)
        size_t si = 0, pi = 0;
        while (si < str.size() && pi < pattern.size()) {
            if (pattern[pi] == '*') {
                pi++;
                if (pi >= pattern.size()) return true;
                while (si < str.size() && str[si] != pattern[pi]) si++;
            } else if (pattern[pi] == '?' || pattern[pi] == str[si]) {
                si++;
                pi++;
            } else {
                return false;
            }
        }
        return pi >= pattern.size();
    }
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<DebugSymbol>> symbolsByName_;
    std::map<uint64_t, DebugSymbol> symbolsByAddress_;
};

// ============================================================================
// LINE NUMBER TABLE
// ============================================================================

/**
 * @brief Maps addresses to source lines
 */
class LineNumberTable {
public:
    struct Entry {
        uint64_t address;
        SourceLocation location;
        bool isStatement = true;
        bool isBlockEntry = false;
        bool isBlockExit = false;
    };
    
    void addEntry(const Entry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        addressToLine_[entry.address] = entry;
        lineToAddresses_[entry.location].push_back(entry.address);
    }
    
    std::optional<Entry> findByAddress(uint64_t address) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = addressToLine_.find(address);
        if (it != addressToLine_.end()) {
            return it->second;
        }
        // Find nearest lower address
        auto lower = addressToLine_.lower_bound(address);
        if (lower != addressToLine_.begin()) {
            --lower;
            return lower->second;
        }
        return std::nullopt;
    }
    
    std::vector<uint64_t> findByLocation(const SourceLocation& loc) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lineToAddresses_.find(loc);
        if (it != lineToAddresses_.end()) {
            return it->second;
        }
        return {};
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        addressToLine_.clear();
        lineToAddresses_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    std::map<uint64_t, Entry> addressToLine_;
    std::map<SourceLocation, std::vector<uint64_t>> lineToAddresses_;
};

// ============================================================================
// EXPRESSION EVALUATOR
// ============================================================================

/**
 * @brief Evaluates debug expressions
 */
class ExpressionEvaluator {
public:
    using VariableLookup = std::function<std::optional<DebugVariable>(const std::string&)>;
    using MemoryRead = std::function<std::vector<uint8_t>(uint64_t, size_t)>;
    
    void setVariableLookup(VariableLookup lookup) { variableLookup_ = std::move(lookup); }
    void setMemoryRead(MemoryRead reader) { memoryRead_ = std::move(reader); }
    
    EvaluationResult evaluate(const std::string& expression, const StackFrame& context) {
        EvaluationResult result;
        
        try {
            // Parse expression
            auto tokens = tokenize(expression);
            if (tokens.empty()) {
                result.error = "Empty expression";
                return result;
            }
            
            // Simple evaluation
            if (tokens.size() == 1) {
                // Single identifier or literal
                return evaluateSingleToken(tokens[0]);
            }
            
            // Binary expression
            if (tokens.size() == 3) {
                return evaluateBinaryOp(tokens[0], tokens[1], tokens[2]);
            }
            
            // Member access
            for (size_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] == "." && i + 1 < tokens.size()) {
                    return evaluateMemberAccess(expression);
                }
            }
            
            // Array access
            if (expression.find('[') != std::string::npos) {
                return evaluateArrayAccess(expression);
            }
            
            result.error = "Unsupported expression: " + expression;
            
        } catch (const std::exception& e) {
            result.error = e.what();
        }
        
        return result;
    }
    
    // Format value for display
    std::string formatValue(const DebugVariable& var) {
        if (var.type == "int" || var.type == "int32_t") {
            return var.value;
        }
        if (var.type == "float" || var.type == "double") {
            return var.value;
        }
        if (var.type == "bool") {
            return var.value;
        }
        if (var.type.find("char*") != std::string::npos || 
            var.type.find("string") != std::string::npos) {
            return "\"" + var.value + "\"";
        }
        if (var.type.find("*") != std::string::npos) {
            return "0x" + var.value;
        }
        if (var.hasChildren) {
            std::ostringstream oss;
            oss << var.type << " {";
            for (size_t i = 0; i < var.children.size() && i < 3; ++i) {
                if (i > 0) oss << ", ";
                oss << var.children[i].name << "=" << var.children[i].value;
            }
            if (var.children.size() > 3) {
                oss << ", ...";
            }
            oss << "}";
            return oss.str();
        }
        return var.value;
    }
    
private:
    std::vector<std::string> tokenize(const std::string& expr) {
        std::vector<std::string> tokens;
        std::string current;
        
        for (size_t i = 0; i < expr.size(); ++i) {
            char c = expr[i];
            
            if (std::isspace(c)) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else if (c == '.' || c == '[' || c == ']' || c == '(' || c == ')' ||
                       c == '+' || c == '-' || c == '*' || c == '/' ||
                       c == '<' || c == '>' || c == '=' || c == '!' ||
                       c == '&' || c == '|') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                std::string op(1, c);
                // Handle two-char operators
                if (i + 1 < expr.size()) {
                    char next = expr[i + 1];
                    if ((c == '=' && next == '=') || (c == '!' && next == '=') ||
                        (c == '<' && next == '=') || (c == '>' && next == '=') ||
                        (c == '&' && next == '&') || (c == '|' && next == '|') ||
                        (c == '-' && next == '>')) {
                        op += next;
                        i++;
                    }
                }
                tokens.push_back(op);
            } else {
                current += c;
            }
        }
        
        if (!current.empty()) {
            tokens.push_back(current);
        }
        
        return tokens;
    }
    
    EvaluationResult evaluateSingleToken(const std::string& token) {
        EvaluationResult result;
        
        // Check if it's a number
        if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1)) {
            result.success = true;
            result.value = token;
            if (token.find('.') != std::string::npos) {
                result.type = "double";
            } else {
                result.type = "int";
            }
            return result;
        }
        
        // Check if it's a string literal
        if (token[0] == '"' && token.back() == '"') {
            result.success = true;
            result.value = token.substr(1, token.size() - 2);
            result.type = "string";
            return result;
        }
        
        // Check if it's a boolean
        if (token == "true" || token == "false") {
            result.success = true;
            result.value = token;
            result.type = "bool";
            return result;
        }
        
        // Try variable lookup
        if (variableLookup_) {
            auto var = variableLookup_(token);
            if (var.has_value()) {
                result.success = true;
                result.value = var->value;
                result.type = var->type;
                result.children = var->children;
                result.childCount = var->childCount;
                result.hasChildren = var->hasChildren;
                result.variablesReference = var->variablesReference;
                return result;
            }
        }
        
        result.error = "Unknown identifier: " + token;
        return result;
    }
    
    EvaluationResult evaluateBinaryOp(const std::string& left, const std::string& op,
                                       const std::string& right) {
        EvaluationResult result;
        
        auto leftResult = evaluateSingleToken(left);
        auto rightResult = evaluateSingleToken(right);
        
        if (!leftResult.success || !rightResult.success) {
            result.error = leftResult.success ? rightResult.error : leftResult.error;
            return result;
        }
        
        // Numeric operations
        if ((leftResult.type == "int" || leftResult.type == "double") &&
            (rightResult.type == "int" || rightResult.type == "double")) {
            
            double lval = std::stod(leftResult.value);
            double rval = std::stod(rightResult.value);
            double res = 0;
            bool boolResult = false;
            bool isBoolOp = false;
            
            if (op == "+") res = lval + rval;
            else if (op == "-") res = lval - rval;
            else if (op == "*") res = lval * rval;
            else if (op == "/") res = rval != 0 ? lval / rval : 0;
            else if (op == "%") res = static_cast<int>(lval) % static_cast<int>(rval);
            else if (op == "==") { boolResult = lval == rval; isBoolOp = true; }
            else if (op == "!=") { boolResult = lval != rval; isBoolOp = true; }
            else if (op == "<") { boolResult = lval < rval; isBoolOp = true; }
            else if (op == ">") { boolResult = lval > rval; isBoolOp = true; }
            else if (op == "<=") { boolResult = lval <= rval; isBoolOp = true; }
            else if (op == ">=") { boolResult = lval >= rval; isBoolOp = true; }
            else {
                result.error = "Unknown operator: " + op;
                return result;
            }
            
            result.success = true;
            if (isBoolOp) {
                result.value = boolResult ? "true" : "false";
                result.type = "bool";
            } else {
                result.value = std::to_string(res);
                result.type = (leftResult.type == "double" || rightResult.type == "double") 
                    ? "double" : "int";
            }
        } else {
            result.error = "Type mismatch in binary operation";
        }
        
        return result;
    }
    
    EvaluationResult evaluateMemberAccess(const std::string& expr) {
        EvaluationResult result;
        
        // Split by . or ->
        std::vector<std::string> parts;
        std::string current;
        for (size_t i = 0; i < expr.size(); ++i) {
            if (expr[i] == '.') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else if (expr[i] == '-' && i + 1 < expr.size() && expr[i + 1] == '>') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
                i++; // Skip >
            } else {
                current += expr[i];
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        
        if (parts.empty()) {
            result.error = "Invalid member access expression";
            return result;
        }
        
        // Start with first variable
        auto varResult = evaluateSingleToken(parts[0]);
        if (!varResult.success) {
            return varResult;
        }
        
        // Navigate through members
        for (size_t i = 1; i < parts.size(); ++i) {
            bool found = false;
            for (const auto& child : varResult.children) {
                if (child.name == parts[i]) {
                    varResult.success = true;
                    varResult.value = child.value;
                    varResult.type = child.type;
                    varResult.children = child.children;
                    varResult.hasChildren = child.hasChildren;
                    found = true;
                    break;
                }
            }
            if (!found) {
                result.error = "Member not found: " + parts[i];
                return result;
            }
        }
        
        return varResult;
    }
    
    EvaluationResult evaluateArrayAccess(const std::string& expr) {
        EvaluationResult result;
        
        size_t bracketPos = expr.find('[');
        size_t closeBracket = expr.find(']');
        
        if (bracketPos == std::string::npos || closeBracket == std::string::npos) {
            result.error = "Invalid array access";
            return result;
        }
        
        std::string arrayName = expr.substr(0, bracketPos);
        std::string indexExpr = expr.substr(bracketPos + 1, closeBracket - bracketPos - 1);
        
        // Evaluate index
        auto indexResult = evaluateSingleToken(indexExpr);
        if (!indexResult.success) {
            return indexResult;
        }
        
        int index = std::stoi(indexResult.value);
        
        // Get array variable
        auto arrayResult = evaluateSingleToken(arrayName);
        if (!arrayResult.success) {
            return arrayResult;
        }
        
        // Access element
        if (index >= 0 && index < static_cast<int>(arrayResult.children.size())) {
            const auto& element = arrayResult.children[index];
            result.success = true;
            result.value = element.value;
            result.type = element.type;
            result.children = element.children;
            result.hasChildren = element.hasChildren;
        } else {
            result.error = "Array index out of bounds: " + std::to_string(index);
        }
        
        return result;
    }
    
    VariableLookup variableLookup_;
    MemoryRead memoryRead_;
};

// ============================================================================
// BREAKPOINT MANAGER
// ============================================================================

/**
 * @brief Manages breakpoints
 */
class BreakpointManager {
public:
    using BreakpointCallback = std::function<void(const Breakpoint&)>;
    
    uint32_t addBreakpoint(const Breakpoint& bp) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        Breakpoint newBp = bp;
        newBp.id = nextId_++;
        breakpoints_[newBp.id] = newBp;
        
        // Index by location
        breakpointsByLocation_[newBp.location].push_back(newBp.id);
        
        // Index by function
        if (!newBp.functionName.empty()) {
            breakpointsByFunction_[newBp.functionName].push_back(newBp.id);
        }
        
        return newBp.id;
    }
    
    bool removeBreakpoint(uint32_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = breakpoints_.find(id);
        if (it == breakpoints_.end()) {
            return false;
        }
        
        // Remove from location index
        auto& locBps = breakpointsByLocation_[it->second.location];
        locBps.erase(std::remove(locBps.begin(), locBps.end(), id), locBps.end());
        
        // Remove from function index
        if (!it->second.functionName.empty()) {
            auto& funcBps = breakpointsByFunction_[it->second.functionName];
            funcBps.erase(std::remove(funcBps.begin(), funcBps.end(), id), funcBps.end());
        }
        
        breakpoints_.erase(it);
        return true;
    }
    
    std::optional<Breakpoint> getBreakpoint(uint32_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = breakpoints_.find(id);
        if (it != breakpoints_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<Breakpoint> getBreakpointsAtLocation(const SourceLocation& loc) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Breakpoint> result;
        
        auto it = breakpointsByLocation_.find(loc);
        if (it != breakpointsByLocation_.end()) {
            for (auto id : it->second) {
                auto bpIt = breakpoints_.find(id);
                if (bpIt != breakpoints_.end() && bpIt->second.enabled) {
                    result.push_back(bpIt->second);
                }
            }
        }
        
        return result;
    }
    
    std::vector<Breakpoint> getAllBreakpoints() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Breakpoint> result;
        for (const auto& [id, bp] : breakpoints_) {
            result.push_back(bp);
        }
        return result;
    }
    
    void setEnabled(uint32_t id, bool enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = breakpoints_.find(id);
        if (it != breakpoints_.end()) {
            it->second.enabled = enabled;
        }
    }
    
    void setCondition(uint32_t id, const std::string& condition) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = breakpoints_.find(id);
        if (it != breakpoints_.end()) {
            it->second.condition = condition;
        }
    }
    
    bool shouldBreak(uint32_t id, ExpressionEvaluator& evaluator, const StackFrame& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = breakpoints_.find(id);
        if (it == breakpoints_.end() || !it->second.enabled) {
            return false;
        }
        
        auto& bp = it->second;
        bp.hitCount++;
        
        // Check hit target
        if (bp.hitTarget > 0 && bp.hitCount < bp.hitTarget) {
            return false;
        }
        
        // Check condition
        if (!bp.condition.empty()) {
            auto result = evaluator.evaluate(bp.condition, frame);
            if (!result.success || result.value != "true") {
                return false;
            }
        }
        
        // If it's a logpoint, log and don't break
        if (!bp.logMessage.empty()) {
            // TODO: Emit log event
            return false;
        }
        
        return true;
    }
    
    void clearAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        breakpoints_.clear();
        breakpointsByLocation_.clear();
        breakpointsByFunction_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    uint32_t nextId_ = 1;
    std::map<uint32_t, Breakpoint> breakpoints_;
    std::map<SourceLocation, std::vector<uint32_t>> breakpointsByLocation_;
    std::unordered_map<std::string, std::vector<uint32_t>> breakpointsByFunction_;
};

// ============================================================================
// DEBUGGER CORE
// ============================================================================

/**
 * @brief Core debugger implementation
 */
class Debugger {
public:
    using EventCallback = std::function<void(const DebugEvent&)>;
    
    enum class State {
        Idle,
        Running,
        Stopped,
        Terminated
    };
    
    Debugger()
        : state_(State::Idle)
        , stepping_(false)
        , stepType_(StepType::None)
    {
    }
    
    // Configuration
    void setEventCallback(EventCallback callback) { eventCallback_ = std::move(callback); }
    
    // Session management
    bool initialize(const std::string& program, const std::vector<std::string>& args = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        program_ = program;
        arguments_ = args;
        state_ = State::Idle;
        
        // Load debug info
        loadDebugInfo(program);
        
        DebugEvent event;
        event.type = DebugEventType::Initialized;
        emitEvent(event);
        
        return true;
    }
    
    bool launch() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        state_ = State::Running;
        
        // Create main thread
        ThreadState mainThread;
        mainThread.id = 1;
        mainThread.name = "main";
        mainThread.status = ThreadState::Status::Running;
        threads_[mainThread.id] = mainThread;
        
        return true;
    }
    
    void terminate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        state_ = State::Terminated;
        
        DebugEvent event;
        event.type = DebugEventType::Terminated;
        emitEvent(event);
    }
    
    // Execution control
    void continue_() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != State::Stopped) return;
        
        state_ = State::Running;
        stepping_ = false;
        
        DebugEvent event;
        event.type = DebugEventType::Continued;
        event.threadId = currentThread_;
        emitEvent(event);
        
        resumeExecution();
    }
    
    void pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != State::Running) return;
        
        stopExecution("pause");
    }
    
    enum class StepType { None, In, Over, Out };
    
    void stepIn() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != State::Stopped) return;
        
        state_ = State::Running;
        stepping_ = true;
        stepType_ = StepType::In;
        stepStartFrame_ = getCurrentCallStack().size();
        
        resumeExecution();
    }
    
    void stepOver() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != State::Stopped) return;
        
        state_ = State::Running;
        stepping_ = true;
        stepType_ = StepType::Over;
        stepStartFrame_ = getCurrentCallStack().size();
        stepStartLine_ = getCurrentLocation().line;
        
        resumeExecution();
    }
    
    void stepOut() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != State::Stopped) return;
        
        state_ = State::Running;
        stepping_ = true;
        stepType_ = StepType::Out;
        stepStartFrame_ = getCurrentCallStack().size();
        
        resumeExecution();
    }
    
    // Called by execution engine on each statement
    void onStatement(uint64_t address) {
        auto lineEntry = lineTable_.findByAddress(address);
        if (!lineEntry.has_value()) return;
        
        SourceLocation loc = lineEntry->location;
        
        // Check breakpoints
        auto bps = breakpointManager_.getBreakpointsAtLocation(loc);
        for (auto& bp : bps) {
            auto frame = getCurrentFrame();
            if (breakpointManager_.shouldBreak(bp.id, expressionEvaluator_, frame)) {
                stopExecution("breakpoint");
                return;
            }
        }
        
        // Check stepping
        if (stepping_) {
            bool shouldStop = false;
            size_t currentDepth = getCurrentCallStack().size();
            
            switch (stepType_) {
                case StepType::In:
                    shouldStop = true;
                    break;
                    
                case StepType::Over:
                    if (currentDepth <= stepStartFrame_ && loc.line != stepStartLine_) {
                        shouldStop = true;
                    }
                    break;
                    
                case StepType::Out:
                    if (currentDepth < stepStartFrame_) {
                        shouldStop = true;
                    }
                    break;
                    
                default:
                    break;
            }
            
            if (shouldStop) {
                stopExecution("step");
            }
        }
    }
    
    // State queries
    State getState() const { return state_; }
    
    std::vector<ThreadState> getThreads() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ThreadState> result;
        for (const auto& [id, thread] : threads_) {
            result.push_back(thread);
        }
        return result;
    }
    
    std::vector<StackFrame> getCallStack(uint32_t threadId = 0) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (threadId == 0) threadId = currentThread_;
        
        auto it = threads_.find(threadId);
        if (it != threads_.end()) {
            return it->second.callStack;
        }
        return {};
    }
    
    std::vector<DebugVariable> getVariables(uint32_t frameId, VariableScope scope) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find frame
        for (const auto& [tid, thread] : threads_) {
            for (const auto& frame : thread.callStack) {
                if (frame.id == frameId) {
                    return getVariablesForFrame(frame, scope);
                }
            }
        }
        
        return {};
    }
    
    EvaluationResult evaluate(const std::string& expression, uint32_t frameId = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        StackFrame frame;
        if (frameId == 0) {
            frame = getCurrentFrame();
        } else {
            for (const auto& [tid, thread] : threads_) {
                for (const auto& f : thread.callStack) {
                    if (f.id == frameId) {
                        frame = f;
                        break;
                    }
                }
            }
        }
        
        return expressionEvaluator_.evaluate(expression, frame);
    }
    
    // Breakpoint management
    BreakpointManager& getBreakpointManager() { return breakpointManager_; }
    const BreakpointManager& getBreakpointManager() const { return breakpointManager_; }
    
    // Symbol access
    SymbolTable& getSymbolTable() { return symbolTable_; }
    const SymbolTable& getSymbolTable() const { return symbolTable_; }
    
    // Line table access
    LineNumberTable& getLineTable() { return lineTable_; }
    const LineNumberTable& getLineTable() const { return lineTable_; }
    
private:
    void loadDebugInfo(const std::string& program) {
        // In real implementation, would load DWARF, PDB, or other debug info
        // For now, just set up evaluator
        expressionEvaluator_.setVariableLookup([this](const std::string& name) 
            -> std::optional<DebugVariable> {
            auto symbols = symbolTable_.findByName(name);
            if (!symbols.empty()) {
                DebugVariable var;
                var.name = symbols[0].name;
                var.type = symbols[0].type;
                var.address = symbols[0].address;
                // Would read actual value from target
                var.value = "?";
                return var;
            }
            return std::nullopt;
        });
    }
    
    void stopExecution(const std::string& reason) {
        state_ = State::Stopped;
        stepping_ = false;
        
        // Update thread state
        auto it = threads_.find(currentThread_);
        if (it != threads_.end()) {
            it->second.status = ThreadState::Status::Stopped;
            it->second.stopReason = reason;
            it->second.stopLocation = getCurrentLocation();
        }
        
        DebugEvent event;
        event.type = DebugEventType::Stopped;
        event.reason = reason;
        event.threadId = currentThread_;
        event.location = getCurrentLocation();
        emitEvent(event);
    }
    
    void resumeExecution() {
        // Update thread state
        auto it = threads_.find(currentThread_);
        if (it != threads_.end()) {
            it->second.status = ThreadState::Status::Running;
        }
        
        // In real implementation, would resume target process
    }
    
    SourceLocation getCurrentLocation() const {
        auto stack = getCurrentCallStack();
        if (!stack.empty()) {
            return stack[0].location;
        }
        return {};
    }
    
    StackFrame getCurrentFrame() const {
        auto stack = getCurrentCallStack();
        if (!stack.empty()) {
            return stack[0];
        }
        return {};
    }
    
    std::vector<StackFrame> getCurrentCallStack() const {
        auto it = threads_.find(currentThread_);
        if (it != threads_.end()) {
            return it->second.callStack;
        }
        return {};
    }
    
    std::vector<DebugVariable> getVariablesForFrame(const StackFrame& frame, VariableScope scope) const {
        std::vector<DebugVariable> result;
        
        // In real implementation, would read variables from debug info and target memory
        // For now, return symbols matching the scope
        auto symbols = symbolTable_.findByPattern("*");
        for (const auto& sym : symbols) {
            if (sym.kind == DebugSymbol::Kind::Variable ||
                sym.kind == DebugSymbol::Kind::Parameter) {
                DebugVariable var;
                var.name = sym.name;
                var.type = sym.type;
                var.address = sym.address;
                var.scope = (sym.kind == DebugSymbol::Kind::Parameter) 
                    ? VariableScope::Arguments : VariableScope::Local;
                result.push_back(var);
            }
        }
        
        return result;
    }
    
    void emitEvent(const DebugEvent& event) {
        if (eventCallback_) {
            eventCallback_(event);
        }
    }
    
    mutable std::mutex mutex_;
    
    std::string program_;
    std::vector<std::string> arguments_;
    
    std::atomic<State> state_;
    std::map<uint32_t, ThreadState> threads_;
    uint32_t currentThread_ = 1;
    
    bool stepping_;
    StepType stepType_;
    size_t stepStartFrame_;
    int stepStartLine_;
    
    BreakpointManager breakpointManager_;
    SymbolTable symbolTable_;
    LineNumberTable lineTable_;
    ExpressionEvaluator expressionEvaluator_;
    
    EventCallback eventCallback_;
};

} // namespace Debug
} // namespace Compiler
} // namespace RawrXD
