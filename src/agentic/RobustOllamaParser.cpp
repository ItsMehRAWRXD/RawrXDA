#include "RobustOllamaParser.h"
#include <cctype>
#include <cstring>
#include <stdexcept>

using namespace RawrXD::Agentic;

void RobustOllamaParser::skip_whitespace() {
    while (m_pos < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_pos]))) {
        ++m_pos;
    }
}

RobustOllamaParser::TokenView RobustOllamaParser::next_token() {
    skip_whitespace();
    if (m_pos >= m_input.size()) return {Token::End, {}};
    
    char c = m_input[m_pos];
    size_t start = m_pos;
    
    switch (c) {
        case '{': ++m_pos; return {Token::ObjectStart, m_input.substr(start, 1)};
        case '}': ++m_pos; return {Token::ObjectEnd, m_input.substr(start, 1)};
        case '[': ++m_pos; return {Token::ArrayStart, m_input.substr(start, 1)};
        case ']': ++m_pos; return {Token::ArrayEnd, m_input.substr(start, 1)};
        case ':': ++m_pos; return {Token::Colon, m_input.substr(start, 1)};
        case ',': ++m_pos; return {Token::Comma, m_input.substr(start, 1)};
        case '"': {
            ++m_pos; // opening quote
            start = m_pos;
            while (m_pos < m_input.size() && m_input[m_pos] != '"') {
                if (m_input[m_pos] == '\\' && m_pos + 1 < m_input.size()) m_pos += 2;
                else ++m_pos;
            }
            auto str = m_input.substr(start, m_pos - start);
            if (m_pos < m_input.size()) ++m_pos; // closing quote
            return {Token::String, str};
        }
        case 't': 
            if (m_input.substr(m_pos, 4) == "true") { m_pos += 4; return {Token::True, "true"}; }
            break;
        case 'f':
            if (m_input.substr(m_pos, 5) == "false") { m_pos += 5; return {Token::False, "false"}; }
            break;
        case 'n':
            if (m_input.substr(m_pos, 4) == "null") { m_pos += 4; return {Token::Null, "null"}; }
            break;
    }
    
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-') {
        while (m_pos < m_input.size() && (std::isdigit(static_cast<unsigned char>(m_input[m_pos])) || 
               m_input[m_pos] == '.' || m_input[m_pos] == 'e' || m_input[m_pos] == 'E' || 
               m_input[m_pos] == '+' || m_input[m_pos] == '-')) {
            ++m_pos;
        }
        return {Token::Number, m_input.substr(start, m_pos - start)};
    }
    
    ++m_pos; // Skip unknown
    return next_token(); // Retry
}

std::optional<std::string_view> RobustOllamaParser::extract_string() {
    auto tok = next_token();
    if (tok.type == Token::String) return tok.text;
    return std::nullopt;
}

