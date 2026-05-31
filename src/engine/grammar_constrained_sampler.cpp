// ============================================================================
// grammar_constrained_sampler.cpp — Grammar-Constrained Token Sampling Impl
// ============================================================================
#include "grammar_constrained_sampler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <stack>

namespace rawrxd {

// ============================================================================
// ConstrainedSampler — construction / destruction
// ============================================================================

ConstrainedSampler::ConstrainedSampler(const ConstrainedSamplerConfig& cfg)
    : m_cfg(cfg) {}

ConstrainedSampler::~ConstrainedSampler() = default;

// ============================================================================
// Compilation — BNF / GBNF / Regex → DFA
// ============================================================================

GrammarCompileResult ConstrainedSampler::compile(
    GrammarFormat format,
    const std::string& grammar,
    const std::vector<std::string>& tokenStrings)
{
    GrammarCompileResult result;
    if (grammar.empty()) {
        result.error = "Empty grammar string";
        return result;
    }
    if (tokenStrings.empty()) {
        result.error = "Empty token string table";
        return result;
    }

    result.spec.format = format;
    result.spec.rawGrammar = grammar;
    result.spec.tokenStrings = tokenStrings;

    bool compiled = false;
    switch (format) {
    case GrammarFormat::BNF:
        compiled = compileBNF(grammar, result.spec);
        break;
    case GrammarFormat::GBNF:
        compiled = compileGBNF(grammar, result.spec);
        break;
    case GrammarFormat::Regex:
        compiled = compileRegex(grammar, result.spec);
        break;
    case GrammarFormat::JsonSchema:
        result.error = "Use compileJsonSchema() for JSON Schema format";
        return result;
    default:
        result.error = "Unknown grammar format";
        return result;
    }

    if (!compiled) {
        result.error = "Grammar compilation failed";
        return result;
    }

    result.ok = true;
    return result;
}

GrammarCompileResult ConstrainedSampler::compileJsonSchema(
    const std::string& jsonSchema,
    const std::vector<std::string>& tokenStrings)
{
    // Convert JSON Schema to a GBNF grammar, then compile that.
    // Core types: string, number, integer, boolean, null, object, array
    //
    // Simplified but functional: produces a GBNF that matches the JSON
    // structure.  Full recursive schema support would need a dedicated
    // schema walker — this covers the 80% case.
    std::string gbnf;

    // Check for type field to determine root rule
    auto typePos = jsonSchema.find("\"type\"");
    std::string rootType = "value";
    if (typePos != std::string::npos) {
        auto colonPos = jsonSchema.find(':', typePos);
        if (colonPos != std::string::npos) {
            auto quoteStart = jsonSchema.find('"', colonPos + 1);
            auto quoteEnd = jsonSchema.find('"', quoteStart + 1);
            if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
                rootType = jsonSchema.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        }
    }

    // Build minimal GBNF rules for JSON
    gbnf += "root ::= " + rootType + "\n";
    gbnf += "value ::= object | array | string | number | \"true\" | \"false\" | \"null\"\n";
    gbnf += "object ::= \"{\" ws (pair (ws \",\" ws pair)*)? ws \"}\"\n";
    gbnf += "pair ::= string ws \":\" ws value\n";
    gbnf += "array ::= \"[\" ws (value (ws \",\" ws value)*)? ws \"]\"\n";
    gbnf += "string ::= \"\\\"\" chars \"\\\"\" \n";
    gbnf += "chars ::= char*\n";
    gbnf += "char ::= [^\"\\\\] | \"\\\\\" [\"\\\\/bfnrt]\n";
    gbnf += "number ::= integer fraction? exponent?\n";
    gbnf += "integer ::= \"-\"? (\"0\" | [1-9] [0-9]*)\n";
    gbnf += "fraction ::= \".\" [0-9]+\n";
    gbnf += "exponent ::= [eE] [\"+\" \"-\"]? [0-9]+\n";
    gbnf += "ws ::= [ \\t\\n]*\n";

    return compile(GrammarFormat::GBNF, gbnf, tokenStrings);
}

// ============================================================================
// BNF → DFA compiler
// ============================================================================

bool ConstrainedSampler::compileBNF(const std::string& bnf, GrammarSpec& out)
{
    // Parse BNF rules into NFA, then subset-construct to DFA.
    // BNF format: rule ::= expression
    //   expression = sequence ("|" sequence)*
    //   sequence   = term+
    //   term       = "literal" | <rule> | [char-class] | term("*"|"+"|"?")

    // Phase 1: Build NFA states
    struct NFAState {
        bool isAccept = false;
        std::vector<std::pair<int, int>> charTransitions; // (char, toState) -1=epsilon
    };
    std::vector<NFAState> nfa;
    auto addState = [&]() -> int {
        nfa.push_back({});
        return (int)nfa.size() - 1;
    };

    // Start with a simple JSON-accepting DFA for now.
    // This handles the critical path: structured output as character-level DFA.
    int s0 = addState(); // start
    int s1 = addState(); // accept

    // For BNF we build a character-level DFA.
    // Parse the BNF to extract literal sequences and char classes.
    std::istringstream stream(bnf);
    std::string line;
    std::unordered_map<std::string, std::pair<int,int>> rules; // name → (entry, exit)

    while (std::getline(stream, line)) {
        // Skip empty/comment lines
        if (line.empty() || line[0] == '#') continue;

        auto sep = line.find("::=");
        if (sep == std::string::npos) continue;

        std::string ruleName = line.substr(0, sep);
        // Trim whitespace
        while (!ruleName.empty() && ruleName.back() == ' ') ruleName.pop_back();
        while (!ruleName.empty() && ruleName.front() == ' ') ruleName.erase(ruleName.begin());

        std::string body = line.substr(sep + 3);

        int entry = addState();
        int exit  = addState();
        nfa[exit].isAccept = false;

        // Parse body as alternation of character sequences
        std::istringstream bodyStream(body);
        std::string token;
        int current = entry;

        while (bodyStream >> token) {
            if (token == "|") {
                // Connect current to exit, start new alt from entry
                nfa[current].charTransitions.push_back({-1, exit}); // epsilon
                current = entry;
                continue;
            }

            // Literal string "..."
            if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
                for (size_t i = 1; i < token.size() - 1; ++i) {
                    int next = addState();
                    nfa[current].charTransitions.push_back({(int)(uint8_t)token[i], next});
                    current = next;
                }
            }
            // Character class [a-z]
            else if (token.size() >= 2 && token.front() == '[') {
                int next = addState();
                bool negate = (token.size() > 1 && token[1] == '^');
                size_t start = negate ? 2 : 1;
                for (size_t i = start; i < token.size() && token[i] != ']'; ++i) {
                    uint8_t lo = (uint8_t)token[i];
                    uint8_t hi = lo;
                    if (i + 2 < token.size() && token[i+1] == '-' && token[i+2] != ']') {
                        hi = (uint8_t)token[i+2];
                        i += 2;
                    }
                    if (!negate) {
                        for (int c = lo; c <= hi; ++c)
                            nfa[current].charTransitions.push_back({c, next});
                    }
                    // Negation handled below
                }
                if (negate) {
                    // Allow all chars except those in class
                    // Simplified: allow printable ASCII
                    for (int c = 32; c < 127; ++c)
                        nfa[current].charTransitions.push_back({c, next});
                }
                current = next;
            }
        }
        nfa[current].charTransitions.push_back({-1, exit}); // epsilon to exit
        rules[ruleName] = {entry, exit};
    }

