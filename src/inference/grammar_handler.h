/**
 * @file grammar_handler.h
 * @brief Grammar constraint enforcement
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <regex>
#include <json.hpp>

namespace RawrXD::Inference {

// ============================================================================
// Grammar Element Types
// ============================================================================

enum class ElementType {
    Terminal,
    NonTerminal,
    Group,
    Optional,
    Repetition
};

// ============================================================================
// Grammar Element
// ============================================================================

struct GrammarElement {
    ElementType type;
    std::string value;
};

// ============================================================================
// Grammar Handler
// ============================================================================

class GrammarHandler {
public:
    GrammarHandler();
    ~GrammarHandler();
    
    // Grammar loading
    bool loadGrammar(const std::string& grammar);
    bool loadJSONSchema(const std::string& schema);
    bool loadRegex(const std::string& pattern);
    
    // Constraint checking
    bool isValidToken(uint32_t token, const std::string& tokenStr) const;
    std::vector<uint32_t> getValidTokens(const std::vector<uint32_t>& vocab,
                                         const std::vector<std::string>& tokenStrings) const;
    
    // Prefix management
    void updatePrefix(const std::string& token);
    void resetPrefix();
    
    // Schema to grammar conversion
    static std::string generateJSONSchemaGrammar(const nlohmann::json& schema);
    
private:
    bool parseBNF(const std::string& grammar);
    bool checkGrammarConstraint(const std::string& tokenStr) const;
    bool checkSchemaConstraint(const std::string& tokenStr) const;
    
    mutable std::mutex m_mutex;
    std::string m_grammar;
    std::string m_currentPrefix;
    std::string m_regexPattern;
    std::regex m_regex;
    nlohmann::json m_schema;
    std::map<std::string, std::vector<std::vector<GrammarElement>>> m_rules;
};

} // namespace RawrXD::Inference
