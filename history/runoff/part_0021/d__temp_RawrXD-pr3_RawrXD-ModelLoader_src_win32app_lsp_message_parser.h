#pragma once
// LSP Message Parser Integration - C++ API
// Bridges JSON parsing and LSP message validation for RawrXD extension host

#include "lsp_json_parser.h"
#include <string>
#include <cstring>
#include <cstdio>

//==============================================================================
// LSP_MESSAGE Structure (matches MASM layout)
//==============================================================================

enum class MessageType : uint32_t {
    Request = 0,
    Response = 1,
    Notification = 2,
    Unknown = 3
};

struct LspMessage {
    MessageType Type;
    uint32_t Id;                        // request/response ID
    const char* Method;                 // method name (for requests/notifications)
    uint64_t MethodLength;
    JsonValue* Params;                  // JSON params tree (if present)
    JsonValue* Result;                  // JSON result tree (for responses)
    int32_t ErrorCode;                  // error code (for errors)
    const char* ErrorMessage;           // error message
    uint32_t Flags;                     // MSG_VALID_*
    uint32_t _padding;
    
    // Timestamps and metadata
    uint64_t ReceiveTimeUs;             // microseconds since epoch
    uint32_t SourceLine;                // for debugging
    uint32_t _padding2;
};

//==============================================================================
// Message Validation
//==============================================================================

struct MessageValidation {
    bool IsValid;
    uint32_t _padding1;
    uint32_t ValidFlags;                // which fields were present
    const char* ErrorMessage;           // diagnostics
    uint32_t WarningCount;
    uint32_t _padding2;
};

//==============================================================================
// Parse Statistics (for production instrumentation)
//==============================================================================

struct ParseStats {
    uint64_t SuccessCount;
    uint64_t FailCount;
    uint64_t TotalBytesRead;
    uint32_t MaxFieldCount;
    uint32_t MaxDepth;
    uint32_t AvgParseTimeUs;
    uint32_t LastErrorCode;
};

//==============================================================================
// C API (MASM exports)
//==============================================================================

extern "C" {
    // Main parsing entry point: JSON buffer -> LSP_MESSAGE
    LspMessage* LspMessage_ParseFromJson(const uint8_t* buffer, size_t bufferLen);
    
    // Manual field extraction (for streaming/custom parsing)
    void LspMessage_ExtractFields(LspMessage* pMessage, JsonValue* pJsonRoot);
    
    // Validation
    MessageValidation* LspMessage_Validate(LspMessage* pMessage);
    
    // Instrumentation
    ParseStats* LspMessage_GetStats();
    void LspMessage_ResetStats();
    
    // Statistics
    extern ParseStats g_LspParseStats;
}

//==============================================================================
// C++ Wrapper (convenience, type-safety)
//==============================================================================

class LspMessageParser {
public:
    LspMessageParser(const uint8_t* buffer, size_t length)
        : m_buffer(buffer), m_length(length), m_message(nullptr)
    {
    }
    
    ~LspMessageParser() {
        // Message is freed by slab cleanup
    }
    
    bool Parse() {
        m_message = LspMessage_ParseFromJson(m_buffer, m_length);
        return m_message != nullptr;
    }
    
    LspMessage* GetMessage() const { return m_message; }
    
    // Convenience methods
    bool IsRequest() const {
        return m_message && m_message->Id != 0;
    }
    
    bool IsResponse() const {
        return m_message && m_message->Method == nullptr;
    }
    
    bool IsNotification() const {
        return m_message && m_message->Id == 0 && m_message->Method != nullptr;
    }
    
    bool HasError() const {
        return m_message && m_message->ErrorCode != 0;
    }
    
    std::string GetMethod() const {
        if (m_message && m_message->Method) {
            return std::string(m_message->Method, m_message->MethodLength);
        }
        return "";
    }
    
    uint32_t GetId() const {
        return m_message ? m_message->Id : 0;
    }
    
    int32_t GetErrorCode() const {
        return m_message ? m_message->ErrorCode : 0;
    }
    
    std::string GetErrorMessage() const {
        if (m_message && m_message->ErrorMessage) {
            return std::string(m_message->ErrorMessage);
        }
        return "";
    }
    
    // Extract typed params (if present)
    bool GetParamString(const wchar_t* fieldName, std::string& outValue) const {
        if (!m_message || !m_message->Params) return false;
        const char* ptr = nullptr;
        uint64_t len = 0;
        if (Json_GetStringField(m_message->Params, fieldName, &ptr, &len)) {
            outValue.assign(ptr, len);
            return true;
        }
        return false;
    }
    
    // Statistics
    static ParseStats GetStats() {
        return g_LspParseStats;
    }
    
    static void ResetStats() {
        LspMessage_ResetStats();
    }
    
    static std::string FormatStats() {
        auto stats = GetStats();
        char buf[256];
        snprintf(buf, sizeof(buf),
                "JSON Parse Stats: %llu success, %llu fail, %llu bytes, "
                "max_depth=%u, max_fields=%u",
                stats.SuccessCount, stats.FailCount, stats.TotalBytesRead,
                stats.MaxDepth, stats.MaxFieldCount);
        return std::string(buf);
    }
    
private:
    const uint8_t* m_buffer;
    size_t m_length;
    LspMessage* m_message;
};

//==============================================================================
// Protocol Error Codes (LSP standard)
//==============================================================================

namespace LspError {
    constexpr int32_t ParseError = -32700;
    constexpr int32_t InvalidRequest = -32600;
    constexpr int32_t MethodNotFound = -32601;
    constexpr int32_t InvalidParams = -32602;
    constexpr int32_t InternalError = -32603;
    constexpr int32_t ServerErrorStart = -32099;
    constexpr int32_t ServerErrorEnd = -32000;
}