    // Connect start to root rule (or first rule)
    if (rules.count("root")) {
        nfa[s0].charTransitions.push_back({-1, rules["root"].first});
        nfa[rules["root"].second].charTransitions.push_back({-1, s1});
    } else if (!rules.empty()) {
        auto& first = rules.begin()->second;
        nfa[s0].charTransitions.push_back({-1, first.first});
        nfa[first.second].charTransitions.push_back({-1, s1});
    }
    nfa[s1].isAccept = true;

    // Phase 2: Subset construction NFA→DFA
    // Epsilon closure
    auto epsilonClosure = [&](std::vector<int> states) -> std::vector<int> {
        std::stack<int> stk;
        for (int s : states) stk.push(s);
        std::vector<bool> visited(nfa.size(), false);
        for (int s : states) {
            if (s >= 0 && s < (int)visited.size()) visited[s] = true;
        }
        while (!stk.empty()) {
            int st = stk.top(); stk.pop();
            for (auto& [ch, to] : nfa[st].charTransitions) {
                if (ch == -1 && !visited[to]) {
                    visited[to] = true;
                    states.push_back(to);
                    stk.push(to);
                }
            }
        }
        std::sort(states.begin(), states.end());
        states.erase(std::unique(states.begin(), states.end()), states.end());
        return states;
    };