std::vector<RobustOllamaParser::ModelEntry> RobustOllamaParser::parse_tags_response() {
    std::vector<ModelEntry> results;
    
    auto tok = next_token();
    if (tok.type != Token::ObjectStart) return results;
    
    // Find \"models\" key
    while (true) {
        auto key = extract_string();
        if (!key) break;
        
        if (key == "models") {
            tok = next_token();
            if (tok.type != Token::ArrayStart) break;
            
            // Parse array of models
            while (true) {
                tok = next_token();
                if (tok.type == Token::ArrayEnd) break;
                if (tok.type != Token::ObjectStart) continue;
                
                ModelEntry entry;
                while (true) {
                    auto field = extract_string();
                    if (!field) break;
                    
                    tok = next_token(); // colon
                    auto value = next_token();
                    
                    if (field == "name" && value.type == Token::String) {
                        entry.name = std::string(value.text);
                        // Extract family from name (e.g., \"llama3:8b\" -> \"llama\")
                        if (auto colon = entry.name.find(':'); colon != std::string::npos) {
                            entry.family = entry.name.substr(0, colon);
                        }
                    } else if (field == "model" && value.type == Token::String) {
                        entry.model_id = std::string(value.text);
                    } else if (field == "size" && value.type == Token::Number) {
                        try {
                            entry.parameter_size = static_cast<size_t>(std::stoull(std::string(value.text)));
                        } catch(...) {
                            entry.parameter_size = 0;
                        }
                    } else if (field == "details" && value.type == Token::ObjectStart) {
                        // Parse nested details object
                        while (true) {
                            auto detail_key = extract_string();
                            if (!detail_key) break;
                            tok = next_token();
                            auto detail_val = next_token();
                            if (detail_key == "quantization_level" && detail_val.type == Token::String) {
                                entry.quantization = std::string(detail_val.text);
                            }
                            if (detail_val.type == Token::ObjectStart || detail_val.type == Token::ArrayStart) {
                                // Skip nested structures
                                int depth = 1;
                                while (depth > 0 && m_pos < m_input.size()) {
                                    auto t = next_token();
                                    if (t.type == Token::ObjectStart || t.type == Token::ArrayStart) ++depth;
                                    else if (t.type == Token::ObjectEnd || t.type == Token::ArrayEnd) --depth;
                                }
                            }
                            
                            tok = next_token();
                            if (tok.type == Token::ObjectEnd) break;
                            if (tok.type != Token::Comma) break;
                        }
                    } else if (value.type == Token::ObjectStart || value.type == Token::ArrayStart) {
                        // Skip nested
                        int depth = 1;
                        while (depth > 0 && m_pos < m_input.size()) {
                            auto t = next_token();
                            if (t.type == Token::ObjectStart || t.type == Token::ArrayStart) ++depth;
                            else if (t.type == Token::ObjectEnd || t.type == Token::ArrayEnd) --depth;
                        }
                    }
                    
                    tok = next_token();
                    if (tok.type == Token::ObjectEnd) break;
                    if (tok.type != Token::Comma) break;
                }
                
                if (!entry.name.empty()) results.push_back(entry);
                
                tok = next_token();
                if (tok.type == Token::ArrayEnd) break;
                if (tok.type != Token::Comma) break;
            }
            break;
        } else {
            // Skip value
            tok = next_token();
            if (tok.type == Token::ObjectStart || tok.type == Token::ArrayStart) {
                int depth = 1;
                while (depth > 0 && m_pos < m_input.size()) {
                    auto t = next_token();
                    if (t.type == Token::ObjectStart || t.type == Token::ArrayStart) ++depth;
                    else if (t.type == Token::ObjectEnd || t.type == Token::ArrayEnd) --depth;
                }
            }
        }
        
        tok = next_token();
        if (tok.type == Token::ObjectEnd) break;
        if (tok.type != Token::Comma) break;
    }
    
    return results;
}

std::string RobustOllamaParser::extract_message_content() {
    // For streaming responses: {\"message\":{\"content\":\"...\"}}
    std::string content;
    auto tok = next_token();
    if (tok.type != Token::ObjectStart) return content;
    
    while (true) {
        auto key = extract_string();
        if (!key) break;
        
        tok = next_token();
        if (key == "message") {
            if (tok.type != Token::ObjectStart) break;
            while (true) {
                auto msg_key = extract_string();
                if (!msg_key) break;
                tok = next_token();
                auto msg_val = next_token();
                if (msg_key == "content" && msg_val.type == Token::String) {
                    content = std::string(msg_val.text);
                }
                tok = next_token();
                if (tok.type == Token::ObjectEnd) break;
                if (tok.type != Token::Comma) break;
            }
            break;
        } else if (tok.type == Token::ObjectStart || tok.type == Token::ArrayStart) {
            int depth = 1;
            while (depth > 0 && m_pos < m_input.size()) {
                auto t = next_token();
                if (t.type == Token::ObjectStart || t.type == Token::ArrayStart) ++depth;
                else if (t.type == Token::ObjectEnd || t.type == Token::ArrayEnd) --depth;
            }
        }
        
        tok = next_token();
        if (tok.type == Token::ObjectEnd) break;
        if (tok.type != Token::Comma) break;
    }
    
    return content;
}
