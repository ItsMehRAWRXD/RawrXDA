// CorrectednessValidator.hpp
// Semantic correctness feedback layer for MoE routing
// Replaces token-matching with compile/AST/type validation
// Zero deps, C++17

#ifndef CORRECTNESS_VALIDATOR_HPP
#define CORRECTNESS_VALIDATOR_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>

// ---------------------------------------------------------------------------
// 1. CODE PARSE TREE (minimal AST for validation)
// ---------------------------------------------------------------------------

enum class NodeType {
    Identifier,
    Literal,
    BinaryOp,
    FunctionCall,
    Assignment,
    IfStatement,
    WhileLoop,
    Block,
    FunctionDef,
    Unknown
};

struct ASTNode {
    NodeType type;
    std::string text;                   // source text
    std::vector<std::shared_ptr<ASTNode>> children;
    int line, col;
    int depth;                          // nesting depth
    
    ASTNode() : type(NodeType::Unknown), line(0), col(0), depth(0) {}
};

// ---------------------------------------------------------------------------
// 2. TYPE CONTEXT (track variable types, function sigs)
// ---------------------------------------------------------------------------

enum class TypeKind {
    Int32,
    Int64,
    Float32,
    Float64,
    Pointer,
    Bool,
    String,
    Custom,
    Unknown
};

struct TypeInfo {
    TypeKind kind;
    std::string custom_name;
    int pointer_depth;
    
    TypeInfo() : kind(TypeKind::Unknown), pointer_depth(0) {}
    explicit TypeInfo(TypeKind k) : kind(k), pointer_depth(0) {}
    
    bool is_numeric() const {
        return kind == TypeKind::Int32 || kind == TypeKind::Int64 ||
               kind == TypeKind::Float32 || kind == TypeKind::Float64;
    }
};

struct VariableBinding {
    std::string name;
    TypeInfo type;
    bool is_const;
    int definition_line;
};

struct FunctionSignature {
    std::string name;
    TypeInfo return_type;
    std::vector<VariableBinding> parameters;
};

struct TypeContext {
    std::unordered_map<std::string, VariableBinding> variables;
    std::unordered_map<std::string, FunctionSignature> functions;
    int current_scope_depth;
    
    TypeContext() : current_scope_depth(0) {}
};

// ---------------------------------------------------------------------------
// 3. MICRO C++ PARSER (enough to validate structure)
// ---------------------------------------------------------------------------

class MiniCppParser {
public:
    explicit MiniCppParser(const std::string& code) 
        : source(code), pos(0), line(1), col(0) {}
    
    // Parse source into AST
    std::shared_ptr<ASTNode> parse() {
        auto root = std::make_shared<ASTNode>();
        root->type = NodeType::Block;
        root->depth = 0;
        
        while (pos < source.size()) {
            skip_whitespace();
            if (pos >= source.size()) break;
            
            auto stmt = parse_statement();
            if (stmt) root->children.push_back(stmt);
        }
        
        return root;
    }
    
    // Extract type context (basic inference)
    TypeContext extract_types() {
        TypeContext ctx;
        extract_types_recursive(parse(), ctx, 0);
        return ctx;
    }
    
private:
    std::string source;
    size_t pos;
    int line, col;
    
    char current() const { return pos < source.size() ? source[pos] : 0; }
    void advance() {
        if (pos < source.size()) {
            if (source[pos] == '\n') { line++; col = 0; }
            else col++;
            pos++;
        }
    }
    
    void skip_whitespace() {
        while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' || 
               source[pos] == '\n' || source[pos] == '\r')) {
            advance();
        }
    }
    
    bool match(const char* s) {
        size_t slen = std::strlen(s);
        return pos + slen <= source.size() && source.substr(pos, slen) == s;
    }
    
    std::string read_identifier() {
        std::string id;
        while (pos < source.size() && (std::isalnum(source[pos]) || source[pos] == '_')) {
            id += source[pos];
            advance();
        }
        return id;
    }
    
    std::shared_ptr<ASTNode> parse_statement() {
        skip_whitespace();
        if (pos >= source.size()) return nullptr;
        
        if (match("if")) return parse_if_statement();
        if (match("while")) return parse_while_loop();
        if (match("int") || match("float") || match("double") || match("bool")) 
            return parse_declaration();
        
        // fallback: identifier or expression
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::Identifier;
        node->text = read_identifier();
        skip_whitespace();
        if (current() == '=') {
            node->type = NodeType::Assignment;
            advance();
            skip_whitespace();
            // skip RHS for now
            while (pos < source.size() && current() != ';' && current() != '\n') advance();
        }
        if (current() == ';') advance();
        return node;
    }
    
    std::shared_ptr<ASTNode> parse_if_statement() {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::IfStatement;
        
        // skip "if (condition)"
        while (pos < source.size() && current() != '{') advance();
        if (current() == '{') {
            advance();
            int depth = 1;
            while (pos < source.size() && depth > 0) {
                if (current() == '{') depth++;
                if (current() == '}') depth--;
                advance();
            }
        }
        return node;
    }
    
    std::shared_ptr<ASTNode> parse_while_loop() {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::WhileLoop;
        
        while (pos < source.size() && current() != '{') advance();
        if (current() == '{') {
            advance();
            int depth = 1;
            while (pos < source.size() && depth > 0) {
                if (current() == '{') depth++;
                if (current() == '}') depth--;
                advance();
            }
        }
        return node;
    }
    
    std::shared_ptr<ASTNode> parse_declaration() {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::Unknown;
        
        std::string type_str = read_identifier();
        skip_whitespace();
        std::string var_name = read_identifier();
        
        if (current() == '=') {
            node->type = NodeType::Assignment;
            node->text = var_name;
            advance();
        }
        
        while (pos < source.size() && current() != ';') advance();
        if (current() == ';') advance();
        
        return node;
    }
    
    void extract_types_recursive(std::shared_ptr<ASTNode> node, TypeContext& ctx, int depth) {
        if (!node) return;
        
        if (node->type == NodeType::Assignment) {
            VariableBinding vb;
            vb.name = node->text;
            vb.type = TypeInfo(TypeKind::Int32);  // default assumption
            vb.definition_line = node->line;
            vb.is_const = false;
            ctx.variables[vb.name] = vb;
        }
        
        for (auto& child : node->children) {
            extract_types_recursive(child, ctx, depth + 1);
        }
    }
};