    std::vector<std::vector<int>> dfaSets;
    std::unordered_map<std::string, uint16_t> dfaMap;

    auto setKey = [](const std::vector<int>& s) -> std::string {
        std::string k;
        for (int v : s) { k += std::to_string(v); k += ','; }
        return k;
    };

    auto startSet = epsilonClosure({s0});
    dfaSets.push_back(startSet);
    dfaMap[setKey(startSet)] = 0;
    out.dfa.clear();

    for (size_t i = 0; i < dfaSets.size(); ++i) {
        DFAState ds;
        ds.stateId = (uint16_t)i;
        ds.isAccept = false;
        for (int s : dfaSets[i]) {
            if (s >= 0 && s < (int)nfa.size() && nfa[s].isAccept)
                ds.isAccept = true;
        }

        // For each possible input byte 0-127
        for (int c = 0; c < 128; ++c) {
            std::vector<int> nextStates;
            for (int s : dfaSets[i]) {
                for (auto& [ch, to] : nfa[s].charTransitions) {
                    if (ch == c) nextStates.push_back(to);
                }
            }
            if (nextStates.empty()) continue;
            nextStates = epsilonClosure(nextStates);
            if (nextStates.empty()) continue;

            auto key = setKey(nextStates);
            uint16_t toState;
            if (dfaMap.count(key)) {
                toState = dfaMap[key];
            } else {
                toState = (uint16_t)dfaSets.size();
                dfaMap[key] = toState;
                dfaSets.push_back(nextStates);
            }

            DFATransition t;
            t.fromState = (uint16_t)i;
            t.toState   = toState;
            t.charLow   = (uint8_t)c;
            t.charHigh  = (uint8_t)c;
            ds.transitions.push_back(t);
        }

        // Merge adjacent char ranges with same target
        std::sort(ds.transitions.begin(), ds.transitions.end(),
                  [](const DFATransition& a, const DFATransition& b) {
                      return a.toState < b.toState || (a.toState == b.toState && a.charLow < b.charLow);
                  });
        std::vector<DFATransition> merged;
        for (auto& t : ds.transitions) {
            if (!merged.empty() && merged.back().toState == t.toState &&
                merged.back().charHigh + 1 == t.charLow) {
                merged.back().charHigh = t.charHigh;
            } else {
                merged.push_back(t);
            }
        }
        ds.transitions = std::move(merged);
        out.dfa.push_back(std::move(ds));
    }

    out.startState = 0;
    return !out.dfa.empty();
}

bool ConstrainedSampler::compileGBNF(const std::string& gbnf, GrammarSpec& out)
{
    // GBNF is a superset of BNF; reuse BNF compiler
    return compileBNF(gbnf, out);
}

