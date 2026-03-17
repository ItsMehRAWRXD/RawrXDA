#pragma once
// LSP JSON Parser - MASM64 Implementation
// Zero-allocation streaming JSON parser for JSON-RPC 2.0 protocol

#include <cstdint>
#include <cstddef>

//==============================================================================
// JSON Value Types
//==============================================================================

enum class JsonType : uint32_t {
    Null = 0,
    Boolean = 1,
    Number = 2,
    String = 3,
    Array = 4,
    Object = 5
};

enum class JsonTokenType : uint32_t {
    Eof = 0,
    LBrace = 1,
    RBrace = 2,
    LBracket = 3,
    RBracket = 4,
    Colon = 5,
    Comma = 6,
    String = 7,
    Number = 8,
    True = 9,
    False = 10,
    Null = 11,
    Error = 255
};

enum class JsonError : uint32_t {
    None = 0,
    UnexpectedChar = 1,
    UnterminatedString = 2,
    InvalidNumber = 3,
    MaxDepth = 4,
    InvalidEscape = 5,
    TrailingComma = 6
};

//==============================================================================
// JSON_VALUE Structure (matches MASM layout)
//==============================================================================

struct JsonValue {
    uint32_t Type;                      // JsonType
    uint32_t _padding1;
    
    // Value storage (union-like)
    uint8_t BoolValue;
    uint8_t _padding2[7];
    double NumberValue;
    const char* StringValue;            // pointer to buffer (no copy)
    uint64_t StringLength;
    
    // For ARRAY/OBJECT
    JsonValue** Children;
    uint32_t ChildCount;
    uint32_t _capacity;
    
    // For OBJECT
    const char** Keys;                  // parallel array of key pointers
};

//==============================================================================
// JSON_PARSER Structure (matches MASM layout)
//==============================================================================

struct JsonParser {
    const uint8_t* Buffer;
    uint64_t BufferLen;
    uint64_t Pos;
    
    uint32_t State;
    uint32_t Depth;
    uint32_t InObject;
    uint32_t DepthStack[32];
    
    uint32_t TokenType;
    uint64_t TokenStart;
    uint64_t TokenLen;
    uint64_t TokenValue;
    
    uint32_t LastError;
    uint64_t ErrorPos;
    char ErrorContext[32];
    
    uint32_t TokensProcessed;
    uint32_t MaxDepthSeen;
    uint32_t StringsInterned;
};

//==============================================================================
// Public API
//==============================================================================

// Initialize global JSON parser infrastructure (call once at startup)
extern "C" uint64_t JsonParser_GlobalInit();

// Create parser instance (allocates from slab)
extern "C" JsonParser* JsonParser_Create(const uint8_t* buffer, size_t length);

// Parse complete JSON value (returns root JsonValue* or nullptr)
extern "C" JsonValue* JsonParser_ParseValue(JsonParser* parser);

// Tokenizer (for streaming/custom parsing)
extern "C" void JsonParser_NextToken(JsonParser* parser);
extern "C" void JsonParser_SkipWhitespace(JsonParser* parser);

// Zero-copy field access (no allocations, returns pointers into original buffer)
extern "C" JsonValue* Json_FindString(JsonValue* root, const wchar_t* fieldName);
extern "C" uint64_t Json_GetStringField(JsonValue* root, const wchar_t* fieldName,
                                         const char** outString, uint64_t* outLength);
extern "C" int64_t Json_GetIntField(JsonValue* root, const wchar_t* fieldName);
extern "C" uint64_t Json_GetBoolField(JsonValue* root, const wchar_t* fieldName);

// Utility
extern "C" double Json_ParseDouble(const char* buffer, size_t length);

//==============================================================================
// Performance Instrumentation (for monitoring in production)
//==============================================================================

extern "C" {
    extern uint64_t g_JsonParseSuccessCount;      // successful parses
    extern uint64_t g_JsonParseFailCount;         // parse failures
    extern uint32_t g_JsonMaxTokenCount;          // max tokens in a message
    extern uint64_t g_JsonBytesProcessed;         // total bytes parsed
}

//==============================================================================
// C++ Wrapper (convenience)
//==============================================================================

class JsonParserWrapper {
public:
    JsonParserWrapper(const uint8_t* buffer, size_t length)
        : m_buffer(buffer), m_length(length), m_root(nullptr)
    {
        m_parser = JsonParser_Create(buffer, length);
    }
    
    ~JsonParserWrapper() {
        // Parser and values are freed by slab cleanup
    }
    
    JsonValue* Parse() {
        if (!m_parser) return nullptr;
        m_root = JsonParser_ParseValue(m_parser);
        if (m_root) {
            g_JsonParseSuccessCount++;
        } else {
            g_JsonParseFailCount++;
        }
        return m_root;
    }
    
    JsonValue* GetRoot() const { return m_root; }
    JsonParser* GetParser() const { return m_parser; }
    
    bool GetStringField(const wchar_t* fieldName, std::string& out) const {
        if (!m_root) return false;
        const char* ptr = nullptr;
        uint64_t len = 0;
        if (Json_GetStringField(m_root, fieldName, &ptr, &len)) {
            out.assign(ptr, len);
            return true;
        }
        return false;
    }
    
    int64_t GetIntField(const wchar_t* fieldName) const {
        if (!m_root) return 0;
        return Json_GetIntField(m_root, fieldName);
    }
    
    bool GetBoolField(const wchar_t* fieldName) const {
        if (!m_root) return false;
        return Json_GetBoolField(m_root, fieldName) != 0;
    }
    
    const char* GetErrorMessage() const {
        if (!m_parser) return "No parser";
        switch (static_cast<JsonError>(m_parser->LastError)) {
            case JsonError::None: return "None";
            case JsonError::UnexpectedChar: return "Unexpected character";
            case JsonError::UnterminatedString: return "Unterminated string";
            case JsonError::InvalidNumber: return "Invalid number";
            case JsonError::MaxDepth: return "Max nesting depth exceeded";
            case JsonError::InvalidEscape: return "Invalid escape sequence";
            case JsonError::TrailingComma: return "Trailing comma";
            default: return "Unknown error";
        }
    }
    
private:
    const uint8_t* m_buffer;
    size_t m_length;
    JsonParser* m_parser;
    JsonValue* m_root;
};