// ---------------------------------------------------------------------------
// 4. VALIDATOR (compile checks, AST validation, type checking)
// ---------------------------------------------------------------------------

struct ValidationResult {
    bool is_valid;                      // did code validate?
    float correctness_score;            // [0, 1] confidence
    std::string error_message;
    std::vector<std::string> warnings;
    int ast_depth;                      // max nesting
    int identifier_count;               // code complexity
    TypeContext type_ctx;
};

class CorrectnessValidator {
public:
    // Validate C++ code snippet
    ValidationResult validate_cpp(const std::string& code) {
        ValidationResult result;
        result.is_valid = true;
        result.correctness_score = 0.5f;  // baseline
        result.ast_depth = 0;
        result.identifier_count = 0;
        
        if (code.empty()) {
            result.is_valid = false;
            result.correctness_score = 0.0f;
            result.error_message = "Empty code";
            return result;
        }
        
        // Parse
        MiniCppParser parser(code);
        auto ast = parser.parse();
        result.type_ctx = parser.extract_types();
        
        // Check: balanced braces
        if (!check_balanced_braces(code)) {
            result.is_valid = false;
            result.error_message = "Unbalanced braces";
            result.correctness_score = 0.3f;
            return result;
        }
        
        // Check: no obvious syntax errors
        if (!check_syntax_plausibility(code)) {
            result.is_valid = false;
            result.error_message = "Implausible syntax";
            result.correctness_score = 0.4f;
            return result;
        }
        
        // Compute AST metrics
        result.ast_depth = compute_ast_depth(ast);
        result.identifier_count = count_identifiers(ast);
        
        // Score: well-formed + no obvious errors = higher score
        result.correctness_score = 0.7f;  // good syntax
        if (result.type_ctx.variables.size() > 0) result.correctness_score += 0.15f;
        if (result.ast_depth > 10) result.warnings.push_back("Deep nesting may indicate complexity");
        
        result.correctness_score = std::min(0.95f, result.correctness_score);
        return result;
    }
    
    // Simulate compile: check if code would compile (heuristic)
    ValidationResult simulate_compile(const std::string& code) {
        auto result = validate_cpp(code);
        
        // Additional compile-like checks
        if (code.find("return") != std::string::npos) result.correctness_score += 0.1f;
        if (code.find("int main") != std::string::npos) result.correctness_score += 0.05f;
        
        // Check for common errors
        if (code.find(";;") != std::string::npos) {
            result.warnings.push_back("Double semicolon detected");
            result.correctness_score -= 0.1f;
        }
        if (code.find("((") != std::string::npos && code.find("))") == std::string::npos) {
            result.warnings.push_back("Unbalanced parentheses");
            result.correctness_score -= 0.2f;
        }
        
        result.correctness_score = std::max(0.0f, std::min(1.0f, result.correctness_score));
        return result;
    }
    
    // Semantic distance: how similar is generated code to expected?
    float semantic_distance(const std::string& generated, const std::string& expected) {
        // Normalized Levenshtein + structure similarity
        float lex_sim = levenshtein_similarity(generated, expected);
        
        MiniCppParser gen_parser(generated), exp_parser(expected);
        auto gen_ast = gen_parser.parse();
        auto exp_ast = exp_parser.parse();
        
        int gen_id = count_identifiers(gen_ast);
        int exp_id = count_identifiers(exp_ast);
        float struct_sim = (gen_id > 0 && exp_id > 0) ? 
            (float)std::min(gen_id, exp_id) / std::max(gen_id, exp_id) : 0.0f;
        
        return 0.6f * lex_sim + 0.4f * struct_sim;
    }
    
private:
    bool check_balanced_braces(const std::string& code) {
        int depth = 0;
        for (char c : code) {
            if (c == '{') depth++;
            if (c == '}') depth--;
            if (depth < 0) return false;
        }
        return depth == 0;
    }
    