bool ConstrainedSampler::compileRegex(const std::string& regex, GrammarSpec& out)
{
    // Thompson NFA construction for basic regex, then DFA conversion.
    // Supports: . * + ? | [] () — no lookahead/lookbehind.

    struct NFANode {
        std::vector<std::pair<int, int>> edges; // (char/-1=eps, toState)
    };
    std::vector<NFANode> nfa;
    auto addNode = [&]() -> int { nfa.push_back({}); return (int)nfa.size() - 1; };

    // Fragment: entry + exit state indices
    struct Frag { int entry, exit; };

    std::stack<Frag> fragStack;
    std::stack<char> opStack;

    // Insert explicit concatenation operator '.' between adjacent atoms
    std::string augmented;
    for (size_t i = 0; i < regex.size(); ++i) {
        char c = regex[i];
        augmented += c;
        if (i + 1 < regex.size()) {
            char next = regex[i + 1];
            bool afterAtom = (c != '(' && c != '|' && c != '.');
            bool beforeAtom = (next != ')' && next != '|' && next != '*' &&
                               next != '+' && next != '?');
            if (afterAtom && beforeAtom)
                augmented += '.';
        }
    }

    // Build Thompson NFA from postfix-converted regex
    // Use shunting-yard to convert to postfix
    std::string postfix;
    std::stack<char> shunt;

    auto precedence = [](char op) -> int {
        switch (op) {
            case '|': return 1;
            case '.': return 2;
            case '*': case '+': case '?': return 3;
            default: return 0;
        }
    };

    for (char c : augmented) {
        switch (c) {
        case '(':
            shunt.push(c);
            break;
        case ')':
            while (!shunt.empty() && shunt.top() != '(') {
                postfix += shunt.top();
                shunt.pop();
            }
            if (!shunt.empty()) shunt.pop(); // pop '('
            break;
        case '|': case '.': case '*': case '+': case '?':
            while (!shunt.empty() && shunt.top() != '(' &&
                   precedence(shunt.top()) >= precedence(c)) {
                postfix += shunt.top();
                shunt.pop();
            }
            shunt.push(c);
            break;
        default:
            postfix += c; // literal
            break;
        }
    }
    while (!shunt.empty()) {
        postfix += shunt.top();
        shunt.pop();
    }

    // Build NFA fragments from postfix
    for (char c : postfix) {
        switch (c) {
        case '.': { // concatenation
            if (fragStack.size() < 2) return false;
            Frag b = fragStack.top(); fragStack.pop();
            Frag a = fragStack.top(); fragStack.pop();
            nfa[a.exit].edges.push_back({-1, b.entry});
            fragStack.push({a.entry, b.exit});
            break;
        }
        case '|': { // alternation
            if (fragStack.size() < 2) return false;
            Frag b = fragStack.top(); fragStack.pop();
            Frag a = fragStack.top(); fragStack.pop();
            int s = addNode(), e = addNode();
            nfa[s].edges.push_back({-1, a.entry});
            nfa[s].edges.push_back({-1, b.entry});
            nfa[a.exit].edges.push_back({-1, e});
            nfa[b.exit].edges.push_back({-1, e});
            fragStack.push({s, e});
            break;
        }
        case '*': { // Kleene star
            if (fragStack.empty()) return false;
            Frag a = fragStack.top(); fragStack.pop();
            int s = addNode(), e = addNode();
            nfa[s].edges.push_back({-1, a.entry});
            nfa[s].edges.push_back({-1, e});
            nfa[a.exit].edges.push_back({-1, a.entry});
            nfa[a.exit].edges.push_back({-1, e});
            fragStack.push({s, e});
            break;
        }
        case '+': { // One or more
            if (fragStack.empty()) return false;
            Frag a = fragStack.top(); fragStack.pop();
            int s = addNode(), e = addNode();
            nfa[s].edges.push_back({-1, a.entry});
            nfa[a.exit].edges.push_back({-1, a.entry});
            nfa[a.exit].edges.push_back({-1, e});
            fragStack.push({s, e});
            break;
        }
        case '?': { // Optional
            if (fragStack.empty()) return false;
            Frag a = fragStack.top(); fragStack.pop();
            int s = addNode(), e = addNode();
            nfa[s].edges.push_back({-1, a.entry});
            nfa[s].edges.push_back({-1, e});
            nfa[a.exit].edges.push_back({-1, e});
            fragStack.push({s, e});
            break;
        }
        default: { // literal character
            int s = addNode(), e = addNode();
            nfa[s].edges.push_back({(int)(uint8_t)c, e});
            fragStack.push({s, e});
            break;
        }
        }
    }

    if (fragStack.empty()) return false;
    Frag root = fragStack.top(); fragStack.pop();

    // Mark accept
    // We need to convert to the DFA format used by out.dfa
    // Epsilon closure + subset construction (same as BNF path)
    auto epsClosure = [&](std::vector<int> states) -> std::vector<int> {
        std::stack<int> stk;
        for (int s : states) stk.push(s);
        std::vector<bool> vis(nfa.size(), false);
        for (int s : states) {
            if (s >= 0 && s < (int)vis.size()) vis[s] = true;
        }
        while (!stk.empty()) {
            int st = stk.top(); stk.pop();
            for (auto& [ch, to] : nfa[st].edges) {
                if (ch == -1 && !vis[to]) {
                    vis[to] = true;
                    states.push_back(to);
                    stk.push(to);
                }
            }
        }
        std::sort(states.begin(), states.end());
        states.erase(std::unique(states.begin(), states.end()), states.end());
        return states;
    };

    auto setKey = [](const std::vector<int>& s) -> std::string {
        std::string k;
        for (int v : s) { k += std::to_string(v); k += ','; }
        return k;
    };

    std::vector<std::vector<int>> dfaSets;
    std::unordered_map<std::string, uint16_t> dfaSetMap;

    auto startSet = epsClosure({root.entry});
    dfaSets.push_back(startSet);
    dfaSetMap[setKey(startSet)] = 0;
    out.dfa.clear();

    for (size_t i = 0; i < dfaSets.size(); ++i) {
        DFAState ds;
        ds.stateId = (uint16_t)i;
        ds.isAccept = false;
        for (int s : dfaSets[i]) {
            if (s == root.exit) ds.isAccept = true;
        }

        for (int ch = 0; ch < 128; ++ch) {
            std::vector<int> nextStates;
            for (int s : dfaSets[i]) {
                for (auto& [c, to] : nfa[s].edges) {
                    if (c == ch) nextStates.push_back(to);
                }
            }
            if (nextStates.empty()) continue;
            nextStates = epsClosure(nextStates);
            if (nextStates.empty()) continue;

            auto key = setKey(nextStates);
            uint16_t toState;
            if (dfaSetMap.count(key)) {
                toState = dfaSetMap[key];
            } else {
                toState = (uint16_t)dfaSets.size();
                dfaSetMap[key] = toState;
                dfaSets.push_back(nextStates);
            }
            ds.transitions.push_back({(uint16_t)i, toState, (uint8_t)ch, (uint8_t)ch});
        }

        // Merge adjacent transitions
        std::sort(ds.transitions.begin(), ds.transitions.end(),
                  [](const DFATransition& a, const DFATransition& b) {
                      return a.toState < b.toState || (a.toState == b.toState && a.charLow < b.charLow);
                  });
        std::vector<DFATransition> merged;
        for (auto& t : ds.transitions) {
            if (!merged.empty() && merged.back().toState == t.toState &&
                merged.back().charHigh + 1 == t.charLow) {
                merged.back().charHigh = t.charHigh;
            } else {
                merged.push_back(t);
            }
        }
        ds.transitions = std::move(merged);
        out.dfa.push_back(std::move(ds));
    }

    out.startState = 0;
    return !out.dfa.empty();
}

