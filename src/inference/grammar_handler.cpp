/**
 * @file grammar_handler.cpp
 * @brief Grammar constraint enforcement
 * 
 * Provides:
 * - BNF grammar parsing
 * - Constrained decoding
 * - JSON schema validation
 * - Regex-based constraints
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "grammar_handler.h"
#include <stack>
#include <queue>
#include <set>
#include <json.hpp>

namespace RawrXD::Inference {

// ============================================================================
// GrammarHandler Implementation
// ============================================================================

GrammarHandler::GrammarHandler() = default;
GrammarHandler::~GrammarHandler() = default;

// ============================================================================
// Grammar Loading
// ============================================================================

bool GrammarHandler::loadGrammar(const std::string& grammar) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_grammar = grammar;
    m_rules.clear();
    
    // Parse BNF grammar
    // Simplified parser for common patterns
    return parseBNF(grammar);
}

bool GrammarHandler::loadJSONSchema(const std::string& schema) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        nlohmann::json json = nlohmann::json::parse(schema);
        m_schema = json;
        return true;
    } catch (...) {
        return false;
    }
}

bool GrammarHandler::loadRegex(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        m_regex = std::regex(pattern);
        m_regexPattern = pattern;
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// BNF Parsing
// ============================================================================

bool GrammarHandler::parseBNF(const std::string& grammar) {
    // Simplified BNF parser
    // Production rules are of the form:
    //   rule ::= alternative | alternative | ...
    //   alternative ::= sequence
    //   sequence ::= element element ...
    //   element ::= terminal | non-terminal | group | optional | repetition
    
    size_t pos = 0;
    
    while (pos < grammar.length()) {
        // Skip whitespace and comments
        while (pos < grammar.length() && std::isspace(grammar[pos])) pos++;
        if (pos >= grammar.length()) break;
        
        if (grammar[pos] == '#') {
            // Skip comment
            while (pos < grammar.length() && grammar[pos] != '\n') pos++;
            continue;
        }
        
        // Parse rule name
        std::string ruleName;
        while (pos < grammar.length() && (std::isalnum(grammar[pos]) || grammar[pos] == '_')) {
            ruleName += grammar[pos++];
        }
        
        if (ruleName.empty()) {
            return false;
        }
        
        // Expect ::=
        while (pos < grammar.length() && std::isspace(grammar[pos])) pos++;
        if (pos + 2 >= grammar.length() || grammar.substr(pos, 3) != "::=") {
            return false;
        }
        pos += 3;
        
        // Parse alternatives
        std::vector<std::vector<GrammarElement>> alternatives;
        std::vector<GrammarElement> currentAlternative;
        
        while (pos < grammar.length()) {
            while (pos < grammar.length() && std::isspace(grammar[pos])) pos++;
            if (pos >= grammar.length()) break;
            
            if (grammar[pos] == '|') {
                // New alternative
                alternatives.push_back(currentAlternative);
                currentAlternative.clear();
                pos++;
                continue;
            }
            
            if (grammar[pos] == ';' || grammar[pos] == '\n') {
                // End of rule
                pos++;
                break;
            }
            
            // Parse element
            GrammarElement element;
            if (grammar[pos] == '"' || grammar[pos] == '\'') {
                // Terminal
                char quote = grammar[pos++];
                element.type = ElementType::Terminal;
                while (pos < grammar.length() && grammar[pos] != quote) {
                    element.value += grammar[pos++];
                }
                if (pos < grammar.length()) pos++; // Skip closing quote
            } else if (grammar[pos] == '(') {
                // Group
                pos++;
                element.type = ElementType::Group;
                int depth = 1;
                while (pos < grammar.length() && depth > 0) {
                    if (grammar[pos] == '(') depth++;
                    else if (grammar[pos] == ')') depth--;
                    if (depth > 0) element.value += grammar[pos++];
                }
                if (pos < grammar.length()) pos++; // Skip closing paren
            } else if (grammar[pos] == '[') {
                // Optional
                pos++;
                element.type = ElementType::Optional;
                while (pos < grammar.length() && grammar[pos] != ']') {
                    element.value += grammar[pos++];
                }
                if (pos < grammar.length()) pos++; // Skip closing bracket
            } else if (grammar[pos] == '{') {
                // Repetition
                pos++;
                element.type = ElementType::Repetition;
                while (pos < grammar.length() && grammar[pos] != '}') {
                    element.value += grammar[pos++];
                }
                if (pos < grammar.length()) pos++; // Skip closing brace
            } else {
                // Non-terminal
                element.type = ElementType::NonTerminal;
                while (pos < grammar.length() && (std::isalnum(grammar[pos]) || grammar[pos] == '_')) {
                    element.value += grammar[pos++];
                }
            }
            
            currentAlternative.push_back(element);
        }
        
        if (!currentAlternative.empty()) {
            alternatives.push_back(currentAlternative);
        }
        
        m_rules[ruleName] = alternatives;
    }
    
    return true;
}

// ============================================================================
// Constraint Checking
// ============================================================================

bool GrammarHandler::isValidToken(uint32_t token, const std::string& tokenStr) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_regexPattern.empty()) {
        // Check regex constraint
        std::string testStr = m_currentPrefix + tokenStr;
        std::smatch match;
        std::regex_search(testStr, match, m_regex);
        if (match.empty()) {
            return false;
        }
    }
    
    if (!m_grammar.empty()) {
        // Check grammar constraint
        // Simplified: check if token can follow current prefix
        return checkGrammarConstraint(tokenStr);
    }
    
    if (!m_schema.is_null()) {
        // Check JSON schema constraint
        return checkSchemaConstraint(tokenStr);
    }
    
    return true;
}

std::vector<uint32_t> GrammarHandler::getValidTokens(
    const std::vector<uint32_t>& vocab,
    const std::vector<std::string>& tokenStrings) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<uint32_t> validTokens;
    validTokens.reserve(vocab.size());
    
    for (size_t i = 0; i < vocab.size(); ++i) {
        if (isValidToken(vocab[i], tokenStrings[i])) {
            validTokens.push_back(vocab[i]);
        }
    }
    
    return validTokens;
}

void GrammarHandler::updatePrefix(const std::string& token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentPrefix += token;
}

void GrammarHandler::resetPrefix() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentPrefix.clear();
}

// ============================================================================
// Grammar Constraint Checking
// ============================================================================

bool GrammarHandler::checkGrammarConstraint(const std::string& tokenStr) const {
    // Simplified grammar checking
    // In production, this would use a proper parser
    
    if (m_rules.empty()) return true;
    
    // Check if token can be part of any valid derivation
    std::string testStr = m_currentPrefix + tokenStr;
    
    // Try to match against start rule
    auto it = m_rules.find("root");
    if (it == m_rules.end()) {
        it = m_rules.find("start");
    }
    if (it == m_rules.end() && !m_rules.empty()) {
        it = m_rules.begin();
    }
    
    if (it != m_rules.end()) {
        // Check if prefix can be extended to match rule
        // This is a simplified check
        return true; // Allow for now
    }
    
    return true;
}

// ============================================================================
// Schema Constraint Checking
// ============================================================================

bool GrammarHandler::checkSchemaConstraint(const std::string& tokenStr) const {
    if (m_schema.is_null()) return true;
    
    try {
        std::string testStr = m_currentPrefix + tokenStr;
        nlohmann::json testJson = nlohmann::json::parse(testStr);
        
        // Basic type checking
        if (m_schema.contains("type")) {
            std::string type = m_schema["type"];
            if (type == "object" && !testJson.is_object()) return false;
            if (type == "array" && !testJson.is_array()) return false;
            if (type == "string" && !testJson.is_string()) return false;
            if (type == "number" && !testJson.is_number()) return false;
            if (type == "integer" && !testJson.is_number_integer()) return false;
            if (type == "boolean" && !testJson.is_boolean()) return false;
        }
        
        return true;
    } catch (...) {
        // Invalid JSON - might be incomplete
        return true; // Allow incomplete JSON during generation
    }
}

// ============================================================================
// JSON Schema Generation
// ============================================================================

std::string GrammarHandler::generateJSONSchemaGrammar(const nlohmann::json& schema) {
    std::string grammar;
    
    if (schema.contains("type")) {
        std::string type = schema["type"];
        
        if (type == "object") {
            grammar += "object ::= '{' [members] '}'\n";
            grammar += "members ::= pair [',' pair]*\n";
            grammar += "pair ::= string ':' value\n";
        } else if (type == "array") {
            grammar += "array ::= '[' [elements] ']'\n";
            grammar += "elements ::= value [',' value]*\n";
        } else if (type == "string") {
            grammar += "string ::= '\"' [char]* '\"'\n";
        } else if (type == "number") {
            grammar += "number ::= ['-'] digits ['.' digits] [exp]\n";
        } else if (type == "boolean") {
            grammar += "boolean ::= 'true' | 'false'\n";
        } else if (type == "null") {
            grammar += "null ::= 'null'\n";
        }
    }
    
    grammar += "value ::= object | array | string | number | boolean | null\n";
    grammar += "digits ::= digit [digit]*\n";
    grammar += "digit ::= '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7' | '8' | '9'\n";
    grammar += "exp ::= ('e' | 'E') ['+' | '-'] digits\n";
    grammar += "char ::= [^\"\\\\] | '\\\\' [\"\\\\/bfnrt] | '\\\\u' hex hex hex hex\n";
    grammar += "hex ::= digit | 'a' | 'b' | 'c' | 'd' | 'e' | 'f' | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'\n";
    
    return grammar;
}

} // namespace RawrXD::Inference