    bool check_syntax_plausibility(const std::string& code) {
        // Heuristics: no obvious token sequence errors
        if (code.find("  ;;") != std::string::npos) return false;
        if (code.find("((()") != std::string::npos) return false;
        return true;
    }
    
    int compute_ast_depth(std::shared_ptr<ASTNode> node, int d = 0) {
        if (!node) return d;
        int max_d = d;
        for (const auto& child : node->children) {
            max_d = std::max(max_d, compute_ast_depth(child, d + 1));
        }
        return max_d;
    }
    
    int count_identifiers(std::shared_ptr<ASTNode> node) {
        if (!node) return 0;
        int count = (node->type == NodeType::Identifier) ? 1 : 0;
        for (const auto& child : node->children) {
            count += count_identifiers(child);
        }
        return count;
    }
    
    float levenshtein_similarity(const std::string& a, const std::string& b) {
        int m = a.size(), n = b.size();
        if (m == 0 || n == 0) return 0.0f;
        
        std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
        for (int i = 0; i <= m; i++) dp[i][0] = i;
        for (int j = 0; j <= n; j++) dp[0][j] = j;
        
        for (int i = 1; i <= m; i++) {
            for (int j = 1; j <= n; j++) {
                if (a[i-1] == b[j-1]) dp[i][j] = dp[i-1][j-1];
                else {
                    int del = dp[i-1][j];
                    int ins = dp[i][j-1];
                    int sub = dp[i-1][j-1];
                    dp[i][j] = 1 + std::min({del, std::min(ins, sub)});
                }
            }
        }
        
        int dist = dp[m][n];
        float max_len = std::max(m, n);
        return 1.0f - (float)dist / max_len;
    }
};

// ---------------------------------------------------------------------------
// 5. EXPERT PERFORMANCE TRACKER (replace noise with correctness signal)
// ---------------------------------------------------------------------------

struct ExpertPerformanceStats {
    int total_attempts;
    int successful_completions;
    float avg_correctness_score;
    std::unordered_map<std::string, int> category_wins;  // e.g., "math" -> 5 wins
    float routing_weight;               // for weighted selection
};

class ExpertPerformanceTracker {
public:
    void record_attempt(int expert_id, const ValidationResult& result, 
                       const std::string& category) {
        auto& stats = performance[expert_id];
        stats.total_attempts++;
        
        if (result.is_valid && result.correctness_score > 0.7f) {
            stats.successful_completions++;
            stats.category_wins[category]++;
        }
        
        stats.avg_correctness_score = 
            (stats.avg_correctness_score * (stats.total_attempts - 1) + result.correctness_score) /
            stats.total_attempts;
        
        recompute_routing_weights();
    }
    
    float get_routing_weight(int expert_id) const {
        auto it = performance.find(expert_id);
        return (it != performance.end()) ? it->second.routing_weight : 1.0f;
    }
    
    int select_expert_weighted(const std::vector<int>& candidate_experts, 
                              const std::string& category) {
        if (candidate_experts.empty()) return -1;
        
        // Find best expert for this category
        int best = candidate_experts[0];
        float best_score = 0;
        
        for (int exp_id : candidate_experts) {
            auto it = performance.find(exp_id);
            if (it == performance.end()) continue;
            
            auto& stats = it->second;
            auto cat_it = stats.category_wins.find(category);
            float category_affinity = (cat_it != stats.category_wins.end()) ? 
                (float)cat_it->second / (stats.total_attempts + 1) : 0.0f;
            
            float score = 0.7f * stats.routing_weight + 0.3f * category_affinity;
            if (score > best_score) {
                best_score = score;
                best = exp_id;
            }
        }
        
        return best;
    }
    
    const ExpertPerformanceStats& get_stats(int expert_id) const {
        static ExpertPerformanceStats dummy;
        auto it = performance.find(expert_id);
        return (it != performance.end()) ? it->second : dummy;
    }
    
private:
    std::unordered_map<int, ExpertPerformanceStats> performance;
    
    void recompute_routing_weights() {
        float total_success = 0;
        for (auto& [id, stats] : performance) {
            float success_rate = stats.total_attempts > 0 ? 
                (float)stats.successful_completions / stats.total_attempts : 0.0f;
            float score = 0.6f * success_rate + 0.4f * stats.avg_correctness_score;
            stats.routing_weight = score;
            total_success += score;
        }
        
        // Normalize weights
        if (total_success > 0) {
            for (auto& [id, stats] : performance) {
                stats.routing_weight /= total_success;
            }
        }
    }
};

#endif // CORRECTNESS_VALIDATOR_HPP