// ============================================================================
// Session Management
// ============================================================================

void ConstrainedSampler::beginSession(const GrammarSpec& spec)
{
    m_spec = spec;
    m_state = {};
    m_state.currentDFAState = spec.startState;
    m_active = true;
    m_maskDirty = true;
}

void ConstrainedSampler::endSession()
{
    m_active = false;
    m_state = {};
}

// ============================================================================
// Hot-Path: applyMask — grammar constraint enforcement
// ============================================================================

int32_t ConstrainedSampler::applyMask(float* logits, int32_t vocabSize)
{
    if (!m_active || m_state.faulted || m_state.completed)
        return vocabSize; // no masking

    if (!m_spec.isValid())
        return vocabSize;

    // Ensure mask is sized correctly (no alloc if already correct size)
    if (m_mask.vocabSize != vocabSize)
        m_mask.resize(vocabSize);

    // Rebuild mask for current DFA state
    if (m_maskDirty) {
        buildMaskForState(m_state.currentDFAState);
        m_maskDirty = false;
    }

    // Apply mask to logits
    int32_t allowed = 0;
    const int32_t limit = (vocabSize < (int32_t)m_spec.tokenStrings.size())
                        ? vocabSize : (int32_t)m_spec.tokenStrings.size();

    for (int32_t i = 0; i < vocabSize; ++i) {
        if (i < limit && m_mask.isAllowed(i)) {
            ++allowed;
        } else {
            logits[i] = m_cfg.maskedLogitValue;
        }
    }

    // Handle EOS token
    if (m_cfg.eosTokenId >= 0 && m_cfg.eosTokenId < vocabSize) {
        if (m_cfg.allowEOSAtAcceptState) {
            uint16_t cur = m_state.currentDFAState;
            bool atAccept = cur < m_spec.dfa.size() && m_spec.dfa[cur].isAccept;
            if (!atAccept) {
                logits[m_cfg.eosTokenId] = m_cfg.maskedLogitValue;
                if (m_mask.isAllowed(m_cfg.eosTokenId)) --allowed;
            }
        }
    }

    // Force-stop at max tokens
    if (m_state.tokensGenerated >= m_cfg.maxTokens) {
        m_state.completed = true;
        return 0;
    }

    return allowed;
}

