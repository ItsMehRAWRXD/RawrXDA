#pragma once
#include <string_view>
#include <optional>
#include <vector>
#include <string>

namespace RawrXD::Agentic {
    // Schema-validating parser - resilient to whitespace/field reordering
    class RobustOllamaParser {
    public:
        enum class Token { ObjectStart, ObjectEnd, ArrayStart, ArrayEnd, 
                          String, Number, Colon, Comma, True, False, Null, End };
        
        struct TokenView {
            Token type;
            std::string_view text;
        };
        
    private:
        std::string_view m_input;
        size_t m_pos{0};
        
        TokenView next_token();
        std::optional<std::string_view> extract_string();
        void skip_whitespace();
        
    public:
        explicit RobustOllamaParser(std::string_view json) : m_input(json) {}
        
        struct ModelEntry {
            std::string name;
            std::string model_id;
            size_t parameter_size;
            std::string quantization;
            std::string family;
        };
        
        std::vector<ModelEntry> parse_tags_response();
        std::string extract_message_content(); // For /api/generate streaming
    };
}