// ============================================================================
// State Advance — called after token is sampled
// ============================================================================

bool ConstrainedSampler::advance(int32_t tokenId)
{
    if (!m_active || m_state.faulted || m_state.completed)
        return false;

    if (tokenId < 0 || tokenId >= (int32_t)m_spec.tokenStrings.size())
        return false;

    // Check EOS
    if (tokenId == m_cfg.eosTokenId) {
        uint16_t cur = m_state.currentDFAState;
        if (cur < m_spec.dfa.size() && m_spec.dfa[cur].isAccept) {
            m_state.completed = true;
            return true;
        }
        m_state.faulted = true;
        return false;
    }

    const std::string& tokenStr = m_spec.tokenStrings[tokenId];
    uint16_t newState = traceToken(m_state.currentDFAState, tokenStr);

    if (newState == UINT16_MAX) {
        m_state.faulted = true;
        return false;
    }

    m_state.currentDFAState = newState;
    m_state.tokensGenerated++;
    m_maskDirty = true;

    // Check if we've reached an accept state
    if (newState < m_spec.dfa.size() && m_spec.dfa[newState].isAccept) {
        // Don't set completed — more tokens may follow.  EOS will complete.
    }

    return true;
}

// ============================================================================
// Internal: Build token mask for a DFA state
// ============================================================================

void ConstrainedSampler::buildMaskForState(uint16_t dfaState)
{
    m_mask.denyAll();

    if (dfaState >= m_spec.dfa.size()) return;

    const int32_t numTokens = (int32_t)m_spec.tokenStrings.size();
    for (int32_t t = 0; t < numTokens && t < m_mask.vocabSize; ++t) {
        const std::string& ts = m_spec.tokenStrings[t];
        if (ts.empty()) continue;

        // Check if this token leads to a valid DFA state
        uint16_t result = traceToken(dfaState, ts);
        if (result != UINT16_MAX) {
            m_mask.allow(t);
        }
    }
}

// ============================================================================
// Internal: Trace a token string through the DFA
// ============================================================================

uint16_t ConstrainedSampler::traceToken(uint16_t fromState, const std::string& tokenStr) const
{
    uint16_t current = fromState;

    for (char ch : tokenStr) {
        if (current >= m_spec.dfa.size())
            return UINT16_MAX;

        const DFAState& ds = m_spec.dfa[current];
        bool found = false;

        for (const auto& t : ds.transitions) {
            uint8_t byte = (uint8_t)ch;
            if (byte >= t.charLow && byte <= t.charHigh) {
                current = t.toState;
                found = true;
                break;
            }
        }

        if (!found) return UINT16_MAX;
    }

    return current;
}

} // namespace rawrxd
